/*-------------------------------------------------------------------------
 *
 * proc.h
 *	  per-process shared memory data structures
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/proc.h,v 1.112 2009/02/23 09:28:50 heikki Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PROC_H_
#define _PROC_H_

#include "storage/latch.h"
#include "storage/lock.h"
#include "storage/spin.h"
#include "storage/pg_sema.h"
#include "access/xlog.h"

#include "cdb/cdblocaldistribxact.h"  /* LocalDistribXactData */


/*
 * Each backend advertises up to PGPROC_MAX_CACHED_SUBXIDS TransactionIds
 * for non-aborted subtransactions of its current top transaction.	These
 * have to be treated as running XIDs by other backends.
 *
 * We also keep track of whether the cache overflowed (ie, the transaction has
 * generated at least one subtransaction that didn't fit in the cache).
 * If none of the caches have overflowed, we can assume that an XID that's not
 * listed anywhere in the PGPROC array is not a running transaction.  Else we
 * have to look at pg_subtrans.
 */
#define PGPROC_MAX_CACHED_SUBXIDS 64	/* XXX guessed-at value */

struct XidCache
{
	bool		overflowed;
	int			nxids;
	TransactionId xids[PGPROC_MAX_CACHED_SUBXIDS];
};

/* Flags for PGPROC->vacuumFlags */
#define		PROC_IS_AUTOVACUUM	0x01	/* is it an autovac worker? */
#if 0 /* Upstream code not applicable to GPDB */
#define		PROC_IN_VACUUM		0x02	/* currently running lazy vacuum */
#endif
#define		PROC_IN_ANALYZE		0x04	/* currently running analyze */
#define		PROC_VACUUM_FOR_WRAPAROUND 0x08		/* set by autovac only */

/* flags reset at EOXact */
#define		PROC_VACUUM_STATE_MASK (0x0E)

/*
 * Each backend has a PGPROC struct in shared memory.  There is also a list of
 * currently-unused PGPROC structs that will be reallocated to new backends.
 *
 * links: list link for any list the PGPROC is in.	When waiting for a lock,
 * the PGPROC is linked into that lock's waitProcs queue.  A recycled PGPROC
 * is linked into ProcGlobal's freeProcs list.
 *
 * Note: twophase.c also sets up a dummy PGPROC struct for each currently
 * prepared transaction.  These PGPROCs appear in the ProcArray data structure
 * so that the prepared transactions appear to be still running and are
 * correctly shown as holding locks.  A prepared transaction PGPROC can be
 * distinguished from a real one at need by the fact that it has pid == 0.
 * The semaphore and lock-activity fields in a prepared-xact PGPROC are unused,
 * but its myProcLocks[] lists are valid.
 */
struct PGPROC
{
	/* proc->links MUST BE FIRST IN STRUCT (see ProcSleep,ProcWakeup,etc) */
	SHM_QUEUE	links;			/* list link if process is in a list */

	PGSemaphoreData sem;		/* ONE semaphore to sleep on */
	int			waitStatus;		/* STATUS_WAITING, STATUS_OK or STATUS_ERROR */

	Latch		procLatch;		/* generic latch for process */

	LocalTransactionId lxid;	/* local id of top-level transaction currently
								 * being executed by this proc, if running;
								 * else InvalidLocalTransactionId */

	TransactionId xid;			/* id of top-level transaction currently being
								 * executed by this proc, if running and XID
								 * is assigned; else InvalidTransactionId */

	TransactionId xmin;			/* minimal running XID as it was when we were
								 * starting our xact, excluding LAZY VACUUM:
								 * vacuum must not remove tuples deleted by
								 * xid >= xmin ! */

	/*
	 * Distributed transaction information. This is only maintained on QE's
	 * and accessed by the backend itself, so this doesn't need to be
	 * protected by any lock. On QD currentGXact provides this info, hence
	 * redundant info is not maintained here for QD. In fact, it could be just
	 * a global variable in backend-private memory, but it seems useful to
	 * have this information available for debugging purposes.
	 */
	LocalDistribXactData localDistribXactData;

	int			pid;			/* This backend's process id, or 0 */
	BackendId	backendId;		/* This backend's backend ID (if assigned) */
	Oid			databaseId;		/* OID of database this backend is using */
	Oid			roleId;			/* OID of role using this backend */
    int         mppSessionId;   /* serial num of the qDisp process */
    int         mppLocalProcessSerial;  /* this backend's PGPROC serial num */
    bool		mppIsWriter;	/* The writer gang member, holder of locks */

	bool		inCommit;		/* true if within commit critical section */

	uint8		vacuumFlags;	/* vacuum-related flags, see above */

	/* Info about LWLock the process is currently waiting for, if any. */
	bool		lwWaiting;		/* true if waiting for an LW lock */
	bool		lwExclusive;	/* true if waiting for exclusive access */
	struct PGPROC *lwWaitLink;	/* next waiter for same LW lock */

