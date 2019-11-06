#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "cmockery.h"
#include "libpq-fe.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"

#include "utilities/bdd-helpers.h"
#include "ao_table.h"
#include "greenplum_five_to_greenplum_six_upgrade_test_suite.h"

typedef struct UserData
{
	int			id;
	char	   *name;
} User;

static bool
users_match(const User * expected_user, const User * actual_user)
{
	return
		expected_user->id == actual_user->id &&
			strncmp(expected_user->name, actual_user->name, strlen(expected_user->name)) == 0
		;
}

typedef struct Rows
{
	int			size;
	User		rows[10];
} Rows;

static void
assert_rows_contain_users(const Rows *expected_rows, const Rows *rows)
{
	bool		found = false;

	for (int j = 0; j < expected_rows->size; ++j)
	{
		found = false;
		const		User *expected_user = &expected_rows->rows[j];

		for (int i = 0; i < rows->size; ++i)
		{
			const		User *current_user = &rows->rows[i];

			if (users_match(expected_user, current_user))
			{
				found = true;
				break;
			}
		}
		assert_true(found);
	}
	assert_true(found);
}

static void
extract_user_rows(PGresult *result, Rows *rows)
{
	int			number_of_rows = PQntuples(result);

	const int	i_id = PQfnumber(result, "id");
	const int	i_name = PQfnumber(result, "name");

	for (int i = 0; i < number_of_rows; i++)
	{
		User	   *user = &rows->rows[i];

		user->id = atoi(PQgetvalue(result, i, i_id));
		user->name = PQgetvalue(result, i, i_name);
	}
	rows->size = number_of_rows;
}

static void
aoTableShouldHaveDataUpgradedToSixCluster(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result = executeQuery(connection, "SELECT * FROM ao_users;");

	Rows		rows = {};

	extract_user_rows(result, &rows);

	assert_int_equal(3, rows.size);
	const Rows	expected_users = {
		.size = 3,
		.rows = {
			{.id = 1,.name = "Jane"},
			{.id = 2,.name = "John"},
			{.id = 3,.name = "Joe"}
		}
	};

	assert_rows_contain_users(&expected_users, &rows);
	PQfinish(connection);
}

static void
createAoTableWithDataInFiveCluster(void **state)
{
	PGconn	   *con1 = connectToFive();

	executeQueryClearResult(con1, "CREATE TABLE ao_users (id integer, name text) WITH (appendonly=true) DISTRIBUTED BY (id);");
	executeQueryClearResult(con1, "BEGIN;");
	executeQueryClearResult(con1, "INSERT INTO ao_users VALUES (1, 'Jane')");
	executeQueryClearResult(con1, "INSERT INTO ao_users VALUES (2, 'John')");

	PGconn	   *con2 = connectToFive();

	executeQueryClearResult(con2, "BEGIN;");
	executeQueryClearResult(con2, "INSERT INTO ao_users VALUES (3, 'Joe')");

	executeQueryClearResult(con1, "END;");
	executeQueryClearResult(con2, "END;");

	PQfinish(con2);
	PQfinish(con1);
}

void
test_an_ao_table_with_data_can_be_upgraded(void)
{
	unit_test_given(createAoTableWithDataInFiveCluster, "test_an_ao_table_with_data_can_be_upgraded");
	unit_test_then(aoTableShouldHaveDataUpgradedToSixCluster, "test_an_ao_table_with_data_can_be_upgraded");
}
