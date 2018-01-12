/*-------------------------------------------------------------------------
 *
 * proc.c
 *	  routines to manage per-process shared memory data structure
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/lmgr/proc.c,v 1.207 2009/06/11 14:49:02 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * Interface (a):
 *		ProcSleep(), ProcWakeup(),
 *		ProcQueueAlloc() -- create a shm queue for sleeping processes
 *		ProcQueueInit() -- create a queue without allocing memory
 *
 * Waiting for a lock causes the backend to be put to sleep.  Whoever releases
 * the lock wakes the process up again (and gives it an error code so it knows
 * whether it was awoken on an error condition).
 *
 * Interface (b):
 *
 * ProcReleaseLocks -- frees the locks associated with current transaction
 *
 * ProcKill -- destroys the shared memory state (and locks)
 * associated with the process.
 */
#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "access/transam.h"
#include "access/xact.h"
#include "catalog/namespace.h" /* TempNamespaceOidIsValid */
#include "commands/async.h"
#include "miscadmin.h"
#include "postmaster/autovacuum.h"
#include "postmaster/fts.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "storage/ipc.h"
#include "storage/spin.h"
#include "storage/sinval.h"
#include "storage/lmgr.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/pmsignal.h"
#include "executor/execdesc.h"
#include "utils/resscheduler.h"
#include "utils/timestamp.h"
#include "utils/portal.h"

#include "utils/sharedsnapshot.h"  /*SharedLocalSnapshotSlot*/

#include "cdb/cdblocaldistribxact.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"  /*Gp_is_writer*/
#include "port/atomics.h"
#include "utils/session_state.h"

/* GUC variables */
int			DeadlockTimeout = 1000;
int			StatementTimeout = 0;
int			IdleSessionGangTimeout = 18000;
bool		log_lock_waits = false;

/* Pointer to this process's PGPROC struct, if any */
PGPROC	   *MyProc = NULL;

/* Special for MPP reader gangs */
PGPROC	   *lockHolderProcPtr = NULL;

/*
 * This spinlock protects the freelist of recycled PGPROC structures.
 * We cannot use an LWLock because the LWLock manager depends on already
 * having a PGPROC and a wait semaphore!  But these structures are touched
 * relatively infrequently (only at backend startup or shutdown) and not for
 * very long, so a spinlock is okay.
 */
NON_EXEC_STATIC slock_t *ProcStructLock = NULL;

/* Pointers to shared-memory structures */
NON_EXEC_STATIC PROC_HDR *ProcGlobal = NULL;
NON_EXEC_STATIC PGPROC *AuxiliaryProcs = NULL;

/* If we are waiting for a lock, this points to the associated LOCALLOCK */
static LOCALLOCK *lockAwaited = NULL;

/* Mark these volatile because they can be changed by signal handler */
static volatile bool statement_timeout_active = false;
static volatile bool deadlock_timeout_active = false;
static volatile DeadLockState deadlock_state = DS_NOT_YET_CHECKED;
static volatile sig_atomic_t clientWaitTimeoutInterruptEnabled = 0;
static volatile sig_atomic_t clientWaitTimeoutInterruptOccurred = 0;
volatile bool cancel_from_timeout = false;

/* timeout_start_time is set when log_lock_waits is true */
static TimestampTz timeout_start_time;

/* statement_fin_time is valid only if statement_timeout_active is true */
static TimestampTz statement_fin_time;

static void RemoveProcFromArray(int code, Datum arg);
static void ProcKill(int code, Datum arg);
static void AuxiliaryProcKill(int code, Datum arg);
static bool CheckStatementTimeout(void);
static void ClientWaitTimeoutInterruptHandler(void);
static void ProcessClientWaitTimeout(void);


/*
 * Report shared-memory space needed by InitProcGlobal.
 */
Size
ProcGlobalShmemSize(void)
{
	Size		size = 0;

	/* ProcGlobal */
	size = add_size(size, sizeof(PROC_HDR));
	/* AuxiliaryProcs */
	size = add_size(size, mul_size(NUM_AUXILIARY_PROCS, sizeof(PGPROC)));
	/* MyProcs, including autovacuum */
	size = add_size(size, mul_size(MaxBackends, sizeof(PGPROC)));
	/* ProcStructLock */
	size = add_size(size, sizeof(slock_t));

	return size;
}

/*
 * Report number of semaphores needed by InitProcGlobal.
 */
int
ProcGlobalSemas(void)
{
	/*
	 * We need a sema per backend (including autovacuum), plus one for each
	 * auxiliary process.
	 */
	return MaxBackends + NUM_AUXILIARY_PROCS;
}

/*
 * InitProcGlobal -
 *	  Initialize the global process table during postmaster or standalone
 *	  backend startup.
 *
 *	  We also create all the per-process semaphores we will need to support
 *	  the requested number of backends.  We used to allocate semaphores
 *	  only when backends were actually started up, but that is bad because
 *	  it lets Postgres fail under load --- a lot of Unix systems are
 *	  (mis)configured with small limits on the number of semaphores, and
 *	  running out when trying to start another backend is a common failure.
 *	  So, now we grab enough semaphores to support the desired max number
 *	  of backends immediately at initialization --- if the sysadmin has set
 *	  MaxConnections or autovacuum_max_workers higher than his kernel will
 *	  support, he'll find out sooner rather than later.
 *
 *	  Another reason for creating semaphores here is that the semaphore
 *	  implementation typically requires us to create semaphores in the
 *	  postmaster, not in backends.
 *
 * Note: this is NOT called by individual backends under a postmaster,
 * not even in the EXEC_BACKEND case.  The ProcGlobal and AuxiliaryProcs
 * pointers must be propagated specially for EXEC_BACKEND operation.
 */
void
InitProcGlobal(void)
{
	PGPROC	   *procs;
	int			i;
	bool		found;

	/* Create the ProcGlobal shared structure */
	ProcGlobal = (PROC_HDR *)
		ShmemInitStruct("Proc Header", sizeof(PROC_HDR), &found);
	Assert(!found);

	/*
	 * Create the PGPROC structures for auxiliary (bgwriter) processes, too.
	 * These do not get linked into the freeProcs list.
	 */
	AuxiliaryProcs = (PGPROC *)
		ShmemInitStruct("AuxiliaryProcs", NUM_AUXILIARY_PROCS * sizeof(PGPROC),
						&found);
	Assert(!found);

	/*
	 * Initialize the data structures.
	 */
	ProcGlobal->freeProcs = NULL;
	ProcGlobal->autovacFreeProcs = NULL;

	ProcGlobal->spins_per_delay = DEFAULT_SPINS_PER_DELAY;

	ProcGlobal->mppLocalProcessCounter = 0;

	/*
	 * Pre-create the PGPROC structures and create a semaphore for each.
	 */
	procs = (PGPROC *) ShmemAlloc((MaxConnections) * sizeof(PGPROC));
	if (!procs)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of shared memory")));
	MemSet(procs, 0, MaxConnections * sizeof(PGPROC));
	for (i = 0; i < MaxConnections; i++)
	{
		PGSemaphoreCreate(&(procs[i].sem));
		InitSharedLatch(&(procs[i].procLatch));

		procs[i].links.next = (SHM_QUEUE *) ProcGlobal->freeProcs;
		ProcGlobal->freeProcs = &procs[i];
	}
	ProcGlobal->procs = procs;
	ProcGlobal->numFreeProcs = MaxConnections;

	procs = (PGPROC *) ShmemAlloc((autovacuum_max_workers) * sizeof(PGPROC));
	if (!procs)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of shared memory")));
	MemSet(procs, 0, autovacuum_max_workers * sizeof(PGPROC));
	for (i = 0; i < autovacuum_max_workers; i++)
	{
		PGSemaphoreCreate(&(procs[i].sem));
		InitSharedLatch(&(procs[i].procLatch));
		procs[i].links.next = (SHM_QUEUE *) ProcGlobal->autovacFreeProcs;
		ProcGlobal->autovacFreeProcs = &procs[i];
	}

	MemSet(AuxiliaryProcs, 0, NUM_AUXILIARY_PROCS * sizeof(PGPROC));
	for (i = 0; i < NUM_AUXILIARY_PROCS; i++)
	{
		AuxiliaryProcs[i].pid = 0;		/* marks auxiliary proc as not in use */
		PGSemaphoreCreate(&(AuxiliaryProcs[i].sem));
		InitSharedLatch(&(AuxiliaryProcs[i].procLatch));
	}

	/* Create ProcStructLock spinlock, too */
	ProcStructLock = (slock_t *) ShmemAlloc(sizeof(slock_t));
	SpinLockInit(ProcStructLock);
}

/*
 * InitProcess -- initialize a per-process data structure for this backend
 */
