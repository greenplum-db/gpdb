/*
 *	info.c
 *
 *	information support functions
 *
 *	Portions Copyright (c) 2016, Pivotal Software Inc
 *	Copyright (c) 2010-2011, PostgreSQL Global Development Group
 *	contrib/pg_upgrade/info.c
 */

#include "pg_upgrade.h"

#include "access/transam.h"
#include "catalog/pg_class.h"

static void create_rel_filename_map(const char *old_data, const char *new_data,
						const DbInfo *old_db, const DbInfo *new_db,
						const RelInfo *old_rel, const RelInfo *new_rel,
						FileNameMap *map, RelType reltype);
static void get_db_infos(ClusterInfo *cluster);
static void get_rel_infos(ClusterInfo *cluster, DbInfo *dbinfo);
static void free_rel_infos(RelInfoArr *rel_arr);
static void print_db_infos(DbInfoArr *dbinfo);
static void print_rel_infos(RelInfoArr *arr);

/*
 * gen_db_file_maps()
 *
 * generates database mappings for "old_db" and "new_db". Returns a malloc'ed
 * array of mappings. nmaps is a return parameter which refers to the number
 * mappings.
 */
FileNameMap *
gen_db_file_maps(DbInfo *old_db, DbInfo *new_db,
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
	if (log_opts.debug && (old_db->rel_arr.nrels != new_db->rel_arr.nrels))
	{
		pg_log(PG_WARNING, "Different number of relations found; old: %d, new: %s\n",
			   old_db->rel_arr.nrels, new_db->rel_arr.nrels);

		for (new_relnum = 0; new_relnum < new_db->rel_arr.nrels; new_relnum++)
			pg_log(PG_WARNING, "new: %s\n",
				   (new_db->rel_arr.rels[new_relnum]).relname);
		for (old_relnum = 0; old_relnum < old_db->rel_arr.nrels; old_relnum++)
			pg_log(PG_WARNING, "old: %s\n",
				   (old_db->rel_arr.rels[old_relnum]).relname);
	}

	maps = (FileNameMap *) pg_malloc(sizeof(FileNameMap) *
									 old_db->rel_arr.nrels);

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
				pg_log(PG_WARNING, "Mismatch of relation id: database \"%s\", old relid %d (%s), new relid %d (%s)\n",
					   old_db->db_name, old_rel->reloid, old_rel->relname, new_rel->reloid, new_rel->relname);
			}
			old_relnum++;
			continue;
		}
		else if (old_rel->reloid > new_rel->reloid)
		{
			if (strcmp(new_rel->nspname, "pg_toast") != 0)
			{
				pg_log(PG_WARNING, "Mismatch of relation id: database \"%s\", old relid %d (%s), new relid %d (%s)\n",
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
			pg_log(PG_WARNING, "Relation names for OID %u in database \"%s\" do not match: "
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

		/* OK, create a mapping entry */
		create_rel_filename_map(old_pgdata, new_pgdata, old_db, new_db,
								old_rel, new_rel, maps + num_maps, reltype);
		num_maps++;
		old_relnum++;
		new_relnum++;
	}

	if (!all_matched)
		pg_log(PG_FATAL, "Failed to match up old and new tables in database \"%s\"\n",
			   old_db->db_name);

	*nmaps = num_maps;
	return maps;
}


/*
 * create_rel_filename_map()
 *
 * fills a file node map structure and returns it in "map".
 */
static void
create_rel_filename_map(const char *old_data, const char *new_data,
						const DbInfo *old_db, const DbInfo *new_db,
						const RelInfo *old_rel, const RelInfo *new_rel,
						FileNameMap *map, RelType reltype)
{
	/* In case old/new tablespaces don't match, do them separately. */
	if (strlen(old_rel->tablespace) == 0)
	{
		/*
		 * relation belongs to the default tablespace, hence relfiles should
		 * exist in the data directories.
		 */
		snprintf(map->old_dir, sizeof(map->old_dir), "%s/base/%u", old_data,
				 old_db->db_oid);
	}
	else
	{
		/* relation belongs to a tablespace, so use the tablespace location */
		snprintf(map->old_dir, sizeof(map->old_dir), "%s%s/%u", old_rel->tablespace,
				 old_cluster.tablespace_suffix, old_db->db_oid);
	}

	/* Do the same for new tablespaces */
	if (strlen(new_rel->tablespace) == 0)
	{
		/*
		 * relation belongs to the default tablespace, hence relfiles should
		 * exist in the data directories.
		 */
		snprintf(map->new_dir, sizeof(map->new_dir), "%s/base/%u", new_data,
				 new_db->db_oid);
	}
	else
	{
		/* relation belongs to a tablespace, so use the tablespace location */
		snprintf(map->new_dir, sizeof(map->new_dir), "%s%s/%u", new_rel->tablespace,
				 new_cluster.tablespace_suffix, new_db->db_oid);
	}

	/*
	 * old_relfilenode might differ from pg_class.oid (and hence
	 * new_relfilenode) because of CLUSTER, REINDEX, or VACUUM FULL.
	 */
	map->old_relfilenode = old_rel->relfilenode;

	/* new_relfilenode will match old and new pg_class.oid */
	map->new_relfilenode = new_rel->relfilenode;

	/* used only for logging and error reporing, old/new are identical */
	snprintf(map->nspname, sizeof(map->nspname), "%s", old_rel->nspname);
	snprintf(map->relname, sizeof(map->relname), "%s", old_rel->relname);

	/* GPDB additions to map data */
	map->has_numerics = old_rel->has_numerics;
	map->atts = old_rel->atts;
	map->natts = old_rel->natts;
	map->gpdb4_heap_conversion_needed = old_rel->gpdb4_heap_conversion_needed;
	map->type = reltype;

	/* An AO table doesn't necessarily have segment 0 at all. */
	map->missing_seg0_ok = relstorage_is_ao(old_rel->relstorage);

	report_progress(NULL, FILE_MAP, "Map \"%s\" to \"%s\"", map->old_dir, map->new_dir);
}


void
print_maps(FileNameMap *maps, int n_maps, const char *db_name)
{
	if (log_opts.debug)
	{
		int			mapnum;

		pg_log(PG_DEBUG, "mappings for db %s:\n", db_name);

		for (mapnum = 0; mapnum < n_maps; mapnum++)
			pg_log(PG_DEBUG, "%s.%s: %u to %u\n",
				   maps[mapnum].nspname, maps[mapnum].relname,
				   maps[mapnum].old_relfilenode,
				   maps[mapnum].new_relfilenode);

		pg_log(PG_DEBUG, "\n\n");
	}
}


/*
 * get_db_and_rel_infos()
 *
 * higher level routine to generate dbinfos for the database running
 * on the given "port". Assumes that server is already running.
 */
void
get_db_and_rel_infos(ClusterInfo *cluster)
{
	int			dbnum;

	if (cluster->dbarr.dbs != NULL)
		free_db_and_rel_infos(&cluster->dbarr);

	get_db_infos(cluster);

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
		get_rel_infos(cluster, &cluster->dbarr.dbs[dbnum]);

	if (log_opts.debug)
	{
		pg_log(PG_DEBUG, "\n%s databases:\n", CLUSTER_NAME(cluster));
		print_db_infos(&cluster->dbarr);
	}
}


/*
 * get_db_infos()
 *
 * Scans pg_database system catalog and populates all user
 * databases.
 */
static void
get_db_infos(ClusterInfo *cluster)
{
	PGconn	   *conn = connectToServer(cluster, "template1");
	PGresult   *res;
	int			ntups;
	int			tupnum;
	DbInfo	   *dbinfos;
	int			i_datname,
				i_oid,
				i_spclocation;
	char		query[QUERY_ALLOC];

	snprintf(query, sizeof(query),
			 "SELECT d.oid, d.datname, %s "
			 "FROM pg_catalog.pg_database d "
			 " LEFT OUTER JOIN pg_catalog.pg_tablespace t "
			 " ON d.dattablespace = t.oid "
			 "WHERE d.datallowconn = true "
	/* we don't preserve pg_database.oid so we sort by name */
			 "ORDER BY 2",
	/*
	 * 9.2 removed the spclocation column in upstream postgres, in GPDB it was
	 * removed in 6.0.0 during then merge of postgres 8.4
	 */
			 (GET_MAJOR_VERSION(old_cluster.major_version) <= 803) ?
			 "t.spclocation" : "pg_catalog.pg_tablespace_location(t.oid) AS spclocation");

	res = executeQueryOrDie(conn, "%s", query);

	i_oid = PQfnumber(res, "oid");
	i_datname = PQfnumber(res, "datname");
	i_spclocation = PQfnumber(res, "spclocation");

	ntups = PQntuples(res);
	dbinfos = (DbInfo *) pg_malloc(sizeof(DbInfo) * ntups);

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

	cluster->dbarr.dbs = dbinfos;
	cluster->dbarr.ndbs = ntups;
}

/*
 * get_numeric_types()
 *
 * queries the cluster for all types based on NUMERIC, as well as NUMERIC
 * itself, and return an InvalidOid terminated array of pg_type Oids for
 * these types.
 */
static Oid *
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
get_rel_infos(ClusterInfo *cluster, DbInfo *dbinfo)
{
	PGconn	   *conn = connectToServer(cluster,
									   dbinfo->db_name);
	PGresult   *res;
	RelInfo    *relinfos;
	int			ntups;
	int			relnum;
	int			num_rels = 0;
	char	   *nspname = NULL;
	char	   *relname = NULL;
	char		relstorage;
	char		relkind;
	int			i_spclocation,
				i_nspname,
				i_relname,
				i_relstorage,
				i_relkind,
				i_oid,
				i_relfilenode,
				i_reltablespace,
				i_segrelid;
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
	numeric_types = get_numeric_types(conn);
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

	res = executeQueryOrDie(conn, query);
	ntups = PQntuples(res);

	/* Store the relations in a simple ordered array for lookup */
	if (ntups > 0)
	{
		numeric_rel_num = ntups;

		i_oid = PQfnumber(res, "attrelid");
		numeric_rels = pg_malloc(ntups * sizeof(Oid));

		for (relnum = 0; relnum < ntups; relnum++)
			numeric_rels[relnum] = atooid(PQgetvalue(res, relnum, i_oid));
	}

	PQclear(res);

	/*
	 * pg_largeobject contains user data that does not appear in pg_dumpall
	 * --schema-only output, so we have to copy that system table heap and
	 * index.  We could grab the pg_largeobject oids from template1, but it is
	 * easy to treat it as a normal table. Order by oid so we can join old/new
	 * structures efficiently.
	 */

	snprintf(query, sizeof(query),
			 "SELECT c.oid, n.nspname, c.relname, "
			 "  c.relstorage, c.relkind, c.reltablespace, "
			 "	c.relfilenode, %s "
			 "FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n "
			 "	   ON c.relnamespace = n.oid "
			 "  LEFT OUTER JOIN pg_catalog.pg_index i "
			 "	   ON c.oid = i.indexrelid "
			 "  LEFT OUTER JOIN pg_catalog.pg_tablespace t "
			 "	   ON c.reltablespace = t.oid "
			 "WHERE "
			 /* exclude possible orphaned temp tables */
			 "  ((n.nspname !~ '^pg_temp_' AND "
			 "    n.nspname !~ '^pg_toast_temp_' AND "
			 "    n.nspname NOT IN ('gp_toolkit', 'pg_catalog', 'information_schema', 'binary_upgrade', 'pg_aoseg', 'pg_bitmapindex') AND "
			 "    c.relkind <> 'i' AND " // GPDB TODO: Indexes not supported
			 "	  c.oid >= %u) "
			 "  OR (n.nspname = 'pg_catalog' AND "
	"    relname IN ('pg_largeobject', 'pg_largeobject_loid_pn_index'%s) )) "
			 "	AND relkind IN ('r','t','o','m','b', 'i'%s) "
	/* work around what appears to be old 4.3 gp_toolkit bugs */
			 "  AND relname NOT IN ('__gp_localid', '__gp_masterid', '__gp_log_segment_ext', '__gp_log_master_ext', 'gp_disk_free') "
			 " %s "
			 "GROUP BY  c.oid, n.nspname, c.relname, c.relfilenode, c.relstorage, c.relkind, "
			 "			c.reltablespace, spclocation, "
			 "			n.nspname "
	/* we preserve pg_class.oid so we sort by it to match old/new */
			 "ORDER BY 1;",
	/*
	 * 9.2 removed the spclocation column in upstream postgres, in GPDB it was
	 * removed in 6.0.0 during the 8.4 merge
	 */
			 (GET_MAJOR_VERSION(old_cluster.major_version) <= 803) ?
			 "t.spclocation" : "pg_catalog.pg_tablespace_location(t.oid) AS spclocation",
	/* this oid allows us to skip system toast tables */
			 FirstNormalObjectId,
	/* does pg_largeobject_metadata need to be migrated? */
			 (GET_MAJOR_VERSION(old_cluster.major_version) <= 804) ?
			 "" : ", 'pg_largeobject_metadata', 'pg_largeobject_metadata_oid_index'",
	/* see the comment at the top of old_8_3_create_sequence_script() */
			 (GET_MAJOR_VERSION(old_cluster.major_version) <= 803
			  && GET_MAJOR_VERSION(new_cluster.major_version) >= 804) ?
			 "" : ", 'S'",
	/* pg_dump only dumps valid indexes;  testing indisready is
	 * necessary in 9.2, and harmless in earlier/later versions. */
	/* GPDB 4.3 (based on PostgreSQL 8.2), however, doesn't have indisvalid
	 * nor indisready. */
			 (GET_MAJOR_VERSION(old_cluster.major_version) <= 802) ?
			 "" : " AND i.indisvalid IS DISTINCT FROM false AND i.indisready IS DISTINCT FROM false "
		);

	res = executeQueryOrDie(conn, query);

	ntups = PQntuples(res);

	relinfos = (RelInfo *) pg_malloc(sizeof(RelInfo) * ntups);

	i_oid = PQfnumber(res, "oid");
	i_nspname = PQfnumber(res, "nspname");
	i_relname = PQfnumber(res, "relname");
	i_relstorage = PQfnumber(res, "relstorage");
	i_relkind = PQfnumber(res, "relkind");
	i_relfilenode = PQfnumber(res, "relfilenode");
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

			aores = executeQueryOrDie(conn, aoquery);
			if (PQntuples(aores) == 0)
				pg_log(PG_FATAL, "Unable to find auxiliary AO relations for %u (%s)\n",
					   curr->reloid, curr->relname);

			segrel = pg_strdup(PQgetvalue(aores, 0, PQfnumber(aores, "segrel")));
			visimaprel = pg_strdup(PQgetvalue(aores, 0, PQfnumber(aores, "visimaprel")));
			if (!PQgetisnull(aores, 0, PQfnumber(aores, "blkdirrel")))
				blkdirrel = pg_strdup(PQgetvalue(aores, 0, PQfnumber(aores, "blkdirrel")));

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
				if (GET_MAJOR_VERSION(cluster->major_version) <= 802)
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
				aores = executeQueryOrDie(conn, aoquery);

				curr->naosegments = PQntuples(aores);
				curr->aosegments = (AOSegInfo *) pg_malloc(sizeof(AOSegInfo) * curr->naosegments);

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
				if (GET_MAJOR_VERSION(cluster->major_version) <= 802)
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

				aores = executeQueryOrDie(conn, aoquery);

				curr->naosegments = PQntuples(aores);
				curr->aocssegments = (AOCSSegInfo *) pg_malloc(sizeof(AOCSSegInfo) * curr->naosegments);

				for (j = 0; j < curr->naosegments; j++)
				{
					AOCSSegInfo *aocsseg = &curr->aocssegments[j];

					aocsseg->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
					aocsseg->tupcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "tupcount")));
					aocsseg->varblockcount = atoll(PQgetvalue(aores, j, PQfnumber(aores, "varblockcount")));
					aocsseg->vpinfo = pg_strdup(PQgetvalue(aores, j, PQfnumber(aores, "vpinfo")));
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
			if (GET_MAJOR_VERSION(cluster->major_version) <= 802)
			{
				if (!bitmaphack_created)
				{
					PQclear(executeQueryOrDie(conn,
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
			aores = executeQueryOrDie(conn, aoquery);

			curr->naovisimaps = PQntuples(aores);
			curr->aovisimaps = (AOVisiMapInfo *) pg_malloc(sizeof(AOVisiMapInfo) * curr->naovisimaps);

			for (j = 0; j < curr->naovisimaps; j++)
			{
				AOVisiMapInfo *aovisimap = &curr->aovisimaps[j];

				aovisimap->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
				aovisimap->first_row_no = atoll(PQgetvalue(aores, j, PQfnumber(aores, "first_row_no")));
				aovisimap->visimap = pg_strdup(PQgetvalue(aores, j, PQfnumber(aores, "visimap")));
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
				if (GET_MAJOR_VERSION(cluster->major_version) <= 802)
				{
					if (!bitmaphack_created)
					{
						PQclear(executeQueryOrDie(conn,
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

				aores = executeQueryOrDie(conn, aoquery);

				curr->naoblkdirs = PQntuples(aores);
				curr->aoblkdirs = (AOBlkDir *) pg_malloc(sizeof(AOBlkDir) * curr->naoblkdirs);

				for (j = 0; j < curr->naoblkdirs; j++)
				{
					AOBlkDir *aoblkdir = &curr->aoblkdirs[j];

					aoblkdir->segno = atoi(PQgetvalue(aores, j, PQfnumber(aores, "segno")));
					aoblkdir->columngroup_no = atoi(PQgetvalue(aores, j, PQfnumber(aores, "columngroup_no")));
					aoblkdir->first_row_no = atoll(PQgetvalue(aores, j, PQfnumber(aores, "first_row_no")));
					aoblkdir->minipage = pg_strdup(PQgetvalue(aores, j, PQfnumber(aores, "minipage")));
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
			(relkind == 'r' || relkind == 't' || relkind == 'S'))
			/* RELKIND_RELATION, RELKIND_TOASTVALUE, or RELKIND_SEQUENCE */
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

				hres = executeQueryOrDie(conn, hquery);
				i_attlen = PQfnumber(hres, "attlen");
				i_attalign = PQfnumber(hres, "attalign");
				i_atttypid = PQfnumber(hres, "atttypid");
				i_typbasetype = PQfnumber(hres, "typbasetype");

				curr->natts = PQntuples(hres);
				curr->atts = (AttInfo *) pg_malloc(sizeof(AttInfo) * curr->natts);
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
			if (GET_MAJOR_VERSION(old_cluster.major_version) <= 802)
				curr->gpdb4_heap_conversion_needed = true;
		}
		else
			curr->gpdb4_heap_conversion_needed = false;
	}
	PQclear(res);

	PQfinish(conn);

	pg_free(numeric_rels);

	dbinfo->rel_arr.rels = relinfos;
	dbinfo->rel_arr.nrels = num_rels;
}


void
free_db_and_rel_infos(DbInfoArr *db_arr)
{
	int			dbnum;

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
		free_rel_infos(&db_arr->dbs[dbnum].rel_arr);
	pg_free(db_arr->dbs);
	db_arr->dbs = NULL;
	db_arr->ndbs = 0;
}


static void
free_rel_infos(RelInfoArr *rel_arr)
{
	pg_free(rel_arr->rels);
	rel_arr->nrels = 0;
}


static void
print_db_infos(DbInfoArr *db_arr)
{
	int			dbnum;

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
	{
		pg_log(PG_DEBUG, "Database: %s\n", db_arr->dbs[dbnum].db_name);
		print_rel_infos(&db_arr->dbs[dbnum].rel_arr);
		pg_log(PG_DEBUG, "\n\n");
	}
}


static void
print_rel_infos(RelInfoArr *arr)
{
	int			relnum;

	for (relnum = 0; relnum < arr->nrels; relnum++)
		pg_log(PG_DEBUG, "relname: %s.%s: reloid: %u reltblspace: %s\n",
			   arr->rels[relnum].nspname, arr->rels[relnum].relname,
			   arr->rels[relnum].reloid, arr->rels[relnum].tablespace);
}
