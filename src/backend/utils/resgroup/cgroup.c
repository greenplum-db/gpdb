#include "postgres.h"

#include <limits.h>

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/cgroup.h"
#include "utils/resgroup.h"
#include "utils/resource_manager.h"
#include "utils/vmem_tracker.h"
#include "storage/shmem.h"

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

/* cgroup component names. */
const char *component_names[CGROUP_COMPONENT_COUNT] =
{
	"cpu", "cpuacct", "cpuset"
};

/* cgroup component dirs. */
char component_dirs[CGROUP_COMPONENT_COUNT][MAX_CGROUP_PATHLEN] =
{
	FALLBACK_COMP_DIR, FALLBACK_COMP_DIR, FALLBACK_COMP_DIR
};


/*
 * Get the name of cgroup controller component.
 */
const char *
getComponentName(CGroupComponentType component)
{
	Assert(component < CGROUP_COMPONENT_COUNT);

	return component_names[component];
}


/*
 * Get the component type from the cgroup controller name.
 */
CGroupComponentType
getComponentType(const char *name)
{
	CGroupComponentType component;

	for (component = 0; component < CGROUP_COMPONENT_COUNT; component++)
		if (strcmp(name, getComponentName(component)) == 0)
			return component;

	return CGROUP_COMPONENT_UNKNOWN;
}


/*
 * Get the directory of component.
 */
const char *
getComponentDir(CGroupComponentType component)
{
	Assert(component < CGROUP_COMPONENT_COUNT);

	return component_dirs[component];
}

/*
 * Set the component dir of component.
 */
void
setComponentDir(CGroupComponentType component, const char *dir)
{
	Assert(component < CGROUP_COMPONENT_COUNT);
	Assert(strlen(dir) < MAX_CGROUP_PATHLEN);

	strcpy(component_dirs[component], dir);
}

/*
 * Build path string with parameters.
 *
 * Will raise an exception if the path buffer is not large enough.
 *
 * Examples (path and path_size are omitted):
 *
 * - buildPath(ROOT, PARENT, CPU, ""     ): /sys/fs/cgroup/cpu
 * - buildPath(ROOT, PARENT, CPU, "tasks"): /sys/fs/cgroup/cpu/tasks
 * - buildPath(ROOT, GPDB  , CPU, "tasks"): /sys/fs/cgroup/cpu/gpdb/tasks
 *
 * - buildPath(ROOT, PARENT, ALL, "     "): /sys/fs/cgroup/
 * - buildPath(ROOT, PARENT, ALL, "tasks"): /sys/fs/cgroup/tasks
 * - buildPath(ROOT, GPDB  , ALL, "tasks"): /sys/fs/cgroup/gpdb/tasks
 *
 * - buildPath(6437, GPDB  , CPU, "tasks"): /sys/fs/cgroup/cpu/gpdb/6437/tasks
 * - buildPath(6437, GPDB  , ALL, "tasks"): /sys/fs/cgroup/gpdb/6437/tasks
 */
void
buildPath(Oid group,
		  BaseDirType base,
		  CGroupComponentType component,
		  const char *filename,
		  char *path,
		  size_t path_size)
{
	bool result = buildPathSafe(group, base, component, filename, path, path_size);

	if (!result)
	{
		CGROUP_CONFIG_ERROR("invalid %s name '%s': %m",
							filename[0] ? "file" : "directory",
							path);
	}
}

/*
 * Build path string with parameters.
 *
 * Return false if the path buffer is not large enough, errno will also be set.
 */
bool
buildPathSafe(Oid group,
			  BaseDirType base,
			  CGroupComponentType component,
			  const char *filename,
			  char *path,
			  size_t path_size)
{
	const char *component_name = getComponentName(component);
	const char *component_dir = component_name;
	const char *base_dir = "";
	char		group_dir[MAX_CGROUP_PATHLEN] = "";
	int			len;

	Assert(cgroupSystemInfo->cgroup_dir[0] != 0);
	Assert(base == BASEDIR_GPDB || base == BASEDIR_PARENT);

	if (base == BASEDIR_GPDB)
		base_dir = "/gpdb";
	else
		base_dir = "";

	/* add group name to the path */
	if (group != CGROUP_ROOT_ID)
	{
		len = snprintf(group_dir, sizeof(group_dir), "/%u", group);
		/* We are sure group_dir is large enough */
		Assert(len > 0 && len < sizeof(group_dir));
	}

	if (component != CGROUP_COMPONENT_PLAIN)
	{
		/*
		 * for cgroup v1, we need add the component name to the path,
		 * such as "/gpdb/cpu/...", "/gpdb/cpuset/...".
		 */
		len = snprintf(path, path_size, "%s/%s%s%s/%s",
					   cgroupSystemInfo->cgroup_dir, component_dir, base_dir, group_dir, filename);
	}
	else
	{
		/*
		 * for cgroup v2, we just have the top level and child level,
		 * don't need to care about the component.
		 */
		base_dir = base == BASEDIR_GPDB ? "gpdb" : "";
		len = snprintf(path, path_size, "%s/%s%s/%s",
					   cgroupSystemInfo->cgroup_dir, base_dir, group_dir, filename);
	}

	if (len >= path_size || len < 0)
	{
		errno = ENAMETOOLONG;
		return false;
	}

	return true;
}

