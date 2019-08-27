/*-------------------------------------------------------------------------
 *
 * faultinject.c
 *	  Fault injection utilities
 * 
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/error/faultinject.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "funcapi.h"

#include "miscadmin.h"

#include <unistd.h>

/* exposed in pg_proc.h */
extern Datum gp_fault_inject(PG_FUNCTION_ARGS) ;
Datum gp_fault_inject(PG_FUNCTION_ARGS) 
{
	/*
	 * The fault injection functionality is provided by
	 * gpcontrib/gp_inject_fault module.  This function definition is kept
	 * only to avoid a catalog change in Greenplum 6.  It has been removed
	 * in newer versions.
	 */
	elog(ERROR, "not implemented");
}
