/*-------------------------------------------------------------------------
 *
 * cgroup-ops-linux-v1.c
 *	  OS dependent resource group operations - cgroup implementation
 *
 * Copyright (c) 2017 VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resgroup/cgroup-ops-linux-v1.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/cgroup.h"
#include "utils/resgroup.h"
#include "utils/cgroup-ops-v1.h"
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

static CGroupSystemInfo cgroupSystemInfoV1 = {
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
 * cgroup memory permission is only mandatory on 6.x and main;
 * on 5.x we need to make it optional to provide backward compatibilities.
 */
#define CGROUP_MEMORY_IS_OPTIONAL (GP_VERSION_NUM < 60000)
/*
 * cpuset permission is only mandatory on 6.x and main;
 * on 5.x we need to make it optional to provide backward compatibilities.
 */
#define CGROUP_CPUSET_IS_OPTIONAL (GP_VERSION_NUM < 60000)

/* The functions current file used */
static void detect_component_dirs_v1(void);
static void dump_component_dirs_v1(void);

static void check_component_hierarchy_v1();

static void init_cpu_v1(void);
static void init_cpuset_v1(void);

static void create_default_cpuset_group_v1(void);
static int64 get_cfs_period_us_v1(CGroupComponentType component);

/*
 * currentGroupIdInCGroup & oldCaps are used for reducing redundant
 * file operations
 */
static Oid currentGroupIdInCGroup = InvalidOid;

static int64 system_cfs_quota_us = -1LL;
static int64 parent_cfs_quota_us = -1LL;

/*
 * These checks should keep in sync with gpMgmt/bin/gpcheckresgroupimpl
 */
