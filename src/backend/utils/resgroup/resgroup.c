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
#include "utils/memutils.h"
#include "utils/resgroup-ops.h"
#include "utils/resgroup.h"
#include "utils/resource_manager.h"
#include "utils/resowner.h"
#include "utils/session_state.h"
#include "utils/vmem_tracker.h"

#define InvalidSlotId	(-1)
#define RESGROUP_MAX_SLOTS	(MaxConnections)

/*
 * A hard memory limit in by pass mode, in chunks
 * More chunks are reserved on QD than on QE because planner and orca
 * may need more memory to generate and optimize the plan.
 */
#define RESGROUP_BYPASS_MODE_MEMORY_LIMIT_ON_QD	30
#define RESGROUP_BYPASS_MODE_MEMORY_LIMIT_ON_QE	10

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

typedef struct ResGroupInfo				ResGroupInfo;
typedef struct ResGroupHashEntry		ResGroupHashEntry;
typedef struct ResGroupProcData			ResGroupProcData;
typedef struct ResGroupSlotData			ResGroupSlotData;
typedef struct ResGroupData				ResGroupData;
typedef struct ResGroupControl			ResGroupControl;

/*
 * Resource group info.
 *
 * This records the group and groupId for a transaction.
 * When group->groupId != groupId, it means the group
 * has been dropped.
 */
struct ResGroupInfo
{
	ResGroupData	*group;
	Oid				groupId;
};

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
	/* 
	 * Record current bypass memory limit for each bypass queries.
	 * For bypass mode, memUsage of current process could accumulate in a session.
	 * So should limit the memory usage for each query instead of the whole session.
	 */
	int32	bypassMemoryLimit;
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
	Oid				groupId;
	ResGroupData	*group;		/* pointer to the group */

	ResGroupCaps	caps;

	int32			memQuota;	/* memory quota of current slot */
	int32			memUsage;	/* total memory usage of procs belongs to this slot */
	int				nProcs;		/* number of procs in this slot */

	ResGroupSlotData	*next;
};

/*
 * Resource group operations for memory.
 *
 * Groups with different memory auditor will have different
 * operations.
 */
typedef struct ResGroupMemOperations
{
	void (*group_mem_on_create) (Oid groupId, ResGroupData *group);
	void (*group_mem_on_alter) (Oid groupId, ResGroupData *group);
	void (*group_mem_on_drop) (Oid groupId, ResGroupData *group);
	void (*group_mem_on_notify) (ResGroupData *group);
	void (*group_mem_on_dump) (ResGroupData *group, StringInfo str);
} ResGroupMemOperations;

/*
 * Resource group information.
 */
struct ResGroupData
{
	Oid			groupId;		/* Id for this group */
	ResGroupCaps	caps;		/* capabilities of this group */
	volatile int			nRunning;		/* number of running trans */
	volatile int	nRunningBypassed;		/* number of running trans in bypass mode */
	PROC_QUEUE	waitProcs;		/* list of PGPROC objects waiting on this group */
	int			totalExecuted;	/* total number of executed trans */
	int			totalQueued;	/* total number of queued trans	*/
	Interval	totalQueuedTime;/* total queue time */

	bool		lockedForDrop;  /* true if resource group is dropped but not committed yet */

	/*
	 * memGap is calculated as:
	 * 	(memory limit (before alter) - memory expected (after alter))
	 *
	 * It stands for how many memory (in chunks) this group should
	 * give back to MEM POOL.
	 */
	int32       memGap;

	int32		memExpected;		/* expected memory chunks according to current caps */
	int32		memQuotaGranted;	/* memory chunks for quota part */
	int32		memSharedGranted;	/* memory chunks for shared part */

	volatile int32	memQuotaUsed;	/* memory chunks assigned to all the running slots */

	/*
	 * memory usage of this group, should always equal to the
	 * sum of session memory(session_state->sessionVmem) that
	 * belongs to this group
	 */
	/*
	 * operation functions for resource group
	 */
	const ResGroupMemOperations *groupMemOps;
	volatile int32	memUsage;
	volatile int32	memSharedUsage;
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
	volatile int32	freeChunks;		/* memory chunks not allocated to any group,
									will be used for the query which group share
									memory is not enough*/
	/* 
	 * Safe memory threshold:
	 * if remained global shared memory is less than this threshold,
	 * then the resource group memory usage is in red zone.
	 */
	pg_atomic_uint32 safeChunksThreshold;

	int32			chunkSizeInBits;

	ResGroupSlotData	*slots;		/* slot pool shared by all resource groups */
	ResGroupSlotData	*freeSlot;	/* head of the free list */

	int				nGroups;
	ResGroupData	groups[1];
};

bool gp_resource_group_enable_cgroup_memory = false;
bool gp_resource_group_enable_cgroup_swap = false;
bool gp_resource_group_enable_cgroup_cpuset = false;

/* hooks */
resgroup_assign_hook_type resgroup_assign_hook = NULL;

/* static variables */

static ResGroupControl *pResGroupControl = NULL;

static ResGroupProcData __self =
{
	InvalidOid,
};
static ResGroupProcData *self = &__self;

/* If we are waiting on a group, this points to the associated group */
static ResGroupData *groupAwaited = NULL;

/* the resource group self is running in in bypass mode */
static ResGroupData *bypassedGroup = NULL;
/* a fake slot used in bypass mode */
static ResGroupSlotData bypassedSlot;

/* static functions */

static bool groupApplyMemCaps(ResGroupData *group);
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
static int32 slotGetMemQuotaOnQE(const ResGroupCaps *caps, ResGroupData *group);
static int32 slotGetMemSpill(const ResGroupCaps *caps);
static void wakeupSlots(ResGroupData *group, bool grant);
static void notifyGroupsOnMem(Oid skipGroupId);
static int32 mempoolAutoRelease(ResGroupData *group);
static int32 mempoolAutoReserve(ResGroupData *group, const ResGroupCaps *caps);
static ResGroupData *groupHashNew(Oid groupId);
static ResGroupData *groupHashFind(Oid groupId, bool raise);
static ResGroupData *groupHashRemove(Oid groupId);
static void waitOnGroup(ResGroupData *group);
static ResGroupData *createGroup(Oid groupId, const ResGroupCaps *caps);
static void removeGroup(Oid groupId);
static void AtProcExit_ResGroup(int code, Datum arg);
static void groupWaitCancel(void);
static int32 groupReserveMemQuota(ResGroupData *group);
static void groupReleaseMemQuota(ResGroupData *group, ResGroupSlotData *slot);
static int32 groupIncMemUsage(ResGroupData *group,
							  ResGroupSlotData *slot,
							  int32 chunks);
static int32 groupDecMemUsage(ResGroupData *group,
							  ResGroupSlotData *slot,
							  int32 chunks);
static void initSlot(ResGroupSlotData *slot, ResGroupData *group,
					 int32 slotMemQuota);
static void selfAttachResGroup(ResGroupData *group, ResGroupSlotData *slot);
static void selfDetachResGroup(ResGroupData *group, ResGroupSlotData *slot);
static bool slotpoolInit(void);
static ResGroupSlotData *slotpoolAllocSlot(void);
static void slotpoolFreeSlot(ResGroupSlotData *slot);
static ResGroupSlotData *groupGetSlot(ResGroupData *group);
static void groupPutSlot(ResGroupData *group, ResGroupSlotData *slot);
static Oid decideResGroupId(void);
static void decideResGroup(ResGroupInfo *pGroupInfo);
static bool groupIncBypassedRef(ResGroupInfo *pGroupInfo);
static void groupDecBypassedRef(ResGroupData *group);
static ResGroupSlotData *groupAcquireSlot(ResGroupInfo *pGroupInfo);
static void groupReleaseSlot(ResGroupData *group, ResGroupSlotData *slot);
static void addTotalQueueDuration(ResGroupData *group);
static void groupSetMemorySpillRatio(const ResGroupCaps *caps);
static char *groupDumpMemUsage(ResGroupData *group);
static void selfValidateResGroupInfo(void);
static bool selfIsAssigned(void);
static void selfSetGroup(ResGroupData *group);
static void selfUnsetGroup(void);
static void selfSetSlot(ResGroupSlotData *slot);
static void selfUnsetSlot(void);
static bool procIsWaiting(const PGPROC *proc);
static void procWakeup(PGPROC *proc);
static int slotGetId(const ResGroupSlotData *slot);
static void groupWaitQueueValidate(const ResGroupData *group);
static void groupWaitQueuePush(ResGroupData *group, PGPROC *proc);
static PGPROC *groupWaitQueuePop(ResGroupData *group);
static void groupWaitQueueErase(ResGroupData *group, PGPROC *proc);
static bool groupWaitQueueIsEmpty(const ResGroupData *group);
static bool shouldBypassQuery(const char *query_string);
static void lockResGroupForDrop(ResGroupData *group);
static void unlockResGroupForDrop(ResGroupData *group);
static bool groupIsDropped(ResGroupInfo *pGroupInfo);

static void resgroupDumpGroup(StringInfo str, ResGroupData *group);
static void resgroupDumpWaitQueue(StringInfo str, PROC_QUEUE *queue);
static void resgroupDumpCaps(StringInfo str, ResGroupCap *caps);
static void resgroupDumpSlots(StringInfo str);
static void resgroupDumpFreeSlots(StringInfo str);

static void sessionSetSlot(ResGroupSlotData *slot);
static ResGroupSlotData *sessionGetSlot(void);

static void bindGroupOperation(ResGroupData *group);
static void groupMemOnAlterForVmtracker(Oid groupId, ResGroupData *group);
static void groupMemOnDropForVmtracker(Oid groupId, ResGroupData *group);
static void groupMemOnNotifyForVmtracker(ResGroupData *group);
static void groupMemOnDumpForVmtracker(ResGroupData *group, StringInfo str);

