#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "../pg_upgrade.h"

PGconn *
connectToServer(ClusterInfo *cluster, const char *db_name)
{
	check_expected(cluster);
	check_expected(db_name);
	return (PGconn *)mock();
}

PGresult *
executeQueryOrDie(PGconn *conn, const char *fmt,...)
{
	check_expected(conn);
	check_expected(fmt);
	return (PGresult *)mock();
}

char *
cluster_conn_opts(ClusterInfo *cluster)
{
	check_expected(cluster);
	return (char *)mock();
}

bool
start_postmaster(ClusterInfo *cluster, bool throw_error)
{
	check_expected(cluster);
	check_expected(throw_error);
	return (bool)mock();
}

uint32
get_major_server_version(ClusterInfo *cluster)
{
	check_expected(cluster);
	return (uint32)mock();
}

void
stop_postmaster(bool fast)
{
	check_expected(fast);
	mock();
}
