#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "greenplum_five_to_greenplum_six_upgrade_test_suite.h"

#include "partitioned_ao_table.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"

static void
partitionedAoTableShouldHaveDataUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "SELECT * FROM users_ao_singlelevel_part_1_prt_1 WHERE id=1 AND name='Jane';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users_ao_singlelevel_part_1_prt_2 WHERE id=2 AND name='John';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users_ao_singlelevel_part;");
	assert_int_equal(2, PQntuples(result));

	PQfinish(connection);
}

static void
partitionedAocoTableShouldHaveDataUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "SELECT * FROM users_aoco_singlelevel_part_1_prt_1 WHERE id=1 AND name='Jane';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users_aoco_singlelevel_part_1_prt_2 WHERE id=2 AND name='John';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "SELECT * FROM users_aoco_singlelevel_part;");
	assert_int_equal(2, PQntuples(result));

	PQfinish(connection);
}

static void
partitionedAoTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQueryClearResult(connection, "CREATE INDEX users_ao_index ON users_ao(name);");
	executeQueryClearResult(connection, "SET enable_seqscan=OFF");

	result = executeQuery(connection, "SELECT * FROM users_ao;");
	assert_int_equal(5, PQntuples(result));
	PQclear(result);

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users_ao WHERE name='Carolyn';");
	assert_int_equal(1, PQntuples(result));
	PQclear(result);

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users_ao WHERE name='Bob';");
	assert_int_equal(0, PQntuples(result));
	PQclear(result);

	PQfinish(connection);
}

static void
partitionedAocoTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQueryClearResult(connection, "CREATE INDEX users_aoco_index ON users_aoco(name);");
	executeQueryClearResult(connection, "SET enable_seqscan=OFF");

	result = executeQuery(connection, "SELECT * FROM users_aoco;");
	assert_int_equal(5, PQntuples(result));
	PQclear(result);

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users_aoco WHERE name='Carolyn';");
	assert_int_equal(1, PQntuples(result));
	PQclear(result);

	result = executeQuery(connection, "SET enable_seqscan=OFF; SELECT * FROM users_aoco WHERE name='Bob';");
	assert_int_equal(0, PQntuples(result));
	PQclear(result);

	PQfinish(connection);
}

static void
createPartitionedAOTableWithDataInFiveCluster(void **state)
{
	PGconn	   *connection = connectToFive();
	char buffer[1000];

	sprintf(buffer, "CREATE TABLE users_ao_singlelevel_part (id integer, name text) WITH (appendonly=true) DISTRIBUTED BY (id) PARTITION BY RANGE(id) (START(1) END(3) EVERY(1));");
	executeQueryClearResult(connection,buffer);
	executeQueryClearResult(connection, "INSERT INTO users_ao_singlelevel_part VALUES (1, 'Jane')");
	executeQueryClearResult(connection, "INSERT INTO users_ao_singlelevel_part VALUES (2, 'John')");
	PQfinish(connection);
}

static void
createPartitionedAOCOTableWithDataInFiveCluster(void **state)
{

	PGconn	   *connection = connectToFive();
	char buffer[1000];

	sprintf(buffer, "CREATE TABLE users_aoco_singlelevel_part (id integer, name text) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id) PARTITION BY RANGE(id) (START(1) END(3) EVERY(1));");
	executeQueryClearResult(connection,buffer);
	executeQueryClearResult(connection, "INSERT INTO users_aoco_singlelevel_part VALUES (1, 'Jane')");
	executeQueryClearResult(connection, "INSERT INTO users_aoco_singlelevel_part VALUES (2, 'John')");
	PQfinish(connection);

}

static void
createPartitionedAOTableWithDataOnMultipleSegfilesInFiveCluster(void **state)
{
	PGconn	   *connection1 = connectToFive();
	PGconn	   *connection2 = connectToFive();
	char buffer[1000];

	sprintf(buffer,
			"CREATE TABLE users_ao (id int, name text) WITH (appendonly=true) DISTRIBUTED BY (id) "
			"PARTITION BY RANGE (id) "
			"    SUBPARTITION BY LIST (name) "
			"        SUBPARTITION TEMPLATE ( "
			"         SUBPARTITION jane VALUES ('Jane'), "
			"          SUBPARTITION john VALUES ('John'), "
			"           DEFAULT SUBPARTITION other_names ) "
			"(START (1) END (2) EVERY (1), "
			"    DEFAULT PARTITION other_ids );");
	executeQueryClearResult(connection1, buffer);
	/*
	 * Table has indexes which will be dropped before upgrade and be re-created
	 * after upgrade.
	 */
	executeQueryClearResult(connection1, "CREATE INDEX users_ao_index ON users_ao(name);");
	executeQueryClearResult(connection1, "BEGIN;");
	executeQueryClearResult(connection1, "INSERT INTO users_ao VALUES (1, 'Jane')");
	executeQueryClearResult(connection1, "INSERT INTO users_ao VALUES (2, 'Jane')");

	executeQueryClearResult(connection2, "BEGIN;");
	/*
	 * (1, 'Jane') and (2, 'Jane') are also being inserted on connection1 in a
	 * transaction so we expect this will create additional segment files.
	 */
	executeQueryClearResult(connection2, "INSERT INTO users_ao VALUES (1, 'Jane')");
	executeQueryClearResult(connection2, "INSERT INTO users_ao VALUES (2, 'Jane')");
	executeQueryClearResult(connection2, "INSERT INTO users_ao VALUES (4, 'Andy')");

	executeQueryClearResult(connection1, "END");
	executeQueryClearResult(connection2, "END");

	/*
	 * Ensure that we can correctly upgrade tables with dropped or deleted
	 * tuples.
	 */
	executeQueryClearResult(connection2, "UPDATE users_ao SET name='Carolyn' WHERE name='Andy'");
	executeQueryClearResult(connection2, "INSERT INTO users_ao VALUES (5, 'Bob')");
	executeQueryClearResult(connection2, "DELETE FROM users_ao WHERE id=5");

	executeQueryClearResult(connection1, "DROP INDEX users_ao_index;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_2;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_other_ids;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_2_2_prt_jane;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_2_2_prt_john;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_2_2_prt_other_names;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_other_ids_2_prt_jane;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_other_ids_2_prt_john;");
	executeQueryClearResult(connection1, "DROP INDEX users_ao_index_1_prt_other_ids_2_prt_other_names;");
	PQfinish(connection1);
	PQfinish(connection2);
}

