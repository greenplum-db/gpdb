/*-------------------------------------------------------------------------
 *
 * lock.c
 *	  POSTGRES primary lock mechanism
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/lmgr/lock.c
 *
 * NOTES
 *	  A lock table is a shared memory hash table.  When
 *	  a process tries to acquire a lock of a type that conflicts
 *	  with existing locks, it is put to sleep using the routines
 *	  in storage/lmgr/proc.c.
 *
 *	  For the most part, this code should be invoked via lmgr.c
 *	  or another lock-management module, not directly.
 *
 *	Interface:
 *
 *	InitLocks(), GetLocksMethodTable(),
 *	LockAcquire(), LockRelease(), LockReleaseAll(),
 *	LockCheckConflicts(), GrantLock()
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "storage/standby.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/resscheduler.h"
#include "utils/resowner.h"


/* This configuration variable is used to set the lock table size */
int			max_locks_per_xact; /* set by guc.c */

#define NLOCKENTS() \
	mul_size(max_locks_per_xact, add_size(MaxBackends, max_prepared_xacts))

#define NRESLOCKENTS() \
	MaxResourceQueues

#define NRESPROCLOCKENTS() \
	mul_size(MaxResourceQueues, MaxBackends)
	
/*
 * Data structures defining the semantics of the standard lock methods.
 *
 * The conflict table defines the semantics of the various lock modes.
 */
static const LOCKMASK LockConflicts[] = {
	0,

	/* AccessShareLock */
	(1 << AccessExclusiveLock),

	/* RowShareLock */
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock),

	/* RowExclusiveLock */
	(1 << ShareLock) | (1 << ShareRowExclusiveLock) |
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock),

	/* ShareUpdateExclusiveLock */
	(1 << ShareUpdateExclusiveLock) |
	(1 << ShareLock) | (1 << ShareRowExclusiveLock) |
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock),

	/* ShareLock */
	(1 << RowExclusiveLock) | (1 << ShareUpdateExclusiveLock) |
	(1 << ShareRowExclusiveLock) |
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock),

	/* ShareRowExclusiveLock */
	(1 << RowExclusiveLock) | (1 << ShareUpdateExclusiveLock) |
	(1 << ShareLock) | (1 << ShareRowExclusiveLock) |
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock),

	/* ExclusiveLock */
	(1 << RowShareLock) |
	(1 << RowExclusiveLock) | (1 << ShareUpdateExclusiveLock) |
	(1 << ShareLock) | (1 << ShareRowExclusiveLock) |
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock),

	/* AccessExclusiveLock */
	(1 << AccessShareLock) | (1 << RowShareLock) |
	(1 << RowExclusiveLock) | (1 << ShareUpdateExclusiveLock) |
	(1 << ShareLock) | (1 << ShareRowExclusiveLock) |
	(1 << ExclusiveLock) | (1 << AccessExclusiveLock)

};

/* Names of lock modes, for debug printouts */
static const char *const lock_mode_names[] =
{
	"INVALID",
	"AccessShareLock",
	"RowShareLock",
	"RowExclusiveLock",
	"ShareUpdateExclusiveLock",
	"ShareLock",
	"ShareRowExclusiveLock",
	"ExclusiveLock",
	"AccessExclusiveLock"
};

#ifndef LOCK_DEBUG
static bool Dummy_trace = false;
#endif

const LockMethodData default_lockmethod = {
	AccessExclusiveLock,		/* highest valid lock mode number */
	LockConflicts,
	lock_mode_names,
#ifdef LOCK_DEBUG
	&Trace_locks
#else
	&Dummy_trace
#endif
};

const LockMethodData user_lockmethod = {
	AccessExclusiveLock,		/* highest valid lock mode number */
	LockConflicts,
	lock_mode_names,
#ifdef LOCK_DEBUG
	&Trace_userlocks
#else
	&Dummy_trace
#endif
};

const LockMethodData resource_lockmethod = {
	AccessExclusiveLock,        /* highest valid lock mode number */
	LockConflicts,
	lock_mode_names,
#ifdef LOCK_DEBUG
	&Trace_locks
#else
	&Dummy_trace
#endif
};


/*
 * map from lock method id to the lock table data structures
 */
const LockMethod LockMethods[] = {
	NULL,
	&default_lockmethod,
	&user_lockmethod,
	&resource_lockmethod
};


/* Record that's written to 2PC state file when a lock is persisted */
typedef struct TwoPhaseLockRecord
{
	LOCKTAG		locktag;
	LOCKMODE	lockmode;
} TwoPhaseLockRecord;


/*
 * Count of the number of fast path lock slots we believe to be used.  This
 * might be higher than the real number if another backend has transferred
 * our locks to the primary lock table, but it can never be lower than the
 * real value, since only we can acquire locks on our own behalf.
 */
static int	FastPathLocalUseCount = 0;

/* Macros for manipulating proc->fpLockBits */
#define FAST_PATH_BITS_PER_SLOT			3
#define FAST_PATH_LOCKNUMBER_OFFSET		1
#define FAST_PATH_MASK					((1 << FAST_PATH_BITS_PER_SLOT) - 1)
#define FAST_PATH_GET_BITS(proc, n) \
	(((proc)->fpLockBits >> (FAST_PATH_BITS_PER_SLOT * n)) & FAST_PATH_MASK)
#define FAST_PATH_BIT_POSITION(n, l) \
	(AssertMacro((l) >= FAST_PATH_LOCKNUMBER_OFFSET), \
	 AssertMacro((l) < FAST_PATH_BITS_PER_SLOT+FAST_PATH_LOCKNUMBER_OFFSET), \
	 AssertMacro((n) < FP_LOCK_SLOTS_PER_BACKEND), \
	 ((l) - FAST_PATH_LOCKNUMBER_OFFSET + FAST_PATH_BITS_PER_SLOT * (n)))
#define FAST_PATH_SET_LOCKMODE(proc, n, l) \
	 (proc)->fpLockBits |= UINT64CONST(1) << FAST_PATH_BIT_POSITION(n, l)
#define FAST_PATH_CLEAR_LOCKMODE(proc, n, l) \
	 (proc)->fpLockBits &= ~(UINT64CONST(1) << FAST_PATH_BIT_POSITION(n, l))
#define FAST_PATH_CHECK_LOCKMODE(proc, n, l) \
	 ((proc)->fpLockBits & (UINT64CONST(1) << FAST_PATH_BIT_POSITION(n, l)))
/*
 * fpHoldTillEndXactBits is used for GDD
 * bits is the result of FAST_PATH_GET_BITS(proc, n)
 * we simply set the whole bits to the corresponding bits
 * as fpLockBits.
 */
#define FAST_PATH_SET_HOLD_TILL_END_XACT(proc, n, bits) \
	 (proc)->fpHoldTillEndXactBits |= ((bits) & FAST_PATH_MASK) << (FAST_PATH_BITS_PER_SLOT * n)
#define FAST_PATH_GET_HOLD_TILL_END_XACT_BITS(proc, n) \
	(((proc)->fpHoldTillEndXactBits >> (FAST_PATH_BITS_PER_SLOT * n)) & FAST_PATH_MASK)
/*
 * The fast-path lock mechanism is concerned only with relation locks on
 * unshared relations by backends bound to a database.	The fast-path
 * mechanism exists mostly to accelerate acquisition and release of locks
 * that rarely conflict.  Because ShareUpdateExclusiveLock is
 * self-conflicting, it can't use the fast-path mechanism; but it also does
 * not conflict with any of the locks that do, so we can ignore it completely.
 */
#define EligibleForRelationFastPath(locktag, mode) \
	((locktag)->locktag_lockmethodid == DEFAULT_LOCKMETHOD && \
	(locktag)->locktag_type == LOCKTAG_RELATION && \
	(locktag)->locktag_field1 == MyDatabaseId && \
	MyDatabaseId != InvalidOid && \
	(mode) < ShareUpdateExclusiveLock)
#define ConflictsWithRelationFastPath(locktag, mode) \
	((locktag)->locktag_lockmethodid == DEFAULT_LOCKMETHOD && \
	(locktag)->locktag_type == LOCKTAG_RELATION && \
	(locktag)->locktag_field1 != InvalidOid && \
	(mode) > ShareUpdateExclusiveLock)

static bool FastPathGrantRelationLock(Oid relid, LOCKMODE lockmode);
static bool FastPathUnGrantRelationLock(Oid relid, LOCKMODE lockmode);
static bool FastPathTransferRelationLocks(LockMethod lockMethodTable,
							  const LOCKTAG *locktag, uint32 hashcode);
static PROCLOCK *FastPathGetRelationLockEntry(LOCALLOCK *locallock);
static void VirtualXactLockTableCleanup(void);

/*
 * To make the fast-path lock mechanism work, we must have some way of
 * preventing the use of the fast-path when a conflicting lock might be
 * present.  We partition* the locktag space into FAST_PATH_HASH_BUCKETS
 * partitions, and maintain an integer count of the number of "strong" lockers
 * in each partition.  When any "strong" lockers are present (which is
 * hopefully not very often), the fast-path mechanism can't be used, and we
 * must fall back to the slower method of pushing matching locks directly
 * into the main lock tables.
 *
 * The deadlock detector does not know anything about the fast path mechanism,
 * so any locks that might be involved in a deadlock must be transferred from
 * the fast-path queues to the main lock table.
 */

#define FAST_PATH_STRONG_LOCK_HASH_BITS			10
#define FAST_PATH_STRONG_LOCK_HASH_PARTITIONS \
	(1 << FAST_PATH_STRONG_LOCK_HASH_BITS)
#define FastPathStrongLockHashPartition(hashcode) \
	((hashcode) % FAST_PATH_STRONG_LOCK_HASH_PARTITIONS)

typedef struct
{
	slock_t		mutex;
	uint32		count[FAST_PATH_STRONG_LOCK_HASH_PARTITIONS];
} FastPathStrongRelationLockData;

FastPathStrongRelationLockData *FastPathStrongRelationLocks;


/*
 * Pointers to hash tables containing lock state
 *
 * The LockMethodLockHash and LockMethodProcLockHash hash tables are in
 * shared memory; LockMethodLocalHash is local to each backend.
 */
HTAB *LockMethodLockHash;
HTAB *LockMethodProcLockHash;
HTAB *LockMethodLocalHash;


/* private state for error cleanup */
static LOCALLOCK *StrongLockInProgress;
LOCALLOCK *awaitedLock;
ResourceOwner awaitedOwner;


#ifdef LOCK_DEBUG

/*------
 * The following configuration options are available for lock debugging:
 *
 *	   TRACE_LOCKS		-- give a bunch of output what's going on in this file
 *	   TRACE_USERLOCKS	-- same but for user locks
 *	   TRACE_LOCK_OIDMIN-- do not trace locks for tables below this oid
 *						   (use to avoid output on system tables)
 *	   TRACE_LOCK_TABLE -- trace locks on this table (oid) unconditionally
 *	   DEBUG_DEADLOCKS	-- currently dumps locks at untimely occasions ;)
 *
 * Furthermore, but in storage/lmgr/lwlock.c:
 *	   TRACE_LWLOCKS	-- trace lightweight locks (pretty useless)
 *
 * Define LOCK_DEBUG at compile time to get all these enabled.
 * --------
 */

int			Trace_lock_oidmin = FirstNormalObjectId;
bool		Trace_locks = false;
bool		Trace_userlocks = false;
int			Trace_lock_table = 0;
bool		Debug_deadlocks = false;


inline static bool
LOCK_DEBUG_ENABLED(const LOCKTAG *tag)
{
	return
		(*(LockMethods[tag->locktag_lockmethodid]->trace_flag) &&
		 ((Oid) tag->locktag_field2 >= (Oid) Trace_lock_oidmin))
		|| (Trace_lock_table &&
			(tag->locktag_field2 == Trace_lock_table));
}


inline static void
LOCK_PRINT(const char *where, const LOCK *lock, LOCKMODE type)
{
	if (LOCK_DEBUG_ENABLED(&lock->tag))
		elog(LOG,
			 "%s: lock(%p) id(%u,%u,%u,%u,%u,%u) grantMask(%x) "
			 "req(%d,%d,%d,%d,%d,%d,%d)=%d "
			 "grant(%d,%d,%d,%d,%d,%d,%d)=%d wait(%d) type(%s)",
			 where, lock,
			 lock->tag.locktag_field1, lock->tag.locktag_field2,
			 lock->tag.locktag_field3, lock->tag.locktag_field4,
			 lock->tag.locktag_type, lock->tag.locktag_lockmethodid,
			 lock->grantMask,
			 lock->requested[1], lock->requested[2], lock->requested[3],
			 lock->requested[4], lock->requested[5], lock->requested[6],
			 lock->requested[7], lock->nRequested,
			 lock->granted[1], lock->granted[2], lock->granted[3],
			 lock->granted[4], lock->granted[5], lock->granted[6],
			 lock->granted[7], lock->nGranted,
			 lock->waitProcs.size,
			 LockMethods[LOCK_LOCKMETHOD(*lock)]->lockModeNames[type]);
}


inline static void
PROCLOCK_PRINT(const char *where, const PROCLOCK *proclockP)
{
	if (LOCK_DEBUG_ENABLED(&proclockP->tag.myLock->tag))
		elog(LOG,
			 "%s: proclock(%p) lock(%p) method(%u) proc(%p) hold(%x)",
			 where, proclockP, proclockP->tag.myLock,
			 PROCLOCK_LOCKMETHOD(*(proclockP)),
			 proclockP->tag.myProc, (int) proclockP->holdMask);
}
#else							/* not LOCK_DEBUG */

#define LOCK_PRINT(where, lock, type)
#define PROCLOCK_PRINT(where, proclockP)
#endif   /* not LOCK_DEBUG */


static uint32 proclock_hash(const void *key, Size keysize);
void RemoveLocalLock(LOCALLOCK *locallock);
static PROCLOCK *SetupLockInTable(LockMethod lockMethodTable, PGPROC *proc,
				 const LOCKTAG *locktag, uint32 hashcode, LOCKMODE lockmode);
static void GrantLockLocal(LOCALLOCK *locallock, ResourceOwner owner);
static void BeginStrongLockAcquire(LOCALLOCK *locallock, uint32 fasthashcode);
static void FinishStrongLockAcquire(void);
static void WaitOnLock(LOCALLOCK *locallock, ResourceOwner owner);
static void ReleaseLockIfHeld(LOCALLOCK *locallock, bool sessionLock);
static void LockReassignOwner(LOCALLOCK *locallock, ResourceOwner parent);
static bool UnGrantLock(LOCK *lock, LOCKMODE lockmode,
			PROCLOCK *proclock, LockMethod lockMethodTable);
static void CleanUpLock(LOCK *lock, PROCLOCK *proclock,
			LockMethod lockMethodTable, uint32 hashcode,
			bool wakeupNeeded);
static void LockRefindAndRelease(LockMethod lockMethodTable, PGPROC *proc,
					 LOCKTAG *locktag, LOCKMODE lockmode,
					 bool decrement_strong_lock_count);
static bool setFPHoldTillEndXact(Oid relid);


/*
 * InitLocks -- Initialize the lock manager's data structures.
 *
 * This is called from CreateSharedMemoryAndSemaphores(), which see for
 * more comments.  In the normal postmaster case, the shared hash tables
 * are created here, as well as a locallock hash table that will remain
 * unused and empty in the postmaster itself.  Backends inherit the pointers
 * to the shared tables via fork(), and also inherit an image of the locallock
 * hash table, which they proceed to use.  In the EXEC_BACKEND case, each
 * backend re-executes this code to obtain pointers to the already existing
 * shared hash tables and to create its locallock hash table.
 */
void
InitLocks(void)
{
	HASHCTL		info;
	int			hash_flags;
	long		init_table_size,
				max_table_size;
	bool		found;

	/*
	 * Compute init/max size to request for lock hashtables.  Note these
	 * calculations must agree with LockShmemSize!
	 */
	max_table_size = NLOCKENTS();
	
		/* Allow for extra entries if resource locking is enabled. */
	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled())
	{
		add_size(max_table_size, NRESLOCKENTS() );
		//add_size(max_plock_table_size, NRESPROCLOCKENTS() );
	}
	
	init_table_size = max_table_size / 2;

	/*
	 * Allocate hash table for LOCK structs.  This stores per-locked-object
	 * information.
	 */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(LOCKTAG);
	info.entrysize = sizeof(LOCK);
	info.hash = tag_hash;
	info.num_partitions = NUM_LOCK_PARTITIONS;
	hash_flags = (HASH_ELEM | HASH_FUNCTION | HASH_PARTITION);

	LockMethodLockHash = ShmemInitHash("LOCK hash",
									   init_table_size,
									   max_table_size,
									   &info,
									   hash_flags);

	/* Assume an average of 2 holders per lock */
	max_table_size *= 2;
	init_table_size *= 2;

	/*
	 * Allocate hash table for PROCLOCK structs.  This stores
	 * per-lock-per-holder information.
	 */
	info.keysize = sizeof(PROCLOCKTAG);
	info.entrysize = sizeof(PROCLOCK);
	info.hash = proclock_hash;
	info.num_partitions = NUM_LOCK_PARTITIONS;
	hash_flags = (HASH_ELEM | HASH_FUNCTION | HASH_PARTITION);

	LockMethodProcLockHash = ShmemInitHash("PROCLOCK hash",
										   init_table_size,
										   max_table_size,
										   &info,
										   hash_flags);

	/*
	 * Allocate fast-path structures.
	 */
	FastPathStrongRelationLocks =
		ShmemInitStruct("Fast Path Strong Relation Lock Data",
						sizeof(FastPathStrongRelationLockData), &found);
	if (!found)
		SpinLockInit(&FastPathStrongRelationLocks->mutex);

	/*
	 * Allocate non-shared hash table for LOCALLOCK structs.  This stores lock
	 * counts and resource owner information.
	 *
	 * The non-shared table could already exist in this process (this occurs
	 * when the postmaster is recreating shared memory after a backend crash).
	 * If so, delete and recreate it.  (We could simply leave it, since it
	 * ought to be empty in the postmaster, but for safety let's zap it.)
	 */
	if (LockMethodLocalHash)
		hash_destroy(LockMethodLocalHash);

	info.keysize = sizeof(LOCALLOCKTAG);
	info.entrysize = sizeof(LOCALLOCK);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	LockMethodLocalHash = hash_create("LOCALLOCK hash",
									  16,
									  &info,
									  hash_flags);
}


