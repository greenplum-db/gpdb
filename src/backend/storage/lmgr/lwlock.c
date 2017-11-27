/*-------------------------------------------------------------------------
 *
 * lwlock.c
 *	  Lightweight lock manager
 *
 * Lightweight locks are intended primarily to provide mutual exclusion of
 * access to shared-memory data structures.  Therefore, they offer both
 * exclusive and shared lock modes (to support read/write and read-only
 * access to a shared object).	There are few other frammishes.  User-level
 * locking should be done with the full lock manager --- which depends on
 * LWLocks to protect its shared state.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/lmgr/lwlock.c,v 1.53 2009/01/01 17:23:48 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/clog.h"
#include "access/multixact.h"
#include "access/distributedlog.h"
#include "access/subtrans.h"
#include "access/twophase.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "storage/barrier.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/spin.h"
#include "utils/sharedsnapshot.h"
#include "pg_trace.h"


/* We use the ShmemLock spinlock to protect LWLockAssign */
extern slock_t *ShmemLock;


typedef struct LWLock
{
	slock_t		mutex;			/* Protects LWLock and queue of PGPROCs */
	bool		releaseOK;		/* T if ok to release waiters */
	char		exclusive;		/* # of exclusive holders (0 or 1) */
	int			shared;			/* # of shared holders (0..MaxBackends) */
	int			exclusivePid;	/* PID of the exclusive holder. */
	PGPROC	   *head;			/* head of list of waiting PGPROCs */
	PGPROC	   *tail;			/* tail of list of waiting PGPROCs */
	/* tail is undefined when head is NULL */
} LWLock;

/*
 * All the LWLock structs are allocated as an array in shared memory.
 * (LWLockIds are indexes into the array.)	We force the array stride to
 * be a power of 2, which saves a few cycles in indexing, but more
 * importantly also ensures that individual LWLocks don't cross cache line
 * boundaries.	This reduces cache contention problems, especially on AMD
 * Opterons.  (Of course, we have to also ensure that the array start
 * address is suitably aligned.)
 *
 * LWLock is between 16 and 32 bytes on all known platforms, so these two
 * cases are sufficient.
 */
#define LWLOCK_PADDED_SIZE	(sizeof(LWLock) <= 16 ? 16 : 32)

typedef union LWLockPadded
{
	LWLock		lock;
	char		pad[LWLOCK_PADDED_SIZE];
} LWLockPadded;

/*
 * This points to the array of LWLocks in shared memory.  Backends inherit
 * the pointer by fork from the postmaster (except in the EXEC_BACKEND case,
 * where we have special measures to pass it down).
 */
NON_EXEC_STATIC LWLockPadded *LWLockArray = NULL;


/*
 * We use this structure to keep track of locked LWLocks for release
 * during error recovery.  The maximum size could be determined at runtime
 * if necessary, but it seems unlikely that more than a few locks could
 * ever be held simultaneously.
 */
#define MAX_SIMUL_LWLOCKS	100
#define MAX_FRAME_DEPTH  	64

/* LW lock object id current PGPPROC is sleeping on (valid when PGPROC->lwWaiting = true) */
static LWLockId lwWaitingLockId = NullLock;

static int	num_held_lwlocks = 0;
static LWLockId held_lwlocks[MAX_SIMUL_LWLOCKS];
static bool held_lwlocks_exclusive[MAX_SIMUL_LWLOCKS];

#ifdef USE_TEST_UTILS_X86
static void *held_lwlocks_addresses[MAX_SIMUL_LWLOCKS][MAX_FRAME_DEPTH];
static int32 held_lwlocks_depth[MAX_SIMUL_LWLOCKS];
#endif /* USE_TEST_UTILS_X86 */

static int	lock_addin_request = 0;
static bool lock_addin_request_allowed = true;

#ifdef LWLOCK_STATS
static int	counts_for_pid = 0;
static int *sh_acquire_counts;
static int *ex_acquire_counts;
static int *block_counts;
#endif

#ifdef LOCK_DEBUG
bool		Trace_lwlocks = false;

inline static void
PRINT_LWDEBUG(const char *where, LWLockId lockid, const volatile LWLock *lock)
{
	if (Trace_lwlocks)
		elog(LOG, "%s(%d): excl %d excl pid %d shared %d head %p rOK %d",
			 where, (int) lockid,
			 (int) lock->exclusive, lock->exclusivePid, lock->shared, lock->head,
			 (int) lock->releaseOK);
}

