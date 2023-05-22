/*-------------------------------------------------------------------------
 *
 * fmgr_gb.c
 *	  GPDB extra function manager.
 *
 * Portions Copyright (c) 2017-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/fmgr/fmgr_gp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"

/*-------------------------------------------------------------------------
 *		Support routines for callers of nullable fmgr-compatible functions
 *-------------------------------------------------------------------------
 */

/*
 * These are for invocation of a specifically named function with a
 * directly-computed parameter list.  Note that the arguments
 * are not allowed to be NULL, but the result is.  In the latter case, isNull
 * will be set to true.  Also, the function cannot be one that needs to look at
 * FmgrInfo, since there won't be any.
 */
Datum
DirectNullFunctionCall1Coll(PGFunction func, Oid collation, Datum arg1,
						bool *isNull)
{
	LOCAL_FCINFO(fcinfo, 1);
	Datum		result;

	InitFunctionCallInfoData(*fcinfo, NULL, 1, collation, NULL, NULL);

	fcinfo->args[0].value = arg1;
	fcinfo->args[0].isnull = false;

	result = (*func) (fcinfo);

	/* Check for null result */
	*isNull = fcinfo->isnull;
	if (*isNull)
		return (Datum) 0;
	else
		return result;
}