/*
 * Fetch the lock method table associated with a given lock
 */
LockMethod
GetLocksMethodTable(const LOCK *lock)
{
	LOCKMETHODID lockmethodid = LOCK_LOCKMETHOD(*lock);

	Assert(0 < lockmethodid && lockmethodid < lengthof(LockMethods));
	return LockMethods[lockmethodid];
}


/*
 * Compute the hash code associated with a LOCKTAG.
 *
 * To avoid unnecessary recomputations of the hash code, we try to do this
 * just once per function, and then pass it around as needed.  Aside from
 * passing the hashcode to hash_search_with_hash_value(), we can extract
 * the lock partition number from the hashcode.
 */
uint32
LockTagHashCode(const LOCKTAG *locktag)
{
	return get_hash_value(LockMethodLockHash, (const void *) locktag);
}

/*
 * Compute the hash code associated with a PROCLOCKTAG.
 *
 * Because we want to use just one set of partition locks for both the
 * LOCK and PROCLOCK hash tables, we have to make sure that PROCLOCKs
 * fall into the same partition number as their associated LOCKs.
 * dynahash.c expects the partition number to be the low-order bits of
 * the hash code, and therefore a PROCLOCKTAG's hash code must have the
 * same low-order bits as the associated LOCKTAG's hash code.  We achieve
 * this with this specialized hash function.
 */
static uint32
proclock_hash(const void *key, Size keysize)
{
	const PROCLOCKTAG *proclocktag = (const PROCLOCKTAG *) key;
	uint32		lockhash;
	Datum		procptr;

	Assert(keysize == sizeof(PROCLOCKTAG));

	/* Look into the associated LOCK object, and compute its hash code */
	lockhash = LockTagHashCode(&proclocktag->myLock->tag);

	/*
	 * To make the hash code also depend on the PGPROC, we xor the proc
	 * struct's address into the hash code, left-shifted so that the
	 * partition-number bits don't change.  Since this is only a hash, we
	 * don't care if we lose high-order bits of the address; use an
	 * intermediate variable to suppress cast-pointer-to-int warnings.
	 */
	procptr = PointerGetDatum(proclocktag->myProc);
	lockhash ^= ((uint32) procptr) << LOG2_NUM_LOCK_PARTITIONS;

	return lockhash;
}




/*
 * LockAcquire -- Check for lock conflicts, sleep if conflict found,
 *		set lock if/when no conflicts.
 *
 * Inputs:
 *	locktag: unique identifier for the lockable object
 *	lockmode: lock mode to acquire
 *	sessionLock: if true, acquire lock for session not current transaction
 *	dontWait: if true, don't wait to acquire lock
 *
 * Returns one of:
 *		LOCKACQUIRE_NOT_AVAIL		lock not available, and dontWait=true
 *		LOCKACQUIRE_OK				lock successfully acquired
 *		LOCKACQUIRE_ALREADY_HELD	incremented count for lock already held
 *
 * In the normal case where dontWait=false and the caller doesn't need to
 * distinguish a freshly acquired lock from one already taken earlier in
 * this same transaction, there is no need to examine the return value.
 *
 * Side Effects: The lock is acquired and recorded in lock tables.
 *
 * NOTE: if we wait for the lock, there is no way to abort the wait
 * short of aborting the transaction.
 */
LockAcquireResult
LockAcquire(const LOCKTAG *locktag,
			LOCKMODE lockmode,
			bool sessionLock,
			bool dontWait)
{
	return LockAcquireExtended(locktag, lockmode, sessionLock, dontWait, true);
}

/*
 * LockAcquireExtended - allows us to specify additional options
 *
 * reportMemoryError specifies whether a lock request that fills the
 * lock table should generate an ERROR or not. This allows a priority
 * caller to note that the lock table is full and then begin taking
 * extreme action to reduce the number of other lock holders before
 * retrying the action.
 */
LockAcquireResult
LockAcquireExtended(const LOCKTAG *locktag,
					LOCKMODE lockmode,
					bool sessionLock,
					bool dontWait,
					bool reportMemoryError)
{
	LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
	LockMethod	lockMethodTable;
	LOCALLOCKTAG localtag;
	LOCALLOCK  *locallock;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	bool		found;
	ResourceOwner owner;
	uint32		hashcode;
	LWLockId	partitionLock;
	int			status;
	bool		log_lock = false;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];
	if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
		elog(ERROR, "unrecognized lock mode: %d", lockmode);

	if (RecoveryInProgress() && !InRecovery &&
		(locktag->locktag_type == LOCKTAG_OBJECT ||
		 locktag->locktag_type == LOCKTAG_RELATION) &&
		lockmode > RowExclusiveLock)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("cannot acquire lock mode %s on database objects while recovery is in progress",
						lockMethodTable->lockModeNames[lockmode]),
				 errhint("Only RowExclusiveLock or less can be acquired on database objects during recovery.")));

#ifdef LOCK_DEBUG
	if (LOCK_DEBUG_ENABLED(locktag))
		elog(LOG, "LockAcquire: lock [%u,%u] %s",
			 locktag->locktag_field1, locktag->locktag_field2,
			 lockMethodTable->lockModeNames[lockmode]);
#endif

	/* Identify owner for lock */
	if (sessionLock)
		owner = NULL;
	else
		owner = CurrentResourceOwner;

	/*
	 * Find or create a LOCALLOCK entry for this lock and lockmode
	 */
	MemSet(&localtag, 0, sizeof(localtag));		/* must clear padding */
	localtag.lock = *locktag;
	localtag.mode = lockmode;

	locallock = (LOCALLOCK *) hash_search(LockMethodLocalHash,
										  (void *) &localtag,
										  HASH_ENTER, &found);

	/*
	 * if it's a new locallock object, initialize it
	 */
	if (!found)
	{
		locallock->lock = NULL;
		locallock->proclock = NULL;
		locallock->hashcode = LockTagHashCode(&(localtag.lock));
		locallock->nLocks = 0;
		locallock->numLockOwners = 0;
		locallock->maxLockOwners = 8;
		locallock->holdsStrongLockCount = FALSE;
		locallock->lockOwners = NULL;
		locallock->lockOwners = (LOCALLOCKOWNER *)
			MemoryContextAlloc(TopMemoryContext,
						  locallock->maxLockOwners * sizeof(LOCALLOCKOWNER));
	}
	else
	{
		/* Make sure there will be room to remember the lock */
		if (locallock->numLockOwners >= locallock->maxLockOwners)
		{
			int			newsize = locallock->maxLockOwners * 2;

			locallock->lockOwners = (LOCALLOCKOWNER *)
				repalloc(locallock->lockOwners,
						 newsize * sizeof(LOCALLOCKOWNER));
			locallock->maxLockOwners = newsize;
		}
	}
	hashcode = locallock->hashcode;

	/*
	 * If we already hold the lock, we can just increase the count locally.
	 */
	if (locallock->nLocks > 0)
	{
		GrantLockLocal(locallock, owner);
		return LOCKACQUIRE_ALREADY_HELD;
	}
	
#ifdef USE_TEST_UTILS_X86
	if (gp_test_deadlock_hazard && !dontWait)
	{
		/* blocking lock request, check if any lightweight lock is already held */
		LWLockHeldDetect(locktag, lockmode);
	}
#endif /* USE_TEST_UTILS_X86 */

	/*
	 * lockHolder is the gang member that should hold and manage locks for this
	 * transaction.  In Utility mode, or on the QD, it's allways myself.
	 * 
	 * On the QEs, it should normally be the Writer gang member.
	 */
	if (lockHolderProcPtr == NULL)
		lockHolderProcPtr = MyProc;
	
	if (lockmethodid == DEFAULT_LOCKMETHOD && locktag->locktag_type != LOCKTAG_TRANSACTION)
	{
		if (Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		{	
			if (lockHolderProcPtr == NULL || lockHolderProcPtr == MyProc)
			{
				/* Find the guy who should manage our locks */
				PGPROC * proc = FindProcByGpSessionId(gp_session_id);
				int count = 0;
				while(proc==NULL && count < 5)
				{
					pg_usleep( /* microseconds */ 2000);
					count++;
					CHECK_FOR_INTERRUPTS();
					proc = FindProcByGpSessionId(gp_session_id);
				}
				if (proc != NULL)
				{
					elog(DEBUG1,"Found writer proc entry.  My Pid %d, his pid %d", MyProc-> pid, proc->pid);
					lockHolderProcPtr = proc;
				}
				else
					elog(ERROR, "reader could not find writer proc entry, "
						 "lock [%u,%u] %s %d", locktag->locktag_field1,
						 locktag->locktag_field2, lock_mode_names[lockmode],
						 (int)locktag->locktag_type);
			}
		}
	}

	/*
	 * Emit a WAL record if acquisition of this lock needs to be replayed in a
	 * standby server. Only AccessExclusiveLocks can conflict with lock types
	 * that read-only transactions can acquire in a standby server.
	 *
	 * Make sure this definition matches the one in
	 * GetRunningTransactionLocks().
	 *
	 * First we prepare to log, then after lock acquired we issue log record.
	 */
	if (lockmode >= AccessExclusiveLock &&
		locktag->locktag_type == LOCKTAG_RELATION &&
		!RecoveryInProgress() &&
		XLogStandbyInfoActive())
	{
		LogAccessExclusiveLockPrepare();
		log_lock = true;
	}

	/*
	 * Attempt to take lock via fast path, if eligible.  But if we remember
	 * having filled up the fast path array, we don't attempt to make any
	 * further use of it until we release some locks.  It's possible that some
	 * other backend has transferred some of those locks to the shared hash
	 * table, leaving space free, but it's not worth acquiring the LWLock just
	 * to check.  It's also possible that we're acquiring a second or third
	 * lock type on a relation we have already locked using the fast-path, but
	 * for now we don't worry about that case either.
	 */
	if (EligibleForRelationFastPath(locktag, lockmode)
		&& FastPathLocalUseCount < FP_LOCK_SLOTS_PER_BACKEND)
	{
		uint32		fasthashcode = FastPathStrongLockHashPartition(hashcode);
		bool		acquired;

		/*
		 * LWLockAcquire acts as a memory sequencing point, so it's safe to
		 * assume that any strong locker whose increment to
		 * FastPathStrongRelationLocks->counts becomes visible after we test
		 * it has yet to begin to transfer fast-path locks.
		 */
		LWLockAcquire(MyProc->backendLock, LW_EXCLUSIVE);
		if (FastPathStrongRelationLocks->count[fasthashcode] != 0)
			acquired = false;
		else
			acquired = FastPathGrantRelationLock(locktag->locktag_field2,
												 lockmode);
		LWLockRelease(MyProc->backendLock);
		if (acquired)
		{
			GrantLockLocal(locallock, owner);
			return LOCKACQUIRE_OK;
		}
	}

	/*
	 * If this lock could potentially have been taken via the fast-path by
	 * some other backend, we must (temporarily) disable further use of the
	 * fast-path for this lock tag, and migrate any locks already taken via
	 * this method to the main lock table.
	 */
	if (ConflictsWithRelationFastPath(locktag, lockmode))
	{
		uint32		fasthashcode = FastPathStrongLockHashPartition(hashcode);

		BeginStrongLockAcquire(locallock, fasthashcode);
		if (!FastPathTransferRelationLocks(lockMethodTable, locktag,
										   hashcode))
		{
			AbortStrongLockAcquire();
			if (reportMemoryError)
				ereport(ERROR,
						(errcode(ERRCODE_OUT_OF_MEMORY),
						 errmsg("out of shared memory"),
						 errhint("You might need to increase max_locks_per_transaction.")));
			else
				return LOCKACQUIRE_NOT_AVAIL;
		}
	}

	/*
	 * We didn't find the lock in our LOCALLOCK table, and we didn't manage to
	 * take it via the fast-path, either, so we've got to mess with the shared
	 * lock table.
	 */
	partitionLock = LockHashPartitionLock(hashcode);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * Find or create a proclock entry with this tag
	 */
	proclock = SetupLockInTable(lockMethodTable, MyProc, locktag,
								hashcode, lockmode);
	if (!proclock)
	{
		AbortStrongLockAcquire();
		LWLockRelease(partitionLock);
		if (reportMemoryError)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of shared memory"),
					 errhint("You might need to increase max_locks_per_transaction.")));
		else
			return LOCKACQUIRE_NOT_AVAIL;
	}
	locallock->proclock = proclock;
	lock = proclock->tag.myLock;
	locallock->lock = lock;

	/*
	 * We shouldn't already hold the desired lock; else locallock table is
	 * broken.
	 */
	if (Gp_role != GP_ROLE_UTILITY)
	{
		if (proclock->holdMask & LOCKBIT_ON(lockmode))
		{
			elog(LOG, "lock %s on object %u/%u/%u is already held",
				 lock_mode_names[lockmode],
				 lock->tag.locktag_field1, lock->tag.locktag_field2,
				 lock->tag.locktag_field3);
			if (MyProc == lockHolderProcPtr)
			{
				elog(LOG, "writer found lock %s on object %u/%u/%u that it didn't know it held",
						 lock_mode_names[lockmode],
						 lock->tag.locktag_field1, lock->tag.locktag_field2,
						 lock->tag.locktag_field3);
				GrantLock(lock, proclock, lockmode);
				GrantLockLocal(locallock, owner);
			}
			else
			{
				if (MyProc != lockHolderProcPtr)
				{
					elog(LOG, "reader found lock %s on object %u/%u/%u which is already held by writer",
						 lock_mode_names[lockmode],
						 lock->tag.locktag_field1, lock->tag.locktag_field2,
						 lock->tag.locktag_field3);
				}
				lock->nRequested--;
				lock->requested[lockmode]--;
			}
			LWLockRelease(partitionLock);
			return LOCKACQUIRE_ALREADY_HELD;
		}
		
	}
	else if (proclock->holdMask & LOCKBIT_ON(lockmode))
	{
		ereport(ERROR, (errmsg("lock %s on object %u/%u/%u is already held",
			 lockMethodTable->lockModeNames[lockmode],
			 lock->tag.locktag_field1, lock->tag.locktag_field2,
			 lock->tag.locktag_field3)));
	}

	if (MyProc == lockHolderProcPtr)
	{
		/*
		 * We are a writer or utility mode connection.  The following logic is
		 * identical to upstream PostgreSQL.
		 */

		/*
		 * If lock requested conflicts with locks requested by waiters, must join
		 * wait queue.	Otherwise, check for conflict with already-held locks.
		 * (That's last because most complex check.)
		 */
		if (lockMethodTable->conflictTab[lockmode] & lock->waitMask)
			status = STATUS_FOUND;
		else
			status = LockCheckConflicts(lockMethodTable, lockmode,
										lock, proclock, MyProc);
	}
	else
	{
		/*
		 * We are a reader, check waitMask conflict only if the writer doesn't
		 * hold this lock.  We don't want a reader waiting for a lock that the
		 * writer is holding.  This could lead to a deadlock.  If writer
		 * doesn't hold the lock, waitMask conflict must be checked to avoid
		 * starvation of backends already waiting on the same lock.
		 */
		Assert(!Gp_is_writer);

		PROCLOCKTAG writerProcLockTag;
		uint32 writerProcLockHashCode;

		writerProcLockTag.myLock = lock;
		writerProcLockTag.myProc = lockHolderProcPtr;
		writerProcLockHashCode = ProcLockHashCode(&writerProcLockTag, hashcode);
		/*
		 * It is safe to access LockMethodProcLock hash table because
		 * partitionLock is already held at this point.
		 */
		Assert(LWLockHeldByMe(partitionLock));
		PROCLOCK *writerProcLock = (PROCLOCK *)
			hash_search_with_hash_value(LockMethodProcLockHash,
										(void *) &writerProcLockTag,
										writerProcLockHashCode,
										HASH_FIND,
										&found);
		if (found && writerProcLock->holdMask)
		{
			/* Writer holds the same lock, bypass waitMask check. */
			status = LockCheckConflicts(lockMethodTable, lockmode,
										lock, proclock, MyProc);
		}
		else
		{
			/*
			 * Writer either hasn't requested this lock or is waiting on this
			 * lock.  Checking for waitMask conflict is necessary to avoid
			 * starvation of existing waiters.  Special case is conflict with
			 * awaiting writer's lockmode.  Should the reader move ahead or
			 * continue to wait?  It seems best to keep parity with behavior
			 * prior to this change, which is to let the reader wait.
			 */
			if (lockMethodTable->conflictTab[lockmode] & lock->waitMask)
				status = STATUS_FOUND;
			else
				status = LockCheckConflicts(lockMethodTable, lockmode,
											lock, proclock, MyProc);
		}
	}

	if (status == STATUS_OK)
	{
		if (MyProc != lockHolderProcPtr)
					elog(DEBUG1, "Reader found lock %s on object %u/%u/%u doesn't conflict ",
						 lock_mode_names[lockmode],
						 lock->tag.locktag_field1, lock->tag.locktag_field2,
						 lock->tag.locktag_field3);
		/* No conflict with held or previously requested locks */
		GrantLock(lock, proclock, lockmode);
		GrantLockLocal(locallock, owner);
	}
	else
	{
		Assert(status == STATUS_FOUND);

		/*
		 * We can't acquire the lock immediately.  If caller specified no
		 * blocking, remove useless table entries and return NOT_AVAIL without
		 * waiting.
		 */
		if (dontWait)
		{
			AbortStrongLockAcquire();
			if (proclock->holdMask == 0)
			{
				uint32		proclock_hashcode;

				proclock_hashcode = ProcLockHashCode(&proclock->tag, hashcode);
				SHMQueueDelete(&proclock->lockLink);
				SHMQueueDelete(&proclock->procLink);
				if (!hash_search_with_hash_value(LockMethodProcLockHash,
												 (void *) &(proclock->tag),
												 proclock_hashcode,
												 HASH_REMOVE,
												 NULL))
					elog(PANIC, "proclock table corrupted");
			}
			else
				PROCLOCK_PRINT("LockAcquire: NOWAIT", proclock);
			lock->nRequested--;
			lock->requested[lockmode]--;
			LOCK_PRINT("LockAcquire: conditional lock failed", lock, lockmode);
			Assert((lock->nRequested > 0) && (lock->requested[lockmode] >= 0));
			Assert(lock->nGranted <= lock->nRequested);
			LWLockRelease(partitionLock);
			if (locallock->nLocks == 0)
				RemoveLocalLock(locallock);
			return LOCKACQUIRE_NOT_AVAIL;
		}
		
		if (Gp_role == GP_ROLE_EXECUTE)
		{
			if (!Gp_is_writer)
				elog(LOG,"Reader gang member waiting on a lock [%u,%u] %s",
					 locktag->locktag_field1, locktag->locktag_field2,
					 lock_mode_names[lockmode]);
			 else
				 elog(DEBUG1,"Writer gang member waiting on a lock [%u,%u] %s",
					 locktag->locktag_field1, locktag->locktag_field2,
					 lock_mode_names[lockmode]);
		}
		
		/*
		 * Set bitmask of locks this process already holds on this object.
		 */
		MyProc->heldLocks = proclock->holdMask;

		/*
		 * Sleep till someone wakes me up.
		 */

		TRACE_POSTGRESQL_LOCK_WAIT_START(locktag->locktag_field1,
										 locktag->locktag_field2,
										 locktag->locktag_field3,
										 locktag->locktag_field4,
										 locktag->locktag_type,
										 lockmode);

		WaitOnLock(locallock, owner);

		TRACE_POSTGRESQL_LOCK_WAIT_DONE(locktag->locktag_field1,
										locktag->locktag_field2,
										locktag->locktag_field3,
										locktag->locktag_field4,
										locktag->locktag_type,
										lockmode);

		/*
		 * NOTE: do not do any material change of state between here and
		 * return.	All required changes in locktable state must have been
		 * done when the lock was granted to us --- see notes in WaitOnLock.
		 */

		/*
		 * Check the proclock entry status, in case something in the ipc
		 * communication doesn't work correctly.
		 */
		if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
		{
			AbortStrongLockAcquire();
			PROCLOCK_PRINT("LockAcquire: INCONSISTENT", proclock);
			LOCK_PRINT("LockAcquire: INCONSISTENT", lock, lockmode);
			/* Should we retry ? */
			LWLockRelease(partitionLock);
			elog(ERROR, "LockAcquire failed");
		}
		PROCLOCK_PRINT("LockAcquire: granted", proclock);
		LOCK_PRINT("LockAcquire: granted", lock, lockmode);
	}

	/*
	 * Lock state is fully up-to-date now; if we error out after this, no
	 * special error cleanup is required.
	 */
	FinishStrongLockAcquire();

	LWLockRelease(partitionLock);

	/*
	 * Emit a WAL record if acquisition of this lock need to be replayed in a
	 * standby server.
	 */
	if (log_lock)
	{
		/*
		 * Decode the locktag back to the original values, to avoid sending
		 * lots of empty bytes with every message.	See lock.h to check how a
		 * locktag is defined for LOCKTAG_RELATION
		 */
		LogAccessExclusiveLock(locktag->locktag_field1,
							   locktag->locktag_field2);
	}

	return LOCKACQUIRE_OK;
}

