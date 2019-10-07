#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "partitioned_ao_table.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "bdd-library/bdd.h"

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

void test_a_partitioned_ao_table_with_data_can_be_upgraded(void **state)
{
	given(createPartitionedAOTableWithDataInFiveCluster);
	when(anAdministratorPerformsAnUpgrade);
	then(partitionedAOTableShouldHaveDataUpgradedToSixCluster);
}
