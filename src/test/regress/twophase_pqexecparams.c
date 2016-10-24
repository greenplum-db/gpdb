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



/*
 * This function prints a query result that is a fetch from the test table.
 */
static void
show_results(PGresult *res)
{
	int			i,
				j;
	int			i_fnum,
				t_fnum;


	/* Use PQfnumber to avoid assumptions about field order in result */
	i_fnum = PQfnumber(res, "i");
	t_fnum = PQfnumber(res, "t");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *iptr;
		char	   *tptr;
		int			ival;

		/* Get the field values (we ignore possibility they are null!) */
		iptr = PQgetvalue(res, i, i_fnum);
		tptr = PQgetvalue(res, i, t_fnum);

		/*
		 * The binary representation of INT4 is in network byte order, which
		 * we'd better coerce to the local byte order.
		 */
		ival = ntohl(*((uint32_t *) iptr));


		printf("tuple %d: got\n", i);
		printf(" i = (%d bytes) %d\n",
			   PQgetlength(res, i, i_fnum), ival);
		printf(" t = (%d bytes) '%s'\n",
			   PQgetlength(res, i, t_fnum), tptr);
		printf("\n");
	}
}



int
main(int argc, char **argv)
{
	const char *conninfo;
	PGconn	   *conn;
	PGresult   *res;
	const char *paramValues[1];

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

	paramValues[0] = "joe's place";

	/* Upone receving the SELECT below, the segment will error out due to the
	 * fault-injector GUCs set earlier.  However, the master will retry and we
	 * should get a message saying that retry succeeded.
	 */

	res = PQexecParams(conn,
					   "SELECT * FROM test1 WHERE t = $1",
					   1,		/* one param */
					   NULL,	/* let the backend deduce param type */
					   paramValues,
					   NULL,	/* don't need param lengths since text */
					   NULL,	/* default to all text params */
					   1);		/* ask for binary results */

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		exit_nicely(conn);
	}

	show_results(res);

	PQclear(res);

	PQfinish(conn);

	return 0;
}
