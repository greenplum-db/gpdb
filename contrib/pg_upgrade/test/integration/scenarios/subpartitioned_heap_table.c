#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "cmockery.h"
#include "libpq-fe.h"

#include "bdd-library/bdd.h"

#include "utilities/upgrade-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/query-helpers.h"

#include "subpartitioned_heap_table.h"

typedef struct UserData
{
	int id;
	int age;
} User;

typedef struct RowsData
{
	int  size;
	User *users[10];
} Rows;

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

		rows->users[i] = user;
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
users_match(User *first_user, User *second_user)
{
	return first_user->age == second_user->age &&
		first_user->id == second_user->id;
}

static bool
row_in(User *expected_user, Rows *actual_rows)
{
	for (int i = 0; i < actual_rows->size; i++)
	{
		User *current_user = actual_rows->users[i];

		if (users_match(current_user, expected_user))
			return true;
	}

	return false;
}

static void
assert_row_in(User *expected_user, Rows *actual_rows)
{
	bool found = row_in(expected_user, actual_rows);

	if (!found)
		printf("==============> expected {.id=%d, .age=%d} to be in actual rows\n",
			expected_user->id,
			expected_user->age);

	assert_true(found);
}

static void
assert_rows(Rows *actual_rows, Rows expected_rows)
{
	for (int i = 0; i < expected_rows.size; i++)
		assert_row_in(expected_rows.users[i], actual_rows);
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
	Rows *rows_in_partition_a = queryForRows("select id, age from users_1_prt_partition_id_2_prt_subpartition_age_first");
	Rows *rows_in_partition_b = queryForRows("select id, age from users_1_prt_partition_id_2_prt_subpartition_age_second");

	User expected_user = {.id = 1, .age = 10};
	User other_expected_user = {.id = 2, .age = 20};

	assert_rows(rows_in_partition_a, (Rows) {
		.size = 1,
		.users = {&expected_user}
	});

	assert_rows(rows_in_partition_b, (Rows) {
		.size = 1,
		.users = {&other_expected_user}
	});

	Rows *all_rows = queryForRows("select id, age from users;");

	assert_rows(all_rows, (Rows) {
		.size = 2,
		.users = {&expected_user, &other_expected_user}
	});
}

void
test_a_subpartitioned_heap_table_with_data_can_be_upgraded(void **state)
{
	given(aSubpartitionedHeapTableHasDataInAGpdbFiveCluster);
	when(anAdministratorPerformsAnUpgrade);
	then(theSubpartitionShouldExistWithDataInTheGpdbSixCluster);
}