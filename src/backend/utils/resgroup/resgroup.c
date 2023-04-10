/*-------------------------------------------------------------------------
 *
 * resgroup.c
 *	  GPDB resource group management code.
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resgroup/resgroup.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "libpq-fe.h"
#include "access/genam.h"
#include "access/table.h"
#include "tcop/tcopprot.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "catalog/pg_resgroupcapability.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/memquota.h"
#include "commands/resgroupcmds.h"
#include "common/hashfn.h"
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
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/cgroup.h"
#include "utils/resgroup.h"
#include "utils/resource_manager.h"
#include "utils/session_state.h"
#include "utils/vmem_tracker.h"
#include "utils/cgroup-ops-v1.h"
#include "utils/cgroup-ops-dummy.h"
#include "utils/cgroup-ops-v2.h"

#define InvalidSlotId	(-1)
#define RESGROUP_MAX_SLOTS	(MaxConnections)

/*
 * GUC variables.
 */
int							gp_resgroup_memory_policy = RESMANAGER_MEMORY_POLICY_NONE;
bool						gp_log_resgroup_memory = false;
int							gp_resgroup_memory_query_fixed_mem;
int							gp_resgroup_memory_policy_auto_fixed_mem;
bool						gp_resgroup_print_operator_memory_limits = false;

bool						gp_resgroup_debug_wait_queue = true;
int							gp_resource_group_queuing_timeout = 0;

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

	int				nProcs;		/* number of procs in this slot */

	ResGroupSlotData	*next;

	ResGroupCaps	caps;
};

/*
 * Resource group information.
 */
struct ResGroupData
{
	Oid				groupId;			/* ID for this group */

	volatile int	nRunning;			/* number of running trans */
	volatile int	nRunningBypassed;	/* number of running trans in bypass mode */
	int64			totalExecuted;		/* total number of executed trans */
	int64			totalQueued;		/* total number of queued trans	*/
	int64			totalQueuedTimeMs;	/* total queue time, in milliseconds */
	PROC_QUEUE		waitProcs;			/* list of PGPROC objects waiting on this group */

	bool			lockedForDrop;  	/* true if resource group is dropped but not committed yet */

	ResGroupCaps	caps;				/* capabilities of this group */
};

struct ResGroupControl
{
	int 			segmentsOnMaster;

	ResGroupSlotData	*slots;		/* slot pool shared by all resource groups */
	ResGroupSlotData	*freeSlot;	/* head of the free list */

	HTAB			*htbl;

	/*
	 * The hash table for resource groups in shared memory should only be populated
	 * once, so we add a flag here to implement this requirement.
	 */
	bool			loaded;

	int				nGroups;
	ResGroupData	groups[1];
};

bool gp_resource_group_enable_cgroup_cpuset = false;

CGroupOpsRoutine *cgroupOpsRoutine = NULL;
CGroupSystemInfo *cgroupSystemInfo = NULL;

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
static TimestampTz groupWaitStart;
static TimestampTz groupWaitEnd;

/* the resource group self is running in bypass mode */
static ResGroupData *bypassedGroup = NULL;
/* a fake slot used in bypass mode */
static ResGroupSlotData bypassedSlot;

/* static functions */


static void wakeupSlots(ResGroupData *group, bool grant);

static ResGroupData *groupHashNew(Oid groupId);
static ResGroupData *groupHashFind(Oid groupId, bool raise);
static ResGroupData *groupHashRemove(Oid groupId);
static void waitOnGroup(ResGroupData *group, bool isMoveQuery);
static ResGroupData *createGroup(Oid groupId, const ResGroupCaps *caps);
static void removeGroup(Oid groupId);
static void AtProcExit_ResGroup(int code, Datum arg);
static void groupWaitCancel(bool isMoveQuery);

static void initSlot(ResGroupSlotData *slot, ResGroupData *group);
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
static ResGroupSlotData *groupAcquireSlot(ResGroupInfo *pGroupInfo, bool isMoveQuery);
static void groupReleaseSlot(ResGroupData *group, ResGroupSlotData *slot, bool isMoveQuery);
static void addTotalQueueDuration(ResGroupData *group);
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
static void groupWaitProcValidate(PGPROC *proc, PROC_QUEUE *head);
static void groupWaitQueuePush(ResGroupData *group, PGPROC *proc);
static PGPROC *groupWaitQueuePop(ResGroupData *group);
static void groupWaitQueueErase(ResGroupData *group, PGPROC *proc);
static bool groupWaitQueueIsEmpty(const ResGroupData *group);
static bool checkBypassWalker(Node *node, void *context);
static bool shouldBypassSelectQuery(Node *node);
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
static void sessionResetSlot(void);
static ResGroupSlotData *sessionGetSlot(void);

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
 * Initialize the global CGroupOpsRoutine struct of resource groups.
 */
