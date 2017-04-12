/*-------------------------------------------------------------------------
 *
 * resgroup.c
 *	  GPDB resource group management code.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "commands/resgroupcmds.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lmgr.h"
#include "storage/lock.h"
#include "storage/pg_shmem.h"
#include "storage/proc.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/resgroup.h"
#include "utils/resowner.h"

/* GUC */
int		MaxResourceGroups;

/* global variables */
Oid			CurrentResGroupId = InvalidOid;

/* static variables */
static ResGroupControl *pResGroupControl;

static bool localResWaiting = false;

/* static functions */

static ResGroup ResGroupHashNew(Oid groupId);
static ResGroup ResGroupHashFind(Oid groupId);
static bool ResGroupHashRemove(Oid groupId);
static void ResGroupWait(ResGroup group);
static bool ResGroupCreate(Oid groupId);
static void AtProcExit_ResGroup(int code, Datum arg);
static void ResGroupWaitCancel(void);

Datum
pg_resgroup_get_status_kv(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		int			nattr = 3;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "rsgid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "prop", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "value", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		if (PG_ARGISNULL(0))
		{
			funcctx->max_calls = 0;
		}
		else
		{
			/* dummy output */
			funcctx->max_calls = 2;
			funcctx->user_fctx = palloc0(sizeof(Datum) * funcctx->max_calls);
			((Datum *) funcctx->user_fctx)[0] = ObjectIdGetDatum(6437);
			((Datum *) funcctx->user_fctx)[1] = ObjectIdGetDatum(6438);
		}

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		/* for each row */
		char *		prop = text_to_cstring(PG_GETARG_TEXT_P(0));
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		values[0] = ((Datum *) funcctx->user_fctx)[funcctx->call_cntr];
		values[1] = CStringGetTextDatum(prop);

		/* Fill with dummy values */
		if (!strcmp(prop, "num_running"))
			values[2] = CStringGetTextDatum("1");
		else if (!strcmp(prop, "num_queueing"))
			values[2] = CStringGetTextDatum("0");
		else if (!strcmp(prop, "cpu_usage"))
			values[2] = CStringGetTextDatum("0.0");
		else if (!strcmp(prop, "memory_usage"))
			values[2] = CStringGetTextDatum("0.0");
		else if (!strcmp(prop, "total_queue_duration"))
			values[2] = CStringGetTextDatum("00:00:00");
		else if (!strcmp(prop, "num_queued"))
			values[2] = CStringGetTextDatum("0");
		else if (!strcmp(prop, "num_executed"))
			values[2] = CStringGetTextDatum("0");
		else
			/* unknown property name */
			nulls[2] = true;

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* nothing left */
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * Estimate size the resource group structures will need in
 * shared memory.
 */
Size
ResGroupShmemSize(void)
{
	Size		size = 0;

	/* The hash of groups. */
	size = hash_estimate_size(MaxResourceGroups, sizeof(ResGroupData));

	/* The control structure. */
	size = add_size(size, sizeof(ResGroupControl));

	/* Add a safety margin */
	size = add_size(size, size / 10);

	return size;
}

/*
 * Initialize the global ResGroupControl struct of resource groups.
 */
void
ResGroupControlInit(void)
{
    bool        found;
    HASHCTL     info;
    int         hash_flags;

    pResGroupControl = ShmemInitStruct("global resource group control",
                                       sizeof(*pResGroupControl), &found);
    if (found)
        return;
    if (pResGroupControl == NULL)
        goto error_out;

    /* Set key and entry sizes of hash table */
    MemSet(&info, 0, sizeof(info));
    info.keysize = sizeof(Oid);
    info.entrysize = sizeof(ResGroupData);
    info.hash = tag_hash;

    hash_flags = (HASH_ELEM | HASH_FUNCTION);

    LOG_RESGROUP_DEBUG(LOG, "Creating hash table for %d resource groups", MaxResourceGroups);

    pResGroupControl->htbl = ShmemInitHash("Resource Group Hash Table",
                                           MaxResourceGroups,
                                           MaxResourceGroups,
                                           &info, hash_flags);

    if (!pResGroupControl->htbl)
        goto error_out;

    /*
     * No need to acquire LWLock here, since this is expected to be called by
     * postmaster only
     */
    pResGroupControl->loaded = false;
    return;

error_out:
	ereport(FATAL,
			(errcode(ERRCODE_OUT_OF_MEMORY),
			 errmsg("not enough shared memory for resource group control")));
}

/*
 * Allocate a resource group entry from a hash table
 */
void
AllocResGroupEntry(Oid groupId)
{
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	bool groupOK = ResGroupCreate(groupId);
	if (!groupOK)
	{
		LWLockRelease(ResGroupLock);

		ereport(PANIC,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				errmsg("not enough shared memory for resource groups")));
	}

	LWLockRelease(ResGroupLock);
}