static void groupMemOnAlterForCgroup(Oid groupId, ResGroupData *group);
static void groupMemOnDropForCgroup(Oid groupId, ResGroupData *group);
static void groupMemOnNotifyForCgroup(ResGroupData *group);
static void groupMemOnDumpForCgroup(ResGroupData *group, StringInfo str);
static void groupApplyCgroupMemInc(ResGroupData *group);
static void groupApplyCgroupMemDec(ResGroupData *group);

static void cpusetOperation(char *cpuset1,
							const char *cpuset2,
							int len,
							bool sub);

#ifdef USE_ASSERT_CHECKING
static bool selfHasGroup(void);
static bool selfHasSlot(void);
static void slotValidate(const ResGroupSlotData *slot);
static bool slotIsInFreelist(const ResGroupSlotData *slot);
static bool slotIsInUse(const ResGroupSlotData *slot);
static bool groupIsNotDropped(const ResGroupData *group);
static bool groupWaitQueueFind(ResGroupData *group, const PGPROC *proc);
#endif /* USE_ASSERT_CHECKING */

/*
 * Operations of memory for resource groups with vmtracker memory auditor.
 */
static const ResGroupMemOperations resgroup_memory_operations_vmtracker = {
	.group_mem_on_create	= NULL,
	.group_mem_on_alter		= groupMemOnAlterForVmtracker,
	.group_mem_on_drop		= groupMemOnDropForVmtracker,
	.group_mem_on_notify	= groupMemOnNotifyForVmtracker,
	.group_mem_on_dump		= groupMemOnDumpForVmtracker,
};

/*
 * Operations of memory for resource groups with cgroup memory auditor.
 */
