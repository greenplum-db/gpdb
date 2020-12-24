/*-------------------------------------------------------------------------
 *
 * exttable_fdw_shim.h
 *	  routines for making legacy GPDB external tables look like a FDW
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

extern Datum pg_exttable(PG_FUNCTION_ARGS);

extern ForeignScan *create_foreignscan_for_external_table(Oid relid, Index scanrelid, List *qual, List *targetlist);

#endif   /* EXTTABLE_FDW_SHIM_H */
