#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "../check_gp.h"
#include "../pg_upgrade.h"

ClusterInfo old_cluster,
			new_cluster;
OSInfo		os_info;
char	   *output_files[] = {
	SERVER_LOG_FILE,
	UTILITY_LOG_FILE,
	INTERNAL_LOG_FILE,
	NULL
};

static void
test_check_with_no_databases(void **state)
{
	/*
	 * Given a cluster with 1 database
	 */
	old_cluster.dbarr.ndbs = 0;

	expect_string(prep_status, fmt, "Checking for heterogeneous partitioned tables");
	will_be_called(prep_status);

	/*
	 * When no query are run
	 */

	/*
	 * Then the check should pass
	 */
	will_be_called(check_ok);

	check_heterogeneous_partition();
}

static void
test_check_with_one_database_and_no_mismatches(void **state)
{
	/*
	 * Given a cluster with 1 database
	 */
	old_cluster.dbarr.ndbs = 1;
	old_cluster.dbarr.dbs = (DbInfo*)malloc(sizeof(DbInfo));

	expect_string(prep_status, fmt, "Checking for heterogeneous partitioned tables");
	will_be_called(prep_status);

	expect_any(connectToServer, cluster);
	expect_any(connectToServer, db_name);
	will_return(connectToServer, NULL);

	/*
	 * When the query to check different columns counts returns 0 results
	 */
	expect_any(executeQueryOrDie, conn);
	expect_string(executeQueryOrDie, fmt, CHECK_PARTITION_TABLE_MATCHES_COLUMN_COUNT);
	will_return(executeQueryOrDie, NULL);
	expect_any(PQntuples, res);
	will_return(PQntuples, NULL);
	expect_any(PQclear, res);
	will_be_called(PQclear);

	/*
	 * And the query to check different columns attributes returns 0 results
	 */
	expect_any(executeQueryOrDie, conn);
	expect_string(executeQueryOrDie, fmt, CHECK_PARTITION_TABLE_MATCHES_COLUMN_ATTRIBUTES);
	will_return(executeQueryOrDie, NULL);
	expect_any(PQntuples, res);
	will_return(PQntuples, NULL);
	expect_any(PQclear, res);
	will_be_called(PQclear);

	/*
	 * Then the check should pass
	 */
	will_be_called(check_ok);

	check_heterogeneous_partition();
}

static void
test_check_with_one_database_and_mismatch_number_of_columns(void **state)
{
	/*
	 * Given a cluster with 1 database
	 */
	old_cluster.dbarr.ndbs = 1;
	old_cluster.dbarr.dbs = (DbInfo*)malloc(sizeof(DbInfo));

	expect_string(prep_status, fmt, "Checking for heterogeneous partitioned tables");
	will_be_called(prep_status);

	expect_any(connectToServer, cluster);
	expect_any(connectToServer, db_name);
	will_return(connectToServer, NULL);

	/*
	 * When the query to check different columns counts returns 1 result
	 */
	expect_any(executeQueryOrDie, conn);
	expect_string(executeQueryOrDie, fmt, CHECK_PARTITION_TABLE_MATCHES_COLUMN_COUNT);
	int result_addr = 0xDEADBEEF;
	will_return(executeQueryOrDie, result_addr);
	expect_value(PQntuples, res, result_addr);
	will_return(PQntuples, 1);

	expect_value(PQfnumber, res, result_addr);
	expect_string(PQfnumber, field_name, "parrelid");
	will_be_called(PQfnumber);

	expect_value(PQgetvalue, res, result_addr);
	expect_value(PQgetvalue, tup_num, 0);
	expect_any(PQgetvalue, field_num);
	will_be_called(PQgetvalue);

	expect_value(PQclear, res, result_addr);
	will_be_called(PQclear);

	/*
	 * And the query to check different columns attributes returns 0 results
	 */
	expect_any(executeQueryOrDie, conn);
	expect_string(executeQueryOrDie, fmt, CHECK_PARTITION_TABLE_MATCHES_COLUMN_ATTRIBUTES);
	will_return(executeQueryOrDie, NULL);
	expect_any(PQntuples, res);
	will_return(PQntuples, 0);
	expect_any(PQclear, res);
	will_be_called(PQclear);

	/*
	 * Then the check should report fatal
	 */
	expect_value(pg_log, type, PG_REPORT);
	expect_string(pg_log, fmt, "fatal\n");
	will_be_called(pg_log);
	expect_value(pg_log, type, PG_FATAL);
	expect_any(pg_log, fmt);
	will_be_called(pg_log);

	check_heterogeneous_partition();
}

int
main(int argc, char *argv[])
{
	const UnitTest tests[] = {
		unit_test(test_check_with_no_databases),
		unit_test(test_check_with_one_database_and_no_mismatches),
		unit_test(test_check_with_one_database_and_mismatch_number_of_columns),
	};
	return run_tests(tests);
}