void
InitProcess(void)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile PROC_HDR *procglobal = ProcGlobal;
	int			i;

	/*
	 * Autovacuum, WAL sender and FTS handler processes are marked as
	 * GP_ROLE_UTILITY to prevent unwanted GP_ROLE_DISPATCH MyProc settings
	 * such as mppSessionId being valid and mppIsWriter set to true.
	 */
	if (IsAutoVacuumWorkerProcess() || am_walsender || am_ftshandler)
		Gp_role = GP_ROLE_UTILITY;

	/*
	 * ProcGlobal should be set up already (if we are a backend, we inherit
	 * this by fork() or EXEC_BACKEND mechanism from the postmaster).
	 */
	if (procglobal == NULL)
		elog(PANIC, "proc header uninitialized");

	if (MyProc != NULL)
		elog(ERROR, "you already exist");

	/*
	 * Initialize process-local latch support.  This could fail if the kernel
	 * is low on resources, and if so we want to exit cleanly before acquiring
	 * any shared-memory resources.
	 */
	InitializeLatchSupport();

	/*
	 * Try to get a proc struct from the free list.  If this fails, we must be
	 * out of PGPROC structures (not to mention semaphores).
	 *
	 * While we are holding the ProcStructLock, also copy the current shared
	 * estimate of spins_per_delay to local storage.
	 */
	SpinLockAcquire(ProcStructLock);

	set_spins_per_delay(procglobal->spins_per_delay);

	if (IsAutoVacuumWorkerProcess())
		MyProc = procglobal->autovacFreeProcs;
	else
		MyProc = procglobal->freeProcs;

	if (MyProc != NULL)
	{
		if (IsAutoVacuumWorkerProcess())
			procglobal->autovacFreeProcs = (PGPROC *) MyProc->links.next;
		else
			procglobal->freeProcs = (PGPROC *) MyProc->links.next;

		procglobal->numFreeProcs--;		/* we removed an entry from the list. */
		Assert(procglobal->numFreeProcs >= 0);

		SpinLockRelease(ProcStructLock);
	}
	else
	{
		/*
		 * If we reach here, all the PGPROCs are in use.  This is one of the
		 * possible places to detect "too many backends", so give the standard
		 * error message.  XXX do we need to give a different failure message
		 * in the autovacuum case?
		 */
		SpinLockRelease(ProcStructLock);
		ereport(FATAL,
				(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
				 errmsg("sorry, too many clients already")));
	}

	if (gp_debug_pgproc)
	{
		elog(LOG, "allocating PGPROC entry for pid %d, freeProcs (prev ptr, new ptr): (%p, %p)",
			 MyProcPid, MyProc, MyProc->links.next);
	}

	int mppLocalProcessSerial = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)&procglobal->mppLocalProcessCounter, 1);

	lockHolderProcPtr = MyProc;

	/* Set the next pointer to NULL */
	MyProc->links.next = NULL;

	/*
	 * Now that we have a PGPROC, mark ourselves as an active postmaster
	 * child; this is so that the postmaster can detect it if we exit without
	 * cleaning up.  (XXX autovac launcher currently doesn't participate in
	 * this; it probably should.)
	 *
	 * Ideally, we should create functions similar to IsAutoVacuumWorkerProcess()
	 * for ftsProber, SeqServer etc who call InitProcess().
	 * But MyPMChildSlot helps to get away with it.
	 */
	if (IsUnderPostmaster && !IsAutoVacuumWorkerProcess()
		&& MyPMChildSlot > 0)
		MarkPostmasterChildActive();

	/*
	 * Initialize all fields of MyProc, except for the semaphore which was
	 * prepared for us by InitProcGlobal.
	 */
	SHMQueueElemInit(&(MyProc->links));
	MyProc->waitStatus = STATUS_OK;
	MyProc->lxid = InvalidLocalTransactionId;
	MyProc->xid = InvalidTransactionId;
	MyProc->localDistribXactData.state = LOCALDISTRIBXACT_STATE_NONE;
	MyProc->xmin = InvalidTransactionId;
	MyProc->serializableIsoLevel = false;
	MyProc->inDropTransaction = false;
	MyProc->pid = MyProcPid;
	/* backendId, databaseId and roleId will be filled in later */
	MyProc->backendId = InvalidBackendId;
	MyProc->databaseId = InvalidOid;
	MyProc->roleId = InvalidOid;
	MyProc->inCommit = false;
	MyProc->vacuumFlags = 0;
	if (IsAutoVacuumWorkerProcess())
		MyProc->vacuumFlags |= PROC_IS_AUTOVACUUM;
	MyProc->lwWaiting = false;
	MyProc->lwExclusive = false;
	MyProc->lwWaitLink = NULL;
	MyProc->waitLock = NULL;
	MyProc->waitProcLock = NULL;
	MyProc->resSlot = NULL;
	for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
		SHMQueueInit(&(MyProc->myProcLocks[i]));

    /* 
     * mppLocalProcessSerial uniquely identifies this backend process among
     * all those that our parent postmaster process creates over its lifetime. 
     *
  	 * Since we use the process serial number to decide if we should
	 * deliver a response from a server under this spin, we need to 
	 * assign it under the spin lock.
	 */
    MyProc->mppLocalProcessSerial = mppLocalProcessSerial;

    /* 
     * A nonzero gp_session_id uniquely identifies an MPP client session 
     * over the lifetime of the entry postmaster process. A qDisp passes
     * its gp_session_id down to all of its qExecs. If this is a qExec,
     * we have already received the gp_session_id from the qDisp.
     */
    if (Gp_role == GP_ROLE_DISPATCH && gp_session_id == -1)
        gp_session_id = mppLocalProcessSerial;
    MyProc->mppSessionId = gp_session_id;
    elog(DEBUG1,"InitProcess(): gp_session_id %d, Gp_role %d",gp_session_id, Gp_role);
    
    MyProc->mppIsWriter = Gp_is_writer;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		MyProc->mppIsWriter = true;
	}
    
	/* Initialize fields for sync rep */
	MyProc->waitLSN.xlogid = 0;
	MyProc->waitLSN.xrecoff = 0;
	MyProc->syncRepState = SYNC_REP_NOT_WAITING;
	SHMQueueElemInit(&(MyProc->syncRepLinks));

	/*
	 * Acquire ownership of the PGPROC's latch, so that we can use WaitLatch.
	 * Note that there's no particular need to do ResetLatch here.
	 */
	OwnLatch(&MyProc->procLatch);

	/*
	 * We might be reusing a semaphore that belonged to a failed process. So
	 * be careful and reinitialize its value here.	(This is not strictly
	 * necessary anymore, but seems like a good idea for cleanliness.)
	 */
	PGSemaphoreReset(&MyProc->sem);

	/* Set wait portal (do not check if resource scheduling is enabled) */
	MyProc->waitPortalId = INVALID_PORTALID;

	MyProc->queryCommandId = -1;

	/*
	 * Arrange to clean up at backend exit.
	 */
	on_shmem_exit(ProcKill, 0);

	/*
	 * Now that we have a PGPROC, we could try to acquire locks, so initialize
	 * the deadlock checker.
	 */
	InitDeadLockChecking();
}

/*
 * InitProcessPhase2 -- make MyProc visible in the shared ProcArray.
 *
 * This is separate from InitProcess because we can't acquire LWLocks until
 * we've created a PGPROC, but in the EXEC_BACKEND case there is a good deal
 * of stuff to be done before this step that will require LWLock access.
 */
void
InitProcessPhase2(void)
{
	Assert(MyProc != NULL);

	/*
	 * Add our PGPROC to the PGPROC array in shared memory.
	 */
	ProcArrayAdd(MyProc);

	/*
	 * Arrange to clean that up at backend exit.
	 */
	on_shmem_exit(RemoveProcFromArray, 0);
}

/*
 * InitAuxiliaryProcess -- create a per-auxiliary-process data structure
 *
 * This is called by bgwriter and similar processes so that they will have a
 * MyProc value that's real enough to let them wait for LWLocks.  The PGPROC
 * and sema that are assigned are one of the extra ones created during
 * InitProcGlobal.
 *
 * Auxiliary processes are presently not expected to wait for real (lockmgr)
 * locks, so we need not set up the deadlock checker.  They are never added
 * to the ProcArray or the sinval messaging mechanism, either.	They also
 * don't get a VXID assigned, since this is only useful when we actually
 * hold lockmgr locks.
 */
