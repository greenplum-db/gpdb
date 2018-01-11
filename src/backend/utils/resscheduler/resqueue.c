/*-------------------------------------------------------------------------
 *
 * resqueue.c
 *	  POSTGRES internals code for resource queues and locks.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resscheduler/resqueue.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <time.h>

#include "pgstat.h"
#include "access/heapam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_type.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lock.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/guc_tables.h"
#include "utils/memutils.h"
#include "utils/portal.h"
#include "utils/ps_status.h"
#include "utils/resowner.h"
#include "utils/resscheduler.h"
#include "cdb/memquota.h"
#include "commands/queue.h"

static void ResCleanUpLock(LOCK *lock, PROCLOCK *proclock, uint32 hashcode, bool wakeupNeeded);

static ResPortalIncrement *ResIncrementAdd(ResPortalIncrement *incSet, PROCLOCK *proclock, ResourceOwner owner);
static bool ResIncrementRemove(ResPortalTag *portaltag);

static void ResWaitOnLock(LOCALLOCK *locallock, ResourceOwner owner, ResPortalIncrement *incrementSet);

static void ResLockUpdateLimit(LOCK *lock, PROCLOCK *proclock, ResPortalIncrement *incrementSet, bool increment, bool inError);

static void ResGrantLock(LOCK *lock, PROCLOCK *proclock);
static bool ResUnGrantLock(LOCK *lock, PROCLOCK *proclock);

static uint64 ResourceQueueGetSuperuserQueryMemoryLimit(void);
/*
 * Global Variables
 */
static HTAB *ResPortalIncrementHash;	/* Hash of resource increments. */
static HTAB *ResQueueHash;		/* Hash of resource queues. */


/*
 * Record structure holding the to be exposed per queue data, used by
 * pg_resqueue_status().
 */
typedef struct
{
	Oid			queueid;
	float4		queuecountthreshold;
	float4		queuecostthreshold;
	float4		queuememthreshold;
	float4		queuecountvalue;
	float4		queuecostvalue;
	float4		queuememvalue;
	int			queuewaiters;
	int			queueholders;
}	QueueStatusRec;


/*
 * Function context for data persisting over repeated calls, used by
 * pg_resqueue_status().
 */
typedef struct
{
	QueueStatusRec *record;
	int			numRecords;

}	QueueStatusContext;

static void BuildQueueStatusContext(QueueStatusContext *fctx);


/*
 * ResLockAcquire -- acquire a resource lock.
 *
 * Notes and critisms:
 *
 *	Returns LOCKACQUIRE_OK if we get the lock,
 *			LOCKACQUIRE_NOT_AVAIL if we don't want to take the lock after all.
 *
 *	Analogous to LockAcquire, but the lockmode and session boolean are not
 *	required in the function prototype as we are *always* lockmode ExclusiveLock
 *	and have no session locks.
 *
 *	The semantics of resource locks mean that lockmode has minimal meaning -
 *	the conflict rules are determined by the state of the counters of the
 *	corresponding queue. We are maintaining the lock lockmode and related
 *	elements (holdmask etc), in order to ease comparison with standard locks
 *	at deadlock check time (well, so we hope anyway.)
 *
 * The "locktag" here consists of the queue-id and the "lockmethod" of
 * "resource-queue" and an identifier specifying that this is a
 * resource-locktag.
 *
 */