void
CGroupOpsAndInfoInit(void)
{
	bool        found;
	int			size;

	size = sizeof(CGroupOpsRoutine);
	cgroupOpsRoutine = (CGroupOpsRoutine *)
		ShmemInitStruct("global cgroup operations routine",
									   size, &found);
	if (found)
		return;
	if (cgroupOpsRoutine == NULL)
		goto error_out;

	size = sizeof(CGroupSystemInfo);
	cgroupSystemInfo = (CGroupSystemInfo *)
		ShmemInitStruct("global cgroup system info",
									   size, &found);

	if (found)
		return;
	if (cgroupSystemInfo == NULL)
		goto error_out;

	return;

error_out:
	ereport(FATAL,
			(errcode(ERRCODE_OUT_OF_MEMORY),
					errmsg("not enough shared memory for cgroup operations routine")));
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
initCgroup(void)
{
#ifdef __linux__
	if (Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP)
	{
		cgroupOpsRoutine = get_group_routine_v1();
		cgroupSystemInfo = get_cgroup_sysinfo_v1();
	}
	else
	{
		cgroupOpsRoutine = get_group_routine_v2();
		cgroupSystemInfo = get_cgroup_sysinfo_v2();
	}
#else
	cgroupOpsRoutine = get_cgroup_routine_dummy();
	cgroupSystemInfo = get_cgroup_sysinfo_dummy();
#endif

	bool probe_result = cgroupOpsRoutine->probecgroup();
	if (!probe_result)
		elog(ERROR, "The control group is not well configured, please check your"
					"system configuration.");

	cgroupOpsRoutine->checkcgroup();
	cgroupOpsRoutine->initcgroup();
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

	if (Gp_role == GP_ROLE_DISPATCH && pResGroupControl->segmentsOnMaster == 0)
	{
		Assert(IS_QUERY_DISPATCHER());
		qdinfo = cdbcomponent_getComponentInfo(MASTER_CONTENT_ID); 
		pResGroupControl->segmentsOnMaster = qdinfo->hostPrimaryCount;
		Assert(pResGroupControl->segmentsOnMaster > 0);
	}

	/*
	 * The resgroup shared mem initialization must be serialized. Only the first session
	 * should do the init.
	 * Serialization is done by LW_EXCLUSIVE ResGroupLock. However, we must obtain all DB
	 * locks before obtaining LWlock to prevent deadlock.
	 */
	relResGroup = table_open(ResGroupRelationId, AccessShareLock);
	relResGroupCapability = table_open(ResGroupCapabilityRelationId, AccessShareLock);
	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	if (pResGroupControl->loaded)
		goto exit;

	if (gp_resource_group_enable_cgroup_cpuset)
	{
		/* Get cpuset from cpuset/gpdb, and transform it into bitset */
		cgroupOpsRoutine->getcpuset(CGROUP_ROOT_ID, cpuset, MaxCpuSetLength);
		bmsUnused = CpusetToBitset(cpuset, MaxCpuSetLength);
		/* get the minimum core number, in case of the zero core is not exist */
		defaultCore = bms_next_member(bmsUnused, -1);
		Assert(defaultCore >= 0);
	}

	numGroups = 0;
	sscan = systable_beginscan(relResGroup, InvalidOid, false, NULL, 0, NULL);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Oid			groupId = ((Form_pg_resgroup) GETSTRUCT(tuple))->oid;
		ResGroupData	*group;

		Bitmapset *bmsCurrent;

		GetResGroupCapabilities(relResGroupCapability, groupId, &caps);

		group = createGroup(groupId, &caps);
		Assert(group != NULL);

		cgroupOpsRoutine->createcgroup(groupId);

		if (CpusetIsEmpty(caps.cpuset))
		{
			cgroupOpsRoutine->setcpulimit(groupId, caps.cpuHardQuotaLimit);
			cgroupOpsRoutine->setcpupriority(groupId, caps.cpuSoftPriority);
		}
		else
		{
			char *cpuset = getCpuSetByRole(caps.cpuset);
			bmsCurrent = CpusetToBitset(cpuset, MaxCpuSetLength);

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

			Assert(caps.cpuHardQuotaLimit == CPU_HARD_QUOTA_LIMIT_DISABLED);

			if (bms_is_empty(bmsMissing))
			{
				/*
				 * write cpus to corresponding file
				 * if all the cores are available
				 */
				char *cpuset= getCpuSetByRole(caps.cpuset);
				cgroupOpsRoutine->setcpuset(groupId, cpuset);
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
				cgroupOpsRoutine->setcpuset(groupId, cpuset);
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

		cgroupOpsRoutine->setcpuset(DEFAULT_CPUSET_GROUP_ID, cpuset);
	}

	pResGroupControl->loaded = true;
	LOG_RESGROUP_DEBUG(LOG, "initialized %d resource groups", numGroups);

exit:
	LWLockRelease(ResGroupLock);

	/*
	 * release lock here to guarantee we have no lock held when acquiring
	 * resource group slot
	 */
	table_close(relResGroup, AccessShareLock);
	table_close(relResGroupCapability, AccessShareLock);
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
			removeGroup(callbackCtx->groupid);
			if (!CpusetIsEmpty(group->caps.cpuset))
			{
				if (gp_resource_group_enable_cgroup_cpuset)
				{
					/* reset default group, add cpu cores to it */
					char cpuset[MaxCpuSetLength];
					cgroupOpsRoutine->getcpuset(DEFAULT_CPUSET_GROUP_ID,
										  cpuset, MaxCpuSetLength);
					CpusetUnion(cpuset, group->caps.cpuset, MaxCpuSetLength);
					cgroupOpsRoutine->setcpuset(DEFAULT_CPUSET_GROUP_ID, cpuset);
				}
			}

			cgroupOpsRoutine->destroycgroup(callbackCtx->groupid, true);
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
		cgroupOpsRoutine->destroycgroup(callbackCtx->groupid, true);

		if (!CpusetIsEmpty(callbackCtx->caps.cpuset) &&
			gp_resource_group_enable_cgroup_cpuset)
		{
			/* return cpu cores to default group */
			char defaultGroupCpuset[MaxCpuSetLength];
			cgroupOpsRoutine->getcpuset(DEFAULT_CPUSET_GROUP_ID,
								  defaultGroupCpuset,
								  MaxCpuSetLength);
			CpusetUnion(defaultGroupCpuset,
						callbackCtx->caps.cpuset,
						MaxCpuSetLength);
			cgroupOpsRoutine->setcpuset(DEFAULT_CPUSET_GROUP_ID, defaultGroupCpuset);
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
			cgroupOpsRoutine->setcpulimit(callbackCtx->groupid,
										callbackCtx->caps.cpuHardQuotaLimit);
		}
		else if (callbackCtx->limittype == RESGROUP_LIMIT_TYPE_CPU_SHARES)
		{
			cgroupOpsRoutine->setcpupriority(callbackCtx->groupid,
										  callbackCtx->caps.cpuSoftPriority);
		}
		else if (callbackCtx->limittype == RESGROUP_LIMIT_TYPE_CPUSET)
		{
			if (gp_resource_group_enable_cgroup_cpuset)
			{
				char *cpuset = getCpuSetByRole(callbackCtx->caps.cpuset);
				cgroupOpsRoutine->setcpuset(callbackCtx->groupid,
									        cpuset);
			}
		}
		else if (callbackCtx->limittype == RESGROUP_LIMIT_TYPE_CONCURRENCY)
		{
			wakeupSlots(group, true);
		}
		/* reset default group if cpuset has changed */
		if (strcmp(callbackCtx->oldCaps.cpuset, callbackCtx->caps.cpuset) &&
			gp_resource_group_enable_cgroup_cpuset)
		{
			char defaultCpusetGroup[MaxCpuSetLength];
			/* get current default group value */
			cgroupOpsRoutine->getcpuset(DEFAULT_CPUSET_GROUP_ID,
								  defaultCpusetGroup,
								  MaxCpuSetLength);
			/* Add old value to default group
			 * sub new value from default group */
			char *cpuset= getCpuSetByRole(callbackCtx->caps.cpuset);
			char *oldcpuset = getCpuSetByRole(callbackCtx->oldCaps.cpuset);
			CpusetUnion(defaultCpusetGroup,
						oldcpuset,
						MaxCpuSetLength);
			CpusetDifference(defaultCpusetGroup,
						cpuset,
						MaxCpuSetLength);
			cgroupOpsRoutine->setcpuset(DEFAULT_CPUSET_GROUP_ID, defaultCpusetGroup);
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

/*
 *  Retrieve statistic information of type from resource group
 */
Datum
ResGroupGetStat(Oid groupId, ResGroupStatType type)
{
	ResGroupData *group;
	Interval   *interval;
	Datum		result;

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
			result = Int64GetDatum(group->totalExecuted);
			break;
		case RES_GROUP_STAT_TOTAL_QUEUED:
			result = Int64GetDatum(group->totalQueued);
			break;
		case RES_GROUP_STAT_TOTAL_QUEUE_TIME:
			/*
			 * Turn milliseconds in totalQueuedTimeMs into an Interval.
			 *
			 * Note: we only use the 'time' field. The user can call
			 * justify_interval() if she wants.
			 */
			interval = (Interval *) palloc(sizeof(Interval));
			interval->time = group->totalQueuedTimeMs * 1000;
			interval->day = 0;
			interval->month = 0;
			result = IntervalPGetDatum(interval);
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
ResGroupGetHostPrimaryCount()
{
	return (Gp_role == GP_ROLE_EXECUTE ? host_primary_segment_count : pResGroupControl->segmentsOnMaster);
}

/*
 * removeGroup -- remove resource group from share memory and
 * reclaim the group's memory back to MEM POOL.
 */
static void
removeGroup(Oid groupId)
{
	ResGroupData *group;

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(OidIsValid(groupId));

	group = groupHashRemove(groupId);

	group->groupId = InvalidOid;
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
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
	group->totalQueuedTimeMs = 0;
	group->lockedForDrop = false;

	return group;
}

/*
 * Attach a process (QD or QE) to a slot.
 */
static void
selfAttachResGroup(ResGroupData *group, ResGroupSlotData *slot)
{
	selfSetGroup(group);
	selfSetSlot(slot);

	pg_atomic_add_fetch_u32((pg_atomic_uint32*) &slot->nProcs, 1);
}

/*
 * Detach a process (QD or QE) from a slot.
 */
static void
selfDetachResGroup(ResGroupData *group, ResGroupSlotData *slot)
{
	pg_atomic_sub_fetch_u32((pg_atomic_uint32*) &slot->nProcs, 1);
	selfUnsetSlot();
	selfUnsetGroup();
}

/*
 * Initialize the members of a slot
 */
static void
initSlot(ResGroupSlotData *slot, ResGroupData *group)
{
	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(!slotIsInUse(slot));
	Assert(group->groupId != InvalidOid);

	slot->group = group;
	slot->groupId = group->groupId;
	slot->caps = group->caps;
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
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
	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(slotIsInUse(slot));
	Assert(slot->nProcs == 0);

	slot->group = NULL;
	slot->groupId = InvalidOid;

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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(groupIsNotDropped(group));

	caps = &group->caps;

	/* First check if the concurrency limit is reached */
	if (group->nRunning >= caps->concurrency)
		return NULL;

	/* Now actually get a free slot */
	slot = slotpoolAllocSlot();
	Assert(!slotIsInUse(slot));

	initSlot(slot, group);

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
	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(slotIsInUse(slot));

	/* Return the slot back to free list */
	slotpoolFreeSlot(slot);
	group->nRunning--;
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
groupAcquireSlot(ResGroupInfo *pGroupInfo, bool isMoveQuery)
{
	ResGroupSlotData *slot;
	ResGroupData	 *group;

	Assert(!selfIsAssigned() || isMoveQuery);
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
			pgstat_report_resgroup(group->groupId);
			return slot;
		}
	}

	/*
	 * Add into group wait queue (if not waiting yet).
	 */
	Assert(!proc_exit_inprogress);
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
	waitOnGroup(group, isMoveQuery);

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

	pgstat_report_resgroup(group->groupId);
	return slot;
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
	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));

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

/* Update the total queued time of this group */
static void
addTotalQueueDuration(ResGroupData *group)
{
	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	if (group == NULL)
		return;

	group->totalQueuedTimeMs += (groupWaitEnd - groupWaitEnd);
}

/*
 * Release the resource group slot
 *
 * Call this function at the end of the transaction.
 */
static void
groupReleaseSlot(ResGroupData *group, ResGroupSlotData *slot, bool isMoveQuery)
{
	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(!selfIsAssigned() || isMoveQuery);

	groupPutSlot(group, slot);

	/*
	 * We should wake up other pending queries on master nodes.
	 */
	if (IS_QUERY_DISPATCHER())
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
	unsigned int cpuset_len;
	int32		itmp;
	ResGroupCaps empty_caps;
	ResGroupCaps *caps;

	if (selfIsAssigned())
		caps = &self->caps;
	else
	{
		ClearResGroupCaps(&empty_caps);
		caps = &empty_caps;
	}

	itmp = htonl(self->groupId);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(int32));

	itmp = htonl(caps->concurrency);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(int32));
	itmp = htonl(caps->cpuHardQuotaLimit);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(int32));
	itmp = htonl(caps->cpuSoftPriority);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(int32));
	itmp = htonl(caps->memory_limit);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(int32));

	cpuset_len = strlen(caps->cpuset);
	itmp = htonl(cpuset_len);
	appendBinaryStringInfo(str, (char *) &itmp, sizeof(int32));
	appendBinaryStringInfo(str, caps->cpuset, cpuset_len);

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
	int32		itmp;
	unsigned int cpuset_len;
	const char	*ptr = buf;

	Assert(len > 0);

	ClearResGroupCaps(capsOut);

	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	*groupId = ntohl(itmp);

	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	capsOut->concurrency = ntohl(itmp);
	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	capsOut->cpuHardQuotaLimit = ntohl(itmp);
	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	capsOut->cpuSoftPriority = ntohl(itmp);
	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	capsOut->memory_limit = ntohl(itmp);


	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	cpuset_len = ntohl(itmp);
	if (cpuset_len >= sizeof(capsOut->cpuset))
		elog(ERROR, "malformed serialized resource group info");
	memcpy(capsOut->cpuset, ptr, cpuset_len); ptr += cpuset_len;
	capsOut->cpuset[cpuset_len] = '\0';

	memcpy(&itmp, ptr, sizeof(int32)); ptr += sizeof(int32);
	bypassedSlot.groupId = ntohl(itmp);

	Assert(len == ptr - buf);
}

