/*-------------------------------------------------------------------------
 *
 * resgroup.c
 *	  GPDB resource group management code.
 *
 *
 * TERMS:
 *
 * - FIXED QUOTA: the minimal memory quota reserved for a slot. This quota
 *   is promised to be available during the lifecycle of the slot.
 *
 * - SHARED QUOTA: the preemptive memory quota shared by all the slots
 *   in a resource group. When a slot want to use more memory than its
 *   FIXED QUOTA it can attempt to allocate from this SHARED QUOTA, however
 *   this allocation is possible to fail depending on the actual usage.
 *
 * - MEM POOL: the global memory quota pool shared by all the resource groups.
 *   Overuse in this pool is strictly forbidden. A resource group must
 *   acquire from this pool to have enough memory quota for its slots'
 *   FIXED QUOTA and SHARED QUOTA, and should release overused quota to
 *   this pool as soon as possible.
 *
 * - SLOT POOL: the global slot pool shared by all the resource groups.
 *   A resource group must acquire a free slot in this pool for a new
 *   transaction to run in it.
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

#include "tcop/tcopprot.h"
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
#define InvalidSessionId	(0)
#define RESGROUP_MAX_SLOTS	(MaxConnections)

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

	ResGroupData		*group;
	ResGroupSlotData	*slot;

	ResGroupCaps	caps;

	int32	memUsage;			/* memory usage of current proc */
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
	Oid				groupId;

	ResGroupCaps	caps;

	int32			memQuota;	/* memory quota of current slot */
	int32			memUsage;	/* total memory usage of procs belongs to this slot */
	int				nProcs;		/* number of procs in this slot */

	ResGroupSlotData	*next;
};

/*
 * Resource group information.
 */
struct ResGroupData
{
	Oid			groupId;		/* Id for this group */
	ResGroupCaps	caps;		/* capabilities of this group */
	int			nRunning;		/* number of running trans */
	PROC_QUEUE	waitProcs;		/* list of PGPROC objects waiting on this group */
	int			totalExecuted;	/* total number of executed trans */
	int			totalQueued;	/* total number of queued trans	*/
	Interval	totalQueuedTime;/* total queue time */

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

	int32			chunkSizeInBits;

	ResGroupSlotData	*slots;		/* slot pool shared by all resource groups */
	ResGroupSlotData	*freeSlot;	/* head of the free list */

	int				nGroups;
	ResGroupData	groups[1];
};

/* hooks */
resgroup_assign_hook_type resgroup_assign_hook = NULL;

/* static variables */

static ResGroupControl *pResGroupControl = NULL;

static ResGroupProcData __self =
{
	InvalidOid,
};
static ResGroupProcData *self = &__self;

static bool localResWaiting = false;

/* static functions */

static bool groupApplyMemCaps(ResGroupData *group, const ResGroupCaps *caps);
static int32 mempoolReserve(Oid groupId, int32 chunks);
static void mempoolRelease(Oid groupId, int32 chunks);
static void groupRebalanceQuota(ResGroupData *group,
								int32 chunks,
								const ResGroupCaps *caps);
static void decideTotalChunks(int32 *totalChunks, int32 *chunkSizeInBits);
static int32 groupGetMemExpected(const ResGroupCaps *caps);
static int32 groupGetMemQuotaExpected(const ResGroupCaps *caps);
static int32 groupGetMemSharedExpected(const ResGroupCaps *caps);
static int32 groupGetMemSpillTotal(const ResGroupCaps *caps);
static int32 slotGetMemQuotaExpected(const ResGroupCaps *caps);
static int32 slotGetMemSpill(const ResGroupCaps *caps);
static void wakeupSlots(ResGroupData *group, bool grant);
static void wakeupGroups(Oid skipGroupId);
static bool mempoolAutoRelease(ResGroupData *group, ResGroupSlotData *slot);
static void mempoolAutoReserve(ResGroupData *group, const ResGroupCaps *caps);
static ResGroupData *groupHashNew(Oid groupId);
static ResGroupData *groupHashFind(Oid groupId, bool raise);
static void groupHashRemove(Oid groupId);
static void waitOnGroup(ResGroupData *group);
static ResGroupData *createGroup(Oid groupId, const ResGroupCaps *caps);
static void AtProcExit_ResGroup(int code, Datum arg);
static void groupWaitCancel(void);
static bool groupReserveMemQuota(ResGroupData *group);
static void groupReleaseMemQuota(ResGroupData *group, ResGroupSlotData *slot);
static int32 groupIncMemUsage(ResGroupData *group,
							  ResGroupSlotData *slot,
							  int32 chunks);
static void groupDecMemUsage(ResGroupData *group,
							 ResGroupSlotData *slot,
							 int32 chunks);
static void initSlot(ResGroupSlotData *slot, ResGroupCaps *caps,
					Oid groupId, int sessionId);
static void selfAttachToSlot(ResGroupData *group, ResGroupSlotData *slot);
static void selfDetachSlot(ResGroupData *group, ResGroupSlotData *slot);
static bool slotpoolInit(void);
static ResGroupSlotData *slotpoolAllocSlot(void);
static void slotpoolFreeSlot(ResGroupSlotData *slot);
static ResGroupSlotData *groupGetSlot(ResGroupData *group);
static void groupPutSlot(ResGroupData *group, ResGroupSlotData *slot);
static Oid decideResGroupId(void);
static ResGroupData *decideResGroup(void);
static ResGroupSlotData *groupAcquireSlot(ResGroupData *group);
static void groupReleaseSlot(ResGroupData *group, ResGroupSlotData *slot);
static void addTotalQueueDuration(ResGroupData *group);
static void groupSetMemorySpillRatio(const ResGroupCaps *caps);
static char* groupDumpMemUsage(ResGroupData *group);
static void selfValidateResGroupInfo(void);
static bool selfIsAssignedDroppedGroup(void);
static bool selfIsAssignedValidGroup(void);
static bool selfIsUnassigned(void);
static void selfUnassignDroppedGroup(void);
static bool selfHasGroup(void);
static void selfSetGroup(ResGroupData *group);
static void selfUnsetGroup(void);
static void selfSetSlot(ResGroupSlotData *slot);
static void selfUnsetSlot(void);
static bool procIsInWaitQueue(const PGPROC *proc);
static bool procIsWaiting(const PGPROC *proc);
static void procWakeup(PGPROC *proc);
static int slotGetId(const ResGroupSlotData *slot);
static void groupWaitQueueValidate(const ResGroupData *group);
static void groupWaitQueuePush(ResGroupData *group, PGPROC *proc);
static PGPROC * groupWaitQueuePop(ResGroupData *group);
static void groupWaitQueueErase(ResGroupData *group, PGPROC *proc);
static bool groupWaitQueueIsEmpty(const ResGroupData *group);
static bool shouldBypassQuery(const char* query_string);
static void lockResGroupForDrop(ResGroupData *group);
static void unlockResGroupForDrop(ResGroupData *group);

static void resgroupDumpGroup(StringInfo str, ResGroupData *group);
static void resgroupDumpWaitQueue(StringInfo str, PROC_QUEUE *queue);
static void resgroupDumpCaps(StringInfo str, ResGroupCap *caps);
static void resgroupDumpSlots(StringInfo str);
static void resgroupDumpFreeSlots(StringInfo str);

static void sessionSetSlot(ResGroupSlotData *slot);
static ResGroupSlotData *sessionGetSlot(void);
static void sessionResetSlot(void);

#ifdef USE_ASSERT_CHECKING
static bool selfIsAssigned(void);
static bool selfHasSlot(void);
static void slotValidate(const ResGroupSlotData *slot);
static bool slotIsInFreelist(const ResGroupSlotData *slot);
static bool slotIsInUse(const ResGroupSlotData *slot);
static bool groupIsNotDropped(const ResGroupData *group);
#endif /* USE_ASSERT_CHECKING */

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

	/* The slot pool. */
	size = add_size(size, mul_size(RESGROUP_MAX_SLOTS, sizeof(ResGroupSlotData)));

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

    LOG_RESGROUP_DEBUG(LOG, "creating hash table for %d resource groups", MaxResourceGroups);

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
	pResGroupControl->chunkSizeInBits = BITS_IN_MB;

	for (i = 0; i < MaxResourceGroups; i++)
		pResGroupControl->groups[i].groupId = InvalidOid;

	if (!slotpoolInit())
		goto error_out;

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
AllocResGroupEntry(Oid groupId, const ResGroupCaps *caps)
{
	ResGroupData	*group;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	group = createGroup(groupId, caps);
	Assert(group != NULL);

	LWLockRelease(ResGroupLock);
}

