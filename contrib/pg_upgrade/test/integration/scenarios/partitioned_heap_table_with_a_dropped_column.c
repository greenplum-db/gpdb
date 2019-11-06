#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "partitioned_heap_table_with_a_dropped_column.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"

#include "greenplum_five_to_greenplum_six_upgrade_test_suite.h"

static void
partitionedHeapTableShouldHaveDataUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result;

	result = executeQuery(connection, "SELECT c, d FROM abuela WHERE a=10;");

	assert_int_equal(1, PQntuples(result));

	assert_int_equal(10, atoi(PQgetvalue(result, 0, PQfnumber(result, "c"))));
	assert_true(10.0 == atof(PQgetvalue(result, 0, PQfnumber(result, "d"))));

	PQfinish(connection);
	PQclear(result);
}

static void
createPartitionedHeapTableWithDroppedColumnAndDataInFiveCluster(void **state)
{
	PGconn	   *connection = connectToFive();

	executeQueryClearResult(connection,
			"CREATE TABLE abuela (a int, b int, c int, d numeric) "
			"  DISTRIBUTED BY (a) "
			"    PARTITION BY range(c) "
			"    SUBPARTITION BY range(d) (PARTITION mama START(0) END(42) (SUBPARTITION chica START(0) END(42)));");
	executeQueryClearResult(connection, "INSERT INTO abuela SELECT i, i, i, i FROM generate_series(1, 10)i;");
	executeQueryClearResult(connection, "ALTER TABLE abuela DROP COLUMN b;");
	PQfinish(connection);
}


void
test_a_partitioned_heap_table_with_a_dropped_column_can_be_upgraded(void)
{
	unit_test_given(createPartitionedHeapTableWithDroppedColumnAndDataInFiveCluster, "test_a_partitioned_heap_table_with_a_dropped_column_can_be_upgraded");
	unit_test_then(partitionedHeapTableShouldHaveDataUpgradedToSixCluster, "test_a_partitioned_heap_table_with_a_dropped_column_can_be_upgraded");
}