void
InitAuxiliaryProcess(void)
{
	PGPROC	   *auxproc;
	int			proctype;
	int			i;

	/*
	 * ProcGlobal should be set up already (if we are a backend, we inherit
	 * this by fork() or EXEC_BACKEND mechanism from the postmaster).
	 */
	if (ProcGlobal == NULL || AuxiliaryProcs == NULL)
		elog(PANIC, "proc header uninitialized");

	if (MyProc != NULL)
		elog(ERROR, "you already exist");

	/*
	 * Initialize process-local latch support.  This could fail if the kernel
	 * is low on resources, and if so we want to exit cleanly before acquiring
	 * any shared-memory resources.
	 */
	InitializeLatchSupport();

	/*
	 * We use the ProcStructLock to protect assignment and releasing of
	 * AuxiliaryProcs entries.
	 *
	 * While we are holding the ProcStructLock, also copy the current shared
	 * estimate of spins_per_delay to local storage.
	 */
	SpinLockAcquire(ProcStructLock);

	set_spins_per_delay(ProcGlobal->spins_per_delay);

	/*
	 * Find a free auxproc ... *big* trouble if there isn't one ...
	 */
	for (proctype = 0; proctype < NUM_AUXILIARY_PROCS; proctype++)
	{
		auxproc = &AuxiliaryProcs[proctype];
		if (auxproc->pid == 0)
			break;
	}
	if (proctype >= NUM_AUXILIARY_PROCS)
	{
		SpinLockRelease(ProcStructLock);
		elog(FATAL, "all AuxiliaryProcs are in use");
	}

	/* Mark auxiliary proc as in use by me */
	/* use volatile pointer to prevent code rearrangement */
	((volatile PGPROC *) auxproc)->pid = MyProcPid;

	MyProc = auxproc;
	lockHolderProcPtr = auxproc;

	SpinLockRelease(ProcStructLock);

	/*
	 * Initialize all fields of MyProc, except for the semaphore which was
	 * prepared for us by InitProcGlobal.
	 */
	SHMQueueElemInit(&(MyProc->links));
	MyProc->waitStatus = STATUS_OK;
	MyProc->lxid = InvalidLocalTransactionId;
	MyProc->xid = InvalidTransactionId;
	MyProc->localDistribXactData.state = LOCALDISTRIBXACT_STATE_NONE;
	MyProc->xmin = InvalidTransactionId;
	MyProc->serializableIsoLevel = false;
	MyProc->inDropTransaction = false;
	MyProc->databaseId = InvalidOid;
	MyProc->roleId = InvalidOid;
    MyProc->mppLocalProcessSerial = 0;
    MyProc->mppSessionId = 0;
    MyProc->mppIsWriter = false;
	MyProc->inCommit = false;
	MyProc->vacuumFlags = 0;
	MyProc->lwWaiting = false;
	MyProc->lwExclusive = false;
	MyProc->lwWaitLink = NULL;
	MyProc->waitLock = NULL;
	MyProc->waitProcLock = NULL;
	for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
		SHMQueueInit(&(MyProc->myProcLocks[i]));

	/*
	 * Auxiliary process doesn't bother with sync rep.  Though it was
	 * originally supposed to not do transaction work, but it does in GPDB,
	 * we mark it and avoid sync rep work.
	 */
	MyProc->syncRepState = SYNC_REP_DISABLED;

	/*
	 * Acquire ownership of the PGPROC's latch, so that we can use WaitLatch.
	 * Note that there's no particular need to do ResetLatch here.
	 */
	OwnLatch(&MyProc->procLatch);

	/*
	 * We might be reusing a semaphore that belonged to a failed process. So
	 * be careful and reinitialize its value here.	(This is not strictly
	 * necessary anymore, but seems like a good idea for cleanliness.)
	 */
	PGSemaphoreReset(&MyProc->sem);

	MyProc->queryCommandId = -1;

	/*
	 * Arrange to clean up at process exit.
	 */
	on_shmem_exit(AuxiliaryProcKill, Int32GetDatum(proctype));
}

/*
 * Check whether there are at least N free PGPROC objects.
 */
bool
HaveNFreeProcs(int n)
{
	Assert(n >= 0);

	return (ProcGlobal->numFreeProcs >= n);
}

/*
 * Cancel any pending wait for lock, when aborting a transaction.
 *
 * (Normally, this would only happen if we accept a cancel/die
 * interrupt while waiting; but an ereport(ERROR) while waiting is
 * within the realm of possibility, too.)
 */
void
LockWaitCancel(void)
{
	LWLockId	partitionLock;

	/* Nothing to do if we weren't waiting for a lock */
	if (lockAwaited == NULL)
		return;

	/* Don't try to cancel resource locks.*/
	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled() &&
		LOCALLOCK_LOCKMETHOD(*lockAwaited) == RESOURCE_LOCKMETHOD)
		return;

	/* Turn off the deadlock timer, if it's still running (see ProcSleep) */
	disable_sig_alarm(false);

	/* Unlink myself from the wait queue, if on it (might not be anymore!) */
	partitionLock = LockHashPartitionLock(lockAwaited->hashcode);
	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	if (MyProc->links.next != NULL)
	{
		/* We could not have been granted the lock yet */
		RemoveFromWaitQueue(MyProc, lockAwaited->hashcode);
	}
	else
	{
		/*
		 * Somebody kicked us off the lock queue already.  Perhaps they
		 * granted us the lock, or perhaps they detected a deadlock. If they
		 * did grant us the lock, we'd better remember it in our local lock
		 * table.
		 */
		if (MyProc->waitStatus == STATUS_OK)
			GrantAwaitedLock();
	}

	lockAwaited = NULL;

	LWLockRelease(partitionLock);

	/*
	 * We used to do PGSemaphoreReset() here to ensure that our proc's wait
	 * semaphore is reset to zero.	This prevented a leftover wakeup signal
	 * from remaining in the semaphore if someone else had granted us the lock
	 * we wanted before we were able to remove ourselves from the wait-list.
	 * However, now that ProcSleep loops until waitStatus changes, a leftover
	 * wakeup signal isn't harmful, and it seems not worth expending cycles to
	 * get rid of a signal that most likely isn't there.
	 */
}


/*
 * ProcReleaseLocks() -- release locks associated with current transaction
 *			at main transaction commit or abort
 *
 * At main transaction commit, we release all locks except session locks.
 * At main transaction abort, we release all locks including session locks;
 * this lets us clean up after a VACUUM FULL failure.
 *
 * At subtransaction commit, we don't release any locks (so this func is not
 * needed at all); we will defer the releasing to the parent transaction.
 * At subtransaction abort, we release all locks held by the subtransaction;
 * this is implemented by retail releasing of the locks under control of
 * the ResourceOwner mechanism.
 *
 * Note that user locks are not released in any case.
 */
void
ProcReleaseLocks(bool isCommit)
{
	if (!MyProc)
		return;
	/* If waiting, get off wait queue (should only be needed after error) */
	LockWaitCancel();
	/* Release locks */
	LockReleaseAll(DEFAULT_LOCKMETHOD, !isCommit);
}


/*
 * RemoveProcFromArray() -- Remove this process from the shared ProcArray.
 */
static void
RemoveProcFromArray(int code, Datum arg)
{
	Assert(MyProc != NULL);
	ProcArrayRemove(MyProc, InvalidTransactionId);
}

/*
 * update_spins_per_delay
 *   Update spins_per_delay value in ProcGlobal.
 */
static void update_spins_per_delay()
{
	volatile PROC_HDR *procglobal = ProcGlobal;
	bool casResult = false;

	while (!casResult)
	{
		int old_spins_per_delay = procglobal->spins_per_delay;
		int new_spins_per_delay = recompute_spins_per_delay(old_spins_per_delay);
		casResult = pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&procglobal->spins_per_delay,
										(uint32 *)&old_spins_per_delay,
										new_spins_per_delay);
	}
}

/*
 * ProcKill() -- Destroy the per-proc data structure for
 *		this process. Release any of its held LW locks.
 */
