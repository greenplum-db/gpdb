/*
 *	version_gp.c
 *
 *	Greenplum version-specific routines for upgrades
 *
 *	Copyright (c) 2016-Present Pivotal Software, Inc
 *	contrib/pg_upgrade/version_gp.c
 */
#include "postgres_fe.h"

#include "pg_upgrade_greenplum.h"

#include "access/transam.h"

#define NUMERIC_ALLOC 100

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
		gp_fatal_log(
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
									  "       relam IN (SELECT oid FROM pg_am "
									  "       WHERE amname = 'bitmap');",
									  FirstNormalObjectId));
		}
		PQfinish(conn);
	}

	check_ok();
}

void
invalidate_indexes(void)
{
	if (!(is_gpdb6(&old_cluster) && is_gpdb6(&new_cluster)))
	{
		/* TODO: Bitmap indexes are not supported, so mark them as invalid. */
		new_gpdb_invalidate_bitmap_indexes();
	}
}

/*
 * In greenplum, after collecting the relation information, bitmap
 * indexes are marked as invalid. The same catalog is copied to the segment,
 * and when information is collected about the objects, invalid indexes are
 * not considered from the new segment. But the segment should be reset to
 * the original state where the indexes were valid else upgrade will report
 * failure indicating that there are missing index objects in the source segment
 * and the target segment.
 *
 * Note: After the relation information is collected, these indexes are marked
 * invalid.
 */
void
reset_invalid_indexes(void)
{

	/* Should be run only on segment databases */
	Assert(!is_greenplum_dispatcher_mode());
	Assert(!user_opts.check);

	prep_status("Resetting indexes marked as invalid");

	int			dbnum;

	for (dbnum = 0; dbnum < new_cluster.dbarr.ndbs; dbnum++)
	{

		DbInfo	   *active_db = &new_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&new_cluster, active_db->db_name);

		PQclear(executeQueryOrDie(conn, "SET allow_system_table_mods=true"));

		if (GET_MAJOR_VERSION(old_cluster.major_version) == 803)
		{
			/*
			 * Enable btree based bpchar_pattern_ops indexes
			 */
			PQclear(executeQueryOrDie(conn,
									  "UPDATE pg_catalog.pg_index i "
									  "SET	indisvalid = true "
									  "FROM	pg_catalog.pg_class c, "
									  "		pg_catalog.pg_namespace n "
									  "WHERE	indexrelid = c.oid AND "
									  "		c.relnamespace = n.oid AND "
									  "		( "
									  "			SELECT	o.oid "
									  "			FROM	pg_catalog.pg_opclass o, "
									  "					pg_catalog.pg_am a"
									  "			WHERE	a.amname NOT IN ('hash', 'gin', 'bitmap') AND "
									  "					a.oid = o.opcmethod AND "
									  "					o.opcname = 'bpchar_pattern_ops') "
									  "		= ANY (i.indclass)"));
		}


		/*
		 * Enable bitmap indexes
		 */
		PQclear(executeQueryOrDie(conn,
								  "UPDATE pg_index SET indisvalid = true "
								  "  FROM pg_class c "
								  " WHERE c.oid = indexrelid AND "
								  "       indexrelid >= %u AND "
								  "       relam IN (SELECT oid FROM pg_am "
								  "       WHERE amname = 'bitmap');",
								  FirstNormalObjectId));

		PQfinish(conn);
	}

	check_ok();

}
