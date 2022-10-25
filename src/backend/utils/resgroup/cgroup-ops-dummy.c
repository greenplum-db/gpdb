/*-------------------------------------------------------------------------
 *
 * cgroup-ops-dummy.c
 *	  OS dependent resource group operations - dummy implementation
 *
 * Copyright (c) 2017 VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resgroup/cgroup-ops-dummy.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/cgroup.h"
#include "utils/cgroup-ops-dummy.h"

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
	elog(WARNING, "resource group is not supported on this system")

extern CGroupOpsRoutine *cGroupOpsRoutine;

/* Return the name for the OS group implementation */
static const char *
getcgroupname_dummy(void)
{
	return "unsupported";
}

/*
 * Probe the configuration for the OS group implementation.
 *
 * Return true if everything is OK, or false is some requirements are not
 * satisfied.  Will not fail in either case.
 */
static bool
probecgroup_dummy(void)
{
	unsupported_system();
	return false;
}

/* Check whether the OS group implementation is available and useable */
static void
checkcgroup_dummy(void)
{
	unsupported_system();
}

/* Initialize the OS group */
static void
initcgroup_dummy(void)
{
	unsupported_system();
}

/* Adjust GUCs for this OS group implementation */
static void
adjustgucs_dummy(void)
{
	unsupported_system();
}

/*
 * Create the OS group for group.
 */
static void
createcgroup_dummy(Oid group)
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
static void
attachcgroup_dummy(Oid group, int pid, bool is_cpuset_enabled)
{
	unsupported_system();
}

/*
 * un-assign all the processes from a cgroup.
 *
 * These processes will be moved to the gpdb default cgroup.
 *
 * This function must be called with the gpdb toplevel dir locked,
 * fd_dir is the fd for this lock, on any failure fd_dir will be closed
 * (and unlocked implicitly) then an error is raised.
 */
static void
detachcgroup_dummy(Oid group, CGroupComponentType component, int fd_dir)
{
	unsupported_system();
}

/*
 * Destroy the OS group for group.
 *
 * One OS group can not be dropped if there are processes running under it,
 * if migrate is true these processes will be moved out automatically.
 */
static void
destroycgroup_dummy(Oid group, bool migrate)
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
 * ResGroupOps_UnLockGroup() to unlock it.
 */
static int
lockcgroup_dummy(Oid group, CGroupComponentType component, bool block)
{
	unsupported_system();
	return -1;
}

/*
 * Unblock a OS group.
 *
 * fd is the value returned by ResGroupOps_LockGroup().
 */
static oid
unlockcgroup_dummy(int fd)
{
	unsupported_system();
}

/*
 * Set the cpu rate limit for the OS group.
 *
 * cpu_rate_limit should be within [0, 100].
 */
static void
setcpulimit_dummy(Oid group, int cpu_rate_limit)
{
	unsupported_system();
}

/*
 * Get the cpu usage of the OS group, that is the total cpu time obtained
 * by this OS group, in nano seconds.
 */
static int64
getcpuusage_dummy(Oid group)
{
	unsupported_system();
	return 0;
}

/*
 * Get the cpuset of the OS group.
 * @param group: the destination group
 * @param cpuset: the str to be set
 * @param len: the upper limit of the str
 */
static void
getcpuset_dummy(Oid group, char *cpuset, int len)
{
	unsupported_system();
}

/*
 * Set the cpuset for the OS group.
 * @param group: the destination group
 * @param cpuset: the value to be set
 * The syntax of CPUSET is a combination of the tuples, each tuple represents
 * one core number or the core numbers interval, separated by comma.
 * E.g. 0,1,2-3.
 */
static void
setcpuset_dummy(Oid group, const char *cpuset)
{
	unsupported_system();
}

/*
 * Convert the cpu usage to percentage within the duration.
 *
 * usage is the delta of GetCpuUsage() of a duration,
 * duration is in micro seconds.
 *
 * When fully consuming one cpu core the return value will be 100.0 .
 */
static float
convertcpuusage_dummy(int64 usage, int64 duration)
{
	unsupported_system();
	return 0.0;
}


void
cgroup_handler_dummy(void)
{
	cgroupOpsRoutine->getcgroupname = getcgroupname_dummy;

	cgroupOpsRoutine->probecgroup = probecgroup_dummy;
	cgroupOpsRoutine->checkcgroup = checkcgroup_dummy;
	cgroupOpsRoutine->initcgroup = initcgroup_dummy;
	cgroupOpsRoutine->adjustgucs = adjustgucs_dummy;

	cgroupOpsRoutine->createcgroup = createcgroup_dummy;
	cgroupOpsRoutine->destroycgroup = destroycgroup_dummy;

	cgroupOpsRoutine->attachcgroup = attachcgroup_dummy;
	cgroupOpsRoutine->detachcgroup = detachcgroup_dummy;

	cgroupOpsRoutine->lockcgroup = lockcgroup_dummy;
	cgroupOpsRoutine->unlockcgroup = unlockcgroup_dummy;

	cgroupOpsRoutine->setcpulimit = setcpulimit_dummy;
	cgroupOpsRoutine->setcpushare = setcpulimit_dummy;
	cgroupOpsRoutine->getcpuusage = getcpuusage_dummy;

	cgroupOpsRoutine->getcpuset = getcpuset_dummy;
	cgroupOpsRoutine->setcpuset = setcpuset_dummy;

	cgroupOpsRoutine->convertcpuusage = convertcpuusage_dummy;
}