LockAcquireResult
ResLockAcquire(LOCKTAG *locktag, ResPortalIncrement *incrementSet)
{
	LOCKMODE	lockmode = ExclusiveLock;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	PROCLOCKTAG proclocktag;
	LOCALLOCKTAG localtag;
	LOCALLOCK  *locallock;
	uint32		hashcode;
	uint32		proclock_hashcode;
	int			partition;
	LWLockId	partitionLock;
	bool		found;
	ResourceOwner owner;
	ResQueue	queue;
	int			status;

	/* Setup the lock method bits. */
	Assert(locktag->locktag_lockmethodid == RESOURCE_LOCKMETHOD);

	/* Provide a resource owner. */
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
	 * if it's a new locallock object, initialize it, if it already exists
	 * then that is enough for the resource locks.
	 */
	if (!found)
	{
		locallock->lock = NULL;
		locallock->proclock = NULL;
		locallock->hashcode = LockTagHashCode(&(localtag.lock));
		locallock->nLocks = 0;
		locallock->numLockOwners = 0;
		locallock->maxLockOwners = 8;
		locallock->lockOwners = NULL;
		locallock->lockOwners = (LOCALLOCKOWNER *)
			MemoryContextAlloc(TopMemoryContext, locallock->maxLockOwners * sizeof(LOCALLOCKOWNER));
	}

	/* We are going to examine the shared lock table. */
	hashcode = locallock->hashcode;
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
	locallock->lock = lock;
	if (!lock)
	{
		LWLockRelease(partitionLock);
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of shared memory"),
				 errhint("You may need to increase max_resource_qeueues.")));
	}

	/*
	 * if it's a new lock object, initialize it.
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
	}
	else
	{
		Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));
		Assert((lock->nGranted >= 0) && (lock->granted[lockmode] >= 0));
		Assert(lock->nGranted <= lock->nRequested);
	}

	/*
	 * Create the hash key for the proclock table.
	 */
	MemSet(&proclocktag, 0, sizeof(PROCLOCKTAG));		/* Clear padding. */
	proclocktag.myLock = lock;
	proclocktag.myProc = MyProc;

	proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

	/*
	 * Find or create a proclock entry with this tag.
	 */
	proclock = (PROCLOCK *) hash_search_with_hash_value(LockMethodProcLockHash,
														(void *) &proclocktag,
														proclock_hashcode,
														HASH_ENTER_NULL,
														&found);
	locallock->proclock = proclock;
	if (!proclock)
	{
		/* Not enough shmem for the proclock. */
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
				 errhint("You may need to increase max_resource_qeueues.")));
	}

	/*
	 * If new, initialize the new entry.
	 */
	if (!found)
	{
		proclock->holdMask = 0;
		proclock->releaseMask = 0;
		/* Add proclock to appropriate lists */
		SHMQueueInsertBefore(&lock->procLocks, &proclock->lockLink);
		SHMQueueInsertBefore(&(MyProc->myProcLocks[partition]), &proclock->procLink);
		proclock->nLocks = 0;
		SHMQueueInit(&(proclock->portalLinks));
	}
	else
	{
		Assert((proclock->holdMask & ~lock->grantMask) == 0);
		/* Could do a deadlock risk check here. */
	}

	/*
	 * lock->nRequested and lock->requested[] count the total number of
	 * requests, whether granted or waiting, so increment those immediately.
	 * The other counts don't increment till we get the lock.
	 */
	lock->nRequested++;
	lock->requested[lockmode]++;
	Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));

	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	/* Look up existing queue */
	PG_TRY();
	{
		queue = GetResQueueFromLock(lock);
	}
	PG_CATCH();
	{
		/*
		 * Something wrong happened - our RQ is gone. Release all locks and
		 * clean out
		 */
		LWLockReleaseAll();
		PG_RE_THROW();
	}
	PG_END_TRY();

	/*
	 * If the query cost is smaller than the ignore cost limit for this queue
	 * then don't try to take a lock at all.
	 */
	if (incrementSet->increments[RES_COST_LIMIT] < queue->ignorecostlimit)
	{
		/* Decrement requested. */
		lock->nRequested--;
		lock->requested[lockmode]--;
		Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));

		/* Clean up this lock. */
		if (proclock->nLocks == 0)
			RemoveLocalLock(locallock);

		ResCleanUpLock(lock, proclock, hashcode, false);

		LWLockRelease(ResQueueLock);
		LWLockRelease(partitionLock);

		/*
		 * To avoid queue accounting problems, we will need to reset the
		 * queueId and portalId for this portal *after* returning from here.
		 */
		return LOCKACQUIRE_NOT_AVAIL;
	}

	/*
	 * Otherwise, we are going to take a lock, Add an increment to the
	 * increment hash for this process.
	 */
	incrementSet = ResIncrementAdd(incrementSet, proclock, owner);
	if (!incrementSet)
	{
		LWLockRelease(ResQueueLock);
		LWLockRelease(partitionLock);
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of shared memory adding portal increments"),
				 errhint("You may need to increase max_resource_portals_per_transaction.")));
	}

	/*
	 * Check if the lock can be acquired (i.e. if the resource the lock and
	 * queue control is not exhausted).
	 */
	status = ResLockCheckLimit(lock, proclock, incrementSet, true);
	if (status == STATUS_ERROR)
	{
		/*
		 * The requested lock has individual increments that are larger than
		 * some of the thresholds for the corrosponding queue, and overcommit
		 * is not enabled for them. So abort and clean up.
		 */
		ResPortalTag portalTag;

		/* Adjust the counters as we no longer want this lock. */
		lock->nRequested--;
		lock->requested[lockmode]--;
		Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));

		/* Clean up this lock. */
		if (proclock->nLocks == 0)
			RemoveLocalLock(locallock);

		ResCleanUpLock(lock, proclock, hashcode, false);

		/* Kill off the increment. */
		MemSet(&portalTag, 0, sizeof(ResPortalTag));
		portalTag.pid = incrementSet->pid;
		portalTag.portalId = incrementSet->portalId;

		ResIncrementRemove(&portalTag);

		LWLockRelease(ResQueueLock);
		LWLockRelease(partitionLock);
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("statement requires more resources than resource queue allows")));
	}
	else if (status == STATUS_OK)
	{
		/*
		 * The requested lock will *not* exhaust the limit for this resource
		 * queue, so record this in the local lock hash, and grant it.
		 */
		ResGrantLock(lock, proclock);
		ResLockUpdateLimit(lock, proclock, incrementSet, true, false);

		LWLockRelease(ResQueueLock);

		/* Note the start time for queue statistics. */
		pgstat_record_start_queue_exec(incrementSet->portalId,
									   locktag->locktag_field1);
	}
	else
	{
		Assert(status == STATUS_FOUND);

		/*
		 * The requested lock will exhaust the limit for this resource queue,
		 * so must wait.
		 */

		/* Set bitmask of locks this process already holds on this object. */
		MyProc->heldLocks = proclock->holdMask; /* Do we need to do this? */

		/*
		 * Set the portal id so we can identify what increments we are wanting
		 * to apply at wakeup.
		 */
		MyProc->waitPortalId = incrementSet->portalId;

		LWLockRelease(ResQueueLock);

		/* Note count and wait time for queue statistics. */
		pgstat_count_queue_wait(incrementSet->portalId,
								locktag->locktag_field1);
		pgstat_record_start_queue_wait(incrementSet->portalId,
									   locktag->locktag_field1);

		/*
		 * Free/destroy idle gangs as we are going to sleep.
		 */
		if (ResourceCleanupIdleGangs)
		{
			disconnectAndDestroyIdleReaderGangs();
		}

		/*
		 * Sleep till someone wakes me up.
		 */
		ResWaitOnLock(locallock, owner, incrementSet);

		/*
		 * Have been awakened, check state is consistent.
		 */
		if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
		{
			LWLockRelease(partitionLock);
			elog(ERROR, "ResLockAcquire failed");
		}

		/* Reset the portal id. */
		MyProc->waitPortalId = INVALID_PORTALID;

		/* End wait time and start execute time statistics for this queue. */
		pgstat_record_end_queue_wait(incrementSet->portalId,
									 locktag->locktag_field1);
		pgstat_record_start_queue_exec(incrementSet->portalId,
									   locktag->locktag_field1);
	}

	/* Release the	partition lock. */
	LWLockRelease(partitionLock);

	return LOCKACQUIRE_OK;
}

/*
 * ResLockRelease -- release a resource lock.
 *
 * The "locktag" here consists of the queue-id and the "lockmethod" of
 * "resource-queue" and an identifier specifying that this is a
 * resource-locktag.
 */
bool
ResLockRelease(LOCKTAG *locktag, uint32 resPortalId)
{
	LOCKMODE	lockmode = ExclusiveLock;
	LOCK	   *lock;
	PROCLOCK   *proclock;
	LOCALLOCKTAG localtag;
	LOCALLOCK  *locallock;
	uint32		hashcode;
	LWLockId	partitionLock;
	ResourceOwner owner;

	ResPortalIncrement *incrementSet;
	ResPortalTag portalTag;

	/* Check the lock method bits. */
	Assert(locktag->locktag_lockmethodid == RESOURCE_LOCKMETHOD);

	/* Provide a resource owner. */
	owner = CurrentResourceOwner;

	/*
	 * Find the LOCALLOCK entry for this lock and lockmode
	 */
	MemSet(&localtag, 0, sizeof(localtag));		/* must clear padding */
	localtag.lock = *locktag;
	localtag.mode = lockmode;

	locallock = (LOCALLOCK *)
		hash_search(LockMethodLocalHash, (void *) &localtag, HASH_FIND, NULL);

	/*
	 * If the lock request did not get very far, cleanup is easy.
	 */
	if (!locallock ||
		!locallock->lock ||
		!locallock->proclock)
	{
		elog(LOG, "Resource queue %d: no lock to release", locktag->locktag_field1);
		if (locallock)
		{
			RemoveLocalLock(locallock);
		}

		return false;
	}

	hashcode = locallock->hashcode;

	/* We are going to examine the shared lock table. */
	partitionLock = LockHashPartitionLock(hashcode);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	/*
	 * Verify that our LOCALLOCK still matches the shared tables.
	 *
	 * While waiting for the lock, our request could have been canceled to
	 * resolve a deadlock.  It could already have been removed from the shared
	 * LOCK and PROCLOCK tables, and those entries could have been reallocated
	 * for some other request.  Then all we need to do is clean up the
	 * LOCALLOCK entry.
	 */
	lock = locallock->lock;
	proclock = locallock->proclock;
	if (proclock->tag.myLock != lock ||
		proclock->tag.myProc != MyProc ||
		memcmp(&locallock->tag.lock, &lock->tag, sizeof(lock->tag)) != 0)
	{
		LWLockRelease(partitionLock);
		elog(DEBUG1, "Resource queue %d: lock already gone", locktag->locktag_field1);
		RemoveLocalLock(locallock);

		return false;
	}

	/*
	 * Double-check that we are actually holding a lock of the type we want to
	 * Release.
	 */
	if (!(proclock->holdMask & LOCKBIT_ON(lockmode)) || proclock->nLocks <= 0)
	{
		LWLockRelease(partitionLock);
		elog(DEBUG1, "Resource queue %d: proclock not held", locktag->locktag_field1);
		RemoveLocalLock(locallock);

		return false;
	}

	/*
	 * Find the increment for this portal and process.
	 */
	MemSet(&portalTag, 0, sizeof(ResPortalTag));
	portalTag.pid = MyProc->pid;
	portalTag.portalId = resPortalId;

	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	incrementSet = ResIncrementFind(&portalTag);
	if (!incrementSet)
	{
		elog(DEBUG1, "Resource queue %d: increment not found on unlock", locktag->locktag_field1);
		if (proclock->nLocks == 0)
		{
			RemoveLocalLock(locallock);
		}

		ResCleanUpLock(lock, proclock, hashcode, true);
		LWLockRelease(ResQueueLock);
		LWLockRelease(partitionLock);
		return false;
	}

	/*
	 * Un-grant the lock.
	 */
	ResUnGrantLock(lock, proclock);
	ResLockUpdateLimit(lock, proclock, incrementSet, false, false);

	/*
	 * Perform clean-up, waking up any waiters!
	 */
	if (proclock->nLocks == 0)
		RemoveLocalLock(locallock);

	ResCleanUpLock(lock, proclock, hashcode, true);

	/*
	 * Clean up the increment set.
	 */
	if (!ResIncrementRemove(&portalTag))
	{
		LWLockRelease(ResQueueLock);
		LWLockRelease(partitionLock);

		elog(ERROR, "no increment to remove for portal id %u and pid %d", resPortalId, MyProc->pid);
		/* not reached */
	}

	LWLockRelease(ResQueueLock);
	LWLockRelease(partitionLock);

	/* Update execute statistics for this queue, count and elapsed time. */
	pgstat_count_queue_exec(resPortalId, locktag->locktag_field1);
	pgstat_record_end_queue_exec(resPortalId, locktag->locktag_field1);

	return true;
}


