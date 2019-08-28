/*
 *
 *  query_for_user_defined_indexes.c
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#include "pg_upgrade.h"

#include "query_for_user_defined_indexes.h"

static const char * const query_all_user_defined_indexes = "" \
	"SELECT count(1) as number_of_user_defined_indexes " \
	"    FROM pg_index ind " \
	"    LEFT JOIN pg_class rel ON ind.indexrelid = rel.oid " \
	"    AND relkind IN ('i', '')" \
	"    LEFT JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid " \
	"    WHERE nsp.nspname NOT IN (" \
	"        'pg_catalog', 'pg_toast', 'pg_aoseg', 'information_schema'" \
	"    )" \
	"    AND nsp.nspname !~ '^pg_toast';";


struct UserDefinedIndexes 
query_for_user_defined_indexes(ClusterInfo *cluster)
{
	PGresult *result;
	PGconn   *connection;
	DbInfo   *active_db;

	int dbnum;

	struct UserDefinedIndexes indexes;
	indexes.number_of_user_defined_indexes = 0;

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		active_db = &cluster->dbarr.dbs[dbnum];
		connection = connectToServer(cluster, active_db->db_name);
		result = executeQueryOrDie(connection, query_all_user_defined_indexes);

		char *count_as_string = PQgetvalue(result,
			0,
			PQfnumber(result, "number_of_user_defined_indexes"));

		indexes.number_of_user_defined_indexes += atoi(count_as_string);

		PQfinish(connection);
	}

	return indexes;
}