static void
createPartitionedAOCOTableWithDataOnMultipleSegfilesInFiveCluster(void **state)
{
	PGconn	   *connection1 = connectToFive();
	PGconn	   *connection2 = connectToFive();
	char buffer[1000];

	sprintf(buffer,
			"CREATE TABLE users_aoco (id int, name text) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id) "
			"PARTITION BY RANGE (id) "
			"    SUBPARTITION BY LIST (name) "
			"        SUBPARTITION TEMPLATE ( "
			"         SUBPARTITION jane VALUES ('Jane'), "
			"          SUBPARTITION john VALUES ('John'), "
			"           DEFAULT SUBPARTITION other_names ) "
			"(START (1) END (2) EVERY (1), "
			"    DEFAULT PARTITION other_ids );");
	executeQueryClearResult(connection1, buffer);
	/*
	 * Table has indexes which will be dropped before upgrade and be re-created
	 * after upgrade.
	 */
	executeQueryClearResult(connection1, "CREATE INDEX users_aoco_index ON users_aoco(name);");
	executeQueryClearResult(connection1, "BEGIN;");
	executeQueryClearResult(connection1, "INSERT INTO users_aoco VALUES (1, 'Jane')");
	executeQueryClearResult(connection1, "INSERT INTO users_aoco VALUES (2, 'Jane')");

	executeQueryClearResult(connection2, "BEGIN;");
	/*
	 * (1, 'Jane') and (2, 'Jane') are also being inserted on connection1 in a
	 * transaction so we expect this will create additional segment files.
	 */
	executeQueryClearResult(connection2, "INSERT INTO users_aoco VALUES (1, 'Jane')");
	executeQueryClearResult(connection2, "INSERT INTO users_aoco VALUES (2, 'Jane')");
	executeQueryClearResult(connection2, "INSERT INTO users_aoco VALUES (4, 'Andy')");

	executeQueryClearResult(connection1, "END");
	executeQueryClearResult(connection2, "END");

	/*
	 * Ensure that we can correctly upgrade tables with dropped or deleted
	 * tuples.
	 */
	executeQueryClearResult(connection2, "UPDATE users_aoco SET name='Carolyn' WHERE name='Andy'");
	executeQueryClearResult(connection2, "INSERT INTO users_aoco VALUES (5, 'Bob')");
	executeQueryClearResult(connection2, "DELETE FROM users_aoco WHERE id=5");

	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_2;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_other_ids;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_2_2_prt_jane;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_2_2_prt_john;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_2_2_prt_other_names;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_other_ids_2_prt_jane;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_other_ids_2_prt_john;");
	executeQueryClearResult(connection1, "DROP INDEX users_aoco_index_1_prt_other_ids_2_prt_other_names;");
	PQfinish(connection1);
	PQfinish(connection2);
}

void
test_a_partitioned_ao_table_with_data_on_multiple_segfiles_can_be_upgraded(void)
{
	unit_test_given(createPartitionedAOTableWithDataOnMultipleSegfilesInFiveCluster, "test_a_partitioned_ao_table_with_data_on_multiple_segfiles_can_be_upgraded");
	unit_test_then(partitionedAoTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster, "test_a_partitioned_ao_table_with_data_on_multiple_segfiles_can_be_upgraded");
}

void
test_a_partitioned_aoco_table_with_data_on_multiple_segfiles_can_be_upgraded(void)
{
	unit_test_given(createPartitionedAOCOTableWithDataOnMultipleSegfilesInFiveCluster, "test_a_partitioned_aoco_table_with_data_on_multiple_segfiles_can_be_upgraded");
	unit_test_then(partitionedAocoTableShouldHaveDataOnMultipleSegfilesUpgradedToSixCluster, "test_a_partitioned_aoco_table_with_data_on_multiple_segfiles_can_be_upgraded");
}

void
test_a_partitioned_aoco_table_with_data_can_be_upgraded(void)
{
	unit_test_given(createPartitionedAOCOTableWithDataInFiveCluster, "test_a_partitioned_aoco_table_with_data_can_be_upgraded");
	unit_test_then(partitionedAocoTableShouldHaveDataUpgradedToSixCluster, "test_a_partitioned_aoco_table_with_data_can_be_upgraded");
}

void
test_a_partitioned_ao_table_with_data_can_be_upgraded(void)
{
	unit_test_given(createPartitionedAOTableWithDataInFiveCluster, "test_a_partitioned_ao_table_with_data_can_be_upgraded");
	unit_test_then(partitionedAoTableShouldHaveDataUpgradedToSixCluster, "test_a_partitioned_ao_table_with_data_can_be_upgraded");
}
