/*-------------------------------------------------------------------------
 *
 * resscheduler.c
 *	  POSTGRES resource scheduling management code.
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resscheduler/resscheduler.c
 *
 *
-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/indexing.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/print.h"
#include "optimizer/planner.h"
#include "pgstat.h"
#include "rewrite/rewriteHandler.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/lmgr.h"
#include "tcop/tcopprot.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/guc.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/resscheduler.h"
#include "utils/syscache.h"

/*
 * GUC variables.
 */
char                		*gp_resqueue_memory_policy_str = NULL;
ResManagerMemoryPolicy     	gp_resqueue_memory_policy = RESMANAGER_MEMORY_POLICY_NONE;
bool						gp_log_resqueue_memory = false;
int							gp_resqueue_memory_policy_auto_fixed_mem;
bool						gp_resqueue_print_operator_memory_limits = false;


int		MaxResourceQueues;						/* Max # of queues. */
int		MaxResourcePortalsPerXact;				/* Max # tracked portals -
												 * per backend . */
bool	ResourceSelectOnly;						/* Only lock SELECT/DECLARE? */
bool	ResourceCleanupIdleGangs;				/* Cleanup idle gangs? */


/*
 * Global variables
 */
ResSchedulerData	*ResScheduler;	/* Resource Scheduler (shared) data .*/
Oid				MyQueueId = InvalidOid;	/* resource queue for current role. */
static uint32	portalId = 0;		/* id of portal, for tracking cursors. */
static int32	numHoldPortals = 0;	/* # of holdable cursors tracked. */

/*
 * ResSchedulerShmemSize -- estimate size the schedular structures will need in
 *	shared memory.
 *
 */
Size
ResSchedulerShmemSize(void)
{
	Size		size;

	/* The hash of queues.*/
	size = hash_estimate_size(MaxResourceQueues, sizeof(ResQueueData));

	/* The scheduler structure. */
	size = add_size(size, sizeof(ResSchedulerData));

	/* Add a safety margin */
	size = add_size(size, size / 10);


	return size;
}


/*
 * ResPortalIncrementShmemSize -- Estimate the size of the increment hash.
 *
 * Notes
 *	We allow one extra slot for unnamed portals, as it is simpler to not
 *	count them at all.
 */
Size 
ResPortalIncrementShmemSize(void)
{

	Size	size;
	long	max_table_size = (MaxResourcePortalsPerXact + 1) * MaxBackends;

	size = hash_estimate_size(max_table_size, sizeof(ResPortalIncrement));

	/* Add a safety margin */
	size = add_size(size, size / 10);

	return size;
}


/*
 * InitResScheduler -- initialize the schedular queues hash in shared memory.
 *
 * The queuek hash table has no data in it as yet (InitResQueues cannot be 
 * called until catalog access is available.
 *
 */
void
InitResScheduler(void)
{
	bool		found;

	/* Create the scheduler structure. */
	ResScheduler = (ResSchedulerData *)
		ShmemInitStruct("Resource Scheduler Data",
						sizeof(ResSchedulerData),
						&found);

	/* Specify that we have no initialized queues yet. */
	ResScheduler->num_queues = 0;

	if (found)
		return;			/* We are already initialized. */

	/* Create the resource queue hash (empty at this point). */
	found = ResQueueHashTableInit();
	if (!found)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("not enough shared memory for resource scheduler")));

	elog(DEBUG1, "initialized resource scheduler");
	

	return;
}


/*
 * InitResPortalIncrementHash - Initialize the portal increment hash.
 *
 * Notes:
 *	Simply calls the internal initialization function.
 */
void
InitResPortalIncrementHash(void)
{
	bool	result;


	result = ResPortalIncrementHashTableInit();

	if (!result)
		elog(FATAL, "could not initialize portal increment hash");

	elog(DEBUG1, "initialized portal increment hash");
}


/*
 * InitResQueues -- initialize the resource queues in shared memory. Note this
 * can only be done after enough setup has been done. This uses
 * heap_open etc which in turn requires shared memory to be set up.
 */

