/*-------------------------------------------------------------------------
 *
 * resgroup.c
 *	  GPDB resource group management code.
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resgroup/resgroup.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "commands/resgroupcmds.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lmgr.h"
#include "storage/lock.h"
#include "storage/pg_shmem.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/resgroup-ops.h"
#include "utils/resgroup.h"
#include "utils/resource_manager.h"
#include "utils/resowner.h"
#include "utils/session_state.h"
#include "utils/vmem_tracker.h"

#define InvalidSlotId	(-1)
#define RESGROUP_MAX_SLOTS	300

/*
 * GUC variables.
 */
char                		*gp_resgroup_memory_policy_str = NULL;
ResManagerMemoryPolicy     	gp_resgroup_memory_policy = RESMANAGER_MEMORY_POLICY_NONE;
bool						gp_log_resgroup_memory = false;
int							gp_resgroup_memory_policy_auto_fixed_mem;
bool						gp_resgroup_print_operator_memory_limits = false;
int							memory_spill_ratio=20;

/*
 * Data structures
 */

typedef struct ResGroupHashEntry		ResGroupHashEntry;
typedef struct ResGroupProcData			ResGroupProcData;
typedef struct ResGroupSlotData			ResGroupSlotData;
typedef struct ResGroupData				ResGroupData;
typedef struct ResGroupControl			ResGroupControl;

struct ResGroupHashEntry
{
	Oid		groupId;
	int		index;
};

/*
 * Per proc resource group information.
 *
 * Config snapshot and runtime accounting information in current proc.
 */
struct ResGroupProcData
{
	Oid		groupId;
	int		slotId;

	ResGroupData		*group;
	ResGroupSlotData	*slot;

	ResGroupCaps	caps;

	uint32	memUsage;			/* memory usage of current proc */
	bool	doMemCheck;			/* whether to do memory limit check */
};

/*
 * Per slot resource group information.
 *
 * Resource group have 'concurrency' number of slots.
 * Each transaction acquires a slot on master before running.
 * The information shared by QE processes on each segments are stored
 * in this structure.
 */
struct ResGroupSlotData
{
	int				sessionId;

	ResGroupCaps	caps;

	int32			memQuota;	/* memory quota of current slot */
	int32			memUsage;	/* total memory usage of procs belongs to this slot */
	int				nProcs;		/* number of procs in this slot */
	bool			inUse;
};

/*
 * Resource group information.
 */
struct ResGroupData
{
	Oid			groupId;		/* Id for this group */
	ResGroupCaps	caps;
	int			nRunning;		/* number of running trans */
	PROC_QUEUE	waitProcs;
	int			totalExecuted;	/* total number of executed trans */
	int			totalQueued;	/* total number of queued trans	*/
	Interval	totalQueuedTime;/* total queue time */

	/*
	 * All the attributes in the group and its slots should be protected
	 * by this spinlock.
	 */
	slock_t		lock;
#ifdef USE_ASSERT_CHECKING
	int			lockOwnerPid;		/* pid of the lock owner, only for debug */
#endif//USE_ASSERT_CHECKING

	bool		lockedForDrop;  /* true if resource group is dropped but not committed yet */

	int32		memExpected;		/* expected memory chunks according to current caps */
	int32		memQuotaGranted;	/* memory chunks for quota part */
	int32		memSharedGranted;	/* memory chunks for shared part */

	int32		memQuotaUsed;		/* memory chunks assigned to all the running slots */

	/*
	 * memory usage of this group, should always equal to the
	 * sum of session memory(session_state->sessionVmem) that
	 * belongs to this group
	 */
	int32		memUsage;
	int32		memSharedUsage;

	ResGroupSlotData slots[RESGROUP_MAX_SLOTS];
};

struct ResGroupControl
{
	HTAB			*htbl;
	int 			segmentsOnMaster;

	/*
	 * The hash table for resource groups in shared memory should only be populated
	 * once, so we add a flag here to implement this requirement.
	 */
	bool			loaded;

	int32			totalChunks;	/* total memory chunks on this segment */
	int32			freeChunks;		/* memory chunks not allocated to any group */

	int				nGroups;
	ResGroupData	groups[1];
};

/* GUC */
int		MaxResourceGroups;

/* static variables */

static ResGroupControl *pResGroupControl = NULL;

static ResGroupProcData __self =
{
	InvalidOid, InvalidSlotId,
};
static ResGroupProcData *self = &__self;

static bool localResWaiting = false;

/* static functions */

static bool groupApplyMemCaps(ResGroupData *group, const ResGroupCaps *caps);
static int32 getChunksFromPool(ResGroupData *group, int32 chunks);
static void returnChunksToPool(ResGroupData *group, int32 chunks);
static void groupAssginChunks(ResGroupData *group,
							  int32 chunks,
							  const ResGroupCaps *caps);
static int32 getSegmentChunks(void);
static int32 groupGetMemExpected(const ResGroupCaps *caps);
static int32 groupGetMemQuotaExpected(const ResGroupCaps *caps);
static int32 groupGetMemSharedExpected(const ResGroupCaps *caps);
static int32 groupGetMemSpillTotal(const ResGroupCaps *caps);
static int32 slotGetMemQuotaExpected(const ResGroupCaps *caps);
static int32 slotGetMemSpill(const ResGroupCaps *caps);
static void wakeupSlots(ResGroupData *group);
static void wakeupGroups(ResGroupData *skipGroup);
static bool groupReleaseMemQuota(ResGroupData *group,
								ResGroupSlotData *slot);
static void groupAcquireMemQuota(ResGroupData *group, const ResGroupCaps *caps);
static ResGroupData *ResGroupHashNew(Oid groupId);
static ResGroupData *ResGroupHashFind(Oid groupId);
static ResGroupData *ResGroupHashRemove(Oid groupId);
static void ResGroupWait(ResGroupData *group);
static ResGroupData *ResGroupCreate(Oid groupId, const ResGroupCaps *caps);
static void AtProcExit_ResGroup(int code, Datum arg);
static void ResGroupWaitCancel(void);
static void groupAssginChunks(ResGroupData *group,
							  int32 chunks,
							  const ResGroupCaps *caps);
static int32 groupTryIncMemUsage(ResGroupData *group,
								 ResGroupSlotData *slot,
								 int32 chunks,
								 int32 threshold);
static void groupIncMemUsage(ResGroupData *group,
							 ResGroupSlotData *slot,
							 int32 chunks);
static void groupDecMemUsage(ResGroupData *group,
							 ResGroupSlotData *slot,
							 int32 chunks);
static int getFreeSlot(ResGroupData *group);
static int getSlot(ResGroupData *group);
static bool putSlot(void);
static void ResGroupSlotAcquire(void);
static void addTotalQueueDuration(ResGroupData *group);
static bool ResGroupSlotRelease(void);
static void ResGroupSetMemorySpillRatio(const ResGroupCaps *caps);
static char* DumpResGroupMemUsage(ResGroupData *group);
static void selfValidateResGroupInfo(void);
static bool selfIsAssignedDroppedGroup(void);
static bool selfIsAssignedValidGroup(void);
#ifdef USE_ASSERT_CHECKING
static bool selfIsAssigned(void);
#endif//USE_ASSERT_CHECKING
static bool selfIsUnassigned(void);
static void selfUnassignDroppedGroup(void);
static bool selfHasSlot(void);
static bool selfHasGroup(void);
static void selfSetGroup(ResGroupData *group);
static void selfUnsetGroup(void);
static void selfSetSlot(void);
static void selfUnsetSlot(void);
static bool procIsInWaitQueue(const PGPROC *proc);
#ifdef USE_ASSERT_CHECKING
static bool groupIsNotDropped(const ResGroupData *group);
#endif//USE_ASSERT_CHECKING
static void groupWaitQueueValidate(const ResGroupData *group);
static void groupWaitQueuePush(ResGroupData *group, PGPROC *proc);
static PGPROC * groupWaitQueuePop(ResGroupData *group);
static void groupWaitQueueErase(ResGroupData *group, PGPROC *proc);
static bool groupWaitQueueIsEmpty(const ResGroupData *group);
static void groupSpinLockInit(ResGroupData *group);
static void groupSpinLockAcquire(ResGroupData *group);
static void groupSpinLockRelease(ResGroupData *group);
#ifdef USE_ASSERT_CHECKING
static bool groupSpinLockHeldByMe(const ResGroupData *group);
#endif//USE_ASSERT_CHECKING

/*
 * Estimate size the resource group structures will need in
 * shared memory.
 */
Size
ResGroupShmemSize(void)
{
	Size		size = 0;

	/* The hash of groups. */
	size = hash_estimate_size(MaxResourceGroups, sizeof(ResGroupHashEntry));

	/* The control structure. */
	size = add_size(size, sizeof(ResGroupControl) - sizeof(ResGroupData));

	/* The control structure. */
	size = add_size(size, mul_size(MaxResourceGroups, sizeof(ResGroupData)));

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
	int			i;
    bool        found;
    HASHCTL     info;
    int         hash_flags;
	int			size;

	size = sizeof(*pResGroupControl) - sizeof(ResGroupData);
	size += mul_size(MaxResourceGroups, sizeof(ResGroupData));

    pResGroupControl = ShmemInitStruct("global resource group control",
                                       size, &found);
    if (found)
        return;
    if (pResGroupControl == NULL)
        goto error_out;

    /* Set key and entry sizes of hash table */
    MemSet(&info, 0, sizeof(info));
    info.keysize = sizeof(Oid);
    info.entrysize = sizeof(ResGroupHashEntry);
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
    pResGroupControl->nGroups = MaxResourceGroups;
	pResGroupControl->totalChunks = 0;
	pResGroupControl->freeChunks = 0;

	for (i = 0; i < MaxResourceGroups; i++)
		pResGroupControl->groups[i].groupId = InvalidOid;

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
AllocResGroupEntry(Oid groupId, const ResGroupOpts *opts)
{
	ResGroupData	*group;
	ResGroupCaps	caps;

	ResGroupOptsToCaps(opts, &caps);

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	group = ResGroupCreate(groupId, &caps);
	LWLockRelease(ResGroupLock);

	if (!group)
	{
		ereport(PANIC,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				errmsg("not enough shared memory for resource groups")));
	}
}

/*
 * Remove a resource group entry from the hash table
 */
