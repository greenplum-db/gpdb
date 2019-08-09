#include "pg_upgrade.h"
#include "libpq-fe.h"


extern ClusterInfo old_cluster;
extern ClusterInfo new_cluster;
extern OSInfo os_info;
extern UserOpts user_opts;

/*
 * Execute a query in normal mode
 */
extern void
executeQuery(PGconn *conn, const char *fmt,...);