static void
ProcKill(int code, Datum arg)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile PROC_HDR *procglobal = ProcGlobal;
	PGPROC	   *proc;
	Assert(MyProc != NULL);

	/* Make sure we're out of the sync rep lists */
	SyncRepCleanupAtProcExit();

	/* 
	 * Cleanup for any resource locks on portals - from holdable cursors or
	 * unclean process abort (assertion failures).
	 */
	if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled())
		AtExitCleanup_ResPortals();

	/*
	 * Remove the shared snapshot slot.
	 */
	if (SharedLocalSnapshotSlot != NULL)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			SharedSnapshotRemove(SharedLocalSnapshotSlot,
								 "Query Dispatcher");
		}
	    else if (Gp_segment == -1 && Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
	    {
			/* 
			 * Entry db singleton QE is a user of the shared snapshot -- not a creator.
			 */	
	    }
		else if (Gp_role == GP_ROLE_EXECUTE && Gp_is_writer)
		{
			SharedSnapshotRemove(SharedLocalSnapshotSlot,
								 "Writer qExec");
		}
		SharedLocalSnapshotSlot = NULL;
	}

	/*
	 * Release any LW locks I am holding.  There really shouldn't be any, but
	 * it's cheap to check again before we cut the knees off the LWLock
	 * facility by releasing our PGPROC ...
	 */
	LWLockReleaseAll();

	MyProc->localDistribXactData.state = LOCALDISTRIBXACT_STATE_NONE;
    MyProc->mppLocalProcessSerial = 0;
    MyProc->mppSessionId = 0;
    MyProc->mppIsWriter = false;
	MyProc->pid = 0;

	/*
	 * Clear MyProc first; then disown the process latch.  This is so that
	 * signal handlers won't try to clear the process latch after it's no
	 * longer ours.
	 */
	proc = MyProc;
	MyProc = NULL;
	DisownLatch(&proc->procLatch);

	SpinLockAcquire(ProcStructLock);

	/* Return PGPROC structure (and semaphore) to freelist */
	if (IsAutoVacuumWorkerProcess())
	{
		proc->links.next = (SHM_QUEUE *) procglobal->autovacFreeProcs;
		procglobal->autovacFreeProcs = proc;
	}
	else
	{
		proc->links.next = (SHM_QUEUE *) procglobal->freeProcs;
		procglobal->freeProcs = proc;
	}

	procglobal->numFreeProcs++;	/* we added an entry */

	/* Update shared estimate of spins_per_delay */
	update_spins_per_delay();

	SpinLockRelease(ProcStructLock);

	/*
	 * This process is no longer present in shared memory in any meaningful
	 * way, so tell the postmaster we've cleaned up acceptably well.
	 */
	if (IsUnderPostmaster && !IsAutoVacuumWorkerProcess()
		&& MyPMChildSlot > 0)
		MarkPostmasterChildInactive();

	/* wake autovac launcher if needed -- see comments in FreeWorkerInfo */
	if (AutovacuumLauncherPid != 0)
		kill(AutovacuumLauncherPid, SIGUSR1);
}

/*
 * AuxiliaryProcKill() -- Cut-down version of ProcKill for auxiliary
 *		processes (bgwriter, etc).	The PGPROC and sema are not released, only
 *		marked as not-in-use.
 */
static void
AuxiliaryProcKill(int code, Datum arg)
{
	int			proctype = DatumGetInt32(arg);
	PGPROC	   *auxproc;

	Assert(proctype >= 0 && proctype < NUM_AUXILIARY_PROCS);

	auxproc = &AuxiliaryProcs[proctype];

	Assert(MyProc == auxproc);

	/* Release any LW locks I am holding (see notes above) */
	LWLockReleaseAll();

	/* Release ownership of the process's latch, too */
	DisownLatch(&MyProc->procLatch);

	SpinLockAcquire(ProcStructLock);

	/* Mark auxiliary proc no longer in use */
	MyProc->pid = 0;

	/* Update shared estimate of spins_per_delay */
	update_spins_per_delay();

	SpinLockRelease(ProcStructLock);

	/*
	 * If the parent process of this auxiliary process does not exist, we
	 * have trouble. Besides the obvious case that the postmaster is gone,
	 * this could happen to filerep subprocesses when the filerep main
	 * process dies unexpectedly. The postmaster will receive the SIGCHLD
	 * signal when we exit in that case. Make sure we exit with non-zero (and
	 * not 1 either) exit status, to force the postmaster to reset the system
	 * if that happens.
	 */
	if (!ParentProcIsAlive())
		proc_exit(10);

	/* PGPROC struct isn't mine anymore */
	MyProc = NULL;
	lockHolderProcPtr = NULL;
}


/*
 * ProcQueue package: routines for putting processes to sleep
 *		and  waking them up
 */

/*
 * ProcQueueAlloc -- alloc/attach to a shared memory process queue
 *
 * Returns: a pointer to the queue or NULL
 * Side Effects: Initializes the queue if we allocated one
 */
#ifdef NOT_USED
PROC_QUEUE *
ProcQueueAlloc(char *name)
{
	bool		found;
	PROC_QUEUE *queue = (PROC_QUEUE *)
	ShmemInitStruct(name, sizeof(PROC_QUEUE), &found);

	if (!queue)
		return NULL;
	if (!found)
		ProcQueueInit(queue);
	return queue;
}
#endif

/*
 * ProcQueueInit -- initialize a shared memory process queue
 */
void
ProcQueueInit(PROC_QUEUE *queue)
{
	SHMQueueInit(&(queue->links));
	queue->size = 0;
}


/*
 * ProcSleep -- put a process to sleep on the specified lock
 *
 * Caller must have set MyProc->heldLocks to reflect locks already held
 * on the lockable object by this process (under all XIDs).
 *
 * The lock table's partition lock must be held at entry, and will be held
 * at exit.
 *
 * Result: STATUS_OK if we acquired the lock, STATUS_ERROR if not (deadlock).
 *
 * ASSUME: that no one will fiddle with the queue until after
 *		we release the partition lock.
 *
 * NOTES: The process queue is now a priority queue for locking.
 *
 * P() on the semaphore should put us to sleep.  The process
 * semaphore is normally zero, so when we try to acquire it, we sleep.
 */
