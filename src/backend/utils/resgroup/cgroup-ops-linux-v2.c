/*-------------------------------------------------------------------------
 *
 * cgroup-ops-linux-v2.c
 *	  OS dependent resource group operations - cgroup implementation
 *
 * Copyright (c) 2017 VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resgroup/cgroup-ops-linux-v2.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/cgroup.h"
#include "utils/resgroup.h"
#include "utils/cgroup-ops-v2.h"
#include "utils/vmem_tracker.h"

#ifndef __linux__
#error  cgroup is only available on linux
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <stdio.h>
#include <mntent.h>
#include <regex.h>

static CGroupSystemInfo cgroupSystemInfoV2 = {
		0,
		""
};

/*
 * Interfaces for OS dependent operations.
 *
 * Resource group relies on OS dependent group implementation to manage
 * resources like cpu usage, such as cgroup on Linux system.
 * We call it OS group in below function description.
 *
 * So far these operations are mainly for CPU rate limitation and accounting.
 */

/*
 * cpuset permission is only mandatory on 6.x and main;
 * on 5.x we need to make it optional to provide backward compatibilities.
 */
#define CGROUP_CPUSET_IS_OPTIONAL (GP_VERSION_NUM < 60000)

/* The functions current file used */
static void dump_component_dir_v2(void);


static void init_subtree_control(void);
static void init_cpu_v2(void);
static void init_cpuset_v2(void);

static void create_default_cpuset_group_v2(void);
static int64 get_cfs_period_us_v2();

/*
 * currentGroupIdInCGroup & oldCaps are used for reducing redundant
 * file operations
 */
static Oid currentGroupIdInCGroup = InvalidOid;

/* system_cfs_quota_us = 100000 * ncores */
static int64 system_cfs_quota_us = -1LL;

/*
 * These checks should keep in sync with gpMgmt/bin/gpcheckresgroupimpl
 */
