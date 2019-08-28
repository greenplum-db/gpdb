#include "pg_upgrade.h"


#include "cluster.h"


static const char * QUERY_ALL_USER_DEFINED_INDEXES = ""
"SELECT nsp.nspname, relkind, indexrelid::regclass, indrelid::regclass "
"    FROM pg_index ind "
"    LEFT JOIN pg_class rel ON ind.indexrelid = rel.oid "
"    AND relkind IN ('i', '')"
"    LEFT JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid "
"    WHERE nsp.nspname NOT IN ("
"        'pg_catalog', 'pg_toast', 'pg_aoseg', 'information_schema'"
"    )"
"    AND nsp.nspname !~ '^pg_toast';";


static struct UserDefinedIndexes 
query_for_indexes(void *cluster)
{
	PGresult *result;
	PGconn   *connection;

	ClusterInfo *clusterInfo;
	DbInfo   *active_db;

	int dbnum;
	int rowno;

	struct UserDefinedIndexes indexes;
	indexes.number_of_user_defined_indexes = 0;

	clusterInfo = (ClusterInfo *) cluster;

	for (dbnum = 0; dbnum < clusterInfo->dbarr.ndbs; dbnum++)
	{
		active_db = &clusterInfo->dbarr.dbs[dbnum];
		connection = connectToServer(clusterInfo, active_db->db_name);
		result = executeQueryOrDie(connection, QUERY_ALL_USER_DEFINED_INDEXES);

		for (rowno = 0; rowno < PQntuples(result); rowno++)
			indexes.number_of_user_defined_indexes++;
	}

	return indexes;
}


void
init_cluster_for_greenplum_checks(ClusterInfo *cluster)
{
	cluster->query_for_indexes = query_for_indexes;
}
