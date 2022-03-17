/*
 * src/test/examples/test_parallel_cursor_extended_query_error.c
 *
 *
 * this program is to test: RETRIEVE statement not allowed in the non-retrieve
 * mode for extended query protocol.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libpq-fe.h"

#define MASTER_CONNECT_INDEX -1

static void
finish_conn_nicely(PGconn *master_conn, PGconn *endpoint_conns[], size_t endpoint_conns_num)
{
	int			i;

	if (master_conn)
		PQfinish(master_conn);

	for (i = 0; i < endpoint_conns_num; i++)
	{
		if (endpoint_conns[i])
			PQfinish(endpoint_conns[i]);
	}

	free(endpoint_conns);
	endpoint_conns_num = 0;
}

static void
check_prepare_conn(PGconn *conn, const char *dbName, int conn_idx)
{
	PGresult   *res;

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database \"%s\" failed: %s",
				dbName, PQerrorMessage(conn));
		exit(1);
	}

	if (conn_idx == MASTER_CONNECT_INDEX)
	{
		/*
		 * Set always-secure search path, so malicous users can't take
		 * control.
		 */
		res = PQexec(conn,
					 "SELECT pg_catalog.set_config('search_path', '', false)");
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
			PQclear(res);
			exit(1);
		}
		PQclear(res);
	}
}

/* execute sql and check it is a command without result set returned */
static int
exec_sql_without_resultset(PGconn *conn, const char *sql, int conn_idx)
{
	PGresult   *res1;

	if (conn_idx == MASTER_CONNECT_INDEX)
		printf("\nExec SQL on Master:\n\t> %s\n", sql);
	else
		printf("\nExec SQL on EndPoint[%d]:\n\t> %s\n", conn_idx, sql);

	res1 = PQexec(conn, sql);
	if (PQresultStatus(res1) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "execute sql failed: \"%s\"\nfailed %s", sql, PQerrorMessage(conn));
		PQclear(res1);
		return 1;
	}

	/*
	 * make sure to PQclear() a PGresult whenever it is no longer needed to
	 * avoid memory leaks
	 */
	PQclear(res1);
	return 0;
}

/* execute sql and print the result set */
static int
exec_sql_with_resultset_in_extended_query_protocol(PGconn *conn, const char *sql, int conn_idx)
{
	PGresult   *res1;
	int			nFields;
	int			i,
				j;
	const char *paramValues[1];

	paramValues[0] = "0";

	if (conn_idx == MASTER_CONNECT_INDEX)
		printf("\nExec SQL on Master:\n\t> %s\n", sql);
	else
		printf("\nExec SQL on EndPoint[%d]:\n\t> %s\n", conn_idx, sql);

	/*
	 * Just for simplicity, here for different connect index, call different
	 * API in the extended query protocol
	 */
	switch (conn_idx)
	{
		case 0:
			res1 = PQprepare(conn, "extend_query_cursor",
							 sql,
							 0, NULL);

			if (PQresultStatus(res1) != PGRES_TUPLES_OK && PQresultStatus(res1) != PGRES_COMMAND_OK)
			{
				fprintf(stdout, "PQprepare() didn't return properly: resultStatus: %d: \"%s\"\nfailed %s", PQresultStatus(res1), sql, PQerrorMessage(conn));
				PQclear(res1);
				return 1;
			}
			res1 = PQexecPrepared(conn, "extend_query_cursor", 0,
								  paramValues, NULL, NULL, 0);

			if (PQresultStatus(res1) != PGRES_TUPLES_OK)
			{
				fprintf(stderr, "PQexecPrepared() didn't return tuples properly: \"%s\"\nfailed %s", sql, PQerrorMessage(conn));
				PQclear(res1);
				return 1;
			}
			break;
		case 1:
			res1 = PQexecParams(conn,
								sql,
								0,	/* one param */
								NULL,	/* let the backend deduce param type */
								paramValues,
								NULL,	/* don't need param lengths since text */
								NULL,	/* default to all text params */
								0); /* ask for binary results */
			if (PQresultStatus(res1) != PGRES_TUPLES_OK)
			{
				fprintf(stderr, "PQexecParams() didn't return tuples properly: \"%s\"\nfailed %s", sql, PQerrorMessage(conn));
				PQclear(res1);
				return 1;
			}
			break;
		case 2:
		default:
			res1 = PQexec(conn, sql);
			if (PQresultStatus(res1) != PGRES_TUPLES_OK)
			{
				fprintf(stderr, "PQexec() didn't return tuples properly: \"%s\"\nfailed %s", sql, PQerrorMessage(conn));
				PQclear(res1);
				return 1;
			}
			break;
	}


	/* first, print out the attribute names */
	nFields = PQnfields(res1);
	for (i = 0; i < nFields; i++)
		printf("%-15s", PQfname(res1, i));
	printf("\n---------\n");

	/* next, print out the instances */
	for (i = 0; i < PQntuples(res1); i++)
	{
		for (j = 0; j < nFields; j++)
			printf("%-15s", PQgetvalue(res1, i, j));
		printf("\n");
	}

	PQclear(res1);
	return 0;
}

