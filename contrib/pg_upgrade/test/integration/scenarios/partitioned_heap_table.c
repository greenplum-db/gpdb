#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "partitioned_heap_table.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "bdd-library/bdd.h"

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

void
test_a_partitioned_heap_table_with_data_can_be_upgraded(void **state)
{
	given(createPartitionedHeapTableWithDataInFiveCluster);
	when(anAdministratorPerformsAnUpgrade);
	then(partitionedHeapTableShouldHaveDataUpgradedToSixCluster);

}
