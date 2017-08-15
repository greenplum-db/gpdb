/*-------------------------------------------------------------------------
 *
 * resource_manager.c
 *	  GPDB resource manager code.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 *
 -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "utils/guc.h"
#include "utils/resource_manager.h"

/*
 * GUC variables.
 */
bool	ResourceScheduler = false;						/* Is scheduling enabled? */
ResourceManagerPolicy Gp_resource_manager_policy;

bool
IsResQueueEnabled(void)
{
	return ResourceScheduler &&
		Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_QUEUE;
}

bool
IsResGroupEnabled(void)
{
	return ResourceScheduler &&
		Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP;
}

void
InitResManager(void)
{
	if (IsResQueueEnabled() && Gp_role == GP_ROLE_DISPATCH)
	{
		InitResScheduler();
		InitResPortalIncrementHash();

		gp_resmanager_memory_policy = &gp_resqueue_memory_policy;
		gp_log_resmanager_memory = &gp_log_resqueue_memory;
		gp_resmanager_print_operator_memory_limits = &gp_resqueue_print_operator_memory_limits;
		gp_resmanager_memory_policy_auto_fixed_mem = &gp_resqueue_memory_policy_auto_fixed_mem;
	}
	else if (IsResGroupEnabled() && !IsUnderPostmaster)
	{
		ResGroupControlInit();

		gp_resmanager_memory_policy = &gp_resgroup_memory_policy;
		gp_log_resmanager_memory = &gp_log_resgroup_memory;
		gp_resmanager_print_operator_memory_limits = &gp_resgroup_print_operator_memory_limits;
		gp_resmanager_memory_policy_auto_fixed_mem = &gp_resgroup_memory_policy_auto_fixed_mem;
	}
	else
	{
		gp_resmanager_memory_policy = &gp_resmanager_memory_policy_default;
		gp_log_resmanager_memory = &gp_log_resmanager_memory_default;
		gp_resmanager_memory_policy_auto_fixed_mem = &gp_resmanager_memory_policy_auto_fixed_mem_default;
		gp_resmanager_print_operator_memory_limits = &gp_resmanager_print_operator_memory_limits_default;
	}
}