/* execute sql and print the result set */
static int
exec_sql_with_resultset(PGconn *conn, const char *sql, int conn_idx)
{
	PGresult   *res1;
	int			nFields;
	int			i,
				j;

	if (conn_idx == MASTER_CONNECT_INDEX)
		printf("\nExec SQL on Master:\n\t> %s\n", sql);
	else
		printf("\nExec SQL on EndPoint[%d]:\n\t> %s\n", conn_idx, sql);;

	res1 = PQexec(conn, sql);
	if (PQresultStatus(res1) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "Query didn't return tuples properly: \"%s\"\nfailed %s", sql, PQerrorMessage(conn));
		PQclear(res1);
		return 1;
	}

	/* first, print out the attribute names */
	nFields = PQnfields(res1);
	for (i = 0; i < nFields; i++)
		printf("%-15s", PQfname(res1, i));
	printf("\n---------\n");

	/* next, print out the instances */
	for (i = 0; i < PQntuples(res1); i++)
	{
		for (j = 0; j < nFields; j++)
			printf("%-15s", PQgetvalue(res1, i, j));
		printf("\n");
	}

	PQclear(res1);
	return 0;
}

/* this function is in the master connection */
static int
exec_check_parallel_cursor(PGconn *master_conn, int isCheckFinish)
{
	int			result = 0;
	PGresult   *res1;
	const char *check_sql = "SELECT * FROM pg_catalog.gp_wait_parallel_retrieve_cursor('myportal', 0);";


	printf("\n------ Begin checking parallel retrieve cursor status ------\n");
	/* call wait mode monitor UDF and it will wait for finish retrieving. */
	if (!isCheckFinish)
	{
		result = exec_sql_with_resultset(master_conn, check_sql, MASTER_CONNECT_INDEX);
	}
	else
	{
		printf("\nExec SQL on Master:\n\t> %s\n", check_sql);
		res1 = PQexec(master_conn, check_sql);
		if (PQresultStatus(res1) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "\"%s\" didn't return tuples properly\n", check_sql);
			PQclear(res1);
			return 1;
		}
		/* firstly check that the row == 1 */
		int			ntup = PQntuples(res1);

		if (ntup != 1)
		{
			fprintf(stderr, "\"%s\" doesn't return correct row number\n", check_sql);
			return 1;
		}

		result = strcmp(PQgetvalue(res1, 0, 0), "t");
		PQclear(res1);
		if (result)
		{
			fprintf(stderr, "\"%s\" doesn't return 'true'\n", check_sql);
			return 1;
		}
	}

	printf("\n------ End checking parallel retrieve cursor status ------\n");
	return result;
}