/*
 * Check whether resource group should be assigned on master.
 */
bool
ShouldAssignResGroupOnMaster(void)
{
	/*
	 * Bypass resource group when it's waiting for a resource group slot. e.g.
	 * MyProc was interrupted by SIGTERM while waiting for resource group slot.
	 * Some callbacks - RemoveTempRelationsCallback for example - open new
	 * transactions on proc exit. It can cause a double add of MyProc to the
	 * waiting queue (and its corruption).
	 *
	 * Also bypass resource group when it's exiting.
	 */
	return IsResGroupActivated() &&
		IsNormalProcessingMode() &&
		Gp_role == GP_ROLE_DISPATCH &&
		!proc_exit_inprogress &&
		!procIsWaiting(MyProc);
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
		(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE);
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
		 * If it's the first query in the connection (make sure tab completion
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
		pgstat_report_resgroup(bypassedGroup->groupId);

		/* Initialize the fake slot */
		bypassedSlot.group = groupInfo.group;
		bypassedSlot.groupId = groupInfo.groupId;

		/* Add into cgroup */
		cgroupOpsRoutine->attachcgroup(bypassedGroup->groupId, MyProcPid,
									   bypassedGroup->caps.cpuHardQuotaLimit == CPU_HARD_QUOTA_LIMIT_DISABLED);

		return;
	}

	PG_TRY();
	{
		do {
			decideResGroup(&groupInfo);

			/* Acquire slot */
			slot = groupAcquireSlot(&groupInfo, false);
		} while (slot == NULL);

		/* Set resource group slot for current session */
		sessionSetSlot(slot);

		selfAttachResGroup(groupInfo.group, slot);

		/* Init self */
		self->caps = slot->caps;

		/* Don't error out before this line in this function */
		SIMPLE_FAULT_INJECTOR("resgroup_assigned_on_master");

		/* Add into cgroup */
		cgroupOpsRoutine->attachcgroup(self->groupId, MyProcPid,
									   self->caps.cpuHardQuotaLimit == CPU_HARD_QUOTA_LIMIT_DISABLED);
	}
	PG_CATCH();
	{
		UnassignResGroup(false);
		PG_RE_THROW();
	}
	PG_END_TRY();
}

