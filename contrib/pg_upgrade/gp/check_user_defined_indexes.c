/*
 *
 *  check_user_defined_indexes.c
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#include "checks.h"

#include "query_for_user_defined_indexes.h"

static void
report_too_many_indexes(UserDefinedIndexes *indexes)
{
	report_info("total number of found user defined indexes: %d\n",
		indexes->number_of_user_defined_indexes);

	for (int i = 0; i < indexes->number_of_user_defined_indexes; i++)
	{
		UserDefinedIndex *userDefinedIndex = indexes->foundIndexes[i];

		report_info("%s.%s.%s\n",
			userDefinedIndex->database_name,
			userDefinedIndex->namespace_name,
			userDefinedIndex->index_name);
	}
}

bool
check_user_defined_indexes(ClusterInfo *oldCluster)
{
	UserDefinedIndexes *indexes = query_for_user_defined_indexes(oldCluster);
	bool passesCheck = indexes->number_of_user_defined_indexes == 0;

	if (!passesCheck)
		report_too_many_indexes(indexes);

	return passesCheck;
}