static const ResGroupMemOperations resgroup_memory_operations_cgroup = {
	.group_mem_on_create	= NULL,
	.group_mem_on_alter		= groupMemOnAlterForCgroup,
	.group_mem_on_drop		= groupMemOnDropForCgroup,
	.group_mem_on_notify	= groupMemOnNotifyForCgroup,
	.group_mem_on_dump		= groupMemOnDumpForCgroup,
};

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
	pg_atomic_init_u32(&pResGroupControl->safeChunksThreshold, 0);
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
	char		cpuset[MaxCpuSetLength] = {0};
	int			defaultCore = -1;
	Bitmapset	*bmsUnused = NULL;

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
	pg_atomic_write_u32(&pResGroupControl->safeChunksThreshold,
						pResGroupControl->totalChunks * (100 - runaway_detector_activation_percent) / 100);
	if (pResGroupControl->totalChunks == 0)
		ereport(PANIC,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("insufficient memory available"),
				 errhint("Increase gp_resource_group_memory_limit")));

	ResGroupOps_Init();

	if (gp_resource_group_enable_cgroup_cpuset)
	{
		/* Get cpuset from cpuset/gpdb, and transform it into bitset */
		ResGroupOps_GetCpuSet(RESGROUP_ROOT_ID, cpuset, MaxCpuSetLength);
		bmsUnused = CpusetToBitset(cpuset, MaxCpuSetLength);
		/* get the minimum core number, in case of the zero core is not exist */
		defaultCore = bms_first_from(bmsUnused, 0);
		Assert(defaultCore >= 0);
	}

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
		ResGroupOps_SetMemoryLimit(groupId, caps.memLimit);
		
		if (caps.cpuRateLimit != CPU_RATE_LIMIT_DISABLED)
		{
			ResGroupOps_SetCpuRateLimit(groupId, caps.cpuRateLimit);
		}
		else
		{
			Bitmapset *bmsCurrent = CpusetToBitset(caps.cpuset,
												   MaxCpuSetLength);
			Bitmapset *bmsCommon = bms_intersect(bmsCurrent, bmsUnused);
			Bitmapset *bmsMissing = bms_difference(bmsCurrent, bmsCommon);

			/*
			 * Do not call EnsureCpusetIsAvailable() here as resource group is
			 * not activated yet
			 */
			if (!gp_resource_group_enable_cgroup_cpuset)
			{
				ereport(WARNING,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("cgroup is not properly configured to use the cpuset feature"),
						 errhint("Extra cgroup configurations are required to enable this feature, "
								 "please refer to the Greenplum Documentations for details")));
			}

			Assert(caps.cpuRateLimit == CPU_RATE_LIMIT_DISABLED);

			if (bms_is_empty(bmsMissing))
			{
				/*
				 * write cpus to corresponding file
				 * if all the cores are available
				 */
				ResGroupOps_SetCpuSet(groupId, caps.cpuset);
				bmsUnused = bms_del_members(bmsUnused, bmsCurrent);
			}
			else
			{
				char		cpusetMissing[MaxCpuSetLength] = {0};

				/*
				 * if some of the cores are unavailable, just set defaultCore
				 * to this group and send a warning message, so the system
				 * can startup, then DBA can fix it
				 */
				snprintf(cpuset, MaxCpuSetLength, "%d", defaultCore);
				ResGroupOps_SetCpuSet(groupId, cpuset);
				BitsetToCpuset(bmsMissing, cpusetMissing, MaxCpuSetLength);
				ereport(WARNING,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("cpu cores %s are unavailable on the system "
								"in resource group %s",
								cpusetMissing, GetResGroupNameForId(groupId)),
						 errhint("using core %d for this resource group, "
								 "please adjust the settings and restart",
								 defaultCore)));
			}
		}

		numGroups++;
		Assert(numGroups <= MaxResourceGroups);
	}
	systable_endscan(sscan);

	if (gp_resource_group_enable_cgroup_cpuset)
	{
		/*
		 * set default cpuset
		 */

		if (bms_is_empty(bmsUnused))
		{
			/* no unused core, assign default core to default group */
			snprintf(cpuset, MaxCpuSetLength, "%d", defaultCore);
		}
		else
		{
			/* assign all unused cores to default group */
			BitsetToCpuset(bmsUnused, cpuset, MaxCpuSetLength);
		}

		Assert(cpuset[0]);
		Assert(!CpusetIsEmpty(cpuset));

		ResGroupOps_SetCpuSet(DEFAULT_CPUSET_GROUP_ID, cpuset);
	}

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

	if (group->nRunning + group->nRunningBypassed > 0)
	{
		int nQuery = group->nRunning + group->nRunningBypassed + group->waitProcs.size;

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
ResGroupDropFinish(const ResourceGroupCallbackContext *callbackCtx,
				   bool isCommit)
{
	ResGroupData	*group;
	volatile int	savedInterruptHoldoffCount;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	PG_TRY();
	{
		savedInterruptHoldoffCount = InterruptHoldoffCount;

		group = groupHashFind(callbackCtx->groupid, true);

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			wakeupSlots(group, false);
			unlockResGroupForDrop(group);
		}

		if (isCommit)
		{
			bool		migrate;

			/* Only migrate processes out of vmtracker groups */
			migrate = group->caps.memAuditor == RESGROUP_MEMORY_AUDITOR_VMTRACKER;

			removeGroup(callbackCtx->groupid);
			if (!CpusetIsEmpty(group->caps.cpuset))
			{
				if (gp_resource_group_enable_cgroup_cpuset)
				{
					/* reset default group, add cpu cores to it */
					char cpuset[MaxCpuSetLength];
					ResGroupOps_GetCpuSet(DEFAULT_CPUSET_GROUP_ID,
										  cpuset, MaxCpuSetLength);
					CpusetUnion(cpuset, group->caps.cpuset, MaxCpuSetLength);
					ResGroupOps_SetCpuSet(DEFAULT_CPUSET_GROUP_ID, cpuset);
				}
			}

			ResGroupOps_DestroyGroup(callbackCtx->groupid, migrate);
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
ResGroupCreateOnAbort(const ResourceGroupCallbackContext *callbackCtx)
{
	volatile int savedInterruptHoldoffCount;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	PG_TRY();
	{
		savedInterruptHoldoffCount = InterruptHoldoffCount;
		removeGroup(callbackCtx->groupid);
		/* remove the os dependent part for this resource group */
		ResGroupOps_DestroyGroup(callbackCtx->groupid, true);

		if (!CpusetIsEmpty(callbackCtx->caps.cpuset) &&
			gp_resource_group_enable_cgroup_cpuset)
		{
			/* return cpu cores to default group */
			char defaultGroupCpuset[MaxCpuSetLength];
			ResGroupOps_GetCpuSet(DEFAULT_CPUSET_GROUP_ID,
								  defaultGroupCpuset,
								  MaxCpuSetLength);
			CpusetUnion(defaultGroupCpuset,
						callbackCtx->caps.cpuset,
						MaxCpuSetLength);
			ResGroupOps_SetCpuSet(DEFAULT_CPUSET_GROUP_ID, defaultGroupCpuset);
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
 * Apply the new resgroup caps.
 */
void
ResGroupAlterOnCommit(const ResourceGroupCallbackContext *callbackCtx)
{
	ResGroupData	*group;
	volatile int	savedInterruptHoldoffCount;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	PG_TRY();
	{
		savedInterruptHoldoffCount = InterruptHoldoffCount;
		group = groupHashFind(callbackCtx->groupid, true);

		group->caps = callbackCtx->caps;

		if (callbackCtx->limittype == RESGROUP_LIMIT_TYPE_CPU)
		{
			ResGroupOps_SetCpuRateLimit(callbackCtx->groupid,
										callbackCtx->caps.cpuRateLimit);
		}
		else if (callbackCtx->limittype == RESGROUP_LIMIT_TYPE_CPUSET)
		{
			if (gp_resource_group_enable_cgroup_cpuset)
				ResGroupOps_SetCpuSet(callbackCtx->groupid,
									  callbackCtx->caps.cpuset);
		}
		else if (callbackCtx->limittype != RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO)
		{
			Assert(pResGroupControl->totalChunks > 0);
			ResGroupCap	memLimitGap = 0;
			if (callbackCtx->limittype == RESGROUP_LIMIT_TYPE_MEMORY)
				memLimitGap = callbackCtx->oldCaps.memLimit - callbackCtx->caps.memLimit;
			group->memGap += pResGroupControl->totalChunks * memLimitGap / 100;

			Assert(group->groupMemOps != NULL);
			if (group->groupMemOps->group_mem_on_alter)
				group->groupMemOps->group_mem_on_alter(callbackCtx->groupid, group);
		}
		/* reset default group if cpuset has changed */
		if (strcmp(callbackCtx->oldCaps.cpuset, callbackCtx->caps.cpuset) &&
			gp_resource_group_enable_cgroup_cpuset)
		{
			char defaultCpusetGroup[MaxCpuSetLength];
			/* get current default group value */
			ResGroupOps_GetCpuSet(DEFAULT_CPUSET_GROUP_ID,
								  defaultCpusetGroup,
								  MaxCpuSetLength);
			/* Add old value to default group
			 * sub new value from default group */
			CpusetUnion(defaultCpusetGroup,
							callbackCtx->oldCaps.cpuset,
							MaxCpuSetLength);
			CpusetDifference(defaultCpusetGroup,
							callbackCtx->caps.cpuset,
							MaxCpuSetLength);
			ResGroupOps_SetCpuSet(DEFAULT_CPUSET_GROUP_ID, defaultCpusetGroup);
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

bool
ResGroupIsAssigned(void)
{
	return selfIsAssigned();
}

/*
 * Get resource group id of my proc.
 *
 * Returns InvalidOid in any of below cases:
 * - resource group is not enabled;
 * - resource group is not activated (initialized);
 * - my proc is not running inside a transaction;
 * - my proc is not assigned a resource group yet;
 *
 * Otherwise a valid resource group id is returned.
 *
 * This function is not dead code although there is no consumer in the gpdb
 * code tree.  Some extensions require this to get the internal resource group
 * information.
 */
Oid
GetMyResGroupId(void)
{
	return self->groupId;
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
			result = Int32GetDatum(group->nRunning + group->nRunningBypassed);
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

/*
 * Get the number of primary segments on this host
 */
int
ResGroupGetSegmentNum()
{
	return (Gp_role == GP_ROLE_EXECUTE ? host_segments : pResGroupControl->segmentsOnMaster);
}

static char *
groupDumpMemUsage(ResGroupData *group)
{
	StringInfoData memUsage;

	initStringInfo(&memUsage);

	Assert(group->groupMemOps != NULL);
	if (group->groupMemOps->group_mem_on_dump)
		group->groupMemOps->group_mem_on_dump(group, &memUsage);

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
		Assert(selfIsAssigned());

		write_log("Resource group memory information: "
				  "current group id is %u, "
				  "memLimit cap is %d, "
				  "memSharedQuota cap is %d, "
				  "memSpillRatio cap is %d, "
				  "group expected memory limit is %d MB, "
				  "memory quota granted in currenct group is %d MB, "
				  "shared quota granted in current group is %d MB, "
				  "memory assigned to all running slots is %d MB, "
				  "memory usage in current group is %d MB, "
				  "memory shared usage in current group is %d MB, "
				  "memory quota in current slot is %d MB, "
				  "memory usage in current slot is %d MB, "
				  "memory usage in current proc is %d MB",
				  group->groupId,
				  group->caps.memLimit,
				  group->caps.memSharedQuota,
				  group->caps.memSpillRatio,
				  VmemTracker_ConvertVmemChunksToMB(group->memExpected),
				  VmemTracker_ConvertVmemChunksToMB(group->memQuotaGranted),
				  VmemTracker_ConvertVmemChunksToMB(group->memSharedGranted),
				  VmemTracker_ConvertVmemChunksToMB(group->memQuotaUsed),
				  VmemTracker_ConvertVmemChunksToMB(group->memUsage),
				  VmemTracker_ConvertVmemChunksToMB(group->memSharedUsage),
				  VmemTracker_ConvertVmemChunksToMB(slot->memQuota),
				  VmemTracker_ConvertVmemChunksToMB(slot->memUsage),
				  VmemTracker_ConvertVmemChunksToMB(self->memUsage));
	}
	else
	{
		Assert(!selfIsAssigned());

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
	if (bypassedGroup)
	{
		/*
		 * Do not allow to allocate more than the per proc limit.
		 */
		if (self->memUsage > self->bypassMemoryLimit)
		{
			self->memUsage -= memoryChunks;
			return false;
		}

		/*
		 * Set group & slot to bypassed ones so we could follow the limitation
		 * checking logic as normal transactions.
		 */
		group = bypassedGroup;
		slot = &bypassedSlot;
	}
	else if (!selfIsAssigned())
		return true;

	Assert(bypassedGroup || slotIsInUse(slot));
	Assert(group->memUsage >= 0);
	Assert(self->memUsage >= 0);

	/* add memoryChunks into group & slot memory usage */
	overuseMem = groupIncMemUsage(group, slot, memoryChunks);

	/* then check whether there is over usage */
	if (CritSectionCount == 0)
	{
		if (overuseMem > overuseChunks)
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
		else if (overuseMem > 0)
		{
			/* the over usage is within the allowed threshold */
			*waiverUsed = true;
		}
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

	if (!IsResGroupEnabled())
		return;

	Assert(memoryChunks >= 0);
	Assert(memoryChunks <= self->memUsage);

	self->memUsage -= memoryChunks;
	if (bypassedGroup)
	{
		/*
		 * Set group & slot to bypassed ones so we could follow the release
		 * logic as normal transactions.
		 */
		group = bypassedGroup;
		slot = &bypassedSlot;
	}
	else if (!selfIsAssigned())
		return;

	Assert(bypassedGroup || slotIsInUse(slot));

	groupDecMemUsage(group, slot, memoryChunks);
}

int64
ResourceGroupGetQueryMemoryLimit(void)
{
	ResGroupSlotData	*slot = self->slot;
	int64				memSpill;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	if (bypassedGroup)
	{
		int64		bytesInMB = 1 << BITS_IN_MB;
		int64		bytesInChunk = 1 << VmemTracker_GetChunkSizeInBits();

		/*
		 * In bypass mode there is a hard memory limit of
		 * RESGROUP_BYPASS_MODE_MEMORY_LIMIT_ON_QE chunk,
		 * we should make sure query_mem + misc mem <= chunk.
		 */
		return Min(bytesInMB,
				   bytesInChunk * RESGROUP_BYPASS_MODE_MEMORY_LIMIT_ON_QE / 2);
	}

	Assert(selfIsAssigned());

	if (IsResManagerMemoryPolicyNone())
		return 0;

	memSpill = slotGetMemSpill(&slot->caps);
	/* memSpill is already converted to chunks */
	Assert(memSpill >= 0);

	return memSpill << VmemTracker_GetChunkSizeInBits();
}

/*
 * removeGroup -- remove resource group from share memory and
 * reclaim the group's memory back to MEM POOL.
 */
static void
removeGroup(Oid groupId)
{
	ResGroupData *group;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(OidIsValid(groupId));

	group = groupHashRemove(groupId);

	Assert(group->groupMemOps != NULL);
	if (group->groupMemOps->group_mem_on_drop)
		group->groupMemOps->group_mem_on_drop(groupId, group);

	group->groupId = InvalidOid;
	notifyGroupsOnMem(groupId);
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
	group->nRunningBypassed = 0;
	ProcQueueInit(&group->waitProcs);
	group->totalExecuted = 0;
	group->totalQueued = 0;
	group->memGap = 0;
	group->memUsage = 0;
	group->memSharedUsage = 0;
	group->memQuotaUsed = 0;
	group->groupMemOps = NULL;
	memset(&group->totalQueuedTime, 0, sizeof(group->totalQueuedTime));
	group->lockedForDrop = false;

	group->memQuotaGranted = 0;
	group->memSharedGranted = 0;
	group->memExpected = groupGetMemExpected(caps);

	chunks = mempoolReserve(groupId, group->memExpected);
	groupRebalanceQuota(group, chunks, caps);

	bindGroupOperation(group);

	return group;
}

/*
 * Bind operation to resource group according to memory auditor.
 */
static void
bindGroupOperation(ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	if (group->caps.memAuditor == RESGROUP_MEMORY_AUDITOR_VMTRACKER)
		group->groupMemOps = &resgroup_memory_operations_vmtracker;
	else if (group->caps.memAuditor == RESGROUP_MEMORY_AUDITOR_CGROUP)
		group->groupMemOps = &resgroup_memory_operations_cgroup;
	else
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid memory auditor: %d", group->caps.memAuditor)));
}

/*
 * Add chunks into group and slot memory usage.
 *
 * Return the total over used chunks of global share
 */
static int32
groupIncMemUsage(ResGroupData *group, ResGroupSlotData *slot, int32 chunks)
{
	int32			slotMemUsage;	/* the memory current slot has been used */
	int32			sharedMemUsage;	/* the total shared memory usage,
										sum of group share and global share */
	int32			globalOveruse = 0;	/* the total over used chunks of global share*/

	/* Add the chunks to memUsage in slot */
	slotMemUsage = pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &slot->memUsage,
										   chunks);

	/* Check whether shared memory should be added */
	sharedMemUsage = slotMemUsage - slot->memQuota;
	if (sharedMemUsage > 0)
	{
		/* Decide how many chunks should be counted as shared memory */
		int32 deltaSharedMemUsage = Min(sharedMemUsage, chunks);

		/* Add these chunks to memSharedUsage in group, 
		 * and record the old value*/
		int32 oldSharedUsage = pg_atomic_fetch_add_u32((pg_atomic_uint32 *)
													   &group->memSharedUsage,
													   deltaSharedMemUsage);
		/* the free space of group share */
		int32 oldSharedFree = Max(0, group->memSharedGranted - oldSharedUsage);

		/* Calculate the global over used chunks */
		int32 deltaGlobalSharedMemUsage = Max(0, deltaSharedMemUsage - oldSharedFree);

		/* freeChunks -= deltaGlobalSharedMemUsage and get the new value */
		int32 newFreeChunks = pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)
													  &pResGroupControl->freeChunks,
													  deltaGlobalSharedMemUsage);
		/* calculate the total over used chunks of global share */
		globalOveruse = Max(0, 0 - newFreeChunks);
	}

	/* Add the chunks to memUsage in group */
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &group->memUsage,
							chunks);

	return globalOveruse;
}

/*
 * Sub chunks from group ,slot memory usage and global shared memory.
 * return memory chunks of global shared released this time
 */
static int32 
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
		int32 deltaSharedMemUsage = Min(sharedMemUsage, chunks);

		/* Sub chunks from memSharedUsage in group */
		int32 oldSharedUsage = pg_atomic_fetch_sub_u32((pg_atomic_uint32 *) &group->memSharedUsage,
										deltaSharedMemUsage);
		Assert(oldSharedUsage >= deltaSharedMemUsage);

		/* record the total global share usage of current group */
		int32 grpTotalGlobalUsage = Max(0, oldSharedUsage - group->memSharedGranted);
		/* calculate the global share usage of current release */
		int32 deltaGlobalSharedMemUsage = Min(grpTotalGlobalUsage, deltaSharedMemUsage);
		/* add chunks to global shared memory */
		pg_atomic_add_fetch_u32((pg_atomic_uint32 *)
								&pResGroupControl->freeChunks,
								deltaGlobalSharedMemUsage);
		return deltaGlobalSharedMemUsage;
	}

	return 0;
}

/*
 * Attach a process (QD or QE) to a slot.
 */
static void
selfAttachResGroup(ResGroupData *group, ResGroupSlotData *slot)
{
	selfSetGroup(group);
	selfSetSlot(slot);

	groupIncMemUsage(group, slot, self->memUsage);
	pg_atomic_add_fetch_u32((pg_atomic_uint32*) &slot->nProcs, 1);
}

/*
 * Detach a process (QD or QE) from a slot.
 */
static void
selfDetachResGroup(ResGroupData *group, ResGroupSlotData *slot)
{
	groupDecMemUsage(group, slot, self->memUsage);
	pg_atomic_sub_fetch_u32((pg_atomic_uint32*) &slot->nProcs, 1);
	selfUnsetSlot();
	selfUnsetGroup();
}

/*
 * Initialize the members of a slot
 */
static void
initSlot(ResGroupSlotData *slot, ResGroupData *group, int32 slotMemQuota)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(!slotIsInUse(slot));
	Assert(group->groupId != InvalidOid);

	slot->group = group;
	slot->groupId = group->groupId;
	slot->caps = group->caps;
	slot->memQuota = slotMemQuota;
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

		slot->group = NULL;
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

	slot->group = NULL;
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
	int32				slotMemQuota;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(groupIsNotDropped(group));

	caps = &group->caps;

	/* First check if the concurrency limit is reached */
	if (group->nRunning >= caps->concurrency)
		return NULL;

	slotMemQuota = groupReserveMemQuota(group);
	if (slotMemQuota < 0)
		return NULL;

	/* Now actually get a free slot */
	slot = slotpoolAllocSlot();
	Assert(!slotIsInUse(slot));

	initSlot(slot, group, slotMemQuota);

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
	int32		released;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(group->memQuotaUsed >= 0);
	Assert(slotIsInUse(slot));

	/* Return the memory quota granted to this slot */
	groupReleaseMemQuota(group, slot);

	/* Return the slot back to free list */
	slotpoolFreeSlot(slot);
	group->nRunning--;

	/*
	 * Reset resource group slot for current session. Note MySessionState
	 * could be reset as NULL in shmem_exit() before calling this function.
	 */
	if (MySessionState != NULL)
		MySessionState->resGroupSlot = NULL;

	/* And finally release the overused memory quota */
	released = mempoolAutoRelease(group);
	if (released > 0)
		notifyGroupsOnMem(group->groupId);

	/*
	 * Once we have waken up other groups then the slot we just released
	 * might be reused, so we should not touch it anymore since now.
	 */
}

/*
 * Reserve memory quota for a slot in group.
 *
 * If there is not enough free memory quota then return -1 and nothing
 * is changed; otherwise return the reserved quota size.
 */
static int32
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
		return -1;
	}

	group->memQuotaUsed += slotMemQuota;

	return slotMemQuota;
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
static void
decideResGroup(ResGroupInfo *pGroupInfo)
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

	LWLockRelease(ResGroupLock);

	pGroupInfo->group = group;
	pGroupInfo->groupId = groupId;
}

/*
 * Increase the bypassed ref count
 *
 * Return true if the operation is done, or false if the group is dropped.
 */
static bool
groupIncBypassedRef(ResGroupInfo *pGroupInfo)
{
	ResGroupData	*group = pGroupInfo->group;
	bool			result = false;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	/* Has the group been dropped? */
	if (groupIsDropped(pGroupInfo))
		goto end;

	/* Is the group locked for drop? */
	if (group->lockedForDrop)
		goto end;

	result = true;
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &group->nRunningBypassed, 1);

end:
	LWLockRelease(ResGroupLock);
	return result;
}

/*
 * Decrease the bypassed ref count
 */
static void
groupDecBypassedRef(ResGroupData *group)
{
	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) &group->nRunningBypassed, 1);
}

