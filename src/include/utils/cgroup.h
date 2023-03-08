/*-------------------------------------------------------------------------
 *
 * cgroup.h
 *	  Linux control group interface definitions.
 *
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/cgroup.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CGROUP_H
#define CGROUP_H

#include "postgres.h"

#define MAX_CGROUP_PATHLEN 256
#define MAX_CGROUP_CONTENTLEN 1024  

#define CGROUP_ERROR(...) elog(ERROR, __VA_ARGS__)
#define CGROUP_CONFIG_ERROR(...) \
	CGROUP_ERROR("cgroup is not properly configured: " __VA_ARGS__)

#define FALLBACK_COMP_DIR ""
#define PROC_MOUNTS "/proc/self/mounts"
#define MAX_INT_STRING_LEN 20
#define MAX_RETRY 10

/*
 * Default cpuset group is a group manages the cpu cores which not belong to
 * any other cpuset group. All the processes which not belong to any cpuset
 * group will be run on cores in default cpuset group. It is a virtual group,
 * can't be seen in gpdb.
 */
#define DEFAULT_CPUSET_GROUP_ID 1

/*
 * If cpu_hard_quota_limit is set to this value, it means this feature is disabled.
 * And meanwhile, it also means the process can use CPU resource infinitely.
 */
#define CPU_HARD_QUOTA_LIMIT_DISABLED (-1)

/* This is the default value about Linux Control Group */
#define DEFAULT_CPU_PERIOD_US 100000LL


/*
 * Resource Group underlying component types.
 */
typedef enum
{
	CGROUP_COMPONENT_FIRST			= 0,
	CGROUP_COMPONENT_UNKNOWN		= -1,
	CGROUP_COMPONENT_PLAIN			= -2,

	/*
	 * let CGROUP_COMPONENT_CPU equals to CGROUP_COMPONENT_FIRST,
	 * it's convinent to loop all the control component from zero.
	 */
	CGROUP_COMPONENT_CPU			= 0,
	CGROUP_COMPONENT_CPUACCT,
	CGROUP_COMPONENT_CPUSET,

	CGROUP_COMPONENT_COUNT,
} CGroupComponentType;


typedef enum
{
	BASEDIR_GPDB,		/* translate to "/gpdb" */
	BASEDIR_PARENT,		/* translate to "" */
} BaseDirType;

#define CGROUP_ROOT_ID (InvalidOid)

typedef struct CGroupSystemInfo
{
	/* The number of CPU cores on this machine */
	int ncores;

	/* The cgroup mount dir */
	char cgroup_dir[MAX_CGROUP_PATHLEN];

} CGroupSystemInfo;

/*
 * For permission check
 */
typedef struct PermItem PermItem;
typedef struct PermList PermList;

struct PermItem
{
	CGroupComponentType comp;
	/* file name, "" means parent directory */
	const char			*prop;
	/* permission, R_OK | W_OK | X_OK */
	int					perm;
};

struct PermList
{
	const PermItem	*items;
	bool			optional;
	bool			*presult;
};

#define foreach_perm_list(i, lists) \
	for ((i) = 0; (lists)[(i)].items; (i)++)

#define foreach_perm_item(i, items) \
	for ((i) = 0; (items)[(i)].comp != CGROUP_COMPONENT_UNKNOWN; (i)++)

#define foreach_comp_type(comp) \
	for ((comp) = CGROUP_COMPONENT_FIRST; \
		 (comp) < CGROUP_COMPONENT_COUNT; \
		 (comp)++)

/* Read at most datasize bytes from a file. */
extern size_t readData(const char *path, char *data, size_t datasize);
/* Write datasize bytes to a file. */
extern void writeData(const char *path, const char *data, size_t datasize);

/* Read an int64 value from a cgroup interface file. */
extern int64 readInt64(Oid group, BaseDirType base, CGroupComponentType component,
					   const char *filename);
/* Write an int64 value to a cgroup interface file. */
extern void writeInt64(Oid group, BaseDirType base, CGroupComponentType component,
					   const char *filename, int64 x);

/* Read an int32 value from a cgroup interface file. */
extern int32 readInt32(Oid group, BaseDirType base, CGroupComponentType component,
					   const char *filename);
/* Write an int32 value to a cgroup interface file. */
extern void writeInt32(Oid group, BaseDirType base, CGroupComponentType component,
					   const char *filename, int32 x);

/* Read a string value from a cgroup interface file. */
extern void readStr(Oid group, BaseDirType base, CGroupComponentType component,
					const char *filename, char *str, int len);
