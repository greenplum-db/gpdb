/*
 *	check.c
 *
 *	server checks and output routines
 *
 *	Copyright (c) 2010-2011, PostgreSQL Global Development Group
 *	contrib/pg_upgrade/check.c
 */

#include "pg_upgrade.h"


static void set_locale_and_encoding(ClusterInfo *cluster);
static void check_new_cluster_is_empty(void);
static void check_old_cluster_has_new_cluster_dbs(void);
static void check_locale_and_encoding(ControlData *oldctrl,
						  ControlData *newctrl);
static void check_is_super_user(ClusterInfo *cluster);
static void check_proper_datallowconn(ClusterInfo *cluster);
static void check_for_isn_and_int8_passing_mismatch(ClusterInfo *cluster);
static void check_for_reg_data_type_usage(ClusterInfo *cluster);

static void check_external_partition(void);
static void check_covering_aoindex(void);
static void check_hash_partition_usage(void);
static void check_partition_indexes(void);


/*
 * fix_path_separator
 * For non-Windows, just return the argument.
 * For Windows convert any forward slash to a backslash
 * such as is suitable for arguments to builtin commands 
 * like RMDIR and DEL.
 */
static char *
fix_path_separator(char *path)
{
#ifdef WIN32

	char *result;
	char *c;

	result = pg_strdup(path);

	for (c = result; *c != '\0'; c++)
		if (*c == '/')
			*c = '\\';

	return result;

#else

	return path;

#endif
}

void
output_check_banner(bool *live_check)
{
	if (user_opts.check && is_server_running(old_cluster.pgdata))
	{
		*live_check = true;
		if (old_cluster.port == new_cluster.port)
			pg_log(PG_FATAL, "When checking a live server, "
				   "the old and new port numbers must be different.\n");
		pg_log(PG_REPORT, "Performing Consistency Checks on Old Live Server\n");
		pg_log(PG_REPORT, "------------------------------------------------\n");
	}
	else
	{
		pg_log(PG_REPORT, "Performing Consistency Checks\n");
		pg_log(PG_REPORT, "-----------------------------\n");
	}
}


void
check_old_cluster(bool live_check,
				  char **sequence_script_file_name)
{
	/* -- OLD -- */

	if (!live_check)
		start_postmaster(&old_cluster);

	set_locale_and_encoding(&old_cluster);

	get_pg_database_relfilenode(&old_cluster);

	/* Extract a list of databases and tables from the old cluster */
	get_db_and_rel_infos(&old_cluster);

	init_tablespaces();

	get_loadable_libraries();


	/*
	 * Check for various failure cases
	 */
	report_progress(&old_cluster, CHECK, "Failure checks");

	check_is_super_user(&old_cluster);
	check_proper_datallowconn(&old_cluster);
	check_for_reg_data_type_usage(&old_cluster);
	check_for_isn_and_int8_passing_mismatch(&old_cluster);

	check_external_partition();
	check_covering_aoindex();
	check_hash_partition_usage();
	check_partition_indexes();

	/* old = PG 8.3 checks? */
	/*
	 * All of these checks have been disabled in GPDB, since we're using
	 * this to upgrade only to 8.3. Needs to be removed when we merge
	 * with PostgreSQL 8.4.
	 *
	 * GPDB_91_MERGE_FIXME: that comment doesn't make sense to me; what checks
	 * have been disabled?
	 *
	 * The change to name datatype's alignment was backported to GPDB 5.0,
	 * so we need to check even when upgradeing to GPDB 5.0.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) <= 802)
	{
		old_8_3_check_for_name_data_type_usage(&old_cluster);

		old_GPDB4_check_for_money_data_type_usage(&old_cluster);
		old_GPDB4_check_no_free_aoseg(&old_cluster);
	}

	if (GET_MAJOR_VERSION(old_cluster.major_version) <= 803 &&
		GET_MAJOR_VERSION(new_cluster.major_version) >= 804)
	{
		old_8_3_check_for_name_data_type_usage(&old_cluster);
		old_8_3_check_for_tsquery_usage(&old_cluster);
		old_8_3_check_ltree_usage(&old_cluster);
		if (user_opts.check)
		{
			old_8_3_rebuild_tsvector_tables(&old_cluster, true);
			old_8_3_invalidate_hash_gin_indexes(&old_cluster, true);
			old_8_3_invalidate_bpchar_pattern_ops_indexes(&old_cluster, true);
		}
		else

			/*
			 * While we have the old server running, create the script to
			 * properly restore its sequence values but we report this at the
			 * end.
			 */
			*sequence_script_file_name =
				old_8_3_create_sequence_script(&old_cluster);
	}