void 
InitResQueues(void)
{
	HeapTuple			tuple;
	int					numQueues = 0;
	bool				queuesok = true;
	SysScanDesc sscan;
	
	Assert(ResScheduler);

	/*
	 * Need a resource owner to keep the heapam code happy.
	 */
	Assert(CurrentResourceOwner == NULL);

	ResourceOwner owner = ResourceOwnerCreate(NULL, "InitQueues");
	CurrentResourceOwner = owner;
	
	/**
	 * The resqueue shared mem initialization must be serialized. Only the first session
	 * should do the init.
	 * Serialization is done the ResQueueLock LW_EXCLUSIVE. However, we must obtain all DB
	 * lock before obtaining LWlock.
	 * So, we must have obtained ResQueueRelationId and ResQueueCapabilityRelationId lock
	 * first.
	 */
	/* XXX XXX: should this be rowexclusive ? */
	Relation relResqueue = heap_open(ResQueueRelationId, AccessShareLock);
	LockRelationOid(ResQueueCapabilityRelationId, RowExclusiveLock);
	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	if (ResScheduler->num_queues > 0)
	{
		/* Hash table has already been loaded */
		LWLockRelease(ResQueueLock);
		UnlockRelationOid(ResQueueCapabilityRelationId, RowExclusiveLock);
		heap_close(relResqueue, AccessShareLock);
		CurrentResourceOwner = NULL;
		ResourceOwnerDelete(owner);
		return;
	}

	sscan = systable_beginscan(relResqueue, InvalidOid, false, SnapshotNow, 0, NULL);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Form_pg_resqueue	queueform;
		Oid					queueid;
		bool				overcommit;
		float4				ignorelimit;
		Cost				thresholds[NUM_RES_LIMIT_TYPES];
		char				*queuename;

		numQueues++;

		queueform = (Form_pg_resqueue) GETSTRUCT(tuple);

		queueid = HeapTupleGetOid(tuple);
		queuename = NameStr(queueform->rsqname);
		thresholds[RES_COUNT_LIMIT] = queueform->rsqcountlimit;
		thresholds[RES_COST_LIMIT] = queueform->rsqcostlimit;

		thresholds[RES_MEMORY_LIMIT] = ResourceQueueGetMemoryLimit(queueid);
		overcommit = queueform->rsqovercommit;
		ignorelimit = queueform->rsqignorecostlimit;
		queuesok = ResCreateQueue(queueid, thresholds, overcommit, ignorelimit);

		if (!queuesok)
		{
			/** Break out of loop. Close relations, relinquish LWLock and then error out */ 
			break;
		}
	}

	systable_endscan(sscan);
	LWLockRelease(ResQueueLock);
	UnlockRelationOid(ResQueueCapabilityRelationId, RowExclusiveLock);
	heap_close(relResqueue, AccessShareLock);

	if (!queuesok)
		ereport(PANIC,
			(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
			 errmsg("insufficient resource queues available"),
		errhint("Increase max_resource_queues to %d.", numQueues)));


	elog(LOG,"initialized %d resource queues", numQueues);

	CurrentResourceOwner = NULL;
	ResourceOwnerDelete(owner);

	return;
}


/*
 * ResCreateQueue -- initialize the elements for a resource queue.
 *
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this - unless we are the startup process.
 */
bool
ResCreateQueue(Oid queueid, Cost limits[NUM_RES_LIMIT_TYPES], bool overcommit,
			   float4 ignorelimit)
{

	ResQueue		queue;
	int				i;

	Assert(LWLockHeldExclusiveByMe(ResQueueLock));
	
	/* If the new queue pointer is NULL, then we are out of queueus. */
	if (ResScheduler->num_queues >= MaxResourceQueues)
		return false;

	/**
	 * Has an entry for this 
	 */
	
	queue = ResQueueHashNew(queueid);
	Assert(queue != NULL);
	
	/* Set queue oid and offset in the schedular array */
	queue->queueid = queueid;

	/* Set the number of limits 0 initially. */
	queue->num_limits = 0;

	/* Set overcommit.*/
	queue->overcommit = overcommit;

	/* Set ignore cost limit. */
	queue->ignorecostlimit = ignorelimit;

	/* Now run through all the possible limit types.*/
	for (i = 0 ; i < NUM_RES_LIMIT_TYPES; i++)
	{
		/*
		 * Specific initializations for the limit types (the current two are
		 * in fact the same).
		 */
		switch (i)
		{
			case RES_COUNT_LIMIT:
			case RES_COST_LIMIT:
			case RES_MEMORY_LIMIT:
				{
					queue->num_limits++;
					queue->limits[i].type = i;
					queue->limits[i].threshold_value = limits[i];
					queue->limits[i].current_value = 0;
					queue->limits[i].threshold_is_max = true;
					break;
				}
			default:
				elog(ERROR, "unknown resource limit type %d", i);
			break;
		}


	}
	ResScheduler->num_queues++;
	return true;
}


