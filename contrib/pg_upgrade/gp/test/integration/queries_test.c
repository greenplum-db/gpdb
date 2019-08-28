#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"


#include "test_utils.h"


static void
teardown(ClusterInfo *cluster)
{
	PGconn *connection = getTestConnection(cluster);
	executeQuery(connection, "drop database cluster_queries;");
	PQfinish(connection);
}


static void test_cluster_can_query_for_user_defined_indexes(void **state) 
{
	ClusterInfo *cluster = make_cluster();
	Queries *queries = make_queries();

	setup_cluster(cluster);
	setup_os_info();

	PGconn *connection = getTestConnection(cluster);
	executeQuery(connection, "drop database cluster_queries;");
	executeQuery(connection, "create database cluster_queries;");
	PQfinish(connection);

	connection = getTestConnectionToDatabase(cluster, "cluster_queries");
	executeQuery(connection, "set search_path to public;");
	executeQuery(connection, "create table users (id int, name text) distributed by (id);");
	executeQuery(connection, "create index users_index on users(id);");
	PQfinish(connection);

	get_db_and_rel_infos(cluster);

	struct UserDefinedIndexes indexes = queries->query_for_indexes(cluster);
	assert_int_equal(indexes.number_of_user_defined_indexes, 1);

	teardown(cluster);
}


int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_cluster_can_query_for_user_defined_indexes)
	};

	return run_tests(tests);
}