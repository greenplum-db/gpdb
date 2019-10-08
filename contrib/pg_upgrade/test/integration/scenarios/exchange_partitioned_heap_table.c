#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "bdd-library/bdd.h"

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
	performUpgradeCheckFailsWithError("Array types derived from partitions of a partitioned table must not have dependants.");
}

void test_an_exchange_partitioned_heap_table_cannot_be_upgraded(void ** state)
{
	given(createExchangePartitionedHeapTableInFiveCluster);
	when(anAdministratorPerformsAnUpgradeCheck);
}
