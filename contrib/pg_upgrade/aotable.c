/*
 *	aotable.c
 *
 *	functions for restoring Append-Only auxiliary tables
 *
 *	Copyright (c) 2016, Pivotal Software Inc
 */

#include "pg_upgrade.h"

/*
 * Populate contents of the AO segment, block directory, and visibility
 * map tables (pg_ao{cs}seg_<oid>), for one AO relation.
 */
static void
restore_aosegment_table(migratorContext *ctx, PGconn *conn, RelInfo *rel)
{
	char		query[QUERY_ALLOC];
	int			i;
	int			segno;

	/* Restore the entry in the AO segment table. */
	for (i = 0; i < rel->naosegments; i++)
	{
		/* Row oriented AO table */
		if (rel->relstorage == 'a')
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

			snprintf(query, sizeof(query),
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
		PQfreemem(visimap_escaped);

		PQclear(executeQueryOrDie(ctx, conn, query));
	}

	/* Restore the entries in the AO blkdir table. */
	for (i = 0; i < rel->naoblkdirs; i++)
	{
		AOBlkDir	*seg = &rel->aoblkdirs[i];

		snprintf(query, sizeof(query),
				 "INSERT INTO pg_aoseg.pg_aoblkdir_%u (segno, columngroup_no, first_row_no, minipage) "
				 " VALUES (%d, %d, " INT64_FORMAT ", " INT64_FORMAT "::bit(36))",
				 rel->reloid,
				 seg->segno,
				 seg->columngroup_no,
				 seg->first_row_no,
				 seg->minipage);

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

void
restore_persistent_tables(migratorContext *ctx)
{
	PGconn	   *conn = connectToServer(ctx, "template1", CLUSTER_NEW);

	prep_status(ctx, "Rebuild gp_relation_node in new cluster");

	PQclear(executeQueryOrDie(ctx, conn, "checkpoint"));
	PQclear(executeQueryOrDie(ctx, conn, "select gp_persistent_reset_all()"));
	PQclear(executeQueryOrDie(ctx, conn, "select gp_persistent_build_all(false)"));
	PQfinish(conn);

	check_ok(ctx);
}