/*
 * Detach from a resource group at the end of the transaction.
 */
void
UnassignResGroup(bool releaseSlot)
{
	ResGroupData		*group = self->group;
	ResGroupSlotData	*slot = self->slot;

	if (bypassedGroup)
	{
		/* bypass mode ref count is only maintained on qd */
		if (Gp_role == GP_ROLE_DISPATCH)
			groupDecBypassedRef(bypassedGroup);

		/* Reset the fake slot */
		bypassedSlot.group = NULL;
		bypassedSlot.groupId = InvalidOid;
		bypassedGroup = NULL;

		/* Update pg_stat_activity statistics */
		pgstat_report_resgroup(InvalidOid);
		return;
	}

	if (Gp_role == GP_ROLE_EXECUTE && IS_QUERY_DISPATCHER())
		SIMPLE_FAULT_INJECTOR("unassign_resgroup_start_entrydb");

	if (!selfIsAssigned())
		return;

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);

	/* Sub proc memory accounting info from group and slot */
	selfDetachResGroup(group, slot);

	/* Release the slot if no reference. */
	if (slot->nProcs == 0 || releaseSlot)
	{
		if (releaseSlot)
		{
			slot->nProcs = 0;
		}

		groupReleaseSlot(group, slot, false);

		/*
		 * Reset resource group slot for current session. Note MySessionState
		 * could be reset as NULL in shmem_exit() before.
		 */
		sessionResetSlot();
	}

	LWLockRelease(ResGroupLock);

	if (Gp_role == GP_ROLE_DISPATCH)
		SIMPLE_FAULT_INJECTOR("unassign_resgroup_end_qd");

	pgstat_report_resgroup(InvalidOid);
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

		return;
	}

	if (newGroupId == InvalidOid)
	{
		UnassignResGroup(false);
		return;
	}

	if (self->groupId != InvalidOid)
	{
		/* it's not the first dispatch in the same transaction */
		Assert(self->groupId == newGroupId);
		Assert(self->caps.concurrency == caps.concurrency);
		Assert(self->caps.cpuHardQuotaLimit == caps.cpuHardQuotaLimit);
		Assert(!strcmp(self->caps.cpuset, caps.cpuset));
		return;
	}

	LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
	group = groupHashFind(newGroupId, true);
	Assert(group != NULL);

	/* Init self */
	Assert(host_primary_segment_count > 0);
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
		initSlot(slot, group);
		group->nRunning++;
	}

	selfAttachResGroup(group, slot);

	LWLockRelease(ResGroupLock);

	/* finally, we can say we are in a valid resgroup */
	Assert(selfIsAssigned());

	/* Add into cgroup */
	cgroupOpsRoutine->attachcgroup(self->groupId, MyProcPid,
								   self->caps.cpuHardQuotaLimit == CPU_HARD_QUOTA_LIMIT_DISABLED);
}