/*
 * Find or create LOCK and PROCLOCK objects as needed for a new lock
 * request.
 */
static PROCLOCK *
SetupLockInTable(LockMethod lockMethodTable, PGPROC *proc,
				 const LOCKTAG *locktag, uint32 hashcode, LOCKMODE lockmode)
{
	LOCK	   *lock;
	PROCLOCK   *proclock;
	PROCLOCKTAG proclocktag;
	uint32		proclock_hashcode;
	bool		found;

	/*
	 * Find or create a lock with this tag.
	 *
	 * Note: if the locallock object already existed, it might have a pointer
	 * to the lock already ... but we probably should not assume that that
	 * pointer is valid, since a lock object with no locks can go away
	 * anytime.
	 */
	lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
												(const void *) locktag,
												hashcode,
												HASH_ENTER_NULL,
												&found);
	if (!lock)
		return NULL;

	/*
	 * if it's a new lock object, initialize it
	 */
	if (!found)
	{
		lock->grantMask = 0;
		lock->waitMask = 0;
		SHMQueueInit(&(lock->procLocks));
		ProcQueueInit(&(lock->waitProcs));
		lock->nRequested = 0;
		lock->nGranted = 0;
		MemSet(lock->requested, 0, sizeof(int) * MAX_LOCKMODES);
		MemSet(lock->granted, 0, sizeof(int) * MAX_LOCKMODES);
		LOCK_PRINT("LockAcquire: new", lock, lockmode);
	}
	else
	{
		LOCK_PRINT("LockAcquire: found", lock, lockmode);
		Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));
		Assert((lock->nGranted >= 0) && (lock->granted[lockmode] >= 0));
		Assert(lock->nGranted <= lock->nRequested);
	}

	/*
	 * Create the hash key for the proclock table.
	 */
	proclocktag.myLock = lock;
	proclocktag.myProc = proc;

	proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

	/*
	 * Find or create a proclock entry with this tag
	 */
	proclock = (PROCLOCK *) hash_search_with_hash_value(LockMethodProcLockHash,
														(void *) &proclocktag,
														proclock_hashcode,
														HASH_ENTER_NULL,
														&found);
	if (!proclock)
	{
		/* Ooops, not enough shmem for the proclock */
		if (lock->nRequested == 0)
		{
			/*
			 * There are no other requestors of this lock, so garbage-collect
			 * the lock object.  We *must* do this to avoid a permanent leak
			 * of shared memory, because there won't be anything to cause
			 * anyone to release the lock object later.
			 */
			Assert(SHMQueueEmpty(&(lock->procLocks)));
			if (!hash_search_with_hash_value(LockMethodLockHash,
											 (void *) &(lock->tag),
											 hashcode,
											 HASH_REMOVE,
											 NULL))
				elog(PANIC, "lock table corrupted");
		}
		return NULL;
	}

	/*
	 * If new, initialize the new entry
	 */
	if (!found)
	{
		uint32		partition = LockHashPartition(hashcode);

		proclock->holdMask = 0;
		proclock->releaseMask = 0;
		/* Add proclock to appropriate lists */
		SHMQueueInsertBefore(&lock->procLocks, &proclock->lockLink);
		SHMQueueInsertBefore(&(proc->myProcLocks[partition]),
							 &proclock->procLink);
		PROCLOCK_PRINT("LockAcquire: new", proclock);
	}
	else
	{
		PROCLOCK_PRINT("LockAcquire: found", proclock);
		Assert((proclock->holdMask & ~lock->grantMask) == 0);

#ifdef CHECK_DEADLOCK_RISK

		/*
		 * Issue warning if we already hold a lower-level lock on this object
		 * and do not hold a lock of the requested level or higher. This
		 * indicates a deadlock-prone coding practice (eg, we'd have a
		 * deadlock if another backend were following the same code path at
		 * about the same time).
		 *
		 * This is not enabled by default, because it may generate log entries
		 * about user-level coding practices that are in fact safe in context.
		 * It can be enabled to help find system-level problems.
		 *
		 * XXX Doing numeric comparison on the lockmodes is a hack; it'd be
		 * better to use a table.  For now, though, this works.
		 */
		{
			int			i;

			for (i = lockMethodTable->numLockModes; i > 0; i--)
			{
				if (proclock->holdMask & LOCKBIT_ON(i))
				{
					if (i >= (int) lockmode)
						break;	/* safe: we have a lock >= req level */
					elog(LOG, "deadlock risk: raising lock level"
						 " from %s to %s on object %u/%u/%u",
						 lockMethodTable->lockModeNames[i],
						 lockMethodTable->lockModeNames[lockmode],
						 lock->tag.locktag_field1, lock->tag.locktag_field2,
						 lock->tag.locktag_field3);
					break;
				}
			}
		}
#endif   /* CHECK_DEADLOCK_RISK */
	}

	/*
	 * lock->nRequested and lock->requested[] count the total number of
	 * requests, whether granted or waiting, so increment those immediately.
	 * The other counts don't increment till we get the lock.
	 */
	lock->nRequested++;
	lock->requested[lockmode]++;
	Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));

	return proclock;
}

/*
 * Subroutine to free a locallock entry
 */
void
RemoveLocalLock(LOCALLOCK *locallock)
{
	int         i;

	for (i = locallock->numLockOwners - 1; i >= 0; i--)
	{
		if (locallock->lockOwners[i].owner != NULL)
			ResourceOwnerForgetLock(locallock->lockOwners[i].owner, locallock);
	}
	if (locallock->lockOwners != NULL) // TODO FIX_COMMIT^ does not have this check, why?
		pfree(locallock->lockOwners);
	locallock->lockOwners = NULL;
	if (locallock->holdsStrongLockCount)
	{
		uint32		fasthashcode;

		fasthashcode = FastPathStrongLockHashPartition(locallock->hashcode);

		SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
		Assert(FastPathStrongRelationLocks->count[fasthashcode] > 0);
		FastPathStrongRelationLocks->count[fasthashcode]--;
		locallock->holdsStrongLockCount = FALSE;
		SpinLockRelease(&FastPathStrongRelationLocks->mutex);
	}
	if (!hash_search(LockMethodLocalHash,
					 (void *) &(locallock->tag),
					 HASH_REMOVE, NULL))
		elog(WARNING, "locallock table corrupted");
}

/*
 * LockCheckConflicts -- test whether requested lock conflicts
 *		with those already granted
 *
 * Returns STATUS_FOUND if conflict, STATUS_OK if no conflict.
 *
 * NOTES:
 *		Here's what makes this complicated: one process's locks don't
 * conflict with one another, no matter what purpose they are held for
 * (eg, session and transaction locks do not conflict).
 * So, we must subtract off our own locks when determining whether the
 * requested new lock conflicts with those already held.
 *
 * In Greenplum Database, the conflict is more complicated;  not only the
 * process itself but also other processes within the same MPP session may
 * have held conflicting locks.  We must take account  into consideration
 * those MPP session member processes to subtract off the lock mask.
 */
int
LockCheckConflicts(LockMethod lockMethodTable,
				   LOCKMODE lockmode,
				   LOCK *lock,
				   PROCLOCK *proclock,
				   PGPROC *proc)
{
	int			numLockModes = lockMethodTable->numLockModes;
	LOCKMASK	otherLocks;
	int			i;

	/*
	 * first check for global conflicts: If no locks conflict with my request,
	 * then I get the lock.
	 *
	 * Checking for conflict: lock->grantMask represents the types of
	 * currently held locks.  conflictTable[lockmode] has a bit set for each
	 * type of lock that conflicts with request.   Bitwise compare tells if
	 * there is a conflict.
	 */
	if (!(lockMethodTable->conflictTab[lockmode] & lock->grantMask))
	{
		PROCLOCK_PRINT("LockCheckConflicts: no conflict", proclock);
		return STATUS_OK;
	}

	/*
	 * Rats.  Something conflicts.	But it could still be our own lock. We have
	 * to construct a conflict mask that does not reflect our own locks, but
	 * only lock types held by other sessions.
	 */
	otherLocks = 0;
	for (i = 1; i <= numLockModes; i++)
	{
		int				ourHolding = 0;

		/*
		 * If I'm not part of MPP session, consider I am only one process
		 * in a session.
		 */
		if (proc->mppSessionId <= 0)
		{
			LOCKMASK	myLocks = proclock->holdMask;
			if (myLocks & LOCKBIT_ON(i))
				ourHolding = 1;
		}
		else
		{
			SHM_QUEUE	   *procLocks = &(lock->procLocks);
			PROCLOCK	   *otherProclock =
					(PROCLOCK *) SHMQueueNext(procLocks, procLocks,
											  offsetof(PROCLOCK, lockLink));
			/*
			 * Go through the proclock queue in the lock.  otherProclock
			 * may be this process itself.
			 */
			while (otherProclock)
			{
				PGPROC	   *otherProc = otherProclock->tag.myProc;

				/*
				 * If processes in my session are holding the lock, mask
				 * it out so that we won't be blocked by them.
				 */
				if (otherProc->mppSessionId == proc->mppSessionId &&
					otherProclock->holdMask & LOCKBIT_ON(i))
					ourHolding++;

				otherProclock =
					(PROCLOCK *) SHMQueueNext(procLocks,
											  &otherProclock->lockLink,
											  offsetof(PROCLOCK, lockLink));
			}
		}

		/*
		 * I'll be blocked only if processes outside of the session are
		 * holding conflicting locks.
		 */
		if (lock->granted[i] > ourHolding)
			otherLocks |= LOCKBIT_ON(i);
	}

	/*
	 * now check again for conflicts.  'otherLocks' describes the types of
	 * locks held by other sessions.  If one of these conflicts with the kind
	 * of lock that I want, there is a conflict and I have to sleep.
	 */
	if (!(lockMethodTable->conflictTab[lockmode] & otherLocks))
	{
		/* no conflict. OK to get the lock */
		PROCLOCK_PRINT("LockCheckConflicts: resolved", proclock);
		return STATUS_OK;
	}

	PROCLOCK_PRINT("LockCheckConflicts: conflicting", proclock);
	return STATUS_FOUND;
}

/*
 * GrantLock -- update the lock and proclock data structures to show
 *		the lock request has been granted.
 *
 * NOTE: if proc was blocked, it also needs to be removed from the wait list
 * and have its waitLock/waitProcLock fields cleared.  That's not done here.
 *
 * NOTE: the lock grant also has to be recorded in the associated LOCALLOCK
 * table entry; but since we may be awaking some other process, we can't do
 * that here; it's done by GrantLockLocal, instead.
 */
void
GrantLock(LOCK *lock, PROCLOCK *proclock, LOCKMODE lockmode)
{
	lock->nGranted++;
	lock->granted[lockmode]++;
	lock->grantMask |= LOCKBIT_ON(lockmode);
	if (lock->granted[lockmode] == lock->requested[lockmode])
		lock->waitMask &= LOCKBIT_OFF(lockmode);
	proclock->holdMask |= LOCKBIT_ON(lockmode);
	LOCK_PRINT("GrantLock", lock, lockmode);
	Assert((lock->nGranted > 0) && (lock->granted[lockmode] > 0));
	Assert(lock->nGranted <= lock->nRequested);
}

/*
 * UnGrantLock -- opposite of GrantLock.
 *
 * Updates the lock and proclock data structures to show that the lock
 * is no longer held nor requested by the current holder.
 *
 * Returns true if there were any waiters waiting on the lock that
 * should now be woken up with ProcLockWakeup.
 */
static bool
UnGrantLock(LOCK *lock, LOCKMODE lockmode,
			PROCLOCK *proclock, LockMethod lockMethodTable)
{
	bool		wakeupNeeded = false;

	Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));
	Assert((lock->nGranted > 0) && (lock->granted[lockmode] > 0));
	Assert(lock->nGranted <= lock->nRequested);

	/*
	 * fix the general lock stats
	 */
	lock->nRequested--;
	lock->requested[lockmode]--;
	lock->nGranted--;
	lock->granted[lockmode]--;

	if (lock->granted[lockmode] == 0)
	{
		/* change the conflict mask.  No more of this lock type. */
		lock->grantMask &= LOCKBIT_OFF(lockmode);
	}

	LOCK_PRINT("UnGrantLock: updated", lock, lockmode);

	/*
	 * We need only run ProcLockWakeup if the released lock conflicts with at
	 * least one of the lock types requested by waiter(s).	Otherwise whatever
	 * conflict made them wait must still exist.  NOTE: before MVCC, we could
	 * skip wakeup if lock->granted[lockmode] was still positive. But that's
	 * not true anymore, because the remaining granted locks might belong to
	 * some waiter, who could now be awakened because he doesn't conflict with
	 * his own locks.
	 */
	if (lockMethodTable->conflictTab[lockmode] & lock->waitMask)
		wakeupNeeded = true;

	/*
	 * Now fix the per-proclock state.
	 */
	proclock->holdMask &= LOCKBIT_OFF(lockmode);
	PROCLOCK_PRINT("UnGrantLock: updated", proclock);

	return wakeupNeeded;
}

/*
 * CleanUpLock -- clean up after releasing a lock.	We garbage-collect the
 * proclock and lock objects if possible, and call ProcLockWakeup if there
 * are remaining requests and the caller says it's OK.  (Normally, this
 * should be called after UnGrantLock, and wakeupNeeded is the result from
 * UnGrantLock.)
 *
 * The appropriate partition lock must be held at entry, and will be
 * held at exit.
 */
