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
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/smgr/smgr.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "commands/tablespace.h"
#include "postmaster/postmaster.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/smgr.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "utils/inval.h"

/*
 * Each backend has a hashtable that stores all extant SMgrRelation objects.
 */
static HTAB *SMgrRelationHash = NULL;

/* local function prototypes */
static void smgrshutdown(int code, Datum arg);


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
smgropen(RelFileNode rnode, BackendId backend)
{
	RelFileNodeBackend brnode;
	SMgrRelation reln;
	bool		found;

	/* GPDB: don't support MyBackendId as a possible backend. */
	Assert(backend == InvalidBackendId || backend == TempRelBackendId);

	if (SMgrRelationHash == NULL)
	{
		/* First time through: initialize the hash table */
		HASHCTL		ctl;

		MemSet(&ctl, 0, sizeof(ctl));
		ctl.keysize = sizeof(RelFileNodeBackend);
		ctl.entrysize = sizeof(SMgrRelationData);
		ctl.hash = tag_hash;
		SMgrRelationHash = hash_create("smgr relation table", 400,
									   &ctl, HASH_ELEM | HASH_FUNCTION);
	}

	/* Look up or create an entry */
	brnode.node = rnode;
	brnode.backend = backend;
	reln = (SMgrRelation) hash_search(SMgrRelationHash,
									  (void *) &brnode,
									  HASH_ENTER, &found);

	/* Initialize it if not present before */
	if (!found)
	{
		int			forknum;

		/* hash_search already filled in the lookup key */
		reln->smgr_owner = NULL;
		reln->smgr_targblock = InvalidBlockNumber;
		reln->smgr_fsm_nblocks = InvalidBlockNumber;
		reln->smgr_vm_nblocks = InvalidBlockNumber;
		reln->smgr_transient = false;
		reln->smgr_which = 0;	/* we only have md.c at present */

		/* mark it not open */
		for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
			reln->md_fd[forknum] = NULL;
	}
	else
		/* if it was transient before, it no longer is */
		reln->smgr_transient = false;

	return reln;
}

/*
 * smgrsettransient() -- mark an SMgrRelation object as transaction-bound
 *
 * The main effect of this is that all opened files are marked to be
 * kernel-level closed (but not necessarily VFD-closed) when the current
 * transaction ends.
 */
