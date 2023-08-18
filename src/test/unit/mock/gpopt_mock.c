#include "postgres.h"

#include "fmgr.h"
#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"

char *
SerializeDXLPlan(Query *pquery)
{
	elog(ERROR, "mock implementation of SerializeDXLPlan called");
	return NULL;
}

PlannedStmt *
GPOPTOptimizedPlan(Query *pquery, bool pfUnexpectedFailure)
{
	elog(ERROR, "mock implementation of GPOPTOptimizedPlan called");
	return NULL;
}

Datum
LibraryVersion(void)
{
	elog(ERROR, "mock implementation of LibraryVersion called");
	return (Datum) 0;
}

Datum
EnableXform(PG_FUNCTION_ARGS)
{
	elog(ERROR, "mock implementation of EnableXform called");
	return (Datum) 0;
}

Datum
DisableXform(PG_FUNCTION_ARGS)
{
	elog(ERROR, "mock implementation of EnableXform called");
	return (Datum) 0;
}

void
InitGPOPT ()
{
	elog(ERROR, "mock implementation of InitGPOPT called");
}

void
TerminateGPOPT ()
{
	elog(ERROR, "mock implementation of TerminateGPOPT called");
}