/*
 * Remove a resource group entry from the hash table
 */
void
FreeResGroupEntry(Oid groupId)
{
	ResGroup group;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	group = ResGroupHashFind(groupId);
	if (group == NULL)
	{
		LWLockRelease(ResGroupLock);
		
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
			 	 errmsg("Cannot find resource group with Oid %d in shared memory", groupId)));
	}

	if (group->nRunning > 0)
	{
		LWLockRelease(ResGroupLock);

		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("Cannot drop resource group with Oid %d", groupId),
				 errhint("There are running transactions in this group."
						 "Please terminate the queries first, or try dropping the group later.")));
	}

#ifdef USE_ASSERT_CHECKING
	bool groupOK = 
#endif
		ResGroupHashRemove(groupId);
	Assert(groupOK);

	LWLockRelease(ResGroupLock);
}

/*
 * Load the resource groups in shared memory. Note this
 * can only be done after enough setup has been done. This uses
 * heap_open etc which in turn requires shared memory to be set up.
 */
void
InitResGroups(void)
{
	HeapTuple	tuple;
	SysScanDesc	sscan;
	int			numGroups;

	on_shmem_exit(AtProcExit_ResGroup, 0);
	if (pResGroupControl->loaded)
		return;
	/*
	 * Need a resource owner to keep the heapam code happy.
	 */
	Assert(CurrentResourceOwner == NULL);
	ResourceOwner owner = ResourceOwnerCreate(NULL, "InitResGroups");
	CurrentResourceOwner = owner;

	/*
	 * The resgroup shared mem initialization must be serialized. Only the first session
	 * should do the init.
	 * Serialization is done by LW_EXCLUSIVE ResGroupLock. However, we must obtain all DB
	 * locks before obtaining LWlock to prevent deadlock.
	 */
	Relation relResGroup = heap_open(ResGroupRelationId, AccessShareLock);
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	if (pResGroupControl->loaded)
		goto exit;

	numGroups = 0;
	sscan = systable_beginscan(relResGroup, InvalidOid, false, SnapshotNow, 0, NULL);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		bool groupOK = ResGroupCreate(HeapTupleGetOid(tuple));

		if (!groupOK)
			ereport(PANIC,
					(errcode(ERRCODE_OUT_OF_MEMORY),
			 		errmsg("not enough shared memory for resource groups")));

		numGroups ++;
		Assert(numGroups <= MaxResourceGroups);
	}
	systable_endscan(sscan);

	pResGroupControl->loaded = true;
	LOG_RESGROUP_DEBUG(LOG, "initialized %d resource groups", numGroups);

exit:
	LWLockRelease(ResGroupLock);
	heap_close(relResGroup, AccessShareLock);
	CurrentResourceOwner = NULL;
	ResourceOwnerDelete(owner);
}

/*
 * ResGroupCreate -- initialize the elements for a resource group.
 *
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this - unless we are the startup process.
 */
static bool
ResGroupCreate(Oid groupId)
{
	ResGroup		group;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(OidIsValid(groupId));

	group = ResGroupHashNew(groupId);
	if (group == NULL)
		return false;

	group->groupId = groupId;
	group->nRunning = 0;
	ProcQueueInit(&group->waitProcs);

	return true;
}

/*
 * Acquire a resource group slot 
 *
 * Call this function at the start of the transaction.
 */
void
ResGroupSlotAcquire(void)
{
	ResGroup	group;
	Oid			groupId;
	int			concurrencyLimit;

	groupId = GetResGroupIdForRole(GetUserId());
	if (groupId == InvalidOid)
		groupId = superuser() ? ADMINRESGROUP_OID : DEFAULTRESGROUP_OID;

	CurrentResGroupId = groupId;

	if (groupId == ADMINRESGROUP_OID)
		return;

	concurrencyLimit = GetConcurrencyForGroup(groupId);

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	group = ResGroupHashFind(groupId);

	if (group == NULL)
	{
		LWLockRelease(ResGroupLock);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
			 	 errmsg("Cannot find resource group %d in shared memory", groupId)));
	}

	if (group->nRunning < concurrencyLimit) 
	{
		group->nRunning ++;
		LWLockRelease(ResGroupLock);
		return;
	}

	ResGroupWait(group);

	/*
	 * The waking process has granted us the slot.
	 */
}

/*
 * Release the resource group slot
 *
 * Call this function at the end of the transaction.
 */