/*
 * Acquire a resource group slot
 *
 * Call this function at the start of the transaction.
 * This function set current resource group in MyResGroupSharedInfo,
 * and current slot in MyProc->resSlot.
 */
static ResGroupSlotData *
groupAcquireSlot(ResGroupInfo *pGroupInfo)
{
	ResGroupSlotData *slot;
	ResGroupData	 *group;

	Assert(!selfIsAssigned());
	group = pGroupInfo->group;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	/* Has the group been dropped? */
	if (groupIsDropped(pGroupInfo))
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

/*
 * Wake up the backends in the wait queue when 'concurrency' is increased.
 * This function is called in the callback function of ALTER RESOURCE GROUP.
 *
 * Return TRUE if any memory quota or shared quota is returned to MEM POOL.
 */
static bool
groupApplyMemCaps(ResGroupData *group)
{
	int32				reserved;
	int32				released;
	const ResGroupCaps	*caps = &group->caps;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	group->memExpected = groupGetMemExpected(caps);

	released = mempoolAutoRelease(group);
	Assert(released >= 0);

	/*
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
	reserved = mempoolAutoReserve(group, caps);
	Assert(reserved >= 0);

	return released > reserved;
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
	int32 oldFreeChunks;
	int32 newFreeChunks;
	int32 reserved = 0;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	/* Compare And Save to avoid concurrency problem without using lock */
	while (true)
	{
		oldFreeChunks = pg_atomic_read_u32((pg_atomic_uint32 *)
										   &pResGroupControl->freeChunks);
		reserved = Min(Max(0, oldFreeChunks), chunks);
		newFreeChunks = oldFreeChunks - reserved;
		if (reserved == 0)
			break;
		if (pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)
										   &pResGroupControl->freeChunks,
										   (uint32 *) &oldFreeChunks,
										   (uint32) newFreeChunks))
			break;
	}

	/* also update the safeChunksThreshold which is used in runaway detector */
	if (reserved != 0)
	{
		pg_atomic_sub_fetch_u32(&pResGroupControl->safeChunksThreshold,
								reserved * (100 - runaway_detector_activation_percent) / 100);
	}
	LOG_RESGROUP_DEBUG(LOG, "allocate %u out of %u chunks to group %d",
					   reserved, oldFreeChunks, groupId);

	Assert(newFreeChunks <= pResGroupControl->totalChunks);

	return reserved;
}

/*
 * Return chunks to MEM POOL.
 */
static void
mempoolRelease(Oid groupId, int32 chunks)
{
	int32 newFreeChunks;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(chunks >= 0);

	newFreeChunks = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)
											&pResGroupControl->freeChunks,
											chunks);

	/* also update the safeChunksThreshold which is used in runaway detector */
	pg_atomic_add_fetch_u32(&pResGroupControl->safeChunksThreshold,
							chunks * (100 - runaway_detector_activation_percent) / 100);

	LOG_RESGROUP_DEBUG(LOG, "free %u to pool(%u) chunks from group %d",
					   chunks, newFreeChunks - chunks, groupId);

	Assert(newFreeChunks <= pResGroupControl->totalChunks);
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
	if (memory_spill_ratio != RESGROUP_FALLBACK_MEMORY_SPILL_RATIO)
		/* memSpill is in percentage mode */
		return groupGetMemExpected(caps) * memory_spill_ratio / 100;
	else
		/* memSpill is in fallback mode, return statement_mem instead */
		return VmemTracker_ConvertVmemMBToChunks(statement_mem >> 10);
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
 * Get per-slot expected memory quota in chunks on QE.
 */
