/*-------------------------------------------------------------------------
 *
 * tablespace_gp.c
 *
 * Greenplum specific functions for preparing pg_upgrade to perform an upgrade
 * of Greenplum's tablespaces.
 * 
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */
#include "pg_upgrade.h"

static char *
get_generated_old_tablespaces_file_path(void)
{
	char current_working_directory[MAXPGPATH];

	char *result = getcwd(current_working_directory,
	                      sizeof(current_working_directory));

	if (result == NULL)
		pg_fatal("could not determine current working directory");

	return psprintf("%s/%s", current_working_directory, OLD_TABLESPACES_FILE);
}

static char *const OLD_TABLESPACE_QUERY = ""
"copy ("
"    select fsedbid, pg_tablespace.oid as tablespace_oid, spcname, fselocation, (spcname not in ('pg_default', 'pg_global')::int) as is_user_defined_tablespace"
"    from pg_filespace_entry "
"    inner join pg_tablespace on fsefsoid = spcfsoid "
") to '%s' WITH CSV";

static void
dump_old_tablespaces(ClusterInfo *oldCluster,
                     char *generated_old_tablespaces_file_path)
{
	if (!is_gpdb_version_with_filespaces(oldCluster))
		return;

	prep_status("Creating a dump of all tablespace metadata.");

	PGconn *connection = connectToServer(oldCluster, "template1");

	PGresult *result = executeQueryOrDie(connection,
	                                     OLD_TABLESPACE_QUERY,
	                                     generated_old_tablespaces_file_path);
	PQclear(result);

	PQfinish(connection);

	check_ok();
}

void
generate_old_tablespaces_file(ClusterInfo *oldCluster)
{
	char *generated_old_tablespaces_file_path =
		     get_generated_old_tablespaces_file_path();
	dump_old_tablespaces(oldCluster, generated_old_tablespaces_file_path);
	populate_old_cluster_with_old_tablespaces(oldCluster,
	                                          generated_old_tablespaces_file_path);
	pfree(generated_old_tablespaces_file_path);
}

void
populate_old_cluster_with_old_tablespaces(ClusterInfo *oldCluster,
                                          const char *const file_path)
{
	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents(
		file_path);

	oldCluster->old_tablespace_file_contents =
		filter_old_tablespace_file_for_dbid(
			contents,
			oldCluster->gp_dbid);

	clear_old_tablespace_file_contents(contents);
}

void
populate_gpdb6_cluster_tablespace_suffix(ClusterInfo *cluster)
{
	cluster->tablespace_suffix = psprintf("/%d/GPDB_6_%d",
	                                      cluster->gp_dbid,
	                                      cluster->controldata.cat_ver);
}

bool
is_gpdb_version_with_filespaces(ClusterInfo *cluster)
{
	return GET_MAJOR_VERSION(cluster->major_version) < 904;
}