inline static void
LOG_LWDEBUG(const char *where, LWLockId lockid, const char *msg)
{
	if (Trace_lwlocks)
		elog(LOG, "%s(%d): %s", where, (int) lockid, msg);
}
#else							/* not LOCK_DEBUG */
#define PRINT_LWDEBUG(a,b,c)
#define LOG_LWDEBUG(a,b,c)
#endif   /* LOCK_DEBUG */

#ifdef LWLOCK_STATS

static void
print_lwlock_stats(int code, Datum arg)
{
	int			i;
	int		   *LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	int			numLocks = LWLockCounter[1];

	/* Grab an LWLock to keep different backends from mixing reports */
	LWLockAcquire(0, LW_EXCLUSIVE);

	for (i = 0; i < numLocks; i++)
	{
		if (sh_acquire_counts[i] || ex_acquire_counts[i] || block_counts[i])
			fprintf(stderr, "PID %d lwlock %d: shacq %u exacq %u blk %u\n",
					MyProcPid, i, sh_acquire_counts[i], ex_acquire_counts[i],
					block_counts[i]);
	}

	LWLockRelease(0);
}
#endif   /* LWLOCK_STATS */


/*
 * Compute number of LWLocks to allocate.
 */
int
NumLWLocks(void)
{
	int			numLocks;

	/*
	 * Possibly this logic should be spread out among the affected modules,
	 * the same way that shmem space estimation is done.  But for now, there
	 * are few enough users of LWLocks that we can get away with just keeping
	 * the knowledge here.
	 */

	/* Predefined LWLocks */
	numLocks = (int) NumFixedLWLocks;

	/* bufmgr.c needs two for each shared buffer */
	numLocks += 2 * NBuffers;

	/* clog.c needs one per CLOG buffer */
	numLocks += NUM_CLOG_BUFFERS;

	/* subtrans.c needs one per SubTrans buffer */
	numLocks += NUM_SUBTRANS_BUFFERS;
    
    /* cdbtm.c needs one lock */
    numLocks++;
    
    /* cdbfts.c needs one lock */
    numLocks++;

	/* multixact.c needs two SLRU areas */
	numLocks += NUM_MXACTOFFSET_BUFFERS + NUM_MXACTMEMBER_BUFFERS;

	/* cdbdistributedlog.c needs one per DistributedLog buffer */
	numLocks += NUM_DISTRIBUTEDLOG_BUFFERS;

	/* sharedsnapshot.c needs one per shared snapshot slot */
	numLocks += NUM_SHARED_SNAPSHOT_SLOTS;
    
	/*
	 * Add any requested by loadable modules; for backwards-compatibility
	 * reasons, allocate at least NUM_USER_DEFINED_LWLOCKS of them even if
	 * there are no explicit requests.
	 */
	lock_addin_request_allowed = false;
	numLocks += Max(lock_addin_request, NUM_USER_DEFINED_LWLOCKS);

	return numLocks;
}


/*
 * RequestAddinLWLocks
 *		Request that extra LWLocks be allocated for use by
 *		a loadable module.
 *
 * This is only useful if called from the _PG_init hook of a library that
 * is loaded into the postmaster via shared_preload_libraries.	Once
 * shared memory has been allocated, calls will be ignored.  (We could
 * raise an error, but it seems better to make it a no-op, so that
 * libraries containing such calls can be reloaded if needed.)
 */
void
RequestAddinLWLocks(int n)
{
	if (IsUnderPostmaster || !lock_addin_request_allowed)
		return;					/* too late */
	lock_addin_request += n;
}


/*
 * Compute shmem space needed for LWLocks.
 */
Size
LWLockShmemSize(void)
{
	Size		size;
	int			numLocks = NumLWLocks();

	/* Space for the LWLock array. */
	size = mul_size(numLocks, sizeof(LWLockPadded));

	/* Space for dynamic allocation counter, plus room for alignment. */
	size = add_size(size, 2 * sizeof(int) + LWLOCK_PADDED_SIZE);

	return size;
}


/*
 * Allocate shmem space for LWLocks and initialize the locks.
 */
