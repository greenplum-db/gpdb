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

extern CGroupOpsRoutine *get_cgroup_routine_dummy(void);
extern CGroupSystemInfo *get_cgroup_sysinfo_dummy(void);

#endif   /* RES_GROUP_OPS_DUMMY_H */