static const PermItem perm_items_cpu[] =
{
	{ CGROUP_COMPONENT_CPU, "", R_OK | W_OK | X_OK },
	{ CGROUP_COMPONENT_CPU, "cgroup.procs", R_OK | W_OK },
	{ CGROUP_COMPONENT_CPU, "cpu.cfs_period_us", R_OK | W_OK },
	{ CGROUP_COMPONENT_CPU, "cpu.cfs_quota_us", R_OK | W_OK },
	{ CGROUP_COMPONENT_CPU, "cpu.shares", R_OK | W_OK },
	{ CGROUP_COMPONENT_UNKNOWN, NULL, 0 }
};
static const PermItem perm_items_cpu_acct[] =
{
	{ CGROUP_COMPONENT_CPUACCT, "", R_OK | W_OK | X_OK },
	{ CGROUP_COMPONENT_CPUACCT, "cgroup.procs", R_OK | W_OK },
	{ CGROUP_COMPONENT_CPUACCT, "cpuacct.usage", R_OK },
	{ CGROUP_COMPONENT_CPUACCT, "cpuacct.stat", R_OK },
	{ CGROUP_COMPONENT_UNKNOWN, NULL, 0 }
};
static const PermItem perm_items_cpuset[] =
{
	{ CGROUP_COMPONENT_CPUSET, "", R_OK | W_OK | X_OK },
	{ CGROUP_COMPONENT_CPUSET, "cgroup.procs", R_OK | W_OK },
	{ CGROUP_COMPONENT_CPUSET, "cpuset.cpus", R_OK | W_OK },
	{ CGROUP_COMPONENT_CPUSET, "cpuset.mems", R_OK | W_OK },
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
	{ perm_items_cpu_acct, false, NULL },

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

static const char *getcgroupname_v1(void);
static bool probecgroup_v1(void);
static void checkcgroup_v1(void);
static void initcgroup_v1(void);
static void adjustgucs_v1(void);
static void createcgroup_v1(Oid group);
static void attachcgroup_v1(Oid group, int pid, bool is_cpuset_enabled);
static void detachcgroup_v1(Oid group, CGroupComponentType component, int fd_dir);
static void destroycgroup_v1(Oid group, bool migrate);
static int lockcgroup_v1(Oid group, CGroupComponentType component, bool block);
static void unlockcgroup_v1(int fd);
static void setcpulimit_v1(Oid group, int cpu_hard_limit);
static int64 getcpuusage_v1(Oid group);
static void getcpuset_v1(Oid group, char *cpuset, int len);
static void setcpuset_v1(Oid group, const char *cpuset);
static float convertcpuusage_v1(int64 usage, int64 duration);

/*
 * Detect gpdb cgroup component dirs.
 *
 * Take cpu for example, by default we expect gpdb dir to locate at
 * cgroup/cpu/gpdb.  But we'll also check for the cgroup dirs of init process
 * (pid 1), e.g. cgroup/cpu/custom, then we'll look for gpdb dir at
 * cgroup/cpu/custom/gpdb, if it's found and has good permissions, it can be
 * used instead of the default one.
 *
 * If any of the gpdb cgroup component dir can not be found under init process'
 * cgroup dirs or has bad permissions we'll fallback all the gpdb cgroup
 * component dirs to the default ones.
 *
 * NOTE: This auto detection will look for memory & cpuset gpdb dirs even on
 * 5X.
 */
static void
detect_component_dirs_v1(void)
{
	CGroupComponentType component;
	FILE	   *f;
	char		buf[MAX_CGROUP_PATHLEN * 2];
	int			maskAll = (1 << CGROUP_COMPONENT_COUNT) - 1;
	int			maskDetected = 0;

	f = fopen("/proc/1/cgroup", "r");
	if (!f)
		goto fallback;

	/*
	 * format: id:comps:path, e.g.:
	 *
	 *     10:cpuset:/
	 *     4:cpu,cpuacct:/
	 *     1:name=systemd:/init.scope
	 *     0::/init.scope
	 */
	while (fscanf(f, "%*d:%s", buf) != EOF)
	{
		CGroupComponentType components[CGROUP_COMPONENT_COUNT];
		int			ncomps = 0;
		char	   *ptr;
		char	   *tmp;
		char		sep = '\0';
		int			i;

		/* buf is stored with "comps:path" */

		if (buf[0] == ':')
			continue; /* ignore empty comp */

		/* split comps */
		for (ptr = buf; sep != ':'; ptr = tmp)
		{
			tmp = strpbrk(ptr, ":,=");

			sep = *tmp;
			*tmp++ = 0;

			/* for name=comp case there is nothing to do with the name */
			if (sep == '=')
				continue;

			component = getComponentType(ptr);

			if (component == CGROUP_COMPONENT_UNKNOWN)
				continue; /* not used by us */

			/*
			 * push the comp to the comps stack, but if the stack is already
			 * full (which is unlikely to happen in real world), simply ignore
			 * it.
			 */
			if (ncomps < CGROUP_COMPONENT_COUNT)
				components[ncomps++] = component;
		}

		/* now ptr point to the path */
		Assert(strlen(ptr) < MAX_CGROUP_PATHLEN);

		/* if the path is "/" then use empty string "" instead of it */
		if (strcmp(ptr, "/") == 0)
			ptr[0] = '\0';

		/* validate and set path for the comps */
		for (i = 0; i < ncomps; i++)
		{
			component = components[i];
			setComponentDir(component, ptr);

			if (!validateComponentDir(component))
				goto fallback; /* dir missing or bad permissions */

			if (maskDetected & (1 << component))
				goto fallback; /* comp are detected more than once */

			maskDetected |= 1 << component;
		}
	}

	if (maskDetected != maskAll)
		goto fallback; /* not all the comps are detected */

	/*
	 * Dump the comp dirs for debugging?  No!
	 * This function is executed before timezone initialization, logs are
	 * forbidden.
	 */

	fclose(f);
	return;

fallback:
	/* set the fallback dirs for all the comps */
	foreach_comp_type(component)
	{
		setComponentDir(component, FALLBACK_COMP_DIR);
	}

	if (f)
		fclose(f);
}


/*
 * Dump comp dirs.
 */
static void
dump_component_dirs_v1(void)
{
	CGroupComponentType component;
	char		path[MAX_CGROUP_PATHLEN];
	size_t		path_size = sizeof(path);

	foreach_comp_type(component)
	{
		buildPath(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "", path, path_size);

		elog(LOG, "gpdb dir for cgroup component \"%s\": %s",
			 getComponentName(component), path);
	}
}


/*
 * Check the mount hierarchy of cpu and cpuset subsystem.
 *
 * Raise an error if cpu and cpuset are mounted on the same hierarchy.
 */
static void
check_component_hierarchy_v1()
{
	CGroupComponentType component;
	FILE       *f;
	char        buf[MAX_CGROUP_PATHLEN * 2];
	
	f = fopen("/proc/1/cgroup", "r");
	if (!f)
	{
		CGROUP_CONFIG_ERROR("can't check component mount hierarchy \
					file '/proc/1/cgroup' doesn't exist");
		return;
	}

	/*
	 * format: id:comps:path, e.g.:
	 *
	 * 10:cpuset:/
	 * 4:cpu,cpuacct:/
	 * 1:name=systemd:/init.scope
	 * 0::/init.scope
	 */
	while (fscanf(f, "%*d:%s", buf) != EOF)
	{
		char       *ptr;
		char       *tmp;
		char        sep = '\0';
		/* mark if the line has already contained cpu or cpuset component */
		int        markComp = CGROUP_COMPONENT_UNKNOWN;

		/* buf is stored with "comps:path" */
		if (buf[0] == ':')
			continue; /* ignore empty comp */

		/* split comps */
		for (ptr = buf; sep != ':'; ptr = tmp)
		{
			tmp = strpbrk(ptr, ":,=");
			
			sep = *tmp;
			*tmp++ = 0;

			/* for name=comp case there is nothing to do with the name */
			if (sep == '=')
				continue;

			component = getComponentType(ptr);

			if (component == CGROUP_COMPONENT_UNKNOWN)
				continue; /* not used by us */
			
			if (component == CGROUP_COMPONENT_CPU || component == CGROUP_COMPONENT_CPUSET)
			{
				if (markComp == CGROUP_COMPONENT_UNKNOWN)
					markComp = component;
				else
				{
					Assert(markComp != component);
					fclose(f);
					CGROUP_CONFIG_ERROR("can't mount 'cpu' and 'cpuset' on the same hierarchy");
					return;
				}
			}
		}
	}

	fclose(f);
}

/*
 * Init gpdb cpu settings.
 */
static void
init_cpu_v1(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPU;
	int64		cfs_quota_us;
	int64		shares;

	/*
	 * CGroup promises that cfs_quota_us will never be 0, however on centos6
	 * we ever noticed that it has the value 0.
	 */
	if (parent_cfs_quota_us <= 0LL)
	{
		/*
		 * parent cgroup is unlimited, calculate gpdb's limitation based on
		 * system hardware configuration.
		 *
		 * cfs_quota_us := parent.cfs_period_us * ncores * gp_resource_group_cpu_limit
		 */
		cfs_quota_us = system_cfs_quota_us * gp_resource_group_cpu_limit;
	}
	else
	{
		/*
		 * parent cgroup is also limited, then calculate gpdb's limitation
		 * based on it.
		 *
		 * cfs_quota_us := parent.cfs_quota_us * gp_resource_group_cpu_limit
		 */
		cfs_quota_us = parent_cfs_quota_us * gp_resource_group_cpu_limit;
	}

	writeInt64(CGROUP_ROOT_ID, BASEDIR_GPDB,
			   component, "cpu.cfs_quota_us", cfs_quota_us);

	/*
	 * shares := parent.shares * gp_resource_group_cpu_priority
	 *
	 * We used to set a large shares (like 1024 * 50, the maximum possible
	 * value), it has very bad effect on overall system performance,
	 * especially on 1-core or 2-core low-end systems.
	 */
	shares = readInt64(CGROUP_ROOT_ID, BASEDIR_PARENT, component, "cpu.shares");
	shares = shares * gp_resource_group_cpu_priority;

	writeInt64(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpu.shares", shares);
}

/*
 * Init gpdb cpuset settings.
 */
static void
init_cpuset_v1(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPUSET;
	char		buffer[MaxCpuSetLength];

	if (!gp_resource_group_enable_cgroup_cpuset)
		return;

	/*
	 * Get cpuset.mems and cpuset.cpus values from cgroup cpuset root path,
	 * and set them to cpuset/gpdb/cpuset.mems and cpuset/gpdb/cpuset.cpus
	 * to make sure that gpdb directory configuration is same as its
	 * parent directory
	 */

	readStr(CGROUP_ROOT_ID, BASEDIR_PARENT, component, "cpuset.mems",
			buffer, sizeof(buffer));
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.mems", buffer);

	readStr(CGROUP_ROOT_ID, BASEDIR_PARENT, component, "cpuset.cpus",
			buffer, sizeof(buffer));
	writeStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.cpus", buffer);

	create_default_cpuset_group_v1();
}

static int64
get_cfs_period_us_v1(CGroupComponentType component)
{
	int64		cfs_period_us;

	/*
	 * calculate cpu rate limit of system.
	 *
	 * Ideally the cpu quota is calculated from parent information:
	 *
	 * system_cfs_quota_us := parent.cfs_period_us * ncores.
	 *
	 * However, on centos6 we found parent.cfs_period_us can be 0 and is not
	 * writable.  In the other side, gpdb.cfs_period_us should be equal to
	 * parent.cfs_period_us because sub dirs inherit parent properties by
	 * default, so we read it instead.
	 */
	cfs_period_us = readInt64(CGROUP_ROOT_ID, BASEDIR_GPDB,
							  component, "cpu.cfs_period_us");

	if (cfs_period_us == 0LL)
	{
		/*
		 * if gpdb.cfs_period_us is also 0 try to correct it by setting the
		 * default value 100000 (100ms).
		 */
		writeInt64(CGROUP_ROOT_ID, BASEDIR_GPDB,
				   component, "cpu.cfs_period_us", DEFAULT_CPU_PERIOD_US);

		/* read again to verify the effect */
		cfs_period_us = readInt64(CGROUP_ROOT_ID, BASEDIR_GPDB,
								  component, "cpu.cfs_period_us");

		if (cfs_period_us <= 0LL)
			CGROUP_CONFIG_ERROR("invalid cpu.cfs_period_us value: "
								INT64_FORMAT,
								cfs_period_us);
	}

	return cfs_period_us;
}

/* Return the name for the OS group implementation */
static const char *
getcgroupname_v1(void)
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
probecgroup_v1(void)
{
	/*
	 * Ignore the error even if cgroup mount point can not be successfully
	 * probed, the error will be reported in checkcgroup() later.
	 */
	if (!getCgroupMountDir())
		return false;

	detect_component_dirs_v1();

	if (!normalPermissionCheck(permlists, CGROUP_ROOT_ID, false))
		return false;

	return true;
}

/* Check whether the OS group implementation is available and usable */
static void
checkcgroup_v1(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPU;
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
	if (!cgroupSystemInfoV1.cgroup_dir[0])
		CGROUP_CONFIG_ERROR("can not find cgroup mount point");

	/*
	 * Check again, this time we will fail on unmet requirements.
	 */
	normalPermissionCheck(permlists, CGROUP_ROOT_ID, true);

	/*
 	 * Check if cpu and cpuset subsystems are mounted on the same hierarchy.
 	 * We do not allow they mount on the same hierarchy, because writing pid
 	 * to DEFAULT_CPUSET_GROUP_ID in attachcgroup will cause the
 	 * removal of the pid in group BASEDIR_GPDB, which will make cpu usage
 	 * out of control.
	 */
	if (!CGROUP_CPUSET_IS_OPTIONAL)
		check_component_hierarchy_v1();

	/*
	 * Dump the cgroup comp dirs to logs.
	 * Check detect_component_dirs() to know why this is not done in that function.
	 */
	dump_component_dirs_v1();

	/*
	 * Get some necessary system information.
	 * We can not do them in probecgroup() as failure is not allowed in that one.
	 */

	/* get system cpu cores */
	cgroupSystemInfoV1.ncores = getCPUCores();

	cfs_period_us = get_cfs_period_us_v1(component);
	system_cfs_quota_us = cfs_period_us * cgroupSystemInfoV1.ncores;

	/* read cpu rate limit of parent cgroup */
	parent_cfs_quota_us = readInt64(CGROUP_ROOT_ID, BASEDIR_PARENT,
									component, "cpu.cfs_quota_us");
}

/* Initialize the OS group */
static void
initcgroup_v1(void)
{
	init_cpu_v1();
	init_cpuset_v1();

	/* 
	 * After basic controller inited, we need to create the SYSTEM CGROUP
	 * which will control the postmaster and auxiliary process, such as
	 * BgWriter, SysLogger.
	 *
	 * We need to add it to the system cgroup before the postmaster fork
	 * the child process to limit the resource usage of the parent process
	 * and all child processes.
	 */
	createcgroup_v1(SYSTEMRESGROUP_OID);
	attachcgroup_v1(SYSTEMRESGROUP_OID, PostmasterPid, false);
}

/* Adjust GUCs for this OS group implementation */
static void
adjustgucs_v1(void)
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
createcgroup_v1(Oid group)
{
	int retry = 0;

	if (!createDir(group, CGROUP_COMPONENT_CPU) ||
		!createDir(group, CGROUP_COMPONENT_CPUACCT) ||
		(gp_resource_group_enable_cgroup_cpuset &&
		 !createDir(group, CGROUP_COMPONENT_CPUSET)))
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

	if (gp_resource_group_enable_cgroup_cpuset)
	{
		/*
		 * Initialize cpuset.mems and cpuset.cpus values as its parent directory
		 */
		CGroupComponentType component = CGROUP_COMPONENT_CPUSET;
		char buffer[MaxCpuSetLength];

		readStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.mems",
				buffer, sizeof(buffer));
		writeStr(group, BASEDIR_GPDB, component, "cpuset.mems", buffer);

		readStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.cpus",
				buffer, sizeof(buffer));
		writeStr(group, BASEDIR_GPDB, component, "cpuset.cpus", buffer);
	}
}