void
CreateLWLocks(void)
{
	int			numLocks = NumLWLocks();
	Size		spaceLocks = LWLockShmemSize();
	LWLockPadded *lock;
	int		   *LWLockCounter;
	char	   *ptr;
	int			id;

	/* Allocate space */
	ptr = (char *) ShmemAlloc(spaceLocks);

	/* Leave room for dynamic allocation counter */
	ptr += 2 * sizeof(int);

	/* Ensure desired alignment of LWLock array */
	ptr += LWLOCK_PADDED_SIZE - ((unsigned long) ptr) % LWLOCK_PADDED_SIZE;

	LWLockArray = (LWLockPadded *) ptr;

	/*
	 * Initialize all LWLocks to "unlocked" state
	 */
	for (id = 0, lock = LWLockArray; id < numLocks; id++, lock++)
	{
		SpinLockInit(&lock->lock.mutex);
		lock->lock.releaseOK = true;
		lock->lock.exclusive = 0;
		lock->lock.shared = 0;
		lock->lock.head = NULL;
		lock->lock.tail = NULL;
	}

	/*
	 * Initialize the dynamic-allocation counter, which is stored just before
	 * the first LWLock.
	 */
	LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	LWLockCounter[0] = (int) NumFixedLWLocks;
	LWLockCounter[1] = numLocks;
}


/*
 * LWLockAssign - assign a dynamically-allocated LWLock number
 *
 * We interlock this using the same spinlock that is used to protect
 * ShmemAlloc().  Interlocking is not really necessary during postmaster
 * startup, but it is needed if any user-defined code tries to allocate
 * LWLocks after startup.
 */
LWLockId
LWLockAssign(void)
{
	LWLockId	result;

	/* use volatile pointer to prevent code rearrangement */
	volatile int *LWLockCounter;

	LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	SpinLockAcquire(ShmemLock);
	if (LWLockCounter[0] >= LWLockCounter[1])
	{
		SpinLockRelease(ShmemLock);
		elog(ERROR, "no more LWLockIds available");
	}
	result = (LWLockId) (LWLockCounter[0]++);
	SpinLockRelease(ShmemLock);
	return result;
}

#ifdef LOCK_DEBUG

static void
LWLockTryLockWaiting(
		PGPROC	   *proc, 
		LWLockId lockid, 
		LWLockMode mode)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	int 			milliseconds = 0;
	int				exclusivePid;
	
	while(true)
	{
		pg_usleep(5000L);
		if (PGSemaphoreTryLock(&proc->sem))
		{
			if (milliseconds >= 750)
				elog(LOG, "Done waiting on lockid %d", lockid);
			return;
		}

		milliseconds += 5;
		if (milliseconds == 750)
		{
			int l;
			int count = 0;
			char buffer[200];

			SpinLockAcquire(&lock->mutex);
			
			if (lock->exclusive > 0)
				exclusivePid = lock->exclusivePid;
			else
				exclusivePid = 0;
			
			SpinLockRelease(&lock->mutex);

			memcpy(buffer, "none", 5);
			
			for (l = 0; l < num_held_lwlocks; l++)
			{
				if (l == 0)
					count += sprintf(&buffer[count],"(");
				else
					count += sprintf(&buffer[count],", ");
				
				count += sprintf(&buffer[count],
							    "lockid %d",
							    held_lwlocks[l]);
			}
			if (num_held_lwlocks > 0)
				count += sprintf(&buffer[count],")");
				
			elog(LOG, "Waited .75 seconds on lockid %d with no success. Exclusive pid %d. Already held: %s", 
				 lockid, exclusivePid, buffer);

		}
	}
}

#endif

// Turn this on if we find a deadlock or missing unlock issue...
// #define LWLOCK_TRACE_MIRROREDLOCK

/*
 * LWLockAcquire - acquire a lightweight lock in the specified mode
 *
 * If the lock is not available, sleep until it is.
 *
 * Side effect: cancel/die interrupts are held off until lock release.
 */
void
LWLockAcquire(LWLockId lockid, LWLockMode mode)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	PGPROC	   *proc = MyProc;
	bool		retry = false;
	int			extraWaits = 0;

	PRINT_LWDEBUG("LWLockAcquire", lockid, lock);

