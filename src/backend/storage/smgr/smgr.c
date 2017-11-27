/*-------------------------------------------------------------------------
 *
 * smgr.c
 *	  public interface routines to storage manager switch.
 *
 *	  All file system operations in POSTGRES dispatch through these
 *	  routines.
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/smgr/smgr.c,v 1.115 2009/01/01 17:23:48 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/persistentfilesysobjname.h"
#include "access/xact.h"
#include "access/xlogmm.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistentrelation.h"
#include "commands/tablespace.h"
#include "postmaster/postmaster.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/smgr.h"
#include "storage/smgr_ao.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

/*
 * Each backend has a hashtable that stores all extant SMgrRelation objects.
 */
static HTAB *SMgrRelationHash = NULL;

/* local function prototypes */
static void smgrshutdown(int code, Datum arg);

char *
StorageManagerMirrorMode_Name(StorageManagerMirrorMode mirrorMode)
{
	switch (mirrorMode)
	{
		case StorageManagerMirrorMode_None:
			return "None";
		case StorageManagerMirrorMode_PrimaryOnly:
			return "Primary Only";
		case StorageManagerMirrorMode_MirrorOnly:
			return "Mirror Only";
		case StorageManagerMirrorMode_Both:
			return "Both";

		default:
			return "Unknown";
	}
}

/*
 *	smgrinit(), smgrshutdown() -- Initialize or shut down storage
 *								  managers.
 *
 * Note: smgrinit is called during backend startup (normal or standalone
 * case), *not* during postmaster start.  Therefore, any resources created
 * here or destroyed in smgrshutdown are backend-local.
 */
void
smgrinit(void)
{
	mdinit();

	/* register the shutdown proc */
	on_proc_exit(smgrshutdown, 0);
}

/*
 * on_proc_exit hook for smgr cleanup during backend shutdown
 */
static void
smgrshutdown(int code, Datum arg)
{
}

/*
 *	smgropen() -- Return an SMgrRelation object, creating it if need be.
 *
 *		This does not attempt to actually open the object.
 */
SMgrRelation
smgropen(RelFileNode rnode)
{
	SMgrRelation reln;
	bool		found;

	if (SMgrRelationHash == NULL)
	{
		/* First time through: initialize the hash table */
		HASHCTL		ctl;

		MemSet(&ctl, 0, sizeof(ctl));
		ctl.keysize = sizeof(RelFileNode);
		ctl.entrysize = sizeof(SMgrRelationData);
		ctl.hash = tag_hash;
		SMgrRelationHash = hash_create("smgr relation table", 400,
									   &ctl, HASH_ELEM | HASH_FUNCTION);
	}

	/* Look up or create an entry */
	reln = (SMgrRelation) hash_search(SMgrRelationHash,
									  (void *) &rnode,
									  HASH_ENTER, &found);

	/* Initialize it if not present before */
	if (!found)
	{
		int forknum;

		/* hash_search already filled in the lookup key */
		reln->smgr_owner = NULL;
		reln->smgr_which = 0;	/* we only have md.c at present */

		/* mark it not open */
		for(forknum = 0; forknum <= MAX_FORKNUM; forknum++)
			reln->md_fd[forknum] = NULL;
	}

	return reln;
}

/*
 * smgrsetowner() -- Establish a long-lived reference to an SMgrRelation object
 *
 * There can be only one owner at a time; this is sufficient since currently
 * the only such owners exist in the relcache.
 */
void
smgrsetowner(SMgrRelation *owner, SMgrRelation reln)
{
	/*
	 * First, unhook any old owner.  (Normally there shouldn't be any, but it
	 * seems possible that this can happen during swap_relation_files()
	 * depending on the order of processing.  It's ok to close the old
	 * relcache entry early in that case.)
	 */
	if (reln->smgr_owner)
		*(reln->smgr_owner) = NULL;

	/* Now establish the ownership relationship. */
	reln->smgr_owner = owner;
	*owner = reln;
}

