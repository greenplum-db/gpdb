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
#include "access/xlog_fn.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(gp_create_restore_point);
PG_FUNCTION_INFO_V1(gp_switch_wal);

Datum
gp_create_restore_point(PG_FUNCTION_ARGS)
{
	return gp_create_restore_point_internal(fcinfo);
}

Datum
gp_switch_wal(PG_FUNCTION_ARGS)
{
	return gp_switch_wal_internal(fcinfo);
}
