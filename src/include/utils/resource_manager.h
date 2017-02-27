/*-------------------------------------------------------------------------
 *
 * resource_manager.h
 *	  GPDB resource manager definitions.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 * IDENTIFICATION
 *	  $PostgreSQL: $
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESOURCEMANGER_H
#define RESOURCEMANGER_H

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

#endif   /* RESOURCEMANGER_H */