	/* Info about lock the process is currently waiting for, if any. */
	/* waitLock and waitProcLock are NULL if not currently waiting. */
	LOCK	   *waitLock;		/* Lock object we're sleeping on ... */
	PROCLOCK   *waitProcLock;	/* Per-holder info for awaited lock */
	LOCKMODE	waitLockMode;	/* type of lock we're waiting for */
	LOCKMASK	heldLocks;		/* bitmask for lock types already held on this
								 * lock object by this backend */

	/*
	 * Info to allow us to wait for synchronous replication, if needed.
	 * waitLSN is InvalidXLogRecPtr if not waiting; set only by user backend.
	 * syncRepState must not be touched except by owning process or WALSender.
	 * syncRepLinks used only while holding SyncRepLock.
	 */
	XLogRecPtr	waitLSN;		/* waiting for this LSN or higher */
	int			syncRepState;	/* wait state for sync rep */
	SHM_QUEUE	syncRepLinks;	/* list link if process is in syncrep queue */

	/*
	 * All PROCLOCK objects for locks held or awaited by this backend are
	 * linked into one of these lists, according to the partition number of
	 * their lock.
	 */
	SHM_QUEUE	myProcLocks[NUM_LOCK_PARTITIONS];

	struct XidCache subxids;	/* cache for subtransaction XIDs */

	/*
	 * Info for Resource Scheduling, what portal (i.e statement) we might
	 * be waiting on.
	 */
	uint32		waitPortalId;	/* portal id we are waiting on */

	/*
	 * Information for our combocid-map (populated in writer/dispatcher backends only)
	 */
	uint32		combocid_map_count; /* how many entries in the map ? */

	int queryCommandId; /* command_id for the running query */

	bool serializableIsoLevel; /* true if proc has serializable isolation level set */

	bool inDropTransaction; /* true if proc is in vacuum drop transaction */

	/*
	 * Information for resource group
	 */
	void		*resSlot;	/* the resource group slot granted.
   							 * NULL indicates the resource group is
							 * locked for drop. */
};

/* NOTE: "typedef struct PGPROC PGPROC" appears in storage/lock.h. */

extern PGDLLIMPORT PGPROC *MyProc;

/* Special for MPP reader gangs */
extern PGDLLIMPORT PGPROC *lockHolderProcPtr;


/*
 * There is one ProcGlobal struct for the whole database cluster.
 */
typedef struct PROC_HDR
{
	/* The PGPROC structures */
	PGPROC *procs;
	/* Head of list of free PGPROC structures */
	PGPROC	   *freeProcs;
	/* Head of list of autovacuum's free PGPROC structures */
	PGPROC	   *autovacFreeProcs;
	/* Current shared estimate of appropriate spins_per_delay value */
	int			spins_per_delay;

    /* Counter for assigning serial numbers to processes */
    int         mppLocalProcessCounter;

	/* Number of free PGPROC entries in freeProcs list. */
	int			numFreeProcs;

} PROC_HDR;

/*
 * We set aside some extra PGPROC structures for auxiliary processes,
 * ie things that aren't full-fledged backends but need shmem access.
 *
 * Background writer, checkpointer, WAL writer, and autovacuum launcher run
 * during normal operation. Startup process also consumes one slot, but WAL
 * writer and autovacuum launcher are launched only after it has exited (4
 * slots).
 */
#define NUM_AUXILIARY_PROCS	 14


/* configurable options */
extern int	DeadlockTimeout;
extern int	StatementTimeout;
extern bool log_lock_waits;
extern int IdleSessionGangTimeout;

extern volatile bool cancel_from_timeout;


/*
 * Function Prototypes
 */
extern int	ProcGlobalSemas(void);
extern Size ProcGlobalShmemSize(void);
extern void InitProcGlobal(void);
extern void InitProcess(void);
extern void InitProcessPhase2(void);
extern void InitAuxiliaryProcess(void);
extern bool HaveNFreeProcs(int n);
extern void ProcReleaseLocks(bool isCommit);

extern void ProcQueueInit(PROC_QUEUE *queue);
extern int	ProcSleep(LOCALLOCK *locallock, LockMethod lockMethodTable);
extern PGPROC *ProcWakeup(PGPROC *proc, int waitStatus);
extern void ProcLockWakeup(LockMethod lockMethodTable, LOCK *lock);
extern void LockWaitCancel(void);

extern void ProcWaitForSignal(void);
extern void ProcSendSignal(int pid);

extern bool enable_sig_alarm(int delayms, bool is_statement_timeout);
extern bool disable_sig_alarm(bool is_statement_timeout);
extern void handle_sig_alarm(SIGNAL_ARGS);
extern void EnableClientWaitTimeoutInterrupt(void);
extern bool DisableClientWaitTimeoutInterrupt(void);

extern int ResProcSleep(LOCKMODE lockmode, LOCALLOCK *locallock, void *incrementSet);

extern void ResLockWaitCancel(void);
extern bool ProcCanSetMppSessionId(void);
extern void ProcNewMppSessionId(int *newSessionId);

#endif   /* PROC_H */
