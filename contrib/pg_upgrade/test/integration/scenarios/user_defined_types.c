#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>

#include "cmockery.h"
#include "libpq-fe.h"

#include "utilities/bdd-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/upgrade-helpers.h"

#include "user_defined_types.h"

#define NumberOfRows	8

typedef struct RowData
{
	char   *id;
	char   *sc;
	char   *ss;
} RowData;

typedef struct Rows
{
	int		size;
	RowData rows[NumberOfRows];
} Rows;

static void
extract_rows_from_result(PGresult *result, Rows *rows)
{
	int number_of_rows = PQntuples(result);
	int col_id = PQfnumber(result, "id");
	int col_sc = PQfnumber(result, "sc");
	int col_ss = PQfnumber(result, "ss");

	rows->size = number_of_rows;
	if (rows->size > NumberOfRows)
		rows->size = NumberOfRows;

	for (int i = 0; i < rows->size; i++)
	{
		RowData	*row = &rows->rows[i];

		row->id = PQgetvalue(result, i, col_id);
		row->sc = PQgetvalue(result, i, col_sc);
		row->ss = PQgetvalue(result, i, col_ss);
	}
}

static void
executeQueryAndClearResult(PGconn *conn, char *query)
{
	PGresult *result;

	result = executeQuery(conn, query);
	PQclear(result);
}

static void
createUserDefinedTypesReferencedInTableInFiveCluster(void)
{
	PGconn	   *conn = connectToFive();

	executeQueryAndClearResult(conn, "CREATE SCHEMA five_to_six_upgrade;");
	executeQueryAndClearResult(conn, "SET search_path TO five_to_six_upgrade;");
	executeQueryAndClearResult(conn, "                                         \
		CREATE DOMAIN some_check AS text                                       \
		CHECK (value ~ '^[1-9][0-9]-[0-9]{3}$');                               \
	");
	executeQueryAndClearResult(conn, "                                         \
		CREATE TYPE some_state AS ENUM ('warmup', 'qualify', 'race');          \
	");
	executeQueryAndClearResult(conn, "                                         \
		CREATE TABLE some_table (id integer, sc some_check, ss some_state);    \
	");
	executeQueryAndClearResult(conn, "                                         \
		INSERT INTO some_table VALUES                                          \
			(1, '10-100', 'warmup'),                                           \
			(2, '20-200', 'qualify'),                                          \
			(3, '30-300', 'race');                                             \
	");

	PQfinish(conn);
}

static void
anAdministratorPerformsAnUpgrade(void)
{
	performUpgrade();
}

static void
userDefinedTypesShouldBeAccessibleInTableInSixCluster(void)
{
	PGconn	   *conn = connectToSix();
	PGresult   *result;
	Rows		rows;

	result  = executeQuery(conn, "                                             \
		SELECT * FROM five_to_six_upgrade.some_table ORDER BY id;              \
	");
	extract_rows_from_result(result, &rows);
	PQfinish(conn);

	assert_int_equal(rows.size, 3);
	assert_true(strcmp(rows.rows[0].id, "1") == 0);
	assert_true(strcmp(rows.rows[0].sc, "10-100") == 0);
	assert_true(strcmp(rows.rows[0].ss, "warmup") == 0);
	assert_true(strcmp(rows.rows[1].id, "2") == 0);
	assert_true(strcmp(rows.rows[1].sc, "20-200") == 0);
	assert_true(strcmp(rows.rows[1].ss, "qualify") == 0);
	assert_true(strcmp(rows.rows[2].id, "3") == 0);
	assert_true(strcmp(rows.rows[2].sc, "30-300") == 0);
	assert_true(strcmp(rows.rows[2].ss, "race") == 0);
}

void
test_an_user_defined_type_extension_can_be_upgraded(void **state)
{
	given(withinGpdbFiveCluster(createUserDefinedTypesReferencedInTableInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(userDefinedTypesShouldBeAccessibleInTableInSixCluster));
}
