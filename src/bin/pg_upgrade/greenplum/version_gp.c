/*
 *	version_gp.c
 *
 *	Greenplum version-specific routines for upgrades
 *
 *	Copyright (c) 2016-Present VMware, Inc. or its affiliates
 *	contrib/pg_upgrade/version_gp.c
 */
#include "postgres_fe.h"

#include "pg_upgrade_greenplum.h"

#include "access/transam.h"

#define NUMERIC_ALLOC 100

/*
 * old_8_3_check_for_money_data_type_usage()
 *	8.2 -> 8.3
 *	Money data type was widened from 32 to 64 bits. It's incompatible, and we
 *	have no support for converting it. This function is hardcoded to work on
 *	the old_cluster since it's never relevant to run on the new_cluster.
 */
void
old_GPDB4_check_for_money_data_type_usage(void)
{
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for invalid 'money' user columns");

	snprintf(output_path, sizeof(output_path), "tables_using_money.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_relname,
					i_attname;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

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
old_GPDB4_check_no_free_aoseg(void)
{
	int			dbnum;
	char		output_path[MAXPGPATH];
	bool		found = false;
	FILE	   *logfile = NULL;

	prep_status("Checking for AO tables with no free segfiles");

	snprintf(output_path, sizeof(output_path), "tables_no_free_aosegs.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_relname,
					i_attname;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

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

	/* Merge with PostgreSQL v11 introduced hash partitioning again. */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 1100)
		return;

	prep_status("Checking for hash partitioned tables");

	snprintf(output_path, sizeof(output_path), "hash_partitioned_tables.txt");

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
new_gpdb5_0_invalidate_indexes(void)
{
	int			dbnum;
	FILE	   *script = NULL;
	char		output_path[MAXPGPATH];

	prep_status("Invalidating indexes in new cluster");

	snprintf(output_path, sizeof(output_path), "reindex_all.sql");

	if (!user_opts.check)
	{
		if ((script = fopen(output_path, "w")) == NULL)
			pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
				   output_path);
	}

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&new_cluster, olddb->db_name);

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(conn, "set allow_system_table_mods=true"));

		/*
		 * check_mode doesn't do much interesting for this but at least
		 * we'll know we are allowed to change allow_system_table_mods
		 * which is required
		 */
		if (!user_opts.check)
		{
			char	   *olddb_name;

			PQclear(executeQueryOrDie(conn,
									  "UPDATE pg_index SET indisvalid = false "
									  "WHERE indexrelid >= %u",
									  FirstNormalObjectId));

			olddb_name = quote_identifier(olddb->db_name);

			fprintf(script, "\\connect %s\n", olddb_name);
			fprintf(script, "REINDEX DATABASE %s;\n", olddb_name);
			pg_free(olddb_name);
		}
		PQfinish(conn);
	}

	if (!user_opts.check)
		fclose(script);
	report_status(PG_WARNING, "warning");
	if (user_opts.check)
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
 * old_GPDB5_check_for_unsupported_distribution_key_data_types()
 *
 *	abstime, reltime, tinterval, money and anyarray datatypes don't have hash opclasses
 *	in GPDB 6, so they are not supported as distribution keys anymore.
 */
void
old_GPDB5_check_for_unsupported_distribution_key_data_types(void)
{
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for abstime, reltime, tinterval user data types");

	snprintf(output_path, sizeof(output_path), "tables_using_abstime_reltime_tinterval.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_relname,
					i_attname;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
								"SELECT nspname, relname, attname "
								"FROM   pg_catalog.pg_class c, "
								"       pg_catalog.pg_namespace n, "
								"       pg_catalog.pg_attribute a, "
								"       gp_distribution_policy p "
								"WHERE  c.oid = a.attrelid AND "
								"       c.oid = p.localoid AND "
								"       a.atttypid in ('pg_catalog.abstime'::regtype, "
								"                      'pg_catalog.reltime'::regtype, "
								"                      'pg_catalog.tinterval'::regtype, "
								"                      'pg_catalog.money'::regtype, "
								"                      'pg_catalog.anyarray'::regtype) AND "
								"       attnum = any (p.attrnums) AND "
								"       c.relnamespace = n.oid AND "
		/* exclude possible orphaned temp tables */
								"  		n.nspname !~ '^pg_temp_'");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		i_attname = PQfnumber(res, "attname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("Could not open file \"%s\": %s\n",
						 output_path, strerror(errno));
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
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
		pg_fatal("Your installation contains a user table, that uses a 'abstime',\n"
				 "'reltime', 'tinterval', 'money' or 'anyarray' type as a distribution key column. Using\n"
				 "these datatypes as distribution keys is no longer supported. You can use\n"
				 "ALTER TABLE ... SET DISTRIBUTED RANDOMLY to change the distribution keys,\n"
				 "and restart the upgrade.  A list of the problem columns is in the file:\n"
				 "    %s\n\n", output_path);
	}
	else
		check_ok();
}

