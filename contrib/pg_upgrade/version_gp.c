/*
 *	version_old_gpdb4.c
 *
 *	GPDB-version-specific routines for upgrades from Greenplum 4.3.x
 *
 *	Copyright (c) 2016-Present Pivotal Software, Inc
 */
#include "pg_upgrade.h"

#include "access/transam.h"

/*
 * old_8_3_check_for_money_data_type_usage()
 *	8.2 -> 8.3
 *	Money data type was widened from 32 to 64 bits. It's incompatible, and we have no
 *  support for converting it.
 */
void
old_GPDB4_check_for_money_data_type_usage(ClusterInfo *cluster)
{
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for invalid 'money' user columns");

	snprintf(output_path, sizeof(output_path), "%s/tables_using_money.txt",
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

		/*
		 * 
		 */
		res = executeQueryOrDie(conn,
								"SELECT n.nspname, c.relname, a.attname "
								"FROM	pg_catalog.pg_class c, "
								"		pg_catalog.pg_namespace n, "
								"		pg_catalog.pg_attribute a "
								"WHERE	c.oid = a.attrelid AND "
								"		NOT a.attisdropped AND "
								"		a.atttypid = 'pg_catalog.money'::pg_catalog.regtype AND "
								"		c.relnamespace = n.oid AND "
								/* exclude possibly orphaned temp tables */
							 	"		n.nspname != 'pg_catalog' AND "
								"		n.nspname !~ '^pg_temp_' AND "
								"		n.nspname !~ '^pg_toast_temp_' AND "
								"		n.nspname != 'information_schema' ");

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

	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains the \"money\" data type in\n"
			   "| user tables.  This data type changed its internal\n"
			   "| format between your old and new clusters so this\n"
			   "| cluster cannot currently be upgraded.  You can\n"
			   "| remove the problem tables and restart the migration.\n"
			   "| A list of the problem columns is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
}

/*
 * old_GPDB4_check_no_free_aoseg()
 *
 *	In 8.2 -> 8.3 the numeric datatype was altered and AO tables with numeric
 *	columns will be rewritten as pages are encountered. In order to rewrite, we
 *	need to be able to create new segment files so check that we aren't using
 *	all 127 segfiles.
 */
void
old_GPDB4_check_no_free_aoseg(ClusterInfo *cluster)
{
	int			dbnum;
	char		output_path[MAXPGPATH];
	bool		found = false;
	FILE	   *logfile = NULL;

	prep_status("Checking for AO tables with no free segfiles");

	snprintf(output_path, sizeof(output_path), "%s/tables_no_free_aosegs.txt",
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

		res = executeQueryOrDie(conn,
								"SELECT n.nspname, c.relname, a.attname "
								"FROM	pg_catalog.pg_class c, "
								"		pg_catalog.pg_namespace n, "
								"		pg_catalog.pg_attribute a "
								"WHERE	c.oid = a.attrelid AND "
								"		NOT a.attisdropped AND "
								"		a.atttypid = 'pg_catalog.numeric'::pg_catalog.regtype AND "
								"		c.relnamespace = n.oid AND "
								/* exclude possibly orphaned temp tables */
								"		n.nspname != 'pg_catalog' AND "
								"		n.nspname !~ '^pg_temp_' AND "
								"		n.nspname !~ '^pg_toast_temp_' AND "
								"		n.nspname != 'information_schema' AND "
								/* restrict to AO tables with 127 segfiles */
								"		c.relname IN ( "
								"			SELECT relname "
								"			FROM   gp_relation_node "
								"				   JOIN pg_appendonly ON relfilenode_oid = relid "
								"				   JOIN pg_class ON relfilenode_oid = relfilenode "
								"			GROUP BY relname, segment_file_num HAVING count(*) >= 127 "
								"		) ");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		i_attname = PQfnumber(res, "attname");
		if (ntups > 0)
		{
			if (logfile == NULL && (logfile = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);
			found = true;
			for (rowno = 0; rowno < ntups; rowno++)
			{
				if (!db_used)
				{
					fprintf(logfile, "Database:  %s\n", active_db->db_name);
					db_used = true;
				}
				fprintf(logfile, "  %s.%s.%s\n",
						PQgetvalue(res, rowno, i_nspname),
						PQgetvalue(res, rowno, i_relname),
						PQgetvalue(res, rowno, i_attname));
			}
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		if (logfile)
			fclose(logfile);
		pg_log(PG_REPORT, "warning\n");
		pg_log(PG_WARNING,
			   "| Your installation contains the \"numeric\" data type in\n"
			   "| one or more AO tables without free segments.  In order to\n"
			   "| rewrite the table(s), please recreate them using a CREATE\n"
			   "| TABLE AS .. statement.  A list of the problem tables is in\n"
			   "| the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
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
void
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
 * new_gpdb5_0_invalidate_indexes()
 *	new >= GPDB 5.0, old <= GPDB 4.3
 *
 * GPDB 5.0 follows the PostgreSQL 8.3 page format, while GPDB 4.3 used
 * the 8.2 format. A new field was added to the page header, so we need
 * mark all indexes as invalid.
 */
void
new_gpdb5_0_invalidate_indexes(bool check_mode)
{
	int			dbnum;
	FILE	   *script = NULL;
	char		output_path[MAXPGPATH];

	prep_status("Invalidating indexes in new cluster");

	snprintf(output_path, sizeof(output_path), "%s/reindex_all.sql",
			 os_info.cwd);

	if (!check_mode)
	{
		if ((script = fopen(output_path, "w")) == NULL)
			pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
				   output_path);
	}

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&new_cluster, olddb->db_name);
		char		query[QUERY_ALLOC];

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(conn, "set allow_system_table_mods='dml'"));

		/*
		 * check_mode doesn't do much interesting for this but at least
		 * we'll know we are allowed to change allow_system_table_mods
		 * which is required
		 */
		if (!check_mode)
		{
			snprintf(query, sizeof(query),
					 "UPDATE pg_index SET indisvalid = false WHERE indexrelid >= %u",
					 FirstNormalObjectId);
			PQclear(executeQueryOrDie(conn, query));

			fprintf(script, "\\connect %s\n",
					quote_identifier(olddb->db_name));
			fprintf(script, "REINDEX DATABASE %s;\n",
					quote_identifier(olddb->db_name));
		}
		PQfinish(conn);
	}

	if (!check_mode)
		fclose(script);
	report_status(PG_WARNING, "warning");
	if (check_mode)
		pg_log(PG_WARNING, "\n"
			   "| All indexes have different internal formats\n"
			   "| between your old and new clusters so they must\n"
			   "| be reindexed with the REINDEX command. After\n"
			   "| migration, you will be given REINDEX instructions.\n\n");
	else
		pg_log(PG_WARNING, "\n"
			   "| All indexes have different internal formats\n"
			   "| between your old and new clusters so they must\n"
			   "| be reindexed with the REINDEX command.\n"
			   "| The file:\n"
			   "| \t%s\n"
			   "| when executed by psql by the database super-user\n"
			   "| will recreate all invalid indexes.\n\n",
			   output_path);
}

/*
 * new_gpdb_invalidate_bitmap_indexes()
 *
 * TODO: We are currently missing the support to migrate over bitmap indexes.
 * Hence, mark all bitmap indexes as invalid.
 */
void
new_gpdb_invalidate_bitmap_indexes(bool check_mode)
{
	int			dbnum;

	prep_status("Invalidating indexes in new cluster");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&new_cluster, olddb->db_name);

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(conn, "set allow_system_table_mods='dml'"));

		/*
		 * check_mode doesn't do much interesting for this but at least
		 * we'll know we are allowed to change allow_system_table_mods
		 * which is required
		 */
		if (!check_mode)
		{
			PQclear(executeQueryOrDie(conn,
									  "UPDATE pg_index SET indisvalid = false FROM pg_class c WHERE c.oid = indexrelid AND indexrelid >= %u AND relam = 3013;",
									  FirstNormalObjectId));
		}
		PQfinish(conn);
	}

	check_ok();
}

