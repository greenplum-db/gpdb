/*-------------------------------------------------------------------------
 *
 * md.c
 *	  This code manages relations that reside on magnetic disk.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/smgr/md.c,v 1.138 2008/05/02 01:08:27 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "catalog/catalog.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "utils/faultinjector.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "cdb/cdbmirroredbufferpool.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbfilerepprimary.h"
#include "catalog/pg_tablespace.h"


/* interval for calling AbsorbFsyncRequests in mdsync */
#define FSYNCS_PER_ABSORB		10

/*
 * Special values for the segno arg to RememberFsyncRequest.
 *
 * Note that CompactCheckpointerRequestQueue assumes that it's OK to remove an
 * fsync request from the queue if an identical, subsequent request is found.
 * See comments there before making changes here.
 */
#define FORGET_RELATION_FSYNC	(InvalidBlockNumber)
#define FORGET_DATABASE_FSYNC	(InvalidBlockNumber-1)
#define UNLINK_RELATION_REQUEST (InvalidBlockNumber-2)

/*
 * On Windows, we have to interpret EACCES as possibly meaning the same as
 * ENOENT, because if a file is unlinked-but-not-yet-gone on that platform,
 * that's what you get.  Ugh.  This code is designed so that we don't
 * actually believe these cases are okay without further evidence (namely,
 * a pending fsync request getting revoked ... see mdsync).
 */
#ifndef WIN32
#define FILE_POSSIBLY_DELETED(err)	((err) == ENOENT)
#else
#define FILE_POSSIBLY_DELETED(err)	((err) == ENOENT || (err) == EACCES)
#endif

/*
 *	The magnetic disk storage manager keeps track of open file
 *	descriptors in its own descriptor pool.  This is done to make it
 *	easier to support relations that are larger than the operating
 *	system's file size limit (often 2GBytes).  In order to do that,
 *	we break relations up into "segment" files that are each shorter than
 *	the OS file size limit.  The segment size is set by the RELSEG_SIZE
 *	configuration constant in pg_config.h.
 *
 *	On disk, a relation must consist of consecutively numbered segment
 *	files in the pattern
 *		-- Zero or more full segments of exactly RELSEG_SIZE blocks each
 *		-- Exactly one partial segment of size 0 <= size < RELSEG_SIZE blocks
 *		-- Optionally, any number of inactive segments of size 0 blocks.
 *	The full and partial segments are collectively the "active" segments.
 *	Inactive segments are those that once contained data but are currently
 *	not needed because of an mdtruncate() operation.  The reason for leaving
 *	them present at size zero, rather than unlinking them, is that other
 *	backends and/or the checkpointer might be holding open file references to
 *	such segments.	If the relation expands again after mdtruncate(), such
 *	that a deactivated segment becomes active again, it is important that
 *	such file references still be valid --- else data might get written
 *	out to an unlinked old copy of a segment file that will eventually
 *	disappear.
 *
 *	The file descriptor pointer (md_fd field) stored in the SMgrRelation
 *	cache is, therefore, just the head of a list of MdfdVec objects, one
 *	per segment.  But note the md_fd pointer can be NULL, indicating
 *	relation not open.
 *
 *	Also note that mdmir_chain == NULL does not necessarily mean the relation
 *	doesn't have another segment after this one; we may just not have
 *	opened the next segment yet.  (We could not have "all segments are
 *	in the chain" as an invariant anyway, since another backend could
 *	extend the relation when we weren't looking.)  We do not make chain
 *	entries for inactive segments, however; as soon as we find a partial
 *	segment, we assume that any subsequent segments are inactive.
 *
 *	All MdfdVec objects are palloc'd in the MdCxt memory context.
 */

typedef struct _MdMirVec
{
	MirroredBufferPoolOpen		mdmir_open;

	BlockNumber mdmir_segno;		/* segment number, from 0 */
	struct _MdMirVec *mdmir_chain;	/* next segment, or NULL */
} MdMirVec;

static MemoryContext MdCxt;		/* context for all md.c allocations */


/*
 * In some contexts (currently, standalone backends and the checkpointer process)
 * we keep track of pending fsync operations: we need to remember all relation
 * segments that have been written since the last checkpoint, so that we can
 * fsync them down to disk before completing the next checkpoint.  This hash
 * table remembers the pending operations.	We use a hash table mostly as
 * a convenient way of eliminating duplicate requests.
 *
 * We use a similar mechanism to remember no-longer-needed files that can
 * be deleted after the next checkpoint, but we use a linked list instead of
 * a hash table, because we don't expect there to be any duplicate requests.
 *
 * (Regular backends do not track pending operations locally, but forward
 * them to the checkpointer.)
 */
typedef struct
{
	RelFileNode rnode;			/* the targeted relation */
	BlockNumber segno;			/* which segment */
} PendingOperationTag;

typedef uint16 CycleCtr;		/* can be any convenient integer size */

typedef struct
{
	PendingOperationTag tag;	/* hash table key (must be first!) */
	bool		canceled;		/* T => request canceled, not yet removed */
	CycleCtr	cycle_ctr;		/* mdsync_cycle_ctr when request was made */
} PendingOperationEntry;

typedef struct
{
	RelFileNode rnode;			/* the dead relation to delete */
	CycleCtr	cycle_ctr;		/* mdckpt_cycle_ctr when request was made */
} PendingUnlinkEntry;

static HTAB *pendingOpsTable = NULL;
static List *pendingUnlinks = NIL;

static CycleCtr mdsync_cycle_ctr = 0;
static CycleCtr mdckpt_cycle_ctr = 0;

typedef enum					/* behavior for mdopen & _mdfd_getseg */
{
	EXTENSION_FAIL,				/* ereport if segment not present */
	EXTENSION_RETURN_NULL,		/* return NULL if not present */
	EXTENSION_CREATE			/* create new segments as needed */
} ExtensionBehavior;

/* local routines */
static MdMirVec *mdopen(SMgrRelation reln, ExtensionBehavior behavior);
static void register_dirty_segment(SMgrRelation reln, MdMirVec *seg);
static void register_unlink(RelFileNode rnode);
static MdMirVec *_mirvec_alloc(void);

static MdMirVec *_mdmir_openseg(SMgrRelation reln, BlockNumber segno,
			  bool createIfDoesntExist);
static MdMirVec *_mdmir_getseg(SMgrRelation reln, BlockNumber blkno,
			  bool isTemp, ExtensionBehavior behavior);
static BlockNumber _mdnblocks(SMgrRelation reln, MdMirVec *seg);


/*
 *	mdinit() -- Initialize private state for magnetic disk storage manager.
 */
