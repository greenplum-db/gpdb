/*-------------------------------------------------------------------------
 *
 * resgroup.h
 *	  GPDB resource group definitions.
 *
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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

/*
 * The max number of resource groups.
 */
#define MaxResourceGroups 100

/*
 * Resource group capability.
 */
typedef struct ResGroupCap
{
	int		value;
	int		proposed;
} ResGroupCap;

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
	ResGroupCap		cpuRateLimit;
	ResGroupCap		memLimit;
	ResGroupCap		memSharedQuota;
	ResGroupCap		memSpillRatio;
} ResGroupCaps;

/*
 * Resource group setting options.
 *
 * These can represent the effective settings of a resource group,
 * or the new settings from ALTER RESOURCE GROUP syntax.
 *
 * The properties must be in the same order as ResGroupLimitType.
 *
 * This struct can also be converted to an array of int32 so the fields
 * can be accessed via index and iterated with loop.
 *
 *     ResGroupOpts opts;
 *     int32 *array = (int32 *) &opts;
 *     opts.concurrency = 1;
 *     array[RESGROUP_LIMIT_TYPE_CONCURRENCY] = 2;
 *     Assert(opts.concurrency == 2);
 */
typedef struct ResGroupOpts
{
	int32			__unknown;			/* placeholder, do not use it */
	int32			concurrency;
	int32			cpuRateLimit;
	int32			memLimit;
	int32			memSharedQuota;
	int32			memSpillRatio;
} ResGroupOpts;

/*
 * GUC variables.
 */
extern char                		*gp_resgroup_memory_policy_str;
extern bool						gp_log_resgroup_memory;
extern int						gp_resgroup_memory_policy_auto_fixed_mem;
extern bool						gp_resgroup_print_operator_memory_limits;
extern int						memory_spill_ratio;

extern int gp_resource_group_cpu_priority;
extern double gp_resource_group_cpu_limit;
extern double gp_resource_group_memory_limit;

/* Type of statistic infomation */
typedef enum
{
	RES_GROUP_STAT_UNKNOWN = -1,

	RES_GROUP_STAT_NRUNNING = 0,
	RES_GROUP_STAT_NQUEUEING,
	RES_GROUP_STAT_TOTAL_EXECUTED,
	RES_GROUP_STAT_TOTAL_QUEUED,
	RES_GROUP_STAT_TOTAL_QUEUE_TIME,
	RES_GROUP_STAT_CPU_USAGE,
	RES_GROUP_STAT_MEM_USAGE,
} ResGroupStatType;

/*
 * Functions in resgroup.c
 */

/* Shared memory and semaphores */
extern Size ResGroupShmemSize(void);
extern void ResGroupControlInit(void);

/* Load resource group information from catalog */
extern void	InitResGroups(void);

extern void AllocResGroupEntry(Oid groupId, const ResGroupOpts *opts);

extern void SerializeResGroupInfo(StringInfo str);
extern void DeserializeResGroupInfo(struct ResGroupCaps *capsOut,
									Oid *groupId,
									int *slotId,
									const char *buf,
									int len);

extern bool ShouldAssignResGroupOnMaster(void);
extern bool ShouldUnassignResGroup(void);
extern void AssignResGroupOnMaster(void);
extern void UnassignResGroup(void);
extern void SwitchResGroupOnSegment(const char *buf, int len);

/* Retrieve statistic information of type from resource group */
extern Datum ResGroupGetStat(Oid groupId, ResGroupStatType type);

extern void ResGroupOptsToCaps(const ResGroupOpts *optsIn, ResGroupCaps *capsOut);
extern void ResGroupCapsToOpts(const ResGroupCaps *capsIn, ResGroupOpts *optsOut);
extern void ResGroupDumpMemoryInfo(void);

/* Check the memory limit of resource group */
extern bool ResGroupReserveMemory(int32 memoryChunks, int32 overuseChunks, bool *waiverUsed);
/* Update the memory usage of resource group */
extern void ResGroupReleaseMemory(int32 memoryChunks);

extern void ResGroupDropFinish(Oid groupId, bool isCommit);
extern void ResGroupCreateOnAbort(Oid groupId);
extern void ResGroupAlterOnCommit(Oid groupId,
								  ResGroupLimitType limittype,
								  const ResGroupCaps *caps);
extern void ResGroupCheckForDrop(Oid groupId, char *name);
extern void ResGroupDecideMemoryCaps(int groupId,
									 ResGroupCaps *caps,
									 const ResGroupOpts *opts);
extern void ResGroupDecideConcurrencyCaps(Oid groupId,
										  ResGroupCaps *caps,
										  const ResGroupOpts *opts);

extern int32 ResGroupGetVmemLimitChunks(void);
extern int32 ResGroupGetVmemChunkSizeInBits(void);
extern int32 ResGroupGetMaxChunksPerQuery(void);

/* test helper function */
extern void ResGroupGetMemInfo(int *memLimit, int *slotQuota, int *sharedQuota);

extern int64 ResourceGroupGetQueryMemoryLimit(void);

extern void ResGroupDumpInfo(StringInfo str);

#define LOG_RESGROUP_DEBUG(...) \
	do {if (Debug_resource_group) elog(__VA_ARGS__); } while(false);

#endif   /* RES_GROUP_H */