void
FreeResGroupEntry(Oid groupId)
{
	ResGroupData	*group;
	ResGroupData	group0;

	group = ResGroupHashRemove(groupId);

	groupSpinLockRelease(group);

	/*
	 * Similar as what we do in ResGroupHashRemove(),
	 * construct a fake group for wakeupGroups().
	 */
	group0 = *group;
	group0.groupId = groupId;
	wakeupGroups(&group0);
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
	CdbComponentDatabases *cdbComponentDBs;
	CdbComponentDatabaseInfo *qdinfo;
	ResGroupCaps		caps;
	Relation			relResGroup;
	Relation			relResGroupCapability;

	/*
	 * On master, the postmaster does the initialization
	 * On segments, the first QE does the initialization
	 */
	if (Gp_role == GP_ROLE_DISPATCH && GpIdentity.segindex != MASTER_CONTENT_ID)
		return;

	on_shmem_exit(AtProcExit_ResGroup, 0);
	if (pResGroupControl->loaded)
		return;
	/*
	 * Need a resource owner to keep the heapam code happy.
	 */
	Assert(CurrentResourceOwner == NULL);
	ResourceOwner owner = ResourceOwnerCreate(NULL, "InitResGroups");
	CurrentResourceOwner = owner;

	if (Gp_role == GP_ROLE_DISPATCH && pResGroupControl->segmentsOnMaster == 0)
	{
		Assert(GpIdentity.segindex == MASTER_CONTENT_ID);
		cdbComponentDBs = getCdbComponentDatabases();
		qdinfo = &cdbComponentDBs->entry_db_info[0];
		pResGroupControl->segmentsOnMaster = qdinfo->hostSegs;
		Assert(pResGroupControl->segmentsOnMaster > 0);
	}

	/*
	 * The resgroup shared mem initialization must be serialized. Only the first session
	 * should do the init.
	 * Serialization is done by LW_EXCLUSIVE ResGroupLock. However, we must obtain all DB
	 * locks before obtaining LWlock to prevent deadlock.
	 */
	relResGroup = heap_open(ResGroupRelationId, AccessShareLock);
	relResGroupCapability = heap_open(ResGroupCapabilityRelationId, AccessShareLock);
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	if (pResGroupControl->loaded)
		goto exit;

	/* These initialization must be done before ResGroupCreate() */
	pResGroupControl->totalChunks = getSegmentChunks();
	pResGroupControl->freeChunks = pResGroupControl->totalChunks;
	if (pResGroupControl->totalChunks == 0)
		ereport(PANIC,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("insufficient memory available"),
				 errhint("Increase gp_resource_group_memory_limit")));

	ResGroupOps_Init();

	numGroups = 0;
	sscan = systable_beginscan(relResGroup, InvalidOid, false, SnapshotNow, 0, NULL);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		ResGroupData	*group;
		int cpuRateLimit;
		Oid groupId = HeapTupleGetOid(tuple);

		GetResGroupCapabilities(groupId, &caps);
		cpuRateLimit = caps.cpuRateLimit.value;

		group = ResGroupCreate(groupId, &caps);
		if (!group)
			ereport(PANIC,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("not enough shared memory for resource groups")));

		ResGroupOps_CreateGroup(groupId);
		ResGroupOps_SetCpuRateLimit(groupId, cpuRateLimit);

		numGroups++;
		Assert(numGroups <= MaxResourceGroups);
	}
	systable_endscan(sscan);

	pResGroupControl->loaded = true;
	LOG_RESGROUP_DEBUG(LOG, "initialized %d resource groups", numGroups);

exit:
	LWLockRelease(ResGroupLock);
	heap_close(relResGroup, AccessShareLock);
	heap_close(relResGroupCapability, AccessShareLock);
	CurrentResourceOwner = NULL;
	ResourceOwnerDelete(owner);
}

/*
 * Check resource group status when DROP RESOURCE GROUP
 *
 * Errors out if there're running transactions, otherwise lock the resource group.
 * New transactions will be queued if the resource group is locked.
 */
void
ResGroupCheckForDrop(Oid groupId, char *name)
{
	ResGroupData	*group;

	group = ResGroupHashFind(groupId);

	if (group->nRunning > 0)
	{
		int nQuery = group->nRunning + group->waitProcs.size;
		groupSpinLockRelease(group);

		Assert(name != NULL);
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("Cannot drop resource group \"%s\"", name),
				 errhint(" The resource group is currently managing %d query(ies) and cannot be dropped.\n"
						 "\tTerminate the queries first or try dropping the group later.\n"
						 "\tThe view pg_stat_activity tracks the queries managed by resource groups.", nQuery)));
	}
	group->lockedForDrop = true;

	groupSpinLockRelease(group);
}

/*
 * Wake up the backends in the wait queue when DROP RESOURCE GROUP finishes.
 * Unlock the resource group if the transaction is aborted.
 * Remove the resource group entry in shared memory if the transaction is committed.
 *
 * This function is called in the callback function of DROP RESOURCE GROUP.
 */
void
ResGroupDropCheckForWakeup(Oid groupId, bool isCommit)
{
	ResGroupData	*group;
	ResGroupData	group0;

	group = isCommit ? ResGroupHashRemove(groupId) : ResGroupHashFind(groupId);

	Assert(group->lockedForDrop);

	while (!groupWaitQueueIsEmpty(group))
	{
		PGPROC *waitProc;

		/* wake up one process in the wait queue */
		waitProc = groupWaitQueuePop(group);
		Assert(waitProc->resWaiting != false);
		Assert(waitProc->resSlotId == InvalidSlotId);

		waitProc->resWaiting = false;
		waitProc->resSlotId = InvalidSlotId;
		SetLatch(&waitProc->procLatch);
	}

	group->lockedForDrop = false;

	groupSpinLockRelease(group);

	/*
	 * Similar as what we do in ResGroupHashRemove(),
	 * construct a fake group for wakeupGroups().
	 */
	group0 = *group;
	group0.groupId = groupId;
	wakeupGroups(&group0);
}

/*
 * Apply the new resgroup caps.
 */
void
ResGroupAlterOnCommit(Oid groupId,
					  ResGroupLimitType limittype,
					  const ResGroupCaps *caps)
{
	ResGroupData	*group;
	bool			shouldWakeUp = false;

	group = ResGroupHashFind(groupId);

	group->caps = *caps;

	if (limittype == RESGROUP_LIMIT_TYPE_CPU)
	{
		volatile int savedInterruptHoldoffCount;

		PG_TRY();
		{
			savedInterruptHoldoffCount = InterruptHoldoffCount;
			ResGroupOps_SetCpuRateLimit(groupId, caps->cpuRateLimit.proposed);
		}
		PG_CATCH();
		{
			InterruptHoldoffCount = savedInterruptHoldoffCount;
			elog(LOG, "Fail to set cpu_rate_limit for resource group %d", groupId);
		}
		PG_END_TRY();
	}
	else
	{
		shouldWakeUp = groupApplyMemCaps(group, caps);

		wakeupSlots(group);
	}

	groupSpinLockRelease(group);

	if (shouldWakeUp)
		wakeupGroups(group);
}

/*
 *  Retrieve statistic information of type from resource group
 */
Datum
ResGroupGetStat(Oid groupId, ResGroupStatType type)
{
	ResGroupData	*group;
	Datum result;

	Assert(IsResGroupActivated());

	group = ResGroupHashFind(groupId);

	switch (type)
	{
		case RES_GROUP_STAT_NRUNNING:
			result = Int32GetDatum(group->nRunning);
			break;
		case RES_GROUP_STAT_NQUEUEING:
			result = Int32GetDatum(group->waitProcs.size);
			break;
		case RES_GROUP_STAT_TOTAL_EXECUTED:
			result = Int32GetDatum(group->totalExecuted);
			break;
		case RES_GROUP_STAT_TOTAL_QUEUED:
			result = Int32GetDatum(group->totalQueued);
			break;
		case RES_GROUP_STAT_TOTAL_QUEUE_TIME:
			result = IntervalPGetDatum(&group->totalQueuedTime);
			break;
		case RES_GROUP_STAT_MEM_USAGE:
			result = CStringGetDatum(DumpResGroupMemUsage(group));
			break;
		default:
			groupSpinLockRelease(group);
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Invalid stat type %d", type)));
	}

	groupSpinLockRelease(group);

	return result;
}

static char*
DumpResGroupMemUsage(ResGroupData *group)
{
	int32 slotUsage;
	StringInfoData memUsage;

	Assert(groupSpinLockHeldByMe(group));

	if (Gp_role == GP_ROLE_DISPATCH)
		slotUsage = group->memQuotaUsed;
	else
		/* slotUsage has no meaning in QE */
		slotUsage = -1;	

	initStringInfo(&memUsage);

	appendStringInfo(&memUsage, "{");
	appendStringInfo(&memUsage, "\"used\":%d, ",
					 group->memUsage);
	appendStringInfo(&memUsage, "\"available\":%d, ",
					 group->memQuotaGranted + group->memSharedGranted - group->memUsage);
	appendStringInfo(&memUsage, "\"quota_used\":%d, ",
					 slotUsage);
	appendStringInfo(&memUsage, "\"quota_available\":%d, ",
					 group->memQuotaGranted - group->memQuotaUsed);
	appendStringInfo(&memUsage, "\"quota_granted\":%d, ",
					 group->memQuotaGranted);
	appendStringInfo(&memUsage, "\"quota_proposed\":%d, ",
					 groupGetMemQuotaExpected(&group->caps));
	appendStringInfo(&memUsage, "\"shared_used\":%d, ",
					 group->memSharedUsage);
	appendStringInfo(&memUsage, "\"shared_available\":%d, ",
					 group->memSharedGranted - group->memSharedUsage);
	appendStringInfo(&memUsage, "\"shared_granted\":%d, ",
					 group->memSharedGranted);
	appendStringInfo(&memUsage, "\"shared_proposed\":%d",
					 groupGetMemSharedExpected(&group->caps));
	appendStringInfo(&memUsage, "}");

	return memUsage.data;
}

/*
 * Dump memory information for current resource group.
 */
void
ResGroupDumpMemoryInfo(void)
{
	ResGroupSlotData	*slot = self->slot;
	ResGroupData		*group = self->group;

	if (group)
	{
		Assert(groupSpinLockHeldByMe(group));
		Assert(selfIsAssignedValidGroup());

		write_log("Resource group memory information: "
				  "group memory limit is %d MB, "
				  "shared quota in current resource group is %d MB, "
				  "memory usage in current resource group is %d MB, "
				  "memory quota in current slot is %d MB, "
				  "memory usage in current slot is %d MB, "
				  "memory usage in current proc is %d MB",
				  VmemTracker_ConvertVmemChunksToMB(group->memExpected),
				  VmemTracker_ConvertVmemChunksToMB(group->memSharedGranted),
				  VmemTracker_ConvertVmemChunksToMB(group->memUsage),
				  VmemTracker_ConvertVmemChunksToMB(slot->memQuota),
				  VmemTracker_ConvertVmemChunksToMB(slot->memUsage),
				  VmemTracker_ConvertVmemChunksToMB(self->memUsage));
	}
	else
	{
		Assert(selfIsUnassigned());

		write_log("Resource group memory information: "
				  "memory usage in current proc is %d MB",
				  VmemTracker_ConvertVmemChunksToMB(self->memUsage));
	}
}

