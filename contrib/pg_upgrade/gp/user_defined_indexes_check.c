#include "checks.h"


bool
user_defined_indexes_check(ClusterInfo *cluster)
{
	struct UserDefinedIndexes indexes = cluster->query_for_indexes(cluster);
	return indexes.number_of_user_defined_indexes == 0;
}
