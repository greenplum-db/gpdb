/*
 *	aotable.c
 *
 *	functions for restoring Append-Only auxiliary tables
 *
 *	Copyright (c) 2016-Present, VMware, Inc. or its affiliates
 */
#include "postgres_fe.h"

#include "pg_upgrade_greenplum.h"

#include "pqexpbuffer.h"

/* needs to be kept in sync with pg_class.h */
#define RELSTORAGE_AOROWS	'a'
#define RELSTORAGE_AOCOLS	'c'

/*
 * We cannot use executeQueryOrDie for the INSERTs below, because it has a size
 * limit on the query.
 */
static void
executeLargeCommandOrDie(PGconn *conn, const char *command)
{
	PGresult   *result;
	ExecStatusType status;

	pg_log(PG_VERBOSE, "executing: %s\n", command);
	result = PQexec(conn, command);
	status = PQresultStatus(result);

	if ((status != PGRES_TUPLES_OK) && (status != PGRES_COMMAND_OK))
	{
		/*
		 * Put these on separate lines, because the command can be so large that
		 * pg_log truncates it. We don't want the error message to be truncated
		 * even if that happens.
		 */
		pg_log(PG_REPORT, "DB command failed\n%s\n", command);
		pg_log(PG_REPORT, "libpq error was: %s\n",
			   PQerrorMessage(conn));
		PQclear(result);
		PQfinish(conn);

		printf("Failure, exiting\n");
		exit(1);
	}

	PQclear(result);
}

/*
 * Populate contents of the AO segment, block directory, and visibility
 * map tables (pg_ao{cs}seg_<oid>), for one AO relation.
 */