int
ProcSleep(LOCALLOCK *locallock, LockMethod lockMethodTable)
{
	LOCKMODE	lockmode = locallock->tag.mode;
	LOCK	   *lock = locallock->lock;
	PROCLOCK   *proclock = locallock->proclock;
	uint32		hashcode = locallock->hashcode;
	LWLockId	partitionLock = LockHashPartitionLock(hashcode);
	PROC_QUEUE *waitQueue = &(lock->waitProcs);
	LOCKMASK	myHeldLocks = MyProc->heldLocks;
	bool		early_deadlock = false;
	bool		allow_autovacuum_cancel = true;
	int			myWaitStatus;
	PGPROC	   *proc;
	int			i;

	/*
	 * Determine where to add myself in the wait queue.
	 *
	 * Normally I should go at the end of the queue.  However, if I already
	 * hold locks that conflict with the request of any previous waiter, put
	 * myself in the queue just in front of the first such waiter. This is not
	 * a necessary step, since deadlock detection would move me to before that
	 * waiter anyway; but it's relatively cheap to detect such a conflict
	 * immediately, and avoid delaying till deadlock timeout.
	 *
	 * Special case: if I find I should go in front of some waiter, check to
	 * see if I conflict with already-held locks or the requests before that
	 * waiter.	If not, then just grant myself the requested lock immediately.
	 * This is the same as the test for immediate grant in LockAcquire, except
	 * we are only considering the part of the wait queue before my insertion
	 * point.
	 */
	if (myHeldLocks != 0)
	{
		LOCKMASK	aheadRequests = 0;

		proc = (PGPROC *) waitQueue->links.next;
		for (i = 0; i < waitQueue->size; i++)
		{
			/* Must he wait for me? */
			if (lockMethodTable->conflictTab[proc->waitLockMode] & myHeldLocks)
			{
				/* Must I wait for him ? */
				if (lockMethodTable->conflictTab[lockmode] & proc->heldLocks)
				{
					/*
					 * Yes, so we have a deadlock.	Easiest way to clean up
					 * correctly is to call RemoveFromWaitQueue(), but we
					 * can't do that until we are *on* the wait queue. So, set
					 * a flag to check below, and break out of loop.  Also,
					 * record deadlock info for later message.
					 */
					RememberSimpleDeadLock(MyProc, lockmode, lock, proc);
					early_deadlock = true;
					break;
				}
				/* I must go before this waiter.  Check special case. */
				if ((lockMethodTable->conflictTab[lockmode] & aheadRequests) == 0 &&
					LockCheckConflicts(lockMethodTable,
									   lockmode,
									   lock,
									   proclock,
									   MyProc) == STATUS_OK)
				{
					/* Skip the wait and just grant myself the lock. */
					GrantLock(lock, proclock, lockmode);
					GrantAwaitedLock();
					return STATUS_OK;
				}
				/* Break out of loop to put myself before him */
				break;
			}
			/* Nope, so advance to next waiter */
			aheadRequests |= LOCKBIT_ON(proc->waitLockMode);
			proc = (PGPROC *) proc->links.next;
		}

		/*
		 * If we fall out of loop normally, proc points to waitQueue head, so
		 * we will insert at tail of queue as desired.
		 */
	}
	else
	{
		/* I hold no locks, so I can't push in front of anyone. */
		proc = (PGPROC *) &(waitQueue->links);
	}

	/*
	 * Insert self into queue, ahead of the given proc (or at tail of queue).
	 */
	SHMQueueInsertBefore(&(proc->links), &(MyProc->links));
	waitQueue->size++;

	lock->waitMask |= LOCKBIT_ON(lockmode);

	/* Set up wait information in PGPROC object, too */
	MyProc->waitLock = lock;
	MyProc->waitProcLock = proclock;
	MyProc->waitLockMode = lockmode;

	MyProc->waitStatus = STATUS_WAITING;

	/*
	 * If we detected deadlock, give up without waiting.  This must agree with
	 * CheckDeadLock's recovery code, except that we shouldn't release the
	 * semaphore since we haven't tried to lock it yet.
	 */
	if (early_deadlock)
	{
		RemoveFromWaitQueue(MyProc, hashcode);
		return STATUS_ERROR;
	}

	/* mark that we are waiting for a lock */
	lockAwaited = locallock;

	/*
	 * Release the lock table's partition lock.
	 *
	 * NOTE: this may also cause us to exit critical-section state, possibly
	 * allowing a cancel/die interrupt to be accepted. This is OK because we
	 * have recorded the fact that we are waiting for a lock, and so
	 * LockWaitCancel will clean up if cancel/die happens.
	 */
	LWLockRelease(partitionLock);

	/* Reset deadlock_state before enabling the signal handler */
	deadlock_state = DS_NOT_YET_CHECKED;

	/*
	 * Set timer so we can wake up after awhile and check for a deadlock. If a
	 * deadlock is detected, the handler releases the process's semaphore and
	 * sets MyProc->waitStatus = STATUS_ERROR, allowing us to know that we
	 * must report failure rather than success.
	 *
	 * By delaying the check until we've waited for a bit, we can avoid
	 * running the rather expensive deadlock-check code in most cases.
	 */
	if (!enable_sig_alarm(DeadlockTimeout, false))
		elog(FATAL, "could not set timer for process wakeup");

	/*
	 * If someone wakes us between LWLockRelease and PGSemaphoreLock,
	 * PGSemaphoreLock will not block.	The wakeup is "saved" by the semaphore
	 * implementation.	While this is normally good, there are cases where a
	 * saved wakeup might be leftover from a previous operation (for example,
	 * we aborted ProcWaitForSignal just before someone did ProcSendSignal).
	 * So, loop to wait again if the waitStatus shows we haven't been granted
	 * nor denied the lock yet.
	 *
	 * We pass interruptOK = true, which eliminates a window in which
	 * cancel/die interrupts would be held off undesirably.  This is a promise
	 * that we don't mind losing control to a cancel/die interrupt here.  We
	 * don't, because we have no shared-state-change work to do after being
	 * granted the lock (the grantor did it all).  We do have to worry about
	 * updating the locallock table, but if we lose control to an error,
	 * LockWaitCancel will fix that up.
	 */
	do
	{
		PGSemaphoreLock(&MyProc->sem, true);

		/*
		 * waitStatus could change from STATUS_WAITING to something else
		 * asynchronously.	Read it just once per loop to prevent surprising
		 * behavior (such as missing log messages).
		 */
		myWaitStatus = MyProc->waitStatus;

		/*
		 * If we are not deadlocked, but are waiting on an autovacuum-induced
		 * task, send a signal to interrupt it.
		 */
		if (deadlock_state == DS_BLOCKED_BY_AUTOVACUUM && allow_autovacuum_cancel)
		{
			PGPROC	   *autovac = GetBlockingAutoVacuumPgproc();

			LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

			/*
			 * Only do it if the worker is not working to protect against Xid
			 * wraparound.
			 */
			if ((autovac != NULL) &&
				(autovac->vacuumFlags & PROC_IS_AUTOVACUUM) &&
				!(autovac->vacuumFlags & PROC_VACUUM_FOR_WRAPAROUND))
			{
				int			pid = autovac->pid;
				StringInfoData locktagbuf;
				StringInfoData logbuf;		/* errdetail for server log */

				initStringInfo(&locktagbuf);
				initStringInfo(&logbuf);
				DescribeLockTag(&locktagbuf, &lock->tag);
				appendStringInfo(&logbuf,
					  _("Process %d waits for %s on %s."),
						 MyProcPid,
						 GetLockmodeName(lock->tag.locktag_lockmethodid,
										 lockmode),
						 locktagbuf.data);

				/* release lock as quickly as possible */
				LWLockRelease(ProcArrayLock);

				ereport(LOG,
						(errmsg("sending cancel to blocking autovacuum PID %d",
							pid),
						 errdetail("%s", logbuf.data)));

				pfree(logbuf.data);
				pfree(locktagbuf.data);

				/* send the autovacuum worker Back to Old Kent Road */
				if (kill(pid, SIGINT) < 0)
				{
					/* Just a warning to allow multiple callers */
					ereport(WARNING,
							(errmsg("could not send signal to process %d: %m",
									pid)));
				}
			}
			else
				LWLockRelease(ProcArrayLock);

			/* prevent signal from being resent more than once */
			allow_autovacuum_cancel = false;
		}

		/*
		 * If awoken after the deadlock check interrupt has run, and
		 * log_lock_waits is on, then report about the wait.
		 */
		if (log_lock_waits && deadlock_state != DS_NOT_YET_CHECKED)
		{
			StringInfoData buf;
			const char *modename;
			long		secs;
			int			usecs;
			long		msecs;

			initStringInfo(&buf);
			DescribeLockTag(&buf, &locallock->tag.lock);
			modename = GetLockmodeName(locallock->tag.lock.locktag_lockmethodid,
									   lockmode);
			TimestampDifference(timeout_start_time, GetCurrentTimestamp(),
								&secs, &usecs);
			msecs = secs * 1000 + usecs / 1000;
			usecs = usecs % 1000;

			if (deadlock_state == DS_SOFT_DEADLOCK)
				ereport(LOG,
						(errmsg("process %d avoided deadlock for %s on %s by rearranging queue order after %ld.%03d ms",
							  MyProcPid, modename, buf.data, msecs, usecs)));
			else if (deadlock_state == DS_HARD_DEADLOCK)
			{
				/*
				 * This message is a bit redundant with the error that will be
				 * reported subsequently, but in some cases the error report
				 * might not make it to the log (eg, if it's caught by an
				 * exception handler), and we want to ensure all long-wait
				 * events get logged.
				 */
				ereport(LOG,
						(errmsg("process %d detected deadlock while waiting for %s on %s after %ld.%03d ms",
							  MyProcPid, modename, buf.data, msecs, usecs)));
			}

			if (myWaitStatus == STATUS_WAITING)
				ereport(LOG,
						(errmsg("process %d still waiting for %s on %s after %ld.%03d ms",
							  MyProcPid, modename, buf.data, msecs, usecs)));
			else if (myWaitStatus == STATUS_OK)
				ereport(LOG,
					(errmsg("process %d acquired %s on %s after %ld.%03d ms",
							MyProcPid, modename, buf.data, msecs, usecs)));
			else
			{
				Assert(myWaitStatus == STATUS_ERROR);

				/*
				 * Currently, the deadlock checker always kicks its own
				 * process, which means that we'll only see STATUS_ERROR when
				 * deadlock_state == DS_HARD_DEADLOCK, and there's no need to
				 * print redundant messages.  But for completeness and
				 * future-proofing, print a message if it looks like someone
				 * else kicked us off the lock.
				 */
				if (deadlock_state != DS_HARD_DEADLOCK)
					ereport(LOG,
							(errmsg("process %d failed to acquire %s on %s after %ld.%03d ms",
							  MyProcPid, modename, buf.data, msecs, usecs)));
			}

			/*
			 * At this point we might still need to wait for the lock. Reset
			 * state so we don't print the above messages again.
			 */
			deadlock_state = DS_NO_DEADLOCK;

			pfree(buf.data);
		}
	} while (myWaitStatus == STATUS_WAITING);

	/*
	 * Disable the timer, if it's still running
	 */
	if (!disable_sig_alarm(false))
		elog(FATAL, "could not disable timer for process wakeup");

	/*
	 * Re-acquire the lock table's partition lock.  We have to do this to hold
	 * off cancel/die interrupts before we can mess with lockAwaited (else we
	 * might have a missed or duplicated locallock update).
	 */
	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * We no longer want LockWaitCancel to do anything.
	 */
	lockAwaited = NULL;

	/*
	 * If we got the lock, be sure to remember it in the locallock table.
	 */
	if (MyProc->waitStatus == STATUS_OK)
		GrantAwaitedLock();

	/*
	 * We don't have to do anything else, because the awaker did all the
	 * necessary update of the lock table and MyProc.
	 */
	return MyProc->waitStatus;
}