/*
 * Wait on the queue of resource group
 */
static void
waitOnGroup(ResGroupData *group, bool isMoveQuery)
{
	int64 timeout = -1;
	int64 curTime;
	const char *old_status;
	char *new_status = NULL;
	int len;
	PGPROC *proc = MyProc;
	const char *queueStr = " queuing";

	Assert(!LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(!selfIsAssigned() || isMoveQuery);

	/* set ps status to waiting */
	if (update_process_title)
	{
		old_status = get_real_act_ps_display(&len);
		new_status = (char *) palloc(len + strlen(queueStr) + 1);
		memcpy(new_status, old_status, len);
		strcpy(new_status + len, queueStr);
		set_ps_display(new_status, false);
		/* truncate off " queuing" */
		new_status[len] = '\0';
	}

	/*
	 * The low bits of 'wait_event_info' argument to WaitLatch are
	 * not enough to store a full Oid, so we set groupId out-of-band,
	 * via the backend entry.
	 */
	pgstat_report_resgroup(group->groupId);

	/*
	 * Mark that we are waiting on resource group
	 *
	 * This is used for interrupt cleanup, similar to lockAwaited in ProcSleep
	 */
	groupAwaited = group;
	groupWaitStart = GetCurrentTimestamp();

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

			if (gp_resource_group_queuing_timeout > 0)
			{
				curTime = GetCurrentTimestamp();
				timeout = gp_resource_group_queuing_timeout - (curTime - groupWaitStart) / 1000;
				if (timeout < 0)
					ereport(ERROR,
							(errcode(ERRCODE_QUERY_CANCELED),
							 errmsg("canceling statement due to resource group waiting timeout")));

				WaitLatch(&proc->procLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
						  (long) timeout, PG_WAIT_RESOURCE_GROUP);
			}
			else
			{
				WaitLatch(&proc->procLatch, WL_LATCH_SET | WL_POSTMASTER_DEATH, -1,
						  PG_WAIT_RESOURCE_GROUP);
			}
		}
	}
	PG_CATCH();
	{
		/* reset ps status */
		if (update_process_title)
		{
			set_ps_display(new_status, false);
			pfree(new_status);
		}

		groupWaitCancel(false);
		PG_RE_THROW();
	}
	PG_END_TRY();

	groupAwaited = NULL;

	/* reset ps status */
	if (update_process_title)
	{
		set_ps_display(new_status, false);
		pfree(new_status);
	}
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(groupId != InvalidOid);

	for (i = 0; i < pResGroupControl->nGroups; i++)
	{
		if (pResGroupControl->groups[i].groupId == InvalidOid)
			break;
	}
	Assert(i < pResGroupControl->nGroups);

	entry = (ResGroupHashEntry *)
		hash_search(pResGroupControl->htbl, (void *) &groupId, HASH_ENTER, &found);
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));

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
	groupWaitCancel(false);
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
groupWaitCancel(bool isMoveQuery)
{
	ResGroupData		*group;
	ResGroupSlotData	*slot;

	/* Nothing to do if we weren't waiting on a group */
	if (groupAwaited == NULL)
		return;

	pgstat_report_wait_end();
	groupWaitEnd = GetCurrentTimestamp();

	Assert(!selfIsAssigned() || isMoveQuery);

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
		groupReleaseSlot(group, slot, false);
		/*
		 * Reset resource group slot for current session. Note MySessionState
		 * could be reset as NULL in shmem_exit() before.
		 */
		sessionResetSlot();

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
}