/*
 *	smgrexists() -- Does the underlying file for a fork exist?
 */
bool
smgrexists(SMgrRelation reln, ForkNumber forknum)
{
	return mdexists(reln, forknum);
}

/*
 *	smgrclose() -- Close and delete an SMgrRelation object.
 */
void
smgrclose(SMgrRelation reln)
{
	SMgrRelation *owner;
	ForkNumber forknum;

	for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
		mdclose(reln, forknum);

	owner = reln->smgr_owner;

	if (hash_search(SMgrRelationHash,
					(void *) &(reln->smgr_rnode),
					HASH_REMOVE, NULL) == NULL)
		elog(ERROR, "SMgrRelation hashtable corrupted");

	/*
	 * Unhook the owner pointer, if any.  We do this last since in the remote
	 * possibility of failure above, the SMgrRelation object will still exist.
	 */
	if (owner)
		*owner = NULL;
}

/*
 *	smgrcloseall() -- Close all existing SMgrRelation objects.
 */
void
smgrcloseall(void)
{
	HASH_SEQ_STATUS status;
	SMgrRelation reln;

	/* Nothing to do if hashtable not set up */
	if (SMgrRelationHash == NULL)
		return;

	hash_seq_init(&status, SMgrRelationHash);

	while ((reln = (SMgrRelation) hash_seq_search(&status)) != NULL)
		smgrclose(reln);
}

/*
 *	smgrclosenode() -- Close SMgrRelation object for given RelFileNode,
 *					   if one exists.
 *
 * This has the same effects as smgrclose(smgropen(rnode)), but it avoids
 * uselessly creating a hashtable entry only to drop it again when no
 * such entry exists already.
 */
void
smgrclosenode(RelFileNode rnode)
{
	SMgrRelation reln;

	/* Nothing to do if hashtable not set up */
	if (SMgrRelationHash == NULL)
		return;

	reln = (SMgrRelation) hash_search(SMgrRelationHash,
									  (void *) &rnode,
									  HASH_FIND, NULL);
	if (reln != NULL)
		smgrclose(reln);
}

/*
 *	smgrcreatefilespacedir() -- Create a new filespace directory.
 */
void
smgrcreatefilespacedir(Oid filespaceOid,
					   char *primaryFilespaceLocation,

 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */
					   char *mirrorFilespaceLocation,
					   StorageManagerMirrorMode mirrorMode,
					   bool ignoreAlreadyExists,
					   int *primaryError,
					   bool *mirrorDataLossOccurred)
{
	mdcreatefilespacedir(filespaceOid,
						 primaryFilespaceLocation,
						 mirrorFilespaceLocation,
						 mirrorMode,
						 ignoreAlreadyExists,
						 primaryError,
						 mirrorDataLossOccurred);
}

/*
 *	smgrcreatetablespacedir() -- Create a new tablespace directory.
 *
 */
void
smgrcreatetablespacedir(Oid tablespaceOid,
						StorageManagerMirrorMode mirrorMode,
						bool ignoreAlreadyExists,
						int *primaryError,
						bool *mirrorDataLossOccurred)
{
	mdcreatetablespacedir(tablespaceOid,
						  mirrorMode,
						  ignoreAlreadyExists,
						  primaryError,
						  mirrorDataLossOccurred);
}

/*
 *	smgrcreatedbdir() -- Create a new database directory.
 */
void
smgrcreatedbdir(DbDirNode *dbDirNode,
				StorageManagerMirrorMode mirrorMode,
				bool ignoreAlreadyExists,
				int *primaryError,
				bool *mirrorDataLossOccurred)
{
	mdcreatedbdir(dbDirNode,
				  mirrorMode,
				  ignoreAlreadyExists,
				  primaryError,
				  mirrorDataLossOccurred);
}