#ifdef GPDB_90_MERGE_FIXME
	/* Pre-PG 9.0 had no large object permissions */
	if (GET_MAJOR_VERSION(old_cluster.major_version) <= 804)
		new_9_0_populate_pg_largeobject_metadata(&old_cluster, true);
#endif

	/*
	 * While not a check option, we do this now because this is the only time
	 * the old server is running.
	 */
	if (!user_opts.check)
	{
		if (user_opts.dispatcher_mode)
			get_old_oids();

		report_progress(&old_cluster, SCHEMA_DUMP, "Creating catalog dump");
		generate_old_dump();
		split_old_dump();
	}

	if (!live_check)
		stop_postmaster(false);
}


void
check_new_cluster(void)
{
	set_locale_and_encoding(&new_cluster);

	get_db_and_rel_infos(&new_cluster);

	check_new_cluster_is_empty();
	check_old_cluster_has_new_cluster_dbs();

	check_loadable_libraries();

	check_locale_and_encoding(&old_cluster.controldata, &new_cluster.controldata);

	if (user_opts.transfer_mode == TRANSFER_MODE_LINK)
		check_hard_link();
}


void
report_clusters_compatible(void)
{
	if (user_opts.check)
	{
		pg_log(PG_REPORT, "\n*Clusters are compatible*\n");
		/* stops new cluster */
		stop_postmaster(false);
		exit(0);
	}

	pg_log(PG_REPORT, "\n"
		   "| If pg_upgrade fails after this point, you must\n"
		   "| re-initdb the new cluster before continuing.\n"
		   "| You will also need to remove the \".old\" suffix\n"
		   "| from %s/global/pg_control.old.\n", old_cluster.pgdata);
}


void
issue_warnings(char *sequence_script_file_name)
{
	start_postmaster(&new_cluster);

	/* old == GPDB4 warnings */
	if (GET_MAJOR_VERSION(old_cluster.major_version) <= 802)
	{
		new_gpdb5_0_invalidate_indexes(false);
	}

	/* old = PG 8.3 warnings? */
	if (GET_MAJOR_VERSION(old_cluster.major_version) <= 803 &&
		GET_MAJOR_VERSION(new_cluster.major_version) >= 804)
	{
		/* restore proper sequence values using file created from old server */
		if (sequence_script_file_name)
		{
			prep_status("Adjusting sequences");
			exec_prog(true,
					  SYSTEMQUOTE "\"%s/psql\" --set ON_ERROR_STOP=on "
					  "--no-psqlrc --port %d --username \"%s\" "
					  "-f \"%s\" --dbname template1 >> \"%s\"" SYSTEMQUOTE,
					  new_cluster.bindir, new_cluster.port, os_info.user,
					  sequence_script_file_name, log_opts.filename);
			unlink(sequence_script_file_name);
			check_ok();
		}

		old_8_3_rebuild_tsvector_tables(&new_cluster, false);
		old_8_3_invalidate_hash_gin_indexes(&new_cluster, false);
		old_8_3_invalidate_bpchar_pattern_ops_indexes(&new_cluster, false);
	}

#ifdef GPDB_90_MERGE_FIXME
	/* Create dummy large object permissions for old < PG 9.0? */
	if (GET_MAJOR_VERSION(old_cluster.major_version) <= 804)
	{
		new_9_0_populate_pg_largeobject_metadata(&new_cluster, false);
	}
#endif

	stop_postmaster(false);
}


void
output_completion_banner(char *deletion_script_file_name)
{
	/* Did we copy the free space files? */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 804)
		pg_log(PG_REPORT,
			   "| Optimizer statistics are not transferred by pg_upgrade\n"
			   "| so consider running:\n"
			   "| \tvacuumdb --all --analyze-only\n"
			   "| on the newly-upgraded cluster.\n\n");
	else
		pg_log(PG_REPORT,
			   "| Optimizer statistics and free space information\n"
			   "| are not transferred by pg_upgrade so consider\n"
			   "| running:\n"
			   "| \tvacuumdb --all --analyze\n"
			   "| on the newly-upgraded cluster.\n\n");

	pg_log(PG_REPORT,
		   "| Running this script will delete the old cluster's data files:\n"
		   "| \t%s\n",
		   deletion_script_file_name);
}