/*
 * Reserve 'memoryChunks' number of chunks for current resource group.
 * It will first try to reserve memory from the resource group slot; if the slot
 * quota exceeded, it will reserve memory from the shared zone. It fails if the
 * shared quota is also exceeded, and no memory is reserved.
 *
 * 'overuseChunks' number of chunks can be overused for error handling,
 * in such a case waiverUsed is marked as true.
 */
bool
ResGroupReserveMemory(int32 memoryChunks, int32 overuseChunks, bool *waiverUsed)
{
	ResGroupSlotData	*slot = self->slot;
	ResGroupData		*group = self->group;

	/*
	 * Memories may be allocated before resource group is initialized,
	 * however,we need to track those memories once resource group is
	 * enabled, so we use IsResGroupEnabled() instead of
	 * IsResGroupActivated() here.
	 */
	if (!IsResGroupEnabled())
		return true;

	Assert(memoryChunks >= 0);

	/*
	 * Bypass the limit check when we are not in a valid resource group.
	 * But will update the memory usage of this proc, and it will be added up
	 * when this proc is assigned to a valid resource group.
	 */
	self->memUsage += memoryChunks;
	if (!self->doMemCheck)
	{
		Assert(selfIsUnassigned());
		return true;
	}

	groupSpinLockAcquire(group);

	/* When doMemCheck is on, self must has been assigned to a resgroup. */
	Assert(selfIsAssigned());

	if (selfIsAssignedDroppedGroup())
	{
		/*
		 * However it might already be dropped. For example QE will stay in
		 * a resgroup even after a transaction, so if the resgroup is
		 * concurrently dropped and there is a memory allocation we'll
		 * reach here.
		 *
		 * We would unset the group and slot from self and turn off memory
		 * limit check so we'll not reach here again and again.
		 */

		Oid		groupId = group->groupId;

		groupSpinLockRelease(group);

		if (Debug_resource_group)
			write_log("Resource group is concurrently dropped while reserving memory: "
					  "dropped group=%d, my group=%d",
					  groupId, self->groupId);
		selfUnassignDroppedGroup();
		self->doMemCheck = false;
		return true;
	}

	/* Otherwise we are in a valid resgroup, perform the memory limit check */
	Assert(selfIsAssignedValidGroup());
	Assert(group->memUsage >= 0);
	Assert(self->memUsage >= 0);

	if (CritSectionCount == 0)
	{
		/*
		 * When CritSectionCount is 0 then memory limit must be checked.
		 */

		int32		overuse;

		overuse = groupTryIncMemUsage(group, slot, memoryChunks, overuseChunks);

		/* then check whether there is overuse */
		if (overuse > overuseChunks)
		{
			/* the overuse is larger than allowed range, give up */

			/* also revert in proc */
			Assert(self->memUsage >= memoryChunks);
			self->memUsage -= memoryChunks;

			if (overuseChunks == 0)
				ResGroupDumpMemoryInfo();

			groupSpinLockRelease(group);

			return false;
		}
		else if (overuse > 0)
		{
			/* the over usage is within the allowed threshold */
			*waiverUsed = true;
		}
	}
	else
	{
		/*
		 * Otherwise force the memory to be reserved.
		 */

		groupIncMemUsage(group, slot, memoryChunks);
	}

	groupSpinLockRelease(group);

	return true;
}

/*
 * Release the memory of resource group
 */
void
ResGroupReleaseMemory(int32 memoryChunks)
{
	ResGroupSlotData	*slot = self->slot;
	ResGroupData		*group = self->group;

	if (!IsResGroupActivated())
		return;

	Assert(memoryChunks >= 0);
	Assert(memoryChunks <= self->memUsage);

	self->memUsage -= memoryChunks;
	if (!self->doMemCheck)
	{
		Assert(selfIsUnassigned());
		return;
	}

	groupSpinLockAcquire(group);

	Assert(selfIsAssigned());

	if (selfIsAssignedDroppedGroup())
	{
		Oid		groupId = group->groupId;

		groupSpinLockRelease(group);

		if (Debug_resource_group)
			write_log("Resource group is concurrently dropped while releasing memory: "
					  "dropped group=%d, my group=%d",
					  groupId, self->groupId);
		selfUnassignDroppedGroup();
		self->doMemCheck = false;
		return;
	}

	Assert(selfIsAssignedValidGroup());

	groupDecMemUsage(group, slot, memoryChunks);

	groupSpinLockRelease(group);
}

/*
 * Decide the new resource group concurrency capabilities
 * of pg_resgroupcapability.
 *
 * The decision is based on current runtime information:
 * - 'proposed' will always be set to the latest setting;
 * - 'value' will be set to the most recent version of concurrency
 *   with which current nRunning doesn't exceed the limit;
 */
void
ResGroupDecideConcurrencyCaps(Oid groupId,
							  ResGroupCaps *caps,
							  const ResGroupOpts *opts)
{
	ResGroupData	*group;

	/* If resource group is not in use we can always pick the new settings. */
	if (!IsResGroupActivated())
	{
		caps->concurrency.value = opts->concurrency;
		caps->concurrency.proposed = opts->concurrency;
		return;
	}

	group = ResGroupHashFind(groupId);

	/*
	 * If the runtime usage information doesn't exceed the new setting
	 * then we can pick this setting as the new 'value'.
	 *
	 * FIXME: will nRunning be increased after we released the spinlock?
	 */
	if (group->nRunning <= opts->concurrency)
		caps->concurrency.value = opts->concurrency;

	/* 'proposed' is always set with latest setting */
	caps->concurrency.proposed = opts->concurrency;

	groupSpinLockRelease(group);
}

/*
 * Decide the new resource group memory capabilities
 * of pg_resgroupcapability.
 *
 * The decision is based on current runtime information:
 * - 'proposed' will always be set to the latest setting;
 * - 'value' will be set to the most recent version of memory settings
 *   with which current memory quota usage and memory shared usage
 *   doesn't exceed the limit;
 */
void
ResGroupDecideMemoryCaps(int groupId,
						 ResGroupCaps *caps,
						 const ResGroupOpts *opts)
{
	ResGroupData	*group;
	ResGroupCaps	capsNew;

	/* If resource group is not in use we can always pick the new settings. */
	if (!IsResGroupActivated())
	{
		caps->memLimit.value = opts->memLimit;
		caps->memLimit.proposed = opts->memLimit;

		caps->memSharedQuota.value = opts->memSharedQuota;
		caps->memSharedQuota.proposed = opts->memSharedQuota;

		return;
	}

	group = ResGroupHashFind(groupId);

	ResGroupOptsToCaps(opts, &capsNew);
	/*
	 * If the runtime usage information doesn't exceed the new settings
	 * then we can pick these settings as the new 'value's.
	 */
	if (opts->memLimit <= caps->memLimit.proposed &&
		group->memQuotaUsed <= groupGetMemQuotaExpected(&capsNew) &&
		group->memSharedUsage <= groupGetMemSharedExpected(&capsNew))
	{
		caps->memLimit.value = opts->memLimit;
		caps->memSharedQuota.value = opts->memSharedQuota;
	}

	/* 'proposed' is always set with latest setting */
	caps->memSharedQuota.proposed = opts->memSharedQuota;
	caps->memLimit.proposed = opts->memLimit;

	groupSpinLockRelease(group);
}

int64
ResourceGroupGetQueryMemoryLimit(void)
{
	ResGroupSlotData	*slot = self->slot;
	int64				memSpill;

	Assert(selfIsAssignedValidGroup());

	if (IsResManagerMemoryPolicyNone())
		return 0;

	memSpill = slotGetMemSpill(&slot->caps);

	return memSpill << VmemTracker_GetChunkSizeInBits();
}

/*
 * ResGroupCreate -- initialize the elements for a resource group.
 *
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this - unless we are the startup process.
 */
static ResGroupData *
ResGroupCreate(Oid groupId, const ResGroupCaps *caps)
{
	ResGroupData	*group;
	int32			chunks;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(OidIsValid(groupId));

	group = ResGroupHashNew(groupId);
	if (group == NULL)
		return NULL;

	groupSpinLockInit(group);

	group->groupId = groupId;
	group->caps = *caps;
	group->nRunning = 0;
	ProcQueueInit(&group->waitProcs);
	group->totalExecuted = 0;
	group->totalQueued = 0;
	group->memUsage = 0;
	group->memSharedUsage = 0;
	group->memQuotaUsed = 0;
	memset(&group->totalQueuedTime, 0, sizeof(group->totalQueuedTime));
	group->lockedForDrop = false;
	memset(group->slots, 0, sizeof(group->slots));

	group->memQuotaGranted = 0;
	group->memSharedGranted = 0;
	group->memExpected = groupGetMemExpected(caps);

	/*
	 * There is no need to hold the spinlock here,
	 * just to meet below functions' expectation.
	 */
	groupSpinLockAcquire(group);

	chunks = getChunksFromPool(group, group->memExpected);
	groupAssginChunks(group, chunks, caps);

	groupSpinLockRelease(group);

	return group;
}

/*
 * Try to add chunks into group and slot memory usage when possible.
 *
 * This function will check for the memory limit.
 * Unless the operation is successfully made nothing will be changed
 * in group and slot during the decision process.
 *
 * Please refer to groupIncMemUsage() which does not check for memory limit.
 *
 * If threshold > 0 then an overuse is allowed as long as overuse <= threshold.
 *
 * Return the overuse in chunks.
 * - overuse > threshold: the operation is not allowed as overuse is too large;
 * - overuse > 0: although there is overuse the operation is still made;
 * - overuse <= 0: there is no overuse, the operation is made;
 */
