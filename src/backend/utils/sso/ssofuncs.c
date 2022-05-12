/*-------------------------------------------------------------------------
 *
 * ssofuncs.c
 *	  Search The PIDs of Overflowed Subtransactions  - Helper Functions
 *
 *
 * Copyright (c) 2022-Present VMware, Inc. or its affiliates.
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

Datum
gp_subtrx_overflowed_pid(PG_FUNCTION_ARGS)
{
	int			i;
	int			len;
	ArrayType	*retval;
	int			lb[1];
	bool		*nulls;
	Datum		*d;

	LWLockAcquire(ProcArrayLock, LW_SHARED);
	len = ProcGlobal->allProcCount;
 
	d = (Datum*) palloc(sizeof(Datum) * len);
	nulls = (bool *) palloc(sizeof(bool) * len);

	for (i = 0; i < len; i++)
	{
		if (ProcGlobal->allPgXact[i].overflowed)
		{
			d[i] = Int32GetDatum(ProcGlobal->allProcs[i].pid);
			nulls[i] = false;
		}
		else
			nulls[i] = true;
	}
	lb[0] = 1;
	retval = construct_md_array(d, nulls, 1, &len, lb,
								   INT4OID, 4, true, 'i');
	LWLockRelease(ProcArrayLock);
	PG_RETURN_ARRAYTYPE_P(retval);
}