static void
restore_aosegment_table(PGconn *conn, RelInfo *rel)
{
	PQExpBuffer query;
	int			i;
	int			segno;
	char	   *segrelname;
	char	   *vmaprelname;
	char	   *blkdirrelname;
	PGresult   *res;

	/*
	 * The visibility maps and such can be quite large, so we need a large
	 * buffer.
	 */
	query = createPQExpBuffer();

	appendPQExpBuffer(query,
					  "SELECT s.relname AS segrelname, "
					  "       v.relname AS vmaprelname, "
					  "       b.relname AS blkdirrelname "
					  "FROM   pg_catalog.pg_appendonly "
					  "       JOIN pg_class s ON (segrelid = s.oid) "
					  "       JOIN pg_class v ON (visimaprelid = v.oid) "
					  "       LEFT OUTER JOIN pg_class b ON (blkdirrelid = b.oid) "
					  "WHERE relid = %u::pg_catalog.oid",
					  rel->reloid);

	res = executeQueryOrDie(conn, "%s", query->data);

	/* XXX We should really find an aosegments table here */
	if (PQntuples(res) == 0)
	{
		PQclear(res);
		destroyPQExpBuffer(query);
		return;
	}

	segrelname = pg_strdup(PQgetvalue(res, 0, PQfnumber(res, "segrelname")));
	vmaprelname = pg_strdup(PQgetvalue(res, 0, PQfnumber(res, "vmaprelname")));
	if (!PQgetisnull(res, 0, PQfnumber(res, "blkdirrelname")))
		blkdirrelname = pg_strdup(PQgetvalue(res, 0, PQfnumber(res, "blkdirrelname")));
	else
		blkdirrelname = NULL;

	PQclear(res);

	/*
	 * Restore the entry in the AO segment table.
	 *
	 * There may already be junk data in the table, since we copy the master
	 * data directory over to the segment before upgrade. Get rid of it first.
	 */
	executeQueryOrDie(conn, "TRUNCATE pg_aoseg.%s", segrelname);

	for (i = 0; i < rel->naosegments; i++)
	{
		resetPQExpBuffer(query);

		/* Row oriented AO table */
		if (rel->relstorage == RELSTORAGE_AOROWS)
		{
			AOSegInfo  *seg = &rel->aosegments[i];

			appendPQExpBuffer(query,
							  "INSERT INTO pg_aoseg.%s "
							  "( "
							  "     segno, eof, tupcount, varblockcount, "
							  "     eofuncompressed, modcount, formatversion, "
							  "     state "
							  ") "
							  "VALUES (%d, " INT64_FORMAT ", " INT64_FORMAT ", "
							                 INT64_FORMAT ", " INT64_FORMAT ", "
											 INT64_FORMAT ", %d, %d)",
							  segrelname,
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
				pg_log(PG_FATAL, "%s: out of memory\n", os_info.progname);

			appendPQExpBuffer(query,
							  "INSERT INTO pg_aoseg.%s "
							  "( "
							  "		segno, tupcount, varblockcount, vpinfo, "
							  "		modcount, formatversion, state"
							  ") "
							  " VALUES (%d, " INT64_FORMAT ", " INT64_FORMAT ", %s, " INT64_FORMAT ", %d, %d)",
							  segrelname,
							  seg->segno,
							  seg->tupcount,
							  seg->varblockcount,
							  vpinfo_escaped,
							  seg->modcount,
							  seg->version,
							  seg->state);
			PQfreemem(vpinfo_escaped);

			segno = seg->segno;
		}

		executeLargeCommandOrDie(conn, query->data);
	}

	/* Restore the entries in the AO visimap table. */
	executeQueryOrDie(conn, "TRUNCATE pg_aoseg.%s", vmaprelname);

	for (i = 0; i < rel->naovisimaps; i++)
	{
		AOVisiMapInfo  *seg = &rel->aovisimaps[i];
		char	   *visimap_escaped;

		resetPQExpBuffer(query);

		visimap_escaped = PQescapeLiteral(conn, seg->visimap, strlen(seg->visimap));
		if (visimap_escaped == NULL)
			pg_log(PG_FATAL, "%s: out of memory\n", os_info.progname);

		appendPQExpBuffer(query,
						  "INSERT INTO pg_aoseg.%s "
						  "( "
						  "		segno, first_row_no, visimap "
						  ") "
						  "VALUES (%d, " INT64_FORMAT ", %s)",
						  vmaprelname,
						  seg->segno,
						  seg->first_row_no,
						  visimap_escaped);
		PQfreemem(visimap_escaped);

		executeLargeCommandOrDie(conn, query->data);
	}

	/* Restore the entries in the AO blkdir table. */
	if (blkdirrelname)
		executeQueryOrDie(conn, "TRUNCATE pg_aoseg.%s", blkdirrelname);

	for (i = 0; i < rel->naoblkdirs && blkdirrelname; i++)
	{
		AOBlkDir   *seg = &rel->aoblkdirs[i];
		char	   *minipage_escaped;

		resetPQExpBuffer(query);

		minipage_escaped = PQescapeLiteral(conn, seg->minipage, strlen(seg->minipage));
		if (minipage_escaped == NULL)
			pg_log(PG_FATAL, "%s: out of memory\n", os_info.progname);

		appendPQExpBuffer(query,
						  "INSERT INTO pg_aoseg.%s "
						  "( "
						  "		segno, columngroup_no, first_row_no, minipage "
						  ") "
						  "VALUES (%d, %d, " INT64_FORMAT ", %s)",
						  blkdirrelname,
						  seg->segno,
						  seg->columngroup_no,
						  seg->first_row_no,
						  minipage_escaped);
		PQfreemem(minipage_escaped);

		executeLargeCommandOrDie(conn, query->data);
	}

	pg_free(segrelname);
	pg_free(vmaprelname);
	if (blkdirrelname)
		pg_free(blkdirrelname);
	destroyPQExpBuffer(query);
}

void
restore_aosegment_tables(void)
{
	int			dbnum;

	prep_status("Restoring append-only auxiliary tables in new cluster");

	/*
	 * Rebuilding AO auxiliary tables can potentially take some time in a large
	 * cluster so swap out the current progress file before starting so that
	 * the user can see what's going on.
	 */
	report_progress(&new_cluster, FIXUP, "Rebuilding AO auxiliary tables");
	close_progress();

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&new_cluster, olddb->db_name);
		int			relnum;

		/*
		 * GPDB doesn't allow hacking the catalogs without setting
		 * allow_system_table_mods first.
		 */
		PQclear(executeQueryOrDie(conn, "set allow_system_table_mods=true"));

		for (relnum = 0; relnum < olddb->rel_arr.nrels; relnum++)
		{
			RelInfo		*rel = &olddb->rel_arr.rels[relnum];

			if (is_appendonly(rel->relstorage))
				restore_aosegment_table(conn, rel);
		}

		PQfinish(conn);
	}

	check_ok();
}

bool
is_appendonly(char relstorage)
{
	return relstorage == RELSTORAGE_AOROWS || relstorage == RELSTORAGE_AOCOLS;
}
