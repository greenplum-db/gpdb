/*--------------------------------------------------------------------------
 *
 * gp_pitr.c
 *	  Backports routines for creating named restore points
 *
 * Portions Copyright (c) 2020-Present Pivotal Software, Inc.
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "fmgr.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(gp_set_recovery_pause_target);

Datum
gp_set_recovery_pause_target(PG_FUNCTION_ARGS)
{
	return gp_set_recovery_pause_target_internal(fcinfo);
}