/* Write a string value to a cgroup interface file. */
extern void writeStr(Oid group, BaseDirType base, CGroupComponentType component,
					 const char *filename, const char *strValue);

extern void buildPath(Oid group, BaseDirType base, CGroupComponentType component,
					  const char *filename, char *pathBuffer, size_t pathBufferSize);
extern bool buildPathSafe(Oid group, BaseDirType base, CGroupComponentType component,
						  const char *filename, char *pathBuffer, size_t pathBufferSize);

extern bool validateComponentDir(CGroupComponentType component);

/* Permission check */
extern bool permListCheck(const PermList *permlist, Oid group, bool report);
extern bool normalPermissionCheck(const PermList *permlists, Oid group, bool report);
extern bool cpusetPermissionCheck(const PermList *cpusetPermList, Oid group, bool report);

extern const char * getComponentName(CGroupComponentType component);
extern CGroupComponentType getComponentType(const char *name);
extern const char *getComponentDir(CGroupComponentType component);
extern void setComponentDir(CGroupComponentType component, const char *dir);

extern int lockDir(const char *path, bool block);

/* Create cgroup dir. */
extern bool createDir(Oid group, CGroupComponentType comp);
/* Delete cgroup dir. */
extern bool deleteDir(Oid group, CGroupComponentType component, const char *filename, bool unassign,
					  void (*detachcgroup) (Oid group, CGroupComponentType component, int fd_dir));

extern int getCPUCores(void);
extern bool getCgroupMountDir(void);

/*
 * Interfaces for OS dependent operations
 */

typedef const char *(*getcgroupname_function) (void);

/* Probe the configuration for the OS group implementation. */
typedef bool (*probecgroup_function) (void);
/* Check whether the OS group implementation is available and usable. */
typedef void (*checkcgroup_function) (void);

/* Initialize the OS group. */
typedef void (*initcgroup_function) (void);

/* Adjust GUCs for this OS group implementation. */
typedef void (*adjustgucs_function) (void);

/* Create OS cgroup. */
typedef void (*createcgroup_function) (Oid group);
/* Destroy OS cgroup. */
typedef void (*destroycgroup_function) (Oid group, bool migrate);

/* Attach a process to the OS cgroup. */
typedef void (*attachcgroup_function) (Oid group, int pid, bool is_cpuset_enabled);
/* detach a process to the OS cgroup. */
typedef void (*detachcgroup_function) (Oid group, CGroupComponentType component, int fd_dir);

/* Lock the OS group. */
typedef int (*lockcgroup_function) (Oid group, CGroupComponentType component, bool block);
/* Unlock the OS group. */
typedef void (*unlockcgroup_function) (int fd);

/* Set the cpu limit. */
typedef void (*setcpulimit_function) (Oid group, int cpu_hard_quota_limit);
/* Set the cpu share. */
typedef void (*setcpupriority_function) (Oid group, int cpu_soft_priority);

/* Get the cpu usage of the OS group. */
typedef int64 (*getcpuusage_function) (Oid group);

/* Get the cpuset configuration of a cgroup. */
typedef void (*getcpuset_function) (Oid group, char *cpuset, int len);

/* Set the cpuset configuration of a cgroup. */
typedef void (*setcpuset_function) (Oid group, const char *cpuset);

/* Convert the cpu usage to percentage within the duration. */
typedef float (*convertcpuusage_function) (int64 usage, int64 duration);


typedef struct CGroupOpsRoutine
{
	getcgroupname_function 	getcgroupname;

	probecgroup_function	probecgroup;
	checkcgroup_function 	checkcgroup;

	initcgroup_function 	initcgroup;

	adjustgucs_function 	adjustgucs;

	createcgroup_function 	createcgroup;
	destroycgroup_function 	destroycgroup;

	attachcgroup_function	attachcgroup;
	detachcgroup_function 	detachcgroup;

	lockcgroup_function 	lockcgroup;
	unlockcgroup_function 	unlockcgroup;

	setcpulimit_function 	setcpulimit;

	setcpupriority_function	setcpupriority;

	getcpuusage_function 	getcpuusage;

	getcpuset_function 		getcpuset;
	setcpuset_function		setcpuset;

	convertcpuusage_function convertcpuusage;
} CGroupOpsRoutine;

/* The global function handler. */
extern CGroupOpsRoutine *cgroupOpsRoutine;

/* The global system info. */
extern CGroupSystemInfo *cgroupSystemInfo;

#endif	/* CGROUP_H */