static int32
groupTryIncMemUsage(ResGroupData *group, ResGroupSlotData *slot,
					int32 chunks, int32 threshold)
{
	int32			sharedMemUsage;
	int32			slotMemUsage = slot->memUsage;
	int32			groupMemUsage = group->memUsage;
	int32			groupMemSharedUsage = group->memSharedUsage;
	int32			overuse = 0;

	Assert(groupSpinLockHeldByMe(group));

	/* Add the chunks to memUsage in group */
	groupMemUsage += chunks;

	/* Add the chunks to memUsage in slot */
	slotMemUsage += chunks;

	/* Check whether shared memory should be added */
	sharedMemUsage = slotMemUsage - slot->memQuota;
	if (sharedMemUsage > 0)
	{
		/* Decide how many chunks should be counted as shared memory */
		sharedMemUsage = Min(sharedMemUsage, chunks);

		/* Add these chunks to memSharedUsage in group */
		groupMemSharedUsage += sharedMemUsage;

		/* Calculate the over used chunks */
		overuse = groupMemSharedUsage - group->memSharedGranted;

		if (overuse > threshold)
		{
			/* The overuse is too large, give up */
			return overuse;
		}
	}

	/* Now we know the operation is allowed, actually apply the changes */
	group->memSharedUsage = groupMemSharedUsage;
	group->memUsage = groupMemUsage;
	slot->memUsage = slotMemUsage;

	return overuse;
}

/*
 * Add chunks into group and slot memory usage.
 *
 * This function does not check for the memory limit.
 *
 * Please refer to groupTryIncMemUsage() which checks for memory limit.
 */
static void
groupIncMemUsage(ResGroupData *group, ResGroupSlotData *slot, int32 chunks)
{
	int32			sharedMemUsage;

	Assert(groupSpinLockHeldByMe(group));

	/* Add the chunks to memUsage in group */
	group->memUsage += chunks;

	/* Add the chunks to memUsage in slot */
	slot->memUsage += chunks;

	/* Check whether shared memory should be added */
	sharedMemUsage = slot->memUsage - slot->memQuota;
	if (sharedMemUsage > 0)
	{
		/* Decide how many chunks should be counted as shared memory */
		sharedMemUsage = Min(sharedMemUsage, chunks);

		/* Add these chunks to memSharedUsage in group */
		group->memSharedUsage += sharedMemUsage;
	}
}

/*
 * Sub chunks from group and slot memory usage.
 */
static void
groupDecMemUsage(ResGroupData *group, ResGroupSlotData *slot, int32 chunks)
{
	int32			sharedMemUsage;

	Assert(groupSpinLockHeldByMe(group));

	/* Check whether shared memory should be subed */
	sharedMemUsage = slot->memUsage - slot->memQuota;
	if (sharedMemUsage > 0)
	{
		/* Decide how many chunks should be counted as shared memory */
		sharedMemUsage = Min(sharedMemUsage, chunks);

		/* Sub chunks from memSharedUsage in group */
		group->memSharedUsage -= sharedMemUsage;
		Assert(group->memSharedUsage >= 0);
	}

	/* Sub chunks from memUsage in slot */
	slot->memUsage -= chunks;
	Assert(slot->memUsage >= 0);

	/* Sub chunks from memUsage in group */
	group->memUsage -= chunks;
	Assert(group->memUsage >= 0);
}

/*
 * Get a free resource group slot.
 *
 * A free resource group slot has inUse == false, no other information is checked.
 */
static int
getFreeSlot(ResGroupData *group)
{
	int i;

	Assert(groupSpinLockHeldByMe(group));

	for (i = 0; i < RESGROUP_MAX_SLOTS; i++)
	{
		if (group->slots[i].inUse)
			continue;

		group->slots[i].inUse = true;
		return i;
	}

	Assert(false && "No free slot available");
	return InvalidSlotId;
}

/*
 * Get a slot with memory quota granted.
 *
 * A slot can be got with this function if there is enough memory quota
 * available and the concurrency limit is not reached.
 *
 * On success the memory quota is marked as granted, nRunning is increased
 * and the slot's inUse flag is also set, the slot id is returned.
 *
 * On failure nothing is changed and InvalidSlotId is returned.
 */
static int
getSlot(ResGroupData *group)
{
	ResGroupSlotData	*slot;
	int32				slotMemQuota;
	int					slotId;
	ResGroupCaps		*caps;

	Assert(pResGroupControl->segmentsOnMaster > 0);
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(groupSpinLockHeldByMe(group));
	Assert(groupIsNotDropped(group));

	caps = &group->caps;

	/* First check if the concurrency limit is reached */
	if (group->nRunning >= caps->concurrency.proposed)
	{
		return InvalidSlotId;
	}

	groupAcquireMemQuota(group, caps);

	/* Then check for memory quota */

	/* Calculate the expected per slot quota */
	slotMemQuota = slotGetMemQuotaExpected(caps);

	Assert(slotMemQuota > 0);
	Assert(group->memQuotaUsed >= 0);
	Assert(group->memQuotaUsed <= group->memQuotaGranted);

	if (group->memQuotaUsed + slotMemQuota > group->memQuotaGranted)
	{
		/* No enough memory quota available, give up */
		return InvalidSlotId;
	}

	group->memQuotaUsed += slotMemQuota;

	/* Now actually get a free slot */
	slotId = getFreeSlot(group);
	Assert(slotId != InvalidSlotId);

	slot = &group->slots[slotId];
	Assert(slot->inUse);

	/* Grant the memory quota to it */
	slot->memQuota = slotMemQuota;

	/* Store the config snapshot to it */
	slot->caps = *caps;

	/* And finally increase nRunning */
	group->nRunning += 1;

	return slotId;
}

/*
 * Put back the slot assigned to self.
 *
 * This will release a slot, its memory quota will be freed and
 * nRunning will be decreased.
 *
 * Return true if should wakeup other resgroups.
 */
static bool
putSlot(void)
{
	ResGroupSlotData	*slot = self->slot;
	ResGroupData		*group = self->group;
	bool				shouldWakeUp;

	Assert(groupSpinLockHeldByMe(group));
	Assert(Gp_role == GP_ROLE_DISPATCH);

	Assert(selfIsAssignedValidGroup());
	Assert(group->memQuotaUsed >= 0);
	Assert(group->nRunning > 0);

	selfUnsetSlot();

	Assert(slot->inUse);
	Assert(slot->memQuota > 0);

	/* Return the memory quota granted to this slot */
	group->memQuotaUsed -= slot->memQuota;
	Assert(group->memQuotaUsed >= 0);

	shouldWakeUp = groupReleaseMemQuota(group, slot);

	/* Mark the slot as free */
	slot->inUse = false;

	/* And finally decrease nRunning */
	group->nRunning -= 1;

	return shouldWakeUp;
}

/*
 * Acquire a resource group slot
 *
 * Call this function at the start of the transaction.
 * Current resgroup and the granted slot will be set to self.
 *
 * The spinlock is acquired, it should be released by the callers.
 */
static void
ResGroupSlotAcquire(void)
{
	ResGroupData	*group;
	Oid				 groupId;

retry:
	Assert(selfIsUnassigned());

	/* always find out the up-to-date resgroup id */
	groupId = GetResGroupIdForRole(GetUserId());
	if (groupId == InvalidOid)
		groupId = superuser() ? ADMINRESGROUP_OID : DEFAULTRESGROUP_OID;

	group = ResGroupHashFind(groupId);

	/*
	 * it's neccessary to set group to self before we
	 * got signal and goes to ResGroupWaitCancel
	 */
	selfSetGroup(group);

	/* should not been granted a slot yet */
	Assert(!selfHasSlot());

	/* acquire a slot */
	if (!group->lockedForDrop)
	{
		/* try to get a slot directly */
		MyProc->resSlotId = getSlot(group);

		if (MyProc->resSlotId != InvalidSlotId)
		{
			/* got one, lucky */
			selfSetSlot();

			group->totalExecuted++;
			pgstat_report_resgroup(0, group->groupId);
			Assert(selfIsAssignedValidGroup());
			return;
		}
	}

	/* add into group wait queue */
	MyProc->resWaiting = true;
	groupWaitQueuePush(group, MyProc);

	if (!group->lockedForDrop)
		group->totalQueued++;

	/* The spinlock must be released to call ResGroupWait() */
	groupSpinLockRelease(group);

	/*
	 * wait on the queue
	 * slot will be assigned by the proc wakes me up
	 * if i am waken up by DROP RESOURCE GROUP statement, the
	 * resSlotId will be InvalidSlotId.
	 */
	ResGroupWait(group);

	if (MyProc->resSlotId == InvalidSlotId)
	{
		selfUnsetGroup();
		goto retry;
	}

	/*
	 * The waking process has granted us a valid slot.
	 * Update the statistic information of the resource group.
	 */
	selfSetSlot();
	groupSpinLockAcquire(group);
	addTotalQueueDuration(group);
	group->totalExecuted++;

	pgstat_report_resgroup(0, group->groupId);
	Assert(selfIsAssignedValidGroup());
}

/* Update the total queued time of this group */
/*
 * Wake up the backends in the wait queue when 'concurrency' is increased.
 * This function is called in the callback function of ALTER RESOURCE GROUP.
 */
/*
 * XXX
 *
 * Suppose concurrency is 10, running is 4,
 * memory limit is 0.5, memory shared is 0.4
 *
 * assume currentSharedUsage is 0
 *
 * currentSharedStocks is 0.5*0.4 = 0.2
 * memQuotaGranted is 0.5*0.6 = 0.3
 * memStocksInuse is 0.5*0.4/10*6 = 0.12
 * memStocksFree is 0.3 - 0.12 = 0.18
 *
 * * memLimit: 0.5 -> 0.4
 *   for memQuotaGranted we could free 0.18 - 0.4*0.6/10*6 = 0.18-0.144 = 0.036
 *       new memQuotaGranted is 0.3-0.036 = 0.264
 *       new memStocksFree is 0.18-0.036 = 0.144
 *   for memShared we could free currentSharedStocks - Max(currentSharedUsage, 0.4*0.4)=0.04
 *       new currentSharedStocks is 0.2-0.04 = 0.16
 *
 * * concurrency: 10 -> 20
 *   for memQuotaGranted we could free 0.144 - 0.4*0.6/20*16 = 0.144 - 0.24*0.8 = -0.048
 *   for memShared we could free currentSharedStocks - Max(currentSharedUsage, 0.4*0.4)=0.00
 *
 * * memShared: 0.4 -> 0.2
 *   for memQuotaGranted we could free 0.144 - 0.4*0.8/20*16 = 0.144 - 0.256 = -0.122
 *   for memShared we could free currentSharedUsage - Max(currentSharedUsage, 0.4*0.2)=0.08
 *       new currentSharedStocks is 0.16-0.08 = 0.08
 *
 * * memShared: 0.2 -> 0.6
 *   for memQuotaGranted we could free 0.144 - 0.4*0.4/20*16 = 0.144 - 0.128 = 0.016
 *       new memQuotaGranted is 0.264 - 0.016 = 0.248
 *       new memStocksFree is 0.144 - 0.016 = 0.128
 *   for memShared we could free currentSharedUsage - Max(currentSharedUsage, 0.4*0.6) = -0.18
 *
 * * memLimit: 0.4 -> 0.2
 *   for memQuotaGranted we could free 0.128 - 0.2*0.4/20*16 = 0.128 - 0.064 = 0.064
 *       new memQuotaGranted is 0.248-0.064 = 0.184
 *       new memStocksFree is 0.128 - 0.064 = 0.064
 *   for memShared we could free currentSharedStocks - Max(currentSharedUsage, 0.2*0.6) = -0.04
 */
