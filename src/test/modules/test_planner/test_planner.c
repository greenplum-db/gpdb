/*--------------------------------------------------------------------------
 *
 * test_planner.c
 *		Test correctness of optimizer's predicate proof logic.
 *
 * Copyright (c) 2018, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		src/test/modules/test_planner/test_planner.c
 *
 * -------------------------------------------------------------------------
 */

#include "postgres.h"
#include "funcapi.h"

#include "integration_tests/planner_integration_tests.h"

PG_MODULE_MAGIC;
extern Datum test_planner(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(test_planner);

Datum
test_planner(PG_FUNCTION_ARGS)
{
	run_planner_integration_test_suite();


	PG_RETURN_VOID();
}
