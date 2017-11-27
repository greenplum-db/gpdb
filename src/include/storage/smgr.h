/*-------------------------------------------------------------------------
 *
 * smgr.h
 *	  storage manager switch public interface declarations.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/smgr.h,v 1.65 2009/01/01 17:24:01 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef SMGR_H
#define SMGR_H

#include "access/xlog.h"
#include "fmgr.h"
#include "storage/block.h"
#include "storage/relfilenode.h"
#include "storage/dbdirnode.h"
#include "access/persistentendxactrec.h"
#include "access/filerepdefs.h"


/*
 * smgr.c maintains a table of SMgrRelation objects, which are essentially
 * cached file handles.  An SMgrRelation is created (if not already present)
 * by smgropen(), and destroyed by smgrclose().  Note that neither of these
 * operations imply I/O, they just create or destroy a hashtable entry.
 * (But smgrclose() may release associated resources, such as OS-level file
 * descriptors.)
 *
 * An SMgrRelation may have an "owner", which is just a pointer to it from
 * somewhere else; smgr.c will clear this pointer if the SMgrRelation is
 * closed.	We use this to avoid dangling pointers from relcache to smgr
 * without having to make the smgr explicitly aware of relcache.  There
 * can't be more than one "owner" pointer per SMgrRelation, but that's
 * all we need.
 */
typedef struct SMgrRelationData
{
	/* rnode is the hashtable lookup key, so it must be first! */
	RelFileNode smgr_rnode;		/* relation physical identifier */

	/* pointer to owning pointer, or NULL if none */
	struct SMgrRelationData **smgr_owner;

	/* additional public fields may someday exist here */

	/*
	 * Fields below here are intended to be private to smgr.c and its
	 * submodules.	Do not touch them from elsewhere.
	 */
	int			smgr_which;		/* storage manager selector */

	/* for md.c; NULL for forks that are not open */
	struct _MdfdVec *md_fd[MAX_FORKNUM + 1];
} SMgrRelationData;

typedef SMgrRelationData *SMgrRelation;

/*
 * Whether the object is being dropped in the original transaction, crash
 * recovery, or as part of (re)create / (re)drop during resynchronize.
 */
typedef enum StorageManagerMirrorMode
{
	StorageManagerMirrorMode_None = 0,
	StorageManagerMirrorMode_PrimaryOnly = 1,
	StorageManagerMirrorMode_Both = 2,
	StorageManagerMirrorMode_MirrorOnly = 3,
	MaxStorageManagerMirrorMode /* must always be last */
} StorageManagerMirrorMode;

inline static bool
StorageManagerMirrorMode_DoPrimaryWork(StorageManagerMirrorMode mirrorMode)
{
	return (mirrorMode == StorageManagerMirrorMode_Both || mirrorMode == StorageManagerMirrorMode_PrimaryOnly);
}

inline static bool
StorageManagerMirrorMode_SendToMirror(StorageManagerMirrorMode mirrorMode)
{
	return (mirrorMode == StorageManagerMirrorMode_Both || mirrorMode == StorageManagerMirrorMode_MirrorOnly);
}

extern char *StorageManagerMirrorMode_Name(StorageManagerMirrorMode mirrorMode);

extern void smgrinit(void);
extern SMgrRelation smgropen(RelFileNode rnode);
extern bool smgrexists(SMgrRelation reln, ForkNumber forknum);
extern void smgrsetowner(SMgrRelation *owner, SMgrRelation reln);
extern void smgrclose(SMgrRelation reln);
extern void smgrcloseall(void);
extern void smgrclosenode(RelFileNode rnode);
extern void smgrcreate(SMgrRelation reln, ForkNumber forknum, bool isRedo);
extern void smgrdounlink(SMgrRelation reln, ForkNumber forknum,
			 bool isTemp, bool isRedo);

extern void smgrcreatefilespacedirpending(Oid filespaceOid,
							  int16 primaryDbId,
							  char *primaryFilespaceLocation,
							  int16 mirrorDbId,
							  char *mirrorFilespaceLocation,
							  MirroredObjectExistenceState mirrorExistenceState,
							  ItemPointer persistentTid,
							  int64 *persistentSerialNum,
							  bool flushToXLog);