static void
CleanUpLock(LOCK *lock, PROCLOCK *proclock,
			LockMethod lockMethodTable, uint32 hashcode,
			bool wakeupNeeded)
{
	/*
	 * If this was my last hold on this lock, delete my entry in the proclock
	 * table.
	 */
	if (proclock->holdMask == 0)
	{
		uint32		proclock_hashcode;

		PROCLOCK_PRINT("CleanUpLock: deleting", proclock);
		SHMQueueDelete(&proclock->lockLink);
		SHMQueueDelete(&proclock->procLink);
		proclock_hashcode = ProcLockHashCode(&proclock->tag, hashcode);
		if (!hash_search_with_hash_value(LockMethodProcLockHash,
										 (void *) &(proclock->tag),
										 proclock_hashcode,
										 HASH_REMOVE,
										 NULL))
			elog(PANIC, "proclock table corrupted");
	}

	if (lock->nRequested == 0)
	{
		/*
		 * The caller just released the last lock, so garbage-collect the lock
		 * object.
		 */
		LOCK_PRINT("CleanUpLock: deleting", lock, 0);
		Assert(SHMQueueEmpty(&(lock->procLocks)));
		if (!hash_search_with_hash_value(LockMethodLockHash,
										 (void *) &(lock->tag),
										 hashcode,
										 HASH_REMOVE,
										 NULL))
			elog(PANIC, "lock table corrupted");
	}
	else if (wakeupNeeded)
	{
		/* There are waiters on this lock, so wake them up. */
		ProcLockWakeup(lockMethodTable, lock);
	}
}

/*
 * GrantLockLocal -- update the locallock data structures to show
 *		the lock request has been granted.
 *
 * We expect that LockAcquire made sure there is room to add a new
 * ResourceOwner entry.
 */
static void
GrantLockLocal(LOCALLOCK *locallock, ResourceOwner owner)
{
	LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
	int			i;

	Assert(locallock->numLockOwners < locallock->maxLockOwners);
	/* Count the total */
	locallock->nLocks++;
	/* Count the per-owner lock */
	for (i = 0; i < locallock->numLockOwners; i++)
	{
		if (lockOwners[i].owner == owner)
		{
			lockOwners[i].nLocks++;
			return;
		}
	}
	lockOwners[i].owner = owner;
	lockOwners[i].nLocks = 1;
	locallock->numLockOwners++;
	if (owner != NULL)
		ResourceOwnerRememberLock(owner, locallock);
}

/*
 * BeginStrongLockAcquire - inhibit use of fastpath for a given LOCALLOCK,
 * and arrange for error cleanup if it fails
 */
static void
BeginStrongLockAcquire(LOCALLOCK *locallock, uint32 fasthashcode)
{
	Assert(StrongLockInProgress == NULL);
	Assert(locallock->holdsStrongLockCount == FALSE);

	/*
	 * Adding to a memory location is not atomic, so we take a spinlock to
	 * ensure we don't collide with someone else trying to bump the count at
	 * the same time.
	 *
	 * XXX: It might be worth considering using an atomic fetch-and-add
	 * instruction here, on architectures where that is supported.
	 */

	SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
	FastPathStrongRelationLocks->count[fasthashcode]++;
	locallock->holdsStrongLockCount = TRUE;
	StrongLockInProgress = locallock;
	SpinLockRelease(&FastPathStrongRelationLocks->mutex);
}

/*
 * FinishStrongLockAcquire - cancel pending cleanup for a strong lock
 * acquisition once it's no longer needed
 */
static void
FinishStrongLockAcquire(void)
{
	StrongLockInProgress = NULL;
}

/*
 * AbortStrongLockAcquire - undo strong lock state changes performed by
 * BeginStrongLockAcquire.
 */
void
AbortStrongLockAcquire(void)
{
	uint32		fasthashcode;
	LOCALLOCK  *locallock = StrongLockInProgress;

	if (locallock == NULL)
		return;

	fasthashcode = FastPathStrongLockHashPartition(locallock->hashcode);
	Assert(locallock->holdsStrongLockCount == TRUE);
	SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
	FastPathStrongRelationLocks->count[fasthashcode]--;
	locallock->holdsStrongLockCount = FALSE;
	StrongLockInProgress = NULL;
	SpinLockRelease(&FastPathStrongRelationLocks->mutex);
}

/*
 * GrantAwaitedLock -- call GrantLockLocal for the lock we are doing
 *		WaitOnLock on.
 *
 * proc.c needs this for the case where we are booted off the lock by
 * timeout, but discover that someone granted us the lock anyway.
 *
 * We could just export GrantLockLocal, but that would require including
 * resowner.h in lock.h, which creates circularity.
 */
void
GrantAwaitedLock(void)
{
	GrantLockLocal(awaitedLock, awaitedOwner);
}

/*
 * WaitOnLock -- wait to acquire a lock
 *
 * Caller must have set MyProc->heldLocks to reflect locks already held
 * on the lockable object by this process.
 *
 * The appropriate partition lock must be held at entry.
 */
static void
WaitOnLock(LOCALLOCK *locallock, ResourceOwner owner)
{
	LOCKMETHODID lockmethodid = LOCALLOCK_LOCKMETHOD(*locallock);
	LockMethod	lockMethodTable = LockMethods[lockmethodid];
	char	   *volatile new_status = NULL;

	LOCK_PRINT("WaitOnLock: sleeping on lock",
			   locallock->lock, locallock->tag.mode);

	/* Report change to waiting status */
	if (update_process_title)
	{
		const char *old_status;
		int			len;

		old_status = get_real_act_ps_display(&len);
		new_status = (char *) palloc(len + 8 + 1);
		memcpy(new_status, old_status, len);
		strcpy(new_status + len, " waiting");
		set_ps_display(new_status, false);
		new_status[len] = '\0'; /* truncate off " waiting" */
	}
	gpstat_report_waiting(PGBE_WAITING_LOCK);

	awaitedLock = locallock;
	awaitedOwner = owner;

	/*
	 * NOTE: Think not to put any shared-state cleanup after the call to
	 * ProcSleep, in either the normal or failure path.  The lock state must
	 * be fully set by the lock grantor, or by CheckDeadLock if we give up
	 * waiting for the lock.  This is necessary because of the possibility
	 * that a cancel/die interrupt will interrupt ProcSleep after someone else
	 * grants us the lock, but before we've noticed it. Hence, after granting,
	 * the locktable state must fully reflect the fact that we own the lock;
	 * we can't do additional work on return.
	 *
	 * We can and do use a PG_TRY block to try to clean up after failure, but
	 * this still has a major limitation: elog(FATAL) can occur while waiting
	 * (eg, a "die" interrupt), and then control won't come back here. So all
	 * cleanup of essential state should happen in LockErrorCleanup, not here.
	 * We can use PG_TRY to clear the "waiting" status flags, since doing that
	 * is unimportant if the process exits.
	 */
	PG_TRY();
	{
		if (ProcSleep(locallock, lockMethodTable) != STATUS_OK)
		{
			/*
			 * We failed as a result of a deadlock, see CheckDeadLock(). Quit
			 * now.
			 */
			awaitedLock = NULL;
			LOCK_PRINT("WaitOnLock: aborting on lock",
					   locallock->lock, locallock->tag.mode);
			LWLockRelease(LockHashPartitionLock(locallock->hashcode));

			/*
			 * Now that we aren't holding the partition lock, we can give an
			 * error report including details about the detected deadlock.
			 */
			DeadLockReport();
			/* not reached */
		}
	}
	PG_CATCH();
	{
		/* In this path, awaitedLock remains set until LockErrorCleanup */

		/* Report change to non-waiting status */
		gpstat_report_waiting(PGBE_WAITING_NONE);
		if (update_process_title)
		{
			set_ps_display(new_status, false);
			pfree(new_status);
		}

		/* and propagate the error */
		PG_RE_THROW();
	}
	PG_END_TRY();

	awaitedLock = NULL;

	/* Report change to non-waiting status */
	gpstat_report_waiting(PGBE_WAITING_NONE);
	if (update_process_title)
	{
		set_ps_display(new_status, false);
		pfree(new_status);
	}

	LOCK_PRINT("WaitOnLock: wakeup on lock",
			   locallock->lock, locallock->tag.mode);
}

/*
 * Remove a proc from the wait-queue it is on (caller must know it is on one).
 * This is only used when the proc has failed to get the lock, so we set its
 * waitStatus to STATUS_ERROR.
 *
 * Appropriate partition lock must be held by caller.  Also, caller is
 * responsible for signaling the proc if needed.
 *
 * NB: this does not clean up any locallock object that may exist for the lock.
 */
void
RemoveFromWaitQueue(PGPROC *proc, uint32 hashcode)
{
	LOCK	   *waitLock = proc->waitLock;
	PROCLOCK   *proclock = proc->waitProcLock;
	LOCKMODE	lockmode = proc->waitLockMode;
	LOCKMETHODID lockmethodid = LOCK_LOCKMETHOD(*waitLock);

	/* Make lockmethod is appropriate. */
	Assert(lockmethodid != RESOURCE_LOCKMETHOD);
	/* Make sure proc is waiting */
	Assert(proc->waitStatus == STATUS_WAITING);
	Assert(proc->links.next != NULL);
	Assert(waitLock);
	Assert(waitLock->waitProcs.size > 0);
	Assert(0 < lockmethodid && lockmethodid < lengthof(LockMethods));

	/* Remove proc from lock's wait queue */
	SHMQueueDelete(&(proc->links));
	waitLock->waitProcs.size--;

	/* Undo increments of request counts by waiting process */
	Assert(waitLock->nRequested > 0);
	Assert(waitLock->nRequested > proc->waitLock->nGranted);
	waitLock->nRequested--;
	Assert(waitLock->requested[lockmode] > 0);
	waitLock->requested[lockmode]--;
	/* don't forget to clear waitMask bit if appropriate */
	if (waitLock->granted[lockmode] == waitLock->requested[lockmode])
		waitLock->waitMask &= LOCKBIT_OFF(lockmode);

	/* Clean up the proc's own state, and pass it the ok/fail signal */
	proc->waitLock = NULL;
	proc->waitProcLock = NULL;
	proc->waitStatus = STATUS_ERROR;

	/*
	 * Delete the proclock immediately if it represents no already-held locks.
	 * (This must happen now because if the owner of the lock decides to
	 * release it, and the requested/granted counts then go to zero,
	 * LockRelease expects there to be no remaining proclocks.) Then see if
	 * any other waiters for the lock can be woken up now.
	 */
	CleanUpLock(waitLock, proclock,
				LockMethods[lockmethodid], hashcode,
				true);
}

/*
 * LockRelease -- look up 'locktag' and release one 'lockmode' lock on it.
 *		Release a session lock if 'sessionLock' is true, else release a
 *		regular transaction lock.
 *
 * Side Effects: find any waiting processes that are now wakable,
 *		grant them their requested locks and awaken them.
 *		(We have to grant the lock here to avoid a race between
 *		the waking process and any new process to
 *		come along and request the lock.)
 */
bool
LockRelease(const LOCKTAG *locktag, LOCKMODE lockmode, bool sessionLock)
{
	LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
	LockMethod	lockMethodTable;
	LOCALLOCKTAG localtag;
	LOCALLOCK  *locallock;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	LWLockId	partitionLock;
	bool		wakeupNeeded;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];
	if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
		elog(ERROR, "unrecognized lock mode: %d", lockmode);

#ifdef LOCK_DEBUG
	if (LOCK_DEBUG_ENABLED(locktag))
		elog(LOG, "LockRelease: lock [%u,%u] %s",
			 locktag->locktag_field1, locktag->locktag_field2,
			 lockMethodTable->lockModeNames[lockmode]);
#endif

	/*
	 * Find the LOCALLOCK entry for this lock and lockmode
	 */
	MemSet(&localtag, 0, sizeof(localtag));		/* must clear padding */
	localtag.lock = *locktag;
	localtag.mode = lockmode;

	locallock = (LOCALLOCK *) hash_search(LockMethodLocalHash,
										  (void *) &localtag,
										  HASH_FIND, NULL);

	/*
	 * let the caller print its own error message, too. Do not ereport(ERROR).
	 */
	if (!locallock || locallock->nLocks <= 0)
	{
		elog(WARNING, "you don't own a lock of type %s",
			 lockMethodTable->lockModeNames[lockmode]);
		return FALSE;
	}

	/*
	 * Decrease the count for the resource owner.
	 */
	{
		LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
		ResourceOwner owner;
		int			i;

		/* Identify owner for lock */
		if (sessionLock)
			owner = NULL;
		else
			owner = CurrentResourceOwner;

		for (i = locallock->numLockOwners - 1; i >= 0; i--)
		{
			if (lockOwners[i].owner == owner)
			{
				Assert(lockOwners[i].nLocks > 0);
				if (--lockOwners[i].nLocks == 0)
				{
					if (owner != NULL)
						ResourceOwnerForgetLock(owner, locallock);
					/* compact out unused slot */
					locallock->numLockOwners--;
					if (i < locallock->numLockOwners)
						lockOwners[i] = lockOwners[locallock->numLockOwners];
				}
				break;
			}
		}
		if (i < 0)
		{
			/* don't release a lock belonging to another owner */
			elog(WARNING, "you don't own a lock of type %s",
				 lockMethodTable->lockModeNames[lockmode]);
			return FALSE;
		}
	}

	/*
	 * Decrease the total local count.	If we're still holding the lock, we're
	 * done.
	 */
	locallock->nLocks--;

	if (locallock->nLocks > 0)
		return TRUE;

	/* Attempt fast release of any lock eligible for the fast path. */
	if (EligibleForRelationFastPath(locktag, lockmode)
		&& FastPathLocalUseCount > 0)
	{
		bool		released;

		/*
		 * We might not find the lock here, even if we originally entered it
		 * here.  Another backend may have moved it to the main table.
		 */
		LWLockAcquire(MyProc->backendLock, LW_EXCLUSIVE);
		released = FastPathUnGrantRelationLock(locktag->locktag_field2,
											   lockmode);
		LWLockRelease(MyProc->backendLock);
		if (released)
		{
			RemoveLocalLock(locallock);
			return TRUE;
		}
	}

	/*
	 * Otherwise we've got to mess with the shared lock table.
	 */
	partitionLock = LockHashPartitionLock(locallock->hashcode);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * Normally, we don't need to re-find the lock or proclock, since we kept
	 * their addresses in the locallock table, and they couldn't have been
	 * removed while we were holding a lock on them.  But it's possible that
	 * the locks have been moved to the main hash table by another backend, in
	 * which case we might need to go look them up after all.
	 */
	lock = locallock->lock;
	if (!lock)
	{
		PROCLOCKTAG proclocktag;
		bool		found;

		Assert(EligibleForRelationFastPath(locktag, lockmode));
		lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
													(const void *) locktag,
													locallock->hashcode,
													HASH_FIND,
													&found);
		Assert(found && lock != NULL);
		locallock->lock = lock;

		proclocktag.myLock = lock;
		proclocktag.myProc = MyProc;
		locallock->proclock = (PROCLOCK *) hash_search(LockMethodProcLockHash,
													   (void *) &proclocktag,
													   HASH_FIND, &found);
		Assert(found);
	}
	LOCK_PRINT("LockRelease: found", lock, lockmode);
	proclock = locallock->proclock;
	PROCLOCK_PRINT("LockRelease: found", proclock);

	/*
	 * Double-check that we are actually holding a lock of the type we want to
	 * release.
	 */
	if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
	{
		PROCLOCK_PRINT("LockRelease: WRONGTYPE", proclock);
		LWLockRelease(partitionLock);
		elog(WARNING, "you don't own a lock of type %s",
			 lockMethodTable->lockModeNames[lockmode]);
		RemoveLocalLock(locallock);
		return FALSE;
	}

	/*
	 * Do the releasing.  CleanUpLock will waken any now-wakable waiters.
	 */
	wakeupNeeded = UnGrantLock(lock, lockmode, proclock, lockMethodTable);

	CleanUpLock(lock, proclock,
				lockMethodTable, locallock->hashcode,
				wakeupNeeded);

	LWLockRelease(partitionLock);

	RemoveLocalLock(locallock);
	return TRUE;
}

void
LockSetHoldTillEndXact(const LOCKTAG *locktag)
{
	LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
	LockMethod	lockMethodTable;
	LOCALLOCKTAG localtag;
	LOCALLOCK  *locallock;
	LOCKMODE    lm;
	Oid         relid;

	/*
	 * A relation lock would exist either in fast-pach or in shared lock
	 * table. So we could return immediately if we have found it in fast-path.
	 */
	relid = locktag->locktag_field2;
	if (setFPHoldTillEndXact(relid))
		return;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];

	for (lm = 1; lm <= lockMethodTable->numLockModes; lm++)
	{
#ifdef LOCK_DEBUG
		if (LOCK_DEBUG_ENABLED(locktag))
			elog(LOG, "LockRelease: lock [%u,%u] %s",
				 locktag->locktag_field1, locktag->locktag_field2,
				 lockMethodTable->lockModeNames[lm]);
#endif
		/*
		 * Find the LOCALLOCK entry for this lock and lockmode
		 */
		MemSet(&localtag, 0, sizeof(localtag));		/* must clear padding */
		localtag.lock = *locktag;
		localtag.mode = lm;

		locallock = (LOCALLOCK *) hash_search(LockMethodLocalHash,
											  (void *) &localtag,
											  HASH_FIND, NULL);

		if (!locallock || locallock->nLocks <= 0)
			continue;

		if (locallock->lock)
			locallock->lock->holdTillEndXact = true;
	}
}

/*
 * LockReleaseAll -- Release all locks of the specified lock method that
 *		are held by the current process.
 *
 * Well, not necessarily *all* locks.  The available behaviors are:
 *		allLocks == true: release all locks including session locks.
 *		allLocks == false: release all non-session locks.
 */