void
check_cluster_versions(void)
{
	/* get old and new cluster versions */
	old_cluster.major_version = get_major_server_version(&old_cluster);
	new_cluster.major_version = get_major_server_version(&new_cluster);

	/*
	 * We allow upgrades from/to the same major version for alpha/beta
	 * upgrades
	 */

	if (GET_MAJOR_VERSION(old_cluster.major_version) < 802)
		pg_log(PG_FATAL, "This utility can only upgrade from Greenplum version 4.3.XX and later.\n");

	/* Only current PG version is supported as a target */
	if (GET_MAJOR_VERSION(new_cluster.major_version) != GET_MAJOR_VERSION(PG_VERSION_NUM))
		pg_log(PG_FATAL, "This utility can only upgrade to PostgreSQL version %s.\n",
			   PG_MAJORVERSION);

	/*
	 * We can't allow downgrading because we use the target pg_dumpall, and
	 * pg_dumpall cannot operate on new datbase versions, only older versions.
	 */
	if (old_cluster.major_version > new_cluster.major_version)
		pg_log(PG_FATAL, "This utility cannot be used to downgrade to older major PostgreSQL versions.\n");
}


void
check_cluster_compatibility(bool live_check)
{
	char		libfile[MAXPGPATH];
	FILE	   *lib_test;

	/*
	 * Test pg_upgrade_support.so is in the proper place.    We cannot copy it
	 * ourselves because install directories are typically root-owned.
	 */
	snprintf(libfile, sizeof(libfile), "%s/pg_upgrade_support%s", new_cluster.libpath,
			 DLSUFFIX);

	if ((lib_test = fopen(libfile, "r")) == NULL)
		pg_log(PG_FATAL,
			   "pg_upgrade_support%s must be created and installed in %s\n", DLSUFFIX, libfile);
	else
		fclose(lib_test);

	/* get/check pg_control data of servers */
	get_control_data(&old_cluster, live_check);
	get_control_data(&new_cluster, false);
	check_control_data(&old_cluster.controldata, &new_cluster.controldata);

	/* Is it 9.0 but without tablespace directories? */
	if (GET_MAJOR_VERSION(new_cluster.major_version) == 900 &&
		new_cluster.controldata.cat_ver < TABLE_SPACE_SUBDIRS_CAT_VER)
		pg_log(PG_FATAL, "This utility can only upgrade to PostgreSQL version 9.0 after 2010-01-11\n"
			   "because of backend API changes made during development.\n");
}


/*
 * set_locale_and_encoding()
 *
 * query the database to get the template0 locale
 */
static void
set_locale_and_encoding(ClusterInfo *cluster)
{
	ControlData *ctrl = &cluster->controldata;
	PGconn	   *conn;
	PGresult   *res;
	int			i_encoding;
	int			cluster_version = cluster->major_version;

	conn = connectToServer(cluster, "template1");

	/* for pg < 80400, we got the values from pg_controldata */
	if (cluster_version >= 80400)
	{
		int			i_datcollate;
		int			i_datctype;

		res = executeQueryOrDie(conn,
								"SELECT datcollate, datctype "
								"FROM 	pg_catalog.pg_database "
								"WHERE	datname = 'template0' ");
		assert(PQntuples(res) == 1);

		i_datcollate = PQfnumber(res, "datcollate");
		i_datctype = PQfnumber(res, "datctype");

		ctrl->lc_collate = pg_strdup(PQgetvalue(res, 0, i_datcollate));
		ctrl->lc_ctype = pg_strdup(PQgetvalue(res, 0, i_datctype));

		PQclear(res);
	}

	res = executeQueryOrDie(conn,
							"SELECT pg_catalog.pg_encoding_to_char(encoding) "
							"FROM 	pg_catalog.pg_database "
							"WHERE	datname = 'template0' ");
	assert(PQntuples(res) == 1);

	i_encoding = PQfnumber(res, "pg_encoding_to_char");
	ctrl->encoding = pg_strdup(PQgetvalue(res, 0, i_encoding));

	PQclear(res);

	PQfinish(conn);
}


