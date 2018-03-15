/*
 *	aotable.c
 *
 *	functions for restoring Append-Only auxiliary tables
 *
 *	Copyright (c) 2016, Pivotal Software Inc
 */

#include "pg_upgrade.h"
#include "pqexpbuffer.h"

/*
 * We cannot use executeQueryOrDie for the INSERTs below, because it has a size limit
 * on the query.
 */
static void
executeLargeCommandOrDie(migratorContext *ctx, PGconn *conn, const char *command)
{
	PGresult   *result;
	ExecStatusType status;

	pg_log(ctx, PG_DEBUG, "executing: %s\n", command);
	result = PQexec(conn, command);
	status = PQresultStatus(result);

	if ((status != PGRES_TUPLES_OK) && (status != PGRES_COMMAND_OK))
	{
		/*
		 * Put these on separate lines, because the command can be so large that
		 * pg_log truncates it. We don't want the error message to be truncated
		 * even if that happens.
		 */
		pg_log(ctx, PG_REPORT, "DB command failed\n%s\n%s\n", command);
		pg_log(ctx, PG_REPORT, "libpq error was: %s\n",
			   PQerrorMessage(conn));
		PQclear(result);
		PQfinish(conn);
		exit_nicely(ctx, true);
	}

	PQclear(result);
}

/*
 * Populate contents of the AO segment, block directory, and visibility
 * map tables (pg_ao{cs}seg_<oid>), for one AO relation.
 */
static void
restore_aosegment_table(migratorContext *ctx, PGconn *conn, RelInfo *rel)
{
	PQExpBuffer query;
	int			i;
	int			segno;

	/* The visibility maps and such can be quite large, so we need a large buffer. */
	query = createPQExpBuffer();

	/* Restore the entry in the AO segment table. */
	for (i = 0; i < rel->naosegments; i++)
	{
		resetPQExpBuffer(query);

		/* Row oriented AO table */
		if (rel->relstorage == 'a')
		{
			AOSegInfo  *seg = &rel->aosegments[i];

			appendPQExpBuffer(query,
							  "INSERT INTO pg_aoseg.pg_aoseg_%u (segno, eof, tupcount, varblockcount, eofuncompressed, modcount, formatversion, state) "
							  " VALUES (%d, " INT64_FORMAT ", " INT64_FORMAT ", " INT64_FORMAT ", " INT64_FORMAT ", " INT64_FORMAT ", %d, %d)",
							  rel->reloid,
							  seg->segno,
							  seg->eof,
							  seg->tupcount,
							  seg->varblockcount,
							  seg->eofuncompressed,
							  seg->modcount,
							  seg->version,
							  seg->state);

			segno = seg->segno;
		}
		/* Column oriented AO table */
		else
		{
			AOCSSegInfo *seg = &rel->aocssegments[i];
			char	    *vpinfo_escaped;

			vpinfo_escaped = PQescapeLiteral(conn, seg->vpinfo, strlen(seg->vpinfo));
			if (vpinfo_escaped == NULL)
				pg_log(ctx, PG_FATAL, "%s: out of memory\n", ctx->progname);

			appendPQExpBuffer(query,
							  "INSERT INTO pg_aoseg.pg_aocsseg_%u (segno, tupcount, varblockcount, vpinfo, modcount, formatversion, state) "
							  " VALUES (%d, " INT64_FORMAT ", " INT64_FORMAT ", %s, " INT64_FORMAT ", %d, %d)",
							  rel->reloid,
							  seg->segno,
							  seg->tupcount,
							  seg->varblockcount,
							  vpinfo_escaped,
							  seg->modcount,
							  seg->version,
							  seg->state);
			free(vpinfo_escaped);

			segno = seg->segno;
		}

		executeLargeCommandOrDie(ctx, conn, query->data);
	}

	/* Restore the entries in the AO visimap table. */
	for (i = 0; i < rel->naovisimaps; i++)
	{
		AOVisiMapInfo  *seg = &rel->aovisimaps[i];
		char	   *visimap_escaped;

		resetPQExpBuffer(query);

		visimap_escaped = PQescapeLiteral(conn, seg->visimap, strlen(seg->visimap));
		if (visimap_escaped == NULL)
			pg_log(ctx, PG_FATAL, "%s: out of memory\n", ctx->progname);

		appendPQExpBuffer(query,
						  "INSERT INTO pg_aoseg.pg_aovisimap_%u (segno, first_row_no, visimap) "
						  " VALUES (%d, " INT64_FORMAT ", %s)",
						  rel->reloid,
						  seg->segno,
						  seg->first_row_no,
						  visimap_escaped);
		PQfreemem(visimap_escaped);

		executeLargeCommandOrDie(ctx, conn, query->data);
	}

	/* Restore the entries in the AO blkdir table. */
	for (i = 0; i < rel->naoblkdirs; i++)
	{
		AOBlkDir	*seg = &rel->aoblkdirs[i];
		char	   *minipage_escaped;

		resetPQExpBuffer(query);

		minipage_escaped = PQescapeLiteral(conn, seg->minipage, strlen(seg->minipage));
		if (minipage_escaped == NULL)
			pg_log(ctx, PG_FATAL, "%s: out of memory\n", ctx->progname);

		appendPQExpBuffer(query,
						  "INSERT INTO pg_aoseg.pg_aoblkdir_%u (segno, columngroup_no, first_row_no, minipage) "
						  " VALUES (%d, %d, " INT64_FORMAT ", %s)",
						  rel->reloid,
						  seg->segno,
						  seg->columngroup_no,
						  seg->first_row_no,
						  minipage_escaped);

		executeLargeCommandOrDie(ctx, conn, query->data);
	}

	destroyPQExpBuffer(query);
}

void
restore_aosegment_tables(migratorContext *ctx)
{
	int			dbnum;

	prep_status(ctx, "Restoring append-only auxiliary tables in new cluster");

	/*
	 * Rebuilding AO auxiliary tables can potentially take some time in a large
	 * cluster so swap out the current progress file before starting so that
	 * the user can see what's going on.
	 */
	report_progress(ctx, CLUSTER_NEW, FIXUP, "Rebuilding AO auxiliary tables");
	close_progress(ctx);

	for (dbnum = 0; dbnum < ctx->old.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &ctx->old.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(ctx, olddb->db_name, CLUSTER_NEW);
		int			relnum;

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(ctx, conn,
								  "set allow_system_table_mods='dml'"));

		for (relnum = 0; relnum < olddb->rel_arr.nrels; relnum++)
			restore_aosegment_table(ctx, conn, &olddb->rel_arr.rels[relnum]);

		PQfinish(conn);
	}

	check_ok(ctx);
}
