/*
 * twopase_pqexecparams.c
 *
 * This file tests the retry during phase 2 of two phase commit while running an
 * extended query.
 *
 * This test program is modified from src/test/examples/testlibpq3.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "libpq-fe.h"

/* for ntohl/htonl */
#include <netinet/in.h>
#include <arpa/inet.h>


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
	const char *paramValues[2];

	/*
	 * If the user supplies a parameter on the command line, use it as the
	 * conninfo string; otherwise default to setting dbname=postgres and using
	 * environment variables or defaults for all other connection parameters.
	 */
	if (argc > 1)
		conninfo = argv[1];
	else
		conninfo = "dbname = postgres";

	/* Make a connection to the database */
	conn = PQconnectdb(conninfo);

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database failed: %s",
				PQerrorMessage(conn));
		exit_nicely(conn);
	}

	PQexec(conn, "DROP TABLE IF EXISTS test1"); 
	PQexec(conn, "CREATE TABLE test1 (i int4, t text)");
	PQexec(conn, "INSERT INTO test1 values (1, 'joe''s place')");
	PQexec(conn, "INSERT INTO test1 values (2, 'ho there')");

	/*
	 * The following GUCs will cause the segment to error out while trying to
	 * commit the prepared transaction.
	 */

	PQexec(conn, "SET debug_dtm_action_target = \"protocol\"");
	PQexec(conn, "SET debug_dtm_action_protocol = \"commit_prepared\"");
	PQexec(conn, "SET debug_dtm_action_segment = 0");
	PQexec(conn, "SET debug_dtm_action = \"fail_begin_command\"");

	paramValues[0] = "1";
	paramValues[1] = "2";

	/* Upone receving the INSERT below, the segment will error out due to the
	 * fault-injector GUCs set earlier.  However, the master will retry and we
	 * should get a message saying that retry succeeded.
	 */

	res = PQexecParams(conn,
					   "INSERT INTO test1(i) VALUES($1), ($2)",
					   2,		/* one param */
					   NULL,	/* let the backend deduce param type */
					   paramValues,
					   NULL,	/* don't need param lengths since text */
					   NULL,	/* default to all text params */
					   1);		/* ask for binary results */

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "INSERT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		exit_nicely(conn);
	}

	printf("result: %s\n", PQcmdStatus(res));

	PQclear(res);

	PQfinish(conn);

	return 0;
}
