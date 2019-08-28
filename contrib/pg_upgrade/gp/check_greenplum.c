/*
 * contrib/pg_upgrade/gp/check_greenplum.c
 *
 * Definition of an interface function to conduct Greenplum-specific pg_upgrade
 * checks
 */



#include "checks.h"
#include "check_greenplum_internal.h"
#include "check_greenplum.h"
#include "cluster.h"


void check_greenplum(void)
{
	check_function list[] = {
		check_online_expansion,
		check_external_partition,
		check_covering_aoindex,
		check_partition_indexes,
		check_orphaned_toastrels,
		check_gphdfs_external_tables,
		check_gphdfs_user_roles
	};

	size_t length = sizeof(list) / sizeof(list[0]);

	ClusterInfo *cluster = &old_cluster;

	init_cluster_for_greenplum_checks(cluster);

	perform_greenplum_checks(list, length, cluster);
}