/*
 * ResAlterQueue -- Change thresholds, overcommit for a resource queue.
 *
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this.
 */
ResAlterQueueResult
ResAlterQueue(Oid queueid, Cost limits[NUM_RES_LIMIT_TYPES], bool overcommit,
			  float4 ignorelimit)
{

	ResQueue		queue = ResQueueHashFind(queueid);
	bool			result = ALTERQUEUE_OK;
	int				i;
	

	/* Handed an Oid for something other than a queue, bail out! */
	if (queue == NULL)
		return ALTERQUEUE_ERROR;


	/* 
	 * Now run through all the possible limit types, checking all the
	 * intended changes are ok.
	 */
	for (i = 0 ; i < NUM_RES_LIMIT_TYPES; i++)
	{
		/* Don't sanity check thresholds if that are about to be disabled. */
		if (limits[i] == INVALID_RES_LIMIT_THRESHOLD)
			continue;

		/*
		 * MPP-4340:
		 * The goal of the resource queues is to allow us to throttle work
		 * onto the cluster: a key requirement is that given a queue we be able to
		 * change the limit values -- especially in a situation where we're already
		 * overloading the system, we'd like to be able to drop the queue limit.
		 *
		 * The original sanity checks here disallowed modifications which might
		 * reduce the threshold. To throttle the system for better performance,
		 * work had to get cancelled. That's an unacceptable limitation; so
		 * now we allow the queues to be changed.
		 *
		 * Overcommit is a much more serious type of change.
		 *
		 * For overcommitable limits we need to ensure the queue is not in 
		 * an overcommited state if we about to turn overcommit off.
		 */
		switch (i)
		{
			case RES_COUNT_LIMIT:
				break;

			case RES_COST_LIMIT:
			{
				/* 
				 * Don't turn overcommit off if queue could be overcommitted.
				 * We err on the side of caution in this case and demand that
				 * the queue is idle - we don't want to leave a query waiting 
				 * forever in the wait queue. We use the same roundoff limit
				 * value (0.1) as ResLockCheckLimit does.
				 */
				if (!overcommit && queue->overcommit && 
					(queue->limits[i].current_value > 0.1))
					result = ALTERQUEUE_OVERCOMMITTED;
			}
			
			break;
			
			case RES_MEMORY_LIMIT:
				break;
		}
	}

	/*
	 * If threshold and overcommit alterations are all ok, do the the changes.
	 */
	if (result == ALTERQUEUE_OK)
	{
		for (i = 0 ; i < NUM_RES_LIMIT_TYPES; i++)
		{
			/*
			 * Assign the new thresholds.
			 */
			switch (i)
			{
				case RES_COUNT_LIMIT:
				case RES_COST_LIMIT:
				case RES_MEMORY_LIMIT:
				{
					
					if (queue->limits[i].threshold_value != limits[i])
						queue->limits[i].threshold_value = limits[i];
				}
				break;
			}
	
		}

		/* Set overcommit if that has changed. */
		if (queue->overcommit != overcommit)
			queue->overcommit = overcommit;

		/* Set ignore cost limit if that has changed. */
		if (queue->ignorecostlimit != ignorelimit)
			queue->ignorecostlimit = ignorelimit;
	}
	
	return result;
}


/*
 * ResDestroyQueue -- destroy a resource queue.
 *
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this.
 */
