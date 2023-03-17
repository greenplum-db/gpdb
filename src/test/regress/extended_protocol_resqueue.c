/*
 * extended_protocol_resqueue.c
 *
 * This program is to test whether exetend-queries can be controled properly 
 * by resource queue.
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
	const char *execrole;
	PGconn     *conn;
	PGresult   *res;
	const char *paramValues[1];
	char		buf[80];

	conninfo = argv[1];
	execrole = argv[2];

	/* Make a connection to the database */
	conn = PQconnectdb(conninfo);

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database failed: %s",
				PQerrorMessage(conn));
		exit_nicely(conn);
	}

	/* Change session role to non_superuser */
	snprintf(buf, 80, "SET SESSION AUTHORIZATION %s;", execrole);
	PQexec(conn, buf);
	
	/* Set guc to print memory requested when resource queue is enabled */
	PQexec(conn, "set gp_log_resqueue_memory = 1;");
	
	res = PQprepare(conn, "extend_query_cursor", "select pg_sleep(1);", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("%s:%d: Execution failed: %s", __FILE__, __LINE__, PQresultErrorMessage(res));
		exit_nicely(conn);
	}
	PQclear(res);
	
	PQexecPrepared(conn, "extend_query_cursor", 0, NULL, NULL, NULL, 0);
	
	PQfinish(conn);
	
	return 0;
}