extern void smgrcreatefilespacedir(Oid filespaceOid,
					   char *primaryFilespaceLocation,
 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */
					   char *mirrorFilespaceLocation,
					   StorageManagerMirrorMode mirrorMode,
					   bool ignoreAlreadyExists,
					   int *primaryError,
					   bool *mirrorDataLossOccurred);
extern void smgrcreatetablespacedirpending(TablespaceDirNode *tablespaceDirNode,
							   MirroredObjectExistenceState mirrorExistenceState,
							   ItemPointer persistentTid,
							   int64 *persistentSerialNum,
							   bool flushToXLog);
extern void smgrcreatetablespacedir(Oid tablespaceOid,
						StorageManagerMirrorMode mirrorMode,
						bool ignoreAlreadyExists,
						int *primaryError,
						bool *mirrorDataLossOccurred);
extern void smgrcreatedbdirjustintime(DbDirNode *justInTimeDbDirNode,
						  MirroredObjectExistenceState mirrorExistenceState,
						  StorageManagerMirrorMode mirrorMode,
						  ItemPointer persistentTid,
						  int64 *persistentSerialNum,
						  int *primaryError,
						  bool *mirrorDataLossOccurred);
extern void smgrcreatedbdirpending(DbDirNode *dbDirNode,
					   MirroredObjectExistenceState mirrorExistenceState,
					   ItemPointer persistentTid,
					   int64 *persistentSerialNum,
					   bool flushToXLog);
extern void smgrcreatedbdir(DbDirNode *dbDirNode,
				StorageManagerMirrorMode mirrorMode,
				bool ignoreAlreadyExists,
				int *primaryError,
				bool *mirrorDataLossOccurred);
extern void smgrcreatepending(RelFileNode *relFileNode,
				  int32 segmentFileNum,
				  PersistentFileSysRelStorageMgr relStorageMgr,
				  PersistentFileSysRelBufpoolKind relBufpoolKind,
				  MirroredObjectExistenceState mirrorExistenceState,
				  MirroredRelDataSynchronizationState relDataSynchronizationState,
				  char *relationName,
				  ItemPointer persistentTid,
				  int64 *persistentSerialNum,
				  bool isLocalBuf,
				  bool bufferPoolBulkLoad,
				  bool flushToXLog);
extern void smgrmirroredcreate(SMgrRelation reln,
				   char	*relationName, /* For tracing only.  Can be NULL in some execution paths. */
				   MirrorDataLossTrackingState mirrorDataLossTrackingState,
				   int64 mirrorDataLossTrackingSessionNum,
				   bool ignoreAlreadyExists,
				   bool *mirrorDataLossOccurred);
extern void smgrscheduleunlink(RelFileNode *relFileNode,
				   ForkNumber forknum,
				   int32 segmentFileNum,
				   PersistentFileSysRelStorageMgr relStorageMgr,
				   bool	isLocalBuf,
				   char	*relationName,
				   ItemPointer persistentTid,
				   int64 persistentSerialNum);
extern void smgrdomirroredunlink(RelFileNode *relFileNode,
					 bool isLocalBuf,
					 char *relationName, /* For tracing only.  Can be NULL in some execution paths. */
					 bool primaryOnly,
					 bool isRedo,
					 bool ignoreNonExistence,
					 bool *mirrorDataLossOccurred);
extern void smgrschedulermfilespacedir(Oid filespaceOid,
						   ItemPointer persistentTid,
						   int64 persistentSerialNum);
extern void smgrschedulermtablespacedir(Oid tablespaceOid,
							ItemPointer persistentTid,
							int64 persistentSerialNum);
extern void smgrschedulermdbdir(DbDirNode *dbDirNode,
					ItemPointer persistentTid,
					int64 persistentSerialNum);
extern void smgrdormfilespacedir(Oid filespaceOid,
					 char *primaryFilespaceLocation,
 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */
					 char *mirrorFilespaceLocation,
					 bool primaryOnly,
					 bool mirrorOnly,
					 bool ignoreNonExistence,
					 bool *mirrorDataLossOccurred);