bool
ResDestroyQueue(Oid queueid)
{

	ResQueue		queue = ResQueueHashFind(queueid);
	bool			allzero = true;
	bool			result;
	int				i;
	

	/* Handed an Oid for something other than a queue, bail out! */
	if (queue == NULL)
		return false;


	/* 
	 * Now run through all the possible limit types, checking all the
	 * current counters are zero.
	 */
	for (i = 0 ; i < NUM_RES_LIMIT_TYPES; i++)
	{
		/*
		 * Specific checks for the limit types (the current two are
		 * in fact the same).
		 */
		switch (i)
		{
			case RES_COUNT_LIMIT:
			case RES_COST_LIMIT:
			case RES_MEMORY_LIMIT:
			{
				
				if (queue->limits[i].current_value != 0)
					allzero = false;
			}
			break;
		}


	}

	/*
	 * No non-zero current values for the queue, are able to destroy it.
	 *
	 * Some care is needed, as this leaves the possibility of a connected
	 * user still using this queue via a ALTER ROLE ... RESOURCE QUEUE while
	 * he is connected. However this is no worse than being able to DROP a 
	 * ROLE that is corrently connected!
	 */
	if (allzero)
	{
		result = ResQueueHashRemove(queueid);
		if (result)
			ResScheduler->num_queues--;

		return result;
	}
	else
	{
		return false;
	}
	
}

/*
 * ResLockPortal -- get a resource lock for Portal execution.
 *
 * Returns:
 *	true if the lock has been taken
 *	false if the lock has been skipped.
 */
