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

#include "utils/resscheduler.h"
#include "utils/resgroup.h"

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

extern bool IsResQueueEnabled(void);
extern bool IsResGroupEnabled(void);

extern void InitResManager(void);

#endif   /* RESOURCEMANAGER_H */
