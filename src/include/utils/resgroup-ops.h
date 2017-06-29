/*-------------------------------------------------------------------------
 *
 * resgroup.h
 *	  GPDB resource group definitions.
 *
 *
 * Copyright (c) 2017, Pivotal Software Inc.
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_OPS_H
#define RES_GROUP_OPS_H

/*
 * Interfaces for OS dependent operations
 */

extern const char * ResGroupOps_Name(void);
extern void ResGroupOps_Bless(void);
extern void ResGroupOps_Init(void);
extern void ResGroupOps_AdjustGUCs(void);
extern void ResGroupOps_CreateGroup(Oid group);
extern void ResGroupOps_DestroyGroup(Oid group);
extern void ResGroupOps_AssignGroup(Oid group, int pid);
extern int ResGroupOps_LockGroup(Oid group, bool block);
extern void ResGroupOps_UnLockGroup(Oid group, int fd);
extern void ResGroupOps_SetCpuRateLimit(Oid group, float cpu_rate_limit);
extern int64 ResGroupOps_GetCpuUsage(Oid group);
extern int ResGroupOps_GetCpuCores(void);
extern int ResGroupOps_GetTotalMemory(void);

#endif   /* RES_GROUP_OPS_H */
