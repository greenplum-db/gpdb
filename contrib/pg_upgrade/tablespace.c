/*
 *	tablespace.c
 *
 *	tablespace functions
 *
 *	Copyright (c) 2010-2014, PostgreSQL Global Development Group
 *	contrib/pg_upgrade/tablespace.c
 */

#include "postgres_fe.h"

#include "pg_upgrade.h"
#include "greenplum/old_tablespace_file_gp.h"
#include "greenplum/pg_upgrade_greenplum.h"

#include <sys/types.h>

static void get_tablespace_paths(void);
static void set_tablespace_directory_suffix(ClusterInfo *cluster);

void
init_tablespaces(void)
{
	get_tablespace_paths();

	set_tablespace_directory_suffix(&old_cluster);
	if(!is_skip_target_check())
	{
		set_tablespace_directory_suffix(&new_cluster);

		if (os_info.num_old_tablespaces > 0 &&
		strcmp(old_cluster.tablespace_suffix, new_cluster.tablespace_suffix) == 0)
			pg_fatal("Cannot upgrade to/from the same system catalog version when\n"
					 "using tablespaces.\n");
	}
}

static void
verify_old_tablespace_paths(void)
{
	for (int tblnum = 0; tblnum < os_info.num_old_tablespaces; tblnum++)
	{
		struct stat statBuf;

		/*
		 * Check that the tablespace path exists and is a directory.
		 * Effectively, this is checking only for tables/indexes in
		 * non-existent tablespace directories.  Databases located in
		 * non-existent tablespaces already throw a backend error.
		 * Non-existent tablespace directories can occur when a data directory
		 * that contains user tablespaces is moved as part of pg_upgrade
		 * preparation and the symbolic links are not updated.
		 */
		if (stat(os_info.old_tablespaces[tblnum], &statBuf) != 0)
		{
			if (errno == ENOENT)
				report_status(PG_FATAL,
				              "tablespace directory \"%s\" does not exist\n",
				              os_info.old_tablespaces[tblnum]);
			else
				report_status(PG_FATAL,
				              "cannot stat() tablespace directory \"%s\": %s\n",
				              os_info.old_tablespaces[tblnum], getErrorText());
		}
		if (!S_ISDIR(statBuf.st_mode))
			report_status(PG_FATAL,
			              "tablespace path \"%s\" is not a directory\n",
			              os_info.old_tablespaces[tblnum]);
	}
}

/*
 * get_tablespace_paths()
 *
 * Scans pg_tablespace and returns a malloc'ed array of all tablespace
 * paths. It's the caller's responsibility to free the array.
 */
static void
get_tablespace_paths(void)
{
	if (old_tablespace_file_contents_exists()) {
		populate_os_info_with_file_contents();
		verify_old_tablespace_paths();
		return;
	}

	PGconn	   *conn = connectToServer(&old_cluster, "template1");
	PGresult   *res;
	int			tblnum;
	int			i_spclocation;
	char		query[QUERY_ALLOC];

	snprintf(query, sizeof(query),
			 "SELECT	%s "
			 "FROM	pg_catalog.pg_tablespace "
			 "WHERE	spcname != 'pg_default' AND "
			 "		spcname != 'pg_global'",
	/*
	 * 9.2 removed the spclocation column in upstream postgres, in GPDB it was
	 * removed in 6.0.0 during the 8.4 merge
	 */
		(GET_MAJOR_VERSION(old_cluster.major_version) == 803) ?
			 "spclocation" : "pg_catalog.pg_tablespace_location(oid) AS spclocation");

	res = executeQueryOrDie(conn, "%s", query);

	if ((os_info.num_old_tablespaces = PQntuples(res)) != 0)
		os_info.old_tablespaces = (char **) pg_malloc(
							   os_info.num_old_tablespaces * sizeof(char *));
	else
		os_info.old_tablespaces = NULL;

	i_spclocation = PQfnumber(res, "spclocation");

	for (tblnum = 0; tblnum < os_info.num_old_tablespaces; tblnum++)
		os_info.old_tablespaces[tblnum] = pg_strdup(
									 PQgetvalue(res, tblnum, i_spclocation));

	/*
	 * gpdb: verification logic has been extracted from the above loop for reuse
	 */
	verify_old_tablespace_paths();

	PQclear(res);

	PQfinish(conn);

	return;
}

static void
set_tablespace_directory_suffix(ClusterInfo *cluster)
{
	/*
	 * GPDB 6 introduced a new layout for tablespaces
	 */
	if (is_gpdb6(cluster)) {
		populate_gpdb6_cluster_tablespace_suffix(cluster);
		return;
	}

	if (GET_MAJOR_VERSION(cluster->major_version) <= 804)
		cluster->tablespace_suffix = pg_strdup("");
	else
	{
		/* This cluster has a version-specific subdirectory */

		/* The leading slash is needed to start a new directory. */
		cluster->tablespace_suffix = psprintf("/PG_%s_%d",
											  cluster->major_version_str,
											  cluster->controldata.cat_ver);
	}
}
