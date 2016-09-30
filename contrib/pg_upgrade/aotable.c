/*
 *	aotable.c
 *
 *	functions for restoring Append-Only auxiliary tables
 */

#include "pg_upgrade.h"

/*
 * Populate contents of the AO segment, block directory, and visibility
 * map tables (pg_aoseg_<oid>), for one AO relation.
 */
static void
restore_aosegment_table(migratorContext *ctx, PGconn *conn, RelInfo *rel)
{
	char		query[QUERY_ALLOC];
	int			i;

	/* Restore the entry in the AO segment table. */
	for (i = 0; i < rel->naosegments; i++)
	{
		AOSegInfo  *seg = &rel->aosegments[i];

		snprintf(query, sizeof(query),
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

		PQclear(executeQueryOrDie(ctx, conn, query));

		/*
		 * Also create an entry in gp_relation_node for this.
		 *
		 * FIXME: I have no clue what the correct values to insert would be.
		 * But these seem to get us past the "Missing pg_aoseg entry 1 in
		 * gp_relation_node" error that you get otherwise.
		 */
		snprintf(query, sizeof(query),
				 "INSERT INTO gp_relation_node (relfilenode_oid, segment_file_num, create_mirror_data_loss_tracking_session_num, persistent_tid, persistent_serial_num) "
				 " VALUES (%u, %d, " INT64_FORMAT ", '(%u, %u)', " INT64_FORMAT ")",
				 rel->reloid,
				 seg->segno,
				 (int64) 0,
				 0, 0,	/* invalid TID */
				 (int64) 0);

		PQclear(executeQueryOrDie(ctx, conn, query));
	}

	/* Restore the entries in the AO visimap table. */
	for (i = 0; i < rel->naovisimaps; i++)
	{
		AOVisiMapInfo  *seg = &rel->aovisimaps[i];
		char	   *visimap_escaped;

		visimap_escaped = PQescapeLiteral(conn, seg->visimap, strlen(seg->visimap));
		if (visimap_escaped == NULL)
			pg_log(ctx, PG_FATAL, "%s: out of memory\n", ctx->progname);

		snprintf(query, sizeof(query),
				 "INSERT INTO pg_aoseg.pg_aovisimap_%u (segno, first_row_no, visimap) "
				 " VALUES (%d, " INT64_FORMAT ", %s)",
				 rel->reloid,
				 seg->segno,
				 seg->first_row_no,
				 visimap_escaped);
		free(visimap_escaped);

		PQclear(executeQueryOrDie(ctx, conn, query));
	}
}

void
restore_aosegment_tables(migratorContext *ctx)
{
	int			dbnum;

	prep_status(ctx, "Restoring append-only auxiliary tables in new cluster");

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

		/*
		 * There are further safeguards for gp_relation_node. Bypass those too.
		 */
		PQclear(executeQueryOrDie(ctx, conn,
								  "set gp_permit_relation_node_change = on"));

		for (relnum = 0; relnum < olddb->rel_arr.nrels; relnum++)
			restore_aosegment_table(ctx, conn, &olddb->rel_arr.rels[relnum]);

		PQfinish(conn);
	}

	check_ok(ctx);
}
