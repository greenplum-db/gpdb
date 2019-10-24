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
partitionedTableShouldHaveDataUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQueryClearResult(connection, "SET search_path TO five_to_six_upgrade;");

	result = executeQuery(connection, "SELECT * FROM users_1_prt_1 WHERE id=1 AND name='Jane';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users_1_prt_2 WHERE id=2 AND name='John';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users;");
	assert_int_equal(2, PQntuples(result));

	PQfinish(connection);
}

static void
partitionedTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQueryClearResult(connection, "SET search_path TO five_to_six_upgrade;");

	executeQueryClearResult(connection, "CREATE INDEX name_index ON users(name);");
	executeQueryClearResult(connection, "SET enable_seqscan=OFF");

	result = executeQuery(connection, "SELECT * FROM users;");
	assert_int_equal(5, PQntuples(result));
	PQclear(result);

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users WHERE name='Carolyn';");
	assert_int_equal(1, PQntuples(result));
	PQclear(result);

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users WHERE name='Bob';");
	assert_int_equal(0, PQntuples(result));
	PQclear(result);

	PQfinish(connection);
}

static void
anAdministratorPerformsAnUpgrade()
{
	performUpgrade();
}

static void
createPartitionedTableWithDataInFiveCluster(char *options)
{
	PGconn	   *connection = connectToFive();
	char buffer[1000];

	executeQueryClearResult(connection, "CREATE SCHEMA five_to_six_upgrade;");
	executeQueryClearResult(connection, "SET search_path TO five_to_six_upgrade");
	sprintf(buffer, "CREATE TABLE users (id integer, name text) %s DISTRIBUTED BY (id) PARTITION BY RANGE(id) (START(1) END(3) EVERY(1));", options);
	executeQueryClearResult(connection,buffer);
	executeQueryClearResult(connection, "INSERT INTO users VALUES (1, 'Jane')");
	executeQueryClearResult(connection, "INSERT INTO users VALUES (2, 'John')");
	PQfinish(connection);
}

static void
createPartitionedAOTableWithDataInFiveCluster()
{
	createPartitionedTableWithDataInFiveCluster("WITH (appendonly=true)");
}

static void
createPartitionedAOCOTableWithDataInFiveCluster()
{
	createPartitionedTableWithDataInFiveCluster("WITH (appendonly=true, orientation=column)");
}

static void
createPartitionedTableWithDataOnMultipleSegfilesInFiveCluster(char *options)
{
	PGconn	   *connection1 = connectToFive();
	PGconn	   *connection2 = connectToFive();
	char buffer[1000];

	executeQueryClearResult(connection1, "CREATE SCHEMA five_to_six_upgrade;");
	executeQueryClearResult(connection1, "SET search_path TO five_to_six_upgrade");

	sprintf(buffer,
			"CREATE TABLE users (id int, name text) %s DISTRIBUTED BY (id) "
			"PARTITION BY RANGE (id) "
			"    SUBPARTITION BY LIST (name) "
			"        SUBPARTITION TEMPLATE ( "
			"         SUBPARTITION jane VALUES ('Jane'), "
			"          SUBPARTITION john VALUES ('John'), "
			"           DEFAULT SUBPARTITION other_names ) "
			"(START (1) END (2) EVERY (1), "
			"    DEFAULT PARTITION other_ids );", options);
	executeQueryClearResult(connection1, buffer);
	/*
	 * Table has indexes which will be dropped before upgrade and be re-created
	 * after upgrade.
	 */
	executeQueryClearResult(connection1, "CREATE INDEX name_index ON users(name);");
	executeQueryClearResult(connection1, "BEGIN;");
	executeQueryClearResult(connection1, "INSERT INTO users VALUES (1, 'Jane')");
	executeQueryClearResult(connection1, "INSERT INTO users VALUES (2, 'Jane')");

	executeQueryClearResult(connection2, "SET search_path TO five_to_six_upgrade");
	executeQueryClearResult(connection2, "BEGIN;");
	/*
	 * (1, 'Jane') and (2, 'Jane') are also being inserted on connection1 in a
	 * transaction so we expect this will create additional segment files.
	 */
	executeQueryClearResult(connection2, "INSERT INTO users VALUES (1, 'Jane')");
	executeQueryClearResult(connection2, "INSERT INTO users VALUES (2, 'Jane')");
	executeQueryClearResult(connection2, "INSERT INTO users VALUES (4, 'Andy')");

	executeQueryClearResult(connection1, "END");
	executeQueryClearResult(connection2, "END");

	/*
	 * Ensure that we can correctly upgrade tables with dropped or deleted
	 * tuples.
	 */
	executeQueryClearResult(connection2, "UPDATE users SET name='Carolyn' WHERE name='Andy'");
	executeQueryClearResult(connection2, "INSERT INTO users VALUES (5, 'Bob')");
	executeQueryClearResult(connection2, "DELETE FROM users WHERE id=5");

	executeQueryClearResult(connection1, "DROP INDEX name_index;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_2;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_other_ids;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_2_2_prt_jane;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_2_2_prt_john;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_2_2_prt_other_names;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_other_ids_2_prt_jane;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_other_ids_2_prt_john;");
	executeQueryClearResult(connection1, "DROP INDEX name_index_1_prt_other_ids_2_prt_other_names;");
	PQfinish(connection1);
	PQfinish(connection2);
}

static void
createPartitionedAOTableWithDataOnMultipleSegfilesInFiveCluster(void)
{
	createPartitionedTableWithDataOnMultipleSegfilesInFiveCluster("WITH (appendonly=true)");
}

static void
createPartitionedAOCOTableWithDataOnMultipleSegfilesInFiveCluster(void)
{
	createPartitionedTableWithDataOnMultipleSegfilesInFiveCluster("WITH (appendonly=true)");
}

void test_a_partitioned_ao_table_with_data_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedAOTableWithDataInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedTableShouldHaveDataUpgradedToSixCluster));
}

void test_a_partitioned_ao_table_with_data_on_multiple_segfiles_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedAOTableWithDataOnMultipleSegfilesInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster));
}

void test_a_partitioned_aoco_table_with_data_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedAOCOTableWithDataInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedTableShouldHaveDataUpgradedToSixCluster));
}

void test_a_partitioned_aoco_table_with_data_on_multiple_segfiles_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedAOCOTableWithDataOnMultipleSegfilesInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster));
}
