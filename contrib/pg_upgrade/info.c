/*
 *	info.c
 *
 *	information support functions
 *
 *	Portions Copyright (c) 2016, Pivotal Software Inc
 *	Portions Copyright (c) 2010, PostgreSQL Global Development Group
 *	$PostgreSQL: pgsql/contrib/pg_upgrade/info.c,v 1.11 2010/07/06 19:18:55 momjian Exp $
 */

#include "pg_upgrade.h"

#include "access/transam.h"
#include "catalog/pg_class.h"

static void get_db_infos(migratorContext *ctx, DbInfoArr *dbinfos,
			 Cluster whichCluster);
static void dbarr_print(migratorContext *ctx, DbInfoArr *arr,
			Cluster whichCluster);
static void relarr_print(migratorContext *ctx, RelInfoArr *arr);
static void get_rel_infos(migratorContext *ctx, const DbInfo *dbinfo,
			  RelInfoArr *relarr, Cluster whichCluster);
static void relarr_free(RelInfoArr *rel_arr);
static void map_rel(migratorContext *ctx, const RelInfo *oldrel,
		const RelInfo *newrel, const DbInfo *old_db,
		const DbInfo *new_db, const char *olddata,
		const char *newdata, FileNameMap *map, RelType reltype);
static void map_rel_by_id(migratorContext *ctx, Oid oldid, Oid newid,
			  const char *old_nspname, const char *old_relname,
			  const char *new_nspname, const char *new_relname,
			  const char *old_tablespace, const char *new_tablespace, const DbInfo *old_db,
			  const DbInfo *new_db, const char *olddata,
			  const char *newdata, FileNameMap *map);

/*
 * gen_db_file_maps()
 *
 * generates database mappings for "old_db" and "new_db". Returns a malloc'ed
 * array of mappings. nmaps is a return parameter which refers to the number
 * mappings.
 *
 * NOTE: Its the Caller's responsibility to free the returned array.
 */
FileNameMap *
gen_db_file_maps(migratorContext *ctx, DbInfo *old_db, DbInfo *new_db,
				 int *nmaps, const char *old_pgdata, const char *new_pgdata)
{
	FileNameMap *maps;
	int			num_maps = 0;
	int			new_relnum;
	int			old_relnum;
	bool		all_matched = true;

	/*
	 * If we in debug mode have a different number of relations in the new and
	 * old cluster, print a list of the relations we've found to aid debugging.
	 */
	if (ctx->debug && (old_db->rel_arr.nrels != new_db->rel_arr.nrels))
	{
		pg_log(ctx, PG_WARNING, "Different number of relations found; old: %d, new: %s\n",
			   old_db->rel_arr.nrels, new_db->rel_arr.nrels);

		for (new_relnum = 0; new_relnum < new_db->rel_arr.nrels; new_relnum++)
			pg_log(ctx, PG_WARNING, "new: %s\n",
				   (new_db->rel_arr.rels[new_relnum]).relname);
		for (old_relnum = 0; old_relnum < old_db->rel_arr.nrels; old_relnum++)
			pg_log(ctx, PG_WARNING, "old: %s\n",
				   (old_db->rel_arr.rels[old_relnum]).relname);
	}

	maps = (FileNameMap *) pg_malloc(ctx, sizeof(FileNameMap) *
									 new_db->rel_arr.nrels);

	old_relnum = new_relnum = 0;
	while (old_relnum < old_db->rel_arr.nrels ||
		   new_relnum < new_db->rel_arr.nrels)
	{
		RelInfo    *old_rel = (old_relnum < old_db->rel_arr.nrels) ?
		&old_db->rel_arr.rels[old_relnum] : NULL;
		RelInfo    *new_rel = (new_relnum < new_db->rel_arr.nrels) ?
		&new_db->rel_arr.rels[new_relnum] : NULL;
		RelType		reltype;

		/* check for mismatched OID */
		if (old_rel->reloid < new_rel->reloid)
		{
			if (strcmp(old_rel->nspname, "pg_toast") != 0)
			{
				pg_log(ctx, PG_WARNING, "Mismatch of relation id: database \"%s\", old relid %d (%s), new relid %d (%s)\n",
					   old_db->db_name, old_rel->reloid, old_rel->relname, new_rel->reloid, new_rel->relname);
			}
			old_relnum++;
			continue;
		}
		else if (old_rel->reloid > new_rel->reloid)
		{
			if (strcmp(new_rel->nspname, "pg_toast") != 0)
			{
				pg_log(ctx, PG_WARNING, "Mismatch of relation id: database \"%s\", old relid %d (%s), new relid %d (%s)\n",
					   old_db->db_name, old_rel->reloid, old_rel->relname, new_rel->reloid, new_rel->relname);
			}
			new_relnum++;
			continue;
		}

		/*
		 * Verify that rels of same OID have same name.  The namespace name
		 * should always match, but the relname might not match for TOAST
		 * tables (and, therefore, their indexes).
		 */
		if (strcmp(old_rel->nspname, new_rel->nspname) != 0 ||
			(strcmp(old_rel->relname, new_rel->relname) != 0 &&
			 strcmp(old_rel->nspname, "pg_toast") != 0))
		{
			pg_log(ctx, PG_WARNING, "Relation names for OID %u in database \"%s\" do not match: "
				   "old name \"%s.%s\", new name \"%s.%s\"\n",
				   old_rel->reloid, old_db->db_name,
				   old_rel->nspname, old_rel->relname,
				   new_rel->nspname, new_rel->relname);
			all_matched = false;
			old_relnum++;
			new_relnum++;
			continue;
		}

		/*
		 * External tables have relfilenodes but no physical files, and aoseg
		 * tables are handled by their AO table
		 */
		if ((old_rel->relstorage == 'x') || (strcmp(new_rel->nspname, "pg_aoseg") == 0))
		{
			old_relnum++;
			new_relnum++;
			continue;
		}

		if (old_rel->aosegments != NULL)
			reltype = AO;
		else if (old_rel->aocssegments != NULL)
			reltype = AOCS;
		else
			reltype = HEAP;

		map_rel(ctx, old_rel, new_rel, old_db, new_db, old_pgdata, new_pgdata,
				maps + num_maps, reltype);

		num_maps++;
		old_relnum++;
		new_relnum++;
	}

	if (!all_matched)
		pg_log(ctx, PG_FATAL, "Failed to match up old and new tables in database \"%s\"\n",
				 old_db->db_name);

	*nmaps = num_maps;
	return maps;
}