/*
 * check_locale_and_encoding()
 *
 *	locale is not in pg_controldata in 8.4 and later so
 *	we probably had to get via a database query.
 */
static void
check_locale_and_encoding(ControlData *oldctrl,
						  ControlData *newctrl)
{
	/* These are often defined with inconsistent case, so use pg_strcasecmp(). */
	if (pg_strcasecmp(oldctrl->lc_collate, newctrl->lc_collate) != 0)
		pg_log(PG_FATAL,
			   "old and new cluster lc_collate values do not match\n");
	if (pg_strcasecmp(oldctrl->lc_ctype, newctrl->lc_ctype) != 0)
		pg_log(PG_FATAL,
			   "old and new cluster lc_ctype values do not match\n");
	if (pg_strcasecmp(oldctrl->encoding, newctrl->encoding) != 0)
		pg_log(PG_FATAL,
			   "old and new cluster encoding values do not match\n");
}


static void
check_new_cluster_is_empty(void)
{
	int			dbnum;

	for (dbnum = 0; dbnum < new_cluster.dbarr.ndbs; dbnum++)
	{
		int			relnum;
		RelInfoArr *rel_arr = &new_cluster.dbarr.dbs[dbnum].rel_arr;

		for (relnum = 0; relnum < rel_arr->nrels;
			 relnum++)
		{
			/* pg_largeobject and its index should be skipped */
			if (strcmp(rel_arr->rels[relnum].nspname, "pg_catalog") != 0)
				pg_log(PG_FATAL, "New cluster database \"%s\" is not empty\n",
					   new_cluster.dbarr.dbs[dbnum].db_name);
		}
	}

}


/*
 *	If someone removes the 'postgres' database from the old cluster and
 *	the new cluster has a 'postgres' database, the number of databases
 *	will not match.  We actually could upgrade such a setup, but it would
 *	violate the 1-to-1 mapping of database counts, so we throw an error
 *	instead.  We would detect this as a database count mismatch during
 *	upgrade, but we want to detect it during the check phase and report
 *	the database name.
 */
static void
check_old_cluster_has_new_cluster_dbs(void)
{
	int			old_dbnum,
				new_dbnum;

	for (new_dbnum = 0; new_dbnum < new_cluster.dbarr.ndbs; new_dbnum++)
	{
		for (old_dbnum = 0; old_dbnum < old_cluster.dbarr.ndbs; old_dbnum++)
			if (strcmp(old_cluster.dbarr.dbs[old_dbnum].db_name,
					   new_cluster.dbarr.dbs[new_dbnum].db_name) == 0)
				break;
		if (old_dbnum == old_cluster.dbarr.ndbs)
			pg_log(PG_FATAL, "New cluster database \"%s\" does not exist in the old cluster\n",
				   new_cluster.dbarr.dbs[new_dbnum].db_name);
	}
}


/*
 * create_script_for_old_cluster_deletion()
 *
 *	This is particularly useful for tablespace deletion.
 */
void
create_script_for_old_cluster_deletion(
									   char **deletion_script_file_name)
{
	FILE	   *script = NULL;
	int			tblnum;

	*deletion_script_file_name = pg_malloc(MAXPGPATH);

	prep_status("Creating script to delete old cluster");

	snprintf(*deletion_script_file_name, MAXPGPATH, "%s/delete_old_cluster.%s",
			 os_info.cwd, SCRIPT_EXT);

	if ((script = fopen(*deletion_script_file_name, "w")) == NULL)
		pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
			   *deletion_script_file_name);

#ifndef WIN32
	/* add shebang header */
	fprintf(script, "#!/bin/sh\n\n");