void
AtEOXact_ResGroup(bool isCommit)
{
	HandleResGroupDDLCallbacks(isCommit);

	/* Release resource group slot at the end of a transaction */
	if (ShouldUnassignResGroup())
		UnassignResGroup();
}

void
AtPrepare_ResGroup(void)
{
	/* Release resource group slot */
	if (ShouldUnassignResGroup())
		UnassignResGroup();
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

	on_shmem_exit(AtProcExit_ResGroup, 0);

	/*
	 * On master and segments, the first backend does the initialization.
	 */
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

	/* These initialization must be done before createGroup() */
	decideTotalChunks(&pResGroupControl->totalChunks, &pResGroupControl->chunkSizeInBits);
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

		GetResGroupCapabilities(relResGroupCapability, groupId, &caps);
		cpuRateLimit = caps.cpuRateLimit;

		group = createGroup(groupId, &caps);
		Assert(group != NULL);

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

	/*
	 * release lock here to guarantee we have no lock held when acquiring
	 * resource group slot
	 */
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

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	group = groupHashFind(groupId, true);

	if (group->nRunning > 0)
	{
		int nQuery = group->nRunning + group->waitProcs.size;

		Assert(name != NULL);
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("cannot drop resource group \"%s\"", name),
				 errhint(" The resource group is currently managing %d query(ies) and cannot be dropped.\n"
						 "\tTerminate the queries first or try dropping the group later.\n"
						 "\tThe view pg_stat_activity tracks the queries managed by resource groups.", nQuery)));
	}

	lockResGroupForDrop(group);

	LWLockRelease(ResGroupLock);
}

/*
 * Drop resource group call back function
 *
 * Wake up the backends in the wait queue when DROP RESOURCE GROUP finishes.
 * Unlock the resource group if the transaction is aborted.
 * Remove the resource group entry in shared memory if the transaction is committed.
 *
 * This function is called in the callback function of DROP RESOURCE GROUP.
 */
void
ResGroupDropFinish(Oid groupId, bool isCommit)
{
	ResGroupData	*group;
	volatile int	savedInterruptHoldoffCount;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	PG_TRY();
	{
		savedInterruptHoldoffCount = InterruptHoldoffCount;

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			group = groupHashFind(groupId, true);
			wakeupSlots(group, false);
			unlockResGroupForDrop(group);
		}

		if (isCommit)
		{
			groupHashRemove(groupId);
			ResGroupOps_DestroyGroup(groupId);
		}
	}
	PG_CATCH();
	{
		InterruptHoldoffCount = savedInterruptHoldoffCount;
		if (elog_demote(WARNING))
		{
			EmitErrorReport();
			FlushErrorState();
		}
		else
		{
			elog(LOG, "unable to demote error");
		}
	}
	PG_END_TRY();

	LWLockRelease(ResGroupLock);
}


/*
 * Remove the resource group entry in shared memory if the transaction is aborted.
 *
 * This function is called in the callback function of CREATE RESOURCE GROUP.
 */
void
ResGroupCreateOnAbort(Oid groupId)
{
	volatile int savedInterruptHoldoffCount;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	PG_TRY();
	{
		savedInterruptHoldoffCount = InterruptHoldoffCount;
		groupHashRemove(groupId);
		/* remove the os dependent part for this resource group */
		ResGroupOps_DestroyGroup(groupId);
	}
	PG_CATCH();
	{
		InterruptHoldoffCount = savedInterruptHoldoffCount;
		if (elog_demote(WARNING))
		{
			EmitErrorReport();
			FlushErrorState();
		}
		else
		{
			elog(LOG, "unable to demote error");
		}
	}
	PG_END_TRY();
	LWLockRelease(ResGroupLock);
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
	bool			shouldWakeUp;
	volatile int	savedInterruptHoldoffCount;

	Assert(caps != NULL);

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	PG_TRY();
	{
		savedInterruptHoldoffCount = InterruptHoldoffCount;
		group = groupHashFind(groupId, true);

		group->caps = *caps;

		if (limittype == RESGROUP_LIMIT_TYPE_CPU)
		{
			ResGroupOps_SetCpuRateLimit(groupId, caps->cpuRateLimit);
		}
		else if (limittype != RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO)
		{
			shouldWakeUp = groupApplyMemCaps(group, caps);

			wakeupSlots(group, true);
			if (shouldWakeUp)
				wakeupGroups(groupId);
		}
	}
	PG_CATCH();
	{
		InterruptHoldoffCount = savedInterruptHoldoffCount;
		if (elog_demote(WARNING))
		{
			EmitErrorReport();
			FlushErrorState();
		}
		else
		{
			elog(LOG, "unable to demote error");
		}
	}
	PG_END_TRY();

	LWLockRelease(ResGroupLock);
}

int32
ResGroupGetVmemLimitChunks(void)
{
	Assert(IsResGroupEnabled());

	return pResGroupControl->totalChunks;
}

int32
ResGroupGetVmemChunkSizeInBits(void)
{
	Assert(IsResGroupEnabled());

	return pResGroupControl->chunkSizeInBits;
}

int32
ResGroupGetMaxChunksPerQuery(void)
{
	return ceil(gp_vmem_limit_per_query /
				(1024.0 * (1 << (pResGroupControl->chunkSizeInBits - BITS_IN_MB))));
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

	LWLockAcquire(ResGroupLock, LW_SHARED);

	group = groupHashFind(groupId, true);

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
			result = CStringGetDatum(groupDumpMemUsage(group));
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("invalid stat type %d", type)));
	}

	LWLockRelease(ResGroupLock);

	return result;
}

static char*
groupDumpMemUsage(ResGroupData *group)
{
	int32 slotUsage;
	StringInfoData memUsage;

	if (Gp_role == GP_ROLE_DISPATCH)
		slotUsage = group->memQuotaUsed;
	else
		/* slotUsage has no meaning in QE */
		slotUsage = -1;	

	initStringInfo(&memUsage);

	appendStringInfo(&memUsage, "{");
	appendStringInfo(&memUsage, "\"used\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(group->memUsage));
	appendStringInfo(&memUsage, "\"available\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(
						group->memQuotaGranted + group->memSharedGranted - group->memUsage));
	appendStringInfo(&memUsage, "\"quota_used\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(slotUsage));
	appendStringInfo(&memUsage, "\"quota_available\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(
						group->memQuotaGranted - group->memQuotaUsed));
	appendStringInfo(&memUsage, "\"quota_granted\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(group->memQuotaGranted));
	appendStringInfo(&memUsage, "\"quota_proposed\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(
						groupGetMemQuotaExpected(&group->caps)));
	appendStringInfo(&memUsage, "\"shared_used\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(group->memSharedUsage));
	appendStringInfo(&memUsage, "\"shared_available\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(
						group->memSharedGranted - group->memSharedUsage));
	appendStringInfo(&memUsage, "\"shared_granted\":%d, ",
					 VmemTracker_ConvertVmemChunksToMB(group->memSharedGranted));
	appendStringInfo(&memUsage, "\"shared_proposed\":%d",
					 VmemTracker_ConvertVmemChunksToMB(
						groupGetMemSharedExpected(&group->caps)));
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
	int32				overuseMem;
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

	/* When doMemCheck is on, self must has been assigned to a resgroup. */
	Assert(selfIsAssigned());
	Assert(slotIsInUse(slot));

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

		Oid		groupGroupId = group->groupId;
		Oid		selfGroupId = self->groupId;

		selfUnassignDroppedGroup();
		self->doMemCheck = false;

		LOG_RESGROUP_DEBUG(LOG, "resource group is concurrently dropped while "
						   "reserving memory: dropped group=%d, my group=%d",
						   groupGroupId, selfGroupId);

		return true;
	}

	/* Otherwise we are in a valid resgroup, perform the memory limit check */
	Assert(selfIsAssignedValidGroup());
	Assert(group->memUsage >= 0);
	Assert(self->memUsage >= 0);

	/* add memoryChunks into group & slot memory usage */
	overuseMem = groupIncMemUsage(group, slot, memoryChunks);

	/* then check whether there is over usage */
	if (CritSectionCount == 0 && overuseMem > overuseChunks)
	{
		/* if the over usage is larger than allowed then revert the change */
		groupDecMemUsage(group, slot, memoryChunks);

		/* also revert in proc */
		self->memUsage -= memoryChunks;
		Assert(self->memUsage >= 0);

		if (overuseChunks == 0)
			ResGroupDumpMemoryInfo();

		return false;
	}
	else if (CritSectionCount == 0 && overuseMem > 0)
	{
		/* the over usage is within the allowed threshold */
		*waiverUsed = true;
	}

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

	Assert(selfIsAssigned());
	Assert(slotIsInUse(slot));

	if (selfIsAssignedDroppedGroup())
	{
		Oid		groupGroupId = group->groupId;
		Oid		selfGroupId = self->groupId;

		selfUnassignDroppedGroup();
		self->doMemCheck = false;

		LOG_RESGROUP_DEBUG(LOG, "resource group is concurrently dropped while "
						   "releasing memory: dropped group=%d, my group=%d",
						   groupGroupId, selfGroupId);

		return;
	}

	Assert(selfIsAssignedValidGroup());

	groupDecMemUsage(group, slot, memoryChunks);
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
 * createGroup -- initialize the elements for a resource group.
 *
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this - unless we are the startup process.
 */