static bool
groupApplyMemCaps(ResGroupData *group, const ResGroupCaps *caps)
{
	int32 memStocksAvailable;
	int32 memStocksNeeded;
	int32 memStocksToFree;
	int32 memSharedNeeded;
	int32 memSharedToFree;

	Assert(groupSpinLockHeldByMe(group));

	group->memExpected = groupGetMemExpected(caps);

	/* memStocksAvailable is the total free non-shared quota */
	memStocksAvailable = group->memQuotaGranted - group->memQuotaUsed;

	if (caps->concurrency.proposed > group->nRunning)
	{
		/*
		 * memStocksNeeded is the total non-shared quota needed
		 * by all the free slots
		 */
		memStocksNeeded = slotGetMemQuotaExpected(caps) *
			(caps->concurrency.proposed - group->nRunning);

		/*
		 * if memStocksToFree > 0 then we can safely release these
		 * non-shared quota and still have enough quota to run
		 * all the free slots.
		 */
		memStocksToFree = memStocksAvailable - memStocksNeeded;
	}
	else
	{
		memStocksToFree = Min(memStocksAvailable,
							  group->memQuotaGranted - groupGetMemQuotaExpected(caps));
	}

	/* TODO: optimize the free logic */
	if (memStocksToFree > 0)
		group->memQuotaGranted -= memStocksToFree;

	memSharedNeeded = Max(group->memSharedUsage,
						  groupGetMemSharedExpected(caps));
	memSharedToFree = group->memSharedGranted - memSharedNeeded;

	if (memSharedToFree > 0)
		group->memSharedGranted -= memSharedToFree;

	memStocksToFree = Max(memStocksToFree, 0);
	memSharedToFree = Max(memSharedToFree, 0);

	if (memStocksToFree + memSharedToFree > 0)
		returnChunksToPool(group, memStocksToFree + memSharedToFree);

#if 1
	/*
	 * FIXME: why we need this?
	 *
	 * suppose rg1 has memory_limit=10, memory_shared_quota=40,
	 * and session1 is running in rg1.
	 *
	 * now we alter rg1 memory_limit to 40 in another session,
	 * apparently both memory quota and shared quota are expected to increase,
	 * our as our design is to let them increase on new queries,
	 * then for session1 it won't see memory shared quota being increased
	 * until new queries being executed in rg1.
	 */
	groupAcquireMemQuota(group, caps);
#endif

	return (memStocksToFree > 0 || memSharedToFree > 0);
}

/*
 * Get quota from sys pool.
 *
 * chunks is the expected amount to get.
 *
 * return the actual got chunks, might be smaller than expectation.
 */
static int32
getChunksFromPool(ResGroupData *group, int32 chunks)
{
	int32		freeChunks;
	int32		gotChunks;

	Assert(groupSpinLockHeldByMe(group));

	for (;;)
	{
		freeChunks = pg_atomic_read_u32((pg_atomic_uint32 *)
										&pResGroupControl->freeChunks);

		/* decide how many chunks to get */
		gotChunks = Min(freeChunks, chunks);
		Assert(gotChunks >= 0);

		if (gotChunks == 0)
			break;	/* no free chunks, give up */

		/* try to set the new value */
		if (pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)
										   &pResGroupControl->freeChunks,
										   (uint32 *) &freeChunks,
										   freeChunks - gotChunks))
			break;	/* done */

		/* retry if concurrently updated */
	}

	LOG_RESGROUP_DEBUG(LOG, "%d chunks required by group %d, "
					   "%d actually allocated, syspool: %d -> %d",
					   chunks, group->groupId, gotChunks,
					   freeChunks, freeChunks - gotChunks);

	freeChunks -= gotChunks;

	Assert(freeChunks >= 0);
	Assert(freeChunks <= pResGroupControl->totalChunks);

	return gotChunks;
}

/*
 * Return chunks to sys pool.
 */
static void
returnChunksToPool(ResGroupData *group, int32 chunks)
{
	int32		freeChunks;

	Assert(groupSpinLockHeldByMe(group));
	Assert(chunks > 0);

	freeChunks = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)
										 &pResGroupControl->freeChunks,
										 chunks);

	LOG_RESGROUP_DEBUG(LOG, "%d chunks returned by group %d, syspool: %d -> %d",
					   chunks, group->groupId, freeChunks - chunks, freeChunks);

	Assert(freeChunks >= 0);
	Assert(freeChunks <= pResGroupControl->totalChunks);
}

/*
 * Assign the chunks we get from the sys pool to the 'quota' and 'shared'
 * part of the group, the amount is calculated from caps.
 */
static void
groupAssginChunks(ResGroupData *group, int32 chunks, const ResGroupCaps *caps)
{
	int32 delta;
	int32 memQuotaGranted = groupGetMemQuotaExpected(caps);

	Assert(groupSpinLockHeldByMe(group));

	delta = memQuotaGranted - group->memQuotaGranted;
	if (delta >= 0)
	{
		delta = Min(chunks, delta);

		group->memQuotaGranted += delta;
		chunks -= delta;
	}

	group->memSharedGranted += chunks;
}

/*
 * Calculate the total memory chunks of the segment
 */
static int32
getSegmentChunks(void)
{
	int nsegments = Gp_role == GP_ROLE_EXECUTE ? host_segments : pResGroupControl->segmentsOnMaster;

	Assert(nsegments > 0);

	return ResGroupOps_GetTotalMemory() * gp_resource_group_memory_limit / nsegments;
}

/*
 * Get total expected memory quota of a group in chunks
 */
static int32
groupGetMemExpected(const ResGroupCaps *caps)
{
	Assert(pResGroupControl->totalChunks > 0);
	return pResGroupControl->totalChunks * caps->memLimit.proposed / 100;
}

/*
 * Get per-group expected memory quota in chunks
 */
static int32
groupGetMemQuotaExpected(const ResGroupCaps *caps)
{
	return groupGetMemExpected(caps) *
		(100 - caps->memSharedQuota.proposed) / 100;
}

/*
 * Get per-group expected memory shared quota in chunks
 */
static int32
groupGetMemSharedExpected(const ResGroupCaps *caps)
{
	return groupGetMemExpected(caps) - groupGetMemQuotaExpected(caps);
}

/*
 * Get per-group expected memory spill in chunks
 */
static int32
groupGetMemSpillTotal(const ResGroupCaps *caps)
{
	return groupGetMemExpected(caps) * memory_spill_ratio / 100;
}

/*
 * Get per-slot expected memory quota in chunks
 */
static int32
slotGetMemQuotaExpected(const ResGroupCaps *caps)
{
	Assert(caps->concurrency.proposed != 0);
	return Max(1, groupGetMemQuotaExpected(caps) / caps->concurrency.proposed);
}

/*
 * Get per-slot expected memory spill in chunks
 */
static int32
slotGetMemSpill(const ResGroupCaps *caps)
{
	Assert(caps->concurrency.proposed != 0);
	return groupGetMemSpillTotal(caps) / caps->concurrency.proposed;
}

/*
 * Attempt to wake up pending slots in the group as long as there are free
 * slots and enough memory quota.
 */
static void
wakeupSlots(ResGroupData *group)
{
	Assert(groupSpinLockHeldByMe(group));

	while (!groupWaitQueueIsEmpty(group))
	{
		PGPROC		*waitProc;
		int			slotId;

		/* try to get a slot for that proc */
		slotId = getSlot(group);
		if (slotId == InvalidSlotId)
			/* if can't get one then give up */
			break;

		/* wake up one process in the wait queue */
		waitProc = groupWaitQueuePop(group);
		Assert(waitProc->resWaiting != false);
		Assert(waitProc->resSlotId == InvalidSlotId);

		waitProc->resWaiting = false;
		waitProc->resSlotId = slotId;
		SetLatch(&waitProc->procLatch);
	}
}

/*
 * When a group returns chunks to sys pool, we need to wake up
 * the transactions waiting on other groups for memory quota.
 *
 * The skipGroup's spinlock must not be held to call this function.
 */
static void
wakeupGroups(ResGroupData *skipGroup)
{
	int				i;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	Assert(!groupSpinLockHeldByMe(skipGroup));

	for (i = 0; i < MaxResourceGroups; i++)
	{
		ResGroupData	*group = &pResGroupControl->groups[i];
		int32			delta;

		groupSpinLockAcquire(group);

		delta = group->memExpected - group->memQuotaGranted - group->memSharedGranted;

		if (group->groupId != InvalidOid &&
			group->groupId != skipGroup->groupId &&
			!group->lockedForDrop &&
			!groupWaitQueueIsEmpty(group) &&
			delta > 0)
		{
			wakeupSlots(group);
		}

		groupSpinLockRelease(group);

		if (!pResGroupControl->freeChunks)
			break;
	}
}

/*
 * Release memory quota when a slot gets freed and the caps has been changed,
 * the released memory quota includes:
 * * the slot over-used quota
 * * the group over-used shared quota
 */