void
LockReleaseAll(LOCKMETHODID lockmethodid, bool allLocks)
{
	HASH_SEQ_STATUS status;
	LockMethod	lockMethodTable;
	int			i,
				numLockModes;
	LOCALLOCK  *locallock;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	int			partition;
	bool		have_fast_path_lwlock = false;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];

#ifdef LOCK_DEBUG
	if (*(lockMethodTable->trace_flag))
		elog(LOG, "LockReleaseAll: lockmethod=%d", lockmethodid);
#endif

	/*
	 * Get rid of our fast-path VXID lock, if appropriate.	Note that this is
	 * the only way that the lock we hold on our own VXID can ever get
	 * released: it is always and only released when a toplevel transaction
	 * ends.
	 */
	if (lockmethodid == DEFAULT_LOCKMETHOD)
		VirtualXactLockTableCleanup();

	numLockModes = lockMethodTable->numLockModes;

	/*
	 * First we run through the locallock table and get rid of unwanted
	 * entries, then we scan the process's proclocks and get rid of those. We
	 * do this separately because we may have multiple locallock entries
	 * pointing to the same proclock, and we daren't end up with any dangling
	 * pointers.
	 */
	hash_seq_init(&status, LockMethodLocalHash);

	while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
	{
		/*
		 * If the LOCALLOCK entry is unused, we must've run out of shared
		 * memory while trying to set up this lock.  Just forget the local
		 * entry.
		 */
		if (locallock->nLocks == 0)
		{
			RemoveLocalLock(locallock);
			continue;
		}

		/* Ignore items that are not of the lockmethod to be removed */
		if (LOCALLOCK_LOCKMETHOD(*locallock) != lockmethodid)
			continue;

		/*
		 * If we are asked to release all locks, we can just zap the entry.
		 * Otherwise, must scan to see if there are session locks. We assume
		 * there is at most one lockOwners entry for session locks.
		 */
		if (!allLocks)
		{
			LOCALLOCKOWNER *lockOwners = locallock->lockOwners;

			/* If session lock is above array position 0, move it down to 0 */
			for (i = 0; i < locallock->numLockOwners; i++)
			{
				if (lockOwners[i].owner == NULL)
					lockOwners[0] = lockOwners[i];
				else
					ResourceOwnerForgetLock(lockOwners[i].owner, locallock);
			}

			if (locallock->numLockOwners > 0 &&
				lockOwners[0].owner == NULL &&
				lockOwners[0].nLocks > 0)
			{
				/* Fix the locallock to show just the session locks */
				locallock->nLocks = lockOwners[0].nLocks;
				locallock->numLockOwners = 1;
				/* We aren't deleting this locallock, so done */
				continue;
			}
			else
				locallock->numLockOwners = 0;
		}

		/*
		 * If the lock or proclock pointers are NULL, this lock was taken via
		 * the relation fast-path.
		 */
		if (locallock->proclock == NULL || locallock->lock == NULL)
		{
			LOCKMODE	lockmode = locallock->tag.mode;
			Oid			relid;

			/* Verify that a fast-path lock is what we've got. */
			if (!EligibleForRelationFastPath(&locallock->tag.lock, lockmode))
				elog(PANIC, "locallock table corrupted");

			/*
			 * If we don't currently hold the LWLock that protects our
			 * fast-path data structures, we must acquire it before attempting
			 * to release the lock via the fast-path.
			 */
			if (!have_fast_path_lwlock)
			{
				LWLockAcquire(MyProc->backendLock, LW_EXCLUSIVE);
				have_fast_path_lwlock = true;
			}

			/* Attempt fast-path release. */
			relid = locallock->tag.lock.locktag_field2;
			if (FastPathUnGrantRelationLock(relid, lockmode))
			{
				RemoveLocalLock(locallock);
				continue;
			}

			/*
			 * Our lock, originally taken via the fast path, has been
			 * transferred to the main lock table.	That's going to require
			 * some extra work, so release our fast-path lock before starting.
			 */
			LWLockRelease(MyProc->backendLock);
			have_fast_path_lwlock = false;

			/*
			 * Now dump the lock.  We haven't got a pointer to the LOCK or
			 * PROCLOCK in this case, so we have to handle this a bit
			 * differently than a normal lock release.	Unfortunately, this
			 * requires an extra LWLock acquire-and-release cycle on the
			 * partitionLock, but hopefully it shouldn't happen often.
			 */
			LockRefindAndRelease(lockMethodTable, MyProc,
								 &locallock->tag.lock, lockmode, false);
			RemoveLocalLock(locallock);
			continue;
		}

		/* Mark the proclock to show we need to release this lockmode */
		if (locallock->nLocks > 0)
			locallock->proclock->releaseMask |= LOCKBIT_ON(locallock->tag.mode);

		/* And remove the locallock hashtable entry */
		RemoveLocalLock(locallock);
	}

	if (have_fast_path_lwlock)
		LWLockRelease(MyProc->backendLock);

	/*
	 * Now, scan each lock partition separately.
	 */
	for (partition = 0; partition < NUM_LOCK_PARTITIONS; partition++)
	{
		LWLockId	partitionLock = FirstLockMgrLock + partition;
		SHM_QUEUE  *procLocks = &(MyProc->myProcLocks[partition]);

		proclock = (PROCLOCK *) SHMQueueNext(procLocks, procLocks,
											 offsetof(PROCLOCK, procLink));

		if (!proclock)
			continue;			/* needn't examine this partition */

		LWLockAcquire(partitionLock, LW_EXCLUSIVE);

		while (proclock)
		{
			bool		wakeupNeeded = false;
			PROCLOCK   *nextplock;

			/* Get link first, since we may unlink/delete this proclock */
			nextplock = (PROCLOCK *)
				SHMQueueNext(procLocks, &proclock->procLink,
							 offsetof(PROCLOCK, procLink));

			Assert(proclock->tag.myProc == MyProc);

			lock = proclock->tag.myLock;

			/* Ignore items that are not of the lockmethod to be removed */
			if (LOCK_LOCKMETHOD(*lock) != lockmethodid)
				goto next_item;

			/*
			 * In allLocks mode, force release of all locks even if locallock
			 * table had problems
			 */
			if (allLocks)
				proclock->releaseMask = proclock->holdMask;
			else
				Assert((proclock->releaseMask & ~proclock->holdMask) == 0);

			/*
			 * Ignore items that have nothing to be released, unless they have
			 * holdMask == 0 and are therefore recyclable
			 */
			if (proclock->releaseMask == 0 && proclock->holdMask != 0)
				goto next_item;

			PROCLOCK_PRINT("LockReleaseAll", proclock);
			LOCK_PRINT("LockReleaseAll", lock, 0);
			Assert(lock->nRequested >= 0);
			Assert(lock->nGranted >= 0);
			Assert(lock->nGranted <= lock->nRequested);
			Assert((proclock->holdMask & ~lock->grantMask) == 0);

			/*
			 * Release the previously-marked lock modes
			 */
			for (i = 1; i <= numLockModes; i++)
			{
				if (proclock->releaseMask & LOCKBIT_ON(i))
					wakeupNeeded |= UnGrantLock(lock, i, proclock,
												lockMethodTable);
			}
			Assert((lock->nRequested >= 0) && (lock->nGranted >= 0));
			Assert(lock->nGranted <= lock->nRequested);
			LOCK_PRINT("LockReleaseAll: updated", lock, 0);

			proclock->releaseMask = 0;

			/* CleanUpLock will wake up waiters if needed. */
			CleanUpLock(lock, proclock,
						lockMethodTable,
						LockTagHashCode(&lock->tag),
						wakeupNeeded);

	next_item:
			proclock = nextplock;
		}						/* loop over PROCLOCKs within this partition */

		LWLockRelease(partitionLock);
	}							/* loop over partitions */

#ifdef LOCK_DEBUG
	if (*(lockMethodTable->trace_flag))
		elog(LOG, "LockReleaseAll done");
#endif
}

/*
 * LockReleaseSession -- Release all session locks of the specified lock method
 *		that are held by the current process.
 */
void
LockReleaseSession(LOCKMETHODID lockmethodid)
{
	HASH_SEQ_STATUS status;
	LOCALLOCK  *locallock;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);

	hash_seq_init(&status, LockMethodLocalHash);

	while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
	{
		/* Ignore items that are not of the specified lock method */
		if (LOCALLOCK_LOCKMETHOD(*locallock) != lockmethodid)
			continue;

		ReleaseLockIfHeld(locallock, true);
	}
}

/*
 * LockReleaseCurrentOwner
 *		Release all locks belonging to CurrentResourceOwner
 *
 * If the caller knows what those locks are, it can pass them as an array.
 * That speeds up the call significantly, when a lot of locks are held.
 * Otherwise, pass NULL for locallocks, and we'll traverse through our hash
 * table to find them.
 */
void
LockReleaseCurrentOwner(LOCALLOCK **locallocks, int nlocks)
{
	if (locallocks == NULL)
	{
		HASH_SEQ_STATUS status;
		LOCALLOCK  *locallock;

		hash_seq_init(&status, LockMethodLocalHash);

		while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
			ReleaseLockIfHeld(locallock, false);
	}
	else
	{
		int                     i;

		for (i = nlocks - 1; i >= 0; i--)
			ReleaseLockIfHeld(locallocks[i], false);
	}
}

/*
 * ReleaseLockIfHeld
 *		Release any session-level locks on this lockable object if sessionLock
 *		is true; else, release any locks held by CurrentResourceOwner.
 *
 * It is tempting to pass this a ResourceOwner pointer (or NULL for session
 * locks), but without refactoring LockRelease() we cannot support releasing
 * locks belonging to resource owners other than CurrentResourceOwner.
 * If we were to refactor, it'd be a good idea to fix it so we don't have to
 * do a hashtable lookup of the locallock, too.  However, currently this
 * function isn't used heavily enough to justify refactoring for its
 * convenience.
 */
static void
ReleaseLockIfHeld(LOCALLOCK *locallock, bool sessionLock)
{
	ResourceOwner owner;
	LOCALLOCKOWNER *lockOwners;
	int			i;

	/* Identify owner for lock (must match LockRelease!) */
	if (sessionLock)
		owner = NULL;
	else
		owner = CurrentResourceOwner;

	/* Scan to see if there are any locks belonging to the target owner */
	lockOwners = locallock->lockOwners;
	for (i = locallock->numLockOwners - 1; i >= 0; i--)
	{
		if (lockOwners[i].owner == owner)
		{
			Assert(lockOwners[i].nLocks > 0);
			if (lockOwners[i].nLocks < locallock->nLocks)
			{
				/*
				 * We will still hold this lock after forgetting this
				 * ResourceOwner.
				 */
				locallock->nLocks -= lockOwners[i].nLocks;
				/* compact out unused slot */
				locallock->numLockOwners--;
				if (owner != NULL)
					ResourceOwnerForgetLock(owner, locallock);
				if (i < locallock->numLockOwners)
					lockOwners[i] = lockOwners[locallock->numLockOwners];
			}
			else
			{
				Assert(lockOwners[i].nLocks == locallock->nLocks);
				/* We want to call LockRelease just once */
				lockOwners[i].nLocks = 1;
				locallock->nLocks = 1;
				if (!LockRelease(&locallock->tag.lock,
								 locallock->tag.mode,
								 sessionLock))
					elog(WARNING, "ReleaseLockIfHeld: failed??");
			}
			break;
		}
	}
}

/*
 * LockReassignCurrentOwner
 *		Reassign all locks belonging to CurrentResourceOwner to belong
 *		to its parent resource owner.
 *
 * If the caller knows what those locks are, it can pass them as an array.
 * That speeds up the call significantly, when a lot of locks are held
 * (e.g pg_dump with a large schema).  Otherwise, pass NULL for locallocks,
 * and we'll traverse through our hash table to find them.
 */
void
LockReassignCurrentOwner(LOCALLOCK **locallocks, int nlocks)
{
	ResourceOwner parent = ResourceOwnerGetParent(CurrentResourceOwner);

	Assert(parent != NULL);

	if (locallocks == NULL)
	{
		HASH_SEQ_STATUS status;
		LOCALLOCK  *locallock;

		hash_seq_init(&status, LockMethodLocalHash);

		while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
			LockReassignOwner(locallock, parent);
	}
	else
	{
		int			i;

		for (i = nlocks - 1; i >= 0; i--)
			LockReassignOwner(locallocks[i], parent);
	}
}

/*
 * Subroutine of LockReassignCurrentOwner. Reassigns a given lock belonging to
 * CurrentResourceOwner to its parent.
 */
static void
LockReassignOwner(LOCALLOCK *locallock, ResourceOwner parent)
{
	LOCALLOCKOWNER *lockOwners;
	int			i;
	int			ic = -1;
	int			ip = -1;

	/*
	 * Scan to see if there are any locks belonging to current owner or its
	 * parent
	 */
	lockOwners = locallock->lockOwners;
	for (i = locallock->numLockOwners - 1; i >= 0; i--)
	{
		if (lockOwners[i].owner == CurrentResourceOwner)
			ic = i;
		else if (lockOwners[i].owner == parent)
			ip = i;
	}

	if (ic < 0)
		return;					/* no current locks */

	if (ip < 0)
	{
		/* Parent has no slot, so just give it the child's slot */
		lockOwners[ic].owner = parent;
		ResourceOwnerRememberLock(parent, locallock);
	}
	else
	{
		/* Merge child's count with parent's */
		lockOwners[ip].nLocks += lockOwners[ic].nLocks;
		/* compact out unused slot */
		locallock->numLockOwners--;
		if (ic < locallock->numLockOwners)
			lockOwners[ic] = lockOwners[locallock->numLockOwners];
	}
	ResourceOwnerForgetLock(CurrentResourceOwner, locallock);
}

/*
 * FastPathGrantRelationLock
 *		Grant lock using per-backend fast-path array, if there is space.
 */
static bool
FastPathGrantRelationLock(Oid relid, LOCKMODE lockmode)
{
	uint32		f;
	uint32		unused_slot = FP_LOCK_SLOTS_PER_BACKEND;

	/* Scan for existing entry for this relid, remembering empty slot. */
	for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
	{
		if (FAST_PATH_GET_BITS(MyProc, f) == 0)
			unused_slot = f;
		else if (MyProc->fpRelId[f] == relid)
		{
			Assert(!FAST_PATH_CHECK_LOCKMODE(MyProc, f, lockmode));
			FAST_PATH_SET_LOCKMODE(MyProc, f, lockmode);
			return true;
		}
	}

	/* If no existing entry, use any empty slot. */
	if (unused_slot < FP_LOCK_SLOTS_PER_BACKEND)
	{
		MyProc->fpRelId[unused_slot] = relid;
		FAST_PATH_SET_HOLD_TILL_END_XACT(MyProc, unused_slot, 0);
		FAST_PATH_SET_LOCKMODE(MyProc, unused_slot, lockmode);
		++FastPathLocalUseCount;
		return true;
	}

	/* No existing entry, and no empty slot. */
	return false;
}

/*
 * FastPathUnGrantRelationLock
 *		Release fast-path lock, if present.  Update backend-private local
 *		use count, while we're at it.
 */
static bool
FastPathUnGrantRelationLock(Oid relid, LOCKMODE lockmode)
{
	uint32		f;
	bool		result = false;

	FastPathLocalUseCount = 0;
	for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
	{
		if (MyProc->fpRelId[f] == relid
			&& FAST_PATH_CHECK_LOCKMODE(MyProc, f, lockmode))
		{
			Assert(!result);
			FAST_PATH_CLEAR_LOCKMODE(MyProc, f, lockmode);
			result = true;
		}
		if (FAST_PATH_GET_BITS(MyProc, f) != 0)
			++FastPathLocalUseCount;
	}
	return result;
}

/*
 * FastPathTransferRelationLocks
 *		Transfer locks matching the given lock tag from per-backend fast-path
 *		arrays to the shared hash table.
 */
static bool
FastPathTransferRelationLocks(LockMethod lockMethodTable, const LOCKTAG *locktag,
							  uint32 hashcode)
{
	LWLockId	partitionLock = LockHashPartitionLock(hashcode);
	Oid			relid = locktag->locktag_field2;
	uint32		i;

	/*
	 * Every PGPROC that can potentially hold a fast-path lock is present in
	 * ProcGlobal->allProcs.  Prepared transactions are not, but any
	 * outstanding fast-path locks held by prepared transactions are
	 * transferred to the main lock table.
	 */
	for (i = 0; i < ProcGlobal->allProcCount; i++)
	{
		PGPROC	   *proc = &ProcGlobal->allProcs[i];
		uint32		f;

		LWLockAcquire(proc->backendLock, LW_EXCLUSIVE);

		/*
		 * If the target backend isn't referencing the same database as we
		 * are, then we needn't examine the individual relation IDs at all;
		 * none of them can be relevant.
		 *
		 * proc->databaseId is set at backend startup time and never changes
		 * thereafter, so it might be safe to perform this test before
		 * acquiring proc->backendLock.  In particular, it's certainly safe to
		 * assume that if the target backend holds any fast-path locks, it
		 * must have performed a memory-fencing operation (in particular, an
		 * LWLock acquisition) since setting proc->databaseId.	However, it's
		 * less clear that our backend is certain to have performed a memory
		 * fencing operation since the other backend set proc->databaseId.	So
		 * for now, we test it after acquiring the LWLock just to be safe.
		 */
		if (proc->databaseId != MyDatabaseId)
		{
			LWLockRelease(proc->backendLock);
			continue;
		}

		for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
		{
			uint32		lockmode;

			/* Look for an allocated slot matching the given relid. */
			if (relid != proc->fpRelId[f] || FAST_PATH_GET_BITS(proc, f) == 0)
				continue;

			/* Find or create lock object. */
			LWLockAcquire(partitionLock, LW_EXCLUSIVE);
			for (lockmode = FAST_PATH_LOCKNUMBER_OFFSET;
			lockmode < FAST_PATH_LOCKNUMBER_OFFSET + FAST_PATH_BITS_PER_SLOT;
				 ++lockmode)
			{
				PROCLOCK   *proclock;

				if (!FAST_PATH_CHECK_LOCKMODE(proc, f, lockmode))
					continue;
				proclock = SetupLockInTable(lockMethodTable, proc, locktag,
											hashcode, lockmode);
				if (!proclock)
				{
					LWLockRelease(partitionLock);
					return false;
				}
				/* Set holdTillEndXact of proclock */
				proclock->tag.myLock->holdTillEndXact = \
					FAST_PATH_GET_HOLD_TILL_END_XACT_BITS(proc, f) > 0;
				GrantLock(proclock->tag.myLock, proclock, lockmode);
				FAST_PATH_CLEAR_LOCKMODE(proc, f, lockmode);
			}
			LWLockRelease(partitionLock);
		}
		LWLockRelease(proc->backendLock);
	}
	return true;
}