void
smgrsettransient(SMgrRelation reln)
{
	reln->smgr_transient = true;
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
	ForkNumber	forknum;

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
smgrclosenode(RelFileNodeBackend rnode)
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
 *	smgrcreate() -- Create a new relation.
 *
 *		Given an already-created (but presumably unused) SMgrRelation,
 *		cause the underlying disk file or other storage for the fork
 *		to be created.
 *
 *		If isRedo is true, it is okay for the underlying file to exist
 *		already because we are in a WAL replay sequence.
 */
void
smgrcreate(SMgrRelation reln, ForkNumber forknum, bool isRedo)
{
	/*
	 * Exit quickly in WAL replay mode if we've already opened the file. If
	 * it's open, it surely must exist.
	 */
	if (isRedo && reln->md_fd[forknum] != NULL)
		return;

	/*
	 * We may be using the target table space for the first time in this
	 * database, so create a per-database subdirectory if needed.
	 *
	 * XXX this is a fairly ugly violation of module layering, but this seems
	 * to be the best place to put the check.  Maybe TablespaceCreateDbspace
	 * should be here and not in commands/tablespace.c?  But that would imply
	 * importing a lot of stuff that smgr.c oughtn't know, either.
	 */
	TablespaceCreateDbspace(reln->smgr_rnode.node.spcNode,
							reln->smgr_rnode.node.dbNode,
							isRedo);

	mdcreate(reln, forknum, isRedo);
}

/*
 *	smgrcreate_ao() -- Create a new AO relation segment.
 *
 *		Given a RelFileNode, cause the underlying disk file for the
 *		AO segment to be created.
 *
 *		If isRedo is true, it is okay for the underlying file to exist
 *		already because we are in a WAL replay sequence.
 */
void
smgrcreate_ao(RelFileNodeBackend rnode, int32 segmentFileNum, bool isRedo)
{
	mdcreate_ao(rnode, segmentFileNum, isRedo);
}


void
smgrdounlink(SMgrRelation reln, ForkNumber forknum, bool isRedo, char relstorage)
{
	/*
	 * AO/CO tables have only MAIN_FORKNUM we should exit early to prevent
	 * extra work.
	 */
	if (relstorage_is_ao(relstorage) &&
		forknum != MAIN_FORKNUM)
		return;


	RelFileNodeBackend rnode = reln->smgr_rnode;

	/* Close the fork */
	mdclose(reln, forknum);

	/*
	 * Get rid of any remaining buffers for the relation.  bufmgr will just
	 * drop them without bothering to write the contents.
	 *
	 * Apart from relstorage == RELSTORAGE_HEAP do any other RELSTOARGE type
	 * expected to have buffers in shared memory ? Can check only for
	 * RELSTORAGE_HEAP below.
	 */
	if ((relstorage != RELSTORAGE_AOROWS) &&
		(relstorage != RELSTORAGE_AOCOLS))
		DropRelFileNodeBuffers(rnode, forknum, 0);

	/*
	 * It'd be nice to tell the stats collector to forget it immediately, too.
	 * But we can't because we don't know the OID (and in cases involving
	 * relfilenode swaps, it's not always clear which table OID to forget,
	 * anyway).
	 */

	/*
	 * Send a shared-inval message to force other backends to close any
	 * dangling smgr references they may have for this rel.  We should do this
	 * before starting the actual unlinking, in case we fail partway through
	 * that step.  Note that the sinval message will eventually come back to
	 * this backend, too, and thereby provide a backstop that we closed our
	 * own smgr rel.
	 */
	CacheInvalidateSmgr(rnode);

	/*
	 * Delete the physical file(s).
	 *
	 * Note: smgr_unlink must treat deletion failure as a WARNING, not an
	 * ERROR, because we've already decided to commit or abort the current
	 * xact.
	 */
	mdunlink(rnode, forknum, isRedo, relstorage);
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
		   char *buffer, bool skipFsync)
{
	mdextend(reln, forknum, blocknum, buffer, skipFsync);
}

/*
 *	smgrprefetch() -- Initiate asynchronous read of the specified block of a relation.
 */
void
smgrprefetch(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum)
{
	mdprefetch(reln, forknum, blocknum);
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
 *		skipFsync indicates that the caller will make other provisions to
 *		fsync the relation, so we needn't bother.  Temporary relations also
 *		do not require fsync.
 */
void
smgrwrite(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum,
		  char *buffer, bool skipFsync)
{
	mdwrite(reln, forknum, blocknum, buffer, skipFsync);
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
 *
 * The truncation is done immediately, so this can't be rolled back.
 */
void
smgrtruncate(SMgrRelation reln, ForkNumber forknum, BlockNumber nblocks)
{
	/*
	 * Get rid of any buffers for the about-to-be-deleted blocks. bufmgr will
	 * just drop them without bothering to write the contents.
	 */
	DropRelFileNodeBuffers(reln->smgr_rnode, forknum, nblocks);

	/*
	 * Send a shared-inval message to force other backends to close any smgr
	 * references they may have for this rel.  This is useful because they
	 * might have open file pointers to segments that got removed, and/or
	 * smgr_targblock variables pointing past the new rel end.	(The inval
	 * message will come back to our backend, too, causing a
	 * probably-unnecessary local smgr flush.  But we don't expect that this
	 * is a performance-critical path.)  As in the unlink code, we want to be
	 * sure the message is sent before we start changing things on-disk.
	 */
	CacheInvalidateSmgr(reln->smgr_rnode);

	/*
	 * Do the truncation.
	 */
	mdtruncate(reln, forknum, nblocks);
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
 *		The preceding writes should specify skipFsync = true to avoid
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