#ifdef LWLOCK_STATS
	/* Set up local count state first time through in a given process */
	if (counts_for_pid != MyProcPid)
	{
		int		   *LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
		int			numLocks = LWLockCounter[1];

		sh_acquire_counts = calloc(numLocks, sizeof(int));
		ex_acquire_counts = calloc(numLocks, sizeof(int));
		block_counts = calloc(numLocks, sizeof(int));

		if(!sh_acquire_counts || !ex_acquire_counts || !block_counts)
			ereport(ERROR, errcode(ERRCODE_OUT_OF_MEMORY),
				errmsg("LWLockAcquire failed: out of memory"));

		counts_for_pid = MyProcPid;
		on_shmem_exit(print_lwlock_stats, 0);
	}
	/* Count lock acquisition attempts */
	if (mode == LW_EXCLUSIVE)
		ex_acquire_counts[lockid]++;
	else
		sh_acquire_counts[lockid]++;
#endif   /* LWLOCK_STATS */

	/*
	 * We can't wait if we haven't got a PGPROC.  This should only occur
	 * during bootstrap or shared memory initialization.  Put an Assert here
	 * to catch unsafe coding practices.
	 */
	Assert(!(proc == NULL && IsUnderPostmaster));

	/* Ensure we will have room to remember the lock */
	if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
		elog(ERROR, "too many LWLocks taken");

	/*
	 * Lock out cancel/die interrupts until we exit the code section protected
	 * by the LWLock.  This ensures that interrupts will not interfere with
	 * manipulations of data structures in shared memory.
	 */
	HOLD_INTERRUPTS();

	/*
	 * Loop here to try to acquire lock after each time we are signaled by
	 * LWLockRelease.
	 *
	 * NOTE: it might seem better to have LWLockRelease actually grant us the
	 * lock, rather than retrying and possibly having to go back to sleep. But
	 * in practice that is no good because it means a process swap for every
	 * lock acquisition when two or more processes are contending for the same
	 * lock.  Since LWLocks are normally used to protect not-very-long
	 * sections of computation, a process needs to be able to acquire and
	 * release the same lock many times during a single CPU time slice, even
	 * in the presence of contention.  The efficiency of being able to do that
	 * outweighs the inefficiency of sometimes wasting a process dispatch
	 * cycle because the lock is not free when a released waiter finally gets
	 * to run.	See pgsql-hackers archives for 29-Dec-01.
	 */
	for (;;)
	{
		bool		mustwait;
		int			c;

		/* Acquire mutex.  Time spent holding mutex should be short! */
		SpinLockAcquire(&lock->mutex);

		/* If retrying, allow LWLockRelease to release waiters again */
		if (retry)
			lock->releaseOK = true;

		/* If I can get the lock, do so quickly. */
		if (mode == LW_EXCLUSIVE)
		{
			if (lock->exclusive == 0 && lock->shared == 0)
			{
				lock->exclusive++;
				lock->exclusivePid = MyProcPid;
				mustwait = false;
			}
			else
				mustwait = true;
		}
		else
		{
			if (lock->exclusive == 0)
			{
				lock->shared++;
				mustwait = false;
			}
			else
				mustwait = true;
		}

		if (!mustwait)
		{
			LOG_LWDEBUG("LWLockAcquire", lockid, "acquired!");
			break;				/* got the lock */
		}

		/*
		 * Add myself to wait queue.
		 *
		 * If we don't have a PGPROC structure, there's no way to wait. This
		 * should never occur, since MyProc should only be null during shared
		 * memory initialization.
		 */
		if (proc == NULL)
			elog(PANIC, "cannot wait without a PGPROC structure");

		proc->lwWaiting = true;
		proc->lwExclusive = (mode == LW_EXCLUSIVE);
		lwWaitingLockId = lockid;
		proc->lwWaitLink = NULL;
		if (lock->head == NULL)
			lock->head = proc;
		else
			lock->tail->lwWaitLink = proc;
		lock->tail = proc;
		
		/* Can release the mutex now */
		SpinLockRelease(&lock->mutex);

		/*
		 * Wait until awakened.
		 *
		 * Since we share the process wait semaphore with the regular lock
		 * manager and ProcWaitForSignal, and we may need to acquire an LWLock
		 * while one of those is pending, it is possible that we get awakened
		 * for a reason other than being signaled by LWLockRelease. If so,
		 * loop back and wait again.  Once we've gotten the LWLock,
		 * re-increment the sema by the number of additional signals received,
		 * so that the lock manager or signal manager will see the received
		 * signal when it next waits.
		 */
		LOG_LWDEBUG("LWLockAcquire", lockid, "waiting");

#ifdef LWLOCK_TRACE_MIRROREDLOCK
	if (lockid == MirroredLock)
		elog(LOG, "LWLockAcquire: waiting for MirroredLock (PID %u)", MyProcPid);
#endif

#ifdef LWLOCK_STATS
		block_counts[lockid]++;
#endif

		for (c = 0; c < num_held_lwlocks; c++)
		{
			if (held_lwlocks[c] == lockid)
				elog(PANIC, "Waiting on lock already held!");
		}

		TRACE_POSTGRESQL_LWLOCK_WAIT_START(lockid, mode);

		for (;;)
		{
			/* "false" means cannot accept cancel/die interrupt here. */
#ifndef LOCK_DEBUG
			PGSemaphoreLock(&proc->sem, false);
#else
			LWLockTryLockWaiting(proc, lockid, mode);
#endif
			if (!proc->lwWaiting)
				break;
			extraWaits++;
		}

		TRACE_POSTGRESQL_LWLOCK_WAIT_DONE(lockid, mode);

		LOG_LWDEBUG("LWLockAcquire", lockid, "awakened");

#ifdef LWLOCK_TRACE_MIRROREDLOCK
		if (lockid == MirroredLock)
			elog(LOG, "LWLockAcquire: awakened for MirroredLock (PID %u)", MyProcPid);
#endif
		/* Now loop back and try to acquire lock again. */
		retry = true;
	}

	/* We are done updating shared state of the lock itself. */
	SpinLockRelease(&lock->mutex);

	TRACE_POSTGRESQL_LWLOCK_ACQUIRE(lockid, mode);