/*
 * ProcWakeup -- wake up a process by releasing its private semaphore.
 *
 *	 Also remove the process from the wait queue and set its links invalid.
 *	 RETURN: the next process in the wait queue.
 *
 * The appropriate lock partition lock must be held by caller.
 *
 * XXX: presently, this code is only used for the "success" case, and only
 * works correctly for that case.  To clean up in failure case, would need
 * to twiddle the lock's request counts too --- see RemoveFromWaitQueue.
 * Hence, in practice the waitStatus parameter must be STATUS_OK.
 */
PGPROC *
ProcWakeup(PGPROC *proc, int waitStatus)
{
	PGPROC	   *retProc;

	/* Proc should be sleeping ... */
	if (proc->links.prev == NULL ||
		proc->links.next == NULL)
		return NULL;
	Assert(proc->waitStatus == STATUS_WAITING);

	/* Save next process before we zap the list link */
	retProc = (PGPROC *) proc->links.next;

	/* Remove process from wait queue */
	SHMQueueDelete(&(proc->links));
	(proc->waitLock->waitProcs.size)--;

	/* Clean up process' state and pass it the ok/fail signal */
	proc->waitLock = NULL;
	proc->waitProcLock = NULL;
	proc->waitStatus = waitStatus;

	/* And awaken it */
	PGSemaphoreUnlock(&proc->sem);

	return retProc;
}

/*
 * ProcLockWakeup -- routine for waking up processes when a lock is
 *		released (or a prior waiter is aborted).  Scan all waiters
 *		for lock, waken any that are no longer blocked.
 *
 * The appropriate lock partition lock must be held by caller.
 */
void
ProcLockWakeup(LockMethod lockMethodTable, LOCK *lock)
{
	PROC_QUEUE *waitQueue = &(lock->waitProcs);
	int			queue_size = waitQueue->size;
	PGPROC	   *proc;
	LOCKMASK	aheadRequests = 0;

	Assert(queue_size >= 0);

	if (queue_size == 0)
		return;

	proc = (PGPROC *) waitQueue->links.next;

	while (queue_size-- > 0)
	{
		LOCKMODE	lockmode = proc->waitLockMode;

		/*
		 * Waken if (a) doesn't conflict with requests of earlier waiters, and
		 * (b) doesn't conflict with already-held locks.
		 */
		if ((lockMethodTable->conflictTab[lockmode] & aheadRequests) == 0 &&
			LockCheckConflicts(lockMethodTable,
							   lockmode,
							   lock,
							   proc->waitProcLock,
							   proc) == STATUS_OK)
		{
			/* OK to waken */
			GrantLock(lock, proc->waitProcLock, lockmode);
			proc = ProcWakeup(proc, STATUS_OK);

			/*
			 * ProcWakeup removes proc from the lock's waiting process queue
			 * and returns the next proc in chain; don't use proc's next-link,
			 * because it's been cleared.
			 */
		}
		else
		{
			/*
			 * Cannot wake this guy. Remember his request for later checks.
			 */
			aheadRequests |= LOCKBIT_ON(lockmode);
			proc = (PGPROC *) proc->links.next;
		}
	}

	Assert(waitQueue->size >= 0);
}

/*
 * CheckDeadLock
 *
 * We only get to this routine if we got SIGALRM after DeadlockTimeout
 * while waiting for a lock to be released by some other process.  Look
 * to see if there's a deadlock; if not, just return and continue waiting.
 * (But signal ProcSleep to log a message, if log_lock_waits is true.)
 * If we have a real deadlock, remove ourselves from the lock's wait queue
 * and signal an error to ProcSleep.
 *
 * NB: this is run inside a signal handler, so be very wary about what is done
 * here or in called routines.
 */
static void
CheckDeadLock(void)
{
	int			i;

	/*
	 * Acquire exclusive lock on the entire shared lock data structures. Must
	 * grab LWLocks in partition-number order to avoid LWLock deadlock.
	 *
	 * Note that the deadlock check interrupt had better not be enabled
	 * anywhere that this process itself holds lock partition locks, else this
	 * will wait forever.  Also note that LWLockAcquire creates a critical
	 * section, so that this routine cannot be interrupted by cancel/die
	 * interrupts.
	 */
	for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
		LWLockAcquire(FirstLockMgrLock + i, LW_EXCLUSIVE);

	/*
	 * Check to see if we've been awoken by anyone in the interim.
	 *
	 * If we have, we can return and resume our transaction -- happy day.
	 * Before we are awoken the process releasing the lock grants it to us so
	 * we know that we don't have to wait anymore.
	 *
	 * We check by looking to see if we've been unlinked from the wait queue.
	 * This is quicker than checking our semaphore's state, since no kernel
	 * call is needed, and it is safe because we hold the lock partition lock.
	 */
	if (MyProc->links.prev == NULL ||
		MyProc->links.next == NULL)
		goto check_done;

#ifdef LOCK_DEBUG
	if (Debug_deadlocks)
		DumpAllLocks();
#endif

	/* Run the deadlock check, and set deadlock_state for use by ProcSleep */
	deadlock_state = DeadLockCheck(MyProc);

	if (deadlock_state == DS_HARD_DEADLOCK)
	{
		/*
		 * Oops.  We have a deadlock.
		 *
		 * Get this process out of wait state.	(Note: we could do this more
		 * efficiently by relying on lockAwaited, but use this coding to
		 * preserve the flexibility to kill some other transaction than the
		 * one detecting the deadlock.)
		 *
		 * RemoveFromWaitQueue sets MyProc->waitStatus to STATUS_ERROR, so
		 * ProcSleep will report an error after we return from the signal
		 * handler.
		 */
		Assert(MyProc->waitLock != NULL);
		if (Gp_role == GP_ROLE_DISPATCH && IsResQueueEnabled() &&
			LOCK_LOCKMETHOD(*(MyProc->waitLock)) == RESOURCE_LOCKMETHOD)
		{
			ResRemoveFromWaitQueue(MyProc, 
								   LockTagHashCode(&(MyProc->waitLock->tag)));
		}
		else
		{
			RemoveFromWaitQueue(MyProc, LockTagHashCode(&(MyProc->waitLock->tag)));
		}

		/*
		 * Unlock my semaphore so that the interrupted ProcSleep() call can
		 * finish.
		 */
		PGSemaphoreUnlock(&MyProc->sem);

		/*
		 * We're done here.  Transaction abort caused by the error that
		 * ProcSleep will raise will cause any other locks we hold to be
		 * released, thus allowing other processes to wake up; we don't need
		 * to do that here.  NOTE: an exception is that releasing locks we
		 * hold doesn't consider the possibility of waiters that were blocked
		 * behind us on the lock we just failed to get, and might now be
		 * wakable because we're not in front of them anymore.  However,
		 * RemoveFromWaitQueue took care of waking up any such processes.
		 */
	}
	else if (log_lock_waits || deadlock_state == DS_BLOCKED_BY_AUTOVACUUM)
	{
		/*
		 * Unlock my semaphore so that the interrupted ProcSleep() call can
		 * print the log message (we daren't do it here because we are inside
		 * a signal handler).  It will then sleep again until someone releases
		 * the lock.
		 *
		 * If blocked by autovacuum, this wakeup will enable ProcSleep to send
		 * the cancelling signal to the autovacuum worker.
		 */
		PGSemaphoreUnlock(&MyProc->sem);
	}

	/*
	 * And release locks.  We do this in reverse order for two reasons: (1)
	 * Anyone else who needs more than one of the locks will be trying to lock
	 * them in increasing order; we don't want to release the other process
	 * until it can get all the locks it needs. (2) This avoids O(N^2)
	 * behavior inside LWLockRelease.
	 */
check_done:
	for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
		LWLockRelease(FirstLockMgrLock + i);
}


/*
 * ProcWaitForSignal - wait for a signal from another backend.
 *
 * This can share the semaphore normally used for waiting for locks,
 * since a backend could never be waiting for a lock and a signal at
 * the same time.  As with locks, it's OK if the signal arrives just
 * before we actually reach the waiting state.	Also as with locks,
 * it's necessary that the caller be robust against bogus wakeups:
 * always check that the desired state has occurred, and wait again
 * if not.	This copes with possible "leftover" wakeups.
 */
void
ProcWaitForSignal(void)
{
	PGSemaphoreLock(&MyProc->sem, true);
}

/*
 * ProcSendSignal - send a signal to a backend identified by PID
 */
void
ProcSendSignal(int pid)
{
	PGPROC	   *proc = BackendPidGetProc(pid);

	if (proc != NULL)
		PGSemaphoreUnlock(&proc->sem);
}


/*****************************************************************************
 * SIGALRM interrupt support
 *
 * Maybe these should be in pqsignal.c?
 *****************************************************************************/