#endif

	/* delete old cluster's default tablespace */
	fprintf(script, RMDIR_CMD " \"%s\"\n", fix_path_separator(old_cluster.pgdata));

	/* delete old cluster's alternate tablespaces */
	for (tblnum = 0; tblnum < os_info.num_tablespaces; tblnum++)
	{
		/*
		 * Do the old cluster's per-database directories share a directory
		 * with a new version-specific tablespace?
		 */
		if (strlen(old_cluster.tablespace_suffix) == 0)
		{
			/* delete per-database directories */
			int			dbnum;

			fprintf(script, "\n");
			/* remove PG_VERSION? */
			if (GET_MAJOR_VERSION(old_cluster.major_version) <= 804)
				fprintf(script, RM_CMD " %s%s%cPG_VERSION\n",
						fix_path_separator(os_info.tablespaces[tblnum]),
						fix_path_separator(old_cluster.tablespace_suffix),
						PATH_SEPARATOR);

			for (dbnum = 0; dbnum < new_cluster.dbarr.ndbs; dbnum++)
			{
				fprintf(script, RMDIR_CMD " \"%s%s%c%d\"\n",
						fix_path_separator(os_info.tablespaces[tblnum]),
						fix_path_separator(old_cluster.tablespace_suffix),
						PATH_SEPARATOR, old_cluster.dbarr.dbs[dbnum].db_oid);
			}
		}
		else

			/*
			 * Simply delete the tablespace directory, which might be ".old"
			 * or a version-specific subdirectory.
			 */
			fprintf(script, RMDIR_CMD " \"%s%s\"\n",
					fix_path_separator(os_info.tablespaces[tblnum]),
					fix_path_separator(old_cluster.tablespace_suffix));
	}

	fclose(script);

#ifndef WIN32
	if (chmod(*deletion_script_file_name, S_IRWXU) != 0)
		pg_log(PG_FATAL, "Could not add execute permission to file:  %s\n",
			   *deletion_script_file_name);
#endif

	check_ok();
}


/*
 *	check_is_super_user()
 *
 *	Make sure we are the super-user.
 */
static void
check_is_super_user(ClusterInfo *cluster)
{
	PGresult   *res;
	PGconn	   *conn = connectToServer(cluster, "template1");

	prep_status("Checking database user is a superuser");

	/* Can't use pg_authid because only superusers can view it. */
	res = executeQueryOrDie(conn,
							"SELECT rolsuper "
							"FROM pg_catalog.pg_roles "
							"WHERE rolname = current_user");

	if (PQntuples(res) != 1 || strcmp(PQgetvalue(res, 0, 0), "t") != 0)
		pg_log(PG_FATAL, "database user \"%s\" is not a superuser\n",
			   os_info.user);

	PQclear(res);

	PQfinish(conn);

	check_ok();
}


/*
 *	check_for_isn_and_int8_passing_mismatch()
 *
 *	contrib/isn relies on data type int8, and in 8.4 int8 can now be passed
 *	by value.  The schema dumps the CREATE TYPE PASSEDBYVALUE setting so
 *	it must match for the old and new servers.
 */
static void
check_for_isn_and_int8_passing_mismatch(ClusterInfo *cluster)
{
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for contrib/isn with bigint-passing mismatch");

	if (old_cluster.controldata.float8_pass_by_value ==
		new_cluster.controldata.float8_pass_by_value)
	{
		/* no mismatch */
		check_ok();
		return;
	}

	snprintf(output_path, sizeof(output_path), "%s/contrib_isn_and_int8_pass_by_value.txt",
			 os_info.cwd);

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_proname;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(cluster, active_db->db_name);

		/* Find any functions coming from contrib/isn */
		res = executeQueryOrDie(conn,
								"SELECT n.nspname, p.proname "
								"FROM	pg_catalog.pg_proc p, "
								"		pg_catalog.pg_namespace n "
								"WHERE	p.pronamespace = n.oid AND "
								"		p.probin = '$libdir/isn'");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_proname = PQfnumber(res, "proname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database:  %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_proname));
		}

		PQclear(res);

		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains \"contrib/isn\" functions\n"
			   "| which rely on the bigint data type.  Your old and\n"
			   "| new clusters pass bigint values differently so this\n"
			   "| cluster cannot currently be upgraded.  You can\n"
			   "| manually upgrade data that use \"contrib/isn\"\n"
			   "| facilities and remove \"contrib/isn\" from the\n"
			   "| old cluster and restart the upgrade.  A list\n"
			   "| of the problem functions is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
}


/*
 * check_for_reg_data_type_usage()
 *	pg_upgrade only preserves these system values:
 *		pg_class.relfilenode
 *		pg_type.oid
 *		pg_enum.oid
 *
 *	Most of the reg* data types reference system catalog info that is
 *	not preserved, and hence these data types cannot be used in user
 *	tables upgraded by pg_upgrade.
 */
