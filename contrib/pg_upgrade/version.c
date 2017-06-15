/*
 *	version.c
 *
 *	Postgres-version-specific routines
 *
 *	Copyright (c) 2010, PostgreSQL Global Development Group
 *	$PostgreSQL: pgsql/contrib/pg_upgrade/version.c,v 1.5 2010/07/03 16:33:14 momjian Exp $
 */

#include "pg_upgrade.h"

#include "access/transam.h"


/*
 * new_9_0_populate_pg_largeobject_metadata()
 *	new >= 9.0, old <= 8.4
 *	9.0 has a new pg_largeobject permission table
 */
void
new_9_0_populate_pg_largeobject_metadata(migratorContext *ctx, bool check_mode,
										 Cluster whichCluster)
{
	ClusterInfo *active_cluster = (whichCluster == CLUSTER_OLD) ?
	&ctx->old : &ctx->new;
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status(ctx, "Checking for large objects");

	snprintf(output_path, sizeof(output_path), "%s/pg_largeobject.sql",
			 ctx->cwd);

	for (dbnum = 0; dbnum < active_cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			i_count;
		DbInfo	   *active_db = &active_cluster->dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(ctx, active_db->db_name, whichCluster);

		/* find if there are any large objects */
		res = executeQueryOrDie(ctx, conn,
								"SELECT count(*) "
								"FROM	pg_catalog.pg_largeobject ");

		i_count = PQfnumber(res, "count");
		if (atoi(PQgetvalue(res, 0, i_count)) != 0)
		{
			found = true;
			if (!check_mode)
			{
				if (script == NULL && (script = fopen(output_path, "w")) == NULL)
					pg_log(ctx, PG_FATAL, "Could not create necessary file:  %s\n", output_path);
				fprintf(script, "\\connect %s\n",
						quote_identifier(ctx, active_db->db_name));
				fprintf(script,
						"SELECT pg_catalog.lo_create(t.loid)\n"
						"FROM (SELECT DISTINCT loid FROM pg_catalog.pg_largeobject) AS t;\n");
			}
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		if (!check_mode)
			fclose(script);
		report_status(ctx, PG_WARNING, "warning");
		if (check_mode)
			pg_log(ctx, PG_WARNING, "\n"
				   "| Your installation contains large objects.\n"
				   "| The new database has an additional large object\n"
				   "| permission table.  After migration, you will be\n"
				   "| given a command to populate the pg_largeobject\n"
				   "| permission table with default permissions.\n\n");
		else
			pg_log(ctx, PG_WARNING, "\n"
				   "| Your installation contains large objects.\n"
				   "| The new database has an additional large object\n"
				   "| permission table so default permissions must be\n"
				   "| defined for all large objects.  The file:\n"
				   "| \t%s\n"
				   "| when executed by psql by the database super-user\n"
				   "| will define the default permissions.\n\n",
				   output_path);
	}
	else
		check_ok(ctx);
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
new_gpdb5_0_invalidate_indexes(migratorContext *ctx, bool check_mode,
							   Cluster whichCluster)
{
	int			dbnum;

	prep_status(ctx, "Invalidating indexes in new cluster");

	for (dbnum = 0; dbnum < ctx->old.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &ctx->old.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(ctx, olddb->db_name, CLUSTER_NEW);
		char		query[QUERY_ALLOC];

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(ctx, conn,
								  "set allow_system_table_mods='dml'"));

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
			PQclear(executeQueryOrDie(ctx, conn, query));
		}
		PQfinish(conn);
	}

	check_ok(ctx);
}

/*
 * new_gpdb_invalidate_bitmap_indexes()
 *
 * TODO: We are currently missing the support to migrate over bitmap indexes.
 * Hence, mark all bitmap indexes as invalid.
 */
void
new_gpdb_invalidate_bitmap_indexes(migratorContext *ctx, bool check_mode,
								   Cluster whichCluster)
{
	int			dbnum;

	prep_status(ctx, "Invalidating indexes in new cluster");

	for (dbnum = 0; dbnum < ctx->old.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &ctx->old.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(ctx, olddb->db_name, CLUSTER_NEW);

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(ctx, conn,
								  "set allow_system_table_mods='dml'"));

		/*
		 * check_mode doesn't do much interesting for this but at least
		 * we'll know we are allowed to change allow_system_table_mods
		 * which is required
		 */
		if (!check_mode)
		{
			PQclear(executeQueryOrDie(ctx, conn,
									  "UPDATE pg_index SET indisvalid = false FROM pg_class c WHERE c.oid = indexrelid AND indexrelid >= %u AND relam = 3013;",
									  FirstNormalObjectId));
		}
		PQfinish(conn);
	}

	check_ok(ctx);
}
