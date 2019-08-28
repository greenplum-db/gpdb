#include "checks.h"


bool
check_user_defined_indexes(ClusterInfo *cluster, Queries *queries)
{
	struct UserDefinedIndexes indexes = queries->query_for_indexes(cluster);
	return indexes.number_of_user_defined_indexes == 0;
}
