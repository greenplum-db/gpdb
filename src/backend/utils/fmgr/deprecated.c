/*-------------------------------------------------------------------------
 *
 * deprecated.c
 *
 * Utility function to support deprecating functions from one release
 * to the next.  Deleting the function outright as part of upgrade is
 * not good because it could theoretically cascade to user views, so 
 * instead the definition of a deprecated function should be changed
 * to point to this function instead which will provide an error message
 * when the function is called.
 *
 * Portions Copyright (c) 2010, Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/fmgr/deprecated.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "funcapi.h"

#include "utils/builtins.h"

Datum
gp_deprecated(PG_FUNCTION_ARGS)
{
	/* Lookup the function that was called in the catalog */
	Oid   procOid  = fcinfo->flinfo->fn_oid;
	char *procName = format_procedure(procOid);

	/* Return error that the function is deprecated */
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function %s has been deprecated", procName)));

	/* unreachable */
	PG_RETURN_NULL();
}
