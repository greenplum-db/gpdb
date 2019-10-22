#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "cmockery.h"
#include "libpq-fe.h"

#include "utilities/bdd-helpers.h"
#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/row-assertions.h"

#include "subpartitioned_heap_table.h"


typedef struct UserData
{
	int id;
	int age;
} User;


static Rows *
extract_rows(PGresult *result)
{
	int number_of_rows = PQntuples(result);

	Rows *rows = calloc(1, sizeof(Rows));

	int id_column_index  = PQfnumber(result, "id");
	int age_column_index = PQfnumber(result, "age");

	for (int i = 0; i < number_of_rows; i++)
	{
		User *user = calloc(1, sizeof(User));
		user->id  = atoi(PQgetvalue(result, i, id_column_index));
		user->age = atoi(PQgetvalue(result, i, age_column_index));

		rows->rows[i] = user;
	}

	rows->size = number_of_rows;

	return rows;
}

static Rows *
queryForRows(char *queryString)
{
	PGconn *connection = connectToSix();
	executeQuery(connection, "set search_path to 'five_to_six_upgrade';");
	PGresult *result = executeQuery(connection, queryString);

	Rows *rows = extract_rows(result);
	PQfinish(connection);

	return rows;
}

static bool
users_match(void *expected, void *actual)
{
	User *first_user = (User *) expected;
	User *second_user = (User *) actual;
	
	return first_user->age == second_user->age &&
		first_user->id == second_user->id;
}

static void match_failed_for_user(void *expected_row)
{
	User *expected_user = (User*) expected_row;

	printf("==============> expected {.id=%d, .age=%d} to be in actual rows\n",
	       expected_user->id,
	       expected_user->age);
}



static void
aSubpartitionedHeapTableHasDataInAGpdbFiveCluster(void)
{
	PGconn *connection = connectToFive();

	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to 'five_to_six_upgrade';");
	executeQuery(connection, "create table users (id int, age int) distributed by (id) partition by range (id) subpartition by range (age) (partition partition_id start(1) end(3) ( subpartition subpartition_age_first start(1) end(20), subpartition subpartition_age_second start(20) end(30) ))");
	executeQuery(connection, "insert into users (id, age) values (1, 10), (2, 20)");
	executeQuery(connection, "vacuum freeze;");

	PQfinish(connection);
}

static void
anAdministratorPerformsAnUpgrade(void)
{
	performUpgrade();
}

static void
theSubpartitionShouldExistWithDataInTheGpdbSixCluster(void)
{
	matcher = users_match;
	match_failed = match_failed_for_user;

	Rows *rows_in_partition_a = queryForRows("select id, age from users_1_prt_partition_id_2_prt_subpartition_age_first");
	Rows *rows_in_partition_b = queryForRows("select id, age from users_1_prt_partition_id_2_prt_subpartition_age_second");

	User expected_user = {.id = 1, .age = 10};
	User other_expected_user = {.id = 2, .age = 20};

	assert_rows(rows_in_partition_a, (Rows) {
		.size = 1,
		.rows = {&expected_user}
	});

	assert_rows(rows_in_partition_b, (Rows) {
		.size = 1,
		.rows = {&other_expected_user}
	});

	Rows *all_rows = queryForRows("select id, age from users;");

	assert_rows(all_rows, (Rows) {
		.size = 2,
		.rows = {&expected_user, &other_expected_user}
	});
}

void
test_a_subpartitioned_heap_table_with_data_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(aSubpartitionedHeapTableHasDataInAGpdbFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(theSubpartitionShouldExistWithDataInTheGpdbSixCluster));
}