void
mdinit(void)
{
	MdCxt = AllocSetContextCreate(TopMemoryContext,
								  "MdSmgr",
								  ALLOCSET_DEFAULT_MINSIZE,
								  ALLOCSET_DEFAULT_INITSIZE,
								  ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * Create pending-operations hashtable if we need it.  Currently, we need
	 * it if we are standalone (not under a postmaster) or if we are a startup
	 * or checkpointer auxiliary process.
	 */
	if (!IsUnderPostmaster || AmStartupProcess() || AmCheckpointerProcess())
	{
		HASHCTL		hash_ctl;

		MemSet(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(PendingOperationTag);
		hash_ctl.entrysize = sizeof(PendingOperationEntry);
		hash_ctl.hash = tag_hash;
		hash_ctl.hcxt = MdCxt;
		pendingOpsTable = hash_create("Pending Ops Table",
									  100L,
									  &hash_ctl,
								   HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);
		pendingUnlinks = NIL;
	}
}

static int errdetail_nonexistent_relation(
	int				error,

	RelFileNode		*relFileNode)
{
#define MAX_MSG_LEN MAXPGPATH + 100

	char *buffer;
	int snprintfResult;

	char *primaryFilespaceLocation = NULL;
	char *mirrorFilespaceLocation = NULL;

	struct stat fst;

	if (!FILE_POSSIBLY_DELETED(error))
	{
		/*
		 * No call to errdetail.
		 */
		return 0;
	}

	buffer = (char*)palloc(MAX_MSG_LEN);

	if (IsBuiltinTablespace(relFileNode->spcNode))
	{
		/*
		 * Assume filespace and tablespace exists.
		 */
	}
	else
	{		
		char *tablespacePath;

		/*
		 * Investigate whether the containing directories exist to give more detail.
		 */
		PersistentTablespace_GetPrimaryAndMirrorFilespaces(
											relFileNode->spcNode,
											&primaryFilespaceLocation,
											&mirrorFilespaceLocation);
		if (primaryFilespaceLocation == NULL ||
			strlen(primaryFilespaceLocation) == 0)
		{
			elog(ERROR, "Empty primary filespace directory location");
		}

		if (mirrorFilespaceLocation != NULL)
		{
			pfree(mirrorFilespaceLocation);
			mirrorFilespaceLocation = NULL;
		}

		/*
		 * Does the filespace directory exist?
		 */
		if (stat(primaryFilespaceLocation, &fst) < 0)
		{
			int saved_err;

			saved_err = errno;

			if (FILE_POSSIBLY_DELETED(saved_err))
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Filespace directory \"%s\" does not exist",
						primaryFilespaceLocation);
			}
			else
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Filespace directory \"%s\" existence check: %s",
						primaryFilespaceLocation,
						strerror(saved_err));
			}
			
			Assert(snprintfResult >= 0);
			Assert(snprintfResult < MAX_MSG_LEN);

			/*
			 * Give DETAIL on the filespace directory!
			 */
			errdetail("%s", buffer);
			pfree(buffer);

			pfree(primaryFilespaceLocation);
			return 0;
		}
		else
		{
			if (Debug_persistent_recovery_print)
			{
				/*
				 * Since we are being invoked inside an elog, we don't want to
				 * recurse.  So, use errdetail instead!
				 */
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Filespace directory \"%s\" does exist",
						primaryFilespaceLocation);
				
				Assert(snprintfResult >= 0);
				Assert(snprintfResult < MAX_MSG_LEN);
				
				errdetail("%s",buffer);
			}
		}
	
		tablespacePath = (char*)palloc(MAXPGPATH + 1);

		FormTablespacePath(
					tablespacePath,
					primaryFilespaceLocation,
					relFileNode->spcNode);

		/*
		 * Does the tablespace directory exist?
		 */
		if (stat(tablespacePath, &fst) < 0)
		{
			int saved_err;

			saved_err = errno;

			if (FILE_POSSIBLY_DELETED(saved_err))
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Tablespace directory \"%s\" does not exist",
						tablespacePath);
			}
			else
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Tablespace directory \"%s\" existence check: %s",
						tablespacePath,
						strerror(saved_err));
			}
			
			Assert(snprintfResult >= 0);
			Assert(snprintfResult < MAX_MSG_LEN);

			/*
			 * Give DETAIL on the tablespace directory!
			 */
			errdetail("%s", buffer);
			pfree(buffer);

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);

			pfree(tablespacePath);
			return 0;
		}
		else
		{
			if (Debug_persistent_recovery_print)
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Tablespace directory \"%s\" does exist",
						tablespacePath);
				
				Assert(snprintfResult >= 0);
				Assert(snprintfResult < MAX_MSG_LEN);
				
				errdetail("%s", buffer);
			}
		}

		pfree(tablespacePath);
	}

	if (relFileNode->spcNode != GLOBALTABLESPACE_OID)
	{
		char *dbPath;

		dbPath = (char*)palloc(MAXPGPATH + 1);
		
		FormDatabasePath(
					dbPath,
					primaryFilespaceLocation,
					relFileNode->spcNode,
					relFileNode->dbNode);

		/*
		 * Does the database directory exist?
		 */
		if (stat(dbPath, &fst) < 0)
		{
			int saved_err;
		
			saved_err = errno;
		
			if (FILE_POSSIBLY_DELETED(saved_err))
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Database directory \"%s\" does not exist",
						dbPath);
			}
			else
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Database directory \"%s\" existence check: %s",
						dbPath,
						strerror(saved_err));
			}
			
			Assert(snprintfResult >= 0);
			Assert(snprintfResult < MAX_MSG_LEN);
		
			/*
			 * Give DETAIL on the database directory!
			 */
			errdetail("%s", buffer);
			pfree(buffer);
		
			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);

			pfree(dbPath);
			return 0;
		}
		else
		{
			if (Debug_persistent_recovery_print)
			{
				snprintfResult = 
					snprintf(
						buffer,
						MAX_MSG_LEN,
						"Database directory \"%s\" does exist",
						dbPath);
				
				Assert(snprintfResult >= 0);
				Assert(snprintfResult < MAX_MSG_LEN);
				
				errdetail("%s", buffer);
			}
		}

		pfree(dbPath);
	}
	
	pfree(buffer);
	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);

	/*
	 * No (normal) call to errdetail.
	 */
	return 0;
}

/*
 *	mdcreatefilespacedir() -- Create a new filespace directory on magnetic disk.
 *
 * If isRedo is true, it's okay for the filespace directory to exist already.
 */
void
mdcreatefilespacedir(
	Oid 						filespaceOid,

	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */

	char						*mirrorFilespaceLocation,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred)
{
	int mirrorStatus = FileRepStatusSuccess;
	*primaryError = 0;
	*mirrorDataLossOccurred = false;

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		mirrorStatus = FileRepPrimary_MirrorCreate(
												   FileRep_GetDirFilespaceIdentifier(
																					 mirrorFilespaceLocation),
												   FileRepRelationTypeDir,
												   ignoreAlreadyExists);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();		
	}

	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		if (mkdir(primaryFilespaceLocation, 0700) < 0)
			*primaryError = errno;
	}
	
	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_IsOperationCompleted(
											FileRep_GetDirFilespaceIdentifier(
																			  mirrorFilespaceLocation),
											FileRepRelationTypeDir);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
		
		if (! *mirrorDataLossOccurred)
		{
			mirrorStatus = FileRepPrimary_GetMirrorStatus();

			if (mirrorStatus != FileRepStatusSuccess) 
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_COMMAND_ERROR),
						 errmsg("could not create dbdir on mirror '%s' ",
						 FileRepStatusToString[mirrorStatus])));
			}
		}
	}
}

/*
 *	mdcreatetablespacedir() -- Create a new tablespace directory on magnetic disk.
 *
 * If isRedo is true, it's okay for the tablespace directory to exist already.
 */
void
mdcreatetablespacedir(
	Oid							tablespaceOid,

	StorageManagerMirrorMode 	mirrorMode,

	bool						ignoreAlreadyExists,

	int							*primaryError,

	bool						*mirrorDataLossOccurred)
{
	char *primaryFilespaceLocation;
	char *mirrorFilespaceLocation;
	char tablespacePath[MAXPGPATH];

	*primaryError = 0;
	*mirrorDataLossOccurred = false;
	
	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
										tablespaceOid,
										&primaryFilespaceLocation,
										&mirrorFilespaceLocation);

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_MirrorCreate(
									FileRep_GetDirTablespaceIdentifier(
																	   mirrorFilespaceLocation,
																	   tablespaceOid),
									FileRepRelationTypeDir,
									TRUE);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();		
	}
	
	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		FormTablespacePath(
					tablespacePath,
					primaryFilespaceLocation,
					tablespaceOid);

		if (mkdir(tablespacePath, 0700) < 0)
			*primaryError = errno;
	}

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_IsOperationCompleted(
											FileRep_GetDirTablespaceIdentifier(
																			   mirrorFilespaceLocation,
																			   tablespaceOid),
											FileRepRelationTypeDir);	
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}
		
	if (primaryFilespaceLocation)
		pfree(primaryFilespaceLocation);
	if (mirrorFilespaceLocation)
		pfree(mirrorFilespaceLocation);
}

/*
 *	mdcreatedbdir() -- Create a new database directory on magnetic disk.
 *
 * If isRedo is true, it's okay for the database directory to exist already.
 */
void
mdcreatedbdir(
	DbDirNode					*dbDirNode,

	StorageManagerMirrorMode 	mirrorMode,

	bool						ignoreAlreadyExists,

	int							*primaryError,

	bool						*mirrorDataLossOccurred)
{
	char *primaryFilespaceLocation;
	char *mirrorFilespaceLocation;
	char dbPath[MAXPGPATH];

	*primaryError = 0;
	*mirrorDataLossOccurred = false;
	
	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
										dbDirNode->tablespace,
										&primaryFilespaceLocation,
										&mirrorFilespaceLocation);
	
	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_MirrorCreate(
									FileRep_GetDirDatabaseIdentifier(
																	 mirrorFilespaceLocation,
																	 *dbDirNode),
									FileRepRelationTypeDir,
									TRUE);

		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}

	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		FormDatabasePath(
					dbPath,
					primaryFilespaceLocation,
					dbDirNode->tablespace,
					dbDirNode->database);

		if (mkdir(dbPath, 0700) < 0)
		{
			if (ignoreAlreadyExists && (errno == EEXIST))
			{
				elog(LOG, "Directory already exists %s", dbPath);
			}
			else
			{
				*primaryError = errno;
			}
		}
	}
	
	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_IsOperationCompleted(
											FileRep_GetDirDatabaseIdentifier(
																			 mirrorFilespaceLocation,
																			 *dbDirNode),
											FileRepRelationTypeDir);

		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}

	if (primaryFilespaceLocation)
		pfree(primaryFilespaceLocation);
	if (mirrorFilespaceLocation)
		pfree(mirrorFilespaceLocation);
}


/*
 *	mdcreate() -- Create a new relation on magnetic disk.
 */
