/*-------------------------------------------------------------------------
 *
 * info_gp.h
 *
 * Greenplum specific logic for determining tablespace paths
 * for a given tablespace_oid.
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 */


#ifndef PG_UPGRADE_INFO_GP_H
#define PG_UPGRADE_INFO_GP_H

#include "pg_upgrade_greenplum.h"
#include "old_tablespace_file_gp.h"

char *determine_db_tablespace_path(ClusterInfo *currentCluster,
	char *spclocation,
	Oid tablespace_oid);

#endif /* PG_UPGRADE_INFO_GP_H */