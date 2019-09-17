#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "test_utils.h"

static void
teardown(ClusterInfo *cluster)
{
	PGconn *connection = getTestConnection(cluster);
	executeQuery(connection, "drop database if exists cluster_queries;");
	executeQuery(connection, "drop database if exists first_cluster_queries;");
	executeQuery(connection, "drop database if exists second_cluster_queries;");
	PQfinish(connection);
}


static void test_cluster_can_query_for_user_defined_indexes(void **state) 
{
	ClusterInfo *cluster = make_cluster();

	setup_cluster(cluster);
	setup_os_info();

	PGconn *connection = getTestConnection(cluster);
	executeQuery(connection, "drop database if exists cluster_queries;");
	executeQuery(connection, "create database cluster_queries;");
	PQfinish(connection);

	connection = getTestConnectionToDatabase(cluster, "cluster_queries");
	executeQuery(connection, "set search_path to public;");
	executeQuery(connection, "create table users (id int, name text) distributed by (id);");
	executeQuery(connection, "create index users_index on users(id);");
	PQfinish(connection);

	get_db_and_rel_infos(cluster);

	UserDefinedIndexes *indexes = query_for_user_defined_indexes(cluster);
	assert_int_equal(indexes->number_of_user_defined_indexes, 1);
	assert_int_not_equal(indexes->foundIndexes[0], NULL);

	teardown(cluster);
}


static void
test_cluster_returns_zero_user_defined_indexes_when_there_are_none(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup_cluster(cluster);
	setup_os_info();

	PGconn *connection = getTestConnection(cluster);
	executeQuery(connection, "drop database if exists cluster_queries;");
	executeQuery(connection, "create database cluster_queries;");
	PQfinish(connection);

	get_db_and_rel_infos(cluster);

	UserDefinedIndexes *indexes = query_for_user_defined_indexes(cluster);
	assert_int_equal(indexes->number_of_user_defined_indexes, 0);

	teardown(cluster);
}

static void
test_cluster_returns_user_defined_queries_from_all_databases(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup_cluster(cluster);
	setup_os_info();

	PGconn *connection = getTestConnection(cluster);
	executeQuery(connection, "drop database if exists first_cluster_queries;");
	executeQuery(connection, "drop database if exists second_cluster_queries;");
	executeQuery(connection, "create database first_cluster_queries;");
	executeQuery(connection, "create database second_cluster_queries;");
	PQfinish(connection);

	connection = getTestConnectionToDatabase(cluster, "first_cluster_queries");
	executeQuery(connection, "set search_path to public;");
	executeQuery(connection, "create table users (id int, name text) distributed by (id);");
	executeQuery(connection, "create index users_index on users(id);");
	PQfinish(connection);

	connection = getTestConnectionToDatabase(cluster, "second_cluster_queries");
	executeQuery(connection, "set search_path to public;");
	executeQuery(connection, "create table users (id int, name text) distributed by (id);");
	executeQuery(connection, "create index users_index on users(id);");
	PQfinish(connection);

	get_db_and_rel_infos(cluster);

	UserDefinedIndexes *indexes = query_for_user_defined_indexes(cluster);
	assert_int_equal(indexes->number_of_user_defined_indexes, 2);

	teardown(cluster);
}

//
// TODO: Outstanding logic to test:
//
// it should not consider an index user defined if:
// 
// [ ] it is within information_schema
// [ ] it is within the pg_aoseg namespace
// [ ] it is within the pg_toast namespace
// [ ] namespace name starts with pg_toast
// [ ] pg_class relkind empty string
// [ ] pg_class relkind not 'i' (index) after joining with pg_index (curious how this situation arises)
//


int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_cluster_can_query_for_user_defined_indexes),
		unit_test(test_cluster_returns_zero_user_defined_indexes_when_there_are_none),
		unit_test(test_cluster_returns_user_defined_queries_from_all_databases)
	};

	return run_tests(tests);
}
