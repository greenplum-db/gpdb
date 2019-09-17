#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "test_utils.h"
#include "../../checks.h"


void
setup(ClusterInfo *cluster)
{
	setup_cluster(cluster);
	setup_os_info();

	get_db_and_rel_infos(cluster);
}

void
teardown(PGconn *connection)
{
	executeQuery(connection, "drop schema greenplum_pg_upgrade_integration_test cascade;");
	PQfinish(connection);
}


static void 
test_a_user_defined_index_should_fail_checks(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "drop schema greenplum_pg_upgrade_integration_test cascade;");
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer) distributed by (a);");
	executeQuery(connection, "create index t_index on t(a);");

	bool result = check_user_defined_indexes(cluster);

	assert_false(result);

	teardown(connection);
}

static void
test_a_cluster_without_user_defined_indexes_should_pass_checks(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "drop schema greenplum_pg_upgrade_integration_test cascade;");
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table some_table (a integer) distributed by (a);");

	bool result = check_user_defined_indexes(cluster);

	assert_true(result);

	teardown(connection);
}


int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_a_user_defined_index_should_fail_checks),
		unit_test(test_a_cluster_without_user_defined_indexes_should_pass_checks)
	};

	return run_tests(tests);
}
