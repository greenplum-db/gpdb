#include "postgres.h"

#include "optimizer/optimizer.h"
#include "tcop/tcopprot.h"

#include "planner_integration_tests.h"

#include "src/assertions.h"
#include "src/planner_test_helpers.h"


static void
test_stable_function_in_subquery_is_evaluated_to_const()
{
	const char *query_string = "select * from (select now()) a;";

	Query *query = make_query(query_string);
	PlannedStmt *plannedstmt = planner(query, 0, NULL);

	TargetEntry *tle = get_target_entry_from_root_plan_node(plannedstmt);

	assert_that_bool(IsA(tle->expr, Const), is_equal_to(true));
}

static void
test_stable_function_in_simple_query_is_not_evaluated_in_planner()
{
	const char *query_string = "select now();";

	Query *query = make_query(query_string);
	PlannedStmt *plannedstmt = planner(query, 0, NULL);

	TargetEntry *tle = get_target_entry_from_root_plan_node(plannedstmt);

	assert_that_bool(IsA(tle->expr, FuncExpr), is_equal_to(true));
}

void
run_planner_integration_test_suite(void)
{
	/*
	 * Tests that are generic between planner and optimizer
	 */
	test_stable_function_in_subquery_is_evaluated_to_const();
	test_stable_function_in_simple_query_is_not_evaluated_in_planner();
}