/*
 * Validate the consistency of the resgroup information in self.
 *
 * This function checks the consistency of (group & groupId).
 */
static void
selfValidateResGroupInfo(void)
{
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
	/*------
	 * The typical asm instructions fow below C operation can be like this:
	 * ( gcc 4.8.5-11, x86_64-redhat-linux, -O0 )
	 *
     *     mov    -0x8(%rbp),%rax           ; load proc
     *     mov    0x8(%rax),%rax            ; load proc->links.next
     *     cmp    $0,%rax                   ; compare with NULL
     *     setne  %al                       ; store the result
	 *
	 * The operation is atomic, so a lock is not required here.
	 *------
	 */
	return proc->links.next != NULL;
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
	if (group->lockedForDrop)
		return;

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(group->nRunning == 0);
	Assert(group->nRunningBypassed == 0);
	group->lockedForDrop = true;
}

static void
unlockResGroupForDrop(ResGroupData *group)
{
	if (!group->lockedForDrop)
		return;

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));

	waitQueue = &group->waitProcs;

	if (gp_resgroup_debug_wait_queue)
	{
		if (waitQueue->size == 0)
		{
			if (waitQueue->links.next != &waitQueue->links ||
				waitQueue->links.prev != &waitQueue->links)
				elog(PANIC, "resource group wait queue is corrupted");
		}
		else
		{
			PGPROC *nextProc = (PGPROC *)waitQueue->links.next;
			PGPROC *prevProc = (PGPROC *)waitQueue->links.prev;

			if (!nextProc->mppIsWriter ||
				!prevProc->mppIsWriter ||
				nextProc->links.prev != &waitQueue->links ||
				prevProc->links.next != &waitQueue->links)
				elog(PANIC, "resource group wait queue is corrupted");
		}

		return;
	}

	AssertImply(waitQueue->size == 0,
				waitQueue->links.next == &waitQueue->links &&
				waitQueue->links.prev == &waitQueue->links);
}

static void
groupWaitProcValidate(PGPROC *proc, PROC_QUEUE *head)
{
	PGPROC *nextProc = (PGPROC *)proc->links.next;
	PGPROC *prevProc = (PGPROC *)proc->links.prev;

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));

	if (!gp_resgroup_debug_wait_queue)
		return;

	if (!proc->mppIsWriter ||
		((PROC_QUEUE *)nextProc != head && !nextProc->mppIsWriter) ||
		((PROC_QUEUE *)prevProc != head && !prevProc->mppIsWriter) ||
		nextProc->links.prev != &proc->links ||
		prevProc->links.next != &proc->links)
		elog(PANIC, "resource group wait queue is corrupted");

	return;
}

/*
 * Push a proc to the resgroup wait queue.
 */
