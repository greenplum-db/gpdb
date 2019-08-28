#include "pg_upgrade.h"


#include "queries.h"


#define QUERY_ALL_USER_DEFINED_INDEXES "" \
	"SELECT nsp.nspname, relkind, indexrelid::regclass, indrelid::regclass " \
	"    FROM pg_index ind " \
	"    LEFT JOIN pg_class rel ON ind.indexrelid = rel.oid " \
	"    AND relkind IN ('i', '')" \
	"    LEFT JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid " \
	"    WHERE nsp.nspname NOT IN (" \
	"        'pg_catalog', 'pg_toast', 'pg_aoseg', 'information_schema'" \
	"    )" \
	"    AND nsp.nspname !~ '^pg_toast';"


static struct UserDefinedIndexes 
query_for_indexes(ClusterInfo *cluster)
{
	PGresult *result;
	PGconn   *connection;
	DbInfo   *active_db;

	int dbnum;
	int rowno;

	struct UserDefinedIndexes indexes;
	indexes.number_of_user_defined_indexes = 0;

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		active_db = &cluster->dbarr.dbs[dbnum];
		connection = connectToServer(cluster, active_db->db_name);
		result = executeQueryOrDie(connection, QUERY_ALL_USER_DEFINED_INDEXES);

		for (rowno = 0; rowno < PQntuples(result); rowno++)
			indexes.number_of_user_defined_indexes++;

		PQfinish(connection);
	}

	return indexes;
}


void
init_queries_for_greenplum_checks(Queries *queries)
{
	queries->query_for_indexes = query_for_indexes;
}