/*
 * FastPathGetLockEntry
 *		Return the PROCLOCK for a lock originally taken via the fast-path,
 *		transferring it to the primary lock table if necessary.
 */
static PROCLOCK *
FastPathGetRelationLockEntry(LOCALLOCK *locallock)
{
	LockMethod	lockMethodTable = LockMethods[DEFAULT_LOCKMETHOD];
	LOCKTAG    *locktag = &locallock->tag.lock;
	PROCLOCK   *proclock = NULL;
	LWLockId	partitionLock = LockHashPartitionLock(locallock->hashcode);
	Oid			relid = locktag->locktag_field2;
	uint32		f;

	LWLockAcquire(MyProc->backendLock, LW_EXCLUSIVE);

	for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
	{
		uint32		lockmode;

		/* Look for an allocated slot matching the given relid. */
		if (relid != MyProc->fpRelId[f] || FAST_PATH_GET_BITS(MyProc, f) == 0)
			continue;

		/* If we don't have a lock of the given mode, forget it! */
		lockmode = locallock->tag.mode;
		if (!FAST_PATH_CHECK_LOCKMODE(MyProc, f, lockmode))
			break;

		/* Find or create lock object. */
		LWLockAcquire(partitionLock, LW_EXCLUSIVE);

		proclock = SetupLockInTable(lockMethodTable, MyProc, locktag,
									locallock->hashcode, lockmode);
		if (!proclock)
		{
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of shared memory"),
					 errhint("You might need to increase max_locks_per_transaction.")));
		}
		GrantLock(proclock->tag.myLock, proclock, lockmode);
		FAST_PATH_CLEAR_LOCKMODE(MyProc, f, lockmode);

		LWLockRelease(partitionLock);
	}

	LWLockRelease(MyProc->backendLock);

	/* Lock may have already been transferred by some other backend. */
	if (proclock == NULL)
	{
		LOCK	   *lock;
		PROCLOCKTAG proclocktag;
		uint32		proclock_hashcode;

		LWLockAcquire(partitionLock, LW_SHARED);

		lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
													(void *) locktag,
													locallock->hashcode,
													HASH_FIND,
													NULL);
		if (!lock)
			elog(ERROR, "failed to re-find shared lock object");

		proclocktag.myLock = lock;
		proclocktag.myProc = MyProc;

		proclock_hashcode = ProcLockHashCode(&proclocktag, locallock->hashcode);
		proclock = (PROCLOCK *)
			hash_search_with_hash_value(LockMethodProcLockHash,
										(void *) &proclocktag,
										proclock_hashcode,
										HASH_FIND,
										NULL);
		if (!proclock)
			elog(ERROR, "failed to re-find shared proclock object");
		LWLockRelease(partitionLock);
	}

	return proclock;
}

/*
 * GetLockConflicts
 *		Get an array of VirtualTransactionIds of xacts currently holding locks
 *		that would conflict with the specified lock/lockmode.
 *		xacts merely awaiting such a lock are NOT reported.
 *
 * The result array is palloc'd and is terminated with an invalid VXID.
 *
 * Of course, the result could be out of date by the time it's returned,
 * so use of this function has to be thought about carefully.
 *
 * Note we never include the current xact's vxid in the result array,
 * since an xact never blocks itself.  Also, prepared transactions are
 * ignored, which is a bit more debatable but is appropriate for current
 * uses of the result.
 */
VirtualTransactionId *
GetLockConflicts(const LOCKTAG *locktag, LOCKMODE lockmode)
{
	static VirtualTransactionId *vxids;
	LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
	LockMethod	lockMethodTable;
	LOCK	   *lock;
	LOCKMASK	conflictMask;
	SHM_QUEUE  *procLocks;
	PROCLOCK   *proclock;
	uint32		hashcode;
	LWLockId	partitionLock;
	int			count = 0;
	int			fast_count = 0;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];
	if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
		elog(ERROR, "unrecognized lock mode: %d", lockmode);

	/*
	 * Allocate memory to store results, and fill with InvalidVXID.  We only
	 * need enough space for MaxBackends + a terminator, since prepared xacts
	 * don't count. InHotStandby allocate once in TopMemoryContext.
	 */
	if (InHotStandby)
	{
		if (vxids == NULL)
			vxids = (VirtualTransactionId *)
				MemoryContextAlloc(TopMemoryContext,
						   sizeof(VirtualTransactionId) * (MaxBackends + 1));
	}
	else
		vxids = (VirtualTransactionId *)
			palloc0(sizeof(VirtualTransactionId) * (MaxBackends + 1));

	/* Compute hash code and partiton lock, and look up conflicting modes. */
	hashcode = LockTagHashCode(locktag);
	partitionLock = LockHashPartitionLock(hashcode);
	conflictMask = lockMethodTable->conflictTab[lockmode];

	/*
	 * Fast path locks might not have been entered in the primary lock table.
	 * If the lock we're dealing with could conflict with such a lock, we must
	 * examine each backend's fast-path array for conflicts.
	 */
	if (ConflictsWithRelationFastPath(locktag, lockmode))
	{
		int			i;
		Oid			relid = locktag->locktag_field2;
		VirtualTransactionId vxid;

		/*
		 * Iterate over relevant PGPROCs.  Anything held by a prepared
		 * transaction will have been transferred to the primary lock table,
		 * so we need not worry about those.  This is all a bit fuzzy, because
		 * new locks could be taken after we've visited a particular
		 * partition, but the callers had better be prepared to deal with that
		 * anyway, since the locks could equally well be taken between the
		 * time we return the value and the time the caller does something
		 * with it.
		 */
		for (i = 0; i < ProcGlobal->allProcCount; i++)
		{
			PGPROC	   *proc = &ProcGlobal->allProcs[i];
			uint32		f;

			/* A backend never blocks itself */
			if (proc == MyProc)
				continue;

			LWLockAcquire(proc->backendLock, LW_SHARED);

			/*
			 * If the target backend isn't referencing the same database as we
			 * are, then we needn't examine the individual relation IDs at
			 * all; none of them can be relevant.
			 *
			 * See FastPathTransferLocks() for discussion of why we do this
			 * test after acquiring the lock.
			 */
			if (proc->databaseId != MyDatabaseId)
			{
				LWLockRelease(proc->backendLock);
				continue;
			}

			for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
			{
				uint32		lockmask;

				/* Look for an allocated slot matching the given relid. */
				if (relid != proc->fpRelId[f])
					continue;
				lockmask = FAST_PATH_GET_BITS(proc, f);
				if (!lockmask)
					continue;
				lockmask <<= FAST_PATH_LOCKNUMBER_OFFSET;

				/*
				 * There can only be one entry per relation, so if we found it
				 * and it doesn't conflict, we can skip the rest of the slots.
				 */
				if ((lockmask & conflictMask) == 0)
					break;

				/* Conflict! */
				GET_VXID_FROM_PGPROC(vxid, *proc);

				/*
				 * If we see an invalid VXID, then either the xact has already
				 * committed (or aborted), or it's a prepared xact.  In either
				 * case we may ignore it.
				 */
				if (VirtualTransactionIdIsValid(vxid))
					vxids[count++] = vxid;
				break;
			}

			LWLockRelease(proc->backendLock);
		}
	}

	/* Remember how many fast-path conflicts we found. */
	fast_count = count;

	/*
	 * Look up the lock object matching the tag.
	 */
	LWLockAcquire(partitionLock, LW_SHARED);

	lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
												(const void *) locktag,
												hashcode,
												HASH_FIND,
												NULL);
	if (!lock)
	{
		/*
		 * If the lock object doesn't exist, there is nothing holding a lock
		 * on this lockable object.
		 */
		LWLockRelease(partitionLock);
		return vxids;
	}

	/*
	 * Examine each existing holder (or awaiter) of the lock.
	 */

	procLocks = &(lock->procLocks);

	proclock = (PROCLOCK *) SHMQueueNext(procLocks, procLocks,
										 offsetof(PROCLOCK, lockLink));

	while (proclock)
	{
		if (conflictMask & proclock->holdMask)
		{
			PGPROC	   *proc = proclock->tag.myProc;

			/* A backend never blocks itself */
			if (proc != MyProc)
			{
				VirtualTransactionId vxid;

				GET_VXID_FROM_PGPROC(vxid, *proc);

				/*
				 * If we see an invalid VXID, then either the xact has already
				 * committed (or aborted), or it's a prepared xact.  In either
				 * case we may ignore it.
				 */
				if (VirtualTransactionIdIsValid(vxid))
				{
					int			i;

					/* Avoid duplicate entries. */
					for (i = 0; i < fast_count; ++i)
						if (VirtualTransactionIdEquals(vxids[i], vxid))
							break;
					if (i >= fast_count)
						vxids[count++] = vxid;
				}
			}
		}

		proclock = (PROCLOCK *) SHMQueueNext(procLocks, &proclock->lockLink,
											 offsetof(PROCLOCK, lockLink));
	}

	LWLockRelease(partitionLock);

	if (count > MaxBackends)	/* should never happen */
		elog(PANIC, "too many conflicting locks found");

	return vxids;
}

/*
 * Find a lock in the shared lock table and release it.  It is the caller's
 * responsibility to verify that this is a sane thing to do.  (For example, it
 * would be bad to release a lock here if there might still be a LOCALLOCK
 * object with pointers to it.)
 *
 * We currently use this in two situations: first, to release locks held by
 * prepared transactions on commit (see lock_twophase_postcommit); and second,
 * to release locks taken via the fast-path, transferred to the main hash
 * table, and then released (see LockReleaseAll).
 */
static void
LockRefindAndRelease(LockMethod lockMethodTable, PGPROC *proc,
					 LOCKTAG *locktag, LOCKMODE lockmode,
					 bool decrement_strong_lock_count)
{
	LOCK	   *lock;
	PROCLOCK   *proclock;
	PROCLOCKTAG proclocktag;
	uint32		hashcode;
	uint32		proclock_hashcode;
	LWLockId	partitionLock;
	bool		wakeupNeeded;

	hashcode = LockTagHashCode(locktag);
	partitionLock = LockHashPartitionLock(hashcode);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * Re-find the lock object (it had better be there).
	 */
	lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
												(void *) locktag,
												hashcode,
												HASH_FIND,
												NULL);
	if (!lock)
		elog(PANIC, "failed to re-find shared lock object");

	/*
	 * Re-find the proclock object (ditto).
	 */
	proclocktag.myLock = lock;
	proclocktag.myProc = proc;

	proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

	proclock = (PROCLOCK *) hash_search_with_hash_value(LockMethodProcLockHash,
														(void *) &proclocktag,
														proclock_hashcode,
														HASH_FIND,
														NULL);
	if (!proclock)
		elog(PANIC, "failed to re-find shared proclock object");

	/*
	 * Double-check that we are actually holding a lock of the type we want to
	 * release.
	 */
	if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
	{
		PROCLOCK_PRINT("lock_twophase_postcommit: WRONGTYPE", proclock);
		LWLockRelease(partitionLock);
		elog(WARNING, "you don't own a lock of type %s",
			 lockMethodTable->lockModeNames[lockmode]);
		return;
	}

	/*
	 * Do the releasing.  CleanUpLock will waken any now-wakable waiters.
	 */
	wakeupNeeded = UnGrantLock(lock, lockmode, proclock, lockMethodTable);

	CleanUpLock(lock, proclock,
				lockMethodTable, hashcode,
				wakeupNeeded);

	LWLockRelease(partitionLock);

	/*
	 * Decrement strong lock count.  This logic is needed only for 2PC.
	 */
	if (decrement_strong_lock_count
		&& ConflictsWithRelationFastPath(locktag, lockmode))
	{
		uint32		fasthashcode = FastPathStrongLockHashPartition(hashcode);

		SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
		FastPathStrongRelationLocks->count[fasthashcode]--;
		SpinLockRelease(&FastPathStrongRelationLocks->mutex);
	}
}

/*
 * AtPrepare_Locks
 *		Do the preparatory work for a PREPARE: make 2PC state file records
 *		for all locks currently held.
 *
 * Session-level locks are ignored, as are VXID locks.
 *
 * There are some special cases that we error out on: we can't be holding any
 * locks at both session and transaction level (since we must either keep or
 * give away the PROCLOCK object), and we can't be holding any locks on
 * temporary objects (since that would mess up the current backend if it tries
 * to exit before the prepared xact is committed).
 */
