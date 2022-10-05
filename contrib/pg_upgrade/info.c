/*
 *	info.c
 *
 *	information support functions
 *
 *	Copyright (c) 2010-2014, PostgreSQL Global Development Group
 *	contrib/pg_upgrade/info.c
 */

#include "postgres_fe.h"

#include "pg_upgrade.h"

#include "access/transam.h"
#include "catalog/pg_class.h"
#include "greenplum/info_gp.h"
#include "greenplum/old_tablespace_file_gp.h"
#include "greenplum/pg_upgrade_greenplum.h"

static void create_rel_filename_map(const char *old_data, const char *new_data,
						const DbInfo *old_db, const DbInfo *new_db,
						const RelInfo *old_rel, const RelInfo *new_rel,
						FileNameMap *map);
static void report_unmatched_relation(const RelInfo *rel, const DbInfo *db,
						  bool is_new_db);
static void free_db_and_rel_infos(DbInfoArr *db_arr);
static void get_db_infos(ClusterInfo *cluster);
static void get_rel_infos(ClusterInfo *cluster, DbInfo *dbinfo);
static void free_rel_infos(RelInfoArr *rel_arr);
static void print_db_infos(DbInfoArr *dbinfo);
static void print_rel_infos(RelInfoArr *rel_arr);


/*
 * gen_db_file_maps()
 *
 * generates a database mapping from "old_db" to "new_db".
 *
 * Returns a malloc'ed array of mappings.  The length of the array
 * is returned into *nmaps.
 */