static void
map_rel(migratorContext *ctx, const RelInfo *oldrel, const RelInfo *newrel,
		const DbInfo *old_db, const DbInfo *new_db, const char *olddata,
		const char *newdata, FileNameMap *map, RelType reltype)
{
	map_rel_by_id(ctx, oldrel->relfilenode, newrel->relfilenode, oldrel->nspname,
				  oldrel->relname, newrel->nspname, newrel->relname, oldrel->tablespace, newrel->tablespace, old_db,
				  new_db, olddata, newdata, map);

	map->has_numerics = oldrel->has_numerics;
	map->atts = oldrel->atts;
	map->natts = oldrel->natts;
	map->gpdb4_heap_conversion_needed = oldrel->gpdb4_heap_conversion_needed;
	map->type = reltype;

	/* An AO table doesn't necessarily have segment 0 at all. */
	map->missing_seg0_ok = relstorage_is_ao(oldrel->relstorage);
}


/*
 * map_rel_by_id()
 *
 * fills a file node map structure and returns it in "map".
 */
static void
map_rel_by_id(migratorContext *ctx, Oid oldid, Oid newid,
			  const char *old_nspname, const char *old_relname,
			  const char *new_nspname, const char *new_relname,
			  const char *old_tablespace, const char *new_tablespace, const DbInfo *old_db,
			  const DbInfo *new_db, const char *olddata,
			  const char *newdata, FileNameMap *map)
{
	map->new = newid;
	map->old = oldid;

	snprintf(map->old_nspname, sizeof(map->old_nspname), "%s", old_nspname);
	snprintf(map->old_relname, sizeof(map->old_relname), "%s", old_relname);
	snprintf(map->new_nspname, sizeof(map->new_nspname), "%s", new_nspname);
	snprintf(map->new_relname, sizeof(map->new_relname), "%s", new_relname);

	/* In case old/new tablespaces don't match, do them separately. */
	if (strlen(old_tablespace) == 0)
	{
		/*
		 * relation belongs to the default tablespace, hence relfiles would
		 * exist in the data directories.
		 */
		snprintf(map->old_file, sizeof(map->old_file), "%s/base/%u", olddata, old_db->db_oid);
	}
	else
	{
		/*
		 * relation belongs to some tablespace, hence copy its physical
		 * location
		 */
		snprintf(map->old_file, sizeof(map->old_file), "%s%s/%u", old_tablespace,
				 ctx->old.tablespace_suffix, old_db->db_oid);
	}

	/* Do the same for new tablespaces */
	if (strlen(new_tablespace) == 0)
	{
		/*
		 * relation belongs to the default tablespace, hence relfiles would
		 * exist in the data directories.
		 */
		snprintf(map->new_file, sizeof(map->new_file), "%s/base/%u", newdata, new_db->db_oid);
	}
	else
	{
		/*
		 * relation belongs to some tablespace, hence copy its physical
		 * location
		 */
		snprintf(map->new_file, sizeof(map->new_file), "%s%s/%u", new_tablespace,
				 ctx->new.tablespace_suffix, new_db->db_oid);
	}

	report_progress(ctx, NONE, FILE_MAP, "Map \"%s\" to \"%s\"", map->old_file, map->new_file);
}


