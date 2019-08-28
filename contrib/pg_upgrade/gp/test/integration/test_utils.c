#include "test_utils.h"
#include "../../../pg_upgrade.h"
#include "../../cluster.h"

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

	init_cluster_for_greenplum_checks(cluster);

	return cluster;
}

PGconn *
getTestConnection(ClusterInfo *cluster)
{
	disable_utility_mode(cluster);

	return connectToServer(cluster, get_database_name());
}

PGconn *
getTestConnectionToDatabase(ClusterInfo *cluster, const char *database_name)
{
	disable_utility_mode(cluster);

	return connectToServer(cluster, database_name);
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

void setup_os_info()
{
	os_info.user = getenv("USER");
}

void
enable_utility_mode(ClusterInfo *clusterInfo)
{
	clusterInfo->use_utility_mode = true;
}

void
disable_utility_mode(ClusterInfo *clusterInfo)
{
	clusterInfo->use_utility_mode = false;
}