/*
 * old_GPDB6_check_for_unsupported_distribution_key_data_types()
 *
 *  Support for password_hash_algorithm='sha-256' was removed in GPDB 7. Check if
 *  any roles have SHA-256 password hashes.
 */
void
old_GPDB6_check_for_unsupported_sha256_password_hashes(void)
{
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for SHA-256 hashed passwords");

	snprintf(output_path, sizeof(output_path), "roles_using_sha256_passwords.txt");

	/* It's enough to check this in one database, pg_authid is a shared catalog. */
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		int			i_rolname;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[0];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
								"SELECT rolname FROM pg_catalog.pg_authid "
								"WHERE rolpassword LIKE 'sha256%%'");

		ntups = PQntuples(res);
		i_rolname = PQfnumber(res, "rolname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("Could not open file \"%s\": %s\n",
						 output_path, strerror(errno));
			fprintf(script, "  %s\n",
					PQgetvalue(res, rowno, i_rolname));
		}

		PQclear(res);

		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		pg_fatal("Your installation contains roles with SHA-256 hashed passwords. Using\n"
				 "SHA-256 for password hashes is no longer supported. You can use\n"
				 "ALTER ROLE <role name> WITH PASSWORD NULL as superuser to clear passwords,\n"
				 "and restart the upgrade.  A list of the problem roles is in the file:\n"
				 "    %s\n\n", output_path);
	}
	else
		check_ok();
}

/*
 * new_gpdb_invalidate_bitmap_indexes()
 *
 * TODO: We are currently missing the support to migrate over bitmap indexes.
 * Hence, mark all bitmap indexes as invalid.
 */
static void
new_gpdb_invalidate_bitmap_indexes(void)
{
	int			dbnum;

	prep_status("Invalidating bitmap indexes in new cluster");

	for (dbnum = 0; dbnum < new_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &new_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&new_cluster, olddb->db_name);

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(conn, "set allow_system_table_mods=true"));

		/*
		 * check mode doesn't do much interesting for this but at least
		 * we'll know we are allowed to change allow_system_table_mods
		 * which is required
		 */
		if (!user_opts.check)
		{
			PQclear(executeQueryOrDie(conn,
									  "UPDATE pg_index SET indisvalid = false "
									  "  FROM pg_class c "
									  " WHERE c.oid = indexrelid AND "
									  "       indexrelid >= %u AND "
									  "       relam = 3013;",
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
		res = executeQueryOrDie(conn,
				 "SELECT typ.oid AS typoid, base.oid AS baseoid "
				 "FROM pg_type base "
				 "     JOIN pg_type typ ON base.oid = typ.typbasetype "
				 "WHERE base.typbasetype = '%u'::pg_catalog.oid;",
				 result[iterator++]);

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

void
after_create_new_objects_greenplum(void)
{
	/*
	 * If we're upgrading from GPDB4, mark all indexes as invalid.
	 * The page format is incompatible, and while convert heap
	 * and AO tables automatically, we don't have similar code for
	 * indexes. Also, the heap conversion relocates tuples, so
	 * any indexes on heaps would need to be rebuilt for that
	 * reason, anyway.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) == 802)
		new_gpdb5_0_invalidate_indexes();
	else
	{
		/* TODO: Bitmap indexes are not supported, so mark them as invalid. */
		new_gpdb_invalidate_bitmap_indexes();
	}
}