void
print_maps(migratorContext *ctx, FileNameMap *maps, int n, const char *dbName)
{
	if (ctx->debug)
	{
		int			mapnum;

		pg_log(ctx, PG_DEBUG, "mappings for db %s:\n", dbName);

		for (mapnum = 0; mapnum < n; mapnum++)
			pg_log(ctx, PG_DEBUG, "%s.%s:%u ==> %s.%s:%u\n",
				   maps[mapnum].old_nspname, maps[mapnum].old_relname, maps[mapnum].old,
				   maps[mapnum].new_nspname, maps[mapnum].new_relname, maps[mapnum].new);

		pg_log(ctx, PG_DEBUG, "\n\n");
	}
}


/*
 * get_db_infos()
 *
 * Scans pg_database system catalog and returns (in dbinfs_arr) all user
 * databases.
 */
static void
get_db_infos(migratorContext *ctx, DbInfoArr *dbinfs_arr, Cluster whichCluster)
{
	PGconn	   *conn = connectToServer(ctx, "template1", whichCluster);
	PGresult   *res;
	int			ntups;
	int			tupnum;
	DbInfo	   *dbinfos;
	int			i_datname;
	int			i_oid;
	int			i_spclocation;

	res = executeQueryOrDie(ctx, conn,
							"SELECT d.oid, d.datname, t.spclocation "
							"FROM pg_catalog.pg_database d "
							" LEFT OUTER JOIN pg_catalog.pg_tablespace t "
							" ON d.dattablespace = t.oid "
							"WHERE d.datallowconn = true "
	/* we don't preserve pg_database.oid so we sort by name */
							"ORDER BY 2");

	i_datname = PQfnumber(res, "datname");
	i_oid = PQfnumber(res, "oid");
	i_spclocation = PQfnumber(res, "spclocation");

	ntups = PQntuples(res);
	dbinfos = (DbInfo *) pg_malloc(ctx, sizeof(DbInfo) * ntups);

	for (tupnum = 0; tupnum < ntups; tupnum++)
	{
		dbinfos[tupnum].db_oid = atooid(PQgetvalue(res, tupnum, i_oid));

		snprintf(dbinfos[tupnum].db_name, sizeof(dbinfos[tupnum].db_name), "%s",
				 PQgetvalue(res, tupnum, i_datname));
		snprintf(dbinfos[tupnum].db_tblspace, sizeof(dbinfos[tupnum].db_tblspace), "%s",
				 PQgetvalue(res, tupnum, i_spclocation));
	}
	PQclear(res);

	PQfinish(conn);

	dbinfs_arr->dbs = dbinfos;
	dbinfs_arr->ndbs = ntups;
}


/*
 * get_db_and_rel_infos()
 *
 * higher level routine to generate dbinfos for the database running
 * on the given "port". Assumes that server is already running.
 */
void
get_db_and_rel_infos(migratorContext *ctx, DbInfoArr *db_arr, Cluster whichCluster)
{
	int			dbnum;

	get_db_infos(ctx, db_arr, whichCluster);

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
		get_rel_infos(ctx, &db_arr->dbs[dbnum],
					  &(db_arr->dbs[dbnum].rel_arr), whichCluster);

	if (ctx->debug)
		dbarr_print(ctx, db_arr, whichCluster);
}

/*
 * get_numeric_types()
 *
 * queries the cluster for all types based on NUMERIC, as well as NUMERIC
 * itself, and return an InvalidOid terminated array of pg_type Oids for
 * these types.
 */
static Oid *
get_numeric_types(migratorContext *ctx, PGconn *conn)
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
	result = pg_malloc(ctx, sizeof(Oid) * NUMERIC_ALLOC);
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

		res = executeQueryOrDie(ctx, conn, query);

		if (PQntuples(res) > 0)
		{
			result[result_count++] = atooid(PQgetvalue(res, 0, PQfnumber(res, "typoid")));
			result[result_count++] = atooid(PQgetvalue(res, 0, PQfnumber(res, "baseoid")));
		}

		PQclear(res);

		if (result_count == NUMERIC_ALLOC - 1)
			pg_log(ctx, PG_FATAL, "Too many NUMERIC types found");
	}

	return result;
}

/*
 * get_rel_infos()
 *
 * gets the relinfos for all the user tables of the database refered
 * by "db".
 *
 * NOTE: we assume that relations/entities with oids greater than
 * FirstNormalObjectId belongs to the user
 */
