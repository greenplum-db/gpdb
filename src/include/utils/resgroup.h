/*-------------------------------------------------------------------------
 *
 * resgroup.h
 *	  GPDB resource group definitions.
 *
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/resgroup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_H
#define RES_GROUP_H

#include "cdb/memquota.h"
#include "catalog/pg_resgroup.h"
#include "utils/session_state.h"
#include "utils/cgroup.h"

/*
 * The max number of resource groups.
 */
#define MaxResourceGroups 100

/*
 * The max length of cpuset
 */
#define MaxCpuSetLength 1024

/*
 * Default value of cpuset
 */
#define DefaultCpuset "-1"

/*
 * Default value of cpu soft priority
 */
#define DefaultCPUSoftPriority 100

/*
 * Resource group capability.
 */
typedef int32 ResGroupCap;

/*
 * Resource group capabilities.
 *
 * These are usually a snapshot of the pg_resgroupcapability table
 * for a resource group.
 *
 * The properties must be in the same order as ResGroupLimitType.
 *
 * This struct can also be converted to an array of ResGroupCap so the fields
 * can be accessed via index and iterated with loop.
 *
 *     ResGroupCaps caps;
 *     ResGroupCap *array = (ResGroupCap *) &caps;
 *     caps.concurrency.value = 1;
 *     array[RESGROUP_LIMIT_TYPE_CONCURRENCY] = 2;
 *     Assert(caps.concurrency.value == 2);
 */
typedef struct ResGroupCaps
{
	ResGroupCap		__unknown;			/* placeholder, do not use it */
	ResGroupCap		concurrency;
	ResGroupCap		cpuHardQuotaLimit;
	ResGroupCap		cpuSoftPriority;
	ResGroupCap		memory_limit;
	char			cpuset[MaxCpuSetLength];
} ResGroupCaps;

/* Set 'cpuset' to an empty string, and reset all other fields to zero */
#define ClearResGroupCaps(caps) do { \
	MemSet((caps), 0, offsetof(ResGroupCaps, cpuset) + 1); \
} while(0)


/*
 * GUC variables.
 */
extern int						gp_resgroup_memory_policy;
extern bool						gp_log_resgroup_memory;
extern int						gp_resgroup_memory_query_fixed_mem;
extern int						gp_resgroup_memory_policy_auto_fixed_mem;
extern bool						gp_resgroup_print_operator_memory_limits;
extern bool						gp_resgroup_debug_wait_queue;

extern int gp_resource_group_cpu_priority;
extern double gp_resource_group_cpu_limit;
extern bool gp_resource_group_bypass;
extern int gp_resource_group_queuing_timeout;
extern bool gp_resource_group_bypass_catalog_query;

/*
 * Non-GUC global variables.
 */
extern bool gp_resource_group_enable_cgroup_cpuset;

/*
 * Resource Group assignment hook.
 *
 * This hook can be set by an extension to control how queries are assigned to
 * a resource group.
 */
typedef Oid (*resgroup_assign_hook_type)(void);
extern PGDLLIMPORT resgroup_assign_hook_type resgroup_assign_hook;

/* Type of statistic information */
typedef enum
{
	RES_GROUP_STAT_UNKNOWN = -1,

	RES_GROUP_STAT_NRUNNING = 0,
	RES_GROUP_STAT_NQUEUEING,
	RES_GROUP_STAT_TOTAL_EXECUTED,
	RES_GROUP_STAT_TOTAL_QUEUED,
	RES_GROUP_STAT_TOTAL_QUEUE_TIME,
	RES_GROUP_STAT_CPU_USAGE,
} ResGroupStatType;

/*
 * The context to pass to callback in CREATE/ALTER/DROP resource group
 */
typedef struct
{
	Oid					groupid;
	ResGroupLimitType	limittype;
	ResGroupCaps		caps;
	ResGroupCaps		oldCaps;	/* last config value, alter operation need to
 										* check last config for recycling */
} ResourceGroupCallbackContext;

/* Shared memory and semaphores */
extern Size ResGroupShmemSize(void);
extern void ResGroupControlInit(void);
extern void CGroupOpsAndInfoInit(void);

extern void initCgroup(void);
/* Load resource group information from catalog */
extern void	InitResGroups(void);

extern void AllocResGroupEntry(Oid groupId, const ResGroupCaps *caps);

extern void SerializeResGroupInfo(StringInfo str);
extern void DeserializeResGroupInfo(struct ResGroupCaps *capsOut,
									Oid *groupId,
									const char *buf,
									int len);

extern bool ShouldAssignResGroupOnMaster(void);
extern bool ShouldUnassignResGroup(void);
extern void AssignResGroupOnMaster(void);
extern void UnassignResGroup(bool releaseSlot);
extern void SwitchResGroupOnSegment(const char *buf, int len);

extern bool ResGroupIsAssigned(void);

/* Retrieve statistic information of type from resource group */
extern Datum ResGroupGetStat(Oid groupId, ResGroupStatType type);

extern void ResGroupDropFinish(const ResourceGroupCallbackContext *callbackCtx,
							   bool isCommit);
extern void ResGroupCreateOnAbort(const ResourceGroupCallbackContext *callbackCtx);
extern void ResGroupAlterOnCommit(const ResourceGroupCallbackContext *callbackCtx);
extern void ResGroupCheckForDrop(Oid groupId, char *name);
extern uint64 ResourceGroupGetQueryMemoryLimit(void);

/*
 * Get resource group id of my proc.
 *
 * This function is not dead code although there is no consumer in the gpdb
 * code tree.  Some extensions require this to get the internal resource group
 * information.
 */
extern Oid GetMyResGroupId(void);

extern void ResGroupDumpInfo(StringInfo str);

extern int ResGroupGetHostPrimaryCount(void);

extern Bitmapset *CpusetToBitset(const char *cpuset,
								 int len);
extern void BitsetToCpuset(const Bitmapset *bms,
							char *cpuset,
							int cpusetSize);
extern void CpusetUnion(char *cpuset1, const char *cpuset2, int len);
extern void CpusetDifference(char *cpuset1, const char *cpuset2, int len);
extern bool CpusetIsEmpty(const char *cpuset);
extern void SetCpusetEmpty(char *cpuset, int cpusetSize);
extern bool EnsureCpusetIsAvailable(int elevel);
extern Oid SessionGetResGroupId(SessionState *session);
extern void HandleMoveResourceGroup(void);
extern void ResGroupMoveQuery(int sessionId, Oid groupId, const char *groupName);
extern Oid ResGroupGetGroupIdBySessionId(int sessionId);
extern char *getCpuSetByRole(const char *cpuset);
extern void checkCpuSetByRole(const char *cpuset);

#define LOG_RESGROUP_DEBUG(...) \
	do {if (Debug_resource_group) elog(__VA_ARGS__); } while(false);

#endif   /* RES_GROUP_H */