int
main(int argc, char **argv)
{
	char	   *pghost,
			   *pgport,
			   *pgoptions,
			   *pgoptions_utility_mode,
			   *pgtty;
	char	   *dbName,
			   *dbUser;
	int			i;
	int			retVal;			/* return value for this func */

	PGconn	   *master_conn,
			  **endpoint_conns = NULL;
	size_t		endpoint_conns_num = 0;
	char	  **tokens = NULL,
			  **endpoint_names = NULL;


	/*
	 * PGresult   *res1, *res2;
	 */
	PGresult   *res1;

	if (argc != 3)
	{
		fprintf(stderr, "usage: %s dbUser dbName\n", argv[0]);
		fprintf(stderr, "      show how to use PARALLEL RETRIEVE CURSOR to parallelly retrieve data from multiple endpoints.\n");
		exit(1);
	}
	dbUser = argv[1];
	dbName = argv[2];

	/*
	 * begin, by setting the parameters for a backend connection if the
	 * parameters are null, then the system will try to use reasonable
	 * defaults by looking up environment variables or, failing that, using
	 * hardwired constants
	 */
	pghost = NULL;				/* host name of the backend */
	pgport = NULL;				/* port of the backend */
	pgoptions = NULL;			/* special options to start up the backend
								 * server */
	pgoptions_utility_mode = "-c gp_role=utility";
	pgtty = NULL;				/* debugging tty for the backend */

	/* make a connection to the database */
	master_conn = PQsetdb(pghost, pgport, pgoptions, pgtty, dbName);
	check_prepare_conn(master_conn, dbName, MASTER_CONNECT_INDEX);

	/* do some preparation for test */
	if (exec_sql_without_resultset(master_conn, "DROP TABLE IF EXISTS public.tab_parallel_cursor;", MASTER_CONNECT_INDEX) != 0)
		goto LABEL_ERR;
	if (exec_sql_without_resultset(master_conn, "CREATE TABLE public.tab_parallel_cursor AS SELECT id FROM pg_catalog.generate_series(1,100) id;", MASTER_CONNECT_INDEX) != 0)
		goto LABEL_ERR;

	/*
	 * start a transaction block because PARALLEL RETRIEVE CURSOR only support
	 * WITHOUT HOLD option
	 */
	if (exec_sql_without_resultset(master_conn, "BEGIN;", MASTER_CONNECT_INDEX) != 0)
		goto LABEL_ERR;

	/* declare PARALLEL RETRIEVE CURSOR for this table */
	if (exec_sql_without_resultset(master_conn, "DECLARE myportal PARALLEL RETRIEVE CURSOR FOR select * from public.tab_parallel_cursor;", MASTER_CONNECT_INDEX) != 0)
		goto LABEL_ERR;

	/*
	 * get the endpoints info of this PARALLEL RETRIEVE CURSOR
	 */
	const char *sql1 = "select hostname,port,auth_token,endpointname from pg_catalog.gp_get_endpoints() where cursorname='myportal';";

	printf("\nExec SQL on Master:\n\t> %s\n", sql1);
	res1 = PQexec(master_conn, sql1);
	if (PQresultStatus(res1) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "Cannot find the endpoint for parallel retrieve cursor myportal\n");
		PQclear(res1);
		goto LABEL_ERR;
	}
	/* firstly check that the endpoint info rows > 0 */
	int			ntup = PQntuples(res1);

	if (ntup <= 0)
	{
		fprintf(stderr, "No rows for the endpoint (cursor myportal)\n");
		goto LABEL_ERR;
	}

	endpoint_conns = malloc(ntup * sizeof(PGconn *));
	tokens = malloc(ntup * sizeof(char **));
	endpoint_names = malloc(ntup * sizeof(char *));
	endpoint_conns_num = ntup;

	/*
	 * create utility mode connection to endpoints according to the endpoints
	 * info fetched, so that the RETRIEVE statement should failed.
	 */
	for (i = 0; i < ntup; i++)
	{
		char	   *host = PQgetvalue(res1, i, 0);
		char	   *port = PQgetvalue(res1, i, 1);

		tokens[i] = strdup(PQgetvalue(res1, i, 2));
		endpoint_names[i] = strdup(PQgetvalue(res1, i, 3));

		endpoint_conns[i] = PQsetdbLogin(host, port, pgoptions_utility_mode,
										 pgtty, dbName,
										 dbUser, tokens[i]);
		check_prepare_conn(endpoint_conns[i], dbName, i);
	}
	PQclear(res1);

	/*
	 * Now the endpoint becomes 'READY' after "DECLARE ... PARALLEL RETRIEVE
	 * CURSOR" returns, then we can retrieve the result of the endpoints in
	 * parallel. This section can be executed parallely on different host or
	 * in different threads/processes on the same host. For simplicity, here
	 * just use loop in one process.
	 */
	for (i = 0; i < endpoint_conns_num; i++)
	{
		char		sql[256];

		/*
		 * Run nowait mode monitor UDF to check the parallel retrieve cursor
		 * status at specific time in master connection, so that if any error
		 * occurs, it will detect ASAP.
		 */
		if (exec_check_parallel_cursor(master_conn, 0))
		{
			fprintf(stderr, "Error when checking the PARALLEL RETRIEVE CURSOR\n");
			goto LABEL_ERR;
		}

		/*
		 * the endpoint is ready to be retrieved when 'DECLARE PARALLEL
		 * RETRIEVE CURSOR returns, here begin to retrieve
		 */
		printf("\n------ Begin retrieving data from Endpoint %d# ------\n", i);
		snprintf(sql, sizeof(sql), "RETRIEVE ALL FROM ENDPOINT %s;", endpoint_names[i]);
		if (exec_sql_with_resultset_in_extended_query_protocol(endpoint_conns[i], sql, i))
		{
			/*
			 * This test is to confirmed that all endpoints should be failed
			 * when execute RETRIEVE in UTITILY mode
			 */
			fprintf(stderr, "Error as expected during retrieving result on endpoint.\n");
			printf("\n------ Skip End retrieving data from Endpoint %d# ------.\n", i);
			continue;
		}

		printf("\n------ End retrieving data from Endpoint %d# ------.\n", i);
	}

	/* Check the status returns not finished */
	if (!exec_check_parallel_cursor(master_conn, 1))
	{
		fprintf(stderr, "Error during check the PARALLEL RETRIEVE CURSOR\n");
		goto LABEL_ERR;
	}

	/* close the cursor */
	if (exec_sql_without_resultset(master_conn, "CLOSE myportal;", MASTER_CONNECT_INDEX) != 0)
		goto LABEL_ERR;

	/* end the transaction */
	if (exec_sql_without_resultset(master_conn, "END;", MASTER_CONNECT_INDEX) != 0)
		goto LABEL_ERR;


	/* fclose(debug); */
	retVal = 0;
	goto LABEL_FINISH;

LABEL_ERR:
	retVal = 1;

LABEL_FINISH:
	/* close the connections to the database and cleanup */
	finish_conn_nicely(master_conn, endpoint_conns, endpoint_conns_num);

	if (tokens)
	{
		int			i;

		for (i = 0; i < endpoint_conns_num; i++)
		{
			if (tokens[i])
				free(tokens[i]);
		}
		free(tokens);
	}

	if (endpoint_names)
	{
		for (i = 0; i < endpoint_conns_num; i++)
		{
			if (endpoint_names[i])
				free(endpoint_names[i]);
		}
		free(endpoint_names);
	}

	return retVal;
}
