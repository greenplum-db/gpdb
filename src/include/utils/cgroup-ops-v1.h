/*-------------------------------------------------------------------------
 *
 * cgroup-ops-v1.h
 *	  GPDB resource group definitions.
 *
 * Copyright (c) 2017 VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/cgroup-ops-v1.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_OPS_V1_H
#define RES_GROUP_OPS_V1_H

#include "utils/cgroup.h"

extern const char *getcgroupname_v1(void);

extern bool probecgroup_v1(void);

extern void checkcgroup_v1(void);

extern void initcgroup_v1(void);

extern void adjustgucs_v1(void);

extern void createcgroup_v1(Oid group);

extern void attachcgroup_v1(Oid group, int pid, bool is_cpuset_enabled);

extern void detachcgroup_v1(Oid group, CGroupComponentType component, int fd_dir);

extern void destroycgroup_v1(Oid group, bool migrate);

extern int lockcgroup_v1(Oid group, CGroupComponentType component, bool block);

extern void unlockcgroup_v1(int fd);

extern void setcpulimit_v1(Oid group, int cpu_rate_limit);

extern int64 getcpuusage_v1(Oid group);

extern void getcpuset_v1(Oid group, char *cpuset, int len);

extern void setcpuset_v1(Oid group, const char *cpuset);

extern float convertcpuusage_v1(int64 usage, int64 duration);

extern void cgroup_handler_alpha(void);

#endif   /* RES_GROUP_OPS_V1_H */