static int32
slotGetMemQuotaOnQE(const ResGroupCaps *caps, ResGroupData *group)
{
	int nFreeSlots = caps->concurrency - group->nRunning;

	/*
	 * On QE the runtime status must also be considered as it might have
	 * different caps with QD.
	 */
	if (nFreeSlots <= 0)
		return Min(slotGetMemQuotaExpected(caps),
			   (group->memQuotaGranted - group->memQuotaUsed) / caps->concurrency);
	else
		return Min(slotGetMemQuotaExpected(caps),
				(group->memQuotaGranted - group->memQuotaUsed) / nFreeSlots);
}

/*
 * Get per-slot expected memory spill in chunks
 */
static int32
slotGetMemSpill(const ResGroupCaps *caps)
{
	if (memory_spill_ratio != RESGROUP_FALLBACK_MEMORY_SPILL_RATIO)
	{
		/* memSpill is in percentage mode */
		Assert(caps->concurrency != 0);
		return groupGetMemSpillTotal(caps) / caps->concurrency;
	}
	else
	{
		/*
		 * memSpill is in fallback mode, it is an absolute value, no need to
		 * divide by concurrency.
		 */
		return groupGetMemSpillTotal(caps);
	}
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

		waitProc->resSlot = slot;

		procWakeup(waitProc);
	}
}

/*
 * When a group returns chunks to MEM POOL, we need to:
 * 1. For groups with vmtracker memory auditor, wake up the
 *    transactions waiting on them for memory quota.
 * 2. For groups with cgroup memory auditor, increase their
 *    memory limit if needed.
 */
static void
notifyGroupsOnMem(Oid skipGroupId)
{
	int				i;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	for (i = 0; i < MaxResourceGroups; i++)
	{
		ResGroupData	*group = &pResGroupControl->groups[i];

		if (group->groupId == InvalidOid)
			continue;

		if (group->groupId == skipGroupId)
			continue;

		Assert(group->groupMemOps != NULL);
		if (group->groupMemOps->group_mem_on_notify)
			group->groupMemOps->group_mem_on_notify(group);

		if (!pResGroupControl->freeChunks)
			break;
	}
}

/*
 * Release overused memory quota to MEM POOL.
 *
 * Both overused shared and non-shared memory quota will be released.
 *
 * If there was enough non-shared memory quota for free slots,
 * then after this call there will still be enough non-shared memory quota.
 *
 * If this function is called after a slot is released, make sure that
 * group->nRunning is updated before this function.
 *
 * Return the total released quota in chunks, can be 0.
 *
 * XXX: Some examples.
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
static int32
mempoolAutoRelease(ResGroupData *group)
{
	int32		memQuotaNeeded;
	int32		memQuotaToFree;
	int32		memSharedNeeded;
	int32		memSharedToFree;
	int32		nfreeSlots;
	ResGroupCaps *caps = &group->caps;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	/* nfreeSlots is the number of free slots */
	nfreeSlots = caps->concurrency - group->nRunning;

	/* the in use non-shared quota must be reserved */
	memQuotaNeeded = group->memQuotaUsed;

	/* also should reserve enough non-shared quota for free slots */
	memQuotaNeeded +=
		nfreeSlots > 0 ? slotGetMemQuotaExpected(caps) * nfreeSlots : 0;

	memQuotaToFree = group->memQuotaGranted - memQuotaNeeded;
	if (memQuotaToFree > 0)
	{
		/* release the over used non-shared quota to MEM POOL */
		mempoolRelease(group->groupId, memQuotaToFree); 
		group->memQuotaGranted -= memQuotaToFree; 
	}

	memSharedNeeded = Max(group->memSharedUsage,
						  groupGetMemSharedExpected(caps));
	memSharedToFree = group->memSharedGranted - memSharedNeeded;
	if (memSharedToFree > 0)
	{
		/* release the over used shared quota to MEM POOL */
		mempoolRelease(group->groupId, memSharedToFree);
		group->memSharedGranted -= memSharedToFree;
	}

	return Max(memQuotaToFree, 0) + Max(memSharedToFree, 0);
}

/*
 * Try to acquire enough quota & shared quota for current group from MEM POOL,
 * the actual acquired quota depends on system loads.
 *
 * Return the reserved quota in chunks, can be 0.
 */
static int32
mempoolAutoReserve(ResGroupData *group, const ResGroupCaps *caps)
{
	int32 currentMemStocks = group->memSharedGranted + group->memQuotaGranted;
	int32 neededMemStocks = group->memExpected - currentMemStocks;
	int32 chunks = 0;

	if (neededMemStocks > 0)
	{
		chunks = mempoolReserve(group->groupId, neededMemStocks);
		groupRebalanceQuota(group, chunks, caps);
	}

	return chunks;
}

/* Update the total queued time of this group */
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
	Assert(!selfIsAssigned());

	groupPutSlot(group, slot);

	/*
	 * We should wake up other pending queries on master nodes.
	 */
	if (GpIdentity.segindex == MASTER_CONTENT_ID)
		/*
		 * My slot is put back, then how many queuing queries should I wake up?
		 * Maybe zero, maybe one, maybe more, depends on how the resgroup's
		 * configuration were changed during our execution.
		 */
		wakeupSlots(group, true);
}

/*
 * Serialize the resource group information that need to dispatch to segment.
 */
