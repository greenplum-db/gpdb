#ifndef GPDB_TEST_UTILS_H
#define GPDB_TEST_UTILS_H

/*
 * Utility functions to conduct integration tests
 */

#include "pg_upgrade.h"
#include "libpq-fe.h"

extern void
executeQuery(PGconn *conn, const char *fmt,...);

extern ClusterInfo *
make_cluster();

extern PGconn *
getTestConnection(ClusterInfo *clusterInfo);

extern PGconn *
getTestConnectionToDatabase(ClusterInfo *clusterInfo, const char *);

extern void
setup_cluster(ClusterInfo *cluster);

extern char *
get_database_name(void);

extern void setup_os_info(void);

extern void
enable_utility_mode(ClusterInfo *clusterInfo);

extern void
disable_utility_mode(ClusterInfo *clusterInfo);

#endif //GPDB_TEST_UTILS_H
