/*
 *
 *  checks.h
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */

#ifndef GPDB_CHECKS_H
#define GPDB_CHECKS_H

#include "c.h"
#include "pg_upgrade.h"

/*
 * contrib/pg_upgrade/gp/checks.h
 *
 * Declarations of Greenplum-specific check functions
 */

extern bool check_external_partition(ClusterInfo *cluster);
extern bool check_covering_aoindex(ClusterInfo *cluster);
extern bool check_partition_indexes(ClusterInfo *cluster);
extern bool check_orphaned_toastrels(ClusterInfo *cluster);
extern bool check_online_expansion(ClusterInfo *cluster);
extern bool check_gphdfs_external_tables(ClusterInfo *cluster);
extern bool check_gphdfs_user_roles(ClusterInfo *cluster);
extern bool check_user_defined_indexes(ClusterInfo *cluster);

#endif /* GPDB_CHECKS_H */