static void
check_for_reg_data_type_usage(ClusterInfo *cluster)
{
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for reg* system oid user data types");

	snprintf(output_path, sizeof(output_path), "%s/tables_using_reg.txt",
			 os_info.cwd);

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_relname,
					i_attname;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(cluster, active_db->db_name);
		char		query[QUERY_ALLOC];
		char	   *pg83_atts_str;

		if (GET_MAJOR_VERSION(old_cluster.major_version) <= 802)
			pg83_atts_str = "0";
		else
			pg83_atts_str =		"'pg_catalog.regconfig'::pg_catalog.regtype, "
								"			'pg_catalog.regdictionary'::pg_catalog.regtype ";

		snprintf(query, sizeof(query),
								"SELECT n.nspname, c.relname, a.attname "
								"FROM	pg_catalog.pg_class c, "
								"		pg_catalog.pg_namespace n, "
								"		pg_catalog.pg_attribute a "
								"WHERE	c.oid = a.attrelid AND "
								"		NOT a.attisdropped AND "
								"		a.atttypid IN ( "
		  "			'pg_catalog.regproc'::pg_catalog.regtype, "
								"			'pg_catalog.regprocedure'::pg_catalog.regtype, "
		  "			'pg_catalog.regoper'::pg_catalog.regtype, "
								"			'pg_catalog.regoperator'::pg_catalog.regtype, "
#if 0 /* allowed in GPDB */
		 "			'pg_catalog.regclass'::pg_catalog.regtype, "
#endif
		/* regtype.oid is preserved, so 'regtype' is OK */
								"			%s) AND "
								"		c.relnamespace = n.oid AND "
							  "		n.nspname != 'pg_catalog' AND "
						 "		n.nspname != 'information_schema'",
				 pg83_atts_str);

		res = executeQueryOrDie(conn, query);

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		i_attname = PQfnumber(res, "attname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database:  %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s.%s\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_relname),
					PQgetvalue(res, rowno, i_attname));
		}

		PQclear(res);

		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains one of the reg* data types in\n"
			   "| user tables.  These data types reference system oids that\n"
			   "| are not preserved by pg_upgrade, so this cluster cannot\n"
			   "| currently be upgraded.  You can remove the problem tables\n"
			   "| and restart the upgrade.  A list of the problem columns\n"
			   "| is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
}


static void
check_proper_datallowconn(ClusterInfo *cluster)
{
	int			dbnum;
	PGconn	   *conn_template1;
	PGresult   *dbres;
	int			ntups;
	int			i_datname;
	int			i_datallowconn;

	prep_status("Checking database connection settings");

	conn_template1 = connectToServer(cluster, "template1");

	/* get database names */
	dbres = executeQueryOrDie(conn_template1,
							  "SELECT	datname, datallowconn "
							  "FROM	pg_catalog.pg_database");

	i_datname = PQfnumber(dbres, "datname");
	i_datallowconn = PQfnumber(dbres, "datallowconn");

	ntups = PQntuples(dbres);
	for (dbnum = 0; dbnum < ntups; dbnum++)
	{
		char	   *datname = PQgetvalue(dbres, dbnum, i_datname);
		char	   *datallowconn = PQgetvalue(dbres, dbnum, i_datallowconn);

		if (strcmp(datname, "template0") == 0)
		{
			/* avoid restore failure when pg_dumpall tries to create template0 */
			if (strcmp(datallowconn, "t") == 0)
				pg_log(PG_FATAL, "template0 must not allow connections, "
						 "i.e. its pg_database.datallowconn must be false\n");
		}
		else
		{
			/* avoid datallowconn == false databases from being skipped on restore */
			if (strcmp(datallowconn, "f") == 0)
				pg_log(PG_FATAL, "All non-template0 databases must allow connections, "
						 "i.e. their pg_database.datallowconn must be true\n");
		}
	}

	PQclear(dbres);

	PQfinish(conn_template1);

	check_ok();
}


/*
 *	check_external_partition
 *
 *	External tables cannot be included in the partitioning hierarchy during the
 *	initial definition with CREATE TABLE, they must be defined separately and
 *	injected via ALTER TABLE EXCHANGE. The partitioning system catalogs are
 *	however not replicated onto the segments which means ALTER TABLE EXCHANGE
 *	is prohibited in utility mode. This means that pg_upgrade cannot upgrade a
 *	cluster containing external partitions, they must be handled manually
 *	before/after the upgrade.
 *
 *	Check for the existence of external partitions and refuse the upgrade if
 *	found.
 */
