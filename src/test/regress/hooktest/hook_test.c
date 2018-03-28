#include "postgres.h"

#include "fmgr.h"
#include "optimizer/planner.h"
#include "tcop/idle_resource_cleaner.h"

PG_MODULE_MAGIC;

static planner_hook_type prev_planner_hook = NULL;

static PlannedStmt *test_planner_hook(Query *parse, int cursorOptions, ParamListInfo boundParams);

static int	test_get_idle_session_timeout(void);
static void test_idle_session_timeout_action(void);

void		_PG_init(void);
void		_PG_fini(void);

void
_PG_init(void)
{
	prev_planner_hook = planner_hook;
	planner_hook = test_planner_hook;

	get_idle_session_timeout_hook = test_get_idle_session_timeout;
	idle_session_timeout_action_hook = test_idle_session_timeout_action;
}

void
_PG_fini(void)
{
	planner_hook = prev_planner_hook;

	get_idle_session_timeout_hook = NULL;
	idle_session_timeout_action_hook = NULL;
}

static PlannedStmt *
test_planner_hook(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *stmt;

	elog(LOG, "In test_planner_hook");

	if (prev_planner_hook)
		stmt = (*prev_planner_hook) (parse, cursorOptions, boundParams);
	else
		stmt = standard_planner(parse, cursorOptions, boundParams);

	return stmt;
}

static int
test_get_idle_session_timeout(void)
{
	return 50;					/* ms */
}

static void
test_idle_session_timeout_action(void)
{
	elog(LOG, "In test_idle_session_timeout_action");
}
