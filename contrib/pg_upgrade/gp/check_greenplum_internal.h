#ifndef GPDB_CHECK_GREENPLUM_INTERNAL_H
#define GPDB_CHECK_GREENPLUM_INTERNAL_H


#include "c.h"
#include "pg_upgrade.h"

typedef bool (*check_function)(ClusterInfo *cluster);

extern void perform_greenplum_checks(check_function gp_checks_list[], int gp_checks_list_length, ClusterInfo *cluster);

#endif // GPDB_CHECK_GREENPLUM_INTERNAL_H