/*
 * Create the OS group for default cpuset group.
 * default cpuset group is a special group, only take effect in cpuset
 */
static void
create_default_cpuset_group_v1(void)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPUSET;
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
	 * Initialize cpuset.mems and cpuset.cpus in default group as its
	 * parent directory
	 */
	char buffer[MaxCpuSetLength];

	readStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.mems",
			buffer, sizeof(buffer));
	writeStr(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB, component, "cpuset.mems", buffer);

	readStr(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "cpuset.cpus",
			buffer, sizeof(buffer));
	writeStr(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB, component, "cpuset.cpus", buffer);
}


/*
 * Assign a process to the OS group. A process can only be assigned to one
 * OS group, if it's already running under other OS group then it'll be moved
 * out that OS group.
 *
 * pid is the process id.
 */
static void
attachcgroup_v1(Oid group, int pid, bool is_cpuset_enabled)
{
	/*
	 * needn't write to file if the pid has already been written in.
	 * Unless it has not been written or the group has changed or
	 * cpu control mechanism has changed.
	 */
	if (IsUnderPostmaster && group == currentGroupIdInCGroup)
		return;

	writeInt64(group, BASEDIR_GPDB, CGROUP_COMPONENT_CPU,
			   "cgroup.procs", pid);
	writeInt64(group, BASEDIR_GPDB, CGROUP_COMPONENT_CPUACCT,
			   "cgroup.procs", pid);

	if (gp_resource_group_enable_cgroup_cpuset)
	{
		if (is_cpuset_enabled)
		{
			writeInt64(group, BASEDIR_GPDB,
					   CGROUP_COMPONENT_CPUSET, "cgroup.procs", pid);
		}
		else
		{
			/* add pid to default group */
			writeInt64(DEFAULT_CPUSET_GROUP_ID, BASEDIR_GPDB,
					   CGROUP_COMPONENT_CPUSET, "cgroup.procs", pid);
		}
	}

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
detachcgroup_v1(Oid group, CGroupComponentType component, int fd_dir)
{
	char 	path[MAX_CGROUP_PATHLEN];
	size_t 	path_size = sizeof(path);

	char 	*buf;
	size_t 	buf_size;
	size_t 	buf_len = -1;

	int fdr = -1;
	int fdw = -1;

	const size_t buf_delta_size = 512;

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
destroycgroup_v1(Oid group, bool migrate)
{
	if (!deleteDir(group, CGROUP_COMPONENT_CPU, "cpu.shares", migrate, detachcgroup_v1) ||
		!deleteDir(group, CGROUP_COMPONENT_CPUACCT, NULL, migrate, detachcgroup_v1) ||
		(gp_resource_group_enable_cgroup_cpuset &&
		 !deleteDir(group, CGROUP_COMPONENT_CPUSET, NULL, migrate, detachcgroup_v1)))
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
 * On success, it returns a fd to the OS group, pass it to unlockcgroup_v1()
 * to unlock it.
 */
static int
lockcgroup_v1(Oid group, CGroupComponentType component, bool block)
{
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, BASEDIR_GPDB, component, "", path, path_size);

	return lockDir(path, block);
}

/*
 * Unblock an OS group.
 *
 * fd is the value returned by lockcgroup_v1().
 */
static void
unlockcgroup_v1(int fd)
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
setcpulimit_v1(Oid group, int cpu_hard_limit)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPU;

	if (cpu_hard_limit > 0)
	{
		int64 periods = get_cfs_period_us_v1(component);
		writeInt64(group, BASEDIR_GPDB, component, "cpu.cfs_quota_us",
				   periods * cgroupSystemInfoV1.ncores * cpu_hard_limit / 100);
	}
	else
	{
		writeInt64(group, BASEDIR_GPDB, component, "cpu.cfs_quota_us", cpu_hard_limit);
	}
}

/*
 * Set the cpu soft priority for the OS group.
 *
 * For version 1, the default value of cpu.shares is 1024, corresponding to
 * our cpu_soft_priority, which default value is 100, so we need to adjust it.
 */
static void
setcpupriority_v1(Oid group, int shares)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPU;
	writeInt64(group, BASEDIR_GPDB, component,
			   "cpu.shares", (int64)(shares * 1024 / 100));
}

