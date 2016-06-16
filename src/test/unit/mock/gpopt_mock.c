#include "postgres.h"

#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"

char *
SzDXLPlan(Query *pquery)
{
	elog(ERROR, "mock implementation of SzDXLPlan called");
	return NULL;
}

PlannedStmt *
PplstmtOptimize(Query *pquery, bool pfUnexpectedFailure)
{
	elog(ERROR, "mock implementation of PplStmtOptimize called");
	return NULL;
}

Datum
LibraryVersion(void)
{
	elog(ERROR, "mock implementation of LibraryVersion called");
	return NULL;
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

StringInfo
OptVersion(void)
{
	elog(ERROR, "mock implementation of OptVersion called");
	return (Datum) 0;
}

void
InitGPOPT (bool use_gpdb_allocators)
{
	elog(ERROR, "mock implementation of InitGPOPT called");
}

void
TerminateGPOPT ()
{
	elog(ERROR, "mock implementation of TerminateGPOPT called");
}
