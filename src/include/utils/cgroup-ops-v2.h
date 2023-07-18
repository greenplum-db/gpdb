/*-------------------------------------------------------------------------
 *
 * cgroup-ops-v1.h
 *	  GPDB resource group definitions.
 *
 * Copyright (c) 2017 VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/cgroup-ops-v2.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_OPS_V2_H
#define RES_GROUP_OPS_V2_H

#include "utils/cgroup.h"

extern CGroupOpsRoutine *get_group_routine_v2(void);
extern CGroupSystemInfo *get_cgroup_sysinfo_v2(void);

#endif   /* RES_GROUP_OPS_V2_H */
