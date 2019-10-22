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
aoTableShouldHaveDataUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();
	PGresult   *result = executeQuery(connection, "SELECT * FROM five_to_six_upgrade.ao_users;");

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
createAoTableWithDataInFiveCluster(void)
{
	PGconn	   *con1 = connectToFive();

	executeQuery(con1, "CREATE SCHEMA five_to_six_upgrade;");
	executeQuery(con1, "CREATE TABLE five_to_six_upgrade.ao_users (id integer, name text) WITH (appendonly=true) DISTRIBUTED BY (id);");
	executeQuery(con1, "BEGIN;");
	executeQuery(con1, "INSERT INTO five_to_six_upgrade.ao_users VALUES (1, 'Jane')");
	executeQuery(con1, "INSERT INTO five_to_six_upgrade.ao_users VALUES (2, 'John')");

	PGconn	   *con2 = connectToFive();

	executeQuery(con2, "BEGIN;");
	executeQuery(con2, "INSERT INTO five_to_six_upgrade.ao_users VALUES (3, 'Joe')");

	executeQuery(con1, "END;");
	executeQuery(con2, "END;");

	PQfinish(con2);
	PQfinish(con1);
}

static void
anAdministratorPerformsAnUpgrade()
{
	performUpgrade();
}

void
test_an_ao_table_with_data_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createAoTableWithDataInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(aoTableShouldHaveDataUpgradedToSixCluster));
}
