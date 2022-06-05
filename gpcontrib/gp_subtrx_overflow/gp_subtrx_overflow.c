#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/lwlock.h"
#include "string.h"
#include "utils/builtins.h"

Datum gp_find_subtx_overflowed_pids(PG_FUNCTION_ARGS);

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(gp_find_subtx_overflowed_pids);
/*
 * Find the pids when subtransaction overflowed.
 */
Datum
gp_find_subtx_overflowed_pids(PG_FUNCTION_ARGS)
{
	int				i;
	StringInfoData 	buf;
	bool 			traverse = false;

	initStringInfo(&buf);

	appendStringInfoChar(&buf, '{');
	LWLockAcquire(ProcArrayLock, LW_SHARED);
	for (i = 0; i < ProcGlobal->allProcCount; i++)
	{
		if (ProcGlobal->allPgXact[i].overflowed)
		{
			if (traverse)
				appendStringInfoString(&buf, ",");
			appendStringInfo(&buf, "%d", ProcGlobal->allProcs[i].pid);
			traverse = true;
		}
	}
	LWLockRelease(ProcArrayLock);
	appendStringInfoChar(&buf, '}');
	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}