static bool 
groupReleaseMemQuota(ResGroupData *group, ResGroupSlotData *slot)
{
	int32		memQuotaNeedFree;
	int32		memSharedNeeded;
	int32		memQuotaToFree;
	int32		memSharedToFree;
	int32       memQuotaExpected;
	ResGroupCaps *caps = &group->caps;

	Assert(groupSpinLockHeldByMe(group));

	/* Return the over used memory quota to sys */
	memQuotaNeedFree = group->memQuotaGranted - groupGetMemQuotaExpected(caps);
	memQuotaToFree = memQuotaNeedFree > 0 ? Min(memQuotaNeedFree, slot->memQuota) : 0;

	if (caps->concurrency.proposed > 0)
	{
		/*
		 * Under this situation, when this slot is released,
		 * others will not be blocked by concurrency limit if
		 * they come to acquire this slot. So we could decide
		 * not to give all the memory to syspool even if we could.
		 */
		memQuotaExpected = slotGetMemQuotaExpected(caps);
		if (memQuotaToFree > memQuotaExpected)
			memQuotaToFree -= memQuotaExpected;
	}

	if (memQuotaToFree > 0)
		group->memQuotaGranted -= memQuotaToFree; 

	/* Return the over used shared quota to sys */
	memSharedNeeded = Max(group->memSharedUsage,
						  groupGetMemSharedExpected(caps));
	memSharedToFree = group->memSharedGranted - memSharedNeeded;
	if (memSharedToFree > 0)
	{
		group->memSharedGranted -= memSharedToFree;
		Assert(group->memSharedGranted >= 0);
	}

	memQuotaToFree = Max(memQuotaToFree, 0);
	memSharedToFree = Max(memSharedToFree, 0);

	if (memQuotaToFree + memSharedToFree <= 0)
		return false;

	returnChunksToPool(group, memQuotaToFree + memSharedToFree);
	return true;
}

/*
 * Try to acquire enough quota & shared quota for current group,
 * the actual acquired quota depends on system loads.
 */
static void
groupAcquireMemQuota(ResGroupData *group, const ResGroupCaps *caps)
{
	int32 currentMemStocks = group->memSharedGranted + group->memQuotaGranted;
	int32 neededMemStocks = group->memExpected - currentMemStocks;

	Assert(groupSpinLockHeldByMe(group));

	if (neededMemStocks > 0)
	{
		int32 chunks = getChunksFromPool(group, neededMemStocks);
		groupAssginChunks(group, chunks, caps);
	}
}

static void
addTotalQueueDuration(ResGroupData *group)
{
	if (group == NULL)
		return;

	Assert(groupSpinLockHeldByMe(group));

	TimestampTz start = pgstat_fetch_resgroup_queue_timestamp();
	TimestampTz now = GetCurrentTimestamp();
	Datum durationDatum = DirectFunctionCall2(timestamptz_age, TimestampTzGetDatum(now), TimestampTzGetDatum(start));
	Datum sumDatum = DirectFunctionCall2(interval_pl, IntervalPGetDatum(&group->totalQueuedTime), durationDatum);
	memcpy(&group->totalQueuedTime, DatumGetIntervalP(sumDatum), sizeof(Interval));
}

/*
 * Release the resource group slot
 *
 * Call this function at the end of the transaction.
 *
 * Return true if should wakeup other resgroups.
 */
static bool
ResGroupSlotRelease(void)
{
	ResGroupData	*group = self->group;
	bool			shouldWakeUp;

	Assert(groupSpinLockHeldByMe(group));
	Assert(selfIsAssignedValidGroup());

	shouldWakeUp = putSlot();
	Assert(!selfHasSlot());

	/*
	 * My slot is put back, then how many queuing queries should I wake up?
	 * Maybe zero, maybe one, maybe more, depends on how the resgroup's
	 * configuration were changed during our execution.
	 */
	while (!groupWaitQueueIsEmpty(group))
	{
		PGPROC		*waitProc;
		int			slotId;

		slotId = getSlot(group);
		if (slotId == InvalidSlotId)
			break;

		/* wake up one process in the wait queue */
		waitProc = groupWaitQueuePop(group);
		Assert(waitProc->resWaiting != false);
		Assert(waitProc->resSlotId == InvalidSlotId);

		waitProc->resSlotId = slotId;	/* pass the slot to new query */
		waitProc->resWaiting = false;
		SetLatch(&waitProc->procLatch);
	}

	return shouldWakeUp;
}

/*
 * Serialize the resource group information that need to dispatch to segment.
 */
void
SerializeResGroupInfo(StringInfo str)
{
	int i;
	int tmp;
	ResGroupCaps caps0;
	ResGroupCap *caps;

	if (selfIsAssignedValidGroup())
		caps = (ResGroupCap *) &self->caps;
	else
	{
		Assert(selfIsUnassigned());
		MemSet(&caps0, 0, sizeof(caps0));
		caps = (ResGroupCap *) &caps0;
	}

	tmp = htonl(self->groupId);
	appendBinaryStringInfo(str, (char *) &tmp, sizeof(self->groupId));

	tmp = htonl(self->slotId);
	appendBinaryStringInfo(str, (char *) &tmp, sizeof(self->slotId));

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		tmp = htonl(caps[i].value);
		appendBinaryStringInfo(str, (char *) &tmp, sizeof(caps[i].value));

		tmp = htonl(caps[i].proposed);
		appendBinaryStringInfo(str, (char *) &tmp, sizeof(caps[i].proposed));
	}
}

/*
 * Deserialize the resource group information dispatched by QD.
 */
void
DeserializeResGroupInfo(struct ResGroupCaps *capsOut,
						const char *buf, int len)
{
	int			i;
	int			tmp;
	const char	*ptr = buf;
	ResGroupCap *caps = (ResGroupCap *) capsOut;

	Assert(len > 0);

	/* TODO: don't deserialize into self directly */

	memcpy(&tmp, ptr, sizeof(self->groupId));
	self->groupId = ntohl(tmp);
	ptr += sizeof(self->groupId);

	memcpy(&tmp, ptr, sizeof(self->slotId));
	self->slotId = ntohl(tmp);
	ptr += sizeof(self->slotId);

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		memcpy(&tmp, ptr, sizeof(caps[i].value));
		caps[i].value = ntohl(tmp);
		ptr += sizeof(caps[i].value);

		memcpy(&tmp, ptr, sizeof(caps[i].proposed));
		caps[i].proposed = ntohl(tmp);
		ptr += sizeof(caps[i].proposed);
	}

	Assert(len == ptr - buf);
}

/*
 * Check whether should assign resource group on master.
 */
bool
ShouldAssignResGroupOnMaster(void)
{
	return IsResGroupActivated() &&
		IsNormalProcessingMode() &&
		Gp_role == GP_ROLE_DISPATCH &&
		!AmIInSIGUSR1Handler();
}

/*
 * On master, QD is assigned to a resource group at the beginning of a transaction.
 * It will first acquire a slot from the resource group, and then, it will get the
 * current capability snapshot, update the memory usage information, and add to
 * the corresponding cgroup.
 */
void
AssignResGroupOnMaster(void)
{
	ResGroupData		*group;
	ResGroupSlotData	*slot;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	PG_TRY();
	{
		/* Acquire slot */
		Assert(selfIsUnassigned());
		ResGroupSlotAcquire();
		Assert(selfIsAssignedValidGroup());
		Assert(!self->doMemCheck);

		group = self->group;
		Assert(groupSpinLockHeldByMe(group));

		/* Init slot */
		slot = self->slot;
		Assert(slot->memQuota > 0);
		slot->sessionId = gp_session_id;
		pg_atomic_add_fetch_u32((pg_atomic_uint32*)&slot->nProcs, 1);

		/* Init self */
		self->caps = slot->caps;
		Assert(pResGroupControl != NULL);
		Assert(pResGroupControl->segmentsOnMaster > 0);

		/* Add proc memory accounting info into group and slot */
		groupIncMemUsage(group, slot, self->memUsage);

		/* Start memory limit checking */
		self->doMemCheck = true;

		/* Don't error out before this line in this function */
		SIMPLE_FAULT_INJECTOR(ResGroupAssignedOnMaster);

		/* Add into cgroup */
		ResGroupOps_AssignGroup(group->groupId, MyProcPid);

		/* Set spill guc */
		ResGroupSetMemorySpillRatio(&slot->caps);

		groupSpinLockRelease(group);
	}
	PG_CATCH();
	{
		if (self->group)
			groupSpinLockRelease(self->group);

		UnassignResGroupOnMaster();
		PG_RE_THROW();
	}
	PG_END_TRY();
}

/*
 * Detach from a resource group at the end of the transaction.
 */
void
UnassignResGroupOnMaster(void)
{
	ResGroupData		*group = self->group;
	ResGroupSlotData	*slot = self->slot;
	bool				shouldWakeUp;

	if (selfIsUnassigned())
	{
		Assert(self->doMemCheck == false);
		return;
	}

	groupSpinLockAcquire(group);

	Assert(selfIsAssignedValidGroup());

	/* Stop memory limit checking */
	self->doMemCheck = false;

	/* Sub proc memory accounting info from group and slot */
	groupDecMemUsage(group, slot, self->memUsage);

	/* Cleanup self */
	if (self->memUsage > 10)
		LOG_RESGROUP_DEBUG(LOG, "Idle proc memory usage: %d", self->memUsage);

	/* Cleanup slotInfo */
	pg_atomic_sub_fetch_u32((pg_atomic_uint32*)&slot->nProcs, 1);

	/* Release the slot */
	shouldWakeUp = ResGroupSlotRelease();
	Assert(!selfHasSlot());

	groupSpinLockRelease(group);

	if (shouldWakeUp)
		wakeupGroups(group);

	/* Cleanup group */
	selfUnsetGroup();

	Assert(selfIsUnassigned());
}

/*
 * QEs are not assigned/unassigned to a resource group on segments for each
 * transaction, instead, they switch resource group when a new resource group
 * id or slot id is dispatched.
 */