static void
get_rel_infos(migratorContext *ctx, const DbInfo *dbinfo,
			  RelInfoArr *relarr, Cluster whichCluster)
{
	PGconn	   *conn = connectToServer(ctx, dbinfo->db_name, whichCluster);
	PGresult   *res;
	RelInfo    *relinfos;
	int			ntups;
	int			relnum;
	int			num_rels = 0;
	char	   *nspname = NULL;
	char	   *relname = NULL;
	char		relstorage;
	char		relkind;
	int			i_spclocation = -1;
	int			i_nspname = -1;
	int			i_relname = -1;
	int			i_relstorage = -1;
	int			i_relkind = -1;
	int			i_oid = -1;
	int			i_relfilenode = -1;
	int			i_reltablespace = -1;
	int			i_reltoastrelid = -1;
	int			i_segrelid = -1;
	char		query[QUERY_ALLOC];
	bool		bitmaphack_created = false;
	Oid		   *numeric_types;
	Oid		   *numeric_rels = NULL;
	int			numeric_rel_num = 0;
	char		typestr[QUERY_ALLOC];
	int			i;

	/*
	 * We need to extract extra information on all relations which contain
	 * NUMERIC attributes, or attributes of types which are based on NUMERIC.
	 * In order to limit the process to just those tables, first get the set
	 * of pg_type Oids types based on NUMERIC as well as NUMERIC itself, then
	 * find all relations which has these types and limit the extraction to
	 * those.
	 */
	numeric_types = get_numeric_types(ctx, conn);
	memset(typestr, '\0', sizeof(typestr));

	for (i = 0; numeric_types[i] != InvalidOid; i++)
	{
		int len = 0;

		if (i > 0)
			len = strlen(typestr);

		snprintf(typestr + len, sizeof(typestr) - len, "%s%u", (i > 0 ? "," : ""), numeric_types[i]);
	}

	/* typestr can't be NULL since we always have at least one type */
	snprintf(query, sizeof(query) - strlen(typestr),
			 "SELECT DISTINCT attrelid "
			 "FROM pg_attribute "
			 "WHERE atttypid in (%s) "
			 "AND attnum >= 1 "
			 "AND attisdropped = false "
			 "ORDER BY 1 ASC", typestr);

	res = executeQueryOrDie(ctx, conn, query);
	ntups = PQntuples(res);

	/* Store the relations in a simple ordered array for lookup */
	if (ntups > 0)
	{
		numeric_rel_num = ntups;

		i_oid = PQfnumber(res, "attrelid");
		numeric_rels = pg_malloc(ctx, ntups * sizeof(Oid));

		for (relnum = 0; relnum < ntups; relnum++)
			numeric_rels[relnum] = atooid(PQgetvalue(res, relnum, i_oid));
	}

	PQclear(res);

	/*
	 * pg_largeobject contains user data that does not appear the pg_dumpall
	 * --schema-only output, so we have to migrate that system table heap and
	 * index.  Ideally we could just get the relfilenode from template1 but
	 * pg_largeobject_loid_pn_index's relfilenode can change if the table was
	 * reindexed so we get the relfilenode for each database and migrate it as
	 * a normal user table.
	 */

	snprintf(query, sizeof(query),
			 "SELECT DISTINCT c.oid, n.nspname, c.relname, c.relstorage, c.relkind, "
			 "	c.relfilenode, c.reltoastrelid, c.reltablespace, t.spclocation "
			 "FROM pg_catalog.pg_class c JOIN "
			 "		pg_catalog.pg_namespace n "
			 "	ON c.relnamespace = n.oid "
			 "  LEFT OUTER JOIN pg_catalog.pg_index i "
			 "	   ON c.oid = i.indexrelid "
			 "   LEFT OUTER JOIN pg_catalog.pg_tablespace t "
			 "	ON c.reltablespace = t.oid "
			 "WHERE "
			 /* exclude possible orphaned temp tables */
			 "  (((n.nspname !~ '^pg_temp_' AND "
			 "    n.nspname !~ '^pg_toast_temp_' AND "
			 "    n.nspname NOT IN ('gp_toolkit', 'pg_catalog', 'information_schema', 'binary_upgrade', 'pg_aoseg', 'pg_bitmapindex') AND "
			 "    c.relkind <> 'i' AND " // GPDB TODO: Indexes not supported
			 "	  c.oid >= %u) "
			 "	) OR ( "
			 "	n.nspname = 'pg_catalog' "
			 "	AND relname IN "
			 "        ('pg_largeobject', 'pg_largeobject_loid_pn_index'%s) )) "
			 "	AND relkind IN ('r','t','o','m','b', 'i'%s) "
	/* work around what appears to be old 4.3 gp_toolkit bugs */
			 "  AND relname NOT IN ('__gp_localid', '__gp_masterid', '__gp_log_segment_ext', '__gp_log_master_ext', 'gp_disk_free') "
			 /* pg_dump only dumps valid indexes;  testing indisready is
			 * necessary in 9.2, and harmless in earlier/later versions. */
			 /* GPDB 4.3 (based on PostgreSQL 8.2), however, doesn't have indisvalid
			  * nor indisready. */
			 " %s "
			 "GROUP BY  c.oid, n.nspname, c.relname, c.relfilenode, c.relstorage, c.relkind, "
			 "			c.reltoastrelid, c.reltablespace, t.spclocation, "
			 "			n.nspname "
	/* we preserve pg_class.oid so we sort by it to match old/new */
			 "ORDER BY 1;",
			 FirstNormalObjectId,
	/* does pg_largeobject_metadata need to be migrated? */
			 (GET_MAJOR_VERSION(ctx->old.major_version) <= 804) ?
			 "" : ", 'pg_largeobject_metadata', 'pg_largeobject_metadata_oid_index'",
	/* see the comment at the top of old_8_3_create_sequence_script() */
			 (GET_MAJOR_VERSION(ctx->old.major_version) <= 803) ?
			 "" : ", 'S'",
			 (GET_MAJOR_VERSION(ctx->old.major_version) <= 802) ?
			 "" : " AND i.indisvalid IS DISTINCT FROM false AND i.indisready IS DISTINCT FROM false "
		);

	res = executeQueryOrDie(ctx, conn, query);

	ntups = PQntuples(res);

	relinfos = (RelInfo *) pg_malloc(ctx, sizeof(RelInfo) * ntups);

	i_oid = PQfnumber(res, "oid");
	i_nspname = PQfnumber(res, "nspname");
	i_relname = PQfnumber(res, "relname");
	i_relstorage = PQfnumber(res, "relstorage");
	i_relkind = PQfnumber(res, "relkind");
	i_relfilenode = PQfnumber(res, "relfilenode");
	i_reltoastrelid = PQfnumber(res, "reltoastrelid");
	i_reltablespace = PQfnumber(res, "reltablespace");
	i_spclocation = PQfnumber(res, "spclocation");
	i_segrelid = PQfnumber(res, "segrelid");

	for (relnum = 0; relnum < ntups; relnum++)
	{
		RelInfo    *curr = &relinfos[num_rels++];
		const char *tblspace;

		curr->gpdb4_heap_conversion_needed = false;
		curr->reloid = atooid(PQgetvalue(res, relnum, i_oid));

		nspname = PQgetvalue(res, relnum, i_nspname);
		strlcpy(curr->nspname, nspname, sizeof(curr->nspname));

		relname = PQgetvalue(res, relnum, i_relname);
		strlcpy(curr->relname, relname, sizeof(curr->relname));

		curr->relfilenode = atooid(PQgetvalue(res, relnum, i_relfilenode));
		curr->toastrelid = atooid(PQgetvalue(res, relnum, i_reltoastrelid));

		if (atooid(PQgetvalue(res, relnum, i_reltablespace)) != 0)
			/* Might be "", meaning the cluster default location. */
			tblspace = PQgetvalue(res, relnum, i_spclocation);
		else
			/* A zero reltablespace indicates the database tablespace. */
			tblspace = dbinfo->db_tblspace;

		strlcpy(curr->tablespace, tblspace, sizeof(curr->tablespace));

		/* Collect extra information about append-only tables */
		relstorage = PQgetvalue(res, relnum, i_relstorage) [0];
		curr->relstorage = relstorage;

		relkind = PQgetvalue(res, relnum, i_relkind) [0];

		/*
		 * RELSTORAGE_AOROWS and RELSTORAGE_AOCOLS. The structure of append
		 * optimized tables is similar enough for row and column oriented
		 * tables so we can handle them both here.
		 */
		if (relstorage == 'a' || relstorage == 'c')
		{
			char		aoquery[QUERY_ALLOC];
			char	   *segrel;
			char	   *visimaprel;
			char	   *blkdirrel = NULL;
			PGresult   *aores;
			int			j;

			/*
			 * First query the catalog for the auxiliary heap relations which
			 * describe AO{CS} relations. The segrel and visimap must exist
			 * but the blkdirrel is created when required so it might not
			 * exist.
			 *
			 * We don't dump the block directory, even if it exists, if the table
			 * doesn't have any indexes. This isn't just an optimization: restoring
			 * it wouldn't work, because without indexes, restore won't create
			 * a block directory in the new cluster.
			 */
			snprintf(aoquery, sizeof(aoquery),
					 "SELECT cs.relname AS segrel, "
					 "       cv.relname AS visimaprel, "
					 "       cb.relname AS blkdirrel "
					 "FROM   pg_appendonly a "
					 "       JOIN pg_class cs on (cs.oid = a.segrelid) "
					 "       JOIN pg_class cv on (cv.oid = a.visimaprelid) "
					 "       LEFT JOIN pg_class cb on (cb.oid = a.blkdirrelid "
					 "                                 AND a.blkdirrelid <> 0 "
					 "                                 AND EXISTS (SELECT 1 FROM pg_index i WHERE i.indrelid = a.relid)) "
					 "WHERE  a.relid = %u::pg_catalog.oid ",
					 curr->reloid);

			aores = executeQueryOrDie(ctx, conn, aoquery);
			if (PQntuples(aores) == 0)
				pg_log(ctx, PG_FATAL, "Unable to find auxiliary AO relations for %u (%s)\n",
					   curr->reloid, curr->relname);

			segrel = pg_strdup(ctx, PQgetvalue(aores, 0, PQfnumber(aores, "segrel")));
			visimaprel = pg_strdup(ctx, PQgetvalue(aores, 0, PQfnumber(aores, "visimaprel")));
			if (!PQgetisnull(aores, 0, PQfnumber(aores, "blkdirrel")))
				blkdirrel = pg_strdup(ctx, PQgetvalue(aores, 0, PQfnumber(aores, "blkdirrel")));

			PQclear(aores);

			if (relstorage == 'a')
			{
				/* Get contents of pg_aoseg_<oid> */

				/*
				 * In GPDB 4.3, the append only file format version number was the
				 * same for all segments, and was stored in pg_appendonly. In 5.0
				 * and above, it can be different for each segment, and it's stored
				 * in the aosegment relation.
				 */
				if (GET_MAJOR_VERSION(ctx->old.major_version) <= 802 && whichCluster == CLUSTER_OLD)
				{
					snprintf(aoquery, sizeof(aoquery),
							 "SELECT segno, eof, tupcount, varblockcount, "
							 "       eofuncompressed, modcount, state, "
							 "       ao.version as formatversion "
							 "FROM   pg_aoseg.%s, "
							 "       pg_catalog.pg_appendonly ao "
							 "WHERE  ao.relid = %u",
							 segrel, curr->reloid);
				}
				else
				{
					snprintf(aoquery, sizeof(aoquery),
							 "SELECT segno, eof, tupcount, varblockcount, "
							 "       eofuncompressed, modcount, state, "
							 "       formatversion "
							 "FROM   pg_aoseg.%s",
							 segrel);
				}
				aores = executeQueryOrDie(ctx, conn, aoquery);

				curr->naosegments = PQntuples(aores);
				curr->aosegments = (AOSegInfo *) pg_malloc(ctx, sizeof(AOSegInfo) * curr->naosegments);

				for (j = 0; j < curr->naosegments; j++)
				{
					AOSegInfo *aoseg = &curr->aosegments[j];

					aoseg->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
					aoseg->eof = atoll(PQgetvalue(aores, j, PQfnumber(aores, "eof")));
					aoseg->tupcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "tupcount")));
					aoseg->varblockcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "varblockcount")));
					aoseg->eofuncompressed = atoll(PQgetvalue(aores, j, PQfnumber(aores, "eofuncompressed")));
					aoseg->modcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "modcount")));
					aoseg->state = atoi(PQgetvalue(aores, j, PQfnumber(aores, "state")));
					aoseg->version = atoi(PQgetvalue(aores, j, PQfnumber(aores, "formatversion")));
				}

				PQclear(aores);
			}
			else
			{
				/* Get contents of pg_aocsseg_<oid> */
				if (GET_MAJOR_VERSION(ctx->old.major_version) <= 802 && whichCluster == CLUSTER_OLD)
				{
					snprintf(aoquery, sizeof(aoquery),
							 "SELECT segno, tupcount, varblockcount, vpinfo, "
							 "       modcount, state, ao.version as formatversion "
							 "FROM   pg_aoseg.%s, "
							 "       pg_catalog.pg_appendonly ao "
							 "WHERE  ao.relid = %u",
							 segrel, curr->reloid);
				}
				else
				{
					snprintf(aoquery, sizeof(aoquery),
							 "SELECT segno, tupcount, varblockcount, vpinfo, "
							 "       modcount, formatversion, state "
							 "FROM   pg_aoseg.%s",
							 segrel);
				}

				aores = executeQueryOrDie(ctx, conn, aoquery);

				curr->naosegments = PQntuples(aores);
				curr->aocssegments = (AOCSSegInfo *) pg_malloc(ctx, sizeof(AOCSSegInfo) * curr->naosegments);

				for (j = 0; j < curr->naosegments; j++)
				{
					AOCSSegInfo *aocsseg = &curr->aocssegments[j];

					aocsseg->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
					aocsseg->tupcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "tupcount")));
					aocsseg->varblockcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "varblockcount")));
					aocsseg->vpinfo = pg_strdup(ctx, PQgetvalue(aores, j, PQfnumber(aores, "vpinfo")));
					aocsseg->modcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "modcount")));
					aocsseg->state = atoi(PQgetvalue(aores, j, PQfnumber(aores, "state")));
					aocsseg->version = atoi(PQgetvalue(aores, j, PQfnumber(aores, "formatversion")));
				}

				PQclear(aores);
			}

			/*
			 * Get contents of the auxiliary pg_aovisimap_<oid> relation.  In
			 * GPDB 4.3, the pg_aovisimap_<oid>.visimap field was of type "bit
			 * varying", but we didn't actually store a valid "varbit" datum in
			 * it. Because of that, we won't get the valid data out by calling
			 * the varbit output function on it.  Create a little function to
			 * blurp out its content as a bytea instead. in 5.0 and above, the
			 * datatype is also nominally a bytea.
			 *
			 * pg_aovisimap_<oid> is identical for row and column oriented
			 * tables.
			 */
			if (GET_MAJOR_VERSION(ctx->old.major_version) <= 802 && whichCluster == CLUSTER_OLD)
			{
				if (!bitmaphack_created)
				{
					PQclear(executeQueryOrDie(ctx, conn,
											  "create function pg_temp.bitmaphack_out(bit varying) "
											  " RETURNS cstring "
											  " LANGUAGE INTERNAL AS 'byteaout'"));
					bitmaphack_created = true;
				}
				snprintf(aoquery, sizeof(aoquery),
						 "SELECT segno, first_row_no, pg_temp.bitmaphack_out(visimap::bit varying) as visimap "
						 "FROM pg_aoseg.%s",
						 visimaprel);
			}
			else
			{
				snprintf(aoquery, sizeof(aoquery),
						 "SELECT segno, first_row_no, visimap "
						 "FROM pg_aoseg.%s",
						 visimaprel);
			}
			aores = executeQueryOrDie(ctx, conn, aoquery);

			curr->naovisimaps = PQntuples(aores);
			curr->aovisimaps = (AOVisiMapInfo *) pg_malloc(ctx, sizeof(AOVisiMapInfo) * curr->naovisimaps);

			for (j = 0; j < curr->naovisimaps; j++)
			{
				AOVisiMapInfo *aovisimap = &curr->aovisimaps[j];

				aovisimap->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
				aovisimap->first_row_no = atoll(PQgetvalue(aores, j, PQfnumber(aores, "first_row_no")));
				aovisimap->visimap = pg_strdup(ctx, PQgetvalue(aores, j, PQfnumber(aores, "visimap")));
			}

			PQclear(aores);

			/*
			 * Get contents of pg_aoblkdir_<oid>. If pg_appendonly.blkdirrelid
			 * is InvalidOid then there is no blkdir table. Like the visimap
			 * field in pg_aovisimap_<oid>, the minipage field was of type "bit
			 * varying" but didn't store a valid "varbi" datum. We use the same
			 * function to extract the content as a bytea as we did for the
			 * visimap. The datatype has been changed to bytea in 5.0.
			 */
			if (blkdirrel)
			{
				if (GET_MAJOR_VERSION(ctx->old.major_version) <= 802 && whichCluster == CLUSTER_OLD)
				{
					if (!bitmaphack_created)
					{
						PQclear(executeQueryOrDie(ctx, conn,
												  "create function pg_temp.bitmaphack_out(bit varying) "
												  " RETURNS cstring "
												  " LANGUAGE INTERNAL AS 'byteaout'"));
						bitmaphack_created = true;
					}
					snprintf(aoquery, sizeof(aoquery),
							 "SELECT segno, columngroup_no, first_row_no, "
							 "       pg_temp.bitmaphack_out(minipage::bit varying) AS minipage "
							 "FROM   pg_aoseg.%s",
							 blkdirrel);
				}
				else
				{
					snprintf(aoquery, sizeof(aoquery),
							 "SELECT segno, columngroup_no, first_row_no, minipage "
							 "FROM pg_aoseg.%s",
							 blkdirrel);
				}

				aores = executeQueryOrDie(ctx, conn, aoquery);

				curr->naoblkdirs = PQntuples(aores);
				curr->aoblkdirs = (AOBlkDir *) pg_malloc(ctx, sizeof(AOBlkDir) * curr->naoblkdirs);

				for (j = 0; j < curr->naoblkdirs; j++)
				{
					AOBlkDir *aoblkdir = &curr->aoblkdirs[j];

					aoblkdir->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
					aoblkdir->columngroup_no = atoi(PQgetvalue(aores, j, PQfnumber(aores, "columngroup_no")));
					aoblkdir->first_row_no = atoll(PQgetvalue(aores, j, PQfnumber(aores, "first_row_no")));
					aoblkdir->minipage = pg_strdup(ctx, PQgetvalue(aores, j, PQfnumber(aores, "minipage")));
				}

				PQclear(aores);
			}
			else
			{
				curr->aoblkdirs = NULL;
				curr->naoblkdirs = 0;
			}

			pg_free(segrel);
			pg_free(visimaprel);
			pg_free(blkdirrel);
		}
		else
		{
			curr->aosegments = NULL;
			curr->aocssegments = NULL;
			curr->naosegments = 0;
			curr->aovisimaps = NULL;
			curr->naovisimaps = 0;
			curr->naoblkdirs = 0;
			curr->aoblkdirs = NULL;
		}

		if (relstorage == 'h' && /* RELSTORAGE_HEAP */
			(relkind == 'r' || relkind == 't')) /* RELKIND_RELATION or RELKIND_TOASTVALUE */
		{
			char		hquery[QUERY_ALLOC];
			PGresult   *hres;
			int			j;
			int			i_attlen;
			int			i_attalign;
			int			i_atttypid;
			int			i_typbasetype;
			bool		found = false;

			/*
			 * Find out if the curr->reloid in the list of numeric attribute
			 * relations and only if found perform the below extra query
			 */
			if (numeric_rel_num > 0 && numeric_rels
				&& curr->reloid >= numeric_rels[0] && curr->reloid <= numeric_rels[numeric_rel_num - 1])
			{
				for (j = 0; j < numeric_rel_num; j++)
				{
					if (numeric_rels[j] == curr->reloid)
					{
						found = true;
						break;
					}

					if (numeric_rels[j] > curr->reloid)
						break;
				}
			}

			if (found)
			{
				/*
				 * The relation has a numeric attribute, get information
				 * about numeric columns from pg_attribute.
				 */
				snprintf(hquery, sizeof(hquery),
						 "SELECT a.attnum, a.attlen, a.attalign, a.atttypid, t.typbasetype "
						 "FROM pg_attribute a, pg_type t "
						 "WHERE a.attrelid = %u "
						 "AND a.atttypid = t.oid "
						 "AND a.attnum >= 1 "
						 "AND a.attisdropped = false "
						 "ORDER BY attnum",
						 curr->reloid);

				hres = executeQueryOrDie(ctx, conn, hquery);
				i_attlen = PQfnumber(hres, "attlen");
				i_attalign = PQfnumber(hres, "attalign");
				i_atttypid = PQfnumber(hres, "atttypid");
				i_typbasetype = PQfnumber(hres, "typbasetype");

				curr->natts = PQntuples(hres);
				curr->atts = (AttInfo *) pg_malloc(ctx, sizeof(AttInfo) * curr->natts);
				memset(curr->atts, 0, sizeof(AttInfo) * curr->natts);

				for (j = 0; j < PQntuples(hres); j++)
				{
					Oid			typid =  atooid(PQgetvalue(hres, j, i_atttypid));
					Oid			typbasetype =  atooid(PQgetvalue(hres, j, i_typbasetype));

					curr->atts[j].attlen = atoi(PQgetvalue(hres, j, i_attlen));
					curr->atts[j].attalign = PQgetvalue(hres, j, i_attalign)[0];
					for (i = 0; numeric_types[i] != InvalidOid; i++)
					{
						if (numeric_types[i] == typid || numeric_types[i] == typbasetype)
						{
							curr->has_numerics = true;
							curr->atts[j].is_numeric = true;
							break;
						}
					}
				}

				PQclear(hres);
			}

			/*
			 * Regardless of if there is a NUMERIC attribute there is a
			 * conversion needed to fix the headers of heap pages if the
			 * old cluster is based on PostgreSQL 8.2 or older (Greenplum
			 * 4.3 or older).
			 */
			if (GET_MAJOR_VERSION(ctx->old.major_version) <= 802)
				curr->gpdb4_heap_conversion_needed = true;
		}
		else
			curr->gpdb4_heap_conversion_needed = false;
	}
	PQclear(res);

	PQfinish(conn);

	pg_free(numeric_rels);

	relarr->rels = relinfos;
	relarr->nrels = num_rels;
}


