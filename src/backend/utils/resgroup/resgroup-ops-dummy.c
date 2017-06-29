/*-------------------------------------------------------------------------
 *
 * resgroup-ops-dummy.c
 *	  OS dependent resource group operations - dummy implementation
 *
 *
 * Copyright (c) 2017, Pivotal Software Inc.
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/resgroup-ops.h"

/*
 * Interfaces for OS dependent operations.
 *
 * Resource group relies on OS dependent group implementation to manage
 * resources like cpu usage, such as cgroup on Linux system.
 * We call it OS group in below function description.
 *
 * So far these operations are mainly for CPU rate limitation and accounting.
 */

#define unsupported_system() \
	elog(WARNING, "cpu rate limitation for resource group is unsupported on this system")

/* Return the name for the OS group implementation */
const char *
ResGroupOps_Name(void)
{
	return "unsupported";
}

/* Check whether the OS group implementation is available and useable */
void
ResGroupOps_Bless(void)
{
	unsupported_system();
}

/* Initialize the OS group */
void
ResGroupOps_Init(void)
{
	unsupported_system();
}

/* Adjust GUCs for this OS group implementation */
void
ResGroupOps_AdjustGUCs(void)
{
	unsupported_system();
}

/*
 * Create the OS group for group.
 */
void
ResGroupOps_CreateGroup(Oid group)
{
	unsupported_system();
}

/*
 * Destroy the OS group for group.
 *
 * Fail if any process is running under it.
 */
void
ResGroupOps_DestroyGroup(Oid group)
{
	unsupported_system();
}

/*
 * Assign a process to the OS group. A process can only be assigned to one
 * OS group, if it's already running under other OS group then it'll be moved
 * out that OS group.
 *
 * pid is the process id.
 */
void
ResGroupOps_AssignGroup(Oid group, int pid)
{
	unsupported_system();
}

/*
 * Lock the OS group. While the group is locked it won't be removed by other
 * processes.
 *
 * This function would block if block is true, otherwise it return with -1
 * immediately.
 *
 * On success it return a fd to the OS group, pass it to
 * ResGroupOps_UnLockGroup() to unblock it.
 */
int
ResGroupOps_LockGroup(Oid group, bool block)
{
	unsupported_system();
	return -1;
}

/*
 * Unblock a OS group.
 *
 * fd is the value returned by ResGroupOps_LockGroup().
 */
void
ResGroupOps_UnLockGroup(Oid group, int fd)
{
	unsupported_system();
}

/*
 * Set the cpu rate limit for the OS group.
 *
 * cpu_rate_limit should be within (0.0, 1.0].
 */
void
ResGroupOps_SetCpuRateLimit(Oid group, float cpu_rate_limit)
{
	unsupported_system();
}

/*
 * Get the cpu usage of the OS group, that is the total cpu time obtained
 * by this OS group, in nano seconds.
 */
int64
ResGroupOps_GetCpuUsage(Oid group)
{
	unsupported_system();
	return 0;
}

/*
 * Get the count of cpu cores on the system.
 */
int
ResGroupOps_GetCpuCores(void)
{
	unsupported_system();
	return 1;
}

/*
 * Get the total memory on the system.
 * (total RAM * overcommit_ratio + total Swap)
 */
int
ResGroupOps_GetTotalMemory(void)
{
	unsupported_system();
	return 0;
}