/*
 * Enable the SIGALRM interrupt to fire after the specified delay
 *
 * Delay is given in milliseconds.	Caller should be sure a SIGALRM
 * signal handler is installed before this is called.
 *
 * This code properly handles nesting of deadlock timeout alarms within
 * statement timeout alarms.
 *
 * Returns TRUE if okay, FALSE on failure.
 */
bool
enable_sig_alarm(int delayms, bool is_statement_timeout)
{
	TimestampTz fin_time;
	struct itimerval timeval;

	if (is_statement_timeout)
	{
		/*
		 * Begin statement-level timeout
		 *
		 * Note that we compute statement_fin_time with reference to the
		 * statement_timestamp, but apply the specified delay without any
		 * correction; that is, we ignore whatever time has elapsed since
		 * statement_timestamp was set.  In the normal case only a small
		 * interval will have elapsed and so this doesn't matter, but there
		 * are corner cases (involving multi-statement query strings with
		 * embedded COMMIT or ROLLBACK) where we might re-initialize the
		 * statement timeout long after initial receipt of the message. In
		 * such cases the enforcement of the statement timeout will be a bit
		 * inconsistent.  This annoyance is judged not worth the cost of
		 * performing an additional gettimeofday() here.
		 */
		Assert(!deadlock_timeout_active);
		fin_time = GetCurrentStatementStartTimestamp();
		fin_time = TimestampTzPlusMilliseconds(fin_time, delayms);
		statement_fin_time = fin_time;
		cancel_from_timeout = false;
		statement_timeout_active = true;
	}
	else if (statement_timeout_active)
	{
		/*
		 * Begin deadlock timeout with statement-level timeout active
		 *
		 * Here, we want to interrupt at the closer of the two timeout times.
		 * If fin_time >= statement_fin_time then we need not touch the
		 * existing timer setting; else set up to interrupt at the deadlock
		 * timeout time.
		 *
		 * NOTE: in this case it is possible that this routine will be
		 * interrupted by the previously-set timer alarm.  This is okay
		 * because the signal handler will do only what it should do according
		 * to the state variables.	The deadlock checker may get run earlier
		 * than normal, but that does no harm.
		 */
		timeout_start_time = GetCurrentTimestamp();
		fin_time = TimestampTzPlusMilliseconds(timeout_start_time, delayms);
		deadlock_timeout_active = true;
		if (fin_time >= statement_fin_time)
			return true;
	}
	else
	{
		/* Begin deadlock timeout with no statement-level timeout */
		deadlock_timeout_active = true;
		/* GetCurrentTimestamp can be expensive, so only do it if we must */
		if (log_lock_waits)
			timeout_start_time = GetCurrentTimestamp();
	}

	/* If we reach here, okay to set the timer interrupt */
	MemSet(&timeval, 0, sizeof(struct itimerval));
	timeval.it_value.tv_sec = delayms / 1000;
	timeval.it_value.tv_usec = (delayms % 1000) * 1000;
	if (setitimer(ITIMER_REAL, &timeval, NULL))
		return false;
	return true;
}

/*
 * Cancel the SIGALRM timer, either for a deadlock timeout or a statement
 * timeout.  If a deadlock timeout is canceled, any active statement timeout
 * remains in force.
 *
 * Returns TRUE if okay, FALSE on failure.
 */
bool
disable_sig_alarm(bool is_statement_timeout)
{
	/*
	 * Always disable the interrupt if it is active; this avoids being
	 * interrupted by the signal handler and thereby possibly getting
	 * confused.
	 *
	 * We will re-enable the interrupt if necessary in CheckStatementTimeout.
	 */
	if (statement_timeout_active || deadlock_timeout_active)
	{
		struct itimerval timeval;

		MemSet(&timeval, 0, sizeof(struct itimerval));
		if (setitimer(ITIMER_REAL, &timeval, NULL))
		{
			statement_timeout_active = false;
			cancel_from_timeout = false;
			deadlock_timeout_active = false;
			return false;
		}
	}

	/* Always cancel deadlock timeout, in case this is error cleanup */
	deadlock_timeout_active = false;

	/* Cancel or reschedule statement timeout */
	if (is_statement_timeout)
	{
		statement_timeout_active = false;
		cancel_from_timeout = false;
	}
	else if (statement_timeout_active)
	{
		if (!CheckStatementTimeout())
			return false;
	}
	return true;
}

/*
 * We get here when a session has been idle for a while (waiting for the
 * client to send us SQL to execute).  The idea is to consume less resources while sitting idle,
 * so we can support more sessions being logged on.
 *
 * The expectation is that if the session is logged on, but nobody is sending us work to do,
 * we want to free up whatever resources we can.  Usually it means there is a human being at the
 * other end of the connection, and that person has walked away from their terminal, or just hasn't
 * decided what to do next.  We could be idle for a very long time (many hours).
 *
 * Of course, freeing gangs means that the next time the user does send in an SQL statement,
 * we need to allocate gangs (at least the writer gang) to do anything.  This entails extra work,
 * so we don't want to do this if we don't think the session has gone idle.
 *
 * P.s:  Is there anything we can free up on the master (QD) side?  I can't think of anything.
 *
 */
static void
HandleClientWaitTimeout(void)
{
	elog(DEBUG2,"HandleClientWaitTimeout");

	/*
	 * cancel the timer, as there is no reason we need it to go off again.
	 */
	disable_sig_alarm(false);

	/*
	 * Free gangs to free up resources on the segDBs.
	 */
	if (GangsExist())
	{
		if (IsTransactionOrTransactionBlock() || TempNamespaceOidIsValid())
		{
			/*
			 * If we are in a transaction, we can't release the writer gang,
			 * as this will abort the transaction.
			 *
			 * If we have a TempNameSpace, we can't release the writer gang, as this
			 * would drop any temp tables we own.
			 *
			 * Since we are idle, any reader gangs will be available but not allocated.
			 */
			disconnectAndDestroyIdleReaderGangs();
		}
		else
		{
			/*
			 * Get rid of ALL gangs... Readers and primary writer.
			 * After this, we have no resources being consumed on the segDBs at all.
			 *
			 * Our session wasn't destroyed due to an fatal error or FTS action, so
			 * we don't need to do anything special.  Specifically, we DON'T want
			 * to act like we are now in a new session, since that would be confusing
			 * in the log.
			 *
			 */
			DisconnectAndDestroyAllGangs(false);
		}
	}
}

/*
 * Check for statement timeout.  If the timeout time has come,
 * trigger a query-cancel interrupt; if not, reschedule the SIGALRM
 * interrupt to occur at the right time.
 *
 * Returns true if okay, false if failed to set the interrupt.
 */
static bool
CheckStatementTimeout(void)
{
	TimestampTz now;

	if (!statement_timeout_active)
		return true;			/* do nothing if not active */

	/* QD takes care of timeouts for QE. */
	if (Gp_role == GP_ROLE_EXECUTE)
		return true;

	now = GetCurrentTimestamp();

	if (now >= statement_fin_time)
	{
		/* Time to die */
		statement_timeout_active = false;
		cancel_from_timeout = true;
#ifdef HAVE_SETSID
		/* try to signal whole process group */
		kill(-MyProcPid, SIGINT);
#endif
		kill(MyProcPid, SIGINT);
	}
	else
	{
		/* Not time yet, so (re)schedule the interrupt */
		long		secs;
		int			usecs;
		struct itimerval timeval;

		TimestampDifference(now, statement_fin_time,
							&secs, &usecs);

		/*
		 * It's possible that the difference is less than a microsecond;
		 * ensure we don't cancel, rather than set, the interrupt.
		 */
		if (secs == 0 && usecs == 0)
			usecs = 1;
		MemSet(&timeval, 0, sizeof(struct itimerval));
		timeval.it_value.tv_sec = secs;
		timeval.it_value.tv_usec = usecs;
		if (setitimer(ITIMER_REAL, &timeval, NULL))
			return false;
	}

	return true;
}

/*
 * need DoingCommandRead to be extern so we can test it here.
 * Or would it be better to have some routine to call to get the
 * value of the bool?  This is simpler.
 */
extern bool DoingCommandRead;

/*
 * Signal handler for SIGALRM
 *
 * Process deadlock check and/or statement timeout check, as needed.
 * To avoid various edge cases, we must be careful to do nothing
 * when there is nothing to be done.  We also need to be able to
 * reschedule the timer interrupt if called before end of statement.
 */