void
AtPrepare_Locks(void)
{
	HASH_SEQ_STATUS status;
	LOCALLOCK  *locallock;

	/*
	 * For the most part, we don't need to touch shared memory for this ---
	 * all the necessary state information is in the locallock table.
	 * Fast-path locks are an exception, however: we move any such locks to
	 * the main table before allowing PREPARE TRANSACTION to succeed.
	 */
	hash_seq_init(&status, LockMethodLocalHash);

	while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
	{
		TwoPhaseLockRecord record;
		LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
		bool		haveSessionLock;
		bool		haveXactLock;
		int			i;

		/*
		 * Ignore VXID locks.  We don't want those to be held by prepared
		 * transactions, since they aren't meaningful after a restart.
		 */
		if (locallock->tag.lock.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
			continue;

		/* Ignore it if we don't actually hold the lock */
		if (locallock->nLocks <= 0)
			continue;

		/* Scan to see whether we hold it at session or transaction level */
		haveSessionLock = haveXactLock = false;
		for (i = locallock->numLockOwners - 1; i >= 0; i--)
		{
			if (lockOwners[i].owner == NULL)
				haveSessionLock = true;
			else
				haveXactLock = true;
		}

		/* Ignore it if we have only session lock */
		if (!haveXactLock)
			continue;

		/*
		 * If we have both session- and transaction-level locks, fail.	This
		 * should never happen with regular locks, since we only take those at
		 * session level in some special operations like VACUUM.  It's
		 * possible to hit this with advisory locks, though.
		 *
		 * It would be nice if we could keep the session hold and give away
		 * the transactional hold to the prepared xact.  However, that would
		 * require two PROCLOCK objects, and we cannot be sure that another
		 * PROCLOCK will be available when it comes time for PostPrepare_Locks
		 * to do the deed.	So for now, we error out while we can still do so
		 * safely.
		 */
		if (haveSessionLock)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot PREPARE while holding both session-level and transaction-level locks on the same object")));

		/*
		 * If the local lock was taken via the fast-path, we need to move it
		 * to the primary lock table, or just get a pointer to the existing
		 * primary lock table entry if by chance it's already been
		 * transferred.
		 */
		if (locallock->proclock == NULL)
		{
			locallock->proclock = FastPathGetRelationLockEntry(locallock);
			locallock->lock = locallock->proclock->tag.myLock;
		}

		/*
		 * Arrange to not release any strong lock count held by this lock
		 * entry.  We must retain the count until the prepared transaction is
		 * committed or rolled back.
		 */
		locallock->holdsStrongLockCount = FALSE;

		/* gp-change
		 *
		 * We allow 2PC commit transactions to include temp objects.  
		 * After PREPARE we WILL NOT transfer locks on the temp objects
		 * into our 2PC record.  Instead, we will keep them with the proc which
		 * will be released at the end of the session.
		 *
		 * There doesn't seem to be any reason not to do this.  Once the txn
		 * is prepared, it will be committed or aborted regardless of the state
		 * of the temp table.  and quite possibly, the temp table will be
		 * destroyed at the end of the session, while the transaction will be
		 * committed from another session.
		 */
		if (LockTagIsTemp(&locallock->tag.lock))
			continue;

		/*
		 * Create a 2PC record.
		 */
		memcpy(&(record.locktag), &(locallock->tag.lock), sizeof(LOCKTAG));
		record.lockmode = locallock->tag.mode;

		RegisterTwoPhaseRecord(TWOPHASE_RM_LOCK_ID, 0,
							   &record, sizeof(TwoPhaseLockRecord));
	}
}

/*
 * PostPrepare_Locks
 *		Clean up after successful PREPARE
 *
 * Here, we want to transfer ownership of our locks to a dummy PGPROC
 * that's now associated with the prepared transaction, and we want to
 * clean out the corresponding entries in the LOCALLOCK table.
 *
 * Note: by removing the LOCALLOCK entries, we are leaving dangling
 * pointers in the transaction's resource owner.  This is OK at the
 * moment since resowner.c doesn't try to free locks retail at a toplevel
 * transaction commit or abort.  We could alternatively zero out nLocks
 * and leave the LOCALLOCK entries to be garbage-collected by LockReleaseAll,
 * but that probably costs more cycles.
 */
void
PostPrepare_Locks(TransactionId xid)
{
	PGPROC	   *newproc = TwoPhaseGetDummyProc(xid);
	HASH_SEQ_STATUS status;
	LOCALLOCK  *locallock;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	PROCLOCKTAG proclocktag;
	bool		found;
	int			partition;

	/* This is a critical section: any error means big trouble */
	START_CRIT_SECTION();

	/*
	 * First we run through the locallock table and get rid of unwanted
	 * entries, then we scan the process's proclocks and transfer them to the
	 * target proc.
	 *
	 * We do this in two passes: first we find which locks we're going
	 * to remove and mark them. then we take another pass and remove
	 * them. We do it this way because LockTagIsTemp() potentially
	 * acquires new locks, and depending on the ordering in the table
	 * we don't want to remove *those* locallock entries!
	 *
	 * We do this separately because we may have multiple locallock entries
	 * pointing to the same proclock, and we daren't end up with any dangling
	 * pointers.
	 */
	hash_seq_init(&status, LockMethodLocalHash);

	while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
	{
		LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
		bool		haveSessionLock;
		bool		haveXactLock;
		int			i;

		locallock->preparable = false;

		if (locallock->proclock == NULL || locallock->lock == NULL)
		{
			/*
			 * We must've run out of shared memory while trying to set up this
			 * lock.  Just forget the local entry.
			 */
			Assert(locallock->nLocks == 0);
			RemoveLocalLock(locallock);
			continue;
		}

		/* Ignore VXID locks */
		if (locallock->tag.lock.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
			continue;

		/* MPP change for temp objects in 2PC.  we skip over temp
		 * objects. MPP-1094: NOTE THIS CALL MAY ADD LOCKS TO OUR
		 * TABLE!
		 */
		if (LockTagIsTemp(&locallock->tag.lock))
			continue;

		/* Scan to see whether we hold it at session or transaction level */
		haveSessionLock = haveXactLock = false;
		for (i = locallock->numLockOwners - 1; i >= 0; i--)
		{
			if (lockOwners[i].owner == NULL)
				haveSessionLock = true;
			else
				haveXactLock = true;
		}

		/* Ignore it if we have only session lock */
		if (!haveXactLock)
			continue;

		/* This can't happen, because we already checked it */
		if (haveSessionLock)
			ereport(PANIC,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot PREPARE while holding both session-level and transaction-level locks on the same object")));

		/* since our temp-check may be modifying our lock table, we
		 * just mark these as requiring more work */
		locallock->preparable = true;
	}

	/* We've marked the entries we want to delete; now go do the real work */
	hash_seq_init(&status, LockMethodLocalHash);

	while ((locallock = (LOCALLOCK *) hash_seq_search(&status)) != NULL)
	{
		if (!locallock->preparable)
			continue;
		
		/* Mark the proclock to show we need to release this lockmode */
		if (locallock->nLocks > 0)
			locallock->proclock->releaseMask |= LOCKBIT_ON(locallock->tag.mode);

		/* And remove the locallock hashtable entry */
		RemoveLocalLock(locallock);
	}

	/*
	 * Now, scan each lock partition separately.
	 */
	for (partition = 0; partition < NUM_LOCK_PARTITIONS; partition++)
	{
		LWLockId	partitionLock = FirstLockMgrLock + partition;
		SHM_QUEUE  *procLocks = &(MyProc->myProcLocks[partition]);

		proclock = (PROCLOCK *) SHMQueueNext(procLocks, procLocks,
											 offsetof(PROCLOCK, procLink));

		if (!proclock)
			continue;			/* needn't examine this partition */

		LWLockAcquire(partitionLock, LW_EXCLUSIVE);

		while (proclock)
		{
			PROCLOCK   *nextplock;
			LOCKMASK	holdMask;
			PROCLOCK   *newproclock;

			/* Get link first, since we may unlink/delete this proclock */
			nextplock = (PROCLOCK *)
				SHMQueueNext(procLocks, &proclock->procLink,
							 offsetof(PROCLOCK, procLink));

			Assert(proclock->tag.myProc == MyProc);

			lock = proclock->tag.myLock;

			/* MPP change for support of temp objects in 2PC.
			 *
			 * The case where the releaseMask is different than the holdMask is only
			 * for session locks.  Temp objects is the only session lock we could
			 * have here and we DO NOT want to release this lock.  so we
			 * skip over it.
			 */
			if (proclock->releaseMask != proclock->holdMask)
				goto next_item;

			/* Ignore VXID locks */
			if (lock->tag.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
				goto next_item;

			PROCLOCK_PRINT("PostPrepare_Locks", proclock);
			LOCK_PRINT("PostPrepare_Locks", lock, 0);
			Assert(lock->nRequested >= 0);
			Assert(lock->nGranted >= 0);
			Assert(lock->nGranted <= lock->nRequested);
			Assert((proclock->holdMask & ~lock->grantMask) == 0);

			/* Ignore it if nothing to release (must be a session lock) */
			if (proclock->releaseMask == 0)
				goto next_item;

			/* Else we should be releasing all locks */
			if (proclock->releaseMask != proclock->holdMask)
				elog(PANIC, "we seem to have dropped a bit somewhere");

			holdMask = proclock->holdMask;

			/*
			 * We cannot simply modify proclock->tag.myProc to reassign
			 * ownership of the lock, because that's part of the hash key and
			 * the proclock would then be in the wrong hash chain.	So, unlink
			 * and delete the old proclock; create a new one with the right
			 * contents; and link it into place.  We do it in this order to be
			 * certain we won't run out of shared memory (the way dynahash.c
			 * works, the deleted object is certain to be available for
			 * reallocation).
			 */
			SHMQueueDelete(&proclock->lockLink);
			SHMQueueDelete(&proclock->procLink);
			if (!hash_search(LockMethodProcLockHash,
							 (void *) &(proclock->tag),
							 HASH_REMOVE, NULL))
				elog(PANIC, "proclock table corrupted");

			/*
			 * Create the hash key for the new proclock table.
			 */
			proclocktag.myLock = lock;
			proclocktag.myProc = newproc;

			newproclock = (PROCLOCK *) hash_search(LockMethodProcLockHash,
												   (void *) &proclocktag,
												   HASH_ENTER_NULL, &found);
			if (!newproclock)
				ereport(PANIC,	/* should not happen */
						(errcode(ERRCODE_OUT_OF_MEMORY),
						 errmsg("out of shared memory"),
						 errdetail("Not enough memory for reassigning the prepared transaction's locks.")));

			/*
			 * If new, initialize the new entry
			 */
			if (!found)
			{
				newproclock->holdMask = 0;
				newproclock->releaseMask = 0;
				/* Add new proclock to appropriate lists */
				SHMQueueInsertBefore(&lock->procLocks, &newproclock->lockLink);
				SHMQueueInsertBefore(&(newproc->myProcLocks[partition]),
									 &newproclock->procLink);
				PROCLOCK_PRINT("PostPrepare_Locks: new", newproclock);
			}
			else
			{
				PROCLOCK_PRINT("PostPrepare_Locks: found", newproclock);
				Assert((newproclock->holdMask & ~lock->grantMask) == 0);
			}

			/*
			 * Pass over the identified lock ownership.
			 */
			Assert((newproclock->holdMask & holdMask) == 0);
			newproclock->holdMask |= holdMask;

	next_item:
			proclock = nextplock;
		}						/* loop over PROCLOCKs within this partition */

		LWLockRelease(partitionLock);
	}							/* loop over partitions */

	END_CRIT_SECTION();
}


/*
 * Estimate shared-memory space used for lock tables
 */
Size
LockShmemSize(void)
{
	Size		size = 0;
	long		max_table_size;

	/* lock hash table */
	max_table_size = NLOCKENTS();

	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled())
	{
		add_size(max_table_size, NRESLOCKENTS() );
	}

	size = add_size(size, hash_estimate_size(max_table_size, sizeof(LOCK)));
	
	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled())
	{
		add_size(max_table_size, NRESPROCLOCKENTS() );
	}

	/* proclock hash table */
	max_table_size *= 2;
	size = add_size(size, hash_estimate_size(max_table_size, sizeof(PROCLOCK)));

	/*
	 * Since NLOCKENTS is only an estimate, add 10% safety margin.
	 */
	size = add_size(size, size / 10);

	return size;
}

/*
 * GetLockStatusData - Return a summary of the lock manager's internal
 * status, for use in a user-level reporting function.
 *
 * The return data consists of an array of PROCLOCK objects, with the
 * associated PGPROC and LOCK objects for each.  Note that multiple
 * copies of the same PGPROC and/or LOCK objects are likely to appear.
 * It is the caller's responsibility to match up duplicates if wanted.
 *
 * The design goal is to hold the LWLocks for as short a time as possible;
 * thus, this function simply makes a copy of the necessary data and releases
 * the locks, allowing the caller to contemplate and format the data for as
 * long as it pleases.
 */
LockData *
GetLockStatusData(void)
{
	LockData   *data;
	PROCLOCK   *proclock;
	HASH_SEQ_STATUS seqstat;
	int			els;
	int			el;
	int			i;

	data = (LockData *) palloc(sizeof(LockData));

	/* Guess how much space we'll need. */
	els = MaxBackends;
	el = 0;
	data->locks = (LockInstanceData *) palloc(sizeof(LockInstanceData) * els);

	/*
	 * First, we iterate through the per-backend fast-path arrays, locking
	 * them one at a time.	This might produce an inconsistent picture of the
	 * system state, but taking all of those LWLocks at the same time seems
	 * impractical (in particular, note MAX_SIMUL_LWLOCKS).  It shouldn't
	 * matter too much, because none of these locks can be involved in lock
	 * conflicts anyway - anything that might must be present in the main lock
	 * table.
	 */
	for (i = 0; i < ProcGlobal->allProcCount; ++i)
	{
		PGPROC	   *proc = &ProcGlobal->allProcs[i];
		TMGXACT	   *gxact = &ProcGlobal->allTmGxact[i];
		uint32		f;

		LWLockAcquire(proc->backendLock, LW_SHARED);

		for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; ++f)
		{
			LockInstanceData *instance;
			uint32		lockbits = FAST_PATH_GET_BITS(proc, f);
			uint32      holdTillEndXactBits = FAST_PATH_GET_HOLD_TILL_END_XACT_BITS(proc, f);

			/* Skip unallocated slots. */
			if (!lockbits)
				continue;

			if (el >= els)
			{
				els += MaxBackends;
				data->locks = (LockInstanceData *)
					repalloc(data->locks, sizeof(LockInstanceData) * els);
			}

			instance = &data->locks[el];
			SET_LOCKTAG_RELATION(instance->locktag, proc->databaseId,
								 proc->fpRelId[f]);
			instance->holdMask = lockbits << FAST_PATH_LOCKNUMBER_OFFSET;
			instance->waitLockMode = NoLock;
			instance->backend = proc->backendId;
			instance->lxid = proc->lxid;
			instance->pid = proc->pid;
			instance->fastpath = true;
			instance->databaseId = proc->databaseId;
			instance->mppSessionId = proc->mppSessionId;
			instance->mppIsWriter = proc->mppIsWriter;
			instance->distribXid = (Gp_role == GP_ROLE_DISPATCH)?
								   gxact->gxid :
								   proc->localDistribXactData.distribXid;
			instance->holdTillEndXact = (holdTillEndXactBits > 0);
			el++;
		}

		if (proc->fpVXIDLock)
		{
			VirtualTransactionId vxid;
			LockInstanceData *instance;

			if (el >= els)
			{
				els += MaxBackends;
				data->locks = (LockInstanceData *)
					repalloc(data->locks, sizeof(LockInstanceData) * els);
			}

			vxid.backendId = proc->backendId;
			vxid.localTransactionId = proc->fpLocalTransactionId;

			instance = &data->locks[el];
			SET_LOCKTAG_VIRTUALTRANSACTION(instance->locktag, vxid);
			instance->holdMask = LOCKBIT_ON(ExclusiveLock);
			instance->waitLockMode = NoLock;
			instance->backend = proc->backendId;
			instance->lxid = proc->lxid;
			instance->pid = proc->pid;
			instance->fastpath = true;
			instance->databaseId = proc->databaseId;
			instance->mppSessionId = proc->mppSessionId;
			instance->mppIsWriter = proc->mppIsWriter;
			instance->distribXid = (Gp_role == GP_ROLE_DISPATCH)?
								   gxact->gxid :
								   proc->localDistribXactData.distribXid;
			instance->holdTillEndXact = false;
			el++;
		}

		LWLockRelease(proc->backendLock);
	}

	/*
	 * Next, acquire lock on the entire shared lock data structure.  We do
	 * this so that, at least for locks in the primary lock table, the state
	 * will be self-consistent.
	 *
	 * Since this is a read-only operation, we take shared instead of
	 * exclusive lock.	There's not a whole lot of point to this, because all
	 * the normal operations require exclusive lock, but it doesn't hurt
	 * anything either. It will at least allow two backends to do
	 * GetLockStatusData in parallel.
	 *
	 * Must grab LWLocks in partition-number order to avoid LWLock deadlock.
	 */
	for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
		LWLockAcquire(FirstLockMgrLock + i, LW_SHARED);

	/* Now we can safely count the number of proclocks */
	data->nelements = el + hash_get_num_entries(LockMethodProcLockHash);
	if (data->nelements > els)
	{
		els = data->nelements;
		data->locks = (LockInstanceData *)
			repalloc(data->locks, sizeof(LockInstanceData) * els);
	}

	/* Now scan the tables to copy the data */
	hash_seq_init(&seqstat, LockMethodProcLockHash);

	while ((proclock = (PROCLOCK *) hash_seq_search(&seqstat)))
	{
		PGPROC	   *proc = proclock->tag.myProc;
		LOCK	   *lock = proclock->tag.myLock;
		LockInstanceData *instance = &data->locks[el];
		TMGXACT	   *gxact = &ProcGlobal->allTmGxact[proc->pgprocno];

		memcpy(&instance->locktag, &lock->tag, sizeof(LOCKTAG));
		instance->holdMask = proclock->holdMask;
		if (proc->waitLock == proclock->tag.myLock)
			instance->waitLockMode = proc->waitLockMode;
		else
			instance->waitLockMode = NoLock;
		instance->backend = proc->backendId;
		instance->lxid = proc->lxid;
		instance->pid = proc->pid;
		instance->fastpath = false;
		instance->databaseId = proc->databaseId;
		instance->mppSessionId = proc->mppSessionId;
		instance->mppIsWriter = proc->mppIsWriter;
		instance->distribXid = (Gp_role == GP_ROLE_DISPATCH)?
							   gxact->gxid :
							   proc->localDistribXactData.distribXid;
		instance->holdTillEndXact = proclock->tag.myLock->holdTillEndXact;
		el++;
	}

	/*
	 * And release locks.  We do this in reverse order for two reasons: (1)
	 * Anyone else who needs more than one of the locks will be trying to lock
	 * them in increasing order; we don't want to release the other process
	 * until it can get all the locks it needs. (2) This avoids O(N^2)
	 * behavior inside LWLockRelease.
	 */
	for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
		LWLockRelease(FirstLockMgrLock + i);

	Assert(el == data->nelements);

	return data;
}

/*
 * Returns a list of currently held AccessExclusiveLocks, for use
 * by GetRunningTransactionData().
 */
xl_standby_lock *
GetRunningTransactionLocks(int *nlocks)
{
	PROCLOCK   *proclock;
	HASH_SEQ_STATUS seqstat;
	int			i;
	int			index;
	int			els;
	xl_standby_lock *accessExclusiveLocks;

	/*
	 * Acquire lock on the entire shared lock data structure.
	 *
	 * Must grab LWLocks in partition-number order to avoid LWLock deadlock.
	 */
	for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
		LWLockAcquire(FirstLockMgrLock + i, LW_SHARED);

	/* Now scan the tables to copy the data */
	hash_seq_init(&seqstat, LockMethodProcLockHash);

	/* Now we can safely count the number of proclocks */
	els = hash_get_num_entries(LockMethodProcLockHash);

	/*
	 * Allocating enough space for all locks in the lock table is overkill,
	 * but it's more convenient and faster than having to enlarge the array.
	 */
	accessExclusiveLocks = palloc(els * sizeof(xl_standby_lock));

	/*
	 * If lock is a currently granted AccessExclusiveLock then it will have
	 * just one proclock holder, so locks are never accessed twice in this
	 * particular case. Don't copy this code for use elsewhere because in the
	 * general case this will give you duplicate locks when looking at
	 * non-exclusive lock types.
	 */
	index = 0;
	while ((proclock = (PROCLOCK *) hash_seq_search(&seqstat)))
	{
		/* make sure this definition matches the one used in LockAcquire */
		if ((proclock->holdMask & LOCKBIT_ON(AccessExclusiveLock)) &&
			proclock->tag.myLock->tag.locktag_type == LOCKTAG_RELATION)
		{
			PGPROC	   *proc = proclock->tag.myProc;
			PGXACT	   *pgxact = &ProcGlobal->allPgXact[proc->pgprocno];
			LOCK	   *lock = proclock->tag.myLock;
			TransactionId xid = pgxact->xid;

			/*
			 * Don't record locks for transactions if we know they have
			 * already issued their WAL record for commit but not yet released
			 * lock. It is still possible that we see locks held by already
			 * complete transactions, if they haven't yet zeroed their xids.
			 */
			if (!TransactionIdIsValid(xid))
				continue;

			accessExclusiveLocks[index].xid = xid;
			accessExclusiveLocks[index].dbOid = lock->tag.locktag_field1;
			accessExclusiveLocks[index].relOid = lock->tag.locktag_field2;

			index++;
		}
	}

	/*
	 * And release locks.  We do this in reverse order for two reasons: (1)
	 * Anyone else who needs more than one of the locks will be trying to lock
	 * them in increasing order; we don't want to release the other process
	 * until it can get all the locks it needs. (2) This avoids O(N^2)
	 * behavior inside LWLockRelease.
	 */
	for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
		LWLockRelease(FirstLockMgrLock + i);

	*nlocks = index;
	return accessExclusiveLocks;
}

/* Provide the textual name of any lock mode */
const char *
GetLockmodeName(LOCKMETHODID lockmethodid, LOCKMODE mode)
{
	Assert(lockmethodid > 0 && lockmethodid < lengthof(LockMethods));
	Assert(mode > 0 && mode <= LockMethods[lockmethodid]->numLockModes);
	return LockMethods[lockmethodid]->lockModeNames[mode];
}

#ifdef LOCK_DEBUG
/*
 * Dump all locks in the given proc's myProcLocks lists.
 *
 * Caller is responsible for having acquired appropriate LWLocks.
 */
void
DumpLocks(PGPROC *proc)
{
	SHM_QUEUE  *procLocks;
	PROCLOCK   *proclock;
	LOCK	   *lock;
	int			i;

	if (proc == NULL)
		return;

	if (proc->waitLock)
		LOCK_PRINT("DumpLocks: waiting on", proc->waitLock, 0);

	for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
	{
		procLocks = &(proc->myProcLocks[i]);

		proclock = (PROCLOCK *) SHMQueueNext(procLocks, procLocks,
											 offsetof(PROCLOCK, procLink));

		while (proclock)
		{
			Assert(proclock->tag.myProc == proc);

			lock = proclock->tag.myLock;

			PROCLOCK_PRINT("DumpLocks", proclock);
			LOCK_PRINT("DumpLocks", lock, 0);

			proclock = (PROCLOCK *)
				SHMQueueNext(procLocks, &proclock->procLink,
							 offsetof(PROCLOCK, procLink));
		}
	}
}

/*
 * Dump all lmgr locks.
 *
 * Caller is responsible for having acquired appropriate LWLocks.
 */
void
DumpAllLocks(void)
{
	PGPROC	   *proc;
	PROCLOCK   *proclock;
	LOCK	   *lock;
	HASH_SEQ_STATUS status;

	proc = MyProc;

	if (proc && proc->waitLock)
		LOCK_PRINT("DumpAllLocks: waiting on", proc->waitLock, 0);

	hash_seq_init(&status, LockMethodProcLockHash);

	while ((proclock = (PROCLOCK *) hash_seq_search(&status)) != NULL)
	{
		PROCLOCK_PRINT("DumpAllLocks", proclock);

		lock = proclock->tag.myLock;
		if (lock)
			LOCK_PRINT("DumpAllLocks", lock, 0);
		else
			elog(LOG, "DumpAllLocks: proclock->tag.myLock = NULL");
	}
}
#endif   /* LOCK_DEBUG */

/*
 * LOCK 2PC resource manager's routines
 */

/*
 * Re-acquire a lock belonging to a transaction that was prepared.
 *
 * Because this function is run at db startup, re-acquiring the locks should
 * never conflict with running transactions because there are none.  We
 * assume that the lock state represented by the stored 2PC files is legal.
 *
 * When switching from Hot Standby mode to normal operation, the locks will
 * be already held by the startup process. The locks are acquired for the new
 * procs without checking for conflicts, so we don't get a conflict between the
 * startup process and the dummy procs, even though we will momentarily have
 * a situation where two procs are holding the same AccessExclusiveLock,
 * which isn't normally possible because the conflict. If we're in standby
 * mode, but a recovery snapshot hasn't been established yet, it's possible
 * that some but not all of the locks are already held by the startup process.
 *
 * This approach is simple, but also a bit dangerous, because if there isn't
 * enough shared memory to acquire the locks, an error will be thrown, which
 * is promoted to FATAL and recovery will abort, bringing down postmaster.
 * A safer approach would be to transfer the locks like we do in
 * AtPrepare_Locks, but then again, in hot standby mode it's possible for
 * read-only backends to use up all the shared lock memory anyway, so that
 * replaying the WAL record that needs to acquire a lock will throw an error
 * and PANIC anyway.
 */
void
lock_twophase_recover(TransactionId xid, uint16 info,
					  void *recdata, uint32 len)
{
	TwoPhaseLockRecord *rec = (TwoPhaseLockRecord *) recdata;
	PGPROC	   *proc = TwoPhaseGetDummyProc(xid);
	LOCKTAG    *locktag;
	LOCKMODE	lockmode;
	LOCKMETHODID lockmethodid;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	PROCLOCKTAG proclocktag;
	bool		found;
	uint32		hashcode;
	uint32		proclock_hashcode;
	int			partition;
	LWLockId	partitionLock;
	LockMethod	lockMethodTable;

	Assert(len == sizeof(TwoPhaseLockRecord));
	locktag = &rec->locktag;
	lockmode = rec->lockmode;
	lockmethodid = locktag->locktag_lockmethodid;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];

	hashcode = LockTagHashCode(locktag);
	partition = LockHashPartition(hashcode);
	partitionLock = LockHashPartitionLock(hashcode);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * Find or create a lock with this tag.
	 */
	lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
												(void *) locktag,
												hashcode,
												HASH_ENTER_NULL,
												&found);
	if (!lock)
	{
		LWLockRelease(partitionLock);
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of shared memory"),
		  errhint("You might need to increase max_locks_per_transaction.")));
	}

	/*
	 * if it's a new lock object, initialize it
	 */
	if (!found)
	{
		lock->grantMask = 0;
		lock->waitMask = 0;
		SHMQueueInit(&(lock->procLocks));
		ProcQueueInit(&(lock->waitProcs));
		lock->nRequested = 0;
		lock->nGranted = 0;
		lock->holdTillEndXact = false;
		MemSet(lock->requested, 0, sizeof(int) * MAX_LOCKMODES);
		MemSet(lock->granted, 0, sizeof(int) * MAX_LOCKMODES);
		LOCK_PRINT("lock_twophase_recover: new", lock, lockmode);
	}
	else
	{
		LOCK_PRINT("lock_twophase_recover: found", lock, lockmode);
		Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));
		Assert((lock->nGranted >= 0) && (lock->granted[lockmode] >= 0));
		Assert(lock->nGranted <= lock->nRequested);
	}

	/*
	 * Create the hash key for the proclock table.
	 */
	proclocktag.myLock = lock;
	proclocktag.myProc = proc;

	proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

	/*
	 * Find or create a proclock entry with this tag
	 */
	proclock = (PROCLOCK *) hash_search_with_hash_value(LockMethodProcLockHash,
														(void *) &proclocktag,
														proclock_hashcode,
														HASH_ENTER_NULL,
														&found);
	if (!proclock)
	{
		/* Ooops, not enough shmem for the proclock */
		if (lock->nRequested == 0)
		{
			/*
			 * There are no other requestors of this lock, so garbage-collect
			 * the lock object.  We *must* do this to avoid a permanent leak
			 * of shared memory, because there won't be anything to cause
			 * anyone to release the lock object later.
			 */
			Assert(SHMQueueEmpty(&(lock->procLocks)));
			if (!hash_search_with_hash_value(LockMethodLockHash,
											 (void *) &(lock->tag),
											 hashcode,
											 HASH_REMOVE,
											 NULL))
				elog(PANIC, "lock table corrupted");
		}
		LWLockRelease(partitionLock);
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of shared memory"),
		  errhint("You might need to increase max_locks_per_transaction.")));
	}

	/*
	 * If new, initialize the new entry
	 */
	if (!found)
	{
		proclock->holdMask = 0;
		proclock->releaseMask = 0;
		/* Add proclock to appropriate lists */
		SHMQueueInsertBefore(&lock->procLocks, &proclock->lockLink);
		SHMQueueInsertBefore(&(proc->myProcLocks[partition]),
							 &proclock->procLink);
		PROCLOCK_PRINT("lock_twophase_recover: new", proclock);
	}
	else
	{
		PROCLOCK_PRINT("lock_twophase_recover: found", proclock);
		Assert((proclock->holdMask & ~lock->grantMask) == 0);
	}

	/*
	 * lock->nRequested and lock->requested[] count the total number of
	 * requests, whether granted or waiting, so increment those immediately.
	 */
	lock->nRequested++;
	lock->requested[lockmode]++;
	Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));

	/*
	 * We shouldn't already hold the desired lock.
	 */
	if (proclock->holdMask & LOCKBIT_ON(lockmode))
		ereport(ERROR, (errmsg("lock %s on object %u/%u/%u is already held",
			 lockMethodTable->lockModeNames[lockmode],
			 lock->tag.locktag_field1, lock->tag.locktag_field2,
			 lock->tag.locktag_field3)));

	/*
	 * We ignore any possible conflicts and just grant ourselves the lock. Not
	 * only because we don't bother, but also to avoid deadlocks when
	 * switching from standby to normal mode. See function comment.
	 */
	GrantLock(lock, proclock, lockmode);

	/*
	 * Bump strong lock count, to make sure any fast-path lock requests won't
	 * be granted without consulting the primary lock table.
	 */
	if (ConflictsWithRelationFastPath(&lock->tag, lockmode))
	{
		uint32		fasthashcode = FastPathStrongLockHashPartition(hashcode);

		SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
		FastPathStrongRelationLocks->count[fasthashcode]++;
		SpinLockRelease(&FastPathStrongRelationLocks->mutex);
	}

	LWLockRelease(partitionLock);
}