void
mdcreate(
	SMgrRelation 				reln,

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */

	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	bool						ignoreAlreadyExists,

	bool						*mirrorDataLossOccurred)
{
	MirroredBufferPoolOpen		mirroredOpen;
	int			primaryError;

	*mirrorDataLossOccurred = false;

	if (reln->md_mirvec != NULL)
		mdclose(reln);		// Don't assume it has been created -- make sure it gets created on both mirrors.

	Assert(reln->md_mirvec == NULL);
	
	MirroredBufferPool_Create(
					&mirroredOpen,
					&reln->smgr_rnode,
					/* segmentFileNum */ 0,
					relationName,
					mirrorDataLossTrackingState,
					mirrorDataLossTrackingSessionNum,
					&primaryError,
					mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		int			openPrimaryError;

		/*
		 * During bootstrap, there are cases where a system relation will be
		 * accessed (by internal backend processes) before the bootstrap
		 * script nominally creates it.  Therefore, allow the file to exist
		 * already, even if isRedo is not set.	(See also mdopen)
		 */
		if (ignoreAlreadyExists || IsBootstrapProcessingMode())
			MirroredBufferPool_Open(
						&mirroredOpen,
						&reln->smgr_rnode,
						/* segmentFileNum */ 0,
						relationName,
						&openPrimaryError,
						mirrorDataLossOccurred);
		if (!MirroredBufferPool_IsActive(&mirroredOpen))
		{
			/* be sure to report the error reported by create, not open */
			errno = primaryError;
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not create relation file '%s', relation name '%s': %m",
							relpath(reln->smgr_rnode),
							relationName)));
		}
	}

	reln->md_mirvec = _mirvec_alloc();

	reln->md_mirvec->mdmir_open = mirroredOpen;
	reln->md_mirvec->mdmir_segno = 0;
	reln->md_mirvec->mdmir_chain = NULL;
}

/*
 *	mdunlink() -- Unlink a relation.
 *
 * Note that we're passed a RelFileNode --- by the time this is called,
 * there won't be an SMgrRelation hashtable entry anymore.
 *
 * Actually, we don't unlink the first segment file of the relation, but
 * just truncate it to zero length, and record a request to unlink it after
 * the next checkpoint.  Additional segments can be unlinked immediately,
 * however.  Leaving the empty file in place prevents that relfilenode
 * number from being reused.  The scenario this protects us from is:
 * 1. We delete a relation (and commit, and actually remove its file).
 * 2. We create a new relation, which by chance gets the same relfilenode as
 *	  the just-deleted one (OIDs must've wrapped around for that to happen).
 * 3. We crash before another checkpoint occurs.
 * During replay, we would delete the file and then recreate it, which is fine
 * if the contents of the file were repopulated by subsequent WAL entries.
 * But if we didn't WAL-log insertions, but instead relied on fsyncing the
 * file after populating it (as for instance CLUSTER and CREATE INDEX do),
 * the contents of the file would be lost forever.	By leaving the empty file
 * until after the next checkpoint, we prevent reassignment of the relfilenode
 * number until it's safe, because relfilenode assignment skips over any
 * existing file.
 *
 * If isRedo is true, it's okay for the relation to be already gone.
 * Also, we should remove the file immediately instead of queuing a request
 * for later, since during redo there's no possibility of creating a
 * conflicting relation.
 *
 * Note: any failure should be reported as WARNING not ERROR, because
 * we are usually not in a transaction anymore when this is called.
 */
void
mdunlink(
	RelFileNode 				rnode, 

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	
	bool  						primaryOnly,

	bool						isRedo,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred)
{
	int			 primaryError = 0;
	char		 tmp[MAXPGPATH];
	char	   *path;
	int			 segmentFileNum;

	/*
	 * We have to clean out any pending fsync requests for the doomed
	 * relation, else the next mdsync() will fail.
	 */
	ForgetRelationFsyncRequests(rnode);

	/* 
	 * Delete All segment file extensions
	 *
	 * This code used to be implemented via glob(), but globbing data is slow
	 * when there are many files in a directory, so using glob is to be avoided.
	 * Instead we perform point lookups for files and delete the ones we find.
	 * There are different rules for this depending on the type of table:
	 *
	 *   Heap Tables: contiguous extensions, no upper bound
	 *   AO Tables: non contiguous extensions [.1 - .127]
	 *   CO Tables: non contiguous extensions
	 *          [  .1 - .127] for first column
	 *          [.128 - .255] for second column
	 *          [.256 - .283] for third column
	 *          etc
	 *
	 * mdunlink is only called on Heap Tables, AO/CO tables are handled by a
	 * different code path.  The following logic assumes that the files are
	 * a single contiguous range of numbers.
	 *
	 * UNDONE: Probably should have mirror do pattern match too.
	 *    It is conceivably possible that the primary/mirror may have a 
	 *    different set of segment files, so doing the pattern match only
	 *    in one place is dangerous.
	 *
	 * UNDONE: This is broken for mirroring !!!
	 *    The fundamental problem is that if we drop a file on the mirror
	 *    then fail over before we have dropped the file on the primary
	 *    then the mirror is unaware that the file still needs to be dropped
	 *    on the old primary.  
	 *
	 * The above two issues are tracked in MPP-11724
	 */

	/* 
	 * We do this in two passes because it is safer to drop the files in reverse
	 * order so as to prevent the creation of holes, but we need to scan forward
	 * to know what files actually exist.
	 */
	path = relpath(rnode);
	for (segmentFileNum = 0; /* break in code */ ; segmentFileNum++)
	{
		struct stat sbuf;
		
		/* the zero segment file does not have the ".0" extension */
		if (segmentFileNum == 0)
			snprintf(tmp, sizeof(tmp), "%s", path);
		else
			snprintf(tmp, sizeof(tmp), "%s.%d", path, segmentFileNum);

		if (stat(tmp, &sbuf) < 0)
			break;  /* No such file, loop is done */
	}
	pfree(path);

	/* If the zero segment didn't exist raise an error if requested */
	if (segmentFileNum == 0)
	{
		if (!ignoreNonExistence)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("could not remove relation %u/%u/%u: %m",
							rnode.spcNode,
							rnode.dbNode,
							rnode.relNode)));
		}
		else
		{
			/* 
			 * Mirror can still have the file, so lets attempt to delete 
			 * atleast zero segment file.
			 */
			int temp_primaryError = 0;
			MirroredBufferPool_Drop(&rnode, segmentFileNum, relationName, 
						primaryOnly, isRedo, &temp_primaryError,
						mirrorDataLossOccurred);
		}
	}

	/* second pass perform the drops in reverse order: important for REDO */
	for(segmentFileNum--; segmentFileNum >= 0; segmentFileNum--)
	{
		MirroredBufferPool_Drop(&rnode, segmentFileNum, relationName, 
								primaryOnly, isRedo, &primaryError,
								mirrorDataLossOccurred);
		if (primaryError != 0)
		{
			if (segmentFileNum == 0)
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not remove relation %u/%u/%u: %m",
								rnode.spcNode,
								rnode.dbNode,
								rnode.relNode)));
			else
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not remove segment %u of relation %u/%u/%u: %m",
								segmentFileNum,
								rnode.spcNode,
								rnode.dbNode,
								rnode.relNode)));
			break;
		}
	}

	/*
	 * In PostgreSQL, register_unlink is called to let the checkpoint process to clean up the files.
	 * In GPDB, the cleanup is handled by persistent table. Hence, we don't need to register the
	 * unlink request.
	 */
#if 0 /* Upstream code not applicable to GPDB */
	/* Register request to unlink first segment later */
	if (!isRedo)
		register_unlink(rnode);
#endif
}

static void mdsetupdropobjmirroraccess(	
	bool						primaryOnly,

	bool						mirrorOnly,
	
	StorageManagerMirrorMode	*mirrorMode,

	bool						*mirrorDataLossOccurred)
{
	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64						mirrorDataLossTrackingSessionNum;

	*mirrorMode = StorageManagerMirrorMode_None;
	*mirrorDataLossOccurred = false;	// Assume.

	mirrorDataLossTrackingState = 
				FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
												&mirrorDataLossTrackingSessionNum);

	if (gp_initdb_mirrored)
	{
		/* Kludge for initdb */
		*mirrorMode = StorageManagerMirrorMode_Both;
	}
	else
	{
		switch (mirrorDataLossTrackingState)
		{
		case MirrorDataLossTrackingState_MirrorNotConfigured:
			if (mirrorOnly)
				elog(ERROR, "No mirror configured for mirror only");
			
			*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
			break;
			
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
			if (primaryOnly)
				*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
			else if (!mirrorOnly)
				*mirrorMode = StorageManagerMirrorMode_Both;
			else
				*mirrorMode = StorageManagerMirrorMode_MirrorOnly;
			break;
				
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
			if (primaryOnly)
				*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
			else if (!mirrorOnly)
				*mirrorMode = StorageManagerMirrorMode_Both;
			else
				*mirrorMode = StorageManagerMirrorMode_MirrorOnly;
			break;	
		case MirrorDataLossTrackingState_MirrorDown:
			if (!mirrorOnly)
				*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
			else
				*mirrorMode = StorageManagerMirrorMode_MirrorOnly;	// Mirror only operations fails from the outset.

			*mirrorDataLossOccurred = true; 	// Mirror communication is down.
			break;
			
		default:
			elog(ERROR, "Unexpected mirror data loss tracking state: %d",
				 mirrorDataLossTrackingState);
			*mirrorMode = StorageManagerMirrorMode_None; 	// A happy optimizer is the sound of one hand clapping.
		}
	}

}

/*
 *	mdrmfilespacedir() -- Remove a filespace directory on magnetic disk.
 *
 * If isRedo is true, it's okay for the filespace directory to exist already.
 */