/*
 * Validate a component dir.
 *
 * Return true if it exists and has right permissions,
 * otherwise return false.
 */
bool
validateComponentDir(CGroupComponentType component)
{
	char		path[MAX_CGROUP_PATHLEN];
	size_t		path_size = sizeof(path);

	if (!buildPathSafe(CGROUP_ROOT_ID, BASEDIR_GPDB, component, "",
					   path, path_size))
		return false;

	return access(path, R_OK | W_OK | X_OK) == 0;
}

/*
 * Lock a dir
 */
int
lockDir(const char *path, bool block)
{
	int fd_dir;

	fd_dir = open(path, O_RDONLY);
	if (fd_dir < 0)
	{
		if (errno == ENOENT)
			/* the dir doesn't exist, nothing to do */
			return -1;

		CGROUP_ERROR("can't open dir to lock: %s: %m", path);
	}

	int flags = LOCK_EX;
	if (!block)
		flags |= LOCK_NB;

	while (flock(fd_dir, flags))
	{
		/*
		 * EAGAIN is not described in flock(2),
		 * however it does appear in practice.
		 */
		if (errno == EAGAIN)
			continue;

		int err = errno;
		close(fd_dir);

		/*
		 * In block mode all errors should be reported;
		 * In non block mode only report errors != EWOULDBLOCK.
		 */
		if (block || err != EWOULDBLOCK)
			CGROUP_ERROR("can't lock dir: %s: %s", path, strerror(err));
		return -1;
	}

	/*
	 * Even if we acquired the lock the dir may still been removed by other
	 * processes, e.g.:
	 *
	 * 1: open()
	 * 1: flock() -- process 1 acquire the lock
	 *
	 * 2: open()
	 * 2: flock() -- blocked by process 1
	 *
	 * 1: rmdir()
	 * 1: close() -- process 1 released the lock
	 *
	 * 2:flock() will now return w/o error as process 2 still has a valid
	 * fd (reference) on the target dir, and process 2 does acquire the lock
	 * successfully. However, as the dir is already removed so process 2
	 * shouldn't make any further operation (rmdir(), etc.) on the dir.
	 *
	 * So we check for the existence of the dir again and give up if it's
	 * already removed.
	 */
	if (access(path, F_OK))
	{
		/* the dir is already removed by other process, nothing to do */
		close(fd_dir);
		return -1;
	}

	return fd_dir;
}

/*
 * Create cgroup dir
 */
bool
createDir(Oid group, CGroupComponentType component)
{
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, BASEDIR_GPDB, component, "", path, path_size);

	if (mkdir(path, 0755) && errno != EEXIST)
		return false;

	return true;
}


/*
 * Read at most datasize bytes from a file.
 */
size_t
readData(const char *path, char *data, size_t datasize)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		elog(ERROR, "can't open file '%s': %m", path);

	ssize_t ret = read(fd, data, datasize);

	/* save errno before close() */
	int err = errno;
	close(fd);

	if (ret < 0)
		elog(ERROR, "can't read data from file '%s': %s", path, strerror(err));

	return ret;
}

/*
 * Write datasize bytes to a file.
 */
void
writeData(const char *path, const char *data, size_t datasize)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		elog(ERROR, "can't open file '%s': %m", path);

	ssize_t ret = write(fd, data, datasize);

	/* save errno before close */
	int err = errno;
	close(fd);

	if (ret < 0)
		elog(ERROR, "can't write data to file '%s': %s", path, strerror(err));
	if (ret != datasize)
		elog(ERROR, "can't write all data to file '%s'", path);
}