static const PermItem perm_items_cpu[] =
{
	{ CGROUP_COMPONENT_PLAIN, "cpu.max", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpu.pressure", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpu.weight", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpu.weight.nice", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpu.stat", R_OK },
	{ CGROUP_COMPONENT_UNKNOWN, NULL, 0 }
};
static const PermItem perm_items_cpuset[] =
{
	{ CGROUP_COMPONENT_PLAIN, "cpuset.cpus", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpuset.cpus.partition", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpuset.mems", R_OK | W_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpuset.cpus.effective", R_OK },
	{ CGROUP_COMPONENT_PLAIN, "cpuset.mems.effective", R_OK },
	{ CGROUP_COMPONENT_UNKNOWN, NULL, 0 }
};

/*
 * just for cpuset check, same as the cpuset Permlist in permlists
 */
static const PermList cpusetPermList =
{
	perm_items_cpuset,
	CGROUP_CPUSET_IS_OPTIONAL,
	&gp_resource_group_enable_cgroup_cpuset,
};

/*
 * Permission groups.
 */
static const PermList permlists[] =
{
	/* cpu/cpuacct permissions are mandatory */
	{ perm_items_cpu, false, NULL },

	/*
	 * cpuset permissions can be mandatory or optional depends on the switch.
	 *
	 * resgroup cpuset is introduced in 6.0 devel and backport
	 * to 5.x branch since 5.6.1.  To provide backward compatibilities cpuset
	 * permissions are optional on 5.x branch.
	 */
	{ perm_items_cpuset, CGROUP_CPUSET_IS_OPTIONAL,
		&gp_resource_group_enable_cgroup_cpuset},

	{ NULL, false, NULL }
};

static const char *getcgroupname_v2(void);
static bool probecgroup_v2(void);
static void checkcgroup_v2(void);
static void initcgroup_v2(void);
static void adjustgucs_v2(void);
static void createcgroup_v2(Oid group);
static void attachcgroup_v2(Oid group, int pid, bool is_cpuset_enabled);
static void detachcgroup_v2(Oid group, CGroupComponentType component, int fd_dir);
static void destroycgroup_v2(Oid group, bool migrate);
static int lockcgroup_v2(Oid group, CGroupComponentType component, bool block);
static void unlockcgroup_v2(int fd);
static void setcpulimit_v2(Oid group, int cpu_hard_limit);
static int64 getcpuusage_v2(Oid group);
static void getcpuset_v2(Oid group, char *cpuset, int len);
static void setcpuset_v2(Oid group, const char *cpuset);
static float convertcpuusage_v2(int64 usage, int64 duration);

/*
 * Dump component dir to the log.
 */
static void
dump_component_dir_v2(void)
{
	char		path[MAX_CGROUP_PATHLEN];
	size_t		path_size = sizeof(path);

	buildPath(CGROUP_ROOT_ID, BASEDIR_GPDB, CGROUP_COMPONENT_PLAIN, "", path, path_size);

	elog(LOG, "gpdb dir for cgroup component : %s", path);
}

/*
 * Init cgroup.subtree_control, add "cpuset cpu memory pids" to the file cgroup.subtree_control
 */
static void
init_subtree_control(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;

	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cgroup.subtree_control", "+cpuset");
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cgroup.subtree_control", "+cpu");
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cgroup.subtree_control", "+memory");
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cgroup.subtree_control", "+pids");
}

/*
 * Init gpdb cpu settings.
 */
static void
init_cpu_v2(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;
	int64		cpu_max;
	int64		weight;

	/*
	 *
	 * cfs_quota_us := parent.cfs_quota_us * ncores * gp_resource_group_cpu_limit
	 */
	cpu_max = system_cfs_quota_us * gp_resource_group_cpu_limit;


	writeInt64(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpu.max", cpu_max);

	/*
	 * shares := cpu.weight * gp_resource_group_cpu_priority
	 *
	 * We used to set a large shares (like 100 * 50, the maximum possible
	 * value), it has very bad effect on overall system performance,
	 * especially on 1-core or 2-core low-end systems.
	 */
	weight = 100 * gp_resource_group_cpu_priority;
	writeInt32(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpu.weight", weight);
}

/*
 * Init gpdb cpuset settings.
 */
static void
init_cpuset_v2(void)
{
	if (!gp_resource_group_enable_cgroup_cpuset)
		return;

	/*
	 * Initialize cpuset.mems and cpuset.cpus from the default file.
	 *
	 * In Linux Cgroup v2, there's no default parent group, the group of gpdb
	 * itself is the parent, that means we can use all the cpuset in the host.
	 *
	 * We do not need to read the cpuset from the parent group like version 1,
	 * just copy all the value from cpuset.cpus.effective and cpuset.mems.effective
	 * to cpuset.cpus and cpuset.mems, because those files are empty, but we need
	 * a value to do our work.
	 */
	char buffer[MaxCpuSetLength];
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;

	readStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.cpus.effective",
			buffer, sizeof(buffer));
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.cpus", buffer);

	readStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.mems.effective",
			buffer, sizeof(buffer));
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.mems", buffer);

	create_default_cpuset_group_v2();
}

static int64
get_cfs_period_us_v2()
{
	/* For Cgroup v2, the default cpu_period_us is 100000, just return this. */
	return 100000L;
}

/* Return the name for the OS group implementation */
static const char *
getcgroupname_v2(void)
{
	return "cgroup";
}

/*
 * Probe the configuration for the OS group implementation.
 *
 * Return true if everything is OK, or false is some requirements are not
 * satisfied.
 */
static bool
probecgroup_v2(void)
{
	/*
	 * Ignore the error even if cgroup mount point can not be successfully
	 * probed, the error will be reported in checkcgroup() later.
	 */
	if (!getCgroupMountDir())
		return false;

	if (!normalPermissionCheck(permlists, CGROUP_ROOT_ID, false))
		return false;

	return true;
}

/* Check whether the OS group implementation is available and usable */
static void
checkcgroup_v2(void)
{
	int64		cfs_period_us;

	/*
	 * We only have to do these checks and initialization once on each host,
	 * so only let postmaster do the job.
	 */
	Assert(!IsUnderPostmaster);

	/*
	 * We should have already detected for cgroup mount point in probecgroup(),
	 * it was not an error if the detection failed at that step.  But once
	 * we call checkcgroup() we know we want to make use of cgroup then we must
	 * know the mount point, otherwise it's a critical error.
	 */
	if (!cgroupSystemInfoV2.cgroup_dir[0])
		CGROUP_CONFIG_ERROR("can not find cgroup mount point");

	/*
	 * Check again, this time we will fail on unmet requirements.
	 */
	normalPermissionCheck(permlists, CGROUP_ROOT_ID, true);


	/*
	 * Dump the cgroup comp dirs to logs.
	 * Check detect_component_dirs() to know why this is not done in that function.
	 */
	dump_component_dir_v2();

	/*
	 * Get some necessary system information.
	 * We can not do them in probecgroup() as failure is not allowed in that one.
	 */

	/* get system cpu cores */
	cgroupSystemInfoV2.ncores = getCPUCores();

	cfs_period_us = get_cfs_period_us_v2();
	system_cfs_quota_us = cfs_period_us * cgroupSystemInfoV2.ncores;
}

/* Initialize the OS group */
static void
initcgroup_v2(void)
{
	init_subtree_control();

	init_cpu_v2();
	init_cpuset_v2();

	/* 
	 * After basic controller inited, we need to create the SYSTEM CGROUP
	 * which will control the postmaster and auxiliary process, such as
	 * BgWriter, SysLogger.
	 *
	 * We need to add it to the system cgroup before the postmaster fork
	 * the child process to limit the resource usage of the parent process
	 * and all child processes.
	 */
	createcgroup_v2(SYSTEMRESGROUP_OID);
	attachcgroup_v2(SYSTEMRESGROUP_OID, PostmasterPid, false);
}

/* Adjust GUCs for this OS group implementation */
static void
adjustgucs_v2(void)
{
	/*
	 * cgroup cpu limitation works best when all processes have equal
	 * priorities, so we force all the segments and postmaster to
	 * work with nice=0.
	 *
	 * this function should be called before GUCs are dispatched to segments.
	 */
	gp_segworker_relative_priority = 0;
}

/*
 * Create the OS group for group.
 */
static void
createcgroup_v2(Oid group)
{
	int retry = 0;

	if (!createDir(group, CGROUP_COMPONENT_PLAIN))
	{
		CGROUP_ERROR("can't create cgroup for resource group '%d': %m", group);
	}

	/*
	 * although the group dir is created the interface files may not be
	 * created yet, so we check them repeatedly until everything is ready.
	 */
	while (++retry <= MAX_RETRY && !normalPermissionCheck(permlists, group, false))
		pg_usleep(1000);

	if (retry > MAX_RETRY)
	{
		/*
		 * still not ready after MAX_RETRY retries, might be a real error,
		 * raise the error.
		 */
		normalPermissionCheck(permlists, group, true);
	}
}

/*
 * Create the OS group for default cpuset group.
 * default cpuset group is a special group, only take effect in cpuset
 */
static void
create_default_cpuset_group_v2(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;
	int retry = 0;

	if (!createDir(DEFAULT_CPUSET_GROUP_ID, component))
	{
		CGROUP_ERROR("can't create cpuset cgroup for resgroup '%d': %m",
					 DEFAULT_CPUSET_GROUP_ID);
	}

	/*
	 * although the group dir is created the interface files may not be
	 * created yet, so we check them repeatedly until everything is ready.
	 */
	while (++retry <= MAX_RETRY &&
		   !cpusetPermissionCheck(&cpusetPermList, DEFAULT_CPUSET_GROUP_ID, false))
		pg_usleep(1000);

	if (retry > MAX_RETRY)
	{
		/*
		 * still not ready after MAX_RETRY retries, might be a real error,
		 * raise the error.
		 */
		cpusetPermissionCheck(&cpusetPermList, DEFAULT_CPUSET_GROUP_ID, true);
	}

	/*
	 * Initialize cpuset.mems and cpuset.cpus in default group.
	 */
	char buffer[MaxCpuSetLength];

	readStr(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB, component, "cpuset.cpus.effective",
			buffer, sizeof(buffer));
	writeStr(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB, component, "cpuset.cpus", buffer);

	readStr(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB, component, "cpuset.mems.effective",
			buffer, sizeof(buffer));
	writeStr(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB, component, "cpuset.mems", buffer);
}


/*
 * Assign a process to the OS group. A process can only be assigned to one
 * OS group, if it's already running under other OS group then it'll be moved
 * out that OS group.
 *
 * pid is the process id.
 */
static void
attachcgroup_v2(Oid group, int pid, bool is_cpuset_enabled)
{
	/*
	 * needn't write to file if the pid has already been written in.
	 * Unless it has not been written or the group has changed or
	 * cpu control mechanism has changed.
	 */
	if (IsUnderPostmaster && group == currentGroupIdInCGroup)
		return;

	writeInt64(group, BASEDIR_GPDB, CGROUP_COMPONENT_PLAIN,
			   "cgroup.procs", pid);

	/*
	 * Do not assign the process to cgroup/memory for now.
	 */

	currentGroupIdInCGroup = group;
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
detachcgroup_v2(Oid group, CGroupComponentType component, int fd_dir)
{
	char 	path[MAX_CGROUP_PATHLEN];
	size_t 	path_size = sizeof(path);

	char 	*buf;
	size_t 	buf_size;
	size_t 	buf_len = -1;

	int fdr = -1;
	int fdw = -1;

	const size_t buf_delta_size = 512;

	component = CGROUP_COMPONENT_PLAIN;

	/*
	 * Check an operation result on path.
	 *
	 * Operation can be open(), close(), read(), write(), etc., which must
	 * set the errno on error.
	 *
	 * - condition describes the expected result of the operation;
	 * - action is the cleanup action on failure, such as closing the fd,
	 *   multiple actions can be specified by putting them in brackets,
	 *   such as (op1, op2);
	 * - message describes what's failed;
	 */
#define __CHECK(condition, action, message) do { \
	if (!(condition)) \
	{ \
		/* save errno in case it's changed in actions */ \
		int err = errno; \
		action; \
		CGROUP_ERROR(message ": %s: %s", path, strerror(err)); \
	} \
} while (0)

	buildPath(group, BASEDIR_GPDB, component, "cgroup.procs", path, path_size);

	fdr = open(path, O_RDONLY);

	__CHECK(fdr >= 0, ( close(fd_dir) ), "can't open file for read");

	buf_len = 0;
	buf_size = buf_delta_size;
	buf = palloc(buf_size);

	while (1)
	{
		int n = read(fdr, buf + buf_len, buf_delta_size);
		__CHECK(n >= 0, ( close(fdr), close(fd_dir) ), "can't read from file");

		buf_len += n;

		if (n < buf_delta_size)
			break;

		buf_size += buf_delta_size;
		buf = repalloc(buf, buf_size);
	}

	close(fdr);
	if (buf_len == 0)
		return;

	buildPath(DEFAULTRESGROUP_OID, BASEDIR_GPDB, component, "cgroup.procs",
			  path, path_size);

	fdw = open(path, O_WRONLY);
	__CHECK(fdw >= 0, ( close(fd_dir) ), "can't open file for write");

	char *ptr = buf;
	char *end = NULL;
	long pid;

	/*
	 * as required by cgroup, only one pid can be migrated in each single
	 * write() call, so we have to parse the pids from the buffer first,
	 * then write them one by one.
	 */
	while (1)
	{
		pid = strtol(ptr, &end, 10);
		__CHECK(pid != LONG_MIN && pid != LONG_MAX,
				( close(fdw), close(fd_dir) ),
				"can't parse pid");

		if (ptr == end)
			break;

		char str[22];
		sprintf(str, "%ld", pid);
		int n = write(fdw, str, strlen(str));
		if (n < 0)
		{
			elog(LOG, "failed to migrate pid to gpdb root cgroup: pid=%ld: %m",
				 pid);
		}
		else
		{
			__CHECK(n == strlen(str),
					( close(fdw), close(fd_dir) ),
					"can't write to file");
		}

		ptr = end;
	}

	close(fdw);

#undef __CHECK
}


/*
 * Destroy the OS cgroup.
 *
 * One OS group can not be dropped if there are processes running under it,
 * if migrate is true these processes will be moved out automatically.
 */
static void
destroycgroup_v2(Oid group, bool migrate)
{
	if (!deleteDir(group, CGROUP_COMPONENT_PLAIN, NULL, migrate, detachcgroup_v2))
	{
		CGROUP_ERROR("can't remove cgroup for resource group '%d': %m", group);
	}
}


/*
 * Lock the OS group. While the group is locked it won't be removed by other
 * processes.
 *
 * This function would block if block is true, otherwise it returns with -1
 * immediately.
 *
 * On success, it returns a fd to the OS group, pass it to unlockcgroup_v2()
 * to unlock it.
 */
static int
lockcgroup_v2(Oid group, CGroupComponentType component, bool block)
{
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);
	component = CGROUP_COMPONENT_PLAIN;

	buildPath(group, BASEDIR_GPDB, component, "", path, path_size);

	return lockDir(path, block);
}