FileNameMap *
gen_db_file_maps(DbInfo *old_db, DbInfo *new_db,
				 int *nmaps,
				 const char *old_pgdata, const char *new_pgdata)
{
	FileNameMap *maps;
	int			old_relnum, new_relnum;
	int			num_maps = 0;
	bool		all_matched = true;

	/* There will certainly not be more mappings than there are old rels */
	maps = (FileNameMap *) pg_malloc(sizeof(FileNameMap) *
									 old_db->rel_arr.nrels);

	/*
	 * Each of the RelInfo arrays should be sorted by OID.  Scan through them
	 * and match them up.  If we fail to match everything, we'll abort, but
	 * first print as much info as we can about mismatches.
	 */
	old_relnum = new_relnum = 0;
	while (old_relnum < old_db->rel_arr.nrels ||
		   new_relnum < new_db->rel_arr.nrels)
	{
		RelInfo    *old_rel = (old_relnum < old_db->rel_arr.nrels) ?
		&old_db->rel_arr.rels[old_relnum] : NULL;
		RelInfo    *new_rel = (new_relnum < new_db->rel_arr.nrels) ?
		&new_db->rel_arr.rels[new_relnum] : NULL;

		/* handle running off one array before the other */
		if (!new_rel)
		{
			/*
			 * old_rel is unmatched.  This should never happen, because we
			 * force new rels to have TOAST tables if the old one did.
			 */
			report_unmatched_relation(old_rel, old_db, false);
			all_matched = false;
			old_relnum++;
			continue;
		}
		if (!old_rel)
		{
			/*
			 * new_rel is unmatched.  This shouldn't really happen either, but
			 * if it's a TOAST table, we can ignore it and continue
			 * processing, assuming that the new server made a TOAST table
			 * that wasn't needed.
			 */
			if (strcmp(new_rel->nspname, "pg_toast") != 0)
			{
				report_unmatched_relation(new_rel, new_db, true);
				all_matched = false;
			}
			new_relnum++;
			continue;
		}

		/* check for mismatched OID */
		if (old_rel->reloid < new_rel->reloid)
		{
			/* old_rel is unmatched, see comment above */
			report_unmatched_relation(old_rel, old_db, false);
			all_matched = false;
			old_relnum++;
			continue;
		}
		else if (old_rel->reloid > new_rel->reloid)
		{
			/* new_rel is unmatched, see comment above */
			if (strcmp(new_rel->nspname, "pg_toast") != 0)
			{
				report_unmatched_relation(new_rel, new_db, true);
				all_matched = false;
			}
			new_relnum++;
			continue;
		}

		/*
		 * Verify that rels of same OID have same name.  The namespace name
		 * should always match, but the relname might not match for TOAST
		 * tables (and, therefore, their indexes).
		 *
		 * TOAST table names initially match the heap pg_class oid, but
		 * pre-9.0 they can change during certain commands such as CLUSTER, so
		 * don't insist on a match if old cluster is < 9.0.
		 *
		 * XXX GPDB: for TOAST tables, don't insist on a match at all
		 * yet; there are other ways for us to get mismatched names. Ideally
		 * this will go away eventually.
		 */
		if (strcmp(old_rel->nspname, new_rel->nspname) != 0 ||
			(strcmp(old_rel->relname, new_rel->relname) != 0 &&
			 (/* GET_MAJOR_VERSION(old_cluster.major_version) >= 900 || */
			  strcmp(old_rel->nspname, "pg_toast") != 0)))
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
		if (old_rel->relstorage == 'x' || strcmp(new_rel->nspname, "pg_aoseg") == 0)
		{
			old_relnum++;
			new_relnum++;
			continue;
		}

		/* XXX Why are we doing this here and not in get_rel_infos()? */
		if (old_rel->aosegments != NULL)
			old_rel->reltype = AO;
		else if (old_rel->aocssegments != NULL)
			old_rel->reltype = AOCS;
		else
			old_rel->reltype = HEAP;

		/* OK, create a mapping entry */
		create_rel_filename_map(old_pgdata, new_pgdata, old_db, new_db,
								old_rel, new_rel, maps + num_maps);
		num_maps++;
		old_relnum++;
		new_relnum++;
	}

	if (!all_matched)
		pg_fatal("Failed to match up old and new tables in database \"%s\"\n",
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
						FileNameMap *map)
{
	/* In case old/new tablespaces don't match, do them separately. */
	if (strlen(old_rel->tablespace) == 0)
	{
		/*
		 * relation belongs to the default tablespace, hence relfiles should
		 * exist in the data directories.
		 */
		map->old_tablespace = old_data;
		map->old_tablespace_suffix = "/base";
	}
	else
	{
		/* relation belongs to a tablespace, so use the tablespace location */
		map->old_tablespace = old_rel->tablespace;
		map->old_tablespace_suffix = old_cluster.tablespace_suffix;
	}

	/* Do the same for new tablespaces */
	if (strlen(new_rel->tablespace) == 0)
	{
		map->new_tablespace = new_data;
		map->new_tablespace_suffix = "/base";
	}
	else
	{
		map->new_tablespace = new_rel->tablespace;
		map->new_tablespace_suffix = new_cluster.tablespace_suffix;
	}

	map->old_db_oid = old_db->db_oid;
	map->new_db_oid = new_db->db_oid;

	/*
	 * old_relfilenode might differ from pg_class.oid (and hence
	 * new_relfilenode) because of CLUSTER, REINDEX, or VACUUM FULL.
	 */
	map->old_relfilenode = old_rel->relfilenode;

	/* new_relfilenode will match old and new pg_class.oid */
	map->new_relfilenode = new_rel->relfilenode;

	/* GPDB additions to map data */
	map->atts = old_rel->atts;
	map->natts = old_rel->natts;
	map->type = old_rel->reltype;

	/* An AO table doesn't necessarily have segment 0 at all. */
	map->missing_seg0_ok = relstorage_is_ao(old_rel->relstorage);

	/* used only for logging and error reporing, old/new are identical */
	map->nspname = old_rel->nspname;
	map->relname = old_rel->relname;
}


/*
 * Complain about a relation we couldn't match to the other database,
 * identifying it as best we can.
 */
static void
report_unmatched_relation(const RelInfo *rel, const DbInfo *db, bool is_new_db)
{
	Oid			reloid = rel->reloid;	/* we might change rel below */
	char		reldesc[1000];
	int			i;

	snprintf(reldesc, sizeof(reldesc), "\"%s.%s\"",
			 rel->nspname, rel->relname);
	if (rel->indtable)
	{
		for (i = 0; i < db->rel_arr.nrels; i++)
		{
			const RelInfo *hrel = &db->rel_arr.rels[i];

			if (hrel->reloid == rel->indtable)
			{
				snprintf(reldesc + strlen(reldesc),
						 sizeof(reldesc) - strlen(reldesc),
						 " which is an index on \"%s.%s\"",
						 hrel->nspname, hrel->relname);
				/* Shift attention to index's table for toast check */
				rel = hrel;
				break;
			}
		}
		if (i >= db->rel_arr.nrels)
			snprintf(reldesc + strlen(reldesc),
					 sizeof(reldesc) - strlen(reldesc),
					 " which is an index on OID %u", rel->indtable);
	}
	if (rel->toastheap)
	{
		for (i = 0; i < db->rel_arr.nrels; i++)
		{
			const RelInfo *brel = &db->rel_arr.rels[i];

			if (brel->reloid == rel->toastheap)
			{
				snprintf(reldesc + strlen(reldesc),
						 sizeof(reldesc) - strlen(reldesc),
						 " which is the TOAST table for \"%s.%s\"",
						 brel->nspname, brel->relname);
				break;
			}
		}
		if (i >= db->rel_arr.nrels)
			snprintf(reldesc + strlen(reldesc),
					 sizeof(reldesc) - strlen(reldesc),
					 " which is the TOAST table for OID %u", rel->toastheap);
	}

	if (is_new_db)
		pg_log(PG_WARNING, "No match found in old cluster for new relation with OID %u in database \"%s\": %s\n",
			   reloid, db->db_name, reldesc);
	else
		pg_log(PG_WARNING, "No match found in new cluster for old relation with OID %u in database \"%s\": %s\n",
			   reloid, db->db_name, reldesc);
}


void
print_maps(FileNameMap *maps, int n_maps, const char *db_name)
{
	if (log_opts.verbose)
	{
		int			mapnum;

		pg_log(PG_VERBOSE, "mappings for database \"%s\":\n", db_name);

		for (mapnum = 0; mapnum < n_maps; mapnum++)
			pg_log(PG_VERBOSE, "%s.%s: %u to %u\n",
				   maps[mapnum].nspname, maps[mapnum].relname,
				   maps[mapnum].old_relfilenode,
				   maps[mapnum].new_relfilenode);

		pg_log(PG_VERBOSE, "\n\n");
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

	/*
	 * Reset the index state on the new cluster only.
	 */
	if (!is_greenplum_dispatcher_mode() && !user_opts.check && cluster == &new_cluster)
		reset_invalid_indexes();

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
		get_rel_infos(cluster, &cluster->dbarr.dbs[dbnum]);

	pg_log(PG_VERBOSE, "\n%s databases:\n", CLUSTER_NAME(cluster));
	if (log_opts.verbose)
		print_db_infos(&cluster->dbarr);
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
	int 		i_datafrozenxid;
	int 		i_datminmxid = 0;

	/*
	 * greenplum specific indexes
	 */
	int         i_tablespace_oid;

	snprintf(query, sizeof(query),
			 "SELECT d.oid, d.datname, t.oid as tablespace_oid, %s , datfrozenxid %s "
			 "FROM pg_catalog.pg_database d "
			 " LEFT OUTER JOIN pg_catalog.pg_tablespace t "
			 " ON d.dattablespace = t.oid "
			 "WHERE d.datallowconn = true "
	/* we don't preserve pg_database.oid so we sort by name */
			 "ORDER BY 2",
	/* 9.2 removed the spclocation column */
	/* GPDB_XX_MERGE_FIXME: spclocation was removed in 6.0 cycle */
			 (GET_MAJOR_VERSION(cluster->major_version) == 803) ?
			 "t.spclocation" : "pg_catalog.pg_tablespace_location(t.oid) AS spclocation",
			 (GET_MAJOR_VERSION(cluster->major_version) == 803) ?
			 " ": ", datminmxid");

	res = executeQueryOrDie(conn, "%s", query);

	i_oid = PQfnumber(res, "oid");
	i_datname = PQfnumber(res, "datname");
	i_spclocation = PQfnumber(res, "spclocation");
	i_tablespace_oid = PQfnumber(res, "tablespace_oid");
	i_datafrozenxid = PQfnumber(res, "datfrozenxid");
	if (GET_MAJOR_VERSION(cluster->major_version) > 803)
		i_datminmxid = PQfnumber(res, "datminmxid");

	ntups = PQntuples(res);
	dbinfos = (DbInfo *) pg_malloc(sizeof(DbInfo) * ntups);

	for (tupnum = 0; tupnum < ntups; tupnum++)
	{
		dbinfos[tupnum].db_oid = atooid(PQgetvalue(res, tupnum, i_oid));
		dbinfos[tupnum].db_name = pg_strdup(PQgetvalue(res, tupnum, i_datname));
		dbinfos[tupnum].datfrozenxid = strtoul(PQgetvalue(res, tupnum, i_datafrozenxid), NULL, 10);
		if (GET_MAJOR_VERSION(cluster->major_version) > 803)
			dbinfos[tupnum].datminmxid = strtoul(PQgetvalue(res, tupnum, i_datminmxid), NULL, 10);
		snprintf(dbinfos[tupnum].db_tablespace, sizeof(dbinfos[tupnum].db_tablespace), "%s",
		         determine_db_tablespace_path(
		                 cluster,
		                 PQgetvalue(res, tupnum, i_spclocation),
                         atooid(PQgetvalue(res, tupnum, i_tablespace_oid))));
	}
	PQclear(res);

	PQfinish(conn);

	cluster->dbarr.dbs = dbinfos;
	cluster->dbarr.ndbs = ntups;
}

/*
 * get_rel_infos()
 *
 * gets the relinfos for all the user tables of the database referred
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
	char	   *tablespace = NULL;
	int			i_spclocation,
				i_nspname,
				i_relname,
				i_reloid,
				i_indtable,
				i_toastheap,
				i_relfilenode,
				i_reltablespace;
	char		query[QUERY_ALLOC];
	char	   *last_namespace = NULL,
			   *last_tablespace = NULL;

	char		relstorage;
	char		relkind;
	int			i_relstorage = -1;
	int			i_relkind = -1;
	Oid tablespace_oid;

	PGresult   *aorels;
	int			i_aoreloid,
				i_aosegrel,
				i_aovisimaprel,
				i_aoblkdirrel;
	int			num_aorels;
	int			current_ao;

	/*
	 * pg_largeobject contains user data that does not appear in pg_dump
	 * --schema-only output, so we have to copy that system table heap and
	 * index.  We could grab the pg_largeobject oids from template1, but it is
	 * easy to treat it as a normal table. Order by oid so we can join old/new
	 * structures efficiently.
	 */

	snprintf(query, sizeof(query),
			 "CREATE TEMPORARY TABLE info_rels (reloid, indtable, toastheap) AS "
			 "SELECT c.oid, i.indrelid, 0::oid "
			 "FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n "
			 "	   ON c.relnamespace = n.oid "
			 "LEFT OUTER JOIN pg_catalog.pg_index i "
			 "	   ON c.oid = i.indexrelid "
			 "WHERE relkind IN ('r', 'o', 'b', 'i'%s%s) AND "
	/*
	 * pg_dump only dumps valid indexes;  testing indisready is necessary in
	 * 9.2, and harmless in earlier/later versions.
	 */
			 " i.indisvalid IS DISTINCT FROM false AND "
			 " i.indisready IS DISTINCT FROM false AND "
			 " relname NOT IN ('__gp_localid', '__gp_masterid', "
			"'__gp_log_segment_ext', '__gp_log_master_ext', 'gp_disk_free') AND"
	/* exclude possible orphaned temp tables */
			 "  ((n.nspname !~ '^pg_temp_' AND "
			 "    n.nspname !~ '^pg_toast_temp_' AND "
	/* skip pg_toast because toast index have relkind == 'i', not 't' */
			 "    n.nspname NOT IN ('pg_catalog', 'information_schema', "
			 "						'binary_upgrade', 'pg_toast', 'pg_aoseg') AND "
			 "    n.nspname NOT IN ('gp_toolkit', 'pg_bitmapindex') AND "
			 "	  c.oid >= %u) "
			 "  OR (n.nspname = 'pg_catalog' AND "
	"    relname IN ('pg_largeobject', 'pg_largeobject_loid_pn_index'%s, "
	"                'gp_fastsequence', 'gp_fastsequence_objid_objmod_index') ));",
	/* Greenplum 5X use 'm' as aovisimap which is now matview in 6X and above. */
			 (GET_MAJOR_VERSION(cluster->major_version) == 803) ?
			 ", 'm'" : ", 'M'",
	/* see the comment at the top of old_8_3_create_sequence_script() */
			 (GET_MAJOR_VERSION(old_cluster.major_version) == 803) ?
			 "" : ", 'S'",
			 FirstNormalObjectId,
	/* does pg_largeobject_metadata need to be migrated? */
			 (GET_MAJOR_VERSION(old_cluster.major_version) <= 804) ?
	"" : ", 'pg_largeobject_metadata', 'pg_largeobject_metadata_oid_index'");

	PQclear(executeQueryOrDie(conn, "%s", query));

	/*
	 * Get TOAST tables and indexes;  we have to gather the TOAST tables in
	 * later steps because we can't schema-qualify TOAST tables.
	 */
	PQclear(executeQueryOrDie(conn,
							  "INSERT INTO info_rels "
							  "SELECT reltoastrelid, 0::oid, c.oid "
							  "FROM info_rels i JOIN pg_catalog.pg_class c "
							  "		ON i.reloid = c.oid "
						  "		AND c.reltoastrelid != %u", InvalidOid));
	PQclear(executeQueryOrDie(conn,
							  "INSERT INTO info_rels "
							  "SELECT indexrelid, ind.indrelid, 0::oid "
							  "FROM info_rels i JOIN pg_catalog.pg_index ind "
							  "     ON ind.indrelid = i.reloid "
							  "WHERE indisvalid AND i.toastheap != %u", InvalidOid));

	snprintf(query, sizeof(query),
			 "SELECT i.*, n.nspname, c.relname, "
			 "  c.relstorage, c.relkind, "
			 "	c.relfilenode, c.reltablespace, %s "
			 "FROM info_rels i JOIN pg_catalog.pg_class c "
			 "		ON i.reloid = c.oid "
			 "  JOIN pg_catalog.pg_namespace n "
			 "	   ON c.relnamespace = n.oid "
			 "  LEFT OUTER JOIN pg_catalog.pg_tablespace t "
			 "	   ON c.reltablespace = t.oid "
	/* we preserve pg_class.oid so we sort by it to match old/new */
			 "ORDER BY 1;",
	/*
	 * 9.2 removed the spclocation column in upstream postgres, in GPDB it was
	 * removed in 6.0.0 during the 8.4 merge
	 */
		(GET_MAJOR_VERSION(cluster->major_version) == 803) ?
			 "t.spclocation" : "pg_catalog.pg_tablespace_location(t.oid) AS spclocation");


	res = executeQueryOrDie(conn, "%s", query);

	ntups = PQntuples(res);

	relinfos = (RelInfo *) pg_malloc(sizeof(RelInfo) * ntups);

	i_reloid = PQfnumber(res, "reloid");
	i_indtable = PQfnumber(res, "indtable");
	i_toastheap = PQfnumber(res, "toastheap");
	i_nspname = PQfnumber(res, "nspname");
	i_relname = PQfnumber(res, "relname");
	i_relstorage = PQfnumber(res, "relstorage");
	i_relkind = PQfnumber(res, "relkind");
	i_relfilenode = PQfnumber(res, "relfilenode");
	i_reltablespace = PQfnumber(res, "reltablespace");
	i_spclocation = PQfnumber(res, "spclocation");

	/*
	 * GPDB: preload as much AO table information as we can in a single query,
	 * rather than making one query per AO table which slows down upgrades of
	 * real-world AO partition hierarchies considerably. This should ideally be
	 * pulled into the info_rels structure, but to decrease the potential merge
	 * hazard, we make a separate query and "join" by OID later.
	 *
	 * XXX (Unfortunately, since the AO auxiliary catalogs exist in separate
	 * relations named by AO table OID, they can't be easily queried at once.
	 * Improve this situation.)
	 *
	 * First query the catalog for the auxiliary heap relations which describe
	 * AO{CS} relations. The segrel and visimap must exist but the blkdirrel is
	 * created when required so it might not exist.
	 *
	 * We don't dump the block directory, even if it exists, if the table
	 * doesn't have any indexes. This isn't just an optimization: restoring it
	 * wouldn't work, because without indexes, restore won't create a block
	 * directory in the new cluster.
	 */
	aorels = executeQueryOrDie(conn, "%s",
			 "SELECT a.relid, "
			 "       cs.relname AS segrel, "
			 "       cv.relname AS visimaprel, "
			 "       cb.relname AS blkdirrel "
			 "FROM   pg_appendonly a "
			 "       JOIN pg_class cs on (cs.oid = a.segrelid) "
			 "       JOIN pg_class cv on (cv.oid = a.visimaprelid) "
			 "       LEFT JOIN pg_class cb on (cb.oid = a.blkdirrelid "
			 "                                 AND a.blkdirrelid <> 0 "
			 "                                 AND EXISTS (SELECT 1 FROM pg_index i WHERE i.indrelid = a.relid)) "
			 "ORDER BY a.relid ");

	num_aorels = PQntuples(aorels);
	current_ao = 0;

	i_aoreloid = PQfnumber(aorels, "relid");
	i_aosegrel = PQfnumber(aorels, "segrel");
	i_aovisimaprel = PQfnumber(aorels, "visimaprel");
	i_aoblkdirrel = PQfnumber(aorels, "blkdirrel");

	for (relnum = 0; relnum < ntups; relnum++)
	{
		RelInfo    *curr = &relinfos[num_rels++];

		curr->reloid = atooid(PQgetvalue(res, relnum, i_reloid));

		if (!PQgetisnull(res, relnum, i_indtable))
		{
			curr->indtable = atooid(PQgetvalue(res, relnum, i_indtable));
		}
		else
		{
			curr->indtable = 0;
		}

		curr->toastheap = atooid(PQgetvalue(res, relnum, i_toastheap));

		nspname = PQgetvalue(res, relnum, i_nspname);
		curr->nsp_alloc = false;

		/*
		 * Many of the namespace and tablespace strings are identical, so we
		 * try to reuse the allocated string pointers where possible to reduce
		 * memory consumption.
		 */
		/* Can we reuse the previous string allocation? */
		if (last_namespace && strcmp(nspname, last_namespace) == 0)
			curr->nspname = last_namespace;
		else
		{
			last_namespace = curr->nspname = pg_strdup(nspname);
			curr->nsp_alloc = true;
		}

		relname = PQgetvalue(res, relnum, i_relname);
		curr->relname = pg_strdup(relname);

		curr->relfilenode = atooid(PQgetvalue(res, relnum, i_relfilenode));
		curr->tblsp_alloc = false;

		tablespace_oid = atooid(PQgetvalue(res, relnum, i_reltablespace));

		/* Is the tablespace oid non-zero? */
		if (tablespace_oid != 0)
		{
			tablespace = determine_db_tablespace_path(cluster,
			                                          PQgetvalue(res, relnum, i_spclocation),
			                                          tablespace_oid);

			/* Can we reuse the previous string allocation? */
			if (last_tablespace && strcmp(tablespace, last_tablespace) == 0)
				curr->tablespace = last_tablespace;
			else
			{
				last_tablespace = curr->tablespace = pg_strdup(tablespace);
				curr->tblsp_alloc = true;
			}
		}
		else
			/* A zero reltablespace oid indicates the database tablespace. */
			curr->tablespace = dbinfo->db_tablespace;

		/* Collect extra information about append-only tables */
		relstorage = PQgetvalue(res, relnum, i_relstorage) [0];
		curr->relstorage = relstorage;

		relkind = PQgetvalue(res, relnum, i_relkind) [0];

		/*
		 * The structure of append
		 * optimized tables is similar enough for row and column oriented
		 * tables so we can handle them both here.
		 */
		if (is_appendonly(relstorage))
		{
			PGresult   *aores;
			Oid			aoreloid = 0;
			char	   *segrel;
			char	   *visimaprel;
			char	   *blkdirrel = NULL;
			int			j;

			/*
			 * Since res and aorels are both sorted by relation OID, the next
			 * row in aorels should usually be exactly the relation we're
			 * looking for. The exception is that some AO tables aren't included
			 * in info_rels (e.g. append-only materialized views), so we have to
			 * handle extras in the list.
			 */
			while (current_ao < num_aorels)
			{
				aoreloid = atooid(PQgetvalue(aorels, current_ao, i_aoreloid));
				if (aoreloid >= curr->reloid)
					break;

				++current_ao;
			}
			if (aoreloid != curr->reloid)
				pg_log(PG_FATAL, "Unable to find auxiliary AO relations for %u (%s) in database '%s'",
					   curr->reloid, curr->relname, dbinfo->db_name);

			segrel = pg_strdup(PQgetvalue(aorels, current_ao, i_aosegrel));
			visimaprel = pg_strdup(PQgetvalue(aorels, current_ao, i_aovisimaprel));
			if (!PQgetisnull(aorels, current_ao, i_aoblkdirrel))
				blkdirrel = pg_strdup(PQgetvalue(aorels, current_ao, i_aoblkdirrel));

			++current_ao;

			if (relstorage == 'a')
			{
				aores = executeQueryOrDie(conn,
							"SELECT segno, eof, tupcount, varblockcount, "
							"       eofuncompressed, modcount, state, "
							"       formatversion "
							"FROM   pg_aoseg.%s",
							segrel);

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
				aores = executeQueryOrDie(conn,
							"SELECT segno, tupcount, varblockcount, vpinfo, "
							"       modcount, formatversion, state "
							"FROM   pg_aoseg.%s",
							segrel);

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

			aores = executeQueryOrDie(conn,
						"SELECT segno, first_row_no, visimap "
						"FROM pg_aoseg.%s",
						visimaprel);

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
			 * is InvalidOid then there is no blkdir table.
			 */
			if (blkdirrel)
			{
				aores = executeQueryOrDie(conn,
							"SELECT segno, columngroup_no, first_row_no, minipage "
							"FROM pg_aoseg.%s",
							blkdirrel);

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
			/* Not an AO/AOCS relation */
			curr->aosegments = NULL;
			curr->aocssegments = NULL;
			curr->naosegments = 0;
			curr->aovisimaps = NULL;
			curr->naovisimaps = 0;
			curr->naoblkdirs = 0;
			curr->aoblkdirs = NULL;
		}
	}
	PQclear(res);
	PQclear(aorels);

	PQfinish(conn);

	dbinfo->rel_arr.rels = relinfos;
	dbinfo->rel_arr.nrels = num_rels;
}


static void
free_db_and_rel_infos(DbInfoArr *db_arr)
{
	int			dbnum;

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
	{
		free_rel_infos(&db_arr->dbs[dbnum].rel_arr);
		pg_free(db_arr->dbs[dbnum].db_name);
	}
	pg_free(db_arr->dbs);
	db_arr->dbs = NULL;
	db_arr->ndbs = 0;
}


static void
free_rel_infos(RelInfoArr *rel_arr)
{
	int			relnum;

	for (relnum = 0; relnum < rel_arr->nrels; relnum++)
	{
		if (rel_arr->rels[relnum].nsp_alloc)
			pg_free(rel_arr->rels[relnum].nspname);
		pg_free(rel_arr->rels[relnum].relname);
		if (rel_arr->rels[relnum].tblsp_alloc)
			pg_free(rel_arr->rels[relnum].tablespace);
	}
	pg_free(rel_arr->rels);
	rel_arr->nrels = 0;
}


static void
print_db_infos(DbInfoArr *db_arr)
{
	int			dbnum;

	for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
	{
		pg_log(PG_VERBOSE, "Database: %s\n", db_arr->dbs[dbnum].db_name);
		print_rel_infos(&db_arr->dbs[dbnum].rel_arr);
		pg_log(PG_VERBOSE, "\n\n");
	}
}


static void
print_rel_infos(RelInfoArr *rel_arr)
{
	int			relnum;

	for (relnum = 0; relnum < rel_arr->nrels; relnum++)
		pg_log(PG_VERBOSE, "relname: %s.%s: reloid: %u reltblspace: %s\n",
			   rel_arr->rels[relnum].nspname,
			   rel_arr->rels[relnum].relname,
			   rel_arr->rels[relnum].reloid,
			   rel_arr->rels[relnum].tablespace);
}
