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

struct UserDefinedIndexes {
	int number_of_user_defined_indexes;
};

struct UserDefinedIndexes query_for_user_defined_indexes(ClusterInfo *cluster);

#endif /* GP_PG_UPGRADE_GP_QUERY_FOR_INDEXES */