bool
mdrmfilespacedir(
	Oid							filespaceOid,

	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */
	
	char						*mirrorFilespaceLocation,
	
	bool						primaryOnly,

	bool					 	mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred)
{
	StorageManagerMirrorMode	mirrorMode;

	bool result;

	*mirrorDataLossOccurred = false;

	mdsetupdropobjmirroraccess(
							primaryOnly,
							mirrorOnly,
							&mirrorMode,
							mirrorDataLossOccurred);

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_MirrorDrop(
								  FileRep_GetDirFilespaceIdentifier(
																	mirrorFilespaceLocation),
								  FileRepRelationTypeDir);

		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}
			
	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		// The rmtree routine unfortunately emits errors, so there is not errno available...  
		// Just a bool.
		result = rmtree(primaryFilespaceLocation, true);
	}
	else
		result = true;

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_IsOperationCompleted(
											FileRep_GetDirFilespaceIdentifier(
																			  mirrorFilespaceLocation),
											FileRepRelationTypeDir);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();		
	}

	return result;
}

/*
 *	mdrmtablespacedir() -- Remove a tablespace directory on magnetic disk.
 *
 * If isRedo is true, it's okay for the tablespace directory to exist already.
 */
bool
mdrmtablespacedir(
	Oid							tablespaceOid,
	
	bool						primaryOnly,

	bool					 	mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred)
{
	StorageManagerMirrorMode	mirrorMode;

	char *primaryFilespaceLocation;
	char *mirrorFilespaceLocation;
	char tablespacePath[MAXPGPATH];
	bool result;

	*mirrorDataLossOccurred = false;

	mdsetupdropobjmirroraccess(
							primaryOnly,
							mirrorOnly,
							&mirrorMode,
							mirrorDataLossOccurred);

	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
										tablespaceOid,
										&primaryFilespaceLocation,
										&mirrorFilespaceLocation);

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_MirrorDrop(
								  FileRep_GetDirTablespaceIdentifier(
																	 mirrorFilespaceLocation,
																	 tablespaceOid),
								  FileRepRelationTypeDir);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}
	
	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		/*
		 * We've removed all relations, so all that is left are PG* files and work files.
		 */
		FormTablespacePath(
					tablespacePath,
					primaryFilespaceLocation,
					tablespaceOid);
			
		// The rmtree routine unfortunately emits errors, so there is not errno available...  
		// Just a bool.
		result = rmtree(tablespacePath, true);
	}
	else
		result = true;

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_IsOperationCompleted(
											FileRep_GetDirTablespaceIdentifier(
																			   mirrorFilespaceLocation,
																			   tablespaceOid),
											FileRepRelationTypeDir);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}

	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);
	if (mirrorFilespaceLocation != NULL)
		pfree(mirrorFilespaceLocation);

	return result;
}


/*
 *	mdrmbdir() -- Remove a database directory on magnetic disk.
 *
 * If isRedo is true, it's okay for the database directory to exist already.
 */
bool
mdrmdbdir(
	DbDirNode					*dbDirNode,
	
	bool						primaryOnly,

	bool					 	mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred)
{
	StorageManagerMirrorMode	mirrorMode;

	char *primaryFilespaceLocation;
	char *mirrorFilespaceLocation;
	char dbPath[MAXPGPATH];
	bool result;

	*mirrorDataLossOccurred = false;

	mdsetupdropobjmirroraccess(
							primaryOnly,
							mirrorOnly,
							&mirrorMode,
							mirrorDataLossOccurred);

	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
										dbDirNode->tablespace,
										&primaryFilespaceLocation,
										&mirrorFilespaceLocation);
	
	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_MirrorDrop(
								  FileRep_GetDirDatabaseIdentifier(
																   mirrorFilespaceLocation,
																   *dbDirNode),
								  FileRepRelationTypeDir);
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}
	
	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		/*
		 * We've removed all relations, so all that is left are PG* files and work files.
		 */
		FormDatabasePath(
					dbPath,
					primaryFilespaceLocation,
					dbDirNode->tablespace,
					dbDirNode->database);
			
		// The rmtree routine unfortunately emits errors, so there is not errno available...  
		// Just a bool.
		result = rmtree(dbPath, true);
	}
	else
		result = true;

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		FileRepPrimary_IsOperationCompleted(
											FileRep_GetDirDatabaseIdentifier(
																			 mirrorFilespaceLocation,
																			 *dbDirNode),
											FileRepRelationTypeDir);	
		
		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}

	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);
	if (mirrorFilespaceLocation != NULL)
		pfree(mirrorFilespaceLocation);

	return result;
}


/*
 *	mdextend() -- Add a block to the specified relation.
 *
 *		The semantics are nearly the same as mdwrite(): write at the
 *		specified position.  However, this is to be used for the case of
 *		extending a relation (i.e., blocknum is at or beyond the current
 *		EOF).  Note that we assume writing a block beyond current EOF
 *		causes intervening file space to become filled with zeroes.
 */
void
mdextend(SMgrRelation reln, BlockNumber blocknum, char *buffer, bool isTemp)
{
	off_t		seekpos;
#ifdef suppress
	int			nbytes;
#endif
	MdMirVec    *v;

	/* This assert is too expensive to have on normally ... */
#ifdef CHECK_WRITE_VS_EXTEND
	Assert(blocknum >= mdnblocks(reln));
#endif

	/*
	 * If a relation manages to grow to 2^32-1 blocks, refuse to extend it any
	 * more --- we mustn't create a block whose number actually is
	 * InvalidBlockNumber.
	 */
	if (blocknum == InvalidBlockNumber)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("cannot extend relation %u/%u/%u beyond %u blocks",
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode,
						InvalidBlockNumber)));

	v = _mdmir_getseg(reln, blocknum, isTemp, EXTENSION_CREATE);

	seekpos = (off_t) BLCKSZ * (blocknum % ((BlockNumber) RELSEG_SIZE));

	Assert(seekpos < (off_t) BLCKSZ * RELSEG_SIZE);

	/*
	 * Note: because caller usually obtained blocknum by calling mdnblocks,
	 * which did a seek(SEEK_END), this seek is often redundant and will be
	 * optimized away by fd.c.	It's not redundant, however, if there is a
	 * partial page at the end of the file. In that case we want to try to
	 * overwrite the partial page with a full page.  It's also not redundant
	 * if bufmgr.c had to dump another buffer of the same file to make room
	 * for the new page's buffer.
	 */

	if (!MirroredBufferPool_Write(
							&v->mdmir_open,
							seekpos,
							buffer,
							BLCKSZ))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not extend relation %u/%u/%u: %m",
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode),
				 errhint("Check free disk space.")));

#ifdef suppress
	// UNDONE: What do we do with this partial write / truncate back madness????
	if (FileSeek(v->mdfd_vfd, seekpos, SEEK_SET) != seekpos)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek to block %u of relation %u/%u/%u: %m",
						blocknum,
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode)));

	if ((nbytes = FileWrite(v->mdfd_vfd, buffer, BLCKSZ)) != BLCKSZ)
	{
		if (nbytes < 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not extend relation %u/%u/%u: %m",
							reln->smgr_rnode.spcNode,
							reln->smgr_rnode.dbNode,
							reln->smgr_rnode.relNode),
					 errhint("Check free disk space.")));
		/* short write: complain appropriately */
		ereport(ERROR,
				(errcode(ERRCODE_DISK_FULL),
				 errmsg("could not extend relation %u/%u/%u: wrote only %d of %d bytes at block %u",
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode,
						nbytes, BLCKSZ, blocknum),
				 errhint("Check free disk space.")));
	}
#endif

	if (!isTemp)
		register_dirty_segment(reln, v);

	Assert(_mdnblocks(reln, v) <= ((BlockNumber) RELSEG_SIZE));
}

/*
 *	mdopen() -- Open the specified relation.
 *
 * Note we only open the first segment, when there are multiple segments.
 *
 * If first segment is not present, either ereport or return NULL according
 * to "behavior".  We treat EXTENSION_CREATE the same as EXTENSION_FAIL;
 * EXTENSION_CREATE means it's OK to extend an existing relation, not to
 * invent one out of whole cloth.
 */
static MdMirVec *
mdopen(SMgrRelation reln, ExtensionBehavior behavior)
{
	int	primaryError;
	bool mirrorDataLossOccurred;

	MdMirVec    *v;
	MirroredBufferPoolOpen		mirroredOpen;

	/* No work if already open */
	if (reln->md_mirvec)
		return reln->md_mirvec;

	MirroredBufferPool_Open(
				&mirroredOpen,
				&reln->smgr_rnode,
				/* segmentFileNum */ 0,
				/* relationName */ NULL,		// Ok to be NULL -- we don't know the name here.
				&primaryError,
				&mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		/*
		 * During bootstrap, there are cases where a system relation will be
		 * accessed (by internal backend processes) before the bootstrap
		 * script nominally creates it.  Therefore, accept mdopen() as a
		 * substitute for mdcreate() in bootstrap mode only. (See mdcreate)
		 */
		if (IsBootstrapProcessingMode())
		{
			MirrorDataLossTrackingState mirrorDataLossTrackingState;
			int64						mirrorDataLossTrackingSessionNum;
			
			mirrorDataLossTrackingState = 
						FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														&mirrorDataLossTrackingSessionNum);
			MirroredBufferPool_Create(
							&mirroredOpen,
							&reln->smgr_rnode,
							/* segmentFileNum */ 0,
							/* relationName */ NULL,		// Ok to be NULL -- we don't know the name here.
							mirrorDataLossTrackingState,
							mirrorDataLossTrackingSessionNum,
							&primaryError,
							&mirrorDataLossOccurred);
		}
		if (!MirroredBufferPool_IsActive(&mirroredOpen))
		{
			int saved_err;

			if (behavior == EXTENSION_RETURN_NULL && FILE_POSSIBLY_DELETED(errno))
				return NULL;

			saved_err = errno;
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open relation %u/%u/%u: %s",
							reln->smgr_rnode.spcNode,
							reln->smgr_rnode.dbNode,
							reln->smgr_rnode.relNode,
							strerror(saved_err)),
					 errdetail_nonexistent_relation(saved_err, &reln->smgr_rnode)));
		}
	}

	reln->md_mirvec = v = _mirvec_alloc();

	v->mdmir_open = mirroredOpen;
	v->mdmir_segno = 0;
	v->mdmir_chain = NULL;
	Assert(_mdnblocks(reln, v) <= ((BlockNumber) RELSEG_SIZE));

	return v;
}

/*
 *	mdclose() -- Close the specified relation, if it isn't closed already.
 */
void
mdclose(SMgrRelation reln)
{
	MdMirVec    *v = reln->md_mirvec;

	/* No work if already closed */
	if (v == NULL)
		return;

	reln->md_mirvec = NULL;			/* prevent dangling pointer after error */

	while (v != NULL)
	{
		MdMirVec    *ov = v;

		/* if not closed already */
		if (MirroredBufferPool_IsActive(&v->mdmir_open))
			MirroredBufferPool_Close(&v->mdmir_open);
		/* Now free vector */
		v = v->mdmir_chain;
		pfree(ov);
	}
}

/*
 *	mdread() -- Read the specified block from a relation.
 */
void
mdread(SMgrRelation reln, BlockNumber blocknum, char *buffer)
{
	off_t		seekpos;
	int			nbytes;
	MdMirVec    *v;

	v = _mdmir_getseg(reln, blocknum, false, EXTENSION_FAIL);

	seekpos = (off_t) BLCKSZ * (blocknum % ((BlockNumber) RELSEG_SIZE));
	Assert(seekpos < (off_t) BLCKSZ * RELSEG_SIZE);

	if (MirroredBufferPool_SeekSet(&v->mdmir_open, seekpos) != seekpos)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek to block %u of relation %u/%u/%u: %m",
						blocknum,
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode)));

	if ((nbytes = MirroredBufferPool_Read(
								&v->mdmir_open,
								seekpos,
								buffer, 
								BLCKSZ)) != BLCKSZ)
	{
		if (nbytes < 0)
			ereport(ERROR,
					(errcode_for_file_access(),
				   errmsg("could not read block %u of relation %u/%u/%u: %m",
						  blocknum,
						  reln->smgr_rnode.spcNode,
						  reln->smgr_rnode.dbNode,
						  reln->smgr_rnode.relNode)));

		/*
		 * Short read: we are at or past EOF, or we read a partial block at
		 * EOF.  Normally this is an error; upper levels should never try to
		 * read a nonexistent block.  However, if zero_damaged_pages is ON or
		 * we are InRecovery, we should instead return zeroes without
		 * complaining.  This allows, for example, the case of trying to
		 * update a block that was later truncated away.
		 */
		if (zero_damaged_pages || InRecovery)
			MemSet(buffer, 0, BLCKSZ);
		else
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("could not read block %u of relation %u/%u/%u: read only %d of %d bytes",
							blocknum,
							reln->smgr_rnode.spcNode,
							reln->smgr_rnode.dbNode,
							reln->smgr_rnode.relNode,
							nbytes, BLCKSZ)));
	}
}

/*
 *	mdwrite() -- Write the supplied block at the appropriate location.
 *
 *		This is to be used only for updating already-existing blocks of a
 *		relation (ie, those before the current EOF).  To extend a relation,
 *		use mdextend().
 */
void
mdwrite(SMgrRelation reln, BlockNumber blocknum, char *buffer, bool isTemp)
{
	off_t		seekpos;
	MdMirVec    *v;

	/* This assert is too expensive to have on normally ... */
#ifdef CHECK_WRITE_VS_EXTEND
	Assert(blocknum < mdnblocks(reln));
#endif

	v = _mdmir_getseg(reln, blocknum, isTemp, EXTENSION_FAIL);

	seekpos = (off_t) BLCKSZ * (blocknum % ((BlockNumber) RELSEG_SIZE));

	Assert(seekpos < (off_t) BLCKSZ * RELSEG_SIZE);

	if (!MirroredBufferPool_Write(
							&v->mdmir_open,
							seekpos,
							buffer,
							BLCKSZ))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write block %u of relation %u/%u/%u: %m",
						blocknum,
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode)));

	if (!isTemp)
		register_dirty_segment(reln, v);
}

/*
 *	mdnblocks() -- Get the number of blocks stored in a relation.
 *
 *		Important side effect: all active segments of the relation are opened
 *		and added to the mdmir_chain list.  If this routine has not been
 *		called, then only segments up to the last one actually touched
 *		are present in the chain.
 */
BlockNumber
mdnblocks(SMgrRelation reln)
{
	MdMirVec    *v = mdopen(reln, EXTENSION_FAIL);
	BlockNumber nblocks;
	BlockNumber segno = 0;

	/*
	 * Skip through any segments that aren't the last one, to avoid redundant
	 * seeks on them.  We have previously verified that these segments are
	 * exactly RELSEG_SIZE long, and it's useless to recheck that each time.
	 *
	 * NOTE: this assumption could only be wrong if another backend has
	 * truncated the relation.	We rely on higher code levels to handle that
	 * scenario by closing and re-opening the md fd, which is handled via
	 * relcache flush.	(Since the bgwriter doesn't participate in relcache
	 * flush, it could have segment chain entries for inactive segments;
	 * that's OK because the bgwriter never needs to compute relation size.)
	 */
	while (v->mdmir_chain != NULL)
	{
		segno++;
		v = v->mdmir_chain;
	}

	for (;;)
	{
		nblocks = _mdnblocks(reln, v);
		if (nblocks > ((BlockNumber) RELSEG_SIZE))
			elog(FATAL, "segment too big");
		if (nblocks < ((BlockNumber) RELSEG_SIZE))
		{
			return (segno * ((BlockNumber) RELSEG_SIZE)) + nblocks;
        }

		/*
		 * If segment is exactly RELSEG_SIZE, advance to next one.
		 */
		segno++;

		if (v->mdmir_chain == NULL)
		{
			/*
			 * Because we pass true for createIfDoesntExist, we will create the next segment (with
			 * zero length) immediately, if the last segment is of length
			 * RELSEG_SIZE.  While perhaps not strictly necessary, this keeps
			 * the logic simple.
			 */
			v->mdmir_chain = _mdmir_openseg(reln, segno, true /* createIfDoesntExist */);
			if (v->mdmir_chain == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
				 errmsg("could not open segment %u of relation %u/%u/%u: %m",
						segno,
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode)));
		}

		v = v->mdmir_chain;
	}
}

/*
 *	mdtruncate() -- Truncate relation to specified number of blocks.
 */
void
mdtruncate(SMgrRelation reln, BlockNumber nblocks, bool isTemp, bool allowNotFound)
{
	MdMirVec    *v;
	BlockNumber curnblk;
	BlockNumber priorblocks;

	if (allowNotFound)
	{
		if (mdopen(reln, EXTENSION_RETURN_NULL) == NULL)
			return;
	}

	/*
	 * NOTE: mdnblocks makes sure we have opened all active segments, so that
	 * truncation loop will get them all!
	 */
	curnblk = mdnblocks(reln);
	if (nblocks > curnblk)
	{
		/* Bogus request ... but no complaint if InRecovery */
		if (InRecovery || allowNotFound)
			return;
		ereport(ERROR,
				(errmsg("could not truncate relation %u/%u/%u to %u blocks: it's only %u blocks now",
						reln->smgr_rnode.spcNode,
						reln->smgr_rnode.dbNode,
						reln->smgr_rnode.relNode,
						nblocks, curnblk)));
	}

	/*
	 * Resync issues truncate to mirror only. In that case on primary nblocks will be always identical to curnblock
	 * since nblocks is allocated while holding LockRelationForResyncExtension.
	 */
	if (nblocks == curnblk && ! FileRepPrimary_IsResyncWorker())
		return;			/* no work */

	v = mdopen(reln, EXTENSION_FAIL);

	priorblocks = 0;
	
	while (v != NULL)
	{
		MdMirVec    *ov = v;

		if (priorblocks > nblocks)
		{
			/*
			 * This segment is no longer active (and has already been unlinked
			 * from the mdfd_chain). We truncate the file, but do not delete
			 * it, for reasons explained in the header comments.
			 */
			if (!MirroredBufferPool_Truncate(&v->mdmir_open, 0))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not truncate relation %u/%u/%u to %u blocks: %m",
								reln->smgr_rnode.spcNode,
								reln->smgr_rnode.dbNode,
								reln->smgr_rnode.relNode,
								nblocks)));
			if (!isTemp)
				register_dirty_segment(reln, v);
			v = v->mdmir_chain;
			Assert(ov != reln->md_mirvec);	/* we never drop the 1st segment */
			pfree(ov);
		}
		else if (priorblocks + ((BlockNumber) RELSEG_SIZE) > nblocks)
		{
			/*
			 * This is the last segment we want to keep. Truncate the file to
			 * the right length, and clear chain link that points to any
			 * remaining segments (which we shall zap). NOTE: if nblocks is
			 * exactly a multiple K of RELSEG_SIZE, we will truncate the K+1st
			 * segment to 0 length but keep it. This adheres to the invariant
			 * given in the header comments.
			 */
			BlockNumber lastsegblocks = nblocks - priorblocks;

			if (!MirroredBufferPool_Truncate(&v->mdmir_open, (off_t) lastsegblocks * BLCKSZ))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not truncate relation %u/%u/%u to %u blocks: %m",
								reln->smgr_rnode.spcNode,
								reln->smgr_rnode.dbNode,
								reln->smgr_rnode.relNode,
								nblocks)));
			if (!isTemp)
				register_dirty_segment(reln, v);
			v = v->mdmir_chain;
			ov->mdmir_chain = NULL;
		}
		else
		{
			/*
			 * We still need this segment and 0 or more blocks beyond it, so
			 * nothing to do here.
			 */
			v = v->mdmir_chain;
		}
		priorblocks += RELSEG_SIZE;
	}
}