extern void smgrdormtablespacedir(Oid tablespaceOid,
					  bool primaryOnly,
					  bool mirrorOnly,
					  bool ignoreNonExistence,
					  bool *mirrorDataLossOccurred);
extern void smgrdormdbdir(DbDirNode *dropDbDirNode,
			  bool primaryOnly,
			  bool mirrorOnly,
			  bool ignoreNonExistence,
			  bool *mirrorDataLossOccurred);
extern void smgrextend(SMgrRelation reln, ForkNumber forknum, 
					   BlockNumber blocknum, char *buffer, bool isTemp);
extern void smgrread(SMgrRelation reln, ForkNumber forknum,
					 BlockNumber blocknum, char *buffer);
extern void smgrwrite(SMgrRelation reln, ForkNumber forknum,
					  BlockNumber blocknum, char *buffer, bool isTemp);
extern BlockNumber smgrnblocks(SMgrRelation reln, ForkNumber forknum);
extern void smgrtruncate(SMgrRelation reln, ForkNumber forknum,
						 BlockNumber nblocks, bool isTemp);
extern bool smgrgetpersistentinfo(XLogRecord *record,
					  RelFileNode *relFileNode,
					  ItemPointer persistentTid,
					  int64 *persistentSerialNum);
extern void smgrimmedsync(SMgrRelation reln, ForkNumber forknum);
extern int smgrGetPendingFileSysWork(EndXactRecKind endXactRecKind,
						  PersistentEndXactFileSysActionInfo **ptr);
extern bool smgrIsPendingFileSysWork(
						 EndXactRecKind endXactRecKind);
extern void smgrpreckpt(void);
extern void smgrsync(void);
extern void smgrpostckpt(void);


/* internals: move me elsewhere -- ay 7/94 */

/* in md.c */
extern void mdinit(void);
extern void mdclose(SMgrRelation reln, ForkNumber forknum);
extern void mdcreate(SMgrRelation reln, ForkNumber forknum, bool isRedo);
extern void mdmirroredcreate(SMgrRelation reln,
		 char *relationName, /* For tracing only.  Can be NULL in some execution paths. */
		 MirrorDataLossTrackingState mirrorDataLossTrackingState,
		 int64 mirrorDataLossTrackingSessionNum,
		 bool ignoreAlreadyExists,
		 bool *mirrorDataLossOccurred);
extern bool mdexists(SMgrRelation reln, ForkNumber forknum);
extern void mdmirroredunlink(RelFileNode rnode, 
		 char *relationName, /* For tracing only.  Can be NULL in some execution paths. */
		 bool primaryOnly,
		 bool isRedo,
		 bool ignoreNonExistence,
		 bool *mirrorDataLossOccurred);
extern void mdextend(SMgrRelation reln,  ForkNumber forknum,
		 BlockNumber blocknum, char *buffer, bool isTemp);
extern void mdunlink(RelFileNode rnode, ForkNumber forknum, bool isRedo);
extern void mdread(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum,
				   char *buffer);
extern void mdwrite(SMgrRelation reln, ForkNumber forknum,
					BlockNumber blocknum, char *buffer, bool isTemp);
extern BlockNumber mdnblocks(SMgrRelation reln, ForkNumber forknum);
extern void mdtruncate(SMgrRelation reln, ForkNumber forknum,
					   BlockNumber nblocks, bool isTemp,
					   bool allowedNotFound);
extern void mdimmedsync(SMgrRelation reln, ForkNumber forknum);
extern void mdpreckpt(void);
extern void mdsync(void);
extern void mdpostckpt(void);

/* md_gp.c */
extern int	errdetail_nonexistent_relation(int error, RelFileNode *relFileNode);
extern void mdcreatefilespacedir(Oid filespaceOid,
					 char *primaryFilespaceLocation,
 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */
					 char *mirrorFilespaceLocation,
					 StorageManagerMirrorMode mirrorMode,
					 bool ignoreAlreadyExists,
					 int *primaryError,
					 bool *mirrorDataLossOccurred);
