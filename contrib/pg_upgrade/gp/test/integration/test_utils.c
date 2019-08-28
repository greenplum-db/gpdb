#include "test_utils.h"
#include "../../../pg_upgrade.h"
#include "fe_utils/connect.h"

#define GREENPLUM_5_VERSION_NUMBER 80300

/*
 * Functions used to aid writing of integration tests:
 * 
 */

/*
 * Execute a query on the given connection
 *
 * query can have arguments interpolated:
 *
 *  char *table_name = "users";
 *  executeQuery(connection, "select * from %s;", table_name);
 *
 */
void
executeQuery(PGconn *connection, const char *query,...)
{
	static char command[8192];
	va_list		args;
	ExecStatusType status;

	/*
	 * Interpolate variable length arguments into query
	 */
	va_start(args, query);
	vsnprintf(command, sizeof(command), query, args);
	va_end(args);

	PGresult *result = PQexec(connection, command);

	status = PQresultStatus(result);

	if ((status != PGRES_TUPLES_OK) && (status != PGRES_COMMAND_OK))
		printf("query failed: %s, %s\n", query, PQerrorMessage(connection));

}

ClusterInfo *
make_cluster()
{
	ClusterInfo *cluster = palloc0(sizeof(ClusterInfo));
	DbInfoArr *info = palloc0(sizeof(ClusterInfo));

	cluster->dbarr = *info;
	cluster->dbarr.dbs = NULL;
	cluster->dbarr.ndbs = 0;

	return cluster;
}


/*
 * getTestConnectionToDatabase:
 *
 * Connect to a database in the ClusterInfo's connection
 * credentials
 *
 * Note: this connection is not in utility mode
 *
 */
PGconn *
getTestConnection(ClusterInfo *cluster)
{
	return getTestConnectionToDatabase(cluster, get_database_name());
}


/*
 * getTestConnectionToDatabase:
 *
 * Connect to a specific database by name given a ClusterInfo's connection
 * credentials
 *
 * Using code from server.c connectToServer and get_db_conn
 * to avoid merge conflict and still achieve a connection.
 *
 * Note: this connection is not in utility mode
 *
 */
PGconn *
getTestConnectionToDatabase(ClusterInfo *cluster, char * const database_name)
{
	PQExpBufferData conn_opts;
	PGconn	   *conn;
	
	/* Build connection string with proper quoting */
	initPQExpBuffer(&conn_opts);
	appendPQExpBufferStr(&conn_opts, "dbname=");
	appendConnStrVal(&conn_opts, database_name);
	appendPQExpBufferStr(&conn_opts, " user=");
	appendConnStrVal(&conn_opts, os_info.user);
	appendPQExpBuffer(&conn_opts, " port=%d", cluster->port);

	if (cluster->sockdir)
	{
		appendPQExpBufferStr(&conn_opts, " host=");
		appendConnStrVal(&conn_opts, cluster->sockdir);
	}

	conn = PQconnectdb(conn_opts.data);
	termPQExpBuffer(&conn_opts);

	if (conn == NULL || PQstatus(conn) != CONNECTION_OK)
	{
		pg_log(PG_REPORT, "connection to database failed: %s\n",
		       PQerrorMessage(conn));

		if (conn)
			PQfinish(conn);

		printf("Failure, exiting\n");
		exit(1);
	}

	PQclear(executeQueryOrDie(conn, ALWAYS_SECURE_SEARCH_PATH_SQL));

	return conn;
}

void
setup_cluster(ClusterInfo *cluster)
{
	char *port_string = getenv("PGPORTOLD");

	if (!port_string)
	{
		printf("Must set PGPORTOLD to run tests.\n");
		exit(1);
	}

	cluster->port = atoi(port_string);
	cluster->major_version = GREENPLUM_5_VERSION_NUMBER;

	/*
	 * get_db_and_rel_infos currently depends on old_cluster being set:
	 * todo: remove dependency on get_db_and_rel_infos in test
	 */
	old_cluster = *cluster;
}

char *
get_database_name()
{
	return "postgres";
}

void
setup_os_info()
{
	os_info.user = getenv("USER");
}
