/*-------------------------------------------------------------------------
 *
 * lwlock.h
 *	  Lightweight lock manager
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/lwlock.h,v 1.39 2008/06/19 21:32:56 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef LWLOCK_H
#define LWLOCK_H

/*
 * It's a bit odd to declare NUM_BUFFER_PARTITIONS and NUM_LOCK_PARTITIONS
 * here, but we need them to set up enum LWLockId correctly, and having
 * this file include lock.h or bufmgr.h would be backwards.
 * This also applies for WORKFILE_HASHSTABLE_NUM_PARTITIONS.
 */

/* Number of partitions of the shared buffer mapping hashtable */
#define NUM_BUFFER_PARTITIONS  16

/* Number of partitions the shared lock tables are divided into */
#define LOG2_NUM_LOCK_PARTITIONS  4
#define NUM_LOCK_PARTITIONS  (1 << LOG2_NUM_LOCK_PARTITIONS)

/* Number of partitions of the workfile manager hashtable */
#define NUM_WORKFILEMGR_PARTITIONS 32

/* Number of partitions of the workfile query diskspace hashtable */
#define NUM_WORKFILE_QUERYSPACE_PARTITIONS 128

/*
 * We have a number of predefined LWLocks, plus a bunch of LWLocks that are
 * dynamically assigned (e.g., for shared buffers).  The LWLock structures
 * live in shared memory (since they contain shared data) and are identified
 * by values of this enumerated type.  We abuse the notion of an enum somewhat
 * by allowing values not listed in the enum declaration to be assigned.
 * The extra value MaxDynamicLWLock is there to keep the compiler from
 * deciding that the enum can be represented as char or short ...
 *
 * If you remove a lock, please replace it with a placeholder. This retains
 * the lock numbering, which is helpful for DTrace and other external
 * debugging scripts.
 */
typedef enum LWLockId
{
	NullLock = 0,		// Have 0 be no lock.
	BufFreelistLock,
	ShmemIndexLock,
	OidGenLock,
	XidGenLock,
	ProcArrayLock,
	SInvalReadLock,
	SInvalWriteLock,
	FreeSpaceLock,
	WALInsertLock,
	WALWriteLock,
	ControlFileLock,
	CheckpointLock,
	CLogControlLock,
	SubtransControlLock,
	MultiXactGenLock,
	MultiXactOffsetControlLock,
	MultiXactMemberControlLock,
	RelCacheInitLock,
	CheckpointerCommLock,
	TwoPhaseStateLock,
	TablespaceCreateLock,
	BtreeVacuumLock,
	AddinShmemInitLock,
	AutovacuumLock,
	AutovacuumScheduleLock,
	SyncScanLock,
	SharedSnapshotLock,
	DistributedLogControlLock,
	SeqServerControlLock,
	AOSegFileLock,
	PersistentObjLock,
	FileRepShmemLock,
	FileRepAckShmemLock,	
	FileRepAckHashShmemLock,
	ChangeTrackingTransitionLock,
	ChangeTrackingWriteLock,
	ChangeTrackingCompactLock,
	MirroredLock,
	ResQueueLock,
	ResGroupLock,
	FileRepAppendOnlyCommitCountLock,
	SyncRepLock,
	ErrorLogLock,
	FirstWorkfileMgrLock,
	FirstWorkfileQuerySpaceLock = FirstWorkfileMgrLock + NUM_WORKFILEMGR_PARTITIONS,
	FirstBufMappingLock = FirstWorkfileQuerySpaceLock + NUM_WORKFILE_QUERYSPACE_PARTITIONS,
	FirstLockMgrLock = FirstBufMappingLock + NUM_BUFFER_PARTITIONS,
	SessionStateLock = FirstLockMgrLock + NUM_LOCK_PARTITIONS,
	RelfilenodeGenLock,
	FilespaceHashLock,
	TablespaceHashLock,
#ifdef USE_SEGWALREP
	GpReplicationConfigFileLock,
#endif
	/* must be last except for MaxDynamicLWLock: */
	NumFixedLWLocks,

	MaxDynamicLWLock = 1000000000
} LWLockId;

#define LWLOCK_IS_PREDEFINED(lwlock) (lwlock < NumFixedLWLocks + NUM_LOCK_PARTITIONS)

typedef enum LWLockMode
{
	LW_EXCLUSIVE,
	LW_SHARED
} LWLockMode;


#ifdef LOCK_DEBUG
extern bool Trace_lwlocks;
#endif

extern LWLockId LWLockAssign(void);
extern void LWLockAcquire(LWLockId lockid, LWLockMode mode);
extern bool LWLockConditionalAcquire(LWLockId lockid, LWLockMode mode);
extern void LWLockRelease(LWLockId lockid);
extern void LWLockReleaseAll(void);
extern void LWLockWaitCancel(void);
extern bool LWLockHeldByMe(LWLockId lockid);
extern bool LWLockHeldExclusiveByMe(LWLockId lockid);

#ifdef USE_TEST_UTILS_X86
extern uint32 LWLocksHeld(void);
extern LWLockId LWLockHeldLatestId(void);
extern void *LWLockHeldLatestCaller(void);
extern const char *LWLocksHeldStackTraces(void);
#endif /* USE_TEST_UTILS_X86 */

extern int	NumLWLocks(void);
extern Size LWLockShmemSize(void);
extern void CreateLWLocks(void);

extern void RequestAddinLWLocks(int n);

#endif   /* LWLOCK_H */