void
SwitchResGroupOnSegment(const char *buf, int len)
{
	Oid		prevGroupId;
	int		prevSlotId;
	ResGroupCaps		caps;
	ResGroupData		*group;
	ResGroupSlotData	*slot;
	ResGroupData		*prevGroup = NULL;
	ResGroupSlotData	*prevSlot = NULL;

	selfValidateResGroupInfo();

	prevGroupId = self->groupId;
	prevSlotId = self->slotId;
	prevGroup = self->group;
	prevSlot = self->slot;

	/* Stop memory limit checking */
	self->doMemCheck = false;

	DeserializeResGroupInfo(&caps, buf, len);

	AssertImply(self->groupId != InvalidOid,
				self->slotId != InvalidSlotId);
	AssertImply(prevGroupId != InvalidOid,
				prevSlotId != InvalidSlotId);

	if (self->groupId == InvalidOid)
	{
		/* about to switch to a none resgroup state ... */

		self->group = NULL;

		if (prevGroup)
		{
			/* from another resgroup, so detach from it */

			groupSpinLockAcquire(prevGroup);

			Assert(prevGroup->groupId == prevGroupId);

			/* Sub proc memory accounting info from group and slot */
			groupDecMemUsage(prevGroup, prevSlot, self->memUsage);

			/* Update info in previous slot */
			pg_atomic_sub_fetch_u32((pg_atomic_uint32*)&prevSlot->nProcs, 1);

			groupReleaseMemQuota(prevGroup, prevSlot);

			groupSpinLockRelease(prevGroup);
		}
		else
		{
			/* from a none resgroup state, so nothing to do */
			Assert(prevGroupId == InvalidOid);
			Assert(prevSlotId == InvalidSlotId);
		}

		Assert(selfIsUnassigned());
		return;
	}

	/* now we know we are about to switch to some resgroup ... */

	if (prevGroup && prevGroup->groupId != prevGroupId)
	{
		/* from a dropped resgroup, so behave like we are from
		 * a none resgroup state */

		prevGroup = NULL;
		prevSlot = NULL;
		prevGroupId = InvalidOid;
		prevSlotId = InvalidSlotId;
	}

	/* now we are sure to switch to a valid resgroup */

	group = ResGroupHashFind(self->groupId);
	Assert(group != NULL);
	/* we don't set this group to self until end of this function */

	/* Init self */
	Assert(host_segments > 0);
	Assert(caps.concurrency.proposed > 0);
	Assert(self->slotId != InvalidSlotId);
	self->caps = caps;
	self->slot = &group->slots[self->slotId];
	Assert(selfHasSlot());

	/* Init slot */
	slot = self->slot;
	slot->sessionId = gp_session_id;
	slot->caps = caps;
	slot->memQuota = slotGetMemQuotaExpected(&caps);
	ResGroupSetMemorySpillRatio(&caps);
	Assert(slot->memQuota > 0);

	if (prevGroup != group || prevSlot != slot)
	{
		/* we are switching between different resgroups */

		if (prevGroup)
		{
			/* the previous one is valid, so detach from it */
			Assert(prevSlot != NULL);

			/*
			 * We should not hold two resgroups' spinlock at the same time
			 * in case of possible deadlock.
			 */
			groupSpinLockRelease(group);
			groupSpinLockAcquire(prevGroup);

			/* Sub proc memory accounting info from group and slot */
			groupDecMemUsage(prevGroup, prevSlot, self->memUsage);

			/* Update info in previous slot */
			pg_atomic_sub_fetch_u32((pg_atomic_uint32*)&prevSlot->nProcs, 1);

			groupReleaseMemQuota(prevGroup, prevSlot);

			groupSpinLockRelease(prevGroup);
			groupSpinLockAcquire(group);
		}

		/* now attach to the new one */

		groupAcquireMemQuota(group, &slot->caps);

		/* Add proc memory accounting info into group and slot */
		groupIncMemUsage(group, slot, self->memUsage);

		/* Update info in new slot */
		pg_atomic_add_fetch_u32((pg_atomic_uint32*)&slot->nProcs, 1);
	}

	self->group = group;

	/* finally we can say we are in a valid resgroup */
	Assert(selfIsAssignedValidGroup());

	/* Start memory limit checking */
	self->doMemCheck = true;

	/* Add into cgroup */
	ResGroupOps_AssignGroup(self->groupId, MyProcPid);

	groupSpinLockRelease(group);
}

/*
 * Wait on the queue of resource group
 *
 * Neither of the LWLock nor spinlock should be held.
 */
static void
ResGroupWait(ResGroupData *group)
{
	PGPROC *proc = MyProc;

	Assert(!LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(!groupSpinLockHeldByMe(group));
	Assert(selfHasGroup());
	Assert(!selfHasSlot());

	pgstat_report_resgroup(GetCurrentTimestamp(), group->groupId);

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

	pgstat_report_waiting(PGBE_WAITING_NONE);
}

/*
 * ResGroupHashNew -- return a new (empty) group object to initialize.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static ResGroupData *
ResGroupHashNew(Oid groupId)
{
	int			i;
	bool		found;
	ResGroupHashEntry *entry;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	if (groupId == InvalidOid)
		return NULL;

	for (i = 0; i < pResGroupControl->nGroups; i++)
	{
		if (pResGroupControl->groups[i].groupId == InvalidOid)
			break;
	}
	Assert(i < pResGroupControl->nGroups);

	entry = (ResGroupHashEntry *)
		hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_ENTER_NULL, &found);
	/* caller should test that the group does not exist already */
	Assert(!found);
	entry->index = i;

	return &pResGroupControl->groups[i];
}

/*
 * ResGroupHashFind -- return the group for a given oid.
 *
 * The returned group's spinlock is acquired, should be released by callers.
 */
static ResGroupData *
ResGroupHashFind(Oid groupId)
{
	bool				found;
	ResGroupHashEntry	*entry;
	ResGroupData		*group;

	LWLockAcquire(ResGroupLock, LW_SHARED);

	entry = (ResGroupHashEntry *)
		hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_FIND, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				errmsg("Cannot find resource group %d in shared memory", groupId)));
		return NULL;
	}

	Assert(entry->index < pResGroupControl->nGroups);

	group = &pResGroupControl->groups[entry->index];
	groupSpinLockAcquire(group);

	LWLockRelease(ResGroupLock);
	return group;
}

/*
 * ResGroupHashRemove -- remove the group for a given oid.
 *
 * The returned group's spinlock is acquired, should be released by callers.
 */
static ResGroupData *
ResGroupHashRemove(Oid groupId)
{
	bool		found;
	ResGroupHashEntry	*entry;
	ResGroupData		*group;
	int32		toFree;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	entry = (ResGroupHashEntry *) hash_search(pResGroupControl->htbl,
											  (void *) &groupId,
											  HASH_REMOVE,
											  &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				errmsg("Cannot find resource group %d in shared memory to remove",
					   groupId)));
		return NULL;
	}

	group = &pResGroupControl->groups[entry->index];
	groupSpinLockAcquire(group);

	LWLockRelease(ResGroupLock);

	/*
	 * Once groupId is set to InvalidOid this resgroup is considered
	 * as dropped, in palloc() & pfree() no check/update is performed
	 * on dropped resgroup.
	 *
	 * We should make this happen before touching the memory quota
	 * otherwise the quota information might be updated concurrently
	 * in palloc() / pfree().
	 */
	group->groupId = InvalidOid;

	toFree = group->memQuotaGranted + group->memSharedGranted;
	group->memQuotaGranted = 0;
	group->memSharedGranted = 0;

	if (toFree > 0)
	{
		ResGroupData	group0 = *group;

		/*
		 * The group passed to returnChunksToPool() is only for debug purpose,
		 * here we construct a fake group with groupId still set to meet
		 * returnChunksToPool()'s expectation.
		 */
		group0.groupId = groupId;
		returnChunksToPool(&group0, toFree);
	}

	return group;
}

/* Process exit without waiting for slot or received SIGTERM */
static void
AtProcExit_ResGroup(int code, Datum arg)
{
	ResGroupWaitCancel();
}

/*
 * Handle the interrupt cases when waiting on the queue
 *
 * The proc may wait on the queue for a slot, or wait for the
 * DROP transaction to finish. In the first case, at the same time
 * we get interrupted (SIGINT or SIGTERM), we could have been
 * granted a slot or not. In the second case, there's no running
 * transaction in the group. If the DROP transaction is finished
 * (commit or abort) at the same time as we get interrupted,
 * MyProc should have been removed from the wait queue, and the
 * ResGroupData entry may have been removed if the DROP is committed.
 */
static void
ResGroupWaitCancel(void)
{
	ResGroupData	*group = self->group;
	bool			shouldWakeUp = false;

	/* Process exit without waiting for slot */
	if (!selfHasGroup() || !localResWaiting)
		return;

	/* We are sure to be interrupted in the for loop of ResGroupWait now */
	groupSpinLockAcquire(group);

	if (procIsInWaitQueue(MyProc))
	{
		/* Still waiting on the queue when get interrupted, remove myself from the queue */

		Assert(!groupWaitQueueIsEmpty(group));
		Assert(MyProc->resWaiting);
		Assert(selfHasGroup());
		Assert(!selfHasSlot());

		addTotalQueueDuration(group);

		groupWaitQueueErase(group, MyProc);
	}
	else if (!procIsInWaitQueue(MyProc) && selfHasSlot())
	{
		/* Woken up by a slot holder */

		Assert(selfIsAssignedValidGroup());
		shouldWakeUp = putSlot();
		Assert(!selfHasSlot());

		group->totalExecuted++;
		addTotalQueueDuration(group);

		/*
		 * Similar as ResGroupSlotRelease(), how many pending queries to
		 * wake up depends on how many slots we can get.
		 */
		wakeupSlots(group);
	}
	else
	{
		/*
		 * The transaction of DROP RESOURCE GROUP is finished,
		 * ResGroupSlotAcquire will do the retry.
		 */
	}

	localResWaiting = false;
	pgstat_report_waiting(PGBE_WAITING_NONE);
	selfUnsetGroup();
	groupSpinLockRelease(group);

	if (shouldWakeUp)
		wakeupGroups(group);
}

static void
ResGroupSetMemorySpillRatio(const ResGroupCaps *caps)
{
	char value[64];

	snprintf(value, sizeof(value), "%d", caps->memSpillRatio.proposed);
	set_config_option("memory_spill_ratio", value, PGC_USERSET, PGC_S_RESGROUP, GUC_ACTION_SET, true);
}

void
ResGroupGetMemInfo(int *memLimit, int *slotQuota, int *sharedQuota)
{
	const ResGroupCaps *caps = &self->caps;

	*memLimit = groupGetMemExpected(caps);
	*slotQuota = caps->concurrency.proposed ? slotGetMemQuotaExpected(caps) : -1;
	*sharedQuota = groupGetMemSharedExpected(caps);
}

/*
 * Convert ResGroupOpts to ResGroupCaps
 */
void
ResGroupOptsToCaps(const ResGroupOpts *optsIn, ResGroupCaps *capsOut)
{
	int i;
	ResGroupCap		*caps = (ResGroupCap *) capsOut;
	const int32		*opts = (int32 *) optsIn;

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		caps[i].value = opts[i];
		caps[i].proposed = opts[i];
	}
}

/*
 * Convert ResGroupCaps to ResGroupOpts
 */
void
ResGroupCapsToOpts(const ResGroupCaps *capsIn, ResGroupOpts *optsOut)
{
	int i;
	const ResGroupCap	*caps = (ResGroupCap *) capsIn;
	int32				*opts = (int32 *) optsOut;

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
		opts[i] = caps[i].proposed;
}

/*
 * Validate the consistency of the resgroup information in self.
 *
 * This function requires the slot and group to be in
 * a consistent status, they must both be set or unset,
 * so calling this function during the assign/unassign/switch process
 * might cause an error, use with caution.
 */