/*
 * Unblock an OS group.
 *
 * fd is the value returned by lockcgroup_v2().
 */
static void
unlockcgroup_v2(int fd)
{
	if (fd >= 0)
		close(fd);
}

/*
 * Set the cpu hard limit for the OS group.
 *
 * cpu_hard_quota_limit should be within [-1, 100].
 */
static void
setcpulimit_v2(Oid group, int cpu_hard_limit)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;

	if (cpu_hard_limit > 0)
	{
		writeInt64(group, BASEDIR_GPDB, component, "cpu.max",
				   system_cfs_quota_us * cpu_hard_limit / 100);
	}
	else
	{
		writeStr(group, BASEDIR_GPDB, component, "cpu.max", "max");
	}
}

/*
 * Set the cpu soft priority for the OS group.
 *
 * For version 1, the default value of cpu.shares is 1024, corresponding to
 * our cpu_soft_priority, which default value is 100, so we need to adjust it.
 *
 * The weight in the range [1, 10000], so the cpu_soft_priority is in range [1, 976.5625].
 * In Greenplum, we define the range [1, 500].
 */
static void
setcpupriority_v2(Oid group, int shares)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;
	writeInt64(group, BASEDIR_GPDB, component,
			   "cpu.weight", (int64)(shares * 1024 / 100));
}