bool
ResLockPortal(Portal portal, QueryDesc *qDesc)
{
	bool		returnReleaseOk = false;	/* Release resource lock? */
	bool		takeLock;					/* Take resource lock? */
	LOCKTAG		tag;
	Oid			queueid;
	int32		lockResult = 0;
	ResPortalIncrement	incData;
	Plan *plan = NULL;

	Assert(qDesc);
	Assert(qDesc->plannedstmt);

	plan = qDesc->plannedstmt->planTree;

	queueid = portal->queueId;

	/* 
	 * Check we have a valid queue before going any further.
	 */
	if (queueid != InvalidOid)
	{

		/*
		 * Check the source tag to see if the original statement is suitable for
		 * locking. 
		 */
		switch (portal->sourceTag)
		{

			/*
			 * For INSERT/UPDATE/DELETE Skip if we have specified only SELECT,
			 * otherwise drop through to handle like a SELECT.
			 */
			case T_InsertStmt:
			case T_DeleteStmt:
			case T_UpdateStmt:
			{
				if (ResourceSelectOnly)
				{
					takeLock = false;
					returnReleaseOk = false;
					break;
				}
			}


			case T_SelectStmt:
			{
				/*
				 * Setup the resource portal increments, ready to be added.
				 */
				incData.pid = MyProc->pid;
				incData.portalId = portal->portalId;
				incData.increments[RES_COUNT_LIMIT] = 1;
				incData.increments[RES_COST_LIMIT] = ceil(plan->total_cost);

				if (!IsResManagerMemoryPolicyNone())
				{
					Assert(IsResManagerMemoryPolicyAuto() ||
						   IsResManagerMemoryPolicyEagerFree());
					
					uint64 queryMemory = qDesc->plannedstmt->query_mem;
					Assert(queryMemory > 0);
					if (LogResManagerMemory())
					{
						elog(GP_RESMANAGER_MEMORY_LOG_LEVEL, "query requested %.0fKB", (double) queryMemory / 1024.0);
					}					
					
					incData.increments[RES_MEMORY_LIMIT] = (Cost) queryMemory;
				}
				else 
				{
					Assert(IsResManagerMemoryPolicyNone());
					incData.increments[RES_MEMORY_LIMIT] = (Cost) 0.0;				
				}
				takeLock = true;
				returnReleaseOk = true;
			}
			break;
	
			/*
			 * We are declaring a cursor - do the same as T_SelectStmt, but
			 * need to additionally consider setting the isHold option.
			 */
			case T_DeclareCursorStmt:
			{
				/*
				 * Setup the resource portal increments, ready to be added.
				 */
				incData.pid = MyProc->pid;
				incData.portalId = portal->portalId;
				incData.increments[RES_COUNT_LIMIT] = 1;
				incData.increments[RES_COST_LIMIT] = ceil(plan->total_cost);
				incData.isHold = portal->cursorOptions & CURSOR_OPT_HOLD;

				if (!IsResManagerMemoryPolicyNone())
				{
					Assert(IsResManagerMemoryPolicyAuto() ||
						   IsResManagerMemoryPolicyEagerFree());
					
					uint64 queryMemory = qDesc->plannedstmt->query_mem;
					Assert(queryMemory > 0);
					if (LogResManagerMemory())
					{
						elog(NOTICE, "query requested %.0fKB", (double) queryMemory / 1024.0);
					}

					incData.increments[RES_MEMORY_LIMIT] = (Cost) queryMemory;
				}
				else 
				{
					Assert(IsResManagerMemoryPolicyNone());
					incData.increments[RES_MEMORY_LIMIT] = (Cost) 0.0;				
				}

				takeLock = true;
				returnReleaseOk = true;
			}
			break;
	
			/*
			 * We do not want to lock any of these query types.
			 */
			default:
			{
	
				takeLock = false;
				returnReleaseOk = false;
			}
			break;
	
		}
	
		/*
		 * Get the resource lock.
		 */
		if (takeLock)
		{
#ifdef RESLOCK_DEBUG
			elog(DEBUG1, "acquire resource lock for queue %u (portal %u)", 
					queueid, portal->portalId);
#endif
			SET_LOCKTAG_RESOURCE_QUEUE(tag, queueid);

			PG_TRY();
			{
				lockResult = ResLockAcquire(&tag, &incData);
			}
			PG_CATCH();
			{
				/* 
				 * We might have been waiting for a resource queue lock when we get 
				 * here. Calling ResLockRelease without calling ResLockWaitCancel will 
				 * cause the locallock to be cleaned up, but will leave the global
				 * variable lockAwaited still pointing to the locallock hash 
				 * entry.
				 */
				ResLockWaitCancel();
		
				/* Change status to no longer waiting for lock */
				pgstat_report_waiting(PGBE_WAITING_NONE);

				/* If we had acquired the resource queue lock, release it and clean up */	
				ResLockRelease(&tag, portal->portalId);

				/*
				 * Perfmon related stuff: clean up if we got cancelled
				 * while waiting.
				 */
				if (gp_enable_gpperfmon && qDesc->gpmon_pkt)
				{			
					gpmon_qlog_query_error(qDesc->gpmon_pkt);
					pfree(qDesc->gpmon_pkt);
					qDesc->gpmon_pkt = NULL;
				}

				portal->queueId = InvalidOid;
				portal->portalId = INVALID_PORTALID;

				PG_RE_THROW();
			}
			PG_END_TRY();

			/* 
			 * See if query was too small to bother locking at all, i.e had
			 * cost smaller than the ignore cost threshold for the queue.
			 */
			if (lockResult == LOCKACQUIRE_NOT_AVAIL)
			{
#ifdef RESLOCK_DEBUG
				elog(DEBUG1, "cancel resource lock for queue %u (portal %u)", 
						queueid, portal->portalId);
#endif
				/* 
				 * Reset portalId and queueid for this portal so the queue
				 * and increment accounting tests continue to work properly.
				 */
				portal->queueId = InvalidOid;
				portal->portalId = INVALID_PORTALID;
				returnReleaseOk = false;
			}

			/* Count holdable cursors (if we are locking this one) .*/
			if (portal->cursorOptions & CURSOR_OPT_HOLD && returnReleaseOk)
				numHoldPortals++;

		}

	}
	return returnReleaseOk;

}

/* This function is a simple version of ResLockPortal, which is used specially
 * for utility statements; the main logic is same as ResLockPortal, but remove
 * some unnecessary lines and make some tiny adjustments for utility stmts */