static ResGroupData *
createGroup(Oid groupId, const ResGroupCaps *caps)
{
	ResGroupData	*group;
	int32			chunks;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(OidIsValid(groupId));

	group = groupHashNew(groupId);
	Assert(group != NULL);

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

	group->memQuotaGranted = 0;
	group->memSharedGranted = 0;
	group->memExpected = groupGetMemExpected(caps);

	chunks = mempoolReserve(groupId, group->memExpected);
	groupRebalanceQuota(group, chunks, caps);

	return group;
}

/*
 * Add chunks into group and slot memory usage.
 *
 * Return the over used chunks.
 */
static int32
groupIncMemUsage(ResGroupData *group, ResGroupSlotData *slot, int32 chunks)
{
	int32			slotMemUsage;
	int32			sharedMemUsage;
	int32			overuseMem = 0;

	/* Add the chunks to memUsage in slot */
	slotMemUsage = pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &slot->memUsage,
										   chunks);

	/* Check whether shared memory should be added */
	sharedMemUsage = slotMemUsage - slot->memQuota;
	if (sharedMemUsage > 0)
	{
		int32			total;

		/* Decide how many chunks should be counted as shared memory */
		sharedMemUsage = Min(sharedMemUsage, chunks);

		/* Add these chunks to memSharedUsage in group */
		total = pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &group->memSharedUsage,
										sharedMemUsage);

		/* Calculate the over used chunks */
		overuseMem = Max(0, total - group->memSharedGranted);
	}

	/* Add the chunks to memUsage in group */
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &group->memUsage,
							chunks);

	return overuseMem;
}

/*
 * Sub chunks from group and slot memory usage.
 */
static void
groupDecMemUsage(ResGroupData *group, ResGroupSlotData *slot, int32 chunks)
{
	int32			value;
	int32			slotMemUsage;
	int32			sharedMemUsage;

	/* Sub chunks from memUsage in group */
	value = pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) &group->memUsage,
									chunks);
	Assert(value >= 0);

	/* Sub chunks from memUsage in slot */
	slotMemUsage = pg_atomic_fetch_sub_u32((pg_atomic_uint32 *) &slot->memUsage,
										   chunks);
	Assert(slotMemUsage >= chunks);

	/* Check whether shared memory should be subed */
	sharedMemUsage = slotMemUsage - slot->memQuota;
	if (sharedMemUsage > 0)
	{
		/* Decide how many chunks should be counted as shared memory */
		sharedMemUsage = Min(sharedMemUsage, chunks);

		/* Sub chunks from memSharedUsage in group */
		value = pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) &group->memSharedUsage,
										sharedMemUsage);
		Assert(value >= 0);
	}
}

/*
 * Attach a process (QD or QE) to a slot.
 */
static void
selfAttachToSlot(ResGroupData *group, ResGroupSlotData *slot)
{
	selfSetSlot(slot);
	AssertImply(slot->nProcs == 0, slot->memUsage == 0);
	groupIncMemUsage(group, slot, self->memUsage);
	pg_atomic_add_fetch_u32((pg_atomic_uint32*) &slot->nProcs, 1);
}

/*
 * Detach a process (QD or QE) from a slot.
 */
static void
selfDetachSlot(ResGroupData *group, ResGroupSlotData *slot)
{
	groupDecMemUsage(group, slot, self->memUsage);
	pg_atomic_sub_fetch_u32((pg_atomic_uint32*) &slot->nProcs, 1);
	AssertImply(slot->nProcs == 0, slot->memUsage == 0);
	selfUnsetSlot();
}

/*
 * Initialize the members of a slot
 */
static void
initSlot(ResGroupSlotData *slot, ResGroupCaps *caps, Oid groupId, int sessionId)
{
	Assert(!slotIsInUse(slot));
	Assert(caps != NULL);
	Assert(groupId != InvalidOid);
	Assert(sessionId != InvalidSessionId);

	/* TODO: check for LWLock */

	slot->groupId = groupId;
	slot->sessionId = sessionId;
	slot->caps = *caps;
	slot->memQuota = slotGetMemQuotaExpected(caps);
	slot->memUsage = 0;
}

/*
 * Alloc and initialize slot pool
 */
static bool
slotpoolInit(void)
{
	ResGroupSlotData *slot;
	ResGroupSlotData *next;
	int numSlots;
	int memSize;
	int i;

	numSlots = RESGROUP_MAX_SLOTS;
	memSize = mul_size(numSlots, sizeof(ResGroupSlotData));

	pResGroupControl->slots = ShmemAlloc(memSize);
	if (!pResGroupControl->slots)
		return false;

	MemSet(pResGroupControl->slots, 0, memSize);

	/* push all the slots into the list */
	next = NULL;
	for (i = numSlots - 1; i >= 0; i--)
	{
		slot = &pResGroupControl->slots[i];

		slot->sessionId = InvalidSessionId;
		slot->groupId = InvalidOid;
		slot->memQuota = -1;
		slot->memUsage = 0;

		slot->next = next;
		next = slot;
	}
	pResGroupControl->freeSlot = next;

	return true;
}

/*
 * Alloc a slot from shared slot pool
 */
static ResGroupSlotData *
slotpoolAllocSlot(void)
{
	ResGroupSlotData *slot;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(pResGroupControl->freeSlot != NULL);

	slot = pResGroupControl->freeSlot;
	pResGroupControl->freeSlot = slot->next;
	slot->next = NULL;

	return slot;
}

/*
 * Free a slot back to shared slot pool
 */
static void
slotpoolFreeSlot(ResGroupSlotData *slot)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(slotIsInUse(slot));
	Assert(slot->nProcs == 0);

	slot->sessionId = InvalidSessionId;
	slot->groupId = InvalidOid;
	slot->memQuota = -1;
	slot->memUsage = 0;

	slot->next = pResGroupControl->freeSlot;
	pResGroupControl->freeSlot = slot;
}

/*
 * Get a slot with memory quota granted.
 *
 * A slot can be got with this function if there is enough memory quota
 * available and the concurrency limit is not reached.
 *
 * On success the memory quota is marked as granted, nRunning is increased
 * and the slot's groupId is also set accordingly, the slot id is returned.
 *
 * On failure nothing is changed and InvalidSlotId is returned.
 */
static ResGroupSlotData *
groupGetSlot(ResGroupData *group)
{
	ResGroupSlotData	*slot;
	ResGroupCaps		*caps;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(groupIsNotDropped(group));

	caps = &group->caps;

	/* First check if the concurrency limit is reached */
	if (group->nRunning >= caps->concurrency)
		return NULL;

	if (!groupReserveMemQuota(group))
		return NULL;

	/* Now actually get a free slot */
	slot = slotpoolAllocSlot();
	Assert(!slotIsInUse(slot));

	group->nRunning++;

	return slot;
}

/*
 * Put back the slot assigned to self.
 *
 * This will release a slot, its memory quota will be freed and
 * nRunning will be decreased.
 */