/*
 * Get the cpu usage of the OS group, that is the total cpu time obtained
 * by this OS group, in nanoseconds.
 */
static int64
getcpuusage_v2(Oid group)
{
	regex_t 	reg;
	char 		buffer[4096], result[128];
	regmatch_t 	pmatch;
	const char *pattern = "usage_usec ([0-9]+)";
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;

	/*
	 * We read the value of "usage_usec", all time durations are in microseconds,
	 * due to compatible with cgroup v1, return this value is nanoseconds.
	 */
	readStr(group, BASEDIR_GPDB, component, "cpu.stat", buffer, 4096);

	regcomp(&reg, pattern, REG_EXTENDED);

	int status = regexec(&reg, buffer, 1, &pmatch, 0);

	if (status == REG_NOMATCH)
		CGROUP_ERROR("can't read the value of usage_usec from /sys/fs/cgroup/gpdb/cpu.stat");
	else if (pmatch.rm_so != -1)
		memcpy(result, buffer + pmatch.rm_so + strlen("usage_usec "), pmatch.rm_eo - pmatch.rm_so);

	regfree(&reg);

	return atoll(result) * 1000;
}

/*
 * Get the cpuset of the OS group.
 * @param group: the destination group
 * @param cpuset: the str to be set
 * @param len: the upper limit of the str
 */