/*
 * ResLockCheckLimit -- test whether the given process acquiring the this lock
 *	will cause a resource to exceed its limits.
 *
 * Notes:
 *	Returns STATUS_FOUND if limit will be exhausted, STATUS_OK if not.
 *
 *	If increment is true, then the resource counter associated with the lock
 *	is to be incremented, if false then decremented.
 *
 *	Named similarly to the LockCheckconflicts() for standard locks, but it is
 *	not checking a table of lock mode conflicts, but whether a shared counter
 *	for some resource is exhausted.
 *
 *	The resource queue lightweight lock (ResQueueLock) must be held while
 *	this function is called.
 *
 * MPP-4340: modified logic so that we return STATUS_OK when
 * decrementing resource -- decrements shouldn't care, let's not stop
 * them from freeing resources!
 */
int
ResLockCheckLimit(LOCK *lock, PROCLOCK *proclock, ResPortalIncrement *incrementSet, bool increment)
{
	ResQueue	queue;
	ResLimit	limits;
	bool		over_limit = false;
	bool		will_overcommit = false;
	int			status = STATUS_OK;
	Cost		increment_amt;
	int			i;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	/* Get the queue for this lock. */
	queue = GetResQueueFromLock(lock);
	limits = queue->limits;

	for (i = 0; i < NUM_RES_LIMIT_TYPES; i++)
	{
		/*
		 * Skip the default threshold, as it means 'no limit'.
		 */
		if (limits[i].threshold_value == INVALID_RES_LIMIT_THRESHOLD)
			continue;

		switch (limits[i].type)
		{
			case RES_COUNT_LIMIT:
				{
					Assert((limits[i].threshold_is_max));

					/* Setup whether to increment or decrement the # active. */
					if (increment)
					{
						increment_amt = incrementSet->increments[i];

						if (limits[i].current_value + increment_amt > limits[i].threshold_value)
							over_limit = true;
					}
					else
					{
						increment_amt = -1 * incrementSet->increments[i];
					}

#ifdef RESLOCK_DEBUG
					elog(DEBUG1, "checking count limit threshold %.0f current %.0f",
						 limits[i].threshold_value, limits[i].current_value);
#endif
				}
				break;

			case RES_COST_LIMIT:
				{
					Assert((limits[i].threshold_is_max));

					/* Setup whether to increment or decrement the cost. */
					if (increment)
					{
						increment_amt = incrementSet->increments[i];

						/* Check if this will overcommit */
						if (increment_amt > limits[i].threshold_value)
							will_overcommit = true;

						if (queue->overcommit)
						{
							/*
							 * Autocommit is enabled, allow statements that
							 * blowout the limit if noone else is active!
							 */
							if ((limits[i].current_value + increment_amt > limits[i].threshold_value) &&
								(limits[i].current_value > 0.1))
								over_limit = true;
						}
						else
						{
							/*
							 * No autocommit, so always fail statements that
							 * blowout the limit.
							 */
							if (limits[i].current_value + increment_amt > limits[i].threshold_value)
								over_limit = true;
						}
					}
					else
					{
						increment_amt = -1 * incrementSet->increments[i];
					}

#ifdef RESLOCK_DEBUG
					elog(DEBUG1, "checking cost limit threshold %.2f current %.2f",
						 limits[i].threshold_value, limits[i].current_value);
#endif
				}
				break;

			case RES_MEMORY_LIMIT:
				{
					Assert((limits[i].threshold_is_max));

					/* Setup whether to increment or decrement the # active. */
					if (increment)
					{
						increment_amt = incrementSet->increments[i];

						if (limits[i].current_value + increment_amt > limits[i].threshold_value)
							over_limit = true;
					}
					else
					{
						increment_amt = -1 * incrementSet->increments[i];
					}

#ifdef RESLOCK_DEBUG
					elog(DEBUG1, "checking memory limit threshold %.0f current %.0f",
						 limits[i].threshold_value, limits[i].current_value);
#endif
				}
				break;

			default:
				break;
		}
	}

	if (will_overcommit && !queue->overcommit)
		status = STATUS_ERROR;
	else if (over_limit)
		status = STATUS_FOUND;

	return status;

}


/*
 * ResLockUpdateLimit -- update the resource counter for this lock with the
 *	increment for the process.
 *
 * Notes:
 *	If increment is true, then the resource counter associated with the lock
 *	is to be incremented, if false then decremented.
 *
 * Warnings:
 *	The resource queue lightweight lock (ResQueueLock) must be held while
 *	this function is called.
 */
void
ResLockUpdateLimit(LOCK *lock, PROCLOCK *proclock, ResPortalIncrement *incrementSet, bool increment, bool inError)
{
	ResQueue	queue;
	ResLimit	limits;
	Cost		increment_amt;
	int			i;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	/* Get the queue for this lock. */
	queue = GetResQueueFromLock(lock);
	limits = queue->limits;

	for (i = 0; i < NUM_RES_LIMIT_TYPES; i++)
	{
		/*
		 * MPP-8454: NOTE that if our resource-queue has been modified since
		 * we locked our resources, on unlock it is possible that we're
		 * deducting an increment that we never added -- the lowest value we
		 * should allow is 0.0.
		 *
		 */
		switch (limits[i].type)
		{
			case RES_COUNT_LIMIT:
			case RES_COST_LIMIT:
			case RES_MEMORY_LIMIT:
				{
					Cost		new_value;

					Assert((limits[i].threshold_is_max));
					/* setup whether to increment or decrement the # active. */
					if (increment)
					{
						increment_amt = incrementSet->increments[i];
					}
					else
					{
						increment_amt = -1 * incrementSet->increments[i];
					}

					new_value = ceil(limits[i].current_value + increment_amt);
					new_value = Max(new_value, 0.0);

					limits[i].current_value = new_value;
				}
				break;

			default:
				break;
		}
	}

	return;
}