/*
 *	mdimmedsync() -- Immediately sync a relation to stable storage.
 *
 * Note that only writes already issued are synced; this routine knows
 * nothing of dirty buffers that may exist inside the buffer manager.
 */
void
mdimmedsync(SMgrRelation reln)
{
	MdMirVec    *v;
	BlockNumber curnblk;

	/*
	 * NOTE: mdnblocks makes sure we have opened all active segments, so that
	 * fsync loop will get them all!
	 */
	curnblk = mdnblocks(reln);
	v = mdopen(reln, EXTENSION_FAIL);

	while (v != NULL)
	{
		if (!MirroredBufferPool_Flush(&v->mdmir_open))
			ereport(ERROR,
					(errcode_for_file_access(),
				errmsg("could not fsync segment %u of relation %u/%u/%u: %m",
					   v->mdmir_segno,
					   reln->smgr_rnode.spcNode,
					   reln->smgr_rnode.dbNode,
					   reln->smgr_rnode.relNode)));
		v = v->mdmir_chain;
	}
}

/*
 *	mdsync() -- Sync previous writes to stable storage.
 */
void
mdsync(void)
{
	static bool mdsync_in_progress = false;

	HASH_SEQ_STATUS hstat;
	PendingOperationEntry *entry;
	int			absorb_counter;

	/*
	 * This is only called during checkpoints, and checkpoints should only
	 * occur in processes that have created a pendingOpsTable.
	 */
	if (!pendingOpsTable)
		elog(ERROR, "cannot sync without a pendingOpsTable");

	/*
	 * If we are in the bgwriter, the sync had better include all fsync
	 * requests that were queued by backends up to this point.	The tightest
	 * race condition that could occur is that a buffer that must be written
	 * and fsync'd for the checkpoint could have been dumped by a backend just
	 * before it was visited by BufferSync().  We know the backend will have
	 * queued an fsync request before clearing the buffer's dirtybit, so we
	 * are safe as long as we do an Absorb after completing BufferSync().
	 */
	AbsorbFsyncRequests();

	/*
	 * To avoid excess fsync'ing (in the worst case, maybe a never-terminating
	 * checkpoint), we want to ignore fsync requests that are entered into the
	 * hashtable after this point --- they should be processed next time,
	 * instead.  We use mdsync_cycle_ctr to tell old entries apart from new
	 * ones: new ones will have cycle_ctr equal to the incremented value of
	 * mdsync_cycle_ctr.
	 *
	 * In normal circumstances, all entries present in the table at this point
	 * will have cycle_ctr exactly equal to the current (about to be old)
	 * value of mdsync_cycle_ctr.  However, if we fail partway through the
	 * fsync'ing loop, then older values of cycle_ctr might remain when we
	 * come back here to try again.  Repeated checkpoint failures would
	 * eventually wrap the counter around to the point where an old entry
	 * might appear new, causing us to skip it, possibly allowing a checkpoint
	 * to succeed that should not have.  To forestall wraparound, any time the
	 * previous mdsync() failed to complete, run through the table and
	 * forcibly set cycle_ctr = mdsync_cycle_ctr.
	 *
	 * Think not to merge this loop with the main loop, as the problem is
	 * exactly that that loop may fail before having visited all the entries.
	 * From a performance point of view it doesn't matter anyway, as this path
	 * will never be taken in a system that's functioning normally.
	 */
	if (mdsync_in_progress)
	{
		/* prior try failed, so update any stale cycle_ctr values */
		hash_seq_init(&hstat, pendingOpsTable);
		while ((entry = (PendingOperationEntry *) hash_seq_search(&hstat)) != NULL)
		{
			entry->cycle_ctr = mdsync_cycle_ctr;
		}
	}

	/* Advance counter so that new hashtable entries are distinguishable */
	mdsync_cycle_ctr++;

	/* Set flag to detect failure if we don't reach the end of the loop */
	mdsync_in_progress = true;

	/* Now scan the hashtable for fsync requests to process */
	absorb_counter = FSYNCS_PER_ABSORB;
	hash_seq_init(&hstat, pendingOpsTable);
	while ((entry = (PendingOperationEntry *) hash_seq_search(&hstat)) != NULL)
	{
		/*
		 * If the entry is new then don't process it this time.  Note that
		 * "continue" bypasses the hash-remove call at the bottom of the loop.
		 */
		if (entry->cycle_ctr == mdsync_cycle_ctr)
			continue;

		/* Else assert we haven't missed it */
		Assert((CycleCtr) (entry->cycle_ctr + 1) == mdsync_cycle_ctr);

#ifdef FAULT_INJECTOR
		if (!entry->canceled &&
			SIMPLE_FAULT_INJECTOR(FsyncCounter) == FaultInjectorTypeSkip)
		{
			if (MyAuxProcType == CheckpointerProcess)
				elog(LOG, "checkpoint performing fsync for %d/%d/%d",
					 entry->tag.rnode.spcNode, entry->tag.rnode.dbNode,
					 entry->tag.rnode.relNode);
			else
				elog(ERROR, "non checkpoint process trying to fsync "
					 "%d/%d/%d when fsync_counter fault is set",
					 entry->tag.rnode.spcNode, entry->tag.rnode.dbNode,
					 entry->tag.rnode.relNode);
		}
#endif
		/*
		 * If fsync is off then we don't have to bother opening the file at
		 * all.  (We delay checking until this point so that changing fsync on
		 * the fly behaves sensibly.)  Also, if the entry is marked canceled,
		 * fall through to delete it.
		 */
		if (enableFsync && !entry->canceled)
		{
			int			failures;

			/*
			 * If in bgwriter, we want to absorb pending requests every so
			 * often to prevent overflow of the fsync request queue.  It is
			 * unspecified whether newly-added entries will be visited by
			 * hash_seq_search, but we don't care since we don't need to
			 * process them anyway.
			 */
			if (--absorb_counter <= 0)
			{
				AbsorbFsyncRequests();
				absorb_counter = FSYNCS_PER_ABSORB;
			}

			/*
			 * The fsync table could contain requests to fsync segments that
			 * have been deleted (unlinked) by the time we get to them. Rather
			 * than just hoping an ENOENT (or EACCES on Windows) error can be
			 * ignored, what we do on error is absorb pending requests and
			 * then retry.	Since mdunlink() queues a "revoke" message before
			 * actually unlinking, the fsync request is guaranteed to be
			 * marked canceled after the absorb if it really was this case.
			 * DROP DATABASE likewise has to tell us to forget fsync requests
			 * before it starts deletions.
			 */
			for (failures = 0;; failures++)		/* loop exits at "break" */
			{
				SMgrRelation reln;
				MdMirVec    *seg;

				/*
				 * Find or create an smgr hash entry for this relation. This
				 * may seem a bit unclean -- md calling smgr?  But it's really
				 * the best solution.  It ensures that the open file reference
				 * isn't permanently leaked if we get an error here. (You may
				 * say "but an unreferenced SMgrRelation is still a leak!" Not
				 * really, because the only case in which a checkpoint is done
				 * by a process that isn't about to shut down is in the
				 * bgwriter, and it will periodically do smgrcloseall(). This
				 * fact justifies our not closing the reln in the success path
				 * either, which is a good thing since in non-bgwriter cases
				 * we couldn't safely do that.)  Furthermore, in many cases
				 * the relation will have been dirtied through this same smgr
				 * relation, and so we can save a file open/close cycle.
				 */
				reln = smgropen(entry->tag.rnode);

				/*
				 * It is possible that the relation has been dropped or
				 * truncated since the fsync request was entered.  Therefore,
				 * allow ENOENT, but only if we didn't fail already on this
				 * file.  This applies both during _mdfd_getseg() and during
				 * FileSync, since fd.c might have closed the file behind our
				 * back.
				 */
				seg = _mdmir_getseg(reln,
							  entry->tag.segno * ((BlockNumber) RELSEG_SIZE),
								   false, EXTENSION_RETURN_NULL);
				if (seg != NULL &&
					MirroredBufferPool_Flush(&seg->mdmir_open))
					break;		/* success; break out of retry loop */

				/*
				 * XXX is there any point in allowing more than one retry?
				 * Don't see one at the moment, but easy to change the test
				 * here if so.
				 */
				if (!FILE_POSSIBLY_DELETED(errno) ||
					failures > 0)
					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("could not fsync segment %u of relation %u/%u/%u: %m",
									entry->tag.segno,
									entry->tag.rnode.spcNode,
									entry->tag.rnode.dbNode,
									entry->tag.rnode.relNode)));
				else
					ereport(DEBUG1,
							(errcode_for_file_access(),
							 errmsg("could not fsync segment %u of relation %u/%u/%u, but retrying: %m",
									entry->tag.segno,
									entry->tag.rnode.spcNode,
									entry->tag.rnode.dbNode,
									entry->tag.rnode.relNode)));

				/*
				 * Absorb incoming requests and check to see if canceled.
				 */
				AbsorbFsyncRequests();
				absorb_counter = FSYNCS_PER_ABSORB;		/* might as well... */

				if (entry->canceled)
					break;
			}					/* end retry loop */
		}

		/*
		 * If we get here, either we fsync'd successfully, or we don't have to
		 * because enableFsync is off, or the entry is (now) marked canceled.
		 * Okay to delete it.
		 */
		if (hash_search(pendingOpsTable, &entry->tag,
						HASH_REMOVE, NULL) == NULL)
			elog(ERROR, "pendingOpsTable corrupted");
	}							/* end loop over hashtable entries */

	/* Flag successful completion of mdsync */
	mdsync_in_progress = false;
}

