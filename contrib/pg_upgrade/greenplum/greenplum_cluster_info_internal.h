#ifndef GREENPLUM_GREENPLUM_CLUSTER_INFO_INTERNAL_H
#define GREENPLUM_GREENPLUM_CLUSTER_INFO_INTERNAL_H
/*
 *	greenplum/greenplum_cluster_info_internal.h
 *
 *	Portions Copyright (c) 2019-Present, Pivotal Software Inc
 *	contrib/pg_upgrade/greenplum/greenplum_cluster_info_internal.h
 */

#include "postgres_fe.h"
#include "greenplum_cluster_info.h"

GreenplumClusterInfo *make_cluster_info(void);

int get_gp_dbid(GreenplumClusterInfo *info);

void set_gp_dbid(GreenplumClusterInfo *info, int gp_dbid);

bool is_gp_dbid_set(GreenplumClusterInfo *info);

#endif