/*
 * GetResQueueFromLock -- find the resource queue for a given lock;
 *
 * Notes:
 *	should be handed a locktag containing a valid queue id.
 *	should hold the resource queue lightweight lock during this operation
 */
ResQueue
GetResQueueFromLock(LOCK *lock)
{
	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	ResQueue	queue = ResQueueHashFind(GET_RESOURCE_QUEUEID_FOR_LOCK(lock));

	if (queue == NULL)
	{
		elog(ERROR, "cannot find queue id %d", GET_RESOURCE_QUEUEID_FOR_LOCK(lock));
	}

	return queue;
}

/*
 * ResGrantLock -- grant a resource lock.
 *
 * Warnings:
 *	It is expected that the partition lock is held before calling this
 *	function, as the various shared queue counts are inspected.
 */
static void
ResGrantLock(LOCK *lock, PROCLOCK *proclock)
{
	LOCKMODE	lockmode = ExclusiveLock;

	/* Update the standard lock stuff, for locks and proclocks. */
	lock->nGranted++;
	lock->granted[lockmode]++;
	lock->grantMask |= LOCKBIT_ON(lockmode);
	if (lock->granted[lockmode] == lock->requested[lockmode])
	{
		lock->waitMask &= LOCKBIT_OFF(lockmode);		/* no more waiters. */

	}
	proclock->holdMask |= LOCKBIT_ON(lockmode);

	Assert((lock->nGranted > 0) && (lock->granted[lockmode] > 0));
	Assert(lock->nGranted <= lock->nRequested);

	/* Update the holders count. */
	proclock->nLocks++;

	return;
}

/*
 * ResUnGrantLock --  opposite of ResGrantLock.
 *
 * Notes:
 *	The equivalant standard lock function returns true only if there are waiters,
 *	we don't do this.
 *
 * Warnings:
 *	It is expected that the partition lock held before calling this
 *	function, as the various shared queue counts are inspected.
 */
bool
ResUnGrantLock(LOCK *lock, PROCLOCK *proclock)
{
	LOCKMODE	lockmode = ExclusiveLock;

	Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));
	Assert((lock->nGranted > 0) && (lock->granted[lockmode] > 0));
	Assert(lock->nGranted <= lock->nRequested);

	/* Update the standard lock stuff. */
	lock->nRequested--;
	lock->requested[lockmode]--;
	lock->nGranted--;
	lock->granted[lockmode]--;

	if (lock->granted[lockmode] == 0)
	{
		/* change the conflict mask.  No more of this lock type. */
		lock->grantMask &= LOCKBIT_OFF(lockmode);
	}

	/* Update the holders count. */
	proclock->nLocks--;

	/* Fix the per-proclock state. */
	if (proclock->nLocks == 0)
	{
		proclock->holdMask &= LOCKBIT_OFF(lockmode);
	}

	return true;
}


/*
 * ResCleanUpLock -- lock cleanup, remove entry from lock queues and start
 *	waking up waiters.
 *
 * MPP-6055/MPP-6144: we get called more than once; if we've already cleaned
 * up, don't walk off the end of lists; or panic when we can't find our hashtable
 * entries.
 */
static void
ResCleanUpLock(LOCK *lock, PROCLOCK *proclock, uint32 hashcode, bool wakeupNeeded)
{
	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	/*
	 * If this was my last hold on this lock, delete my entry in the proclock
	 * table.
	 */
	if (proclock->holdMask == 0 && proclock->nLocks == 0)
	{
		uint32		proclock_hashcode;

		if (proclock->lockLink.next != NULL)
			SHMQueueDelete(&proclock->lockLink);

		if (proclock->procLink.next != NULL)
			SHMQueueDelete(&proclock->procLink);

		proclock_hashcode = ProcLockHashCode(&proclock->tag, hashcode);
		hash_search_with_hash_value(LockMethodProcLockHash, (void *) &(proclock->tag),
									proclock_hashcode, HASH_REMOVE, NULL);
	}

	if (lock->nRequested == 0)
	{
		/*
		 * The caller just released the last lock, so garbage-collect the lock
		 * object.
		 */
		Assert(SHMQueueEmpty(&(lock->procLocks)));

		hash_search(LockMethodLockHash, (void *) &(lock->tag), HASH_REMOVE, NULL);
	}

	/*
	 * If appropriate, awaken any waiters.
	 */
	if (wakeupNeeded)
	{
		ResProcLockRemoveSelfAndWakeup(lock);
	}

	return;
}


/*
 * WaitOnResLock -- wait to acquire a resource lock.
 *
 *
 * Warnings:
 *	It is expected that the partition lock is held before calling this
 *	function, as the various shared queue counts are inspected.
 */
static void
ResWaitOnLock(LOCALLOCK *locallock, ResourceOwner owner, ResPortalIncrement *incrementSet)
{
	uint32		hashcode = locallock->hashcode;
	LWLockId	partitionLock = LockHashPartitionLock(hashcode);
	const char *old_status;
	char	   *new_status = NULL;
	int			len;

	/* Report change to waiting status */
	if (update_process_title)
	{
		old_status = get_ps_display(&len);
		new_status = (char *) palloc(len + 8 + 1);
		memcpy(new_status, old_status, len);
		strcpy(new_status + len, " queuing");
		set_ps_display(new_status, false);		/* truncate off " queuing" */
		new_status[len] = '\0';
	}
	pgstat_report_waiting(PGBE_WAITING_LOCK);

	awaitedLock = locallock;
	awaitedOwner = owner;

	/*
	 * Now sleep.
	 *
	 * NOTE: self-deadlocks will throw (do a non-local return).
	 */
	if (ResProcSleep(ExclusiveLock, locallock, incrementSet) != STATUS_OK)
	{
		/*
		 * We failed as a result of a deadlock, see CheckDeadLock(). Quit now.
		 */
		LWLockRelease(partitionLock);
		DeadLockReport();
	}

	awaitedLock = NULL;

	/* Report change to non-waiting status */
	if (update_process_title)
	{
		set_ps_display(new_status, false);
		pfree(new_status);
	}
	pgstat_report_waiting(PGBE_WAITING_NONE);

	return;
}


/*
 * ResProcLockRemoveSelfAndWakeup -- awaken any processses waiting on a resource lock.
 *
 * Notes:
 *	It always remove itself from the waitlist.
 *	Need to only awaken enough as many waiters as the resource controlled by
 *	the the lock should allow!
 */