#ifdef LWLOCK_TRACE_MIRROREDLOCK
	if (lockid == MirroredLock)
		elog(LOG, "LWLockAcquire: MirroredLock by PID %u in held_lwlocks[%d] %s", 
			 MyProcPid, 
			 num_held_lwlocks,
			 (mode == LW_EXCLUSIVE ? "Exclusive" : "Shared"));
#endif

#ifdef USE_TEST_UTILS_X86
	/* keep track of stack trace where lock got acquired */
	held_lwlocks_depth[num_held_lwlocks] =
			gp_backtrace(held_lwlocks_addresses[num_held_lwlocks], MAX_FRAME_DEPTH);
#endif /* USE_TEST_UTILS_X86 */

	/* Add lock to list of locks held by this backend */
	held_lwlocks_exclusive[num_held_lwlocks] = (mode == LW_EXCLUSIVE);
	held_lwlocks[num_held_lwlocks++] = lockid;

	/*
	 * Fix the process wait semaphore's count for any absorbed wakeups.
	 */
	while (extraWaits-- > 0)
		PGSemaphoreUnlock(&proc->sem);
}

/*
 * LWLockConditionalAcquire - acquire a lightweight lock in the specified mode
 *
 * If the lock is not available, return FALSE with no side-effects.
 *
 * If successful, cancel/die interrupts are held off until lock release.
 */
