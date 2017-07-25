/*-------------------------------------------------------------------------
 *
 * resgroup.h
 *	  GPDB resource group definitions.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_H
#define RES_GROUP_H

/*
 * GUC variables.
 */
extern int MaxResourceGroups;
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

extern void AllocResGroupEntry(Oid groupId);
extern void FreeResGroupEntry(Oid groupId);

extern void SerializeResGroupInfo(StringInfo str);
extern void DeserializeResGroupInfo(const char *buf, int len);

extern bool ShouldAssignResGroupOnMaster(void);
extern void AssignResGroupOnMaster(void);
extern void UnassignResGroupOnMaster(void);
extern void SwitchResGroupOnSegment(const char *buf, int len);

/* Retrieve statistic information of type from resource group */
extern Datum ResGroupGetStat(Oid groupId, ResGroupStatType type);

extern void ResGroupDumpMemoryInfo(void);

/* Check the memory limit of resource group */
extern bool ResGroupReserveMemory(int32 memoryChunks, int32 overuseChunks, bool *waiverUsed);
/* Update the memory usage of resource group */
extern void ResGroupReleaseMemory(int32 memoryChunks);

extern void ResGroupAlterCheckForWakeup(Oid groupId, int value, int proposed);
extern void ResGroupDropCheckForWakeup(Oid groupId, bool isCommit);
extern void ResGroupCheckForDrop(Oid groupId, char *name);
extern int CalcConcurrencyValue(int groupId, int val, int proposed, int newProposed);

/* test helper function */
extern void ResGroupGetMemInfo(int *memLimit, int *slotQuota, int *sharedQuota);

#define LOG_RESGROUP_DEBUG(...) \
	do {if (Debug_resource_group) elog(__VA_ARGS__); } while(false);

#endif   /* RES_GROUP_H */