static void
check_external_partition(void)
{
	char		query[QUERY_ALLOC];
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	prep_status("Checking for external tables used in partitioning");

	snprintf(output_path, sizeof(output_path), "%s/external_partitions.txt",
			 os_info.cwd);
	/*
	 * We need to query the inheritance catalog rather than the partitioning
	 * catalogs since they are not available on the segments.
	 */
	snprintf(query, sizeof(query),
			 "SELECT cc.relname, c.relname AS partname, c.relnamespace "
			 "FROM   pg_inherits i "
			 "       JOIN pg_class c ON (i.inhrelid = c.oid AND c.relstorage = '%c') "
			 "       JOIN pg_class cc ON (i.inhparent = cc.oid);",
			 RELSTORAGE_EXTERNAL);

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn, query);

		ntups = PQntuples(res);

		if (ntups > 0)
		{
			found = true;

			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
					   output_path);

			for (rowno = 0; rowno < ntups; rowno++)
			{
				fprintf(script, "External partition \"%s\" in relation \"%s\"\n",
						PQgetvalue(res, rowno, PQfnumber(res, "partname")),
						PQgetvalue(res, rowno, PQfnumber(res, "relname")));
			}
		}

		PQclear(res);
		PQfinish(conn);
	}
	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains partitioned tables with external\n"
			   "| tables as partitions.  These partitions need to be removed\n"
			   "| from the partition hierarchy before the upgrade.  A list of\n"
			   "| external partitions to remove is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
	{
		check_ok();
	}
}

/*
 *	check_covering_aoindex
 *
 *	A partitioned AO table which had an index created on the parent relation,
 *	and an AO partition exchanged into the hierarchy without any indexes will
 *	break upgrades due to the way pg_dump generates DDL.
 *
 *	create table t (a integer, b text, c integer)
 *		with (appendonly=true)
 *		distributed by (a)
 *		partition by range(c) (start(1) end(3) every(1));
 *	create index t_idx on t (b);
 *
 *	At this point, the t_idx index has created AO blockdir relations for all
 *	partitions. We now exchange a new table into the hierarchy which has no
 *	index defined:
 *
 *	create table t_exch (a integer, b text, c integer)
 *		with (appendonly=true)
 *		distributed by (a);
 *	alter table t exchange partition for (rank(1)) with table t_exch;
 *
 *	The partition which was swapped into the hierarchy with EXCHANGE does not
 *	have any indexes and thus no AO blockdir relation. This is in itself not
 *	a problem, but when pg_dump generates DDL for the above situation it will
 *	create the index in such a way that it covers the entire hierarchy, as in
 *	its original state. The below snippet illustrates the dumped DDL:
 *
 *	create table t ( ... )
 *		...
 *		partition by (... );
 *	create index t_idx on t ( ... );
 *
 *	This creates a problem for the Oid synchronization in pg_upgrade since it
 *	expects to find a preassigned Oid for the AO blockdir relations for each
 *	partition. A longer term solution would be to generate DDL in pg_dump which
 *	creates the current state, but for the time being we disallow upgrades on
 *	cluster which exhibits this.
 */
static void
check_covering_aoindex(void)
{
	char			query[QUERY_ALLOC];
	char			output_path[MAXPGPATH];
	FILE		   *script = NULL;
	bool			found = false;
	int				dbnum;

	prep_status("Checking for non-covering indexes on partitioned AO tables");

	snprintf(output_path, sizeof(output_path), "%s/mismatched_aopartition_indexes.txt",
			 os_info.cwd);

	snprintf(query, sizeof(query),
			 "SELECT DISTINCT ao.relid, inh.inhrelid "
			 "FROM   pg_catalog.pg_appendonly ao "
			 "       JOIN pg_catalog.pg_inherits inh "
			 "         ON (inh.inhparent = ao.relid) "
			 "       JOIN pg_catalog.pg_appendonly aop "
			 "         ON (inh.inhrelid = aop.relid AND aop.blkdirrelid = 0) "
			 "       JOIN pg_catalog.pg_index i "
			 "         ON (i.indrelid = ao.relid) "
			 "WHERE  ao.blkdirrelid <> 0;");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		PGconn	   *conn;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn, query);

		ntups = PQntuples(res);

		if (ntups > 0)
		{
			found = true;

			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
					   output_path);

			for (rowno = 0; rowno < ntups; rowno++)
			{
				fprintf(script, "Mismatched index on partition %s in relation %s\n",
						PQgetvalue(res, rowno, PQfnumber(res, "inhrelid")),
						PQgetvalue(res, rowno, PQfnumber(res, "relid")));
			}
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains partitioned append-only tables\n"
			   "| with an index defined on the partition parent which isn't\n"
			   "| present on all partition members.  These indexes must be\n"
			   "| dropped before the upgrade.  A list of relations, and the\n"
			   "| partitions in question is in the file:\n"
			   "| \t%s\n\n", output_path);

	}
	else
	{
		check_ok();
	}
}