void
smgrcreatedbdirjustintime(DbDirNode *justInTimeDbDirNode,
						  MirroredObjectExistenceState mirrorExistenceState,
						  StorageManagerMirrorMode mirrorMode,
						  ItemPointer persistentTid,
						  int64 *persistentSerialNum,
						  int *primaryError,
						  bool *mirrorDataLossOccurred)
{
	PersistentDatabase_MarkJustInTimeCreatePending(justInTimeDbDirNode,
												   mirrorExistenceState,
												   persistentTid,
												   persistentSerialNum);

	mdcreatedbdir(justInTimeDbDirNode,
				  mirrorMode,
				  /* ignoreAlreadyExists */ true,
				  primaryError,
				  mirrorDataLossOccurred);
	if (*primaryError != 0)
	{
		PersistentDatabase_AbandonJustInTimeCreatePending(justInTimeDbDirNode,
														  persistentTid,
														  *persistentSerialNum);
		return;
	}

	PersistentDatabase_JustInTimeCreated(justInTimeDbDirNode,
										 persistentTid,
										 *persistentSerialNum);

	/* be sure to set PG_VERSION file for just in time dirs too */
	set_short_version(NULL, justInTimeDbDirNode, true);
}

/*
 *	smgrcreate() -- Create a new relation.
 *
 *		Given an already-created (but presumably unused) SMgrRelation,
 *		cause the underlying disk file or other storage for the fork
 *		to be created.
 *
 *		We assume the persistent 'Create Pending' work has already been done.
 *
 *		And, we assume the Just-In-Time database directory in the tablespace has already
 *		been created.
 */
void
smgrmirroredcreate(SMgrRelation reln,
				   char *relationName, /* For tracing only.  Can be NULL in some execution paths. */
				   MirrorDataLossTrackingState mirrorDataLossTrackingState,
				   int64 mirrorDataLossTrackingSessionNum,
				   bool ignoreAlreadyExists,
				   bool *mirrorDataLossOccurred) /* FIXME: is this arg still needed? */
{
	mdmirroredcreate(reln,
					 relationName,
					 mirrorDataLossTrackingState,
					 mirrorDataLossTrackingSessionNum,
					 ignoreAlreadyExists,
					 mirrorDataLossOccurred);
}

/*
 *	smgrcreate() -- Create a new relation.
 *
 *		Given an already-created (but presumably unused) SMgrRelation,
 *		cause the underlying disk file or other storage for the fork
 *		to be created.
 *
 *		If isRedo is true, it is okay for the underlying file to exist
 *		already because we are in a WAL replay sequence.
 *
 *
 * In GPDB, this is currently only used for non-main forks, which are
 * not mirrored. smgrmirroredcreate must be used for the main fork.
 */
void
smgrcreate(SMgrRelation reln, ForkNumber forknum, bool isRedo)
{
	/*
	 * GPDB_84_MERGE_FIXME: the following performance tweak came in from 8.4;
	 * is it still applicable to our system here?
	 */
#if 0
	/*
	 * Exit quickly in WAL replay mode if we've already opened the file. If
	 * it's open, it surely must exist.
	 */
	if (isRedo && reln->md_fd[forknum] != NULL)
		return;
#endif

	mdcreate(reln, forknum, isRedo);
}


/*
 *	smgrdounlink() -- Immediately unlink a relation.
 *
 *		The specified fork of the relation is removed from the store.  This
 *		should not be used during transactional operations, since it can't be
 *		undone.
 *
 *		If isRedo is true, it is okay for the underlying file to be gone
 *		already.
 */
