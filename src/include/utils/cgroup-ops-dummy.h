/*-------------------------------------------------------------------------
 *
 * cgroup-ops-dummy.h
 *	  GPDB resource group definitions.
 *
 * Copyright (c) 2017 VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/cgroup-ops-dummy.h
 *
 * This file is for the OS that do not support cgroup, such as Windows, MacOS.
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_OPS_DUMMY_H
#define RES_GROUP_OPS_DUMMY_H

#include "utils/cgroup.h"

extern const char *getcgroupname_dummy(void);

extern bool probecgroup_dummy(void);

extern void checkcgroup_dummy(void);

extern void initcgroup_dummy(void);

extern void adjustgucs_dummy(void);

extern void createcgroup_dummy(Oid group);

extern void attachcgroup_dummy(Oid group, int pid, bool is_cpuset_enabled);

extern void detachcgroup_dummy(Oid group, CGroupComponentType component, int fd_dir);

extern void destroycgroup_dummy(Oid group, bool migrate);

extern int lockcgroup_dummy(Oid group, CGroupComponentType component, bool block);

extern void unlockcgroup_dummy(int fd);

extern void setcpulimit_dummy(Oid group, int cpu_rate_limit);

extern void setmemorylimitbychunks_dummy(Oid group, int32 memory_limit_chunks);

extern void setmemorylimit_dummy(Oid group, int memory_limit);

extern int64 getcpuusage_dummy(Oid group);

extern int32 getmemoryusage_dummy(Oid group);

extern int32 getmemorylimitchunks_dummy(Oid group);

extern void getcpuset_dummy(Oid group, char *cpuset, int len);

extern void setcpuset_dummy(Oid group, const char *cpuset);

extern float convertcpuusage_dummy(int64 usage, int64 duration);

extern void cgroup_handler_dummy(void);

#endif   /* RES_GROUP_OPS_DUMMY_H */