/*
 * Re-acquire a lock belonging to a transaction that was prepared, when
 * when starting up into hot standby mode.
 */
void
lock_twophase_standby_recover(TransactionId xid, uint16 info,
							  void *recdata, uint32 len)
{
	TwoPhaseLockRecord *rec = (TwoPhaseLockRecord *) recdata;
	LOCKTAG    *locktag;
	LOCKMODE	lockmode;
	LOCKMETHODID lockmethodid;

	Assert(len == sizeof(TwoPhaseLockRecord));
	locktag = &rec->locktag;
	lockmode = rec->lockmode;
	lockmethodid = locktag->locktag_lockmethodid;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);

	if (lockmode == AccessExclusiveLock &&
		locktag->locktag_type == LOCKTAG_RELATION)
	{
		StandbyAcquireAccessExclusiveLock(xid,
										locktag->locktag_field1 /* dboid */ ,
									  locktag->locktag_field2 /* reloid */ );
	}
}


/*
 * 2PC processing routine for COMMIT PREPARED case.
 *
 * Find and release the lock indicated by the 2PC record.
 */
void
lock_twophase_postcommit(TransactionId xid, uint16 info,
						 void *recdata, uint32 len)
{
	TwoPhaseLockRecord *rec = (TwoPhaseLockRecord *) recdata;
	PGPROC	   *proc = TwoPhaseGetDummyProc(xid);
	LOCKTAG    *locktag;
	LOCKMETHODID lockmethodid;
	LockMethod	lockMethodTable;

	Assert(len == sizeof(TwoPhaseLockRecord));
	locktag = &rec->locktag;
	lockmethodid = locktag->locktag_lockmethodid;

	if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
		elog(ERROR, "unrecognized lock method: %d", lockmethodid);
	lockMethodTable = LockMethods[lockmethodid];

	LockRefindAndRelease(lockMethodTable, proc, locktag, rec->lockmode, true);
}

/*
 * 2PC processing routine for ROLLBACK PREPARED case.
 *
 * This is actually just the same as the COMMIT case.
 */
void
lock_twophase_postabort(TransactionId xid, uint16 info,
						void *recdata, uint32 len)
{
	lock_twophase_postcommit(xid, info, recdata, len);
}

/*
 *		VirtualXactLockTableInsert
 *
 *		Take vxid lock via the fast-path.  There can't be any pre-existing
 *		lockers, as we haven't advertised this vxid via the ProcArray yet.
 *
 *		Since MyProc->fpLocalTransactionId will normally contain the same data
 *		as MyProc->lxid, you might wonder if we really need both.  The
 *		difference is that MyProc->lxid is set and cleared unlocked, and
 *		examined by procarray.c, while fpLocalTransactionId is protected by
 *		backendLock and is used only by the locking subsystem.	Doing it this
 *		way makes it easier to verify that there are no funny race conditions.
 *
 *		We don't bother recording this lock in the local lock table, since it's
 *		only ever released at the end of a transaction.  Instead,
 *		LockReleaseAll() calls VirtualXactLockTableCleanup().
 */
void
VirtualXactLockTableInsert(VirtualTransactionId vxid)
{
	Assert(VirtualTransactionIdIsValid(vxid));

	LWLockAcquire(MyProc->backendLock, LW_EXCLUSIVE);

	Assert(MyProc->backendId == vxid.backendId);
	Assert(MyProc->fpLocalTransactionId == InvalidLocalTransactionId);
	Assert(MyProc->fpVXIDLock == false);

	MyProc->fpVXIDLock = true;
	MyProc->fpLocalTransactionId = vxid.localTransactionId;

	LWLockRelease(MyProc->backendLock);
}

/*
 *		VirtualXactLockTableCleanup
 *
 *		Check whether a VXID lock has been materialized; if so, release it,
 *		unblocking waiters.
 */
static void
VirtualXactLockTableCleanup()
{
	bool		fastpath;
	LocalTransactionId lxid;

	Assert(MyProc->backendId != InvalidBackendId);

	/*
	 * Clean up shared memory state.
	 */
	LWLockAcquire(MyProc->backendLock, LW_EXCLUSIVE);

	fastpath = MyProc->fpVXIDLock;
	lxid = MyProc->fpLocalTransactionId;
	MyProc->fpVXIDLock = false;
	MyProc->fpLocalTransactionId = InvalidLocalTransactionId;

	LWLockRelease(MyProc->backendLock);

	/*
	 * If fpVXIDLock has been cleared without touching fpLocalTransactionId,
	 * that means someone transferred the lock to the main lock table.
	 */
	if (!fastpath && LocalTransactionIdIsValid(lxid))
	{
		VirtualTransactionId vxid;
		LOCKTAG		locktag;

		vxid.backendId = MyBackendId;
		vxid.localTransactionId = lxid;
		SET_LOCKTAG_VIRTUALTRANSACTION(locktag, vxid);

		LockRefindAndRelease(LockMethods[DEFAULT_LOCKMETHOD], MyProc,
							 &locktag, ExclusiveLock, false);
	}
}

/*
 *		VirtualXactLock
 *
 * If wait = true, wait until the given VXID has been released, and then
 * return true.
 *
 * If wait = false, just check whether the VXID is still running, and return
 * true or false.
 */
bool
VirtualXactLock(VirtualTransactionId vxid, bool wait)
{
	LOCKTAG		tag;
	PGPROC	   *proc;

	Assert(VirtualTransactionIdIsValid(vxid));

	SET_LOCKTAG_VIRTUALTRANSACTION(tag, vxid);

	/*
	 * If a lock table entry must be made, this is the PGPROC on whose behalf
	 * it must be done.  Note that the transaction might end or the PGPROC
	 * might be reassigned to a new backend before we get around to examining
	 * it, but it doesn't matter.  If we find upon examination that the
	 * relevant lxid is no longer running here, that's enough to prove that
	 * it's no longer running anywhere.
	 */
	proc = BackendIdGetProc(vxid.backendId);
	if (proc == NULL)
		return true;

	/*
	 * We must acquire this lock before checking the backendId and lxid
	 * against the ones we're waiting for.  The target backend will only set
	 * or clear lxid while holding this lock.
	 */
	LWLockAcquire(proc->backendLock, LW_EXCLUSIVE);

	/* If the transaction has ended, our work here is done. */
	if (proc->backendId != vxid.backendId
		|| proc->fpLocalTransactionId != vxid.localTransactionId)
	{
		LWLockRelease(proc->backendLock);
		return true;
	}

	/*
	 * If we aren't asked to wait, there's no need to set up a lock table
	 * entry.  The transaction is still in progress, so just return false.
	 */
	if (!wait)
	{
		LWLockRelease(proc->backendLock);
		return false;
	}

	/*
	 * OK, we're going to need to sleep on the VXID.  But first, we must set
	 * up the primary lock table entry, if needed.
	 */
	if (proc->fpVXIDLock)
	{
		PROCLOCK   *proclock;
		uint32		hashcode;

		hashcode = LockTagHashCode(&tag);
		proclock = SetupLockInTable(LockMethods[DEFAULT_LOCKMETHOD], proc,
									&tag, hashcode, ExclusiveLock);
		if (!proclock)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of shared memory"),
					 errhint("You might need to increase max_locks_per_transaction.")));
		GrantLock(proclock->tag.myLock, proclock, ExclusiveLock);
		proc->fpVXIDLock = false;
	}

	/* Done with proc->fpLockBits */
	LWLockRelease(proc->backendLock);

	/* Time to wait. */
	(void) LockAcquire(&tag, ShareLock, false, false);

	LockRelease(&tag, ShareLock, false);
	return true;
}

/*
 *         setFPHoldTillEndXact
 * Some locks are acquired via fast path, this function is
 * to set the HoldTillEndXact field for those relation locks.
 */
static bool
setFPHoldTillEndXact(Oid relid)
{
	uint32  f;
	bool result = false;
	PGPROC *proc = MyProc;

	LWLockAcquire(proc->backendLock, LW_EXCLUSIVE);

	for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; ++f)
	{
		uint32 lockbits;

		if (proc->fpRelId[f] != relid ||
			(lockbits = FAST_PATH_GET_BITS(proc, f)) == 0)
			continue;

		/* one relid only occupies one slot. */
		FAST_PATH_SET_HOLD_TILL_END_XACT(proc, f, lockbits);
		result = true;
		break;
	}

	LWLockRelease(proc->backendLock);

	return result;
}
