#ifndef GPDB_CHECK_GREENPLUM_INTERNAL_H
#define GPDB_CHECK_GREENPLUM_INTERNAL_H


#include "c.h"
#include "queries.h"
#include "pg_upgrade.h"

typedef bool (*check_function)(ClusterInfo *cluster, Queries *queries);

extern void perform_greenplum_checks(
	check_function gp_checks_list[], 
	int gp_checks_list_length, 
	ClusterInfo *cluster,
	Queries *queries);

#endif // GPDB_CHECK_GREENPLUM_INTERNAL_H