static void
groupPutSlot(ResGroupData *group, ResGroupSlotData *slot)
{
	bool				shouldWakeUp;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(group->memQuotaUsed >= 0);
	Assert(slotIsInUse(slot));

	/* Return the memory quota granted to this slot */
	groupReleaseMemQuota(group, slot);

	shouldWakeUp = mempoolAutoRelease(group, slot);
	if (shouldWakeUp)
		wakeupGroups(group->groupId);

	/* Return the slot back to free list */
	slotpoolFreeSlot(slot);
	group->nRunning--;
}

/*
 * Reserve memory quota for a slot in group.
 *
 * If there is not enough free memory quota then return false and nothing
 * is changed; otherwise return true and the quota is reserved.
 */
static bool
groupReserveMemQuota(ResGroupData *group)
{
	ResGroupCaps	*caps;
	int32			slotMemQuota;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(pResGroupControl->segmentsOnMaster > 0);

	caps = &group->caps;
	mempoolAutoReserve(group, caps);

	/* Calculate the expected per slot quota */
	slotMemQuota = slotGetMemQuotaExpected(caps);
	Assert(slotMemQuota >= 0);

	Assert(group->memQuotaUsed >= 0);
	Assert(group->memQuotaUsed <= group->memQuotaGranted);

	if (group->memQuotaUsed + slotMemQuota > group->memQuotaGranted)
	{
		/* No enough memory quota available, give up */
		return false;
	}

	group->memQuotaUsed += slotMemQuota;

	return true;
}

/*
 * Release a slot's memory quota to group.
 */
static void
groupReleaseMemQuota(ResGroupData *group, ResGroupSlotData *slot)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	group->memQuotaUsed -= slot->memQuota;
	Assert(group->memQuotaUsed >= 0);
}

/*
 * Pick a resource group for the current transaction.
 */
static Oid
decideResGroupId(void)
{
	Oid groupId = InvalidOid;

	if (resgroup_assign_hook)
		groupId = resgroup_assign_hook();

	if (groupId == InvalidOid)
		groupId = GetResGroupIdForRole(GetUserId());

	return groupId;
}

/*
 * Decide the proper resource group for current role.
 *
 * An exception is thrown if current role is invalid.
 */
static ResGroupData *
decideResGroup(void)
{
	ResGroupData	*group;
	Oid				 groupId;

	Assert(pResGroupControl != NULL);
	Assert(pResGroupControl->segmentsOnMaster > 0);
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(!selfIsAssigned());

	/* always find out the up-to-date resgroup id */
	groupId = decideResGroupId();

	LWLockAcquire(ResGroupLock, LW_SHARED);
	group = groupHashFind(groupId, false);

	if (!group)
	{
		groupId = superuser() ? ADMINRESGROUP_OID : DEFAULTRESGROUP_OID;
		group = groupHashFind(groupId, false);
	}

	Assert(group != NULL);

	selfSetGroup(group);
	LWLockRelease(ResGroupLock);
	return group;
}

/*
 * Acquire a resource group slot
 *
 * Call this function at the start of the transaction.
 * This function set current resource group in MyResGroupSharedInfo,
 * and current slot in MyProc->resSlot.
 */
static ResGroupSlotData *
groupAcquireSlot(ResGroupData *group)
{
	ResGroupSlotData *slot;

	/* should not been granted a slot yet */
	Assert(selfIsAssigned());
	Assert(!selfHasSlot());

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	if (selfIsAssignedDroppedGroup())
	{
		LWLockRelease(ResGroupLock);
		return NULL;
	}

	/* acquire a slot */
	if (!group->lockedForDrop)
	{
		/* try to get a slot directly */
		slot = groupGetSlot(group);

		if (slot != NULL)
		{
			/* got one, lucky */
			initSlot(slot, &group->caps, group->groupId, gp_session_id);
			group->totalExecuted++;
			LWLockRelease(ResGroupLock);
			pgstat_report_resgroup(0, group->groupId);
			return slot;
		}
	}

	/* add into group wait queue */
	groupWaitQueuePush(group, MyProc);

	if (!group->lockedForDrop)
		group->totalQueued++;
	LWLockRelease(ResGroupLock);

	/*
	 * wait on the queue
	 * slot will be assigned by the proc wakes me up
	 * if i am waken up by DROP RESOURCE GROUP statement, the
	 * resSlot will be NULL.
	 */
	waitOnGroup(group);

	if (MyProc->resSlot == NULL)
		return NULL;

	/*
	 * The waking process has granted us a valid slot.
	 * Update the statistic information of the resource group.
	 */
	slot = (ResGroupSlotData *) MyProc->resSlot;
	MyProc->resSlot = NULL;
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	addTotalQueueDuration(group);
	group->totalExecuted++;
	LWLockRelease(ResGroupLock);

	pgstat_report_resgroup(0, group->groupId);
	return slot;
}

/* Update the total queued time of this group */
/*
 * Wake up the backends in the wait queue when 'concurrency' is increased.
 * This function is called in the callback function of ALTER RESOURCE GROUP.
 *
 * Return TRUE if any memory quota or shared quota is returned to MEM POOL.
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
 * memChunksInuse is 0.5*0.4/10*6 = 0.12
 * memChunksFree is 0.3 - 0.12 = 0.18
 *
 * * memLimit: 0.5 -> 0.4
 *   for memQuotaGranted we could free 0.18 - 0.4*0.6/10*6 = 0.18-0.144 = 0.036
 *       new memQuotaGranted is 0.3-0.036 = 0.264
 *       new memChunksFree is 0.18-0.036 = 0.144
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
 *       new memChunksFree is 0.144 - 0.016 = 0.128
 *   for memShared we could free currentSharedUsage - Max(currentSharedUsage, 0.4*0.6) = -0.18
 *
 * * memLimit: 0.4 -> 0.2
 *   for memQuotaGranted we could free 0.128 - 0.2*0.4/20*16 = 0.128 - 0.064 = 0.064
 *       new memQuotaGranted is 0.248-0.064 = 0.184
 *       new memChunksFree is 0.128 - 0.064 = 0.064
 *   for memShared we could free currentSharedStocks - Max(currentSharedUsage, 0.2*0.6) = -0.04
 */
static bool
groupApplyMemCaps(ResGroupData *group, const ResGroupCaps *caps)
{
	int32 memQuotaAvailable;
	int32 memQuotaNeeded;
	int32 memQuotaToFree;
	int32 memSharedNeeded;
	int32 memSharedToFree;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	group->memExpected = groupGetMemExpected(caps);

	/* memQuotaAvailable is the total free non-shared quota */
	memQuotaAvailable = group->memQuotaGranted - group->memQuotaUsed;

	if (caps->concurrency > group->nRunning)
	{
		/*
		 * memQuotaNeeded is the total non-shared quota needed
		 * by all the free slots
		 */
		memQuotaNeeded = slotGetMemQuotaExpected(caps) *
			(caps->concurrency - group->nRunning);

		/*
		 * if memQuotaToFree > 0 then we can safely release these
		 * non-shared quota and still have enough quota to run
		 * all the free slots.
		 */
		memQuotaToFree = memQuotaAvailable - memQuotaNeeded;
	}
	else
	{
		memQuotaToFree = Min(memQuotaAvailable,
							  group->memQuotaGranted - groupGetMemQuotaExpected(caps));
	}

	/* TODO: optimize the free logic */
	if (memQuotaToFree > 0)
	{
		mempoolRelease(group->groupId, memQuotaToFree);
		group->memQuotaGranted -= memQuotaToFree;
	}

	memSharedNeeded = Max(group->memSharedUsage,
						  groupGetMemSharedExpected(caps));
	memSharedToFree = group->memSharedGranted - memSharedNeeded;

	if (memSharedToFree > 0)
	{
		mempoolRelease(group->groupId, memSharedToFree);
		group->memSharedGranted -= memSharedToFree;
	}
#if 1
	/*
	 * FIXME: why we need this?
	 *
	 * suppose rg1 has memory_limit=10, memory_shared_quota=40,
	 * and session1 is running in rg1.
	 *
	 * now we alter rg1 memory_limit to 40 in another session,
	 * apparently both memory quota and shared quota are expected to increase,
	 * however as our design is to let them increase on new queries,
	 * then for session1 it won't see memory shared quota being increased
	 * until new queries being executed in rg1.
	 *
	 * so we should try to acquire the new quota immediately.
	 */
	mempoolAutoReserve(group, caps);
#endif
	return (memQuotaToFree > 0 || memSharedToFree > 0);
}

