/*
 *
 *  check_greenplum_test.c
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "../../check_greenplum_internal.h"


static int number_of_check_functions_called;
static int number_of_check_oks_reported;
static int number_of_failing_checks_reported;
static void* usedClusterInfoAddress;

/*
 * Fake implementations
 */

void check_ok(void) 
{
	number_of_check_oks_reported++;
}


void check_failed(void)
{
	number_of_failing_checks_reported++;
}


/*
 * Fake checks
 */

static bool failing_check(ClusterInfo *cluster)
{
	return false;
}


static bool passing_check(ClusterInfo *cluster)
{
	usedClusterInfoAddress = cluster;
	number_of_check_functions_called++;
	return true;
}


/*
 * Test helper functions
 */

static void
setup()
{
	number_of_check_functions_called = 0;
	number_of_check_oks_reported = 0;
	number_of_failing_checks_reported = 0;
	usedClusterInfoAddress = NULL;
}


/*
 * Tests
 */

static void
test_check_greenplum_runs_all_given_checks(void **state)
{
	setup();

	ClusterInfo clusterInfo;

	check_function my_list[] = {
		passing_check,
		passing_check
	};

	perform_greenplum_checks(my_list, 2, &clusterInfo);

	assert_int_equal(number_of_check_functions_called, 2);
	assert_int_equal(number_of_failing_checks_reported, 0);
}


static void
test_check_greenplum_calls_check_ok_for_success(void **state)
{
	setup();

	ClusterInfo clusterInfo;

	check_function my_list[] = {
		passing_check,
		passing_check
	};

	perform_greenplum_checks(my_list, 2, &clusterInfo);

	assert_int_equal(number_of_check_oks_reported, 2);
	assert_int_equal(number_of_failing_checks_reported, 0);
}


static void
test_check_greenplum_calls_gp_report_failure_on_failure(void **state)
{
	setup();
	
	ClusterInfo clusterInfo;

	check_function my_list[] = {
		passing_check,
		failing_check
	};

	perform_greenplum_checks(my_list, 2, &clusterInfo);

	assert_int_equal(number_of_failing_checks_reported, 1);
	assert_int_equal(number_of_check_functions_called, 1);
}


static void
test_check_greenplum_uses_given_cluster_to_check(void **state)
{
	setup();

	ClusterInfo clusterInfo;

	check_function my_list[] = {
		passing_check
	};

	perform_greenplum_checks(my_list, 1, &clusterInfo);

	assert_int_equal(&clusterInfo, usedClusterInfoAddress);
}


int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_check_greenplum_runs_all_given_checks),
		unit_test(test_check_greenplum_calls_check_ok_for_success),
		unit_test(test_check_greenplum_calls_gp_report_failure_on_failure),
		unit_test(test_check_greenplum_uses_given_cluster_to_check)
	};

	return run_tests(tests);
}