static void
getcpuset_v2(Oid group, char *cpuset, int len)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;

	if (!gp_resource_group_enable_cgroup_cpuset)
		return ;

	readStr(group, BASEDIR_GPDB, component, "cpuset.cpus", cpuset, len);
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
setcpuset_v2(Oid group, const char *cpuset)
{
	CGroupComponentType component = CGROUP_COMPONENT_PLAIN;

	if (!gp_resource_group_enable_cgroup_cpuset)
		return ;

	writeStr(group, BASEDIR_GPDB, component, "cpuset.cpus", cpuset);
}


/*
 * Convert the cpu usage to percentage within the duration.
 *
 * usage is the delta of getcpuusage() of a duration,
 * duration is in micro seconds.
 *
 * When fully consuming one cpu core the return value will be 100.0 .
 */
static float
convertcpuusage_v2(int64 usage, int64 duration)
{
	float		percent;

	Assert(usage >= 0LL);
	Assert(duration > 0LL);

	/* There should always be at least one core on the system */
	Assert(cgroupSystemInfoV2.ncores > 0);

	/*
	 * Usage is the cpu time (nano seconds) obtained by this group in the time
	 * duration (micro seconds), so cpu time on one core can be calculated as:
	 *
	 *     usage / 1000 / duration / ncores
	 *
	 * To convert it to percentage we should multiple 100%:
	 *
	 *     usage / 1000 / duration / ncores * 100%
	 *   = usage / 10 / duration / ncores
	 */
	percent = usage / 10.0 / duration / cgroupSystemInfoV2.ncores;

	/*
	 * Now we have the system level percentage, however when running in a
	 * container with limited cpu quota we need to further scale it with
	 * parent.  Suppose parent has 50% cpu quota and gpdb is consuming all of
	 * it, then we want gpdb to report the cpu usage as 100% instead of 50%.
	 */

	return percent;
}

static CGroupOpsRoutine cGroupOpsRoutineV2 = {
		.getcgroupname = getcgroupname_v2,
		.probecgroup = probecgroup_v2,
		.checkcgroup = checkcgroup_v2,
		.initcgroup = initcgroup_v2,
		.adjustgucs = adjustgucs_v2,
		.createcgroup = createcgroup_v2,
		.destroycgroup = destroycgroup_v2,

		.attachcgroup = attachcgroup_v2,
		.detachcgroup = detachcgroup_v2,

		.lockcgroup = lockcgroup_v2,
		.unlockcgroup = unlockcgroup_v2,

		.setcpulimit = setcpulimit_v2,
		.getcpuusage = getcpuusage_v2,
		.setcpupriority = setcpupriority_v2,
		.getcpuset = getcpuset_v2,
		.setcpuset = setcpuset_v2,

		.convertcpuusage = convertcpuusage_v2,
};

CGroupOpsRoutine *get_group_routine_v2(void)
{
	return &cGroupOpsRoutineV2;
}

CGroupSystemInfo *get_cgroup_sysinfo_v2(void)
{
	return &cgroupSystemInfoV2;
}