bool
LWLockConditionalAcquire(LWLockId lockid, LWLockMode mode)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	bool		mustwait;

	PRINT_LWDEBUG("LWLockConditionalAcquire", lockid, lock);

	/* Ensure we will have room to remember the lock */
	if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
		elog(ERROR, "too many LWLocks taken");

	/*
	 * Lock out cancel/die interrupts until we exit the code section protected
	 * by the LWLock.  This ensures that interrupts will not interfere with
	 * manipulations of data structures in shared memory.
	 */
	HOLD_INTERRUPTS();

	/* Acquire mutex.  Time spent holding mutex should be short! */
	SpinLockAcquire(&lock->mutex);

	/* If I can get the lock, do so quickly. */
	if (mode == LW_EXCLUSIVE)
	{
		if (lock->exclusive == 0 && lock->shared == 0)
		{
			lock->exclusive++;
			lock->exclusivePid = MyProcPid;
			mustwait = false;
		}
		else
			mustwait = true;
	}
	else
	{
		if (lock->exclusive == 0)
		{
			lock->shared++;
			mustwait = false;
		}
		else
			mustwait = true;
	}

	/* We are done updating shared state of the lock itself. */
	SpinLockRelease(&lock->mutex);

	if (mustwait)
	{
		/* Failed to get lock, so release interrupt holdoff */
		RESUME_INTERRUPTS();
		LOG_LWDEBUG("LWLockConditionalAcquire", lockid, "failed");
		TRACE_POSTGRESQL_LWLOCK_CONDACQUIRE_FAIL(lockid, mode);
	}
	else
	{
#ifdef LWLOCK_TRACE_MIRROREDLOCK
		if (lockid == MirroredLock)
			elog(LOG, "LWLockConditionalAcquire: MirroredLock by PID %u in held_lwlocks[%d] %s", 
				 MyProcPid, 
				 num_held_lwlocks,
				 (mode == LW_EXCLUSIVE ? "Exclusive" : "Shared"));
#endif

#ifdef USE_TEST_UTILS_X86
		/* keep track of stack trace where lock got acquired */
		held_lwlocks_depth[num_held_lwlocks] =
				gp_backtrace(held_lwlocks_addresses[num_held_lwlocks], MAX_FRAME_DEPTH);
#endif /* USE_TEST_UTILS_X86 */

		/* Add lock to list of locks held by this backend */
		held_lwlocks_exclusive[num_held_lwlocks] = (mode == LW_EXCLUSIVE);
		held_lwlocks[num_held_lwlocks++] = lockid;
		TRACE_POSTGRESQL_LWLOCK_CONDACQUIRE(lockid, mode);
	}

	return !mustwait;
}

/*
 * LWLockRelease - release a previously acquired lock
 */
void
LWLockRelease(LWLockId lockid)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	PGPROC	   *head;
	PGPROC	   *proc;
	int			i;
	bool		saveExclusive;

	PRINT_LWDEBUG("LWLockRelease", lockid, lock);

	/*
	 * Remove lock from list of locks held.  Usually, but not always, it will
	 * be the latest-acquired lock; so search array backwards.
	 */
	for (i = num_held_lwlocks; --i >= 0;)
	{
		if (lockid == held_lwlocks[i])
			break;
	}
	if (i < 0)
		elog(ERROR, "lock %d is not held", (int) lockid);

	saveExclusive = held_lwlocks_exclusive[i];
	if (InterruptHoldoffCount <= 0)
		elog(PANIC, "upon entering lock release, the interrupt holdoff count is bad (%d) for release of lock %d (%s)", 
			 InterruptHoldoffCount,
			 (int)lockid,
			 (saveExclusive ? "Exclusive" : "Shared"));

#ifdef LWLOCK_TRACE_MIRROREDLOCK
	if (lockid == MirroredLock)
		elog(LOG, 
			 "LWLockRelease: release for MirroredLock by PID %u in held_lwlocks[%d] %s", 
			 MyProcPid, 
			 i,
			 (held_lwlocks_exclusive[i] ? "Exclusive" : "Shared"));
#endif
	
	num_held_lwlocks--;
	for (; i < num_held_lwlocks; i++)
	{
		held_lwlocks_exclusive[i] = held_lwlocks_exclusive[i + 1];
		held_lwlocks[i] = held_lwlocks[i + 1];
#ifdef USE_TEST_UTILS_X86
		/* shift stack traces */
		held_lwlocks_depth[i] = held_lwlocks_depth[i + 1];
		memcpy
			(
			held_lwlocks_addresses[i],
			held_lwlocks_addresses[i + 1],
			held_lwlocks_depth[i] * sizeof(*held_lwlocks_depth)
			)
			;
#endif /* USE_TEST_UTILS_X86 */
	}

	// Clear out old last entry.
	held_lwlocks_exclusive[num_held_lwlocks] = false;
	held_lwlocks[num_held_lwlocks] = 0;
#ifdef USE_TEST_UTILS_X86
	held_lwlocks_depth[num_held_lwlocks] = 0;
