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

/* Acquire and release resource group slot */
extern void ResGroupSlotAcquire(void);
extern void ResGroupSlotRelease(void);

#define LOG_RESGROUP_DEBUG(...) \
	do {if (Debug_resource_group) elog(__VA_ARGS__); } while(false);

#endif   /* RES_GROUP_H */