void
ResGroupSlotRelease(void)
{
	ResGroup	group;
	PROC_QUEUE	*waitQueue;
	PGPROC		*waitProc;

	Assert(CurrentResGroupId != InvalidOid);
	if (CurrentResGroupId == ADMINRESGROUP_OID)
		return;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	group = ResGroupHashFind(CurrentResGroupId);
	if (group == NULL)
	{
		LWLockRelease(ResGroupLock);
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
			 	 errmsg("Cannot find resource group %d in shared memory", CurrentResGroupId)));
	}

	waitQueue = &(group->waitProcs);
	
	if (waitQueue->size == 0)
	{
		Assert(waitQueue->links.next == MAKE_OFFSET(&waitQueue->links) &&
			   waitQueue->links.prev == MAKE_OFFSET(&waitQueue->links));
		Assert(group->nRunning > 0);

		group->nRunning--;
		LWLockRelease(ResGroupLock);
		return;
	}

	/* wake up one process in the wait queue */
	waitProc = (PGPROC *) MAKE_PTR(waitQueue->links.next);
	SHMQueueDelete(&(waitProc->links));
	waitQueue->size --;

	LWLockRelease(ResGroupLock);

	waitProc->resWaiting = false;
	SetLatch(&waitProc->procLatch);

	CurrentResGroupId = InvalidOid;
}

static void
ResGroupWait(ResGroup group)
{
	PGPROC *proc = MyProc, *headProc;
	PROC_QUEUE *waitQueue;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	proc->resWaiting = true;

	waitQueue = &(group->waitProcs);

	headProc = (PGPROC *) &(waitQueue->links);
	SHMQueueInsertBefore(&(headProc->links), &(proc->links));
	waitQueue->size++;

	LWLockRelease(ResGroupLock);

	/* similar to lockAwaited in ProcSleep for interrupt cleanup */
	localResWaiting = true;

	/*
	 * Make sure we have released all locks before going to sleep, to eliminate
	 * deadlock situations
	 */
	PG_TRY();
	{
		for (;;)
		{
			ResetLatch(&proc->procLatch);

			CHECK_FOR_INTERRUPTS();

			if (!proc->resWaiting)
				break;
			WaitLatch(&proc->procLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH, -1);
		}
	}
	PG_CATCH();
	{
		ResGroupWaitCancel();
		PG_RE_THROW();
	}
	PG_END_TRY();

	localResWaiting = false;
}

/*
 * ResGroupHashNew -- return a new (empty) group object to initialize.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static ResGroup
ResGroupHashNew(Oid groupId)
{
	bool		found;
	ResGroup	group = NULL;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	group = (ResGroup)
		hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_ENTER_NULL, &found);

	/* caller should test that the group does not exist already */
	Assert(!found);

	return group;
}

/*
 * ResGroupHashFind -- return the group for a given oid.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static ResGroup
ResGroupHashFind(Oid groupId)
{
	bool		found;
	ResGroup	group = NULL;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	group = (ResGroup)
		hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_FIND, &found);

	return group;
}


/*
 * ResGroupHashRemove -- remove the group for a given oid.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static bool
ResGroupHashRemove(Oid groupId)
{
	bool		found;
	void	   *group;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	group = hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_REMOVE, &found);
	if (!group || !found)
		return false;

	return true;
}

static void
AtProcExit_ResGroup(int code, Datum arg)
{
	ResGroupWaitCancel();
}

static void
ResGroupWaitCancel(void)
{
	ResGroup group;
	PROC_QUEUE	*waitQueue;
	PGPROC		*waitProc;
	bool		granted = false;

	if (CurrentResGroupId == InvalidOid)
		return;

	if (!localResWaiting)
		return;

	/* we are sure to be interrupted in the for loop of ResGroupWait now */
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	group = ResGroupHashFind(CurrentResGroupId);

	/*
	 * We either have been granted the lock, or is still on the waiting list, so
	 * this group should not have been dropped
	 */
	Assert(group != NULL);

	/*
	 * Check if we are on the waiting list to decide whether we have been granted
	 * the lock, not resWaiting.
	 */
	if (MyProc->links.next == INVALID_OFFSET)
	{
		Assert(MyProc->links.prev == INVALID_OFFSET);
		granted = true;
	}

	waitQueue = &(group->waitProcs);
	if (granted)
	{
		if (waitQueue->size == 0)
		{
			Assert(waitQueue->links.next == MAKE_OFFSET(&waitQueue->links) &&
				   waitQueue->links.prev == MAKE_OFFSET(&waitQueue->links));
			Assert(group->nRunning > 0);

			group->nRunning--;

			LWLockRelease(ResGroupLock);
			localResWaiting = false;
			return;
		}

		/* wake up one process in the wait queue */
		waitProc = (PGPROC *) MAKE_PTR(waitQueue->links.next);
		SHMQueueDelete(&(waitProc->links));
		waitQueue->size --;

		LWLockRelease(ResGroupLock);

		waitProc->resWaiting = false;
		SetLatch(&waitProc->procLatch);

		localResWaiting = false;
		return;
	}

	Assert(waitQueue->size > 0);
	Assert(MyProc->resWaiting);

	SHMQueueDelete(&(MyProc->links));
	waitQueue->size --;

	LWLockRelease(ResGroupLock);
	localResWaiting = false;
}