/*
 * Get quota from MEM POOL.
 *
 * chunks is the expected amount to get.
 *
 * return the actual got chunks, might be smaller than expectation.
 */
static int32
mempoolReserve(Oid groupId, int32 chunks)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	LOG_RESGROUP_DEBUG(LOG, "allocate %u out of %u chunks to group %d",
					   chunks, pResGroupControl->freeChunks, groupId);

	chunks = Min(pResGroupControl->freeChunks, chunks);
	pResGroupControl->freeChunks -= chunks;

	Assert(pResGroupControl->freeChunks >= 0);
	Assert(pResGroupControl->freeChunks <= pResGroupControl->totalChunks);

	return chunks;
}

/*
 * Return chunks to MEM POOL.
 */
static void
mempoolRelease(Oid groupId, int32 chunks)
{
	LOG_RESGROUP_DEBUG(LOG, "free %u to pool(%u) chunks from group %d",
					   chunks, pResGroupControl->freeChunks, groupId);

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(chunks > 0);

	pResGroupControl->freeChunks += chunks;

	Assert(pResGroupControl->freeChunks >= 0);
	Assert(pResGroupControl->freeChunks <= pResGroupControl->totalChunks);
}

/*
 * Assign the chunks we get from the MEM POOL to group and rebalance
 * them into the 'quota' and 'shared' part of the group, the amount
 * is calculated from caps.
 */
static void
groupRebalanceQuota(ResGroupData *group, int32 chunks, const ResGroupCaps *caps)
{
	int32 delta;
	int32 memQuotaGranted = groupGetMemQuotaExpected(caps);

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

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
static void
decideTotalChunks(int32 *totalChunks, int32 *chunkSizeInBits)
{
	int32 nsegments;
	int32 tmptotalChunks;
	int32 tmpchunkSizeInBits;

	nsegments = Gp_role == GP_ROLE_EXECUTE ? host_segments : pResGroupControl->segmentsOnMaster;
	Assert(nsegments > 0);

	tmptotalChunks = ResGroupOps_GetTotalMemory() * gp_resource_group_memory_limit / nsegments;

	/*
	 * If vmem is larger than 16GB (i.e., 16K MB), we make the chunks bigger
	 * so that the vmem limit in chunks unit is not larger than 16K.
	 */
	tmpchunkSizeInBits = BITS_IN_MB;
	while(tmptotalChunks > (16 * 1024))
	{
		tmpchunkSizeInBits++;
		tmptotalChunks >>= 1;
	}

	*totalChunks = tmptotalChunks;
	*chunkSizeInBits = tmpchunkSizeInBits;
}

/*
 * Get total expected memory quota of a group in chunks
 */
static int32
groupGetMemExpected(const ResGroupCaps *caps)
{
	Assert(pResGroupControl->totalChunks > 0);
	return pResGroupControl->totalChunks * caps->memLimit / 100;
}

/*
 * Get per-group expected memory quota in chunks
 */
static int32
groupGetMemQuotaExpected(const ResGroupCaps *caps)
{
	if (caps->concurrency > 0)
		return slotGetMemQuotaExpected(caps) * caps->concurrency;
	else
		return groupGetMemExpected(caps) *
			(100 - caps->memSharedQuota) / 100;
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
	Assert(caps->concurrency != 0);
	return groupGetMemExpected(caps) *
		(100 - caps->memSharedQuota) / 100 /
		caps->concurrency;
}

/*
 * Get per-slot expected memory spill in chunks
 */
static int32
slotGetMemSpill(const ResGroupCaps *caps)
{
	Assert(caps->concurrency != 0);
	return groupGetMemSpillTotal(caps) / caps->concurrency;
}

/*
 * Attempt to wake up pending slots in the group.
 *
 * - grant indicates whether to grant the proc a slot;
 * - release indicates whether to wake up the proc with the LWLock
 *   temporarily released;
 *
 * When grant is true we'll give up once no slot can be get,
 * e.g. due to lack of free slot or enough memory quota.
 *
 * When grant is false all the pending procs will be woken up.
 */
static void
wakeupSlots(ResGroupData *group, bool grant)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	while (!groupWaitQueueIsEmpty(group))
	{
		PGPROC		*waitProc;
		ResGroupSlotData *slot = NULL;

		if (grant)
		{
			/* try to get a slot for that proc */
			slot = groupGetSlot(group);
			if (slot == NULL)
				/* if can't get one then give up */
				break;
		}

		/* wake up one process in the wait queue */
		waitProc = groupWaitQueuePop(group);

		if (slot != NULL)
			initSlot(slot, &group->caps,
					 group->groupId, waitProc->mppSessionId);

		waitProc->resSlot = slot;

		procWakeup(waitProc);
	}
}

/*
 * When a group returns chunks to MEM POOL, we need to wake up
 * the transactions waiting on other groups for memory quota.
 */
static void
wakeupGroups(Oid skipGroupId)
{
	int				i;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	for (i = 0; i < MaxResourceGroups; i++)
	{
		ResGroupData	*group = &pResGroupControl->groups[i];
		int32			delta;

		if (group->groupId == InvalidOid)
			continue;

		if (group->groupId == skipGroupId)
			continue;

		if (group->lockedForDrop)
			continue;

		if (groupWaitQueueIsEmpty(group))
			continue;

		delta = group->memExpected - group->memQuotaGranted - group->memSharedGranted;
		if (delta <= 0)
			continue;

		wakeupSlots(group, true);

		if (!pResGroupControl->freeChunks)
			break;
	}
}

/*
 * Release overused memory quota to MEM POOL when a slot gets freed and
 * the caps has been changed, the released memory quota includes:
 * - the slot overused quota
 * - the group overused shared quota
 */
static bool 
mempoolAutoRelease(ResGroupData *group, ResGroupSlotData *slot)
{
	int32		memQuotaNeedFree;
	int32		memSharedNeeded;
	int32		memQuotaToFree;
	int32		memSharedToFree;
	int32       memQuotaExpected;
	ResGroupCaps *caps = &group->caps;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	/* Return the over used memory quota to sys */
	memQuotaNeedFree = group->memQuotaGranted - groupGetMemQuotaExpected(caps);
	memQuotaToFree = memQuotaNeedFree > 0 ? Min(memQuotaNeedFree, slot->memQuota) : 0;

	if (caps->concurrency > 0)
	{
		/*
		 * Under this situation, when this slot is released,
		 * others will not be blocked by concurrency limit if
		 * they come to acquire this slot. So we could decide
		 * not to give all the memory to MEM POOL even if we could.
		 */
		memQuotaExpected = slotGetMemQuotaExpected(caps);
		if (memQuotaToFree > memQuotaExpected)
			memQuotaToFree -= memQuotaExpected;
	}

	if (memQuotaToFree > 0)
	{
		mempoolRelease(group->groupId, memQuotaToFree); 
		group->memQuotaGranted -= memQuotaToFree; 
	}

	/* Return the over used shared quota to sys */
	memSharedNeeded = Max(group->memSharedUsage,
						  groupGetMemSharedExpected(caps));
	memSharedToFree = group->memSharedGranted - memSharedNeeded;
	if (memSharedToFree > 0)
	{
		mempoolRelease(group->groupId, memSharedToFree);
		group->memSharedGranted -= memSharedToFree;
	}
	return (memQuotaToFree > 0 || memSharedToFree > 0);
}

/*
 * Try to acquire enough quota & shared quota for current group from MEM POOL,
 * the actual acquired quota depends on system loads.
 */
static void
mempoolAutoReserve(ResGroupData *group, const ResGroupCaps *caps)
{
	int32 currentMemStocks = group->memSharedGranted + group->memQuotaGranted;
	int32 neededMemStocks = group->memExpected - currentMemStocks;

	if (neededMemStocks > 0)
	{
		int32 chunks = mempoolReserve(group->groupId, neededMemStocks);
		groupRebalanceQuota(group, chunks, caps);
	}
}

static void
addTotalQueueDuration(ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	if (group == NULL)
		return;

	TimestampTz start = pgstat_fetch_resgroup_queue_timestamp();
	TimestampTz now = GetCurrentTimestamp();
	Datum durationDatum = DirectFunctionCall2(timestamptz_age,
											  TimestampTzGetDatum(now),
											  TimestampTzGetDatum(start));
	Datum sumDatum = DirectFunctionCall2(interval_pl,
										 IntervalPGetDatum(&group->totalQueuedTime),
										 durationDatum);
	memcpy(&group->totalQueuedTime, DatumGetIntervalP(sumDatum), sizeof(Interval));
}