void
ResProcLockRemoveSelfAndWakeup(LOCK *lock)
{
	PROC_QUEUE *waitQueue = &(lock->waitProcs);
	int			queue_size = waitQueue->size;
	PGPROC	   *proc;
	uint32		hashcode;
	LWLockId	partitionLock;

	int			status;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	/*
	 * XXX: This code is ugly and hard to read -- it should be a lot simpler,
	 * especially when there are some odd cases (process sitting on its own
	 * wait-queue).
	 */

	Assert(queue_size >= 0);
	if (queue_size == 0)
	{
		return;
	}

	proc = (PGPROC *) waitQueue->links.next;

	while (queue_size-- > 0)
	{
		/*
		 * Get the portal we are waiting on, and then its set of increments.
		 */
		ResPortalTag portalTag;
		ResPortalIncrement *incrementSet;

		/* Our own process may be on our wait-queue! */
		if (proc->pid == MyProc->pid)
		{
			PGPROC	   *nextproc;

			nextproc = (PGPROC *) proc->links.next;

			SHMQueueDelete(&(proc->links));
			(proc->waitLock->waitProcs.size)--;

			proc = nextproc;

			continue;
		}

		MemSet(&portalTag, 0, sizeof(ResPortalTag));
		portalTag.pid = proc->pid;
		portalTag.portalId = proc->waitPortalId;

		incrementSet = ResIncrementFind(&portalTag);
		if (!incrementSet)
		{
			hashcode = LockTagHashCode(&(lock->tag));
			partitionLock = LockHashPartitionLock(hashcode);

			LWLockRelease(partitionLock);
			elog(ERROR, "no increment data for  portal id %u and pid %d", proc->waitPortalId, proc->pid);
		}

		/*
		 * See if it is ok to wake this guy. (note that the wakeup writes to
		 * the wait list, and gives back a *new* next proc).
		 */
		status = ResLockCheckLimit(lock, (PROCLOCK *) proc->waitProcLock, incrementSet, true);
		if (status == STATUS_OK)
		{
			ResGrantLock(lock, (PROCLOCK *) proc->waitProcLock);
			ResLockUpdateLimit(lock, (PROCLOCK *) proc->waitProcLock, incrementSet, true, false);

			proc = ResProcWakeup(proc, STATUS_OK);
		}
		else
		{
			/* Otherwise move on to the next guy. */
			proc = (PGPROC *) proc->links.next;
		}
	}

	Assert(waitQueue->size >= 0);

	return;
}


/*
 * ResProcWakeup -- wake a sleeping process.
 *
 * (could we just use ProcWakeup here?)
 */
PGPROC *
ResProcWakeup(PGPROC *proc, int waitStatus)
{
	PGPROC	   *retProc;

	/* Proc should be sleeping ... */
	if (proc->links.prev == NULL ||
		proc->links.next == NULL)
		return NULL;

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
 * ResRemoveFromWaitQueue -- Remove a process from the wait queue, cleaning up
 *	any locks.
 */
void
ResRemoveFromWaitQueue(PGPROC *proc, uint32 hashcode)
{
	LOCK	   *waitLock = proc->waitLock;
	PROCLOCK   *proclock = proc->waitProcLock;
	LOCKMODE	lockmode = proc->waitLockMode;
#ifdef USE_ASSERT_CHECKING
	LOCKMETHODID lockmethodid = LOCK_LOCKMETHOD(*waitLock);
#endif   /* USE_ASSERT_CHECKING */
	ResPortalTag portalTag;

	/* Make sure lockmethod is for a resource lock. */
	Assert(lockmethodid == RESOURCE_LOCKMETHOD);

	/* Make sure proc is waiting */
	Assert(proc->links.next != NULL);
	Assert(waitLock);
	Assert(waitLock->waitProcs.size > 0);

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

	/* Clean up the proc's own state */
	proc->waitLock = NULL;
	proc->waitProcLock = NULL;

	/*
	 * Remove the waited on portal increment.
	 */
	MemSet(&portalTag, 0, sizeof(ResPortalTag));
	portalTag.pid = MyProc->pid;
	portalTag.portalId = MyProc->waitPortalId;

	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);
	ResIncrementRemove(&portalTag);

	/*
	 * Delete the proclock immediately if it represents no already-held locks.
	 * (This must happen now because if the owner of the lock decides to
	 * release it, and the requested/granted counts then go to zero,
	 * LockRelease expects there to be no remaining proclocks.) Then see if
	 * any other waiters for the lock can be woken up now.
	 */
	ResCleanUpLock(waitLock, (PROCLOCK *) proclock, hashcode, true);
	LWLockRelease(ResQueueLock);

}


/*
 * ResCheckSelfDeadLock -- Check to see if I am going to deadlock myself.
 *
 * What happens here is we scan our own set of portals and total up the
 * increments. If this exceeds any of the thresholds for the queue then
 * we need to signal that a self deadlock is about to occurr - modulo some
 * footwork for overcommit-able queues.
 */
bool
ResCheckSelfDeadLock(LOCK *lock, PROCLOCK *proclock, ResPortalIncrement *incrementSet)
{
	ResQueue	queue;
	ResLimit	limits;
	int			i;
	Cost		incrementTotals[NUM_RES_LIMIT_TYPES];
	int			numPortals = 0;
	bool		countThesholdOvercommitted = false;
	bool		costThesholdOvercommitted = false;
	bool		memoryThesholdOvercommitted = false;
	bool		result = false;

	/* Get the resource queue lock before checking the increments. */
	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	/* Get the queue for this lock. */
	queue = GetResQueueFromLock(lock);
	limits = queue->limits;

	/* Get the increment totals and number of portals for this queue. */
	TotalResPortalIncrements(MyProc->pid, queue->queueid,
							 incrementTotals, &numPortals);

	/*
	 * Now check them against the thresholds using the same logic as
	 * ResLockCheckLimit.
	 */
	for (i = 0; i < NUM_RES_LIMIT_TYPES; i++)
	{
		if (limits[i].threshold_value == INVALID_RES_LIMIT_THRESHOLD)
		{
			continue;
		}

		switch (limits[i].type)
		{
			case RES_COUNT_LIMIT:
				{
					if (incrementTotals[i] > limits[i].threshold_value)
					{
						countThesholdOvercommitted = true;
					}
				}
				break;

			case RES_COST_LIMIT:
				{
					if (incrementTotals[i] > limits[i].threshold_value)
					{
						costThesholdOvercommitted = true;
					}
				}

			case RES_MEMORY_LIMIT:
				{
					if (incrementTotals[i] > limits[i].threshold_value)
					{
						memoryThesholdOvercommitted = true;
					}
				}
				break;
		}
	}

	/* If any threshold is overcommitted then set the result. */
	if (countThesholdOvercommitted || costThesholdOvercommitted || memoryThesholdOvercommitted)
	{
		result = true;
	}

	/*
	 * If the queue can be overcommited and we are overcommitting with 1
	 * portal and *not* overcommitting the count threshold then don't trigger
	 * a self deadlock.
	 */
	if (queue->overcommit && numPortals == 1 && !countThesholdOvercommitted)
	{
		result = false;
	}

	if (result)
	{
		/*
		 * We're about to abort out of a partially completed lock acquisition.
		 *
		 * In order to allow our ref-counts to figure out how to clean things
		 * up we're going to "grant" the lock, which will immediately be
		 * cleaned up when our caller throws an ERROR.
		 */
		if (lock->nRequested > lock->nGranted)
		{
			/* we're no longer waiting. */
			pgstat_report_waiting(PGBE_WAITING_NONE);
			ResGrantLock(lock, proclock);
			ResLockUpdateLimit(lock, proclock, incrementSet, true, true);
		}
		/* our caller will throw an ERROR. */
	}

	LWLockRelease(ResQueueLock);

	return result;
}


/*
 * ResPortalIncrementHashTableInit - Initialize the increment hash.
 *
 * Notes:
 *	This stores the possible increments that a given statement will cause to
 *	be added to the limits for a resource queue.
 *	We allocate one extra slot for each backend, to free us from counting
 *	un-named portals.
 */
