/*
 * extended_protocol_commit_test.c
 *
 * This program is to test whether exetend-queries properly work with FOREIGN
 * TABLEs at transaction commit.
 *
 * See https://github.com/greenplum-db/gpdb/issues/10918
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "libpq-fe.h"


static void
exit_nicely(PGconn *conn)
{
	PQfinish(conn);
	exit(1);
}

int
main(int argc, char **argv)
{
	const char *conninfo;
	PGconn	   *conn;
	PGresult   *res;

	/*
	 * If the user supplies a parameter on the command line, use it as the
	 * conninfo string; otherwise default to setting dbname=postgres and using
	 * environment variables or defaults for all other connection parameters.
	 */
	if (argc > 1)
		conninfo = argv[1];
	else
		conninfo = "dbname = postgres";

	/* Connect to the database */
	conn = PQconnectdb(conninfo);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		printf("%s:%d: Connection to database failed: %s", __FILE__, __LINE__, PQerrorMessage(conn));
		exit_nicely(conn);
	}

	/* Prepare objects for the test */
	res = PQexec(conn, "SET client_min_messages = ERROR;");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	res = PQexec(conn, "DROP EXTENSION IF EXISTS extended_protocol_commit_test_fdw CASCADE;");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	res = PQexec(conn, "CREATE EXTENSION extended_protocol_commit_test_fdw;");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	res = PQexec(conn, "CREATE SERVER extended_protocol_commit_test_server FOREIGN DATA WRAPPER extended_protocol_commit_test_fdw;");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	res = PQexec(conn, "CREATE FOREIGN TABLE extended_protocol_commit_test_table(i INT, t TEXT) SERVER extended_protocol_commit_test_server;");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	res = PQexec(conn, "SELECT * FROM extended_protocol_commit_test_table;");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);

	/* The test itself */
	res = PQprepare(conn, "extend_query_cursor", "SELECT i, t FROM extended_protocol_commit_test_table;", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	res = PQexecPrepared(conn, "extend_query_cursor", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);

	printf("extended_protocol_commit_test passed\n");

	PQfinish(conn);
	return 0;
}