static void
selfValidateResGroupInfo(void)
{
	Assert(self->memUsage >= 0);

	AssertImply(self->groupId != InvalidOid,
				self->slotId != InvalidSlotId);
	AssertImply(self->groupId != InvalidOid,
				self->group != NULL);
	AssertImply(self->slotId != InvalidSlotId,
				self->slot != NULL);
}

/*
 * Check whether self is assigned and the assigned resgroup is dropped.
 *
 * The assigned resgroup is dropped if its groupId is invalid or
 * different with the groupId recorded in self.
 *
 * This function requires the slot and group to be in
 * a consistent status, they must both be set or unset,
 * so calling this function during the assign/unassign/switch process
 * might cause an error, use with caution.
 *
 * Even selfIsAssignedDroppedGroup() is true it doesn't mean the assign/switch
 * process is completely done, for example the memory accounting
 * information might not been updated yet.
 */
static bool
selfIsAssignedDroppedGroup(void)
{
	selfValidateResGroupInfo();

	return self->groupId != InvalidOid
		&& self->groupId != self->group->groupId;
}

/*
 * Check whether self is assigned and the assigned resgroup is valid.
 *
 * The assigned resgroup is valid if its groupId is valid and equal
 * to the groupId recorded in self.
 *
 * This function requires the slot and group to be in
 * a consistent status, they must both be set or unset,
 * so calling this function during the assign/unassign/switch process
 * might cause an error, use with caution.
 *
 * Even selfIsAssignedValidGroup() is true it doesn't mean the assign/switch
 * process is completely done, for example the memory accounting
 * information might not been updated yet.
 */
static bool
selfIsAssignedValidGroup(void)
{
	selfValidateResGroupInfo();

	return self->groupId != InvalidOid
		&& self->groupId == self->group->groupId;
}

#ifdef USE_ASSERT_CHECKING
/*
 * Check whether self is assigned.
 *
 * This is mostly equal to (selfHasSlot() && selfHasGroup()),
 * however this function requires the slot and group to be in
 * a consistent status, they must both be set or unset,
 * so calling this function during the assign/unassign/switch process
 * might cause an error, use with caution.
 *
 * Even selfIsAssigned() is true it doesn't mean the assign/switch
 * process is completely done, for example the memory accounting
 * information might not been updated yet.
 *
 * This function doesn't check whether the assigned resgroup
 * is valid or dropped.
 */
static bool
selfIsAssigned(void)
{
	selfValidateResGroupInfo();

	return self->groupId != InvalidOid;
}
#endif//USE_ASSERT_CHECKING

/*
 * Check whether self is unassigned.
 *
 * This is mostly equal to (!selfHasSlot() && !selfHasGroup()),
 * however this function requires the slot and group to be in
 * a consistent status, they must both be set or unset,
 * so calling this function during the assign/unassign/switch process
 * might cause an error, use with caution.
 *
 * Even selfIsUnassigned() is true it doesn't mean the unassign/switch
 * process is completely done, for example the memory accounting
 * information might not been updated yet.
 */
static bool
selfIsUnassigned(void)
{
	selfValidateResGroupInfo();

	return self->groupId == InvalidOid;
}

/*
 * Unassign from an assigned but dropped resgroup.
 *
 * This is mostly equal to selfUnsetGroup() + selfUnsetSlot(),
 * however this function requires self must be assigned
 * to that dropped resgroup before unassign.
 */
static void
selfUnassignDroppedGroup(void)
{
	Assert(selfIsAssignedDroppedGroup());

	selfUnsetSlot();
	selfUnsetGroup();

	Assert(selfIsUnassigned());
}

/*
 * Check whether self has been set a slot.
 *
 * Consistency will be checked on the slotId and slot pointer.
 *
 * We don't check whether a resgroup is set or not.
 */
static bool
selfHasSlot(void)
{
	AssertImply(self->slotId != InvalidSlotId,
				self->slot != NULL);

	return self->slotId != InvalidSlotId;
}

/*
 * Check whether self has been set a resgroup.
 *
 * Consistency will be checked on the groupId and group pointer.
 *
 * We don't check whether the resgroup is valid or dropped.
 *
 * We don't check whether a slot is set or not.
 */
static bool
selfHasGroup(void)
{
	AssertImply(self->groupId != InvalidOid,
				self->group != NULL);

	return self->groupId != InvalidOid;
}

/*
 * Set both the groupId and the group pointer in self.
 *
 * The group must not be dropped.
 *
 * Some over limitations are put to force the caller understand
 * what it's doing and what it wants:
 * - self must has not been set a resgroup;
 */
static void
selfSetGroup(ResGroupData *group)
{
	Assert(selfIsUnassigned());
	Assert(groupIsNotDropped(group));

	self->group = group;
	self->groupId = group->groupId;
}

/*
 * Unset both the groupId and the resgroup pointer in self.
 *
 * Some over limitations are put to force the caller understand
 * what it's doing and what it wants:
 * - self must has been set a resgroup;
 */
static void
selfUnsetGroup(void)
{
	Assert(selfHasGroup());

	self->groupId = InvalidOid;
	self->group = NULL;
}

/*
 * Set both the slotId and the slot pointer in self.
 *
 * The passed slotId must be valid.
 *
 * Some over limitations are put to force the caller understand
 * what it's doing and what it wants:
 * - self must has been set a resgroup;
 * - self must has not been set a slot before set;
 */
static void
selfSetSlot(void)
{
	int			slotId = MyProc->resSlotId;

	Assert(selfHasGroup());
	Assert(!selfHasSlot());
	Assert(slotId != InvalidSlotId);

	MyProc->resSlotId = InvalidSlotId;

	self->slotId = slotId;
	self->slot = &self->group->slots[slotId];
}

/*
 * Unset both the slotId and the slot pointer in self.
 *
 * Some over limitations are put to force the caller understand
 * what it's doing and what it wants:
 * - self must has been set a resgroup;
 * - self must has been set a slot before unset;
 */
static void
selfUnsetSlot(void)
{
	Assert(selfHasGroup());
	Assert(selfHasSlot());

	self->slotId = InvalidSlotId;
	self->slot = NULL;
}

/*
 * Check whether proc is in the resgroup wait queue.
 */
static bool
procIsInWaitQueue(const PGPROC *proc)
{
	/* TODO: verify that proc is really in the queue in debug mode */

	return proc->links.next != INVALID_OFFSET;
}

#ifdef USE_ASSERT_CHECKING
/*
 * Check whether a resgroup is dropped.
 *
 * A dropped resgroup has groupId == InvalidOid,
 * however there is also the case that the resgroup is first dropped
 * then the shm struct is reused by another newly created resgroup,
 * in such a case the groupId is not InvalidOid but the original
 * resgroup does is dropped.
 *
 * So this function is not always reliable, use with caution.
 *
 * Consider use selfIsAssignedDroppedGroup() instead of this whenever possible.
 */
static bool
groupIsNotDropped(const ResGroupData *group)
{
	return group
		&& group->groupId != InvalidOid;
}
#endif//USE_ASSERT_CHECKING

/*
 * Validate the consistency of the resgroup wait queue.
 */
static void
groupWaitQueueValidate(const ResGroupData *group)
{
	const PROC_QUEUE	*waitQueue;

	Assert(groupSpinLockHeldByMe(group));

	waitQueue = &group->waitProcs;

	AssertImply(waitQueue->size == 0,
				waitQueue->links.next == MAKE_OFFSET(&waitQueue->links) &&
				waitQueue->links.prev == MAKE_OFFSET(&waitQueue->links));
}

/*
 * Push a proc to the resgroup wait queue.
 */
static void
groupWaitQueuePush(ResGroupData *group, PGPROC *proc)
{
	PROC_QUEUE			*waitQueue;
	PGPROC				*headProc;

	Assert(groupSpinLockHeldByMe(group));
	Assert(!procIsInWaitQueue(proc));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;
	headProc = (PGPROC *) &waitQueue->links;

	SHMQueueInsertBefore(&headProc->links, &proc->links);

	waitQueue->size++;
}

/*
 * Pop the top proc from the resgroup wait queue and return it.
 */
static PGPROC *
groupWaitQueuePop(ResGroupData *group)
{
	PROC_QUEUE			*waitQueue;
	PGPROC				*proc;

	Assert(groupSpinLockHeldByMe(group));
	Assert(!groupWaitQueueIsEmpty(group));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	proc = (PGPROC *) MAKE_PTR(waitQueue->links.next);
	Assert(procIsInWaitQueue(proc));

	SHMQueueDelete(&proc->links);
	Assert(!procIsInWaitQueue(proc));

	waitQueue->size--;

	return proc;
}

/*
 * Erase proc from the resgroup wait queue.
 */
static void
groupWaitQueueErase(ResGroupData *group, PGPROC *proc)
{
	PROC_QUEUE			*waitQueue;

	Assert(groupSpinLockHeldByMe(group));
	Assert(!groupWaitQueueIsEmpty(group));
	Assert(procIsInWaitQueue(proc));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	SHMQueueDelete(&proc->links);
	Assert(!procIsInWaitQueue(proc));

	waitQueue->size--;
}

/*
 * Check whether the resgroup wait queue is empty.
 */
static bool
groupWaitQueueIsEmpty(const ResGroupData *group)
{
	const PROC_QUEUE	*waitQueue;

	Assert(groupSpinLockHeldByMe(group));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	return waitQueue->size == 0;
}

static void
groupSpinLockInit(ResGroupData *group)
{
	Assert(group != NULL);

	SpinLockInit(&group->lock);

#ifdef USE_ASSERT_CHECKING
	group->lockOwnerPid = 0;
#endif//USE_ASSERT_CHECKING
}

static void
groupSpinLockAcquire(ResGroupData *group)
{
	Assert(group != NULL);
	Assert(!groupSpinLockHeldByMe(group));

	SpinLockAcquire(&group->lock);

#ifdef USE_ASSERT_CHECKING
	Assert(group->lockOwnerPid == 0);
	group->lockOwnerPid = MyProcPid;
#endif//USE_ASSERT_CHECKING
}

static void
groupSpinLockRelease(ResGroupData *group)
{
	Assert(group != NULL);
	Assert(groupSpinLockHeldByMe(group));

#ifdef USE_ASSERT_CHECKING
	group->lockOwnerPid = 0;
#endif//USE_ASSERT_CHECKING

	SpinLockRelease(&group->lock);
}

#ifdef USE_ASSERT_CHECKING
/*
 * This function should only be used in assert().
 */
static bool
groupSpinLockHeldByMe(const ResGroupData *group)
{
	Assert(group != NULL);
	return group->lockOwnerPid == MyProcPid;
}
#endif//USE_ASSERT_CHECKING
