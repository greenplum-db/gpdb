#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "partitioned_heap_table.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"

#include "greenplum_five_to_greenplum_six_upgrade_test_suite.h"

static void
partitionedHeapTableWithDefaultPartitionSplittedShouldHaveBeenUpgraded(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "select * from p_split_partition_test;");
	assert_int_equal(5, PQntuples(result));

	result = executeQuery(connection, "select * from p_split_partition_test_1_prt_splitted;");
	assert_int_equal(3, PQntuples(result));

	result = executeQuery(connection, "select * from p_split_partition_test_1_prt_extra;");
	assert_int_equal(1, PQntuples(result));

	PQfinish(connection);
}

static void
listPartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "select * from p_add_list_partition_test");
	assert_int_equal(4, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_list_partition_test where b=3");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_list_partition_test_1_prt_added_part where b=2");
	assert_int_equal(1, PQntuples(result));

	PQfinish(connection);
}

static void
rangePartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "select * from p_add_partition_test");
	assert_int_equal(4, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_partition_test where b=3");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_partition_test_1_prt_added_part where b=2");
	assert_int_equal(1, PQntuples(result));

	PQfinish(connection);
}

static void
partitionedHeapTableShouldHaveDataUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "select * from users_1_prt_1 where id=1 and name='Jane';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from users_1_prt_2 where id=2 and name='John';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from users;");
	assert_int_equal(2, PQntuples(result));

	PQfinish(connection);
}

static void
createPartitionedHeapTableWithDataInFiveCluster(void **state)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "create table users (id integer, name text) distributed by (id) partition by range(id) (start(1) end(3) every(1));");
	executeQuery(connection, "insert into users values (1, 'Jane')");
	executeQuery(connection, "insert into users values (2, 'John')");
	PQfinish(connection);
}

static void
createRangePartitionedHeapTableAndAddPartitionsWithData(void **state)
{
	PGconn	   *connection = connectToFive();

	executeQueryClearResult(connection, "create table p_add_partition_test (a int, b int) partition by range(b) (start(1) end(2));");
	executeQueryClearResult(connection, "insert into p_add_partition_test values (1, 1)");
	executeQueryClearResult(connection, "insert into p_add_partition_test values (2, 1)");
	// add partition with a specific name
	executeQueryClearResult(connection, "alter table p_add_partition_test add partition added_part start(2) end(3);");
	executeQueryClearResult(connection, "insert into p_add_partition_test values (1, 2)");
	// add partition with default name
	executeQueryClearResult(connection, "alter table p_add_partition_test add partition start(3) end(4);");
	executeQueryClearResult(connection, "insert into p_add_partition_test values (1, 3)");
	PQfinish(connection);
}

static void
createListPartitionedHeapTableAndAddPartitionsWithData(void **state)
{
	PGconn	   *connection = connectToFive();

	executeQueryClearResult(connection, "create table p_add_list_partition_test (a int, b int) partition by list(b) (PARTITION one VALUES (1));");
	executeQueryClearResult(connection, "insert into p_add_list_partition_test values (1, 1)");
	executeQueryClearResult(connection, "insert into p_add_list_partition_test values (2, 1)");
	// add partition with a specific name
	executeQueryClearResult(connection, "alter table p_add_list_partition_test add partition added_part values(2);");
	executeQueryClearResult(connection, "insert into p_add_list_partition_test values (1, 2)");
	// add partition with default name
	executeQueryClearResult(connection, "alter table p_add_list_partition_test add partition values(3);");
	executeQueryClearResult(connection, "insert into p_add_list_partition_test values (1, 3)");
	PQfinish(connection);
}

static void
createRangePartitionedHeapTableWithDefaultPartition(void **state)
{
	PGconn	   *connection = connectToFive();

	executeQueryClearResult(connection, "create table p_split_partition_test (a int, b int) partition by range(b) (start(1) end(2), default partition extra);");
	executeQueryClearResult(connection, "insert into p_split_partition_test select i, i from generate_series(1,5)i;");
	executeQueryClearResult(connection, "alter table p_split_partition_test split default partition start(2) end(5) into (partition splitted, partition extra);");
	PQfinish(connection);
}

void
test_a_partitioned_heap_table_with_data_can_be_upgraded(void)
{
	unit_test_given(createPartitionedHeapTableWithDataInFiveCluster, "test_a_partitioned_heap_table_with_data_can_be_upgraded");
	unit_test_then(partitionedHeapTableShouldHaveDataUpgradedToSixCluster, "test_a_partitioned_heap_table_with_data_can_be_upgraded");
}

void
test_a_partition_table_with_newly_added_range_partition_can_be_upgraded(void)
{
	unit_test_given(createRangePartitionedHeapTableAndAddPartitionsWithData, "test_a_partition_table_with_newly_added_range_partition_can_be_upgraded");
	unit_test_then(rangePartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded, "test_a_partition_table_with_newly_added_range_partition_can_be_upgraded");
}

void
test_a_partition_table_with_newly_added_list_partition_can_be_upgraded(void)
{
	unit_test_given(createListPartitionedHeapTableAndAddPartitionsWithData, "test_a_partition_table_with_newly_added_list_partition_can_be_upgraded");
	unit_test_then(listPartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded, "test_a_partition_table_with_newly_added_list_partition_can_be_upgraded");
}

void
test_a_partition_table_with_default_partition_after_split_can_be_upgraded(void)
{
	unit_test_given(createRangePartitionedHeapTableWithDefaultPartition, "test_a_partition_table_with_default_partition_after_split_can_be_upgraded");
	unit_test_then(partitionedHeapTableWithDefaultPartitionSplittedShouldHaveBeenUpgraded, "test_a_partition_table_with_default_partition_after_split_can_be_upgraded");
}