extern void mdcreatetablespacedir(Oid tablespaceOid,
					  StorageManagerMirrorMode mirrorMode,
					  bool ignoreAlreadyExists,
					  int *primaryError,
					  bool *mirrorDataLossOccurred);
extern void
mdcreatedbdir(DbDirNode *dbDirNode,
			  StorageManagerMirrorMode mirrorMode,
			  bool ignoreAlreadyExists,
			  int *primaryError,
			  bool *mirrorDataLossOccurred);
extern bool mdrmfilespacedir(Oid filespaceOid,
				 char *primaryFilespaceLocation,
 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */
				 char *mirrorFilespaceLocation,
				 bool primaryOnly,
				 bool mirrorOnly,
				 bool ignoreNonExistence,
				 bool *mirrorDataLossOccurred);
extern bool mdrmtablespacedir(Oid tablespaceOid,
				  bool primaryOnly,
				  bool mirrorOnly,
				  bool ignoreNonExistence,
				  bool *mirrorDataLossOccurred);
extern bool mdrmdbdir(DbDirNode *dbDirNode,
		  bool primaryOnly,
		  bool mirrorOnly,
		  bool ignoreNonExistence,
		  bool *mirrorDataLossOccurred);


/*
 * MPP-18228 - to make addition to pending delete list atomic with adding
 * a 'Create Pending' entry in persistent tables.
 * Wrapper around the static PendingDelete_AddCreatePendingEntry() for relations
 */
extern void
PendingDelete_AddCreatePendingRelationEntry(PersistentFileSysObjName *fsObjName,
											ItemPointer persistentTid,
											int64 *persistentSerialNum,
											PersistentFileSysRelStorageMgr relStorageMgr,
											char *relationName,
											bool isLocalBuf,
											bool bufferPoolBulkLoad);

/*
 * MPP-18228 - Wrapper around PendingDelete_AddCreatePendingEntry() for
 * database, tablespace and filespace
 */
extern void
PendingDelete_AddCreatePendingEntryWrapper(PersistentFileSysObjName *fsObjName,
										   ItemPointer persistentTid,
										   int64 persistentSerialNum);

extern void RememberFsyncRequest(RelFileNode rnode, ForkNumber forknum,
								 BlockNumber segno);
extern void ForgetRelationFsyncRequests(RelFileNode rnode, ForkNumber forknum);
extern void ForgetDatabaseFsyncRequests(Oid tblspc, Oid dbid);

/* md_gp.c */
extern int errdetail_nonexistent_relation(int error, RelFileNode *relFileNode);
extern void mdcreatefilespacedir(
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
	bool						*mirrorDataLossOccurred);
extern void mdcreatetablespacedir(
	Oid 						tablespaceOid,
	StorageManagerMirrorMode	mirrorMode,
	bool						ignoreAlreadyExists,
	int 						*primaryError,
	bool						*mirrorDataLossOccurred);
extern void mdcreatedbdir(
	DbDirNode					*dbDirNode,
	StorageManagerMirrorMode	mirrorMode,
	bool						ignoreAlreadyExists,
	int 						*primaryError,
	bool						*mirrorDataLossOccurred);
extern bool mdrmfilespacedir(
	Oid 						filespaceOid,
	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */
	char						*mirrorFilespaceLocation,
	bool						primaryOnly,
	bool						mirrorOnly,
	bool 						ignoreNonExistence,
	bool						*mirrorDataLossOccurred);
extern bool mdrmtablespacedir(
	Oid 						tablespaceOid,
	bool						primaryOnly,
	bool						mirrorOnly,
	bool 						ignoreNonExistence,
	bool						*mirrorDataLossOccurred);
extern bool mdrmdbdir(
	DbDirNode					*dbDirNode,
	bool						primaryOnly,
	bool						mirrorOnly,
	bool 						ignoreNonExistence,
	bool						*mirrorDataLossOccurred);

/* smgrtype.c */
extern Datum smgrout(PG_FUNCTION_ARGS);
extern Datum smgrin(PG_FUNCTION_ARGS);
extern Datum smgreq(PG_FUNCTION_ARGS);
extern Datum smgrne(PG_FUNCTION_ARGS);

#endif							/* SMGR_H */
