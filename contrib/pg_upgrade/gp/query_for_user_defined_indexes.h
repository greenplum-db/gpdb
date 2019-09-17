/*
 *
 *  query_for_user_defined_indexes.h
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#ifndef GP_PG_UPGRADE_GP_QUERY_FOR_INDEXES
#define GP_PG_UPGRADE_GP_QUERY_FOR_INDEXES

#include "pg_upgrade.h"

typedef struct UserDefinedIndexData {
	char *namespace_name;
	char *index_name;
	char *database_name;
} UserDefinedIndex;

typedef struct UserDefinedIndexesData {
	int number_of_user_defined_indexes;
	UserDefinedIndex **foundIndexes;
} UserDefinedIndexes;

UserDefinedIndexes *query_for_user_defined_indexes(ClusterInfo *cluster);
void cleanup_query_for_user_defined_indexes(UserDefinedIndexes *indexes);

#endif /* GP_PG_UPGRADE_GP_QUERY_FOR_INDEXES */
