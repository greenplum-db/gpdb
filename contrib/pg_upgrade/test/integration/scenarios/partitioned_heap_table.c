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

static void
partitionedHeapTableWithDefaultPartitionSplittedShouldHaveBeenUpgraded()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQuery(connection, "set search_path to five_to_six_upgrade;");

	result = executeQuery(connection, "select * from p_split_partition_test;");
	assert_int_equal(5, PQntuples(result));

	result = executeQuery(connection, "select * from p_split_partition_test_1_prt_splitted;");
	assert_int_equal(3, PQntuples(result));

	result = executeQuery(connection, "select * from p_split_partition_test_1_prt_extra;");
	assert_int_equal(1, PQntuples(result));

	PQfinish(connection);
}

static void
listPartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQuery(connection, "set search_path to five_to_six_upgrade;");

	result = executeQuery(connection, "select * from p_add_list_partition_test");
	assert_int_equal(4, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_list_partition_test where b=3");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_list_partition_test_1_prt_added_part where b=2");
	assert_int_equal(1, PQntuples(result));

	PQfinish(connection);
}

static void
rangePartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQuery(connection, "set search_path to five_to_six_upgrade;");

	result = executeQuery(connection, "select * from p_add_partition_test");
	assert_int_equal(4, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_partition_test where b=3");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from p_add_partition_test_1_prt_added_part where b=2");
	assert_int_equal(1, PQntuples(result));

	PQfinish(connection);
}

static void
partitionedHeapTableShouldHaveDataUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	executeQuery(connection, "set search_path to five_to_six_upgrade;");

	result = executeQuery(connection, "select * from users_1_prt_1 where id=1 and name='Jane';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from users_1_prt_2 where id=2 and name='John';");
	assert_int_equal(1, PQntuples(result));

	result = executeQuery(connection, "select * from users;");
	assert_int_equal(2, PQntuples(result));

	PQfinish(connection);
}

static void
anAdministratorPerformsAnUpgrade()
{
	performUpgrade();
}

static void
createPartitionedHeapTableWithDataInFiveCluster(void)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection, "create table users (id integer, name text) distributed by (id) partition by range(id) (start(1) end(3) every(1));");
	executeQuery(connection, "insert into users values (1, 'Jane')");
	executeQuery(connection, "insert into users values (2, 'John')");
	PQfinish(connection);
}

static void
createRangePartitionedHeapTableAndAddPartitionsWithData(void)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection, "create table p_add_partition_test (a int, b int) partition by range(b) (start(1) end(2));");
	executeQuery(connection, "insert into p_add_partition_test values (1, 1)");
	executeQuery(connection, "insert into p_add_partition_test values (2, 1)");
	// add partition with a specific name
	executeQuery(connection, "alter table p_add_partition_test add partition added_part start(2) end(3);");
	executeQuery(connection, "insert into p_add_partition_test values (1, 2)");
	// add partition with default name
	executeQuery(connection, "alter table p_add_partition_test add partition start(3) end(4);");
	executeQuery(connection, "insert into p_add_partition_test values (1, 3)");
	PQfinish(connection);
}

static void
createListPartitionedHeapTableAndAddPartitionsWithData(void)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection, "create table p_add_list_partition_test (a int, b int) partition by list(b) (PARTITION one VALUES (1));");
	executeQuery(connection, "insert into p_add_list_partition_test values (1, 1)");
	executeQuery(connection, "insert into p_add_list_partition_test values (2, 1)");
	// add partition with a specific name
	executeQuery(connection, "alter table p_add_list_partition_test add partition added_part values(2);");
	executeQuery(connection, "insert into p_add_list_partition_test values (1, 2)");
	// add partition with default name
	executeQuery(connection, "alter table p_add_list_partition_test add partition values(3);");
	executeQuery(connection, "insert into p_add_list_partition_test values (1, 3)");
	PQfinish(connection);
}

static void
createRangePartitionedHeapTableWithDefaultPartition(void)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection, "create table p_split_partition_test (a int, b int) partition by range(b) (start(1) end(2), default partition extra);");
	executeQuery(connection, "insert into p_split_partition_test select i, i from generate_series(1,5)i;");
	executeQuery(connection, "alter table p_split_partition_test split default partition start(2) end(5) into (partition splitted, partition extra);");
	PQfinish(connection);
}

void
test_a_partitioned_heap_table_with_data_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedHeapTableWithDataInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedHeapTableShouldHaveDataUpgradedToSixCluster));
}

void
test_a_partition_table_with_newly_added_range_partition_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createRangePartitionedHeapTableAndAddPartitionsWithData));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(rangePartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded));
}

void
test_a_partition_table_with_newly_added_list_partition_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createListPartitionedHeapTableAndAddPartitionsWithData));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(listPartitionedHeapTableWithAddedPartitionsShouldHaveBeenUpgraded));
}

void
test_a_partition_table_with_default_partition_after_split_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createRangePartitionedHeapTableWithDefaultPartition));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(partitionedHeapTableWithDefaultPartitionSplittedShouldHaveBeenUpgraded));
}