void
smgrdomirroredunlink(RelFileNode *relFileNode,
	bool isLocalBuf,
	char *relationName, /* For tracing only.  Can be NULL in some execution paths. */
	bool primaryOnly,
	bool isRedo,
	bool ignoreNonExistence,
	bool *mirrorDataLossOccurred)
{
	/*
	 * Get rid of any remaining buffers for the relation.  bufmgr will just
	 * drop them without bothering to write the contents.
	 */
	DropRelFileNodeBuffers(*relFileNode, MAIN_FORKNUM, isLocalBuf, 0);

	/*
	 * It'd be nice to tell the stats collector to forget it immediately, too.
	 * But we can't because we don't know the OID (and in cases involving
	 * relfilenode swaps, it's not always clear which table OID to forget,
	 * anyway).
	 */

	/*
	 * And delete the physical files.
	 *
	 * Note: smgr_unlink must treat deletion failure as a WARNING, not an
	 * ERROR, because we've already decided to commit or abort the current
	 * xact.
	 */
	mdmirroredunlink(*relFileNode,
					 relationName,
					 primaryOnly,
					 isRedo,
					 ignoreNonExistence,
					 mirrorDataLossOccurred);
}

void
smgrdounlink(SMgrRelation reln, ForkNumber forknum, bool isTemp, bool isRedo)
{
	RelFileNode rnode = reln->smgr_rnode;

	/* Close the fork */
	mdclose(reln, forknum);

	DropRelFileNodeBuffers(rnode, forknum, isTemp, 0);

	mdunlink(rnode, forknum, isRedo);
}

void
smgrdormfilespacedir(Oid filespaceOid,
					 char *primaryFilespaceLocation,

 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */
					 char *mirrorFilespaceLocation,
					 bool primaryOnly,
					 bool mirrorOnly,
					 bool ignoreNonExistence,
					 bool *mirrorDataLossOccurred)
{
	/*
	 * And remove the physical filespace directory.
	 *
	 * Note: we treat deletion failure as a WARNING, not an error, because
	 * we've already decided to commit or abort the current xact.
	 */
	if (!mdrmfilespacedir(filespaceOid,
						  primaryFilespaceLocation,
						  mirrorFilespaceLocation,
						  primaryOnly,
						  mirrorOnly,
						  ignoreNonExistence,
						  mirrorDataLossOccurred))
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not remove filespace directory %u: %m",
						filespaceOid)));
}

void
smgrdormtablespacedir(Oid tablespaceOid,
					  bool primaryOnly,
					  bool mirrorOnly,
					  bool ignoreNonExistence,
					  bool *mirrorDataLossOccurred)
{
	/*
	 * And remove the physical tablespace directory.
	 *
	 * Note: we treat deletion failure as a WARNING, not an error, because
	 * we've already decided to commit or abort the current xact.
	 */
	if (!mdrmtablespacedir(tablespaceOid, primaryOnly, mirrorOnly, ignoreNonExistence, mirrorDataLossOccurred))
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not remove tablespace directory %u: %m",
						tablespaceOid)));
}

/*
 * Shared subroutine that actually does the rmdir of a database directory ...
 */
static void
smgr_internal_rmdbdir(DbDirNode *dbDirNode,
					  bool primaryOnly,
					  bool mirrorOnly,
					  bool ignoreNonExistence,
					  bool *mirrorDataLossOccurred)
{
	/*
	 * And remove the physical database directory.
	 *
	 * Note: we treat deletion failure as a WARNING, not an error, because
	 * we've already decided to commit or abort the current xact.
	 */
	if (!mdrmdbdir(dbDirNode, primaryOnly, mirrorOnly, ignoreNonExistence, mirrorDataLossOccurred))
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not remove database directory %u/%u: %m",
						dbDirNode->tablespace,
						dbDirNode->database)));
}

void
smgrdormdbdir(DbDirNode *dbDirNode,
			  bool primaryOnly,
			  bool mirrorOnly,
			  bool ignoreNonExistence,
			  bool *mirrorDataLossOccurred)
{
	smgr_internal_rmdbdir(dbDirNode,
						  primaryOnly,
						  mirrorOnly,
						  ignoreNonExistence,
						  mirrorDataLossOccurred);
}

/*
 *	smgrextend() -- Add a new block to a file.
 *
 *		The semantics are nearly the same as smgrwrite(): write at the
 *		specified position.  However, this is to be used for the case of
 *		extending a relation (i.e., blocknum is at or beyond the current
 *		EOF).  Note that we assume writing a block beyond current EOF
 *		causes intervening file space to become filled with zeroes.
 *		failure we clean up by truncating.
 */
