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
#include "pg_upgrade_greenplum.h"
#include "old_tablespace_file_gp.h"
#include "tablespace_gp_internal.h"
#include "greenplum_cluster_info_internal.h"
#include "old_tablespace_file_gp_internal.h"

#define OLD_TABLESPACES_FILE    "old_tablespaces.txt"

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

static char *const OLD_TABLESPACE_QUERY = "copy ( "
                                          "    select fsedbid, "
                                          "    upgrade_tablespace.oid as tablespace_oid, "
                                          "    spcname, "
                                          "    case when is_user_defined_tablespace then location_with_oid else fselocation end, "
                                          "    (is_user_defined_tablespace::int) as is_user_defined_tablespace "
                                          "    from ( "
                                          "        select pg_tablespace.oid, *, "
                                          "        (fselocation || '/' || pg_tablespace.oid) as location_with_oid, "
                                          "        (spcname not in ('pg_default', 'pg_global'))  as is_user_defined_tablespace "
                                          "        from pg_tablespace "
                                          "        inner join pg_filespace_entry on fsefsoid = spcfsoid "
                                          "    ) upgrade_tablespace "
                                          ") to '%s' WITH CSV;";

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

	set_old_tablespace_file_contents(
		filter_old_tablespace_file_for_dbid(
			contents,
			get_gp_dbid(oldCluster->greenplum_cluster_info)));

	clear_old_tablespace_file_contents(contents);
}

void
populate_gpdb6_cluster_tablespace_suffix(ClusterInfo *cluster)
{
	cluster->tablespace_suffix = psprintf("/%d/GPDB_6_%d",
	                                      get_gp_dbid(cluster->greenplum_cluster_info),
	                                      cluster->controldata.cat_ver);
}

bool
is_gpdb_version_with_filespaces(ClusterInfo *cluster)
{
	return GET_MAJOR_VERSION(cluster->major_version) < 904;
}

void
populate_os_info_with_file_contents(void)
{
	OldTablespaceFileContents *contents = get_old_tablespace_file_contents();
	os_info.num_old_tablespaces = OldTablespaceFileContents_TotalNumberOfTablespaces(
		contents);
	os_info.old_tablespaces = OldTablespaceFileContents_GetArrayOfTablespacePaths(
		contents);
}
