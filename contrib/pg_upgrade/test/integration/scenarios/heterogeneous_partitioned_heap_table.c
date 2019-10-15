#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"
#include "bdd-library/bdd.h"

#include "heterogeneous_partitioned_heap_table.h"

static void
createPartitionedHeapTableWithDifferentlySizedDroppedColumsInFiveCluster()
{
	PGconn	   *connection = connectToFive();

	PQclear(executeQuery(connection, "CREATE SCHEMA five_to_six_upgrade;"));
	PQclear(executeQuery(connection, "SET search_path TO five_to_six_upgrade"));

	PQclear(executeQuery(connection,
		"CREATE TABLE abuela (a int, b int, c int) DISTRIBUTED BY (a) "
		"PARTITION BY range(c) "
		"SUBPARTITION BY range(a) (PARTITION mama START(0) END(42) (SUBPARTITION chico START(0) END(22), SUBPARTITION chica START(22) END(42)));"));
	PQclear(executeQuery(connection, "CREATE TABLE chico(a int, b numeric, c int);"));
	PQclear(executeQuery(connection, "ALTER TABLE abuela DROP COLUMN b;"));
	PQclear(executeQuery(connection, "ALTER TABLE chico DROP COLUMN b;"));
	PQclear(executeQuery(connection, "ALTER TABLE abuela ALTER PARTITION mama EXCHANGE PARTITION chico WITH TABLE chico;"));

	PQfinish(connection);
}

static void
createPartitionedHeapTableWithDifferentlyAlignedFixedDroppedColumsInFiveCluster()
{
	PGconn	   *connection = connectToFive();

	PQclear(executeQuery(connection, "CREATE SCHEMA five_to_six_upgrade;"));
	PQclear(executeQuery(connection, "SET search_path TO five_to_six_upgrade"));

	PQclear(executeQuery(connection,
		"CREATE TABLE abuela (a int, b aclitem, c int) DISTRIBUTED BY (a) "
		"PARTITION BY range(c) "
		"SUBPARTITION BY range(a) (PARTITION mama START(0) END(42) (SUBPARTITION chico START(0) END(22), SUBPARTITION chica START(22) END(42)));"));
	/*
	 * 'b' column is intentionally differently aligned - aclitem has 'i'
	 * alignment and timetz has 'd' alignment. If we allow the upgrade then on
	 * the new cluster we will fetch column 'c' at the wrong offset.
	 */
	PQclear(executeQuery(connection, "CREATE TABLE chico(a int, b timetz, c int);"));
	PQclear(executeQuery(connection, "ALTER TABLE abuela DROP COLUMN b;"));
	PQclear(executeQuery(connection, "INSERT INTO chico VALUES (1, '00:00:00-8', 1), (2, '00:00:00-8', 2);"));
	PQclear(executeQuery(connection, "ALTER TABLE chico DROP COLUMN b;"));
	PQclear(executeQuery(connection, "INSERT INTO abuela VALUES(22, 22), (23, 23);"));
	PQclear(executeQuery(connection, "ALTER TABLE abuela ALTER PARTITION mama EXCHANGE PARTITION chico WITH TABLE chico;"));

	PQfinish(connection);
}

static void
createPartitionedHeapTableWithDifferentlyAlignedVarlenDroppedColumsInFiveCluster()
{
	PGconn	   *connection = connectToFive();

	PQclear(executeQuery(connection, "CREATE SCHEMA five_to_six_upgrade;"));
	PQclear(executeQuery(connection, "SET search_path TO five_to_six_upgrade"));

	PQclear(executeQuery(connection,
		"CREATE TABLE abuela (a int, b float8[], c int) DISTRIBUTED BY (a) "
		"PARTITION BY range(c) "
		"SUBPARTITION BY range(a) (PARTITION mama START(0) END(42) (SUBPARTITION chico START(0) END(22), SUBPARTITION chica START(22) END(42)));"));
	/*
	 * 'b' column is intentionally differently aligned - aclitem has 'i'
	 * alignment and timetz has 'd' alignment. If we allow the upgrade then on
	 * the new cluster we will fetch column 'c' at the wrong offset.
	 */
	PQclear(executeQuery(connection, "CREATE TABLE chico(a int, b numeric, c int);"));
	PQclear(executeQuery(connection, "ALTER TABLE abuela DROP COLUMN b;"));
	PQclear(executeQuery(connection, "INSERT INTO chico VALUES (1, 1.987654321, 1), (2, 2.3456789, 2);"));
	PQclear(executeQuery(connection, "ALTER TABLE chico DROP COLUMN b;"));
	PQclear(executeQuery(connection, "ALTER TABLE abuela ALTER PARTITION mama EXCHANGE PARTITION chico WITH TABLE chico;"));

	PQfinish(connection);
}

static void
anAdministratorPerformsAnUpgradeCheck()
{
	performUpgradeCheck();
}

static void
assert_error_in_log(const char *errMsg)
{
	assert_true(strstr(upgradeCheckOutput(), errMsg));
}

static void
checkFailsHeterogeneousPartitionedTableCheck(void)
{
	assert_int_not_equal(0, upgradeCheckStatus());
	assert_error_in_log("Your installation contains heterogenous partitioned tables");
}

void
test_a_partitioned_heap_table_with_differently_sized_dropped_columns_cannot_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedHeapTableWithDifferentlySizedDroppedColumsInFiveCluster));
	when(anAdministratorPerformsAnUpgradeCheck);
	then(checkFailsHeterogeneousPartitionedTableCheck);
}

void
test_a_partitioned_heap_table_with_differently_aligned_fixed_dropped_columns_cannot_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedHeapTableWithDifferentlyAlignedFixedDroppedColumsInFiveCluster));
	when(anAdministratorPerformsAnUpgradeCheck);
	then(checkFailsHeterogeneousPartitionedTableCheck);
}

void
test_a_partitioned_heap_table_with_differently_aligned_varlen_dropped_columns_cannot_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createPartitionedHeapTableWithDifferentlyAlignedVarlenDroppedColumsInFiveCluster));
	when(anAdministratorPerformsAnUpgradeCheck);
	then(checkFailsHeterogeneousPartitionedTableCheck);
}