void
smgrextend(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, 
		   char *buffer, bool isTemp)
{
	mdextend(reln, forknum, blocknum, buffer, isTemp);
}

/*
 *	smgrread() -- read a particular block from a relation into the supplied
 *				  buffer.
 *
 *		This routine is called from the buffer manager in order to
 *		instantiate pages in the shared buffer cache.  All storage managers
 *		return pages in the format that POSTGRES expects.
 */
void
smgrread(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, 
		 char *buffer)
{
	mdread(reln, forknum, blocknum, buffer);
}

/*
 *	smgrwrite() -- Write the supplied buffer out.
 *
 *		This is to be used only for updating already-existing blocks of a
 *		relation (ie, those before the current EOF).  To extend a relation,
 *		use smgrextend().
 *
 *		This is not a synchronous write -- the block is not necessarily
 *		on disk at return, only dumped out to the kernel.  However,
 *		provisions will be made to fsync the write before the next checkpoint.
 *
 *		isTemp indicates that the relation is a temp table (ie, is managed
 *		by the local-buffer manager).  In this case no provisions need be
 *		made to fsync the write before checkpointing.
 */
void
smgrwrite(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, 
		  char *buffer, bool isTemp)
{
		mdwrite(reln, forknum, blocknum, buffer, isTemp);
}

/*
 *	smgrnblocks() -- Calculate the number of blocks in the
 *					 supplied relation.
 */
BlockNumber
smgrnblocks(SMgrRelation reln, ForkNumber forknum)
{
	return mdnblocks(reln, forknum);
}

/*
 *	smgrtruncate() -- Truncate supplied relation to the specified number
 *					  of blocks
 */
void
smgrtruncate(SMgrRelation reln, ForkNumber forknum, BlockNumber nblocks,
			 bool isTemp)
{
	/*
	 * Get rid of any buffers for the about-to-be-deleted blocks. bufmgr will
	 * just drop them without bothering to write the contents.
	 */
	DropRelFileNodeBuffers(reln->smgr_rnode, forknum, isTemp, nblocks);

	/* Do the truncation */
	// GPDB_84_MERGE_FIXME: is allowedNotFound = false correct here?
	mdtruncate(reln, forknum, nblocks, isTemp, false /* allowedNotFound */);
}

/*
 *	smgrimmedsync() -- Force the specified relation to stable storage.
 *
 *		Synchronously force all previous writes to the specified relation
 *		down to disk.
 *
 *		This is useful for building completely new relations (eg, new
 *		indexes).  Instead of incrementally WAL-logging the index build
 *		steps, we can just write completed index pages to disk with smgrwrite
 *		or smgrextend, and then fsync the completed index file before
 *		committing the transaction.  (This is sufficient for purposes of
 *		crash recovery, since it effectively duplicates forcing a checkpoint
 *		for the completed index.  But it is *not* sufficient if one wishes
 *		to use the WAL log for PITR or replication purposes: in that case
 *		we have to make WAL entries as well.)
 *
 *		The preceding writes should specify isTemp = true to avoid
 *		duplicative fsyncs.
 *
 *		Note that you need to do FlushRelationBuffers() first if there is
 *		any possibility that there are dirty buffers for the relation;
 *		otherwise the sync is not very meaningful.
 */
void
smgrimmedsync(SMgrRelation reln, ForkNumber forknum)
{
	mdimmedsync(reln, forknum);
}

/*
 *	smgrpreckpt() -- Prepare for checkpoint.
 */
void
smgrpreckpt(void)
{
	mdpreckpt();
}

/*
 *     smgrsync() -- Sync files to disk during checkpoint.
 */
void
smgrsync(void)
{
	mdsync();
}

/*
 *	smgrpostckpt() -- Post-checkpoint cleanup.
 */
void
smgrpostckpt(void)
{
	mdpostckpt();
}