bool
ResPortalIncrementHashTableInit(void)
{
	HASHCTL		info;
	long		max_table_size = (MaxResourcePortalsPerXact + 1) * MaxBackends;
	int			hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(ResPortalTag);
	info.entrysize = sizeof(ResPortalIncrement);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	ResPortalIncrementHash = ShmemInitHash("Portal Increment Hash",
										   max_table_size / 2,
										   max_table_size,
										   &info,
										   hash_flags);

	if (!ResPortalIncrementHash)
	{
		return false;
	}

	return true;
}


/*
 * ResIncrementAdd -- Add a new increment element to the increment hash.
 *
 * Notes:
 *	Return a pointer to where the new increment is stored, or NULL if we
 *	are out of memory (or if we find a duplicate portalid).
 *
 *	The resource queue lightweight lock (ResQueueLock) *must* be held for
 *	this operation.
 */
static ResPortalIncrement *
ResIncrementAdd(ResPortalIncrement *incSet, PROCLOCK *proclock, ResourceOwner owner)
{
	ResPortalIncrement *incrementSet;
	ResPortalTag portaltag;
	int			i;
	bool		found;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	/* Set up the key. */
	MemSet(&portaltag, 0, sizeof(ResPortalTag));
	portaltag.pid = incSet->pid;
	portaltag.portalId = incSet->portalId;

	/* Add (or find) the value. */
	incrementSet = (ResPortalIncrement *)
		hash_search(ResPortalIncrementHash, (void *) &portaltag, HASH_ENTER_NULL, &found);

	if (!incrementSet)
	{
		return NULL;
	}

	/* Initialize it. */
	if (!found)
	{
		incrementSet->pid = incSet->pid;
		incrementSet->portalId = incSet->portalId;
		incrementSet->isHold = incSet->isHold;
		incrementSet->isCommitted = false;
		for (i = 0; i < NUM_RES_LIMIT_TYPES; i++)
		{
			incrementSet->increments[i] = incSet->increments[i];
		}
		SHMQueueInsertBefore(&proclock->portalLinks, &incrementSet->portalLink);
	}
	else
	{
		/* We have added this portId before - something has gone wrong! */

		elog(WARNING, "duplicate portal id %u for proc %d", incSet->portalId, incSet->pid);
		incrementSet = NULL;
	}

	return incrementSet;
}


/*
 * ResIncrementFind -- Find the increment for a portal and process.
 *
 * Notes
 *	Return a pointer to where the new increment is stored (NULL if not found).
 *
 *	The resource queue lightweight lock (ResQueueLock) *must* be held for
 *	this operation.
 */
ResPortalIncrement *
ResIncrementFind(ResPortalTag *portaltag)
{
	ResPortalIncrement *incrementSet;
	bool		found;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	incrementSet = (ResPortalIncrement *)
		hash_search(ResPortalIncrementHash, (void *) portaltag, HASH_FIND, &found);

	if (!incrementSet)
	{
		return NULL;
	}

	return incrementSet;
}


/*
 * ResIncrementRemove -- Remove a  increment for a portal and process.
 *
 * Notes
 *	The resource queue lightweight lock (ResQueueLock) *must* be held for
 *	this operation.
 */
static bool
ResIncrementRemove(ResPortalTag *portaltag)
{
	ResPortalIncrement *incrementSet;
	bool		found;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	incrementSet = (ResPortalIncrement *)
		hash_search(ResPortalIncrementHash, (void *) portaltag, HASH_REMOVE, &found);

	if (incrementSet == NULL)
	{
		return false;
	}

	SHMQueueDelete(&incrementSet->portalLink);

	return true;
}


/*
 * ResQueueHashTableInit -- initialize the hash table of resource queues.
 *
 * Notes:
 */
bool
ResQueueHashTableInit(void)
{
	HASHCTL		info;
	int			hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(Oid);
	info.entrysize = sizeof(ResQueueData);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

#ifdef RESLOCK_DEBUG
	elog(DEBUG1, "Creating hash table for %d queues", MaxResourceQueues);
#endif

	ResQueueHash = ShmemInitHash("Queue Hash",
								 MaxResourceQueues,
								 MaxResourceQueues,
								 &info,
								 hash_flags);

	if (!ResQueueHash)
		return false;

	return true;
}

/*
 * ResQueuehashNew -- return a new (empty) queue object to initialize.
 *
 * Notes
 *	The resource queue lightweight lock (ResQueueLock) *must* be held for
 *	this operation.
 */
ResQueue
ResQueueHashNew(Oid queueid)
{
	bool		found;
	ResQueueData *queue;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	queue = (ResQueueData *)
		hash_search(ResQueueHash, (void *) &queueid, HASH_ENTER_NULL, &found);

	/* caller should test that the queue does not exist already */
	Assert(!found);

	if (!queue)
		return NULL;

	return (ResQueue) queue;
}

/*
 * ResQueueHashFind -- return the queue for a given oid.
 *
 * Notes
 *	The resource queue lightweight lock (ResQueueLock) *must* be held for
 *	this operation.
 */
ResQueue
ResQueueHashFind(Oid queueid)
{
	bool		found;
	ResQueueData *queue;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	queue = (ResQueueData *)
		hash_search(ResQueueHash, (void *) &queueid, HASH_FIND, &found);

	if (!queue)
		return NULL;

	return (ResQueue) queue;
}


/*
 * ResQueueHashRemove -- remove the queue for a given oid.
 *
 * Notes
 *	The resource queue lightweight lock (ResQueueLock) *must* be held for
 *	this operation.
 */
bool
ResQueueHashRemove(Oid queueid)
{
	bool		found;
	void	   *queue;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));

	queue = hash_search(ResQueueHash, (void *) &queueid, HASH_REMOVE, &found);
	if (!queue)
		return false;

	return true;
}

/* Number of columns produced by pg_resqueue_status() */
#define PG_RESQUEUE_STATUS_COLUMNS 5

/*
 * pg_resqueue_status - produce a view with one row per resource queue
 *	showing internal information (counter values, waiters, holders).
 */
Datum
pg_resqueue_status(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx = NULL;
	Datum		result;
	MemoryContext oldcontext = NULL;
	QueueStatusContext *fctx = NULL;	/* User function context. */
	HeapTuple	tuple = NULL;

	if (SRF_IS_FIRSTCALL())
	{

		funcctx = SRF_FIRSTCALL_INIT();

		/* Switch context when allocating stuff to be used in later calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		fctx = (QueueStatusContext *) palloc(sizeof(QueueStatusContext));

		/*
		 * Allocate space for the per-call area - this overestimates, but
		 * means we can take the resource rescheduler lock after our memory
		 * context switching.
		 */
		fctx->record = (QueueStatusRec *) palloc(sizeof(QueueStatusRec) * MaxResourceQueues);

		funcctx->user_fctx = fctx;

		/* Construct a tuple descriptor for the result rows. */
		TupleDesc	tupledesc = CreateTemplateTupleDesc(PG_RESQUEUE_STATUS_COLUMNS, false);

		TupleDescInitEntry(tupledesc, (AttrNumber) 1, "queueid", OIDOID, -1, 0);
		TupleDescInitEntry(tupledesc, (AttrNumber) 2, "queuecountvalue", FLOAT4OID, -1, 0);
		TupleDescInitEntry(tupledesc, (AttrNumber) 3, "queuecostvalue", FLOAT4OID, -1, 0);
		TupleDescInitEntry(tupledesc, (AttrNumber) 4, "queuewaiters", INT4OID, -1, 0);
		TupleDescInitEntry(tupledesc, (AttrNumber) 5, "queueholders", INT4OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupledesc);

		/* Return to original context when allocating transient memory */
		MemoryContextSwitchTo(oldcontext);

		if (IsResQueueEnabled())
		{
			/* Get a snapshot of current state of resource queues */
			BuildQueueStatusContext(fctx);

			funcctx->max_calls = fctx->numRecords;
		}
		else
		{
			funcctx->max_calls = fctx->numRecords = 0;
		}
	}

	funcctx = SRF_PERCALL_SETUP();

	/* Get the saved state. */
	fctx = funcctx->user_fctx;

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		int			i = funcctx->call_cntr;
		QueueStatusRec *record = &fctx->record[i];
		Datum		values[PG_RESQUEUE_STATUS_COLUMNS];
		bool		nulls[PG_RESQUEUE_STATUS_COLUMNS];

		values[0] = ObjectIdGetDatum(record->queueid);
		nulls[0] = false;

		/* Make the counters null if the limit is disbaled. */
		if (record->queuecountthreshold != INVALID_RES_LIMIT_THRESHOLD)
		{
			values[1] = Float4GetDatum(record->queuecountvalue);
			nulls[1] = false;
		}
		else
			nulls[1] = true;

		if (record->queuecostthreshold != INVALID_RES_LIMIT_THRESHOLD)
		{
			values[2] = Float4GetDatum(record->queuecostvalue);
			nulls[2] = false;
		}
		else
			nulls[2] = true;


		values[3] = record->queuewaiters;
		nulls[3] = false;

		values[4] = record->queueholders;
		nulls[4] = false;

		/* Build and return the tuple. */
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
		SRF_RETURN_DONE(funcctx);
}