void
SerializeResGroupInfo(StringInfo str)
{
	int i;
	int			itmp;
	ResGroupCap	tmp;
	ResGroupCaps	caps;

	if (selfIsAssigned())
		caps = self->caps;
	else
	{
		MemSet(&caps, 0, sizeof(caps));
	}

	tmp = htonl(self->groupId);
	appendBinaryStringInfo(str, (char *) &tmp, sizeof(self->groupId));

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		if (i == RESGROUP_LIMIT_TYPE_CPUSET)
		{
			appendBinaryStringInfo(str, caps.cpuset, sizeof(caps.cpuset));
		}
		else
		{
			switch (i)
			{
				case RESGROUP_LIMIT_TYPE_CONCURRENCY:
					tmp = htonl(caps.concurrency);
					break;
				case RESGROUP_LIMIT_TYPE_CPU:
					tmp = htonl(caps.cpuRateLimit);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY:
					tmp = htonl(caps.memLimit);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
					tmp = htonl(caps.memSharedQuota);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
					tmp = htonl(caps.memSpillRatio);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
					tmp = htonl(caps.memAuditor);
					break;
			}
			appendBinaryStringInfo(str, (char *) &tmp, sizeof(ResGroupCap));
		}
	}

	itmp = htonl(bypassedSlot.groupId);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(itmp));
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
	int			itmp;
	ResGroupCap	tmp;
	const char	*ptr = buf;

	Assert(len > 0);

	memcpy(&tmp, ptr, sizeof(*groupId));
	*groupId = ntohl(tmp);
	ptr += sizeof(*groupId);

	for (i = 0; i < RESGROUP_LIMIT_TYPE_COUNT; i++)
	{
		if (i == RESGROUP_LIMIT_TYPE_CPUSET)
		{
			memcpy(capsOut->cpuset, ptr, sizeof(capsOut->cpuset));
			ptr += sizeof(capsOut->cpuset);
		}
		else
		{
			memcpy(&tmp, ptr, sizeof(ResGroupCap));
			switch (i)
			{
				case RESGROUP_LIMIT_TYPE_CONCURRENCY:
					capsOut->concurrency = ntohl(tmp);
					break;
				case RESGROUP_LIMIT_TYPE_CPU:
					capsOut->cpuRateLimit = ntohl(tmp);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY:
					capsOut->memLimit = ntohl(tmp);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
					capsOut->memSharedQuota = ntohl(tmp);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
					capsOut->memSpillRatio = ntohl(tmp);
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
					capsOut->memAuditor = ntohl(tmp);
					break;
			}
			ptr += sizeof(ResGroupCap);
		}
	}

	memcpy(&itmp, ptr, sizeof(itmp));
	bypassedSlot.groupId = ntohl(itmp);
	ptr += sizeof(itmp);

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
	ResGroupSlotData	*slot;
	ResGroupInfo		groupInfo;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	/*
	 * if query should be bypassed, do not assign a
	 * resource group, leave self unassigned
	 */
	if (shouldBypassQuery(debug_query_string))
	{
		/*
		 * Although we decide to bypass this query we should load the
		 * memory_spill_ratio setting from the resgroup, otherwise a
		 * `SHOW memory_spill_ratio` command will output the default value 20
		 * if it's the first query in the connection (make sure tab completion
		 * is not triggered otherwise it will run some implicit query before
		 * you execute the SHOW command).
		 *
		 * Also need to increase a bypassed ref count to prevent the group
		 * being dropped concurrently.
		 */
		do {
			decideResGroup(&groupInfo);
		} while (!groupIncBypassedRef(&groupInfo));

		/* Record which resgroup we are running in */
		bypassedGroup = groupInfo.group;

		/* Update pg_stat_activity statistics */
		bypassedGroup->totalExecuted++;
		pgstat_report_resgroup(0, bypassedGroup->groupId);

		/* Initialize the fake slot */
		bypassedSlot.group = groupInfo.group;
		bypassedSlot.groupId = groupInfo.groupId;
		bypassedSlot.memQuota = 0;
		bypassedSlot.memUsage = 0;

		/* Attach self memory usage to resgroup */
		groupIncMemUsage(bypassedGroup, &bypassedSlot, self->memUsage);
		
		/* Record the bypass memory limit of current query */
		self->bypassMemoryLimit = self->memUsage + RESGROUP_BYPASS_MODE_MEMORY_LIMIT_ON_QD;

		/* Add into cgroup */
		ResGroupOps_AssignGroup(bypassedGroup->groupId,
								&bypassedGroup->caps,
								MyProcPid);

		groupSetMemorySpillRatio(&bypassedGroup->caps);
		return;
	}

	PG_TRY();
	{
		do {
			decideResGroup(&groupInfo);

			/* Acquire slot */
			slot = groupAcquireSlot(&groupInfo);
		} while (slot == NULL);

		/* Set resource group slot for current session */
		sessionSetSlot(slot);

		/* Add proc memory accounting info into group and slot */
		selfAttachResGroup(groupInfo.group, slot);

		/* Init self */
		self->caps = slot->caps;

		/* Don't error out before this line in this function */
		SIMPLE_FAULT_INJECTOR(ResGroupAssignedOnMaster);

		/* Add into cgroup */
		ResGroupOps_AssignGroup(self->groupId, &(self->caps), MyProcPid);

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

	if (bypassedGroup)
	{
		/* bypass mode ref count is only maintained on qd */
		if (Gp_role == GP_ROLE_DISPATCH)
			groupDecBypassedRef(bypassedGroup);

		/* Detach self memory usage from resgroup */
		groupDecMemUsage(bypassedGroup, &bypassedSlot, self->memUsage);

		/* Reset the fake slot */
		bypassedSlot.group = NULL;
		bypassedSlot.groupId = InvalidOid;
		bypassedGroup = NULL;

		/* Update pg_stat_activity statistics */
		pgstat_report_resgroup(0, InvalidOid);
		return;
	}

	if (Gp_role == GP_ROLE_EXECUTE && GpIdentity.segindex == MASTER_CONTENT_ID)
		SIMPLE_FAULT_INJECTOR(Unassign_Resgroup_Start_Entrydb);

	if (!selfIsAssigned())
		return;

	/* Cleanup self */
	if (self->memUsage > 10)
		LOG_RESGROUP_DEBUG(LOG, "idle proc memory usage: %d", self->memUsage);

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	/* Sub proc memory accounting info from group and slot */
	selfDetachResGroup(group, slot);

	/* Release the slot if no reference. */
	if (slot->nProcs == 0)
		groupReleaseSlot(group, slot);

	LWLockRelease(ResGroupLock);

	if (Gp_role == GP_ROLE_DISPATCH)
		SIMPLE_FAULT_INJECTOR(Unassign_Resgroup_End_QD);

	pgstat_report_resgroup(0, InvalidOid);
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
	ResGroupCaps		caps = {0};
	ResGroupData		*group;
	ResGroupSlotData	*slot;

	DeserializeResGroupInfo(&caps, &newGroupId, buf, len);

	/*
	 * QD will dispatch the resgroup id via bypassedSlot.groupId
	 * in bypass mode.
	 */
	if (bypassedSlot.groupId != InvalidOid)
	{
		/* Are we already running in bypass mode? */
		if (bypassedGroup != NULL)
		{
			Assert(bypassedGroup->groupId == bypassedSlot.groupId);
			return;
		}

		/* Find out the resgroup by id */
		LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
		bypassedGroup = groupHashFind(bypassedSlot.groupId, true);
		LWLockRelease(ResGroupLock);

		Assert(bypassedGroup != NULL);

		/* Initialize the fake slot */
		bypassedSlot.memQuota = 0;
		bypassedSlot.memUsage = 0;

		/* Attach self memory usage to resgroup */
		groupIncMemUsage(bypassedGroup, &bypassedSlot, self->memUsage);
		
		/* Record the bypass memory limit of current query */
		self->bypassMemoryLimit = self->memUsage + RESGROUP_BYPASS_MODE_MEMORY_LIMIT_ON_QE;
		return;
	}

	if (newGroupId == InvalidOid)
	{
		UnassignResGroup();
		return;
	}

	if (self->groupId != InvalidOid)
	{
		/* it's not the first dispatch in the same transaction */
		Assert(self->groupId == newGroupId);
		Assert(self->caps.concurrency == caps.concurrency);
		Assert(self->caps.cpuRateLimit == caps.cpuRateLimit);
		Assert(self->caps.memLimit == caps.memLimit);
		Assert(self->caps.memSharedQuota == caps.memSharedQuota);
		Assert(self->caps.memSpillRatio == caps.memSpillRatio);
		Assert(self->caps.memAuditor == caps.memAuditor);
		Assert(!strcmp(self->caps.cpuset, caps.cpuset));
		return;
	}

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	group = groupHashFind(newGroupId, true);
	Assert(group != NULL);

	/* Init self */
	Assert(host_segments > 0);
	Assert(caps.concurrency > 0);
	self->caps = caps;

	/* Init slot */
	slot = sessionGetSlot();
	if (slot != NULL)
	{
		Assert(slotIsInUse(slot));
		Assert(slot->groupId == newGroupId);
	}
	else
	{
		/* This is the first QE of this session, allocate a slot from slot pool */
		slot = slotpoolAllocSlot();
		Assert(!slotIsInUse(slot));
		sessionSetSlot(slot);
		mempoolAutoReserve(group, &caps);
		initSlot(slot, group,
				 slotGetMemQuotaOnQE(&caps, group));
		group->memQuotaUsed += slot->memQuota;
		Assert(group->memQuotaUsed <= group->memQuotaGranted);
		group->nRunning++;
	}

	selfAttachResGroup(group, slot);

	LWLockRelease(ResGroupLock);

	/* finally we can say we are in a valid resgroup */
	Assert(selfIsAssigned());

	/* Add into cgroup */
	ResGroupOps_AssignGroup(self->groupId, &(self->caps), MyProcPid);
}

/*
 * Wait on the queue of resource group
 */