/*
 * Get the cpu usage of the OS group, that is the total cpu time obtained
 * by this OS group, in nano seconds.
 */
static int64
getcpuusage_v1(Oid group)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPUACCT;

	return readInt64(group, BASEDIR_GPDB, component, "cpuacct.usage");
}

/*
 * Get the cpuset of the OS group.
 * @param group: the destination group
 * @param cpuset: the str to be set
 * @param len: the upper limit of the str
 */
static void
getcpuset_v1(Oid group, char *cpuset, int len)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPUSET;

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
setcpuset_v1(Oid group, const char *cpuset)
{
	CGroupComponentType component = CGROUP_COMPONENT_CPUSET;

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
convertcpuusage_v1(int64 usage, int64 duration)
{
	float		percent;

	Assert(usage >= 0LL);
	Assert(duration > 0LL);

	/* There should always be at least one core on the system */
	Assert(cgroupSystemInfoV1.ncores > 0);

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
	percent = usage / 10.0 / duration / cgroupSystemInfoV1.ncores;

	/*
	 * Now we have the system level percentage, however when running in a
	 * container with limited cpu quota we need to further scale it with
	 * parent.  Suppose parent has 50% cpu quota and gpdb is consuming all of
	 * it, then we want gpdb to report the cpu usage as 100% instead of 50%.
	 */

	if (parent_cfs_quota_us > 0LL)
	{
		/*
		 * Parent cgroup is also limited, scale the percentage to the one in
		 * parent cgroup.  Do not change the expression to `percent *= ...`,
		 * that will lose the precision.
		 */
		percent = percent * system_cfs_quota_us / parent_cfs_quota_us;
	}

	return percent;
}

static CGroupOpsRoutine cGroupOpsRoutineV1 = {
		.getcgroupname = getcgroupname_v1,
		.probecgroup = probecgroup_v1,
		.checkcgroup = checkcgroup_v1,
		.initcgroup = initcgroup_v1,
		.adjustgucs = adjustgucs_v1,
		.createcgroup = createcgroup_v1,
		.destroycgroup = destroycgroup_v1,

		.attachcgroup = attachcgroup_v1,
		.detachcgroup = detachcgroup_v1,

		.lockcgroup = lockcgroup_v1,
		.unlockcgroup = unlockcgroup_v1,

		.setcpulimit = setcpulimit_v1,
		.getcpuusage = getcpuusage_v1,
		.setcpupriority = setcpupriority_v1,
		.getcpuset = getcpuset_v1,
		.setcpuset = setcpuset_v1,

		.convertcpuusage = convertcpuusage_v1,
};

CGroupOpsRoutine *get_group_routine_v1(void)
{
	return &cGroupOpsRoutineV1;
}

CGroupSystemInfo *get_cgroup_sysinfo_v1(void)
{
	return &cgroupSystemInfoV1;
}