bool
ResLockUtilityPortal(Portal portal, float4 ignoreCostLimit)
{
	bool returnReleaseOk = false;
	LOCKTAG		tag;
	Oid			queueid;
	int32		lockResult = 0;
	ResPortalIncrement	incData;

	queueid = portal->queueId;

	/*
	 * Check we have a valid queue before going any further.
	 */
	if (queueid != InvalidOid)
	{
		/*
		 * Setup the resource portal increments, ready to be added.
		 */
		incData.pid = MyProc->pid;
		incData.portalId = portal->portalId;
		incData.increments[RES_COUNT_LIMIT] = 1;
		incData.increments[RES_COST_LIMIT] = ignoreCostLimit;
		incData.increments[RES_MEMORY_LIMIT] = (Cost) 0.0;
		returnReleaseOk = true;

		/*
		 * Get the resource lock.
		 */
#ifdef RESLOCK_DEBUG
		elog(DEBUG1, "acquire resource lock for queue %u (portal %u)",
				queueid, portal->portalId);
#endif
		SET_LOCKTAG_RESOURCE_QUEUE(tag, queueid);

		PG_TRY();
		{
			lockResult = ResLockAcquire(&tag, &incData);
		}
		PG_CATCH();
		{
			/*
			 * We might have been waiting for a resource queue lock when we get
			 * here. Calling ResLockRelease without calling ResLockWaitCancel will
			 * cause the locallock to be cleaned up, but will leave the global
			 * variable lockAwaited still pointing to the locallock hash
			 * entry.
			 */
			ResLockWaitCancel();

			/* Change status to no longer waiting for lock */
			pgstat_report_waiting(PGBE_WAITING_NONE);

			/* If we had acquired the resource queue lock, release it and clean up */
			ResLockRelease(&tag, portal->portalId);

			/*
			 * Perfmon related stuff: clean up if we got cancelled
			 * while waiting.
			 */

			portal->queueId = InvalidOid;
			portal->portalId = INVALID_PORTALID;

			PG_RE_THROW();
		}
		PG_END_TRY();
	}
	return returnReleaseOk;
}

/*
 * ResUnLockPortal -- release a resource lock for a portal.
 */
void
ResUnLockPortal(Portal portal)
{
	LOCKTAG					tag;
	Oid						queueid;

	queueid = portal->queueId;

	/* 
	 * Check we have a valid queue before going any further.
	 */
	if (queueid != InvalidOid)
	{
#ifdef RESLOCK_DEBUG
		elog(DEBUG1, "release resource lock for queue %u (portal %u)", 
			 queueid, portal->portalId);
#endif
		SET_LOCKTAG_RESOURCE_QUEUE(tag, queueid);

		ResLockRelease(&tag, portal->portalId);

		/* Count holdable cursors.*/
		if (portal->cursorOptions & CURSOR_OPT_HOLD)
		{
			Assert(numHoldPortals > 0);
			numHoldPortals--;
		}
	}

	return;
}


/*
 * GetResQueueForRole -- determine what resource queue a role is going to use.
 *
 * Notes
 *	This could be called for each of ResLockPortal and ResUnLockPortal, but we 
 *	can eliminate a relation open and lock if it is cached.
 */
Oid	
GetResQueueForRole(Oid roleid)
{
	HeapTuple	tuple;
	bool		isnull;
	Oid			queueid;

	tuple = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
	if (!tuple)
		return DEFAULTRESQUEUE_OID; /* role not found */

	queueid = SysCacheGetAttr(AUTHOID, tuple, Anum_pg_authid_rolresqueue, &isnull);

	/* MPP-6926: use default queue if none specified */
	if (!OidIsValid(queueid) || isnull)
		queueid = DEFAULTRESQUEUE_OID;

	ReleaseSysCache(tuple);

	return queueid;
	
}


/*
 * SetResQueueId -- set the cached value for the current resource queue.
 *
 * Notes
 *	Needs to be called at session initialization and after (or in) SET ROLE.
 */
void
SetResQueueId(void)
{
	/* to cave the code of cache part, we provide a resource owner here if no
	 * existing */
	ResourceOwner owner = NULL;

	if (CurrentResourceOwner == NULL)
	{
		owner = ResourceOwnerCreate(NULL, "SetResQueueId");
		CurrentResourceOwner = owner;
	}

	MyQueueId = GetResQueueForRole(GetUserId());

	if (owner)
	{
		CurrentResourceOwner = NULL;
		ResourceOwnerDelete(owner);
	}

	return;
}