static void
groupWaitQueuePush(ResGroupData *group, PGPROC *proc)
{
	PROC_QUEUE			*waitQueue;
	PGPROC				*headProc;

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(!procIsWaiting(proc));
	Assert(proc->resSlot == NULL);

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;
	headProc = (PGPROC *) &waitQueue->links;

	SHMQueueInsertBefore(&headProc->links, &proc->links);
	groupWaitProcValidate(proc, waitQueue);

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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(!groupWaitQueueIsEmpty(group));

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	proc = (PGPROC *) waitQueue->links.next;
	groupWaitProcValidate(proc, waitQueue);
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));
	Assert(!groupWaitQueueIsEmpty(group));
	Assert(groupWaitQueueFind(group, proc));
	Assert(proc->resSlot == NULL);

	groupWaitQueueValidate(group);

	waitQueue = &group->waitProcs;

	groupWaitProcValidate(proc, waitQueue);
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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));

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

	Assert(LWLockHeldByMeInMode(ResGroupLock, LW_EXCLUSIVE));

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
 * Walk through the raw expression tree, if there is a RangeVar without
 * `pg_catalog` prefix, terminate the process immediately to save the cpu
 * resource.
 */
static bool
checkBypassWalker(Node *node, void *context)
{
	bool *bypass = context;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar *from = (RangeVar *) node;
		if (from->schemaname == NULL ||
			strcmp(from->schemaname, "pg_catalog") != 0)
		{
			*bypass = false;
			return true;
		}
		else
		{
			/*
			 * Make sure there is at least one RangeVar
			 */
			*bypass = true;
		}
	}

	return raw_expression_tree_walker(node, checkBypassWalker, context);
}

static bool
shouldBypassSelectQuery(Node *node)
{
	bool catalog_bypass = false;

	if (gp_resource_group_bypass_catalog_query)
		raw_expression_tree_walker(node, checkBypassWalker, &catalog_bypass);

	return catalog_bypass;
}

/*
 * Parse the query and check if this query should
 * bypass the management of resource group.
 *
 * Currently, only SET/RESET/SHOW command and SELECT with only catalog tables
 * can be bypassed
 */
static bool
shouldBypassQuery(const char *query_string)
{
	MemoryContext oldcontext = NULL;
	MemoryContext tmpcontext = NULL;
	List *parsetree_list; 
	ListCell *parsetree_item;
	Node *parsetree;
	bool		bypass;

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

	/* Only bypass SET/RESET/SHOW command and SELECT with only catalog tables
	 * for now */
	bypass = true;
	foreach(parsetree_item, parsetree_list)
	{
		parsetree = (Node *) lfirst(parsetree_item);

		if (nodeTag(parsetree) == T_RawStmt)
			parsetree = ((RawStmt *)parsetree)->stmt;

		if (IsA(parsetree, SelectStmt))
		{
			if (!shouldBypassSelectQuery(parsetree))
			{
				bypass = false;
				break;
			}
		}
		else if (nodeTag(parsetree) != T_VariableSetStmt &&
			nodeTag(parsetree) != T_VariableShowStmt)
		{
			bypass = false;
			break;
		}
	}

	list_free_deep(parsetree_list);

	if (tmpcontext)
		MemoryContextDelete(tmpcontext);

	return bypass;
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

	appendStringInfo(str, "{\"segid\":%d,", GpIdentity.segindex);
	/* dump fields in pResGroupControl. */
	appendStringInfo(str, "\"segmentsOnMaster\":%d,", pResGroupControl->segmentsOnMaster);
	appendStringInfo(str, "\"loaded\":%s,", pResGroupControl->loaded ? "true" : "false");
	
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

	if (!ShmemAddrIsValid(&proc->links))
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

	/*
	 * SessionStateLock is required since runaway detector will traverse
	 * the current session array and check corresponding resGroupSlot with
	 * shared lock on SessionStateLock.
	 */
	LWLockAcquire(SessionStateLock, LW_EXCLUSIVE);

	MySessionState->resGroupSlot = (void *) slot;

	LWLockRelease(SessionStateLock);
}

/*
 * Reset resource group slot for current session to NULL.
 */
static void
sessionResetSlot(void)
{
	/*
	 * SessionStateLock is required since runaway detector will traverse
	 * the current session array and check corresponding resGroupSlot with
	 * shared lock on SessionStateLock.
	 */
	if (MySessionState != NULL)
	{
		LWLockAcquire(SessionStateLock, LW_EXCLUSIVE);

		MySessionState->resGroupSlot = NULL;

		LWLockRelease(SessionStateLock);
	}
}

/*
 * Get resource group slot of current session.
 */
static ResGroupSlotData *
sessionGetSlot(void)
{
	if (MySessionState == NULL)
		return NULL;
	else
		return (ResGroupSlotData *) MySessionState->resGroupSlot;
}

/*
 * Parse cpuset to bitset
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

	num = -1;
	while ((num = bms_next_member(bms, num)) >= 0)
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
		cgroupOpsRoutine->getcpuset(CGROUP_ROOT_ID, cpuset, MaxCpuSetLength);
		Bitmapset *bmsDefault = CpusetToBitset(cpuset, MaxCpuSetLength);
		/* get the minimum core number, in case of the zero core is not exist */
		defaultCore = bms_next_member(bmsDefault, -1);
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
 * move a proc to a resource group
 */