static void
waitOnGroup(ResGroupData *group)
{
	PGPROC *proc = MyProc;

	Assert(!LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(!selfIsAssigned());

	pgstat_report_resgroup(GetCurrentTimestamp(), group->groupId);

	/*
	 * Mark that we are waiting on resource group
	 *
	 * This is used for interrupt cleanup, similar to lockAwaited in ProcSleep
	 */
	groupAwaited = group;

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

	groupAwaited = NULL;

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
static ResGroupData *
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

	return group;
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

	/* Nothing to do if we weren't waiting on a group */
	if (groupAwaited == NULL)
		return;

	Assert(!selfIsAssigned());

	group = groupAwaited;

	/* We are sure to be interrupted in the for loop of waitOnGroup now */
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	AssertImply(procIsWaiting(MyProc),
				groupWaitQueueFind(group, MyProc));

	if (procIsWaiting(MyProc))
	{
		/*
		 * Still waiting on the queue when get interrupted, remove
		 * myself from the queue
		 */

		Assert(!groupWaitQueueIsEmpty(group));

		groupWaitQueueErase(group, MyProc);

		addTotalQueueDuration(group);
	}
	else if (MyProc->resSlot != NULL)
	{
		/* Woken up by a slot holder */

		Assert(!procIsWaiting(MyProc));

		/* First complete the slot's transfer from MyProc to self */
		slot = MyProc->resSlot;
		MyProc->resSlot = NULL;

		/*
		 * Similar as groupReleaseSlot(), how many pending queries to
		 * wake up depends on how many slots we can get.
		 */
		groupReleaseSlot(group, slot);

		group->totalExecuted++;

		addTotalQueueDuration(group);
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

		Assert(!procIsWaiting(MyProc));
	}

	LWLockRelease(ResGroupLock);

	groupAwaited = NULL;
	pgstat_report_waiting(PGBE_WAITING_NONE);
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
	AssertImply(self->group == NULL,
			self->slot == NULL);
	AssertImply(self->group != NULL,
			self->slot != NULL);

	return self->groupId != InvalidOid;
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
#endif /* USE_ASSERT_CHECKING */

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
	Assert(!selfIsAssigned());
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
	Assert(!selfHasSlot());

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
 * Check whether proc is in some resgroup's wait queue.
 *
 * The LWLock is not required.
 *
 * This function does not check whether proc is in a specific resgroup's
 * wait queue. To make this check use groupWaitQueueFind().
 */
static bool
procIsWaiting(const PGPROC *proc)
{
	/*
	 * The typical asm instructions fow below C operation can be like this:
	 * ( gcc 4.8.5-11, x86_64-redhat-linux, -O0 )
	 *
     *     mov    -0x8(%rbp),%rax           ; load proc
     *     mov    0x8(%rax),%rax            ; load proc->links.next
     *     cmp    $0xffffffffffffffff,%rax  ; compare with INVALID_OFFSET
     *     setne  %al                       ; store the result
	 *
	 * The operation is atomic, so a lock is not required here.
	 */
	return proc->links.next != INVALID_OFFSET;
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
		Assert(slot->nProcs == 0);
		Assert(slot->memQuota < 0);
		Assert(slot->memUsage == 0);
	}
	else
	{
		Assert(!slotIsInFreelist(slot));
		AssertImply(Gp_role == GP_ROLE_EXECUTE, slot == sessionGetSlot());
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
	Assert(group->nRunningBypassed == 0);
	group->lockedForDrop = true;
}

static void
unlockResGroupForDrop(ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(group->nRunning == 0);
	Assert(group->nRunningBypassed == 0);
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
	Assert(!procIsWaiting(proc));
	Assert(proc->resSlot == NULL);

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;
	headProc = (PGPROC *) &waitQueue->links;

	SHMQueueInsertBefore(&headProc->links, &proc->links);

	waitQueue->size++;

	Assert(groupWaitQueueFind(group, proc));
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
	Assert(groupWaitQueueFind(group, proc));
	Assert(proc->resSlot == NULL);

	SHMQueueDelete(&proc->links);

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
	Assert(groupWaitQueueFind(group, proc));
	Assert(proc->resSlot == NULL);

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	SHMQueueDelete(&proc->links);

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

#ifdef USE_ASSERT_CHECKING
/*
 * Find proc in group's wait queue.
 *
 * Return true if found or false if not found.
 *
 * This functions is expensive so should only be used in debugging logic,
 * in most cases procIsWaiting() shall be used.
 */
static bool
groupWaitQueueFind(ResGroupData *group, const PGPROC *proc)
{
	PROC_QUEUE			*waitQueue;
	SHM_QUEUE			*head;
	PGPROC				*iter;
	Size				offset;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;
	head = &waitQueue->links;
	offset = offsetof(PGPROC, links);

	for (iter = (PGPROC *) SHMQueueNext(head, head, offset); iter;
		 iter = (PGPROC *) SHMQueueNext(head, &iter->links, offset))
	{
		if (iter == proc)
		{
			Assert(procIsWaiting(proc));
			return true;
		}
	}

	return false;
}
#endif/* USE_ASSERT_CHECKING */

/*
 * Parse the query and check if this query should
 * bypass the management of resource group.
 *
 * Currently, only SET/RESET/SHOW command can be bypassed
 */
static bool
shouldBypassQuery(const char *query_string)
{
	MemoryContext oldcontext = NULL;
	MemoryContext tmpcontext = NULL;
	List *parsetree_list; 
	ListCell *parsetree_item;
	Node *parsetree;

	if (gp_resource_group_bypass)
		return true;

	if (!query_string)
		return false;

	/*
	 * Switch to appropriate context for constructing parsetrees.
	 *
	 * It is possible that MessageContext is NULL, for example in a bgworker:
	 *
	 *     debug_query_string = "select 1";
	 *     StartTransactionCommand();
	 *
	 * This is not the recommended order of setting debug_query_string, but we
	 * should not put a constraint on the order by resource group anyway.
	 */
	if (MessageContext)
		oldcontext = MemoryContextSwitchTo(MessageContext);
	else
	{
		/* Create a temp memory context to prevent memory leaks */
		tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
										   "resgroup temporary context",
										   ALLOCSET_DEFAULT_MINSIZE,
										   ALLOCSET_DEFAULT_INITSIZE,
										   ALLOCSET_DEFAULT_MAXSIZE);
		oldcontext = MemoryContextSwitchTo(tmpcontext);
	}

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

	list_free_deep(parsetree_list);

	if (tmpcontext)
		MemoryContextDelete(tmpcontext);

	return true;
}

/*
 * Check whether the resource group has been dropped.
 */
static bool
groupIsDropped(ResGroupInfo *pGroupInfo)
{
	Assert(pGroupInfo != NULL);
	Assert(pGroupInfo->group != NULL);

	return pGroupInfo->group->groupId != pGroupInfo->groupId;
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
	appendStringInfo(str, "\"nRunningBypassed\":%d,", group->nRunningBypassed);
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
		appendStringInfo(str, "\"resWaiting\":%s,",
						 procIsWaiting(proc) ? "true" : "false");
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
 * Operation for resource groups with vmtracker memory auditor
 * when alter its memory limit.
 */
static void
groupMemOnAlterForVmtracker(Oid groupId, ResGroupData *group)
{
	bool shouldNotify;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	shouldNotify = groupApplyMemCaps(group);

	wakeupSlots(group, true);
	if (shouldNotify)
		notifyGroupsOnMem(groupId);
}

/*
 * Operation for resource groups with vmtracker memory auditor
 * when reclaiming its memory back to MEM POOL.
 */
static void
groupMemOnDropForVmtracker(Oid groupId, ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	mempoolRelease(groupId, group->memQuotaGranted + group->memSharedGranted);
	group->memQuotaGranted = 0;
	group->memSharedGranted = 0;
}

/*
 * Operation for resource groups with vmtracker memory auditor
 * when memory in MEM POOL is increased.
 */
static void
groupMemOnNotifyForVmtracker(ResGroupData *group)
{
	int32			delta;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (group->lockedForDrop)
		return;

	if (groupWaitQueueIsEmpty(group))
		return;

	delta = group->memExpected - group->memQuotaGranted - group->memSharedGranted;
	if (delta <= 0)
		return;

	wakeupSlots(group, true);
}

/*
 * Operation for resource groups with vmtracker memory auditor
 * when dump memory statistics.
 */
static void
groupMemOnDumpForVmtracker(ResGroupData *group, StringInfo str)
{
	appendStringInfo(str, "{");
	appendStringInfo(str, "\"used\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(group->memUsage));
	appendStringInfo(str, "\"available\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(
				group->memQuotaGranted + group->memSharedGranted - group->memUsage));
	appendStringInfo(str, "\"quota_used\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(group->memQuotaUsed));
	appendStringInfo(str, "\"quota_available\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(
				group->memQuotaGranted - group->memQuotaUsed));
	appendStringInfo(str, "\"quota_granted\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(group->memQuotaGranted));
	appendStringInfo(str, "\"quota_proposed\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(
				groupGetMemQuotaExpected(&group->caps)));
	appendStringInfo(str, "\"shared_used\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(group->memSharedUsage));
	appendStringInfo(str, "\"shared_available\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(
				group->memSharedGranted - group->memSharedUsage));
	appendStringInfo(str, "\"shared_granted\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(group->memSharedGranted));
	appendStringInfo(str, "\"shared_proposed\":%d",
			VmemTracker_ConvertVmemChunksToMB(
				groupGetMemSharedExpected(&group->caps)));
	appendStringInfo(str, "}");
}

/*
 * Operation for resource groups with cgroup memory auditor
 * when alter its memory limit.
 */
static void
groupMemOnAlterForCgroup(Oid groupId, ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	/*
	 * If memGap is positive, it indicates this group should
	 * give back these many memory back to MEM POOL.
	 *
	 * If memGap is negative, it indicates this group should
	 * retrieve these many memory from MEM POOL.
	 *
	 * If memGap is zero, this group is holding the same memory
	 * as it expects.
	 */
	if (group->memGap == 0)
		return;

	if (group->memGap > 0)
		groupApplyCgroupMemDec(group);
	else
		groupApplyCgroupMemInc(group);
}

/*
 * Increase a resource group's cgroup memory limit
 *
 * This may not take effect immediately.
 */
static void
groupApplyCgroupMemInc(ResGroupData *group)
{
	ResGroupCompType comp = RESGROUP_COMP_TYPE_MEMORY;
	int32 memory_limit;
	int32 memory_inc;
	int fd;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(group->memGap < 0);

	memory_inc = mempoolReserve(group->groupId, group->memGap * -1);

	if (memory_inc <= 0)
		return;

	fd = ResGroupOps_LockGroup(group->groupId, comp, true);
	memory_limit = ResGroupOps_GetMemoryLimit(group->groupId);
	ResGroupOps_SetMemoryLimitByValue(group->groupId, memory_limit + memory_inc);
	ResGroupOps_UnLockGroup(group->groupId, fd);

	group->memGap += memory_inc;
}

/*
 * Decrease a resource group's cgroup memory limit
 *
 * This will take effect immediately for now.
 */
static void
groupApplyCgroupMemDec(ResGroupData *group)
{
	ResGroupCompType comp = RESGROUP_COMP_TYPE_MEMORY;
	int32 memory_limit;
	int32 memory_dec;
	int fd;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));
	Assert(group->memGap > 0);

	fd = ResGroupOps_LockGroup(group->groupId, comp, true);
	memory_limit = ResGroupOps_GetMemoryLimit(group->groupId);
	Assert(memory_limit > group->memGap);

	memory_dec = group->memGap;

	ResGroupOps_SetMemoryLimitByValue(group->groupId, memory_limit - memory_dec);
	ResGroupOps_UnLockGroup(group->groupId, fd);

	mempoolRelease(group->groupId, memory_dec);
	notifyGroupsOnMem(group->groupId);

	group->memGap -= memory_dec;
}

/*
 * Operation for resource groups with cgroup memory auditor
 * when reclaiming its memory back to MEM POOL.
 */
static void
groupMemOnDropForCgroup(Oid groupId, ResGroupData *group)
{
	int32 memory_expected;

	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	memory_expected = groupGetMemExpected(&group->caps);

	mempoolRelease(groupId, memory_expected + group->memGap);
}

/*
 * Operation for resource groups with cgroup memory auditor
 * when memory in MEM POOL is increased.
 */
static void
groupMemOnNotifyForCgroup(ResGroupData *group)
{
	Assert(LWLockHeldExclusiveByMe(ResGroupLock));

	if (group->memGap < 0)
		groupApplyCgroupMemInc(group);
}

/*
 * Operation for resource groups with cgroup memory auditor
 * when dump memory statistics.
 */
static void
groupMemOnDumpForCgroup(ResGroupData *group, StringInfo str)
{
	appendStringInfo(str, "{");
	appendStringInfo(str, "\"used\":%d, ",
			VmemTracker_ConvertVmemChunksToMB(
				ResGroupOps_GetMemoryUsage(group->groupId) / ResGroupGetSegmentNum()));
	appendStringInfo(str, "\"limit_granted\":%d",
			VmemTracker_ConvertVmemChunksToMB(
				ResGroupOps_GetMemoryLimit(group->groupId) / ResGroupGetSegmentNum()));
	appendStringInfo(str, "}");
}

/*
 * Parse cpuset to bitset
 * if onlyCheck is true, the function only check whether cpuset is valid
 * If cpuset is "1,3-5", Bitmapset 1,3,4,5 are set.
 */
Bitmapset *
CpusetToBitset(const char *cpuset, int len)
{
	int	pos = 0, num1 = 0, num2 = 0;
	enum Status
	{
		Initial,
		Begin,
		Number,
		Interval,
		Number2
	};
	enum Status	s = Initial;

	Bitmapset	*bms = NULL;
	if (cpuset == NULL || len <= 0)
		return bms;
	while (pos < len && cpuset[pos])
	{
		char c = cpuset[pos++];
		if (c == ',')
		{
			if (s == Initial || s == Begin)
			{
				continue;
			}
			else if (s == Interval)
			{
				goto error_logic;
			}
			else if (s == Number)
			{
				bms = bms_union(bms, bms_make_singleton(num1));
				num1 = 0;
				s = Begin;
			}
			else if (s == Number2)
			{
				if (num1 > num2)
				{
					goto error_logic;
				}
				for (int i = num1; i <= num2; ++i)
				{
					bms = bms_union(bms, bms_make_singleton(i));
				}
				num1 = num2 = 0;
				s = Begin;
			}
		}
		else if (c == '-')
		{
			if (s != Number)
			{
				goto error_logic;
			}
			s = Interval;
		}
		else if (isdigit(c))
		{
			if (s == Initial || s == Begin)
			{
				s = Number;
			}
			else if (s == Interval)
			{
				s = Number2;
			}
			if (s == Number)
			{
				num1 = num1 * 10 + (c - '0');
			}
			else if (s == Number2)
			{
				num2 = num2 * 10 + (c - '0');
			}
		}
		else if (c == '\n')
		{
			break;
		}
		else
		{
			goto error_logic;
		}
	}
	if (s == Number)
	{
		bms = bms_union(bms, bms_make_singleton(num1));
	}
	else if (s == Number2)
	{
		if (num1 > num2)
		{
			goto error_logic;
		}
		for (int i = num1; i <= num2; ++i)
		{
			bms = bms_union(bms, bms_make_singleton(i));
		}
	}
	else if (s == Initial || s == Interval)
	{
		goto error_logic;
	}
	return bms;
error_logic:
	return NULL;
}

/*
 * Check the value of cpuset is empty or not
 */
bool CpusetIsEmpty(const char *cpuset)
{
	return strcmp(cpuset, DefaultCpuset) == 0;
}

/*
 * Set cpuset value to default value -1.
 */
void SetCpusetEmpty(char *cpuset, int cpusetSize)
{
	StrNCpy(cpuset, DefaultCpuset, cpusetSize);
}

/*
 * Transform non-empty bitset to cpuset.
 *
 * This function does not check the cpu cores are available or not.
 */
void
BitsetToCpuset(const Bitmapset *bms,
			   char *cpuset,
			   int cpusetSize)
{
	int len = 0;
	int lastContinuousBit = -1;
	int	intervalStart = -1;
	int num;
	char buffer[32] = {0};

	Assert(!bms_is_empty(bms));

	cpuset[0] = '\0';

	bms_foreach(num, bms)
	{
		if (lastContinuousBit == -1)
		{
			intervalStart = lastContinuousBit = num;
		}
		else
		{
			if (num != lastContinuousBit + 1)
			{
				if (intervalStart == lastContinuousBit)
				{
					snprintf(buffer, sizeof(buffer), "%d,", intervalStart);
				}
				else
				{
					snprintf(buffer, sizeof(buffer), "%d-%d,", intervalStart, lastContinuousBit);
				}
				if (len + strlen(buffer) >= cpusetSize)
				{
					Assert(cpuset[0]);
					return ;
				}
				strcpy(cpuset + len, buffer);
				len += strlen(buffer);
				intervalStart = lastContinuousBit = num;
			}
			else
			{
				lastContinuousBit = num;
			}
		}
	}
	if (intervalStart != -1)
	{
		if (intervalStart == lastContinuousBit)
		{
			snprintf(buffer, sizeof(buffer), "%d", intervalStart);
		}
		else
		{
			snprintf(buffer, sizeof(buffer), "%d-%d", intervalStart, lastContinuousBit);
		}
		if (len + strlen(buffer) >= cpusetSize)
		{
			Assert(cpuset[0]);
			return ;
		}
		strcpy(cpuset + len, buffer);
		len += strlen(buffer);
	}
	else
	{
		/* bms is non-empty, so it should never reach here */
		pg_unreachable();
	}
}

/*
 * calculate the result of cpuset1 plus/minus cpuset2 and save in place
 * if sub is true, the operation is minus
 * if sub is false, the operation is plus
 */
void
cpusetOperation(char *cpuset1, const char *cpuset2,
							int len, bool sub)
{
	char cpuset[MaxCpuSetLength] = {0};
	int defaultCore = -1;
	Bitmapset *bms1 = CpusetToBitset(cpuset1, len);
	Bitmapset *bms2 = CpusetToBitset(cpuset2, len);
	if (sub)
	{
		bms1 = bms_del_members(bms1, bms2);
	}
	else
	{
		bms1 = bms_add_members(bms1, bms2);
	}
	if (!bms_is_empty(bms1))
	{
		BitsetToCpuset(bms1, cpuset1, len);
	}
	else
	{
		/* Get cpuset from cpuset/gpdb, and transform it into bitset */
		ResGroupOps_GetCpuSet(RESGROUP_ROOT_ID, cpuset, MaxCpuSetLength);
		Bitmapset *bmsDefault = CpusetToBitset(cpuset, MaxCpuSetLength);
		/* get the minimum core number, in case of the zero core is not exist */
		defaultCore = bms_first_from(bmsDefault, 0);
		Assert(defaultCore >= 0);
		snprintf(cpuset1, MaxCpuSetLength, "%d", defaultCore);
	}
}

/*
 * union cpuset2 to cpuset1
 */
void
CpusetUnion(char *cpuset1, const char *cpuset2, int len)
{
	cpusetOperation(cpuset1, cpuset2, len, false);
}

/*
 * subtract cpuset2 from cpuset1
 */
void
CpusetDifference(char *cpuset1, const char *cpuset2, int len)
{
	cpusetOperation(cpuset1, cpuset2, len, true);
}

/*
 * ensure that cpuset is available.
 */
bool
EnsureCpusetIsAvailable(int elevel)
{
	if (!IsResGroupActivated())
	{
		ereport(elevel,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("resource group must be enabled to use cpuset feature")));

		return false;
	}

	if (!gp_resource_group_enable_cgroup_cpuset)
	{
		ereport(elevel,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("cgroup is not properly configured to use the cpuset feature"),
				 errhint("Extra cgroup configurations are required to enable this feature, "
						 "please refer to the Greenplum Documentations for details")));

		return false;
	}

	return true;
}