/*
 * get_numeric_types()
 *
 * queries the cluster for all types based on NUMERIC, as well as NUMERIC
 * itself, and return an InvalidOid terminated array of pg_type Oids for
 * these types.
 */
Oid *
get_numeric_types(PGconn *conn)
{
	char		query[QUERY_ALLOC];
	PGresult   *res;
	Oid		   *result;
	int			result_count = 0;
	int			iterator = 0;

	/*
	 * We don't know beforehands how many types based on NUMERIC that we will
	 * find so allocate space that "should be enough". Once processed we can
	 * shrink the allocation or we could've put these on the stack and moved
	 * to a heap based allocation at that point - but even at a too large
	 * array we still waste very little memory in the grand scheme of things
	 * so keep it simple and leave it be with an overflow check instead.
	 */
	result = pg_malloc(sizeof(Oid) * NUMERIC_ALLOC);
	memset(result, InvalidOid, NUMERIC_ALLOC);

	result[result_count++] = 1700;		/* 1700 == NUMERICOID */

	/*
	 * iterator is a trailing pointer into the array which traverses the
	 * array one by one while result_count fills it - and can do so by
	 * adding n items per loop iteration. Once iterator has caught up with
	 * result_count we know that no more pg_type tuples are of interest.
	 * This is a handcoded version of WITH RECURSIVE and can be replaced
	 * by an actual recursive CTE once GPDB supports these.
	 */
	while (iterator < result_count && result[iterator] != InvalidOid)
	{
		snprintf(query, sizeof(query),
				 "SELECT typ.oid AS typoid, base.oid AS baseoid "
				 "FROM pg_type base "
				 "  JOIN pg_type typ ON base.oid = typ.typbasetype "
				 "WHERE base.typbasetype = '%u'::pg_catalog.oid;",
				 result[iterator++]);

		res = executeQueryOrDie(conn, query);

		if (PQntuples(res) > 0)
		{
			result[result_count++] = atooid(PQgetvalue(res, 0, PQfnumber(res, "typoid")));
			result[result_count++] = atooid(PQgetvalue(res, 0, PQfnumber(res, "baseoid")));
		}

		PQclear(res);

		if (result_count == NUMERIC_ALLOC - 1)
			pg_log(PG_FATAL, "Too many NUMERIC types found");
	}

	return result;
}
