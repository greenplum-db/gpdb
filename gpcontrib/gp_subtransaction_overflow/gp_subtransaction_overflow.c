/*-------------------------------------------------------------------------
 *
 * gp_subtransaction_overflow.c
 *	  Get suboverflowed_backends - Backend
 *
 *
 * Copyright (c) 2022-Present VMware Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/lwlock.h"
#include "string.h"
#include "utils/builtins.h"
#include "utils/array.h"
#include "nodes/pg_list.h"

Datum gp_get_suboverflowed_backends(PG_FUNCTION_ARGS);

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(gp_get_suboverflowed_backends);

/*
 * Find the backends where subtransaction overflowed.
 */
Datum
gp_get_suboverflowed_backends(PG_FUNCTION_ARGS)
{
	int 			i;
	ArrayBuildState *astate = NULL;
	
	LWLockAcquire(ProcArrayLock, LW_SHARED);
	for (i = 0; i < ProcGlobal->allProcCount; i++)
	{
		if (ProcGlobal->allPgXact[i].overflowed)
			astate = accumArrayResult(astate,
									  Int32GetDatum(ProcGlobal->allProcs[i].pid),
									  false, INT4OID, CurrentMemoryContext);
	}
	LWLockRelease(ProcArrayLock);

	if (astate)
		PG_RETURN_DATUM(makeArrayResult(astate,
											CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}
