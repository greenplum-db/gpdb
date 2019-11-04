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

#include "user_defined_extension_type.h"

static void
createUserDefinedExtensionTypeFiveCluster(void)
{
	PGconn	   *conn = connectToFive();
	PGresult   *result;

	loadGpdbFiveExtensionOldSyntax("./gpdb5/share/postgresql/contrib/gp_svec.sql");

	result = executeQuery(conn, "CREATE TABLE udte (a int, b svec);");
	if (PQresultStatus(result) != PGRES_COMMAND_OK)
	{
		PQclear(result);
		PQfinish(conn);
		assert_true(0);
	}

	executeQueryClearResult(conn, "                                            \
		INSERT INTO udte (                                                     \
			SELECT 1,                                                          \
			public.gp_extract_feature_histogram(                               \
				'{\"one\",\"two\",\"three\",\"four\",\"five\",\"six\"}',       \
				'{\"four\",\"five\",\"six\",\"one\",\"three\",\"two\",\"one\"}'\
			)                                                                  \
		);                                                                     \
	");
	executeQueryClearResult(conn, "                                            \
		INSERT INTO udte (                                                     \
			SELECT 2,                                                          \
			public.gp_extract_feature_histogram(                               \
				'{\"one\",\"two\",\"three\",\"four\",\"five\",\"six\"}',       \
				'{\"the\",\"brown\",\"cat\",\"ran\",\"across\",\"three\"}'     \
			)                                                                  \
		);                                                                     \
	");
	executeQueryClearResult(conn, "                                            \
		INSERT INTO udte (                                                     \
			SELECT 3,                                                          \
			public.gp_extract_feature_histogram(                               \
				'{\"one\",\"two\",\"three\",\"four\",\"five\",\"six\"}',       \
				'{\"two\",\"four\",\"five\",\"six\",\"one\",\"three\"}'        \
			)                                                                  \
		);                                                                     \
	");
	executeQueryClearResult(conn, "                                            \
		CREATE INDEX udte_btree_idx ON udte USING btree(b);                    \
	");

	result = executeQuery(conn, "                                              \
		SELECT a,b::float8[] cross_product_equals FROM                         \
			(SELECT a,b FROM udte) bar WHERE b == bar.b ORDER BY a;            \
	");

	PQfinish(conn);

	assert_true(PQntuples(result) == 3);
}

static void
anAdministratorPerformsAnUpgrade(void)
{
	performUpgrade();
}

static void
theDataIsAccessibleInSixCluster(void)
{
	PGconn	   *conn = connectToSix();
	PGresult   *result;

	executeQueryClearResult(conn, "CREATE EXTENSION gp_sparce_vector;");
	result = executeQuery(conn, "                                              \
		SELECT a,b::float8[] cross_product_equals FROM                         \
			(SELECT a,b FROM udte) foo WHERE b = foo.b ORDER BY a;             \
	");

	PQfinish(conn);

	assert_true(PQntuples(result) == 3);
}

void
test_a_user_defined_extension_type(void **state)
{
	given(withinGpdbFiveCluster(createUserDefinedExtensionTypeFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(theDataIsAccessibleInSixCluster));
}

