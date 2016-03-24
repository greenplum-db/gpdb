#include <stdio.h>
#include <stdlib.h>
#include "libpq-fe.h"

#include <sys/time.h>

static PGconn *conn;

static struct
{
	const char *testname;
	int time_ms;
} results[1000];
static int nresults = 0;

static const char *run_name;


static void
recordResult(const char *testname, int time_ms)
{
	results[nresults].testname = testname;
	results[nresults].time_ms = time_ms;
	nresults++;
}

static void
printResults(void)
{
	int			i;

	printf("-------\n");
	for (i = 0; i < nresults; i++)
	{
		printf("%s\t%s\t%d\n", run_name, results[i].testname, results[i].time_ms);
	}
	printf("-------\n");
}

static void
exit_nicely(void)
{
	PQfinish(conn);
	exit(1);
}

static void
die(const char *errmsg)
{
	fprintf(stderr, "Error: %s\n", errmsg);
	exit(1);
}


static void
executeSimple(const char *sql)
{
	PGresult   *res;

	printf("Executing query: %.65s\n", sql);
	res = PQexec(conn, sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "command failed: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit_nicely();
	}
	PQclear(res);
}

static int
executeTimed(const char *testname, const char *sql)
{
	int			nFields;
	int			i,
				j;
	PGresult   *res;
	struct timeval begintv;
	struct timeval endtv;
	int			time_ms;

	printf("Executing test %s...\n", testname);
	
	gettimeofday(&begintv, NULL);
	res = PQexec(conn, sql);
	gettimeofday(&endtv, NULL);
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "query \"%s\" failed: %s", sql, PQerrorMessage(conn));
		PQclear(res);
		exit_nicely();
	}

	/* first, print out the attribute names */
	nFields = PQnfields(res);
	for (i = 0; i < nFields; i++)
		printf("%-15s", PQfname(res, i));
	printf("\n\n");

	/* next, print out the rows */
	for (i = 0; i < PQntuples(res); i++)
	{
		for (j = 0; j < nFields; j++)
			printf("%-15s", PQgetvalue(res, i, j));
		printf("\n");
	}
	
	PQclear(res);

	time_ms = (endtv.tv_sec - begintv.tv_sec) * 1000 +
		(endtv.tv_usec - begintv.tv_usec + 500 /* round */) / 1000;

	printf("Elapsed: %d ms\n", time_ms);

	recordResult(testname, time_ms);
	
	return time_ms;
}

static void
reconnect(const char *conninfo)
{
	/* Make a connection to the database */
	conn = PQconnectdb(conninfo);

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database failed: %s",
				PQerrorMessage(conn));
		exit_nicely();
	}
}

static void
gen_largedispatchfunc(int size)
{
	char	   *buf = malloc(size + 1);
	char	   *sql = malloc(size + 500);
	int			i;

	if (buf == NULL || sql == NULL)
		die("out of memory");

	for (i = 0; i < size; i++)
		buf[i] = 'x';
	buf[i] = '\0';
	
	snprintf(sql, size + 500,
			 "create or replace function dsptest.largedispatch_%d() returns bigint as $$"
			 " select count(*) from dsptest.testtab where t in ('%s'); "
			 "$$ language sql immutable",
			 size, buf);

	executeSimple(sql);
}

int
main(int argc, char **argv)
{
	const char *conninfo;

	/*
	 * If the user supplies a parameter on the command line, use it as the
	 * conninfo string; otherwise default to setting dbname=postgres and using
	 * environment variables or defaults for all other connection parameters.
	 */
	if (argc > 1)
		conninfo = argv[1];
	else
		conninfo = "dbname = postgres";

	if (argc > 2)
		run_name = argv[2];
	else
		run_name = "run";
	
	reconnect(conninfo);

	/* Prepare */
	executeSimple("DROP SCHEMA IF EXISTS dsptest CASCADE");
	executeSimple("CREATE SCHEMA dsptest");
	executeSimple("CREATE TABLE dsptest.testtab (id integer, t text)");
	executeSimple("INSERT INTO dsptest.testtab VALUES (1, 'foo'), (2, 'foo')");

	gen_largedispatchfunc(100);
	gen_largedispatchfunc(1000);
	gen_largedispatchfunc(10000);
	gen_largedispatchfunc(100000);
	gen_largedispatchfunc(1000000);
	gen_largedispatchfunc(10000000);

	/* Execute */

	reconnect(conninfo);
	printf ("Test 1: small query after connecting\n");
	executeTimed("first query", "SELECT * FROM dsptest.testtab");
	printf ("Test 2: small query \n");
	executeTimed("second query", "SELECT * FROM dsptest.testtab");

	executeTimed("large query 100", "SELECT dsptest.largedispatch_100()");
	executeTimed("large query 1000", "SELECT dsptest.largedispatch_1000()");
	executeTimed("large query 10000", "SELECT dsptest.largedispatch_10000()");
	executeTimed("large query 100000", "SELECT dsptest.largedispatch_100000()");
	executeTimed("large query 1000000", "SELECT dsptest.largedispatch_1000000()");
	executeTimed("large query 10000000", "SELECT dsptest.largedispatch_10000000()");
	
	/* close the connection to the database and cleanup */
	PQfinish(conn);

	printResults();

	return 0;
}
