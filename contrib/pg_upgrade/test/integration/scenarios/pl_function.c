#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "cmockery.h"
#include "libpq-fe.h"

#include "utilities/bdd-helpers.h"
#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/upgrade-helpers.h"

#include "pl_function.h"
#include "greenplum_five_to_greenplum_six_upgrade_test_suite.h"

static void
createPlpgsqlFunctionInFiveCluster(void **state)
{
	PGconn	   *conn1 = connectToFive();
	PGresult   *result;

	result = executeQuery(conn1, "                                  \
		CREATE FUNCTION someimmutablefunction2(foo integer)          \
		RETURNS integer                                             \
		LANGUAGE plpgsql IMMUTABLE STRICT AS                        \
		$$                                                          \
		BEGIN                                                       \
			return 42 + foo;                                        \
		END                                                         \
		$$;                                                         \
	");
	PQclear(result);

	PQfinish(conn1);
}

static void
createPlpythonFunctionInFiveCluster(void **state)
{
	PGconn	   *conn1 = connectToFive();
	PGresult   *result;

	result = executeQuery(conn1, "CREATE LANGUAGE plpythonu");
	PQclear(result);

	result = executeQuery(conn1, "                                  \
		CREATE FUNCTION someimmutablefunction(foo integer)          \
		RETURNS integer                                             \
		LANGUAGE plpythonu IMMUTABLE STRICT AS                      \
		$$                                                          \
			return 42 + foo;                                        \
		$$;                                                         \
	");
	PQclear(result);

	PQfinish(conn1);
}

static void
thePlFunctionIsUsable(void **state)
{
	PGconn	   *connection = connectToSix();
	PGresult   *result = NULL;
	char	   *resvalue = NULL;
	int			nrows;
	int			actual;

	result = executeQuery(connection, "SELECT someimmutablefunction2(0) as f;");
	nrows = PQntuples(result);
	resvalue = PQgetvalue(result, 0, PQfnumber(result, "f"));
	actual = atoi((resvalue != NULL) ? resvalue : 0);

	PQclear(result);
	PQfinish(connection);

	assert_int_equal(1, nrows);
	assert_int_equal(42, actual);
}

void
test_a_plpgsql_function_can_be_upgraded(void)
{
	unit_test_given(createPlpgsqlFunctionInFiveCluster, "test_a_plpgsql_function_can_be_upgraded");
	unit_test_then(thePlFunctionIsUsable, "test_a_plpgsql_function_can_be_upgraded");
}

void
test_a_plpython_function_can_be_upgraded(void)
{
	unit_test_given(createPlpythonFunctionInFiveCluster, "test_a_plpython_function_can_be_upgraded");
	unit_test_then(thePlFunctionIsUsable, "test_a_plpython_function_can_be_upgraded");
}
