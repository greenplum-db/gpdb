
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "test_utils.h"
#include "pg_upgrade_fakes.h"

#include "../../checks.h"
#include "../../../pg_upgrade.h"

void setup(ClusterInfo *cluster)
{
	setup_cluster(cluster);
	setup_os_info();
}


void test_check_fails_when_non_default_extensions_are_installed(void **state)
{
	PGconn	   *connection;
	ClusterInfo  *cluster;

	/*
	 * Setup
	 */
	cluster = make_cluster();
	setup(cluster);

	/*
	 * Create a database to work on
	 */
	connection = getTestConnection(cluster);
	executeQuery(connection, "create database checks_database;");
	PQfinish(connection);

	/*
	 * connect to our new database and load its info
	 */
	connection = getTestConnectionToDatabase(cluster, "checks_database");
	get_db_and_rel_infos(cluster);

	/* 
	 * Create a new extension in the database
	 */
	executeQuery(connection, "create extension plperlu;");
	PQfinish(connection);

	/*
	 * Perform check
	 */
	bool result = check_nondefault_extensions(cluster);

	/*
	 * Make assertion
	 */
	assert_false(result);

	/*
	 * Cleanup
	 */
	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop database checks_database;");
	PQfinish(connection);
}


void test_check_succeeds_when_only_default_extensions_are_installed(void **state)
{
	PGconn	   *connection;
	ClusterInfo  *cluster;

	/*
	 * Setup
	 */
	cluster = make_cluster();
	setup(cluster);

	/*
	 * Create a database to work on
	 */
	connection = getTestConnection(cluster);
	executeQuery(connection, "create database checks_database;");
	PQfinish(connection);

	/*
	 * Load our new databases info
	 */
	get_db_and_rel_infos(cluster);

	/*
	 * Perform check
	 */
	bool result = check_nondefault_extensions(cluster);

	/*
	 * Make assertion
	 */
	assert_true(result);

	/*
	 * Cleanup
	 */
	connection = getTestConnection(cluster);
	executeQueryOrDie(connection, "drop database checks_database;");
	PQfinish(connection);
}


int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_check_fails_when_non_default_extensions_are_installed),
		unit_test(test_check_succeeds_when_only_default_extensions_are_installed)
	};

	return run_tests(tests);
}