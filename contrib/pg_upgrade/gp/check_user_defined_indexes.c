/*
 *
 *  check_user_defined_indexes.c
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#include "checks.h"

#include "query_for_user_defined_indexes.h"

bool
check_user_defined_indexes(ClusterInfo *oldCluster)
{
	struct UserDefinedIndexes indexes =
		                          query_for_user_defined_indexes(oldCluster);
	return indexes.number_of_user_defined_indexes == 0;
}
