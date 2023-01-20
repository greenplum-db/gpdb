/*-------------------------------------------------------------------------
 *
 * resource_manager.h
 *	  GPDB resource manager definitions.
 *
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *		src/include/utils/resource_manager.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "utils/resscheduler.h"
#include "utils/resgroup.h"

#define IsResQueueEnabled() \
	(ResourceScheduler && \
	 Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_QUEUE)

/*
 * Caution: resource group may be enabled but not activated.
 */
#define IsResGroupEnabled() \
	(ResourceScheduler && \
	 (Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP || \
	  Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP_V2))

/*
 * Resource group do not govern the auxiliary processes and special backends
 * like ftsprobe, filerep process, so we need to check if resource group is
 * actually activated
 */
#define IsResGroupActivated() \
	(ResGroupActivated)

typedef enum
{
	RESOURCE_MANAGER_POLICY_QUEUE,
	RESOURCE_MANAGER_POLICY_GROUP,			/* Linux Cgroup v1 */
	RESOURCE_MANAGER_POLICY_GROUP_V2,		/* Linux Cgroup v2 */
} ResourceManagerPolicy;

/*
 * GUC variables.
 */
extern bool	ResourceScheduler;
extern ResourceManagerPolicy Gp_resource_manager_policy;
extern bool ResGroupActivated;

extern void ResManagerShmemInit(void);
extern void InitResManager(void);

#endif   /* RESOURCEMANAGER_H */