/*
 * Read an int64 value from a cgroup interface file.
 */
int64
readInt64(Oid group, BaseDirType base, CGroupComponentType component,
		  const char *filename)
{
	int64 x;
	char data[MAX_INT_STRING_LEN];
	size_t data_size = sizeof(data);
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, base, component, filename, path, path_size);

	readData(path, data, data_size);

	if (sscanf(data, "%lld", (long long *) &x) != 1)
		CGROUP_ERROR("invalid number '%s'", data);

	return x;
}

/*
 * Write an int64 value to a cgroup interface file.
 */
void
writeInt64(Oid group, BaseDirType base, CGroupComponentType component,
		   const char *filename, int64 x)
{
	char data[MAX_INT_STRING_LEN];
	size_t data_size = sizeof(data);
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, base, component, filename, path, path_size);

	snprintf(data, data_size, "%lld", (long long) x);

	writeData(path, data, strlen(data));
}

/*
 * Read an int32 value from a cgroup interface file.
 */
int32
readInt32(Oid group, BaseDirType base, CGroupComponentType component,
		  const char *filename)
{
	int32 x;
	char data[MAX_INT_STRING_LEN];
	size_t data_size = sizeof(data);
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, base, component, filename, path, path_size);

	readData(path, data, data_size);

	if (sscanf(data, "%d", &x) != 1)
		CGROUP_ERROR("invalid number '%s'", data);

	return x;
}

/*
 * Write an int32 value to a cgroup interface file.
 */
void
writeInt32(Oid group, BaseDirType base, CGroupComponentType component,
		   const char *filename, int32 x)
{
	char data[MAX_INT_STRING_LEN];
	size_t data_size = sizeof(data);
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, base, component, filename, path, path_size);

	snprintf(data, data_size, "%d", x);

	writeData(path, data, strlen(data));
}

/*
 * Read a string value from a cgroup interface file.
 */
void
readStr(Oid group, BaseDirType base, CGroupComponentType component,
		const char *filename, char *str, int len)
{
	char data[MAX_CGROUP_CONTENTLEN];
	size_t data_size = sizeof(data);
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, base, component, filename, path, path_size);

	readData(path, data, data_size);

	StrNCpy(str, data, len);
}

/*
 * Write a string value to a cgroup interface file.
 */
void
writeStr(Oid group, BaseDirType base, CGroupComponentType component,
		 const char *filename, const char *strValue)
{
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	buildPath(group, base, component, filename, path, path_size);

	writeData(path, strValue, strlen(strValue));
}


bool
deleteDir(Oid group, CGroupComponentType component, const char *filename, bool unassign,
		  void (*detachcgroup) (Oid group, CGroupComponentType component, int fd_dir))
{

	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);

	int retry = unassign ? 0 : MAX_RETRY - 1;
	int fd_dir;

	buildPath(group, BASEDIR_GPDB, component, "", path, path_size);

	/*
	 * To prevent race condition between multiple processes we require a dir
	 * to be removed with the lock acquired first.
	 */
	fd_dir = lockDir(path, true);

	/* the dir is already removed */
	if (fd_dir < 0)
		return true;

	/*
	 * Reset the corresponding control file to zero
	 * RG_FIXME: Can we remove this?
	 */
	if (filename)
		writeInt64(group, BASEDIR_GPDB, component, filename, 0);

	while (++retry <= MAX_RETRY)
	{
		if (unassign)
			detachcgroup(group, component, fd_dir);

		if (rmdir(path))
		{
			int err = errno;

			if (err == EBUSY && unassign && retry < MAX_RETRY)
			{
				elog(DEBUG1, "can't remove dir, will retry: %s: %s",
					 path, strerror(err));
				pg_usleep(1000);
				continue;
			}

			/*
			 * we don't check for ENOENT again as we already acquired the lock
			 * on this dir and the dir still exist at that time, so if then
			 * it's removed by other processes then it's a bug.
			 */
			elog(DEBUG1, "can't remove dir, ignore the error: %s: %s",
				 path, strerror(err));
		}
		break;
	}

	if (retry <= MAX_RETRY)
		elog(DEBUG1, "cgroup dir '%s' removed", path);

	/* close() also releases the lock */
	close(fd_dir);

	return true;
}