/**
 * This copies out the current state of resource queues.
 */
static void
BuildQueueStatusContext(QueueStatusContext *fctx)
{
	LWLockId	partitionLock;
	int			num_calls = 0;
	int			partition = 0;
	HASH_SEQ_STATUS status;
	ResQueueData *queue = NULL;

	Assert(fctx);
	Assert(fctx->record);

	/*
	 * Take all the partition locks. This is necessary as we want to to use
	 * the same lock order as the rest of the code - i.e. partition locks
	 * *first* *then* the queue lock (otherwise we could deadlock ourselves).
	 */
	for (partition = 0; partition < NUM_LOCK_PARTITIONS; partition++)
	{
		partitionLock = FirstLockMgrLock + partition;
		LWLockAcquire(partitionLock, LW_EXCLUSIVE);
	}

	/*
	 * Lock resource queue structures.
	 */
	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	/* Initialize for a sequential scan of the resource queue hash. */
	hash_seq_init(&status, ResQueueHash);
	num_calls = hash_get_num_entries(ResQueueHash);
	Assert(num_calls == ResScheduler->num_queues);

	int			i = 0;

	while ((queue = (ResQueueData *) hash_seq_search(&status)) != NULL)
	{
		int			j;
		ResLimit	limits = NULL;
		uint32		hashcode;

		/**
		 * Gather thresholds and current values on activestatements, cost and memory
		 */
		limits = queue->limits;

		fctx->record[i].queueid = queue->queueid;

		for (j = 0; j < NUM_RES_LIMIT_TYPES; j++)
		{
			switch (limits[j].type)
			{
				case RES_COUNT_LIMIT:
					{
						fctx->record[i].queuecountthreshold =
							limits[j].threshold_value;

						fctx->record[i].queuecountvalue =
							limits[j].current_value;
					}
					break;

				case RES_COST_LIMIT:
					{
						fctx->record[i].queuecostthreshold =
							limits[j].threshold_value;

						fctx->record[i].queuecostvalue =
							limits[j].current_value;
					}
					break;

				case RES_MEMORY_LIMIT:
					{
						fctx->record[i].queuememthreshold =
							limits[j].threshold_value;

						fctx->record[i].queuememvalue =
							limits[j].current_value;
					}
					break;

				default:
					Assert(false && "Should never reach here!");
			}
		}

		/*
		 * Get the holders and waiters count for the corresponding resource
		 * lock.
		 */
		LOCKTAG		tag;
		LOCK	   *lock;

		SET_LOCKTAG_RESOURCE_QUEUE(tag, queue->queueid);
		hashcode = LockTagHashCode(&tag);

		bool		found = false;

		lock = (LOCK *)
			hash_search_with_hash_value(LockMethodLockHash, (void *) &tag, hashcode, HASH_FIND, &found);

		if (!found || !lock)
		{
			fctx->record[i].queuewaiters = 0;
			fctx->record[i].queueholders = 0;
		}
		else
		{
			fctx->record[i].queuewaiters = lock->nRequested - lock->nGranted;
			fctx->record[i].queueholders = lock->nGranted;
		}

		i++;
		Assert(i <= MaxResourceQueues);
	}

	/* Release the resource scheduler lock. */
	LWLockRelease(ResQueueLock);

	/* ...and the partition locks. */
	for (partition = NUM_LOCK_PARTITIONS; --partition >= 0;)
	{
		partitionLock = FirstLockMgrLock + partition;
		LWLockRelease(partitionLock);
	}

	/* Set the real no. of calls as we know it now! */
	fctx->numRecords = i;
	return;
}

/* Number of records produced per queue. */
#define PG_RESQUEUE_STATUS_KV_RECORDS_PER_QUEUE 8

/* Number of columns produced by function */
#define PG_RESQUEUE_STATUS_KV_COLUMNS 3

/* Scratch space used to write out strings */
#define PG_RESQUEUE_STATUS_KV_BUFSIZE 256

/*
 * pg_resqueue_status_extended - outputs the current state of resource queues in the following format:
 * (queueid, key, value) where key and value are text. This makes the function extremely flexible.
 */