/*
 *	check_hash_partition_usage()
 *	8.3 -> 8.4
 *
 *	Hash partitioning was never officially supported in GPDB5 and was removed
 *	in GPDB6, but better check just in case someone has found the hidden GUC
 *	and used them anyway.
 *
 *	The hash algorithm was changed in 8.4, so upgrading is impossible anyway.
 *	This is basically the same problem as with hash indexes in PostgreSQL.
 */
static void
check_hash_partition_usage(void)
{
	int				dbnum;
	FILE		   *script = NULL;
	bool			found = false;
	char			output_path[MAXPGPATH];

	prep_status("Checking for hash partitioned tables");

	snprintf(output_path, sizeof(output_path), "%s/hash_partitioned_tables.txt",
			 os_info.cwd);

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_relname;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
								"SELECT n.nspname, c.relname "
								"FROM pg_catalog.pg_partition p, pg_catalog.pg_class c, pg_catalog.pg_namespace n "
								"WHERE p.parrelid = c.oid AND c.relnamespace = n.oid "
								"AND parkind = 'h'");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database:  %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_relname));
		}

		PQclear(res);

		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains hash partitioned tables.\n"
			   "| Upgrading hash partitioned tables is not supported,\n"
			   "| so this cluster cannot currently be upgraded.  You\n"
			   "| can remove the problem tables and restart the\n"
			   "| migration.  A list of the problem tables is in the\n"
			   "| file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
}

/*
 *	check_partition_indexes
 *
 *	There are numerous pitfalls surrounding indexes on partition hierarchies,
 *	so rather than trying to cover all the cornercases we disallow indexes on
 *	partitioned tables altogether during the upgrade.  Since we in any case
 *	invalidate the indexes forcing a REINDEX, there is little to be gained by
 *	handling them for the end-user.
 */
static void
check_partition_indexes(void)
{
	int				dbnum;
	FILE		   *script = NULL;
	bool			found = false;
	char			output_path[MAXPGPATH];

	prep_status("Checking for indexes on partitioned tables");

	snprintf(output_path, sizeof(output_path), "%s/partitioned_tables_indexes.txt",
			 os_info.cwd);

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname;
		int			i_relname;
		int			i_indexes;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
								"WITH partitions AS ("
								"    SELECT DISTINCT n.nspname, "
								"           c.relname "
								"    FROM pg_catalog.pg_partition p "
								"         JOIN pg_catalog.pg_class c ON (p.parrelid = c.oid) "
								"         JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace) "
								"    UNION "
								"    SELECT n.nspname, "
								"           partitiontablename AS relname "
								"    FROM pg_catalog.pg_partitions p "
								"         JOIN pg_catalog.pg_class c ON (p.partitiontablename = c.relname) "
								"         JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace) "
								") "
								"SELECT nspname, "
								"       relname, "
								"       count(indexname) AS indexes "
								"FROM partitions "
								"     JOIN pg_catalog.pg_indexes ON (relname = tablename AND "
								"                                    nspname = schemaname) "
								"GROUP BY nspname, relname "
								"ORDER BY relname");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		i_indexes = PQfnumber(res, "indexes");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database:  %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s has %s index(es)\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_relname),
					PQgetvalue(res, rowno, i_indexes));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains partitioned tables with\n"
			   "| indexes defined on them.  Indexes on partition parents,\n"
			   "| as well as children, must be dropped before upgrade.\n"
			   "| A list of the problem tables is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
}
