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

#include "utils/guc.h"
#include "utils/resource_manager.h"

/*
 * GUC variables.
 */
bool	ResourceScheduler;						/* Is scheduling enabled? */
ResourceManagerPolicy Gp_resource_manager_policy;