/*
 * Release the resource group slot
 *
 * Call this function at the end of the transaction.
 */
static void
groupReleaseSlot(ResGroupData *group, ResGroupSlotData *slot)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(!selfHasSlot());

	groupPutSlot(group, slot);

	/*
	 * My slot is put back, then how many queuing queries should I wake up?
	 * Maybe zero, maybe one, maybe more, depends on how the resgroup's
	 * configuration were changed during our execution.
	 */
	Assert(!slotIsInUse(slot));
	wakeupSlots(group, true);
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

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		tmp = htonl(caps[i]);
		appendBinaryStringInfo(str, (char *) &tmp, sizeof(caps[i]));
	}
}

/*
 * Deserialize the resource group information dispatched by QD.
 */
void
DeserializeResGroupInfo(struct ResGroupCaps *capsOut,
						Oid *groupId,
						const char *buf,
						int len)
{
	int			i;
	int			tmp;
	const char	*ptr = buf;
	ResGroupCap *caps = (ResGroupCap *) capsOut;

	Assert(len > 0);

	memcpy(&tmp, ptr, sizeof(*groupId));
	*groupId = ntohl(tmp);
	ptr += sizeof(*groupId);

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		memcpy(&tmp, ptr, sizeof(caps[i]));
		caps[i] = ntohl(tmp);
		ptr += sizeof(caps[i]);
	}

	Assert(len == ptr - buf);
}

/*
 * Check whether resource group should be assigned on master.
 *
 * Resource group will not be assigned if we are in SIGUSR1 handler.
 * This is to avoid the deadlock situation cased by the following scenario:
 *
 * Suppose backend A starts a transaction and acquires the LAST slot in resource
 * group G. Then backend A signals other backends who need a catchup interrupt.
 * Suppose backend B receives the signal and wants to respond to catchup event.
 * If backend B is assigned the same resource group G and tries to acquire a slot,
 * it will hang. Backend A will also hang because it is waiting for backend B to
 * catch up and free its space in the global SI message queue.
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
 * Check whether resource group should be un-assigned.
 * This will be called on both master and segments.
 */
bool
ShouldUnassignResGroup(void)
{
	return IsResGroupActivated() &&
		IsNormalProcessingMode() &&
		(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE) &&
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

	/*
	 * if query should be bypassed, do not assign a
	 * resource group, leave self unassigned
	 */
	if (shouldBypassQuery(debug_query_string))
		return;

	PG_TRY();
	{
retry:
		group = decideResGroup();

		/* Acquire slot */
		slot = groupAcquireSlot(group);
		if (slot == NULL)
		{
			selfUnsetGroup();
			goto retry;
		}

		Assert(!self->doMemCheck);

		/* Set resource group slot for current session */
		sessionSetSlot(slot);

		/* Add proc memory accounting info into group and slot */
		selfAttachToSlot(group, slot);

		/* Init self */
		self->caps = slot->caps;

		/* Start memory limit checking */
		self->doMemCheck = true;

		/* Don't error out before this line in this function */
		SIMPLE_FAULT_INJECTOR(ResGroupAssignedOnMaster);

		/* Add into cgroup */
		ResGroupOps_AssignGroup(self->groupId, MyProcPid);

		/* Set spill guc */
		groupSetMemorySpillRatio(&slot->caps);
	}
	PG_CATCH();
	{
		UnassignResGroup();
		PG_RE_THROW();
	}
	PG_END_TRY();
}

/*
 * Detach from a resource group at the end of the transaction.
 */
void
UnassignResGroup(void)
{
	ResGroupData		*group = self->group;
	ResGroupSlotData	*slot = self->slot;

	if (selfIsUnassigned())
	{
		Assert(self->doMemCheck == false);
		return;
	}

	Assert(selfIsAssignedValidGroup());
	Assert(selfHasSlot());

	/* Stop memory limit checking */
	self->doMemCheck = false;

	/* Cleanup self */
	if (self->memUsage > 10)
		LOG_RESGROUP_DEBUG(LOG, "idle proc memory usage: %d", self->memUsage);

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	/* Sub proc memory accounting info from group and slot */
	selfDetachSlot(group, slot);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* Release the slot */
		groupReleaseSlot(group, slot);
		Assert(!selfHasSlot());

		sessionResetSlot();
	}
	else if (slot->nProcs == 0)
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);

		/* Release the slot memory */
		mempoolAutoRelease(group, slot);

		/* Release this slot back to slot pool */
		slotpoolFreeSlot(slot);

		/* Reset resource group slot for current session */
		sessionResetSlot();

		/* And finally decrease nRunning */
		group->nRunning--;
	}

	/* Cleanup group */
	selfUnsetGroup();
	LWLockRelease(ResGroupLock);

	pgstat_report_resgroup(0, InvalidOid);

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
	Oid		newGroupId;
	ResGroupCaps		caps;
	ResGroupData		*group;
	ResGroupSlotData	*slot;

	/* Stop memory limit checking */
	self->doMemCheck = false;

	DeserializeResGroupInfo(&caps, &newGroupId, buf, len);

	if (newGroupId == InvalidOid)
	{
		UnassignResGroup();
		Assert(selfIsUnassigned());
		return;
	}

	if (self->groupId != InvalidOid)
	{
		/* it's not the first dispatch in the same transaction */
		Assert(self->groupId == newGroupId);
		Assert(!memcmp((void*)&self->caps, (void*)&caps, sizeof(caps)));
		self->doMemCheck = true;
		return;
	}

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	group = groupHashFind(newGroupId, true);
	Assert(group != NULL);

	/* Init self */
	Assert(host_segments > 0);
	Assert(caps.concurrency > 0);
	selfSetGroup(group);
	self->caps = caps;

	/* Init slot */
	slot = sessionGetSlot();
	if (slot != NULL)
	{
		Assert(slotIsInUse(slot));
		Assert(slot->sessionId == gp_session_id);
		Assert(slot->groupId == newGroupId);
		Assert(slot->memQuota == slotGetMemQuotaExpected(&caps));
	}
	else
	{
		/* This is the first QE of this session, allocate a slot from slot pool */
		slot = slotpoolAllocSlot();
		Assert(!slotIsInUse(slot));
		sessionSetSlot(slot);
		initSlot(slot, &caps, newGroupId, gp_session_id);
		group->nRunning++;
	}

	selfAttachToSlot(group, slot);
	Assert(selfHasSlot());

	mempoolAutoReserve(group, &caps);
	LWLockRelease(ResGroupLock);

	/* finally we can say we are in a valid resgroup */
	Assert(selfIsAssignedValidGroup());

	/* Start memory limit checking */
	self->doMemCheck = true;

	/* Add into cgroup */
	ResGroupOps_AssignGroup(self->groupId, MyProcPid);
}

/*
 * Wait on the queue of resource group
 */
static void
waitOnGroup(ResGroupData *group)
{
	PGPROC *proc = MyProc;

	Assert(!LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(selfHasGroup());
	Assert(!selfHasSlot());

	pgstat_report_resgroup(GetCurrentTimestamp(), group->groupId);

	/*
	 * Mark that we are waiting on resource group
	 *
	 * This is used for interrupt cleanup, similar to lockAwaited in ProcSleep
	 */
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

			if (!procIsWaiting(proc))
				break;
			WaitLatch(&proc->procLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH, -1);
		}
	}
	PG_CATCH();
	{
		groupWaitCancel();
		PG_RE_THROW();
	}
	PG_END_TRY();

	localResWaiting = false;

	pgstat_report_waiting(PGBE_WAITING_NONE);
}

