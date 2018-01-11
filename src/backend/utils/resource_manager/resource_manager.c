/*-------------------------------------------------------------------------
 *
 * resource_manager.c
 *	  GPDB resource manager code.
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resource_manager/resource_manager.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "executor/spi.h"
#include "postmaster/fts.h"
#include "replication/walsender.h"
#include "utils/guc.h"
#include "utils/resource_manager.h"
#include "utils/resgroup-ops.h"

/*
 * GUC variables.
 */
bool	ResourceScheduler = false;						/* Is scheduling enabled? */
ResourceManagerPolicy Gp_resource_manager_policy;

static bool resourceGroupActivated = false;

bool
IsResQueueEnabled(void)
{
	return ResourceScheduler &&
		Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_QUEUE;
}

/*
 * Caution: resource group may be enabled but not activated.
 */
bool
IsResGroupEnabled(void)
{
	return ResourceScheduler &&
		Gp_resource_manager_policy == RESOURCE_MANAGER_POLICY_GROUP;
}

/*
 * Resource group do not govern the auxiliary processes and special backends
 * like ftsprobe, filerep process, so we need to check if resource group is
 * actually activated
 */
bool
IsResGroupActivated(void)
{
	return IsResGroupEnabled() && resourceGroupActivated;
}

void
ResManagerShmemInit(void)
{
	if (IsResQueueEnabled() && Gp_role == GP_ROLE_DISPATCH)
	{
		InitResScheduler();
		InitResPortalIncrementHash();
	}
	else if (IsResGroupEnabled() && !IsUnderPostmaster)
	{
		ResGroupControlInit();
	}
}

void
InitResManager(void)
{
	if (IsResQueueEnabled() && Gp_role == GP_ROLE_DISPATCH && !am_walsender)
	{
		gp_resmanager_memory_policy = &gp_resqueue_memory_policy;
		gp_log_resmanager_memory = &gp_log_resqueue_memory;
		gp_resmanager_print_operator_memory_limits = &gp_resqueue_print_operator_memory_limits;
		gp_resmanager_memory_policy_auto_fixed_mem = &gp_resqueue_memory_policy_auto_fixed_mem;

		InitResQueues();
	}
	else if  (IsResGroupEnabled() &&
			 (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE) &&
			 IsUnderPostmaster &&
			 !am_walsender && !am_ftshandler)
	{
		/*
		 * InitResManager() is called under PostgresMain(), so resource group is not
		 * initialized for auxiliary processes and other special backends. eg
		 * checkpointer, ftsprobe and filerep processes. Wal sender acts like a backend,
		 * so we also need to exclude it.
		 */
		gp_resmanager_memory_policy = &gp_resgroup_memory_policy;
		gp_log_resmanager_memory = &gp_log_resgroup_memory;
		gp_resmanager_memory_policy_auto_fixed_mem = &gp_resgroup_memory_policy_auto_fixed_mem;
		gp_resmanager_print_operator_memory_limits = &gp_resgroup_print_operator_memory_limits;

		InitResGroups();
		ResGroupOps_AdjustGUCs();

		resourceGroupActivated = true;
	}
	else
	{
		gp_resmanager_memory_policy = &gp_resmanager_memory_policy_default;
		gp_log_resmanager_memory = &gp_log_resmanager_memory_default;
		gp_resmanager_memory_policy_auto_fixed_mem = &gp_resmanager_memory_policy_auto_fixed_mem_default;
		gp_resmanager_print_operator_memory_limits = &gp_resmanager_print_operator_memory_limits_default;
	}

	if (!IsResManagerMemoryPolicyNone())
	{
		SPI_InitMemoryReservation();
	}
}