/*
 * GetResQueueId -- return the current cached value for the resource queue.
 */
Oid
GetResQueueId(void)
{
	return MyQueueId;
}


/*
 * ResQueueIdForName -- Return the Oid for a resource queue name
 *
 * Notes:
 *	Used by the various admin commands to convert a user supplied queue name
 *	to Oid.
 */
Oid
GetResQueueIdForName(char	*name)
{
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	HeapTuple	tuple;
	Oid			queueid;

	rel = heap_open(ResQueueRelationId, AccessShareLock);

	/* SELECT oid FROM pg_resqueue WHERE rsqname = :1 */
	ScanKeyInit(&scankey,
				Anum_pg_resqueue_rsqname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(name));
	scan = systable_beginscan(rel, ResQueueRsqnameIndexId, true,
							  SnapshotNow, 1, &scankey);

	tuple = systable_getnext(scan);
	if (tuple)
		queueid = HeapTupleGetOid(tuple);
	else
		queueid = InvalidOid;

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);

	return queueid;
}


/*
 * ResCreatePortalId -- return an id for a portal
 *
 * Notes
 * 	We return a new id for any named portal, but zero for an un-named one. 
 *	This is because we use the id to track cursors, and there can only be one
 *	un-named portal active.
 */
uint32
ResCreatePortalId(const char *name)
{

	uint32				id;
	ResPortalTag		portalTag;
	bool				found = false;

	/* Unnamed portal. */
	if (name[0] == '\0')
	{
		id = 0;
		return id;
	}


	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	/* 
	 * Named portal.
	 * Search for a id we can re-use from a closed portal - we want to do this
	 * so we can have MaxResourcePortalsPerXact *open* tracked (named) portals 
	 * in a transaction.
	 */
	for (id = 1; id <= portalId; id++)
	{
		MemSet(&portalTag, 0, sizeof(ResPortalTag));
		portalTag.pid = MyProc->pid;
		portalTag.portalId = id;
		if (!ResIncrementFind(&portalTag))
		{
			found = true;
			break;
		}
	}
	
	LWLockRelease(ResQueueLock);
	
	/*
	 * No re-usable portal ids, check we have not exhaused the # of trackable
	 * portals and return the next id.
	 */
	if (!found)
	{
		if (portalId >= MaxResourcePortalsPerXact)
		{
			ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("insufficient portal ids available"),
			errhint("Increase max_resource_portals_per_transaction.")));

		}

		id = (++portalId);
	}

	return id;
}


/*
 * AtCommit_ResScheduler -- do any pre-commit processing for Resource 
 *	Scheduling.
 */
void
AtCommit_ResScheduler(void)
{

	/* reset the portal id. */
	if (numHoldPortals == 0)
		portalId = 0;
}


/*
 * AtAbort_ResScheduler -- do any abort processing for Resource 
 *	Scheduling.
 */
void
AtAbort_ResScheduler(void)
{

	/* reset the portal id. */
	if (numHoldPortals == 0)
		portalId = 0;
}

/* This routine checks whether speficied utility stmt should be involved into
 * resourece queue mgmt; if yes, take the slot from the resource queue; if we
 * want to track additional utility stmts, add it into the condition check */
void
ResHandleUtilityStmt(Portal portal, Node *stmt)
{
	if (!IsA(stmt, CopyStmt))
	{
		return;
	}

	if (Gp_role == GP_ROLE_DISPATCH
		&& IsResQueueEnabled()
		&& (!ResourceSelectOnly)
		&& !superuser())
	{
		Assert(!LWLockHeldExclusiveByMe(ResQueueLock));
		LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);
		ResQueue resQueue = ResQueueHashFind(portal->queueId);
		LWLockRelease(ResQueueLock);

		Assert(resQueue);
		int numSlots = (int) ceil(resQueue->limits[RES_COUNT_LIMIT].threshold_value);

		if (numSlots >= 1) /* statement limit exists */
		{
			portal->status = PORTAL_QUEUE;

			portal->releaseResLock = ResLockUtilityPortal(portal, resQueue->ignorecostlimit);
		}
		portal->status = PORTAL_ACTIVE;
	}
}