/*
 * dbarr_lookup_db()
 *
 * Returns the pointer to the DbInfo structure
 */
DbInfo *
dbarr_lookup_db(DbInfoArr *db_arr, const char *db_name)
{
	int			dbnum;

	if (!db_arr || !db_name)
		return NULL;

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
	{
		if (strcmp(db_arr->dbs[dbnum].db_name, db_name) == 0)
			return &db_arr->dbs[dbnum];
	}

	return NULL;
}


static void
relarr_free(RelInfoArr *rel_arr)
{
	pg_free(rel_arr->rels);
	rel_arr->nrels = 0;
}


void
dbarr_free(DbInfoArr *db_arr)
{
	int			dbnum;

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
		relarr_free(&db_arr->dbs[dbnum].rel_arr);
	db_arr->ndbs = 0;
}


static void
dbarr_print(migratorContext *ctx, DbInfoArr *arr, Cluster whichCluster)
{
	int			dbnum;

	pg_log(ctx, PG_DEBUG, "%s databases\n", CLUSTERNAME(whichCluster));

	for (dbnum = 0; dbnum < arr->ndbs; dbnum++)
	{
		pg_log(ctx, PG_DEBUG, "Database: %s\n", arr->dbs[dbnum].db_name);
		relarr_print(ctx, &arr->dbs[dbnum].rel_arr);
		pg_log(ctx, PG_DEBUG, "\n\n");
	}
}


static void
relarr_print(migratorContext *ctx, RelInfoArr *arr)
{
	int			relnum;

	for (relnum = 0; relnum < arr->nrels; relnum++)
		pg_log(ctx, PG_DEBUG, "relname: %s.%s: reloid: %u reltblspace: %s\n",
			   arr->rels[relnum].nspname, arr->rels[relnum].relname,
			   arr->rels[relnum].reloid, arr->rels[relnum].tablespace);
}