void
HandleMoveResourceGroup(void)
{
	ResGroupSlotData *slot;
	ResGroupData *group;
	ResGroupData *oldGroup;

	/* transaction has finished */
	if (!selfIsAssigned())
		return;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		slot = (ResGroupSlotData *)MyProc->movetoResSlot;
		group = slot->group;
		MyProc->movetoResSlot = NULL;

		/* unassign the old resource group and release the old slot */
		UnassignResGroup(true);

		PG_TRY();
		{
			sessionSetSlot(slot);

			selfAttachResGroup(group, slot);

			/* Init self */
			self->caps = slot->caps;

			/* Add into cgroup */
			cgroupOpsRoutine->attachcgroup(self->groupId, MyProcPid,
										   self->caps.cpuHardQuotaLimit == CPU_HARD_QUOTA_LIMIT_DISABLED);
		}
		PG_CATCH();
		{
			UnassignResGroup(false);
			PG_RE_THROW();
		}
		PG_END_TRY();
		pgstat_report_resgroup(self->groupId);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		Oid groupId = MyProc->movetoGroupId;
		MyProc->movetoGroupId = InvalidOid;

		slot = sessionGetSlot();
		Assert(slot != NULL);

		selfUnsetSlot();
		selfUnsetGroup();

		LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
		group = groupHashFind(groupId, true);
		oldGroup = slot->group;
		Assert(group != NULL);
		Assert(oldGroup != NULL);

		if (slot->groupId != groupId)
		{
			oldGroup->nRunning--;

			/* reset the slot but don't touch the 'memUsage' */
			slot->groupId = groupId;
			slot->group = group;
			slot->caps = group->caps;

			group->nRunning++;
		}

		if (IS_QUERY_DISPATCHER())
			selfAttachResGroup(group, slot);

		LWLockRelease(ResGroupLock);

		selfSetGroup(group);
		selfSetSlot(slot);
		self->caps = group->caps;

		/* finally we can say we are in a valid resgroup */
		Assert(selfIsAssigned());

		/* Add into cgroup */
		cgroupOpsRoutine->attachcgroup(self->groupId, MyProcPid,
									   self->caps.cpuHardQuotaLimit == CPU_HARD_QUOTA_LIMIT_DISABLED);
	}
}

void
ResGroupMoveQuery(int sessionId, Oid groupId, const char *groupName)
{
	ResGroupInfo groupInfo;
	ResGroupData *group;
	ResGroupSlotData *slot;
	char *cmd;

	Assert(pResGroupControl != NULL);
	Assert(pResGroupControl->segmentsOnMaster > 0);
	Assert(Gp_role == GP_ROLE_DISPATCH);

	LWLockAcquire(ResGroupLock, LW_SHARED);
	group = groupHashFind(groupId, false);
	if (!group)
	{
		LWLockRelease(ResGroupLock);
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 (errmsg("invalid resource group id: %d", groupId))));
	}
	LWLockRelease(ResGroupLock);

	groupInfo.group = group;
	groupInfo.groupId = groupId;
	slot = groupAcquireSlot(&groupInfo, true);
	if (slot == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 (errmsg("cannot get slot in resource group %d", groupId))));

	PG_TRY();
	{
		ResGroupSignalMoveQuery(sessionId, slot, groupId);

		cmd = psprintf("SELECT pg_resgroup_move_query(%d, %s)",
				sessionId,
				quote_literal_cstr(groupName));
		CdbDispatchCommand(cmd, 0, NULL);
	}
	PG_CATCH();
	{
		LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
		groupReleaseSlot(group, slot, true);
		LWLockRelease(ResGroupLock);
		PG_RE_THROW();
	}
	PG_END_TRY();
}
/*
 * get resource group id by session id
 */
Oid
ResGroupGetGroupIdBySessionId(int sessionId)
{
	Oid groupId = InvalidOid;
	SessionState *curSessionState;

	LWLockAcquire(SessionStateLock, LW_SHARED);
	curSessionState = AllSessionStateEntries->usedList;
	while (curSessionState != NULL)
	{
		if (curSessionState->sessionId == sessionId)
		{
			ResGroupSlotData *slot = (ResGroupSlotData *)curSessionState->resGroupSlot;
			if (slot != NULL)
				groupId = slot->groupId;
			break;
		}
		curSessionState = curSessionState->next;
	}
	LWLockRelease(SessionStateLock);

	return groupId;
}

/*
 * In resource group mode, how much memory should a query take in bytes.
 */
uint64
ResourceGroupGetQueryMemoryLimit(void)
{
	ResGroupCaps		*caps;
	int64	resgLimit = -1;
	uint64	queryMem = -1;

	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);

	if (bypassedGroup)
		return 0;

	if (gp_resgroup_memory_query_fixed_mem)
		return (uint64) gp_resgroup_memory_query_fixed_mem * 1024L;

	Assert(selfIsAssigned());

	LWLockAcquire(ResGroupLock, LW_SHARED);

	caps = &self->group->caps;
	resgLimit = caps->memory_limit;

	AssertImply(resgLimit < 0, resgLimit == -1);
	if (resgLimit == -1)
	{
		LWLockRelease(ResGroupLock);
		return (uint64) statement_mem * 1024L;
	}

	queryMem = (uint64)(resgLimit *1024L *1024L / caps->concurrency);
	LWLockRelease(ResGroupLock);

	return queryMem;
}