Datum
pg_resqueue_status_kv(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx = NULL;
	Datum		result;
	MemoryContext oldcontext = NULL;
	QueueStatusContext *fctx = NULL;	/* User function context. */
	HeapTuple	tuple = NULL;

	if (SRF_IS_FIRSTCALL())
	{

		funcctx = SRF_FIRSTCALL_INIT();

		/* Switch context when allocating stuff to be used in later calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		fctx = (QueueStatusContext *) palloc(sizeof(QueueStatusContext));

		/*
		 * Allocate space for the per-call area - this overestimates, but
		 * means we can take the resource rescheduler lock after our memory
		 * context switching.
		 */
		fctx->record = (QueueStatusRec *) palloc(sizeof(QueueStatusRec) * MaxResourceQueues);

		funcctx->user_fctx = fctx;

		/* Construct a tuple descriptor for the result rows. */
		TupleDesc	tupledesc = CreateTemplateTupleDesc(PG_RESQUEUE_STATUS_KV_COLUMNS, false);

		TupleDescInitEntry(tupledesc, (AttrNumber) 1, "queueid", OIDOID, -1, 0);
		TupleDescInitEntry(tupledesc, (AttrNumber) 2, "key", TEXTOID, -1, 0);
		TupleDescInitEntry(tupledesc, (AttrNumber) 3, "value", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupledesc);

		/* Return to original context when allocating transient memory */
		MemoryContextSwitchTo(oldcontext);

		if (IsResQueueEnabled())
		{
			/* Get a snapshot of current state of resource queues */
			BuildQueueStatusContext(fctx);

			funcctx->max_calls = fctx->numRecords * PG_RESQUEUE_STATUS_KV_RECORDS_PER_QUEUE;
		}
		else
		{
			funcctx->max_calls = fctx->numRecords = 0;
		}
	}

	funcctx = SRF_PERCALL_SETUP();

	/* Get the saved state. */
	fctx = funcctx->user_fctx;

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		int			i = funcctx->call_cntr / PG_RESQUEUE_STATUS_KV_RECORDS_PER_QUEUE;	/* record number */
		int			j = funcctx->call_cntr % PG_RESQUEUE_STATUS_KV_RECORDS_PER_QUEUE;	/* which attribute is
																						 * being produced */
		QueueStatusRec *record = &fctx->record[i];
		Datum		values[PG_RESQUEUE_STATUS_KV_COLUMNS];
		bool		nulls[PG_RESQUEUE_STATUS_KV_COLUMNS];
		char		buf[PG_RESQUEUE_STATUS_KV_BUFSIZE];

		nulls[0] = false;
		nulls[1] = false;
		nulls[2] = false;

		values[0] = ObjectIdGetDatum(record->queueid);

		switch (j)
		{
			case 0:
				values[1] = PointerGetDatum(cstring_to_text("rsqcountlimit"));
				snprintf(buf, ARRAY_SIZE(buf), "%d", (int) ceil(record->queuecountthreshold));
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 1:
				values[1] = PointerGetDatum(cstring_to_text("rsqcountvalue"));
				snprintf(buf, ARRAY_SIZE(buf), "%d", (int) ceil(record->queuecountvalue));
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 2:
				values[1] = PointerGetDatum(cstring_to_text("rsqcostlimit"));
				snprintf(buf, ARRAY_SIZE(buf), "%.2f", record->queuecostthreshold);
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 3:
				values[1] = PointerGetDatum(cstring_to_text("rsqcostvalue"));
				snprintf(buf, ARRAY_SIZE(buf), "%.2f", record->queuecostvalue);
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 4:
				values[1] = PointerGetDatum(cstring_to_text("rsqmemorylimit"));
				snprintf(buf, ARRAY_SIZE(buf), "%.2f", record->queuememthreshold);
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 5:
				values[1] = PointerGetDatum(cstring_to_text("rsqmemoryvalue"));
				snprintf(buf, ARRAY_SIZE(buf), "%.2f", record->queuememvalue);
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 6:
				values[1] = PointerGetDatum(cstring_to_text("rsqwaiters"));
				snprintf(buf, ARRAY_SIZE(buf), "%d", record->queuewaiters);
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			case 7:
				values[1] = PointerGetDatum(cstring_to_text("rsqholders"));
				snprintf(buf, ARRAY_SIZE(buf), "%d", record->queueholders);
				values[2] = PointerGetDatum(cstring_to_text(buf));
				break;
			default:
				Assert(false && "Cannot reach here");
		}

		/* Build and return the tuple. */
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
		SRF_RETURN_DONE(funcctx);
}

 /**
  * What is the memory limit on a queue per the catalog in bytes. Returns -1 if not set.
  */
int64 ResourceQueueGetMemoryLimitInCatalog(Oid queueId)
{
	int memoryLimitKB = -1;

	Assert(queueId != InvalidOid);

	List * capabilitiesList = GetResqueueCapabilityEntry(queueId); /* This is a list of lists */

	ListCell *le = NULL;
	foreach(le, capabilitiesList)
	{
		List *entry = NULL;
		Value *key = NULL;
		entry = (List *) lfirst(le);
		Assert(entry);
		key = (Value *) linitial(entry);
		Assert(key->type == T_Integer); /* This is resource type id */
		if (intVal(key) == PG_RESRCTYPE_MEMORY_LIMIT)
		{
			Value *val = lsecond(entry);
			Assert(val->type == T_String);

#ifdef USE_ASSERT_CHECKING
			bool result =

#endif
					parse_int(strVal(val), &memoryLimitKB, GUC_UNIT_KB, NULL);

			Assert(result);
		}
	}
	list_free(capabilitiesList);

	Assert(memoryLimitKB == -1 || memoryLimitKB > 0);

	if (memoryLimitKB == -1)
	{
		return (int64) -1;
	}

	return (int64) memoryLimitKB * 1024;

}

/**
 * Get memory limit associated with queue in bytes.
 * Returns -1 if a limit does not exist.
 */
int64 ResourceQueueGetMemoryLimit(Oid queueId)
{
	int64 memoryLimitBytes = -1;

	Assert(queueId != InvalidOid);

	if (!IsResManagerMemoryPolicyNone())
	{
		memoryLimitBytes = ResourceQueueGetMemoryLimitInCatalog(queueId);
	}

	return memoryLimitBytes;
}

/**
 * Given a queueid, how much memory should a query take in bytes.
 */
uint64 ResourceQueueGetQueryMemoryLimit(PlannedStmt *stmt, Oid queueId)
{
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);
	Assert(queueId != InvalidOid);


	/* resource queue will not limit super user */
	if (superuser())
		return ResourceQueueGetSuperuserQueryMemoryLimit();

	if (IsResManagerMemoryPolicyNone())
		return 0;

	/** Assert that I do not hold lwlock */
	Assert(!LWLockHeldExclusiveByMe(ResQueueLock));

	int64 resqLimitBytes = ResourceQueueGetMemoryLimit(queueId);

	/**
	 * If there is no memory limit on the queue, simply use statement_mem.
	 */
	AssertImply(resqLimitBytes < 0, resqLimitBytes == -1);
	if (resqLimitBytes == -1)
	{
		return (uint64) statement_mem * 1024L;
	}

	/**
	 * This method should only be called while holding exclusive lock on ResourceQueues. This means
	 * that nobody can modify any resource queue while current process is performing this computation.
	 */
	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	ResQueue resQueue = ResQueueHashFind(queueId);

	LWLockRelease(ResQueueLock);

	Assert(resQueue);
	int numSlots = (int) ceil(resQueue->limits[RES_COUNT_LIMIT].threshold_value);
	double costLimit = (double) resQueue->limits[RES_COST_LIMIT].threshold_value;
	double planCost = stmt->planTree->total_cost;

	if (planCost < 1.0)
		planCost = 1.0;

	Assert(planCost > 0.0);

	if (LogResManagerMemory())
	{
		elog(GP_RESMANAGER_MEMORY_LOG_LEVEL, "numslots: %d, costlimit: %f", numSlots, costLimit);
	}

	if (numSlots < 1)
	{
		/** there is no statement limit set */
		numSlots = 1;
	}

	if (costLimit < 0.0)
	{
		/** there is no cost limit set */
		costLimit = planCost;
	}

	double minRatio = Min( 1.0/ (double) numSlots, planCost / costLimit);

	minRatio = Min(minRatio, 1.0);

	if (LogResManagerMemory())
	{
		elog(GP_RESMANAGER_MEMORY_LOG_LEVEL, "slotratio: %0.3f, costratio: %0.3f, minratio: %0.3f",
				1.0/ (double) numSlots, planCost / costLimit, minRatio);
	}

	uint64 queryMem = (uint64) resqLimitBytes * minRatio;

	/**
	 * If user requests more using statement_mem, grant that.
	 */
	if (queryMem < statement_mem * 1024L)
	{
		queryMem = (uint64) statement_mem * 1024L;
	}

	return queryMem;
}

/**
 * How much memory should superuser queries get?
 */
static uint64 ResourceQueueGetSuperuserQueryMemoryLimit(void)
{
	Assert(superuser());
	return (uint64) statement_mem * 1024L;
}
