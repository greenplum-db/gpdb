/*
 * extended_protocol_test.c
 *
 * This program is to test whether exetend-queries run on reader gang
 * or writer gang.
 *
 * More details please refer:
 * https://groups.google.com/a/greenplum.org/forum/#!topic/gpdb-dev/ugsZca1qLXU
 *
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

	PQexec(conn, "create table t_extended_protocol_test(c int) distributed randomly;");
	PQexec(conn, "insert into t_extended_protocol_test select * from generate_series(1, 10);");

	PQprepare(conn, "select_forupdate",
			  "select * from t_extended_protocol_test where c > $1 for update",
			  1, NULL);

	paramValues[0] = "1";
	res = PQexecPrepared(conn, "select_forupdate", 1,
						 paramValues, NULL, NULL, 0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		printf("%s", PQresultErrorMessage(res));
	else
		printf("%s", "extended_protocol_test test ok!\n");
	
	PQclear(res);

	PQfinish(conn);

	return 0;
}
