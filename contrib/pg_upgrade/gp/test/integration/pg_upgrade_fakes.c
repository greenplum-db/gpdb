#include "pg_upgrade.h"

/*
 * Variables and functions provided by pg_upgrade.c needed for running integration tests
 *
 * Note: we cannot include pg_upgrade.o in our test because it has a main function.
 *
 */
ClusterInfo old_cluster;
ClusterInfo new_cluster;
OSInfo os_info;
UserOpts user_opts;


bool
exec_prog(const char *log_file, const char *opt_log_file,
          bool throw_error, const char *fmt,...){
	return true;
}
