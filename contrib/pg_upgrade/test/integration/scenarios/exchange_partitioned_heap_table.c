#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"

#include "utilities/bdd-helpers.h"
#include "exchange_partitioned_heap_table.h"

static void
createExchangePartitionedHeapTableInFiveCluster()
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "CREATE SCHEMA five_to_six_upgrade;");
	executeQuery(connection, "SET search_path TO five_to_six_upgrade");

	executeQuery(connection, "CREATE TABLE table_part(a INT, b INT) PARTITION BY RANGE(b) (PARTITION part1 START(0) END(42));");
	executeQuery(connection, "CREATE TABLE replacement(LIKE table_part);");
	executeQuery(connection, "ALTER TABLE table_part EXCHANGE PARTITION part1 WITH TABLE replacement;");
	executeQuery(connection, "CREATE TABLE dependant(d table_part_1_prt_part1[]);");
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
thenCheckFailsWithError(const char* errMsg)
{
	assert_int_not_equal(0, upgradeCheckStatus());
	assert_error_in_log(errMsg);
}

void test_an_exchange_partitioned_heap_table_cannot_be_upgraded(void ** state)
{
	given(withinGpdbFiveCluster(createExchangePartitionedHeapTableInFiveCluster));
	when(anAdministratorPerformsAnUpgradeCheck);
	/* then */
	thenCheckFailsWithError("Array types derived from partitions of a partitioned table must not have dependants.");
}