#endif /* USE_TEST_UTILS_X86 */

	/* Acquire mutex.  Time spent holding mutex should be short! */
	SpinLockAcquire(&lock->mutex);

	/* Release my hold on lock */
	if (lock->exclusive > 0)
	{
		lock->exclusive--;
		lock->exclusivePid = 0;
	}
	else
	{
		Assert(lock->shared > 0);
		lock->shared--;
	}

	/*
	 * See if I need to awaken any waiters.  If I released a non-last shared
	 * hold, there cannot be anything to do.  Also, do not awaken any waiters
	 * if someone has already awakened waiters that haven't yet acquired the
	 * lock.
	 */
	head = lock->head;
	if (head != NULL)
	{
		if (lock->exclusive == 0 && lock->shared == 0 && lock->releaseOK)
		{
			/*
			 * Remove the to-be-awakened PGPROCs from the queue.  If the front
			 * waiter wants exclusive lock, awaken him only. Otherwise awaken
			 * as many waiters as want shared access.
			 */
			proc = head;
			if (!proc->lwExclusive)
			{
				while (proc->lwWaitLink != NULL &&
					   !proc->lwWaitLink->lwExclusive)
				{
					proc = proc->lwWaitLink;
					if (proc->pid != 0)
					{
						lock->releaseOK = false;
					}					
				}
			}
			/* proc is now the last PGPROC to be released */
			lock->head = proc->lwWaitLink;
			proc->lwWaitLink = NULL;
			
			/* proc->pid can be 0 if process exited while waiting for lock */
			if (proc->pid != 0)
			{
				/* prevent additional wakeups until retryer gets to run */
				lock->releaseOK = false;
			}
		}
		else
		{
			/* lock is still held, can't awaken anything */
			head = NULL;
		}
	}

	/* We are done updating shared state of the lock itself. */
	SpinLockRelease(&lock->mutex);

	TRACE_POSTGRESQL_LWLOCK_RELEASE(lockid);

	/*
	 * Awaken any waiters I removed from the queue.
	 */
	while (head != NULL)
	{
#ifdef LWLOCK_TRACE_MIRROREDLOCK
		if (lockid == MirroredLock)
			elog(LOG, "LWLockRelease: release waiter for MirroredLock (this PID %u", MyProcPid);
#endif
		LOG_LWDEBUG("LWLockRelease", lockid, "release waiter");
		proc = head;
		head = proc->lwWaitLink;
		proc->lwWaitLink = NULL;
		pg_write_barrier();
		proc->lwWaiting = false;
		PGSemaphoreUnlock(&proc->sem);
	}

	/*
	 * Now okay to allow cancel/die interrupts.
	 */
	if (InterruptHoldoffCount <= 0)
		elog(PANIC, "upon exiting lock release, the interrupt holdoff count is bad (%d) for release of lock %d (%s)", 
			 InterruptHoldoffCount,
			 (int)lockid,
			 (saveExclusive ? "Exclusive" : "Shared"));
	RESUME_INTERRUPTS();
}

/*
 * LWLockWaitCancel - cancel currently waiting on LW lock
 *
 * Used to clean up before immediate exit in certain very special situations
 * like shutdown request to Filerep Resync Manger or Workers. Although this is
 * not the best practice it is necessary to avoid any starvation situations
 * during filerep transition situations (Resync Mode -> Changetracking mode)
 *
 * Note:- This function should not be used for normal situations. It is strictly
 * written for very special situations. If you need to use this, you may want
 * to re-think your design.
 */
void
LWLockWaitCancel(void)
{
	volatile PGPROC *proc = MyProc;
	volatile LWLock *lwWaitingLock = NULL;

	/* We better have a PGPROC structure */
	Assert(proc != NULL);

	/* If we're not waiting on any LWLock then nothing doing here */
	if (!proc->lwWaiting)
		return;

	lwWaitingLock = &(LWLockArray[lwWaitingLockId].lock);

	/* Protect from other modifiers */
	SpinLockAcquire(&lwWaitingLock->mutex);

	PGPROC *currProc = lwWaitingLock->head;

	/* Search our PROC in the waiters list and remove it */
	if (proc == lwWaitingLock->head)
	{
		lwWaitingLock->head = currProc = proc->lwWaitLink;
		proc->lwWaitLink = NULL;
	}
	else
	{
		while(currProc != NULL)
		{
			if (currProc->lwWaitLink == proc)
			{
				currProc->lwWaitLink = proc->lwWaitLink;
				proc->lwWaitLink = NULL;
				break;
			}
			currProc = currProc->lwWaitLink;
		}
	}

	if (lwWaitingLock->tail == proc)
		lwWaitingLock->tail = currProc;

	/* Done with modification */
	SpinLockRelease(&lwWaitingLock->mutex);

	return;
}

