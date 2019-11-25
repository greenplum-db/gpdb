#include <stdio.h>

#include "libpq-fe.h"

/*
 * implements:
 */
#include "query-helpers.h"


PGconn *
connectTo(int port, char *database_name)
{
	char		buffer[1000];

	snprintf(buffer, sizeof(buffer), "dbname=%s port=%d", database_name, port);
	PGconn	   *connection = PQconnectdb(buffer);

	if (PQstatus(connection) != CONNECTION_OK)
		printf("error: failed to connect to greenplum on port %d\n", port);

	return connection;
}

PGresult *
executeQuery(PGconn *connection, const char *query)
{
	ExecStatusType status;

	PGresult   *result = PQexec(connection, query);

	status = PQresultStatus(result);

	if ((status != PGRES_TUPLES_OK) && (status != PGRES_COMMAND_OK))
		printf("query failed: %s, %s\n", query, PQerrorMessage(connection));

	return result;
}

void
executeQueryClearResult(PGconn *connection, const char *query)
{
	PQclear(executeQuery(connection, query));
}
