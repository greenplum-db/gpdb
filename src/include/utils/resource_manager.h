/*-------------------------------------------------------------------------
 *
 * resource_manager.h
 *	  GPDB resource manager definitions.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 * IDENTIFICATION
 *		src/include/utils/resource_manager.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

typedef enum
{
	RESOURCE_MANAGER_POLICY_QUEUE,
	RESOURCE_MANAGER_POLICY_GROUP,
} ResourceManagerPolicy;

/*
 * GUC variables.
 */
extern bool	ResourceScheduler;
extern ResourceManagerPolicy Gp_resource_manager_policy;

#define IsResQueueEnabled() \
	(bool)(ResourceScheduler && Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_QUEUE)
#define IsResGroupEnabled() \
	(bool)(ResourceScheduler && Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP)


#endif   /* RESOURCEMANAGER_H */