/*
 * LWLockReleaseAll - release all currently-held locks
 *
 * Used to clean up after ereport(ERROR). An important difference between this
 * function and retail LWLockRelease calls is that InterruptHoldoffCount is
 * unchanged by this operation.  This is necessary since InterruptHoldoffCount
 * has been set to an appropriate level earlier in error recovery. We could
 * decrement it below zero if we allow it to drop for each released lock!
 */
void
LWLockReleaseAll(void)
{
	while (num_held_lwlocks > 0)
	{
		HOLD_INTERRUPTS();		/* match the upcoming RESUME_INTERRUPTS */

		LWLockRelease(held_lwlocks[num_held_lwlocks - 1]);
	}
}


/*
 * LWLockHeldByMe - test whether my process currently holds a lock
 *
 * This is meant as debug support only.  We do not distinguish whether the
 * lock is held shared or exclusive.
 */
bool
LWLockHeldByMe(LWLockId lockid)
{
	int			i;

	for (i = 0; i < num_held_lwlocks; i++)
	{
		if (held_lwlocks[i] == lockid)
			return true;
	}
	return false;
}

/*
 * LWLockHeldByMe - test whether my process currently holds an exclusive lock
 *
 * This is meant as debug support only.  We do not distinguish whether the
 * lock is held shared or exclusive.
 */
bool
LWLockHeldExclusiveByMe(LWLockId lockid)
{
	int			i;

	for (i = 0; i < num_held_lwlocks; i++)
	{
		if (held_lwlocks[i] == lockid &&
			held_lwlocks_exclusive[i])
			return true;
	}
	return false;
}

#ifdef USE_TEST_UTILS_X86

/*
 * Return number of locks held by my process
 */
uint32
LWLocksHeld()
{
	Assert(num_held_lwlocks >= 0);

	uint32 locks = 0, i = 0;

	for (i = 0; i < num_held_lwlocks; i++)
	{
		if (LWLOCK_IS_PREDEFINED(held_lwlocks[i]))
		{
			locks++;
		}
	}

	return locks;
}


/*
 * Get lock id of the most lately acquired lwlock
 */
LWLockId
LWLockHeldLatestId()
{
	Assert(num_held_lwlocks > 0);

	uint32 i = 0;

	for (i = num_held_lwlocks; i > 0; i--)
	{
		if (LWLOCK_IS_PREDEFINED(held_lwlocks[i - 1]))
		{
			return held_lwlocks[i - 1];
		}
	}

	Assert(!"No predefined lwlock held");
	return MaxDynamicLWLock;
}


/*
 * Get caller address for the most lately acquired lwlock
 */
void *
LWLockHeldLatestCaller()
{
	Assert(num_held_lwlocks > 0);

	uint32 i = 0;

	for (i = num_held_lwlocks; i > 0; i--)
	{
		if (LWLOCK_IS_PREDEFINED(held_lwlocks[i - 1]))
		{
			return held_lwlocks_addresses[i - 1][1];
		}
	}

	return 0;
}


/*
 * Build string containing stack traces where all exclusively-held
 * locks were acquired;
 */
const char*
LWLocksHeldStackTraces()
{
	if (num_held_lwlocks == 0)
	{
		return NULL;
	}

	StringInfo append = makeStringInfo();	/* palloc'd */
	uint32 i = 0, cnt = 1;

	/* append stack trace for each held lock */
	for (i = 0; i < num_held_lwlocks; i++)
	{
		if (!LWLOCK_IS_PREDEFINED(held_lwlocks[i]))
		{
			continue;
		}

		appendStringInfo(append, "%d: LWLock %d:\n", cnt++, held_lwlocks[i] );

		char *stackTrace =
				gp_stacktrace(held_lwlocks_addresses[i], held_lwlocks_depth[i]);

		Assert(stackTrace != NULL);
		appendStringInfoString(append, stackTrace);
		pfree(stackTrace);
	}

	Assert(append->len > 0);
	return append->data;
}

#endif /* USE_TEST_UTILS_X86 */