int
getCPUCores(void)
{
	int cpucores = 0;

	/*
	 * cpuset ops requires _GNU_SOURCE to be defined,
	 * and _GNU_SOURCE is forced on in src/template/linux,
	 * so we assume these ops are always available on linux.
	 */
	cpu_set_t cpuset;
	int i;

	if (sched_getaffinity(0, sizeof(cpuset), &cpuset) < 0)
		CGROUP_ERROR("can't get cpu cores: %m");

	for (i = 0; i < CPU_SETSIZE; i++)
	{
		if (CPU_ISSET(i, &cpuset))
			cpucores++;
	}

	if (cpucores == 0)
		CGROUP_ERROR("can't get cpu cores");

	return cpucores;
}


/*
 * Get the mount directory of cgroup, the basic method is to read the file "/proc/self/mounts".
 * Normally, cgroup version 1 will return "/sys/fs/cgroup/xxx", so we need remove the "xxx", but
 * version 2 do not need this.
 */
bool
getCgroupMountDir()
{
	struct mntent *me;
	FILE *fp;

	if (strlen(cgroupSystemInfo->cgroup_dir) != 0)
		return true;

	memset(cgroupSystemInfo->cgroup_dir,'\0',sizeof(cgroupSystemInfo->cgroup_dir));

	fp = setmntent(PROC_MOUNTS, "r");
	if (fp == NULL)
		CGROUP_CONFIG_ERROR("can not open '%s' for read", PROC_MOUNTS);

	while ((me = getmntent(fp)))
	{
		char * p;

		if (Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP)
		{
			/* For version 1, we need to find the mnt_type equals to "cgroup" */
			if (strcmp(me->mnt_type, "cgroup"))
				continue;

			strncpy(cgroupSystemInfo->cgroup_dir, me->mnt_dir, sizeof(cgroupSystemInfo->cgroup_dir) - 1);

			p = strrchr(cgroupSystemInfo->cgroup_dir, '/');

			if (p == NULL)
				CGROUP_CONFIG_ERROR("cgroup mount point parse error: %s", cgroupSystemInfo->cgroup_dir);
			else
				*p = 0;
		}
		else
		{
			/* For version 2, we need to find the mnt_type equals to "cgroup2" */
			if (strcmp(me->mnt_type, "cgroup2"))
				continue;

			strncpy(cgroupSystemInfo->cgroup_dir, me->mnt_dir, sizeof(cgroupSystemInfo->cgroup_dir));
		}

		break;
	}

	endmntent(fp);

	return strlen(cgroupSystemInfo->cgroup_dir) != 0;
}

/*
 * Check a list of permissions on group.
 *
 * - if all the permissions are met then return true;
 * - otherwise:
 *   - raise an error if report is true and permList is not optional;
 *   - or return false;
 */
bool
permListCheck(const PermList *permlist, Oid group, bool report)
{
	char path[MAX_CGROUP_PATHLEN];
	size_t path_size = sizeof(path);
	int i;

	if (group == CGROUP_ROOT_ID && permlist->presult)
		*permlist->presult = false;

	foreach_perm_item(i, permlist->items)
	{
		CGroupComponentType component = permlist->items[i].comp;
		const char	*prop = permlist->items[i].prop;
		int			perm = permlist->items[i].perm;

		if (!buildPathSafe(group, BASEDIR_GPDB, component, prop, path, path_size))
		{
			/* Buffer is not large enough for the path */

			if (report && !permlist->optional)
			{
				CGROUP_CONFIG_ERROR("invalid %s name '%s': %m",
									prop[0] ? "file" : "directory",
									path);
			}
			return false;
		}

		if (access(path, perm))
		{
			/* No such file or directory / Permission denied */

			if (report && !permlist->optional)
			{
				CGROUP_CONFIG_ERROR("can't access %s '%s': %m",
									prop[0] ? "file" : "directory",
									path);
			}
			return false;
		}
	}

	if (group == CGROUP_ROOT_ID && permlist->presult)
		*permlist->presult = true;

	return true;
}

bool
normalPermissionCheck(const PermList *permlists, Oid group, bool report)
{
	int i;

	foreach_perm_list(i, permlists)
	{
		const PermList *permList = &permlists[i];

		if (!permListCheck(permList, group, report) && !permList->optional)
			return false;
	}

	return true;
}

bool
cpusetPermissionCheck(const PermList *cpusetPermList, Oid group, bool report)
{
	if (!gp_resource_group_enable_cgroup_cpuset)
		return true;

	if (!permListCheck(cpusetPermList, group, report) && !cpusetPermList->optional)
		return false;

	return true;
}
