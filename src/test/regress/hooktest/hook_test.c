#include "postgres.h"

#include "fmgr.h"
#include "optimizer/planner.h"

PG_MODULE_MAGIC;

static planner_hook_type	prev_planner_hook = NULL;

static PlannedStmt * test_hook(Query *parse, int cursorOptions, ParamListInfo boundParams);
void _PG_init(void);
void _PG_fini(void);

void
_PG_init(void)
{
	prev_planner_hook = planner_hook;
	planner_hook = test_hook;
}

void
_PG_fini(void)
{
	planner_hook = prev_planner_hook;
}

static PlannedStmt *
test_hook(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *stmt;

	elog(LOG, "In test_hook");

	if (prev_planner_hook)
		stmt = (* prev_planner_hook)(parse, cursorOptions, boundParams);
	else
		stmt = standard_planner(parse, cursorOptions, boundParams);

	return stmt;
}
