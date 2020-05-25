/*-------------------------------------------------------------------------
 *
 * exttable_fdw_shim.h
 *	  routines for making legacy GPDB external tables look like a FDW
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/exttable_fdw_shim.h
*
*-------------------------------------------------------------------------
*/
#ifndef EXTTABLE_FDW_SHIM_H
#define EXTTABLE_FDW_SHIM_H

#include "fmgr.h"
#include "nodes/plannodes.h"

extern Datum exttable_fdw_handler(PG_FUNCTION_ARGS);

extern Datum pg_exttable(PG_FUNCTION_ARGS);

extern void gp_exttable_permission_check(PG_FUNCTION_ARGS);

#endif   /* EXTTABLE_FDW_SHIM_H */