/*
 * mdpreckpt() -- Do pre-checkpoint work
 *
 * To distinguish unlink requests that arrived before this checkpoint
 * started from those that arrived during the checkpoint, we use a cycle
 * counter similar to the one we use for fsync requests. That cycle
 * counter is incremented here.
 *
 * This must be called *before* the checkpoint REDO point is determined.
 * That ensures that we won't delete files too soon.
 *
 * Note that we can't do anything here that depends on the assumption
 * that the checkpoint will be completed.
 */
void
mdpreckpt(void)
{
	ListCell   *cell;

	/*
	 * In case the prior checkpoint wasn't completed, stamp all entries in the
	 * list with the current cycle counter.  Anything that's in the list at
	 * the start of checkpoint can surely be deleted after the checkpoint is
	 * finished, regardless of when the request was made.
	 */
	foreach(cell, pendingUnlinks)
	{
		PendingUnlinkEntry *entry = (PendingUnlinkEntry *) lfirst(cell);

		entry->cycle_ctr = mdckpt_cycle_ctr;
	}

	/*
	 * Any unlink requests arriving after this point will be assigned the next
	 * cycle counter, and won't be unlinked until next checkpoint.
	 */
	mdckpt_cycle_ctr++;
}

/*
 * mdpostckpt() -- Do post-checkpoint work
 *
 * Remove any lingering files that can now be safely removed.
 */
void
mdpostckpt(void)
{
	while (pendingUnlinks != NIL)
	{
		PendingUnlinkEntry *entry = (PendingUnlinkEntry *) linitial(pendingUnlinks);
		char	   *path;

		/*
		 * New entries are appended to the end, so if the entry is new we've
		 * reached the end of old entries.
		 */
		if (entry->cycle_ctr == mdckpt_cycle_ctr)
			break;

		/* Else assert we haven't missed it */
		Assert((CycleCtr) (entry->cycle_ctr + 1) == mdckpt_cycle_ctr);

		/* Unlink the file */
		path = relpath(entry->rnode);
		if (unlink(path) < 0)
		{
			/*
			 * There's a race condition, when the database is dropped at the
			 * same time that we process the pending unlink requests. If the
			 * DROP DATABASE deletes the file before we do, we will get ENOENT
			 * here. rmtree() also has to ignore ENOENT errors, to deal with
			 * the possibility that we delete the file first.
			 */
			if (errno != ENOENT)
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not remove relation %u/%u/%u: %m",
								entry->rnode.spcNode,
								entry->rnode.dbNode,
								entry->rnode.relNode)));
		}
		pfree(path);

		pendingUnlinks = list_delete_first(pendingUnlinks);
		pfree(entry);
	}
}

/*
 * register_dirty_segment() -- Mark a relation segment as needing fsync
 *
 * If there is a local pending-ops table, just make an entry in it for
 * mdsync to process later.  Otherwise, try to pass off the fsync request
 * to the background writer process.  If that fails, just do the fsync
 * locally before returning (we expect this will not happen often enough
 * to be a performance problem).
 */
static void
register_dirty_segment(SMgrRelation reln, MdMirVec *seg)
{
	if (pendingOpsTable)
	{
		/* push it into local pending-ops table */
		RememberFsyncRequest(reln->smgr_rnode, seg->mdmir_segno);
	}
	else
	{
		if (ForwardFsyncRequest(reln->smgr_rnode, seg->mdmir_segno))
			return;				/* passed it off successfully */
	}
	if (!MirroredBufferPool_Flush(&seg->mdmir_open))
		ereport(ERROR,
				(errcode_for_file_access(),
				errmsg("could not fsync segment %u of relation %u/%u/%u: %m",
				   seg->mdmir_segno,
				   reln->smgr_rnode.spcNode,
				   reln->smgr_rnode.dbNode,
				   reln->smgr_rnode.relNode)));
}

/*
 * register_unlink() -- Schedule a file to be deleted after next checkpoint
 *
 * As with register_dirty_segment, this could involve either a local or
 * a remote pending-ops table.
 */
static void pg_attribute_unused()
register_unlink(RelFileNode rnode)
{
	if (pendingOpsTable)
	{
		/* push it into local pending-ops table */
		RememberFsyncRequest(rnode, UNLINK_RELATION_REQUEST);
	}
	else
	{
		/*
		 * Notify the bgwriter about it.  If we fail to queue the request
		 * message, we have to sleep and try again, because we can't simply
		 * delete the file now.  Ugly, but hopefully won't happen often.
		 *
		 * XXX should we just leave the file orphaned instead?
		 */
		Assert(IsUnderPostmaster);
		while (!ForwardFsyncRequest(rnode, UNLINK_RELATION_REQUEST))
			pg_usleep(10000L);	/* 10 msec seems a good number */
	}
}

/*
 * RememberFsyncRequest() -- callback from checkpointer side of fsync request
 *
 * We stuff most fsync requests into the local hash table for execution
 * during the checkpointer's next checkpoint.  UNLINK requests go into a
 * separate linked list, however, because they get processed separately.
 *
 * The range of possible segment numbers is way less than the range of
 * BlockNumber, so we can reserve high values of segno for special purposes.
 * We define three:
 * - FORGET_RELATION_FSYNC means to cancel pending fsyncs for a relation
 * - FORGET_DATABASE_FSYNC means to cancel pending fsyncs for a whole database
 * - UNLINK_RELATION_REQUEST is a request to delete the file after the next
 *	 checkpoint.
 *
 * (Handling the FORGET_* requests is a tad slow because the hash table has
 * to be searched linearly, but it doesn't seem worth rethinking the table
 * structure for them.)
 */
void
RememberFsyncRequest(RelFileNode rnode, BlockNumber segno)
{
	Assert(pendingOpsTable);

	if (segno == FORGET_RELATION_FSYNC)
	{
		/* Remove any pending requests for the entire relation */
		HASH_SEQ_STATUS hstat;
		PendingOperationEntry *entry;

		hash_seq_init(&hstat, pendingOpsTable);
		while ((entry = (PendingOperationEntry *) hash_seq_search(&hstat)) != NULL)
		{
			if (RelFileNodeEquals(entry->tag.rnode, rnode))
			{
				/* Okay, cancel this entry */
				entry->canceled = true;
			}
		}
	}
	else if (segno == FORGET_DATABASE_FSYNC)
	{
		/* Remove any pending requests for the entire database */
		HASH_SEQ_STATUS hstat;
		PendingOperationEntry *entry;
		ListCell   *cell, 
				   *prev,
				   *next;

		/* Remove fsync requests */
		hash_seq_init(&hstat, pendingOpsTable);
		while ((entry = (PendingOperationEntry *) hash_seq_search(&hstat)) != NULL)
		{
			if (!OidIsValid(rnode.spcNode) ||
				entry->tag.rnode.spcNode == rnode.spcNode)
			{
				if (entry->tag.rnode.dbNode == rnode.dbNode)
				{
					/* Okay, cancel this entry */
					entry->canceled = true;
				}
			}
		}
	
		/* Remove unlink requests */
		prev = NULL;
		for (cell = list_head(pendingUnlinks); cell; cell = next)
		{
			PendingUnlinkEntry *entry = (PendingUnlinkEntry *) lfirst(cell);

			next = lnext(cell);
			if (entry->rnode.dbNode == rnode.dbNode) 
			{
				pendingUnlinks = list_delete_cell(pendingUnlinks, cell, prev);
				pfree(entry);
			}
			else
				prev = cell;
		}
	}
	else if (segno == UNLINK_RELATION_REQUEST)
	{
		/* Unlink request: put it in the linked list */
		MemoryContext oldcxt = MemoryContextSwitchTo(MdCxt);
		PendingUnlinkEntry *entry;

		entry = palloc(sizeof(PendingUnlinkEntry));
		entry->rnode = rnode;
		entry->cycle_ctr = mdckpt_cycle_ctr;

		pendingUnlinks = lappend(pendingUnlinks, entry);

		MemoryContextSwitchTo(oldcxt);
	}
	else
	{
		/* Normal case: enter a request to fsync this segment */
		PendingOperationTag key;
		PendingOperationEntry *entry;
		bool		found;

		/* ensure any pad bytes in the hash key are zeroed */
		MemSet(&key, 0, sizeof(key));
		key.rnode = rnode;
		key.segno = segno;

		entry = (PendingOperationEntry *) hash_search(pendingOpsTable,
													  &key,
													  HASH_ENTER,
													  &found);
		/* if new or previously canceled entry, initialize it */
		if (!found || entry->canceled)
		{
			entry->canceled = false;
			entry->cycle_ctr = mdsync_cycle_ctr;
		}

		/*
		 * NB: it's intentional that we don't change cycle_ctr if the entry
		 * already exists.	The fsync request must be treated as old, even
		 * though the new request will be satisfied too by any subsequent
		 * fsync.
		 *
		 * However, if the entry is present but is marked canceled, we should
		 * act just as though it wasn't there.  The only case where this could
		 * happen would be if a file had been deleted, we received but did not
		 * yet act on the cancel request, and the same relfilenode was then
		 * assigned to a new file.	We mustn't lose the new request, but it
		 * should be considered new not old.
		 */
	}
}

