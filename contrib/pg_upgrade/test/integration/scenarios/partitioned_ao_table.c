#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "partitioned_ao_table.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"

static void
partitionedAOTableShouldHaveDataUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQuery(connection, "SET search_path TO five_to_six_upgrade;");

	result = executeQuery(connection, "SELECT * FROM users_1_prt_1 WHERE id=1 AND name='Jane';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users_1_prt_2 WHERE id=2 AND name='John';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users;");
	assert_int_equal(2, PQntuples(result));

	PQfinish(connection);
}

static void
partitionedAOTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQuery(connection, "SET search_path TO five_to_six_upgrade;");

	executeQuery(connection, "CREATE INDEX name_index ON users(name);");
	executeQuery(connection, "SET enable_seqscan=OFF");

	result = executeQuery(connection, "SELECT * FROM users;");
	assert_int_equal(5, PQntuples(result));

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users WHERE name='Carolyn';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users WHERE name='Bob';");
	assert_int_equal(0, PQntuples(result));

	PQfinish(connection);
}

static void
anAdministratorPerformsAnUpgrade()
{
	performUpgrade();
}

static void
createPartitionedAOTableWithDataInFiveCluster(void)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "CREATE SCHEMA five_to_six_upgrade;");
	executeQuery(connection, "SET search_path TO five_to_six_upgrade");
	executeQuery(connection, "CREATE TABLE users (id integer, name text) WITH (appendonly=true) DISTRIBUTED BY (id) PARTITION BY RANGE(id) (START(1) END(3) EVERY(1));");
	executeQuery(connection, "INSERT INTO users VALUES (1, 'Jane')");
	executeQuery(connection, "INSERT INTO users VALUES (2, 'John')");
	PQfinish(connection);
}

static void
createPartitionedAOTableWithDataOnMultipleSegfilesInFiveCluster(void)
{
	PGconn	   *connection1 = connectToFive();
	PGconn	   *connection2 = connectToFive();

	executeQuery(connection1, "CREATE SCHEMA five_to_six_upgrade;");
	executeQuery(connection1, "SET search_path TO five_to_six_upgrade");
	executeQuery(connection1,
			"CREATE TABLE users (id int, name text) WITH (appendonly=true) DISTRIBUTED BY (id) "
			"PARTITION BY RANGE (id) "
			"    SUBPARTITION BY LIST (name) "
			"        SUBPARTITION TEMPLATE ( "
			"         SUBPARTITION jane VALUES ('Jane'), "
			"          SUBPARTITION john VALUES ('John'), "
			"           DEFAULT SUBPARTITION other_names ) "
			"(START (1) END (2) EVERY (1), "
			"    DEFAULT PARTITION other_ids );");
	/*
	 * Table has indexes which will be dropped before upgrade and be re-created
	 * after upgrade.
	 */
	executeQuery(connection1, "CREATE INDEX name_index ON users(name);");
	executeQuery(connection1, "BEGIN;");
	executeQuery(connection1, "INSERT INTO users VALUES (1, 'Jane')");
	executeQuery(connection1, "INSERT INTO users VALUES (2, 'Jane')");

	executeQuery(connection2, "SET search_path TO five_to_six_upgrade");
	executeQuery(connection2, "BEGIN;");
	/*
	 * (1, 'Jane') and (2, 'Jane') are also being inserted on connection1 in a
	 * transaction so we expect this will create additional segment files.
	 */
	executeQuery(connection2, "INSERT INTO users VALUES (1, 'Jane')");
	executeQuery(connection2, "INSERT INTO users VALUES (2, 'Jane')");
	executeQuery(connection2, "INSERT INTO users VALUES (4, 'Andy')");

	executeQuery(connection1, "END");
	executeQuery(connection2, "END");

	/*
	 * Ensure that we can correctly upgrade tables with dropped or deleted
	 * tuples.
	 */
	executeQuery(connection2, "UPDATE users SET name='Carolyn' WHERE name='Andy'");
	executeQuery(connection2, "INSERT INTO users VALUES (5, 'Bob')");
	executeQuery(connection2, "DELETE FROM users WHERE id=5");

	executeQuery(connection1, "DROP INDEX name_index;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_2;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_other_ids;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_2_2_prt_jane;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_2_2_prt_john;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_2_2_prt_other_names;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_other_ids_2_prt_jane;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_other_ids_2_prt_john;");
	executeQuery(connection1, "DROP INDEX name_index_1_prt_other_ids_2_prt_other_names;");
	PQfinish(connection1);
	PQfinish(connection2);
}

void test_a_partitioned_ao_table_with_data_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedAOTableWithDataInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedAOTableShouldHaveDataUpgradedToSixCluster));
}

void test_a_partitioned_ao_table_with_data_on_multiple_segfiles_can_be_upgraded(void **state)
{
	given(createPartitionedAOTableWithDataOnMultipleSegfilesInFiveCluster);
	when(anAdministratorPerformsAnUpgrade);
	then(partitionedAOTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster);
}
