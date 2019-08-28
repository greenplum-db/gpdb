#ifndef PG_UPGRADE_GP_QUERIES
#define PG_UPGRADE_GP_QUERIES


#include "pg_upgrade.h"


struct UserDefinedIndexes {
	int number_of_user_defined_indexes;
};

typedef struct QueryFunctions {
	struct UserDefinedIndexes (*query_for_indexes)(ClusterInfo *);
} Queries;

extern void init_queries_for_greenplum_checks(Queries *queries);

#endif // PG_UPGRADE_GP_QUERIES