/*
 * contrib/pg_upgrade/gp/check_greenplum.c
 *
 * Definition of an interface function to conduct Greenplum-specific pg_upgrade
 * checks
 */



#include "checks.h"
#include "check_greenplum_internal.h"
#include "check_greenplum.h"
#include "queries.h"


void check_greenplum(void)
{
	check_function list[] = {
		check_online_expansion,
		check_external_partition,
		check_covering_aoindex,
		check_partition_indexes,
		check_orphaned_toastrels,
		check_gphdfs_external_tables,
		check_gphdfs_user_roles,
		check_user_defined_indexes
	};

	size_t length = sizeof(list) / sizeof(list[0]);

	ClusterInfo *cluster = &old_cluster;

	struct QueryFunctions queries;

	init_queries_for_greenplum_checks(&queries);

	perform_greenplum_checks(list, length, cluster, &queries);
}