/*
 * groupHashNew -- return a new (empty) group object to initialize.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static ResGroupData *
groupHashNew(Oid groupId)
{
	int			i;
	bool		found;
	ResGroupHashEntry *entry;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(groupId != InvalidOid);

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
 * groupHashFind -- return the group for a given oid.
 *
 * If the group cannot be found, then NULL is returned if 'raise' is false,
 * otherwise an exception is thrown.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static ResGroupData *
groupHashFind(Oid groupId, bool raise)
{
	bool				found;
	ResGroupHashEntry	*entry;

	Assert(LWLockHeldByMe(ResGroupLock));

	entry = (ResGroupHashEntry *)
		hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_FIND, &found);

	if (!found)
	{
		ereport(raise ? ERROR : LOG,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("cannot find resource group with Oid %d in shared memory",
						groupId)));
		return NULL;
	}

	Assert(entry->index < pResGroupControl->nGroups);
	return &pResGroupControl->groups[entry->index];
}


/*
 * groupHashRemove -- remove the group for a given oid.
 *
 * If the group cannot be found then an exception is thrown.
 *
 * Notes
 *	The resource group lightweight lock (ResGroupLock) *must* be held for
 *	this operation.
 */
static void
groupHashRemove(Oid groupId)
{
	bool		found;
	ResGroupHashEntry	*entry;
	ResGroupData		*group;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	entry = (ResGroupHashEntry*)hash_search(pResGroupControl->htbl,
											(void *) &groupId,
											HASH_REMOVE,
											&found);
	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("cannot find resource group with Oid %d in shared memory to remove",
						groupId)));

	group = &pResGroupControl->groups[entry->index];
	mempoolRelease(groupId, group->memQuotaGranted + group->memSharedGranted);
	group->memQuotaGranted = 0;
	group->memSharedGranted = 0;
	group->groupId = InvalidOid;

	wakeupGroups(groupId);
}

/* Process exit without waiting for slot or received SIGTERM */
static void
AtProcExit_ResGroup(int code, Datum arg)
{
	groupWaitCancel();
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
groupWaitCancel(void)
{
	ResGroupData		*group;
	ResGroupSlotData	*slot;

	/* Process exit without waiting for slot */
	if (!selfHasGroup() || !localResWaiting)
		return;

	/* We are sure to be interrupted in the for loop of waitOnGroup now */
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	group = self->group;
	Assert(!selfHasSlot());

	if (procIsInWaitQueue(MyProc))
	{
		/*
		 * Still waiting on the queue when get interrupted, remove
		 * myself from the queue
		 */

		Assert(!groupWaitQueueIsEmpty(group));
		Assert(procIsWaiting(MyProc));
		Assert(selfHasGroup());

		groupWaitQueueErase(group, MyProc);
	}
	else if (MyProc->resSlot != NULL)
	{
		/* Woken up by a slot holder */

		Assert(!procIsInWaitQueue(MyProc));
		Assert(selfIsAssignedValidGroup());

		/* First complete the slot's transfer from MyProc to self */
		slot = MyProc->resSlot;
		MyProc->resSlot = NULL;

		/*
		 * Similar as groupReleaseSlot(), how many pending queries to
		 * wake up depends on how many slots we can get.
		 */
		groupReleaseSlot(group, slot);
		Assert(sessionGetSlot() == NULL);

		group->totalExecuted++;
	}
	else
	{
		/*
		 * The transaction of DROP RESOURCE GROUP is finished,
		 * groupAcquireSlot will do the retry.
		 *
		 * The resource group pointed by self->group may have
		 * already been removed by here.
		 */

		Assert(!procIsInWaitQueue(MyProc));
	}

	if (selfIsAssignedValidGroup())
		addTotalQueueDuration(group);

	LWLockRelease(ResGroupLock);

	localResWaiting = false;
	pgstat_report_waiting(PGBE_WAITING_NONE);
	selfUnsetGroup();
}

static void
groupSetMemorySpillRatio(const ResGroupCaps *caps)
{
	char value[64];

	snprintf(value, sizeof(value), "%d", caps->memSpillRatio);
	set_config_option("memory_spill_ratio", value, PGC_USERSET, PGC_S_RESGROUP,
			GUC_ACTION_SET, true);
}

void
ResGroupGetMemInfo(int *memLimit, int *slotQuota, int *sharedQuota)
{
	const ResGroupCaps *caps = &self->caps;

	*memLimit = groupGetMemExpected(caps);
	*slotQuota = caps->concurrency ? slotGetMemQuotaExpected(caps) : -1;
	*sharedQuota = groupGetMemSharedExpected(caps);
}

/*
 * Validate the consistency of the resgroup information in self.
 *
 * This function checks the consistency of (group & groupId).
 */
static void
selfValidateResGroupInfo(void)
{
	Assert(self->memUsage >= 0);

	AssertImply(self->groupId != InvalidOid,
				self->group != NULL);
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
#endif /* USE_ASSERT_CHECKING */

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

#ifdef USE_ASSERT_CHECKING
/*
 * Check whether self has been set a slot.
 *
 * We don't check whether a resgroup is set or not.
 */
static bool
selfHasSlot(void)
{
	return self->slot != NULL;
}
#endif /* USE_ASSERT_CHECKING */

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
 * Set the slot pointer in self.
 *
 * Some over limitations are put to force the caller understand
 * what it's doing and what it wants:
 * - self must has been set a resgroup;
 * - self must has not been set a slot before set;
 */
static void
selfSetSlot(ResGroupSlotData *slot)
{
	Assert(selfHasGroup());
	Assert(!selfHasSlot());
	Assert(slotIsInUse(slot));

	self->slot = slot;
}

/*
 * Unset the slot pointer in self.
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

	self->slot = NULL;
}

/*
 * Check whether proc is in the resgroup wait queue.
 *
 * Unlike procIsWaiting() this function requires the LWLock.
 */
static bool
procIsInWaitQueue(const PGPROC *proc)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	AssertImply(proc->links.next != INVALID_OFFSET,
				proc->resWaiting != false);

	/* TODO: verify that proc is really in the queue in debug mode */

	return proc->links.next != INVALID_OFFSET;
}

/*
 * Check whether proc is waiting.
 *
 * Unlike procIsInWaitQueue() this function doesn't require the LWLock.
 *
 * FIXME: when asserts are enabled this function is not atomic.
 */
static bool
procIsWaiting(const PGPROC *proc)
{
	AssertImply(proc->links.next != INVALID_OFFSET,
				proc->resWaiting != false);

	return proc->resWaiting;
}

/*
 * Notify a proc it's woken up.
 */
static void
procWakeup(PGPROC *proc)
{
	Assert(!procIsWaiting(proc));

	SetLatch(&proc->procLatch);
}

#ifdef USE_ASSERT_CHECKING
/*
 * Validate a slot's attributes.
 */
static void
slotValidate(const ResGroupSlotData *slot)
{
	Assert(slot != NULL);

	/* further checks whether the slot is freed or idle */
	if (slot->groupId == InvalidOid)
	{
		Assert(slot->sessionId == InvalidSessionId);
		Assert(slot->nProcs == 0);
		Assert(slot->memQuota < 0);
		Assert(slot->memUsage == 0);
	}
	else
	{
		Assert(!slotIsInFreelist(slot));
		AssertImply(Gp_role == GP_ROLE_EXECUTE, slot == sessionGetSlot());
		AssertImply(Gp_role == GP_ROLE_EXECUTE, slot->sessionId == gp_session_id);
	}
}

/*
 * A slot is in use if it has a valid groupId.
 */
static bool
slotIsInUse(const ResGroupSlotData *slot)
{
	slotValidate(slot);

	return slot->groupId != InvalidOid;
}

static bool
slotIsInFreelist(const ResGroupSlotData *slot)
{
	ResGroupSlotData *current;

	current = pResGroupControl->freeSlot;

	for ( ; current != NULL; current = current->next)
	{
		if (current == slot)
			return true;
	}

	return false;
}
#endif /* USE_ASSERT_CHECKING */

/*
 * Get the slot id of the given slot.
 *
 * Return InvalidSlotId if slot is NULL.
 */
static int
slotGetId(const ResGroupSlotData *slot)
{
	int			slotId;

	if (slot == NULL)
		return InvalidSlotId;

	slotId = slot - pResGroupControl->slots;

	Assert(slotId >= 0);
	Assert(slotId < RESGROUP_MAX_SLOTS);

	return slotId;
}

static void
lockResGroupForDrop(ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(group->nRunning == 0);
	group->lockedForDrop = true;
}

static void
unlockResGroupForDrop(ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(group->nRunning == 0);
	group->lockedForDrop = false;
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
#endif /* USE_ASSERT_CHECKING */

/*
 * Validate the consistency of the resgroup wait queue.
 */
static void
groupWaitQueueValidate(const ResGroupData *group)
{
	const PROC_QUEUE	*waitQueue;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

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

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(!procIsInWaitQueue(proc));
	Assert(proc->resWaiting == false);
	Assert(proc->resSlot == NULL);

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;
	headProc = (PGPROC *) &waitQueue->links;

	SHMQueueInsertBefore(&headProc->links, &proc->links);
	proc->resWaiting = true;

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

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(!groupWaitQueueIsEmpty(group));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	proc = (PGPROC *) MAKE_PTR(waitQueue->links.next);
	Assert(procIsInWaitQueue(proc));
	Assert(proc->resWaiting != false);
	Assert(proc->resSlot == NULL);

	SHMQueueDelete(&proc->links);
	proc->resWaiting = false;

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

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(!groupWaitQueueIsEmpty(group));
	Assert(procIsInWaitQueue(proc));
	Assert(proc->resWaiting != false);
	Assert(proc->resSlot == NULL);

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	SHMQueueDelete(&proc->links);
	proc->resWaiting = false;

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

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	return waitQueue->size == 0;
}

/*
 * Parse the query and check if this query should
 * bypass the management of resource group.
 *
 * Currently, only SET/RESET/SHOW command can be bypassed
 */
static bool
shouldBypassQuery(const char* query_string)
{
	MemoryContext oldcontext;
	List *parsetree_list; 
	ListCell *parsetree_item;
	Node *parsetree;

	if (!query_string)
		return false;

	/*
	 * Switch to appropriate context for constructing parsetrees.
	 */
	oldcontext = MemoryContextSwitchTo(MessageContext);

	parsetree_list = pg_parse_query(query_string);

	MemoryContextSwitchTo(oldcontext);

	if (parsetree_list == NULL)
		return false;

	/* Only bypass SET/RESET/SHOW command for now */
	foreach(parsetree_item, parsetree_list)
	{
		parsetree = (Node *) lfirst(parsetree_item);

		if (nodeTag(parsetree) != T_VariableSetStmt &&
			nodeTag(parsetree) != T_VariableShowStmt)
			return false;
	}

	return true;
}

/*
 * Debug helper functions
 */
void
ResGroupDumpInfo(StringInfo str)
{
	int				i;

	if (!IsResGroupEnabled())
		return;

	verifyGpIdentityIsSet();

	appendStringInfo(str, "{\"segid\":%d,", GpIdentity.segindex);
	/* dump fields in pResGroupControl. */
	appendStringInfo(str, "\"segmentsOnMaster\":%d,", pResGroupControl->segmentsOnMaster);
	appendStringInfo(str, "\"loaded\":%s,", pResGroupControl->loaded ? "true" : "false");
	appendStringInfo(str, "\"totalChunks\":%d,", pResGroupControl->totalChunks);
	appendStringInfo(str, "\"freeChunks\":%d,", pResGroupControl->freeChunks);
	appendStringInfo(str, "\"chunkSizeInBits\":%d,", pResGroupControl->chunkSizeInBits);
	
	/* dump each group */
	appendStringInfo(str, "\"groups\":[");
	for (i = 0; i < pResGroupControl->nGroups; i++)
	{
		resgroupDumpGroup(str, &pResGroupControl->groups[i]);
		if (i < pResGroupControl->nGroups - 1)
			appendStringInfo(str, ","); 
	}
	appendStringInfo(str, "],"); 
	/* dump slots */
	resgroupDumpSlots(str);

	appendStringInfo(str, ",");

	/* dump freeslot links */
	resgroupDumpFreeSlots(str);

	appendStringInfo(str, "}"); 
}

static void
resgroupDumpGroup(StringInfo str, ResGroupData *group)
{
	appendStringInfo(str, "{");
	appendStringInfo(str, "\"group_id\":%u,", group->groupId);
	appendStringInfo(str, "\"nRunning\":%d,", group->nRunning);
	appendStringInfo(str, "\"locked_for_drop\":%d,", group->lockedForDrop);
	appendStringInfo(str, "\"memExpected\":%d,", group->memExpected);
	appendStringInfo(str, "\"memQuotaGranted\":%d,", group->memQuotaGranted);
	appendStringInfo(str, "\"memSharedGranted\":%d,", group->memSharedGranted);
	appendStringInfo(str, "\"memQuotaUsed\":%d,", group->memQuotaUsed);
	appendStringInfo(str, "\"memUsage\":%d,", group->memUsage);
	appendStringInfo(str, "\"memSharedUsage\":%d,", group->memSharedUsage);

	resgroupDumpWaitQueue(str, &group->waitProcs);
	resgroupDumpCaps(str, (ResGroupCap*)(&group->caps));
	
	appendStringInfo(str, "}");
}

static void
resgroupDumpWaitQueue(StringInfo str, PROC_QUEUE *queue)
{
	PGPROC *proc;

	appendStringInfo(str, "\"wait_queue\":{");
	appendStringInfo(str, "\"wait_queue_size\":%d,", queue->size);
	appendStringInfo(str, "\"wait_queue_content\":[");

	proc = (PGPROC *)SHMQueueNext(&queue->links,
								  &queue->links, 
								  offsetof(PGPROC, links));

	if (!SHM_PTR_VALID(&proc->links))
	{
		appendStringInfo(str, "]},");
		return;
	}

	while (proc)
	{
		appendStringInfo(str, "{");
		appendStringInfo(str, "\"pid\":%d,", proc->pid);
		appendStringInfo(str, "\"resWaiting\":%d,", proc->resWaiting);
		appendStringInfo(str, "\"resSlot\":%d", slotGetId(proc->resSlot));
		appendStringInfo(str, "}");
		proc = (PGPROC *)SHMQueueNext(&queue->links,
							&proc->links, 
							offsetof(PGPROC, links));
		if (proc)
			appendStringInfo(str, ",");
	}
	appendStringInfo(str, "]},");
}

static void
resgroupDumpCaps(StringInfo str, ResGroupCap *caps)
{
	int i;
	appendStringInfo(str, "\"caps\":[");
	for (i = 1; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		appendStringInfo(str, "{\"%d\":%d}", i, caps[i]);
		if (i < RESGROUP_LIMIT_TYPE_COUNT - 1)
			appendStringInfo(str, ",");
	}
	appendStringInfo(str, "]");
}

static void
resgroupDumpSlots(StringInfo str)
{
	int               i;
	ResGroupSlotData* slot;

	appendStringInfo(str, "\"slots\":[");

	for (i = 0; i < RESGROUP_MAX_SLOTS; i++)
	{
		slot = &(pResGroupControl->slots[i]);

		appendStringInfo(str, "{");
		appendStringInfo(str, "\"slotId\":%d,", i);
		appendStringInfo(str, "\"sessionId\":%d,", slot->sessionId);
		appendStringInfo(str, "\"groupId\":%u,", slot->groupId);
		appendStringInfo(str, "\"memQuota\":%d,", slot->memQuota);
		appendStringInfo(str, "\"memUsage\":%d,", slot->memUsage);
		appendStringInfo(str, "\"nProcs\":%d,", slot->nProcs);
		appendStringInfo(str, "\"next\":%d,", slotGetId(slot->next));
		resgroupDumpCaps(str, (ResGroupCap*)(&slot->caps));
		appendStringInfo(str, "}");
		if (i < RESGROUP_MAX_SLOTS - 1)
			appendStringInfo(str, ",");
	}
	
	appendStringInfo(str, "]");
}

static void
resgroupDumpFreeSlots(StringInfo str)
{
	ResGroupSlotData* head;
	
	head = pResGroupControl->freeSlot;
	
	appendStringInfo(str, "\"free_slot_list\":{");
	appendStringInfo(str, "\"head\":%d", slotGetId(head));
	appendStringInfo(str, "}");
}

/*
 * Set resource group slot for current session.
 */
static void
sessionSetSlot(ResGroupSlotData *slot)
{
	Assert(slot != NULL);
	Assert(MySessionState->resGroupSlot == NULL);

	MySessionState->resGroupSlot = (void *) slot;
}

/*
 * Get resource group slot of current session.
 */
static ResGroupSlotData *
sessionGetSlot(void)
{
	return (ResGroupSlotData *) MySessionState->resGroupSlot;
}

/*
 * Reset resource group slot to NULL for current session.
 */
static void
sessionResetSlot(void)
{
	Assert(MySessionState->resGroupSlot != NULL);

	MySessionState->resGroupSlot = NULL;
}
