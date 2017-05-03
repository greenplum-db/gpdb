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

extern Oid	CurrentGroupId;

/*
 * Data structures
 */
/* Resource Groups */
typedef struct ResGroupData
{
	Oid			groupId;		/* Id for this group */
	int 		nRunning;		/* number of running trans */
	PROC_QUEUE	waitProcs;
	int			totalExecuted;	/* total number of executed trans */
	int			totalQueued;	/* total number of queued trans	*/
	Interval	totalQueuedTime;/* total queue time */
} ResGroupData;
typedef ResGroupData *ResGroup;

/*
 * The hash table for resource groups in shared memory should only be populated
 * once, so we add a flag here to implement this requirement.
 */
typedef struct ResGroupControl
{
	HTAB			*htbl;
	bool			loaded;
} ResGroupControl;

/* Type of statistic infomation */
typedef enum
{
	RES_GROUP_STAT_NRUNNING = 0,
	RES_GROUP_STAT_NQUEUEING,
	RES_GROUP_STAT_TOTAL_EXECUTED,
	RES_GROUP_STAT_TOTAL_QUEUED,
	RES_GROUP_STAT_TOTAL_QUEUE_TIME,
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
extern void FreeResGroupEntry(Oid groupId, char *name);

/* Acquire and release resource group slot */
extern void ResGroupSlotAcquire(void);
extern void ResGroupSlotRelease(void);

/* Retrieve statistic information of type from resource group */
extern void ResGroupGetStat(Oid groupId, ResGroupStatType type, char *retStr, int retStrLen);

extern void ResGroupAlterCheckForWakeup(Oid groupId);
extern int CalcConcurrencyValue(int groupId, int val, int proposed, int newProposed);

#define LOG_RESGROUP_DEBUG(...) \
	do {if (Debug_resource_group) elog(__VA_ARGS__); } while(false);

#endif   /* RES_GROUP_H */