void
handle_sig_alarm(SIGNAL_ARGS)
{
	int			save_errno = errno;

	/* SIGALRM is cause for waking anything waiting on the process latch */
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	/* don't joggle the elbow of proc_exit */
	if (!proc_exit_inprogress)
	{
		/*
		 * Idle session timeout shares with the deadlock timeout.
		 * If DoingCommandRead is true, we are deciding the session is idle
		 * In that case, we can't possibly be in a deadlock, so no point
		 * in running the deadlock detection.
		 */

		if (deadlock_timeout_active && !DoingCommandRead)
		{
			deadlock_timeout_active = false;
			CheckDeadLock();
		}

		if (statement_timeout_active)
			(void) CheckStatementTimeout();

		/*
		 * If we are DoingCommandRead, it means we are sitting idle waiting for
		 * the user to send us some SQL.
		 */
		if (DoingCommandRead)
		{
			(void) ClientWaitTimeoutInterruptHandler();
			deadlock_timeout_active = false;
		}
	}

	errno = save_errno;
}

static void
ClientWaitTimeoutInterruptHandler(void)
{
	int save_errno = errno;

	/* Don't joggle the elbow of proc_exit */
	if (proc_exit_inprogress)
		return;

	if (clientWaitTimeoutInterruptEnabled)
	{
		bool save_ImmediateInterruptOK = ImmediateInterruptOK;

		/*
		 * We may be called while ImmediateInterruptOK is true; turn it off
		 * while messing with the client wait timeout state.
		 */
		ImmediateInterruptOK = false;

		/*
		 * I'm not sure whether some flavors of Unix might allow another
		 * SIGALRM occurrence to recursively interrupt this routine. To cope
		 * with the possibility, we do the same sort of dance that
		 * EnableNotifyInterrupt must do -- see that routine for comments.
		 */
		clientWaitTimeoutInterruptEnabled = 0; /* disable any recursive signal */
		clientWaitTimeoutInterruptOccurred = 1; /* do at least one iteration */
		for (;;)
		{
			clientWaitTimeoutInterruptEnabled = 1;
			if (!clientWaitTimeoutInterruptOccurred)
				break;
			clientWaitTimeoutInterruptEnabled = 0;
			if (clientWaitTimeoutInterruptOccurred)
			{
				ProcessClientWaitTimeout();
			}
		}

		/*
		 * Restore ImmediateInterruptOK, and check for interrupts if needed.
		 */
		ImmediateInterruptOK = save_ImmediateInterruptOK;
		if (save_ImmediateInterruptOK)
			CHECK_FOR_INTERRUPTS();
	}
	else
	{
		/*
		 * In this path it is NOT SAFE to do much of anything, except this:
		 */
		clientWaitTimeoutInterruptOccurred = 1;
	}

	errno = save_errno;
}

void
EnableClientWaitTimeoutInterrupt(void)
{
	for (;;)
	{
		clientWaitTimeoutInterruptEnabled = 1;
		if (!clientWaitTimeoutInterruptOccurred)
			break;
		clientWaitTimeoutInterruptEnabled = 0;
		if (clientWaitTimeoutInterruptOccurred)
		{
			ProcessClientWaitTimeout();
		}
	}
}

bool
DisableClientWaitTimeoutInterrupt(void)
{
	bool result = (clientWaitTimeoutInterruptEnabled != 0);

	clientWaitTimeoutInterruptEnabled = 0;

	return result;
}

static void
ProcessClientWaitTimeout(void)
{
	bool notify_enabled;
	bool catchup_enabled;

	/* Must prevent SIGUSR1 and SIGUSR2 interrupt while I am running */
	notify_enabled = DisableNotifyInterrupt();
	catchup_enabled = DisableCatchupInterrupt();

	clientWaitTimeoutInterruptOccurred = 0;

	HandleClientWaitTimeout();

	if (notify_enabled)
		EnableNotifyInterrupt();
	if (catchup_enabled)
		EnableCatchupInterrupt();
}

/*
 * ResProcSleep -- put a process to sleep (that is waiting for a resource lock).
 *
 * Notes:
 * 	Locktable's masterLock must be held at entry, and will be held
 * 	at exit.
 *
 *	This is merely a version of ProcSleep modified for resource locks.
 *	The logic here could have been merged into ProcSleep, however it was
 *	requested to keep as much as possible of this resource lock code 
 *	seperate from its standard lock relatives - in the interest of not
 *	introducing new bugs or performance regressions into the lock code.
 */
int
ResProcSleep(LOCKMODE lockmode, LOCALLOCK *locallock, void *incrementSet)
{
	LOCK	   *lock = locallock->lock;
	PROCLOCK   *proclock = locallock->proclock;
	PROC_QUEUE	*waitQueue = &(lock->waitProcs);
	PGPROC		*proc;
	uint32		hashcode = locallock->hashcode;
	LWLockId	partitionLock = LockHashPartitionLock(hashcode);

	bool		selflock = true;		/* initialize result for error. */

	/*
	 * Don't check my held locks, as we just add at the end of the queue.
	 */
	proc = (PGPROC *) &(waitQueue->links);
	SHMQueueInsertBefore(&(proc->links), &(MyProc->links));
	waitQueue->size++;

	lock->waitMask |= LOCKBIT_ON(lockmode);

	/*
	 * reflect this in PGPROC object, too.
	 */
	MyProc->waitLock = lock;
	MyProc->waitProcLock = (PROCLOCK *) proclock;
	MyProc->waitLockMode = lockmode;

	MyProc->waitStatus = STATUS_ERROR;	/* initialize result for error */

	/* Now check the status of the self lock footgun. */
	selflock = ResCheckSelfDeadLock(lock, proclock, incrementSet);
	if (selflock)
	{
		LWLockRelease(partitionLock);
		ereport(ERROR,
				(errcode(ERRCODE_T_R_DEADLOCK_DETECTED),
				 errmsg("deadlock detected, locking against self")));
	}

	/* Mark that we are waiting for a lock */
	lockAwaited = locallock;

	/* Ok to wait.*/
	LWLockRelease(partitionLock);

	if (!enable_sig_alarm(DeadlockTimeout, false))
   		elog(FATAL, "could not set timer for (resource lock) process wakeup");

	/*
	 * Sleep on the semaphore.
	 */
	PGSemaphoreLock(&MyProc->sem, true);

	if (!disable_sig_alarm(false))
		elog(FATAL, "could not disable timer for (resource lock) process wakeup");

	/*
	 * Have been awakened, so continue.
	 */
	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * We no longer want (Res)LockWaitCancel to do anything.
	 */
	lockAwaited = NULL;

	return MyProc->waitStatus;
}


/*
 * ResLockWaitCancel -- Cancel any pending wait for a resource lock, when 
 *	aborting a transaction.
 */
void
ResLockWaitCancel(void)
{
	LWLockId	partitionLock;

	if (lockAwaited != NULL)
	{
		/* Unlink myself from the wait queue, if on it  */
		partitionLock = LockHashPartitionLock(lockAwaited->hashcode);
		LWLockAcquire(partitionLock, LW_EXCLUSIVE);

		if (MyProc->links.next != NULL)
		{
			/* We could not have been granted the lock yet */
			Assert(MyProc->waitStatus == STATUS_ERROR);

			/* We should only be trying to cancel resource locks. */
			Assert(LOCALLOCK_LOCKMETHOD(*lockAwaited) == RESOURCE_LOCKMETHOD);

			ResRemoveFromWaitQueue(MyProc, lockAwaited->hashcode);
		}

		lockAwaited = NULL;

		LWLockRelease(partitionLock);
	}

	/*
	 * Reset the proc wait semaphore to zero. This is necessary in the
	 * scenario where someone else granted us the lock we wanted before we
	 * were able to remove ourselves from the wait-list.
	 */
	PGSemaphoreReset(&MyProc->sem);

	return;
}

bool ProcCanSetMppSessionId(void)
{
	if (ProcGlobal == NULL || MyProc == NULL)
		return false;

	return true;
}

void ProcNewMppSessionId(int *newSessionId)
{
	Assert(newSessionId != NULL);

    *newSessionId = MyProc->mppSessionId =
		pg_atomic_add_fetch_u32((pg_atomic_uint32 *)&ProcGlobal->mppLocalProcessCounter, 1);

    /*
     * Make sure that our SessionState entry correctly records our
     * new session id.
     */
    if (NULL != MySessionState)
    {
    	/* This should not happen outside of dispatcher on the master */
    	Assert(GpIdentity.segindex == MASTER_CONTENT_ID && Gp_role == GP_ROLE_DISPATCH);

    	ereport(gp_sessionstate_loglevel, (errmsg("ProcNewMppSessionId: changing session id (old: %d, new: %d), pinCount: %d, activeProcessCount: %d",
    			MySessionState->sessionId, *newSessionId, MySessionState->pinCount, MySessionState->activeProcessCount), errprintstack(true)));

#ifdef USE_ASSERT_CHECKING
    	MySessionState->isModifiedSessionId = true;
#endif

    	MySessionState->sessionId = *newSessionId;
    }
}