/*
 * ForgetRelationFsyncRequests -- forget any fsyncs for a rel
 */
void
ForgetRelationFsyncRequests(RelFileNode rnode)
{
	if (pendingOpsTable)
	{
		/* standalone backend or startup process: fsync state is local */
		RememberFsyncRequest(rnode, FORGET_RELATION_FSYNC);
	}
	else if (IsUnderPostmaster)
	{
		/*
		 * Notify the checkpointer about it.  If we fail to queue the revoke
		 * message, we have to sleep and try again ... ugly, but hopefully
		 * won't happen often.
		 *
		 * XXX should we CHECK_FOR_INTERRUPTS in this loop?  Escaping with an
		 * error would leave the no-longer-used file still present on disk,
		 * which would be bad, so I'm inclined to assume that the checkpointer
		 * will always empty the queue soon.
		 */
		while (!ForwardFsyncRequest(rnode, FORGET_RELATION_FSYNC))
			pg_usleep(10000L);	/* 10 msec seems a good number */

		/*
		 * Note we don't wait for the checkpointer to actually absorb the revoke
		 * message; see mdsync() for the implications.
		 */
	}
}

/*
 * ForgetDatabaseFsyncRequests -- forget any fsyncs and unlinks for a DB
 */
void
ForgetDatabaseFsyncRequests(Oid tblspc, Oid dbid)
{
	RelFileNode rnode;

	rnode.dbNode = dbid;
	rnode.spcNode = tblspc;
	rnode.relNode = 0;

	if (pendingOpsTable)
	{
		/* standalone backend or startup process: fsync state is local */
		RememberFsyncRequest(rnode, FORGET_DATABASE_FSYNC);
	}
	else if (IsUnderPostmaster)
	{
		/* see notes in ForgetRelationFsyncRequests */
		while (!ForwardFsyncRequest(rnode, FORGET_DATABASE_FSYNC))
			pg_usleep(10000L);	/* 10 msec seems a good number */
	}
}


/*
 *	_mirvec_alloc() -- Make a MdfdVec object.
 */
static MdMirVec *
_mirvec_alloc(void)
{
	return (MdMirVec *) MemoryContextAllocZero(MdCxt, sizeof(MdMirVec));
}

/*
 * Open the specified segment of the relation,
 * and make a MdfdVec object for it.  Returns NULL on failure.
 *
 * @param createIfDoesntExist if true then create the segment file if it doesn't already exist
 */
static MdMirVec *
_mdmir_openseg(SMgrRelation reln, BlockNumber segno, bool createIfDoesntExist)
{
	int	primaryError;
	bool mirrorDataLossOccurred;

	MdMirVec    *v;

	MirroredBufferPoolOpen		mirroredOpen;

	/* open the file */
	MirroredBufferPool_Open(
					&mirroredOpen,
					&reln->smgr_rnode,
					segno,
					/* relationName */ NULL,		// Ok to be NULL -- we don't know the name here.
					&primaryError,
					&mirrorDataLossOccurred);

	if (!MirroredBufferPool_IsActive(&mirroredOpen))
	{
	    if ( createIfDoesntExist )
	    {
            MirrorDataLossTrackingState mirrorDataLossTrackingState;
            int64						mirrorDataLossTrackingSessionNum;

            mirrorDataLossTrackingState =
                FileRepPrimary_GetMirrorDataLossTrackingSessionNum(&mirrorDataLossTrackingSessionNum);

	        MirroredBufferPool_Create(
					&mirroredOpen,
					&reln->smgr_rnode,
					segno,
					/* relationName */ NULL,		// Ok to be NULL -- we don't know the name here.
					mirrorDataLossTrackingState,
					mirrorDataLossTrackingSessionNum,
					&primaryError,
					&mirrorDataLossOccurred);

        }

        /* check again now that we may have done create */
        if (!MirroredBufferPool_IsActive(&mirroredOpen))
        {
            return NULL;
        }
    }
	/* allocate an mdfdvec entry for it */
	v = _mirvec_alloc();

	/* fill the entry */
	v->mdmir_open = mirroredOpen;
	v->mdmir_segno = segno;
	v->mdmir_chain = NULL;
	Assert(_mdnblocks(reln, v) <= ((BlockNumber) RELSEG_SIZE));

	/* all done */
	return v;
}

/*
 *	_mdfd_getseg() -- Find the segment of the relation holding the
 *		specified block.
 *
 * If the segment doesn't exist, we ereport, return NULL, or create the
 * segment, according to "behavior".  Note: isTemp need only be correct
 * in the EXTENSION_CREATE case.
 */
static MdMirVec *
_mdmir_getseg(SMgrRelation reln, BlockNumber blkno, bool isTemp,
			  ExtensionBehavior behavior)
{
	MdMirVec    *v = mdopen(reln, behavior);
	BlockNumber targetseg;
	BlockNumber nextsegno;

	if (!v)
		return NULL;			/* only possible if EXTENSION_RETURN_NULL */

	targetseg = blkno / ((BlockNumber) RELSEG_SIZE);
	for (nextsegno = 1; nextsegno <= targetseg; nextsegno++)
	{
		Assert(nextsegno == v->mdmir_segno + 1);

		if (v->mdmir_chain == NULL)
		{
			/*
			 * Normally we will create new segments only if authorized by the
			 * caller (i.e., we are doing mdextend()).	But when doing WAL
			 * recovery, create segments anyway; this allows cases such as
			 * replaying WAL data that has a write into a high-numbered
			 * segment of a relation that was later deleted.  We want to go
			 * ahead and create the segments so we can finish out the replay.
			 *
			 * We have to maintain the invariant that segments before the last
			 * active segment are of size RELSEG_SIZE; therefore, pad them out
			 * with zeroes if needed.  (This only matters if caller is
			 * extending the relation discontiguously, but that can happen in
			 * hash indexes.)
			 */
			if (behavior == EXTENSION_CREATE || InRecovery)
			{
				if (_mdnblocks(reln, v) < RELSEG_SIZE)
				{
					char	   *zerobuf = palloc0(BLCKSZ);

					mdextend(reln, nextsegno * ((BlockNumber) RELSEG_SIZE) - 1,
							 zerobuf, isTemp);
					pfree(zerobuf);
				}
				v->mdmir_chain = _mdmir_openseg(reln, nextsegno, true);
			}
			else
			{
				/* We won't create segment if not existent */
				v->mdmir_chain = _mdmir_openseg(reln, nextsegno, false);
			}
			if (v->mdmir_chain == NULL)
			{
				int saved_err;
				
				if (behavior == EXTENSION_RETURN_NULL &&
					FILE_POSSIBLY_DELETED(errno))
					return NULL;

				saved_err = errno;

				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open segment %u of relation %u/%u/%u (target block %u): %s",
								nextsegno,
								reln->smgr_rnode.spcNode,
								reln->smgr_rnode.dbNode,
								reln->smgr_rnode.relNode,
								blkno,
								strerror(saved_err)),
						 errdetail_nonexistent_relation(saved_err, &reln->smgr_rnode)));
			}
		}
		v = v->mdmir_chain;
	}

	return v;
}

/*
 * Get number of blocks present in a single disk file
 */
static BlockNumber
_mdnblocks(SMgrRelation reln, MdMirVec *seg)
{
	off_t		len;

	Assert(MirroredBufferPool_IsActive(&seg->mdmir_open));

	len = MirroredBufferPool_SeekEnd(&seg->mdmir_open);
	if (len < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
		errmsg("could not seek to end of segment %u of relation %u/%u/%u: %m",
			   seg->mdmir_segno,
			   reln->smgr_rnode.spcNode,
			   reln->smgr_rnode.dbNode,
			   reln->smgr_rnode.relNode)));
	/* note that this calculation will ignore any partial block at EOF */
	return (BlockNumber) (len / BLCKSZ);
}
