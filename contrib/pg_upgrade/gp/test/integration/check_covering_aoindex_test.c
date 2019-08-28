/*
 * Integration tests for check_covering_aoindex()
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "test_utils.h"
#include "pg_upgrade_fakes.h"
#include "../../checks.h"


void setup(ClusterInfo *cluster)
{
	setup_cluster(cluster);
	setup_os_info();

	get_db_and_rel_infos(cluster);
}


static void 
test_partition_table_with_index_after_exchange_should_fail(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "drop schema greenplum_pg_upgrade_integration_test cascade;");
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer, b text, c integer) with (appendonly=true) distributed by (a) partition by range(c) (start(1) end(3) every(1)); ");
	executeQuery(connection, "create index t_idx on t (b);");
	executeQuery(connection, "create table t_exch (a integer, b text, c integer) with (appendonly=true) distributed by (a);");
	executeQuery(connection, "alter table t exchange partition for (rank(1)) with table t_exch;");
	PQfinish(connection);

	bool result = check_covering_aoindex(cluster);

	assert_false(result);

	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop schema greenplum_pg_upgrade_integration_test CASCADE;");
	PQfinish(connection);
}


static void
test_partition_table_without_index_after_exchange_should_succeed(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer, b text, c integer) with (appendonly=true) distributed by (a) partition by range(c) (start(1) end(3) every(1)); ");
	executeQuery(connection, "create table t_exch (a integer, b text, c integer) with (appendonly=true) distributed by (a);");
	executeQuery(connection, "alter table t exchange partition for (rank(1)) with table t_exch;");
	PQfinish(connection);

	bool result = check_covering_aoindex(cluster);

	assert_true(result);

	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop schema greenplum_pg_upgrade_integration_test CASCADE;");
	PQfinish(connection);
}


static void
test_partition_co_table_with_index_after_exchange_should_fail(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer, b text, c integer) with (appendonly=true, orientation=column) distributed by (a) partition by range(c) (start(1) end(3) every(1)); ");
	executeQuery(connection, "create index t_idx on t (b);");
	executeQuery(connection, "create table t_exch (a integer, b text, c integer) with (appendonly=true, orientation=column) distributed by (a);");
	executeQuery(connection, "alter table t exchange partition for (rank(1)) with table t_exch;");
	PQfinish(connection);

	bool result = check_covering_aoindex(cluster);

	assert_false(result);

	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop schema greenplum_pg_upgrade_integration_test CASCADE;");
	PQfinish(connection);
}


static void
test_partition_co_table_without_index_after_exchange_should_succeed(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer, b text, c integer) with (appendonly=true, orientation=column) distributed by (a) partition by range(c) (start(1) end(3) every(1)); ");
	executeQuery(connection, "create table t_exch (a integer, b text, c integer) with (appendonly=true, orientation=column) distributed by (a);");
	executeQuery(connection, "alter table t exchange partition for (rank(1)) with table t_exch;");
	PQfinish(connection);

	bool result = check_covering_aoindex(cluster);

	assert_true(result);

	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop schema greenplum_pg_upgrade_integration_test CASCADE;");
	PQfinish(connection);
}


static void
test_append_only_partition_table_without_exchange_and_without_index_should_succeed(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "drop schema greenplum_pg_upgrade_integration_test cascade;");
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer, b text, c integer) with (appendonly=true) distributed by (a) partition by range(c) (start(1) end(3) every(1)); ");
	PQfinish(connection);

	bool result = check_covering_aoindex(cluster);

	assert_true(result);

	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop schema greenplum_pg_upgrade_integration_test CASCADE;");
	PQfinish(connection);
}


static void
test_partition_heap_table_with_index_after_exchange_should_succeed(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup(cluster);

	PGconn	   *connection;
	connection = getTestConnection(cluster);
	executeQuery(connection, "drop schema greenplum_pg_upgrade_integration_test cascade;");
	executeQuery(connection, "create schema greenplum_pg_upgrade_integration_test;");
	executeQuery(connection, "set search_path to greenplum_pg_upgrade_integration_test; ");
	executeQuery(connection, "create table t (a integer, b text, c integer) distributed by (a) partition by range(c) (start(1) end(3) every(1)); ");
	executeQuery(connection, "create index t_idx on t (b);");
	executeQuery(connection, "create table t_exch (a integer, b text, c integer) distributed by (a);");
	executeQuery(connection, "alter table t exchange partition for (rank(1)) with table t_exch;");
	PQfinish(connection);

	bool result = check_covering_aoindex(cluster);

	assert_true(result);

	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop schema greenplum_pg_upgrade_integration_test CASCADE;");
	PQfinish(connection);
}


int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_partition_table_with_index_after_exchange_should_fail),
		unit_test(test_partition_table_without_index_after_exchange_should_succeed),
		unit_test(test_partition_co_table_with_index_after_exchange_should_fail),
		unit_test(test_partition_co_table_without_index_after_exchange_should_succeed),
		unit_test(test_append_only_partition_table_without_exchange_and_without_index_should_succeed),
		unit_test(test_partition_heap_table_with_index_after_exchange_should_succeed)
	};

	return run_tests(tests);
}