/*
 * Check whether current resource group's memory usage is in RedZone.
 */
bool
IsGroupInRedZone(void)
{
	uint32				remainGlobalSharedMem;
	uint32				safeChunksThreshold;
	ResGroupSlotData	*slot = self->slot;
	ResGroupData		*group = self->group;

	/*
	 * IsGroupInRedZone is called frequently, we should put the
	 * condition which returns with higher probability in front.
	 * 
	 * safe: global shared memory is not in redzone
	 */
	remainGlobalSharedMem = (uint32) pg_atomic_read_u32(&pResGroupControl->freeChunks);
	safeChunksThreshold = (uint32) pg_atomic_read_u32(&pResGroupControl->safeChunksThreshold);
	if (remainGlobalSharedMem >= safeChunksThreshold)
		return false;

	AssertImply(slot != NULL, group != NULL);
	if (!slot)
		return false;

	/* safe: slot memory is not used up */
	if (slot->memQuota > slot->memUsage)
		return false;

	/* safe: group shared memory is not in redzone */
	if (group->memSharedGranted > group->memSharedUsage)
		return false;

	/* memory usage in this group is in RedZone */
	return true;
}



/*
 * Dump memory information for current resource group.
 * This is the output of resource group runaway.
 */
void
ResGroupGetMemoryRunawayInfo(StringInfo str)
{
	ResGroupSlotData	*slot = self->slot;
	ResGroupData		*group = self->group;
	uint32				remainGlobalSharedMem = 0;
	uint32				safeChunksThreshold = 0;

	if (group)
	{
		Assert(selfIsAssigned());

		remainGlobalSharedMem = (uint32) pg_atomic_read_u32(&pResGroupControl->freeChunks);
		safeChunksThreshold = (uint32) pg_atomic_read_u32(&pResGroupControl->safeChunksThreshold);

		appendStringInfo(str,
						 "current group id is %u, "
						 "group memory usage %d MB, "
						 "group shared memory quota is %d MB, "
						 "slot memory quota is %d MB, "
						 "global freechunks memory is %u MB, "
						 "global safe memory threshold is %u MB",
						 group->groupId,
						 VmemTracker_ConvertVmemChunksToMB(group->memUsage),
						 VmemTracker_ConvertVmemChunksToMB(group->memSharedGranted),
						 VmemTracker_ConvertVmemChunksToMB(slot->memQuota),
						 VmemTracker_ConvertVmemChunksToMB(remainGlobalSharedMem),
						 VmemTracker_ConvertVmemChunksToMB(safeChunksThreshold));
	}
	else
	{
		Assert(!selfIsAssigned());

		appendStringInfo(str,
						 "Resource group memory information: "
						 "memory usage in current proc is %d MB",
						 VmemTracker_ConvertVmemChunksToMB(self->memUsage));
	}
}

/*
 * Return group id for a session
 */
Oid
SessionGetResGroupId(SessionState *session)
{
	ResGroupSlotData	*sessionSlot = (ResGroupSlotData *)session->resGroupSlot;
	if (sessionSlot)
		return sessionSlot->groupId;
	else
		return InvalidOid;
}

/*
 * Return group global share memory for a session
 */
int32
SessionGetResGroupGlobalShareMemUsage(SessionState *session)
{
	ResGroupSlotData	*sessionSlot = (ResGroupSlotData *)session->resGroupSlot;
	if (sessionSlot)
	{
		/* lock not needed here, we just need esimated result */
		ResGroupData	*group = sessionSlot->group;
		return group->memSharedUsage - group->memSharedGranted;
	}
	else
	{
		/* session doesnot have group slot */
		return 0;
	}
}
