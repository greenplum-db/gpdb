
/*
 *
 *  query_for_user_defined_indexes.c
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#include "pg_upgrade.h"

#include "query_for_user_defined_indexes.h"

static const char * const query_count_of_all_user_defined_indexes = "" \
	"SELECT count(1) as number_of_user_defined_indexes " \
	"    FROM pg_index ind " \
	"    LEFT JOIN pg_class rel ON ind.indexrelid = rel.oid " \
	"    AND relkind IN ('i', '')" \
	"    LEFT JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid " \
	"    WHERE nsp.nspname NOT IN (" \
	"        'pg_catalog', 'pg_toast', 'pg_aoseg', 'information_schema'" \
	"    )" \
	"    AND nsp.nspname !~ '^pg_toast';";

static const char * const query_all_user_defined_indexes = "" \
	"SELECT relname as index_name, nspname as namespace_name, current_database() as database_name " \
	"    FROM pg_index ind " \
	"    LEFT JOIN pg_class rel ON ind.indexrelid = rel.oid " \
	"    AND relkind IN ('i', '')" \
	"    LEFT JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid " \
	"    WHERE nsp.nspname NOT IN (" \
	"        'pg_catalog', 'pg_toast', 'pg_aoseg', 'information_schema'" \
	"    )" \
	"    AND nsp.nspname !~ '^pg_toast';";

static int 
getNumberOfUserDefinedIndexesFor(ClusterInfo *cluster, int databaseNumber)
{
	DbInfo   *active_db;
	PGresult *result;
	PGconn   *connection;

	active_db = &cluster->dbarr.dbs[databaseNumber];
	connection = connectToServer(cluster, active_db->db_name);
	result = executeQueryOrDie(connection, query_count_of_all_user_defined_indexes);

	char *count_as_string = strdup(PQgetvalue(result, 0, PQfnumber(result, "number_of_user_defined_indexes")));

	int number_of_user_defined_indexes = atoi(count_as_string);

	PQfinish(connection);

	return number_of_user_defined_indexes;
}

static UserDefinedIndex **
getUserDefinedIndexesFor(ClusterInfo *cluster, int databaseNumber) 
{
	DbInfo   *active_db;
	PGresult *result;
	PGconn   *connection;

	active_db = &cluster->dbarr.dbs[databaseNumber];
	connection = connectToServer(cluster, active_db->db_name);
	result = executeQueryOrDie(connection, query_all_user_defined_indexes);
	
	int numberOfTuples = PQntuples(result);
	UserDefinedIndex **userDefinedIndexes = (UserDefinedIndex**) palloc0(sizeof(UserDefinedIndex*) * (numberOfTuples + 1));

	for (int i = 0; i < numberOfTuples; i++)
	{
		UserDefinedIndex *userDefinedIndex = palloc0(sizeof(UserDefinedIndex));
		userDefinedIndex->index_name = strdup(PQgetvalue(result, i, PQfnumber(result, "index_name")));
		userDefinedIndex->database_name = strdup(PQgetvalue(result, i, PQfnumber(result, "database_name")));
		userDefinedIndex->namespace_name = strdup(PQgetvalue(result, i, PQfnumber(result, "namespace_name")));
		userDefinedIndexes[i] = userDefinedIndex;
	}

	userDefinedIndexes[numberOfTuples] = NULL;

	PQfinish(connection);

	return userDefinedIndexes;
}

UserDefinedIndexes * 
query_for_user_defined_indexes(ClusterInfo *cluster)
{
	int nextPosition = 0;

	UserDefinedIndexes *indexes = palloc0(sizeof(UserDefinedIndexes));
	indexes->number_of_user_defined_indexes = 0;

	for (int dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
		indexes->number_of_user_defined_indexes += getNumberOfUserDefinedIndexesFor(cluster, dbnum);

	indexes->foundIndexes = palloc0(sizeof(UserDefinedIndex*) * indexes->number_of_user_defined_indexes);

	for (int dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		UserDefinedIndex **userDefinedIndexes = getUserDefinedIndexesFor(cluster, dbnum);

		for (int i = 0; userDefinedIndexes[i] != NULL; i++)
			indexes->foundIndexes[nextPosition++] = userDefinedIndexes[i];
	}

	return indexes;
}
