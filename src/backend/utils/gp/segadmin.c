/*-------------------------------------------------------------------------
 *
 * segadmin.c
 *	  Functions to support administrative tasks with GPDB segments.
 *
 * Portions Copyright (c) 2010 Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/gp/segadmin.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"

#include "libpq-fe.h"
#include "pqexpbuffer.h"

#include "access/xlog.h"
#include "catalog/gp_segment_config.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_filespace_entry.h"
#include "catalog/pg_proc.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistentrelation.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbfts.h"
#include "commands/filespace.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "postmaster/primary_mirror_mode.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"

#define MASTER_ONLY 0x1
#define UTILITY_MODE 0x2
#define SUPERUSER 0x4
#define READ_ONLY 0x8
#define SEGMENT_ONLY 0x10
#define STANDBY_ONLY 0x20
#define SINGLE_USER_MODE 0x40

/* Encapsulation of information about a given segment. */
typedef struct seginfo
{
	CdbComponentDatabaseInfo db;
} seginfo;

/* look up a particular segment */
static seginfo *
get_seginfo(int16 dbid)
{
	seginfo    *i = palloc(sizeof(seginfo));
	CdbComponentDatabaseInfo *c = dbid_get_dbinfo(dbid);

	memcpy(&(i->db), c, sizeof(CdbComponentDatabaseInfo));

	return i;
}

/* Convenience routine to look up the mirror for a given segment index */
static int16
content_get_mirror_dbid(int16 contentid)
{
	return contentid_get_dbid(contentid, SEGMENT_ROLE_MIRROR, false /* false == current, not
							    * preferred, role */ );
}

/* Tell the caller whether a mirror exists at a given segment index */
static bool
segment_has_mirror(int16 contentid)
{
	return content_get_mirror_dbid(contentid) != 0;
}

/*
 * As the function name says, test whether a given dbid is the dbid of the
 * standby master.
 */
static bool
dbid_is_master_standby(int16 dbid)
{
	int16		standbydbid = content_get_mirror_dbid(MASTER_CONTENT_ID);

	return (standbydbid == dbid);
}

/*
 * Tell the caller whether a standby master is defined in the system.
 */
static bool
standby_exists()
{
	return segment_has_mirror(MASTER_CONTENT_ID);
}

/*
 * Get the highest dbid defined in the system. We AccessExclusiveLock
 * gp_segment_configuration to prevent races but no one should be calling
 * this code concurrently if we've done our job right.
 */
static int16
get_maxdbid()
{
	Relation	rel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);
	int16		dbid = 0;
	HeapTuple	tuple;
	SysScanDesc sscan;

	sscan = systable_beginscan(rel, InvalidOid, false, SnapshotNow, 0, NULL);
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		dbid = Max(dbid,
				   ((Form_gp_segment_configuration) GETSTRUCT(tuple))->dbid);
	}
	systable_endscan(sscan);
	heap_close(rel, NoLock);

	return dbid;
}

/**
 * Get an available dbid value. We AccessExclusiveLock
 * gp_segment_configuration to prevent races but no one should be calling
 * this code concurrently if we've done our job right.
 */
static int16
get_availableDbId()
{
	/*
	 * Set up hash of used dbids.  We use int32 here because int16 doesn't
	 * have a convenient hash and we can use casting below to check for
	 * overflow of int16
	 */
	HASHCTL		hash_ctl;

	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(int32);
	hash_ctl.entrysize = sizeof(int32);
	hash_ctl.hash = int32_hash;
	HTAB	   *htab = hash_create("Temporary table of dbids",
								   1024,
								   &hash_ctl,
								   HASH_ELEM | HASH_FUNCTION);

	/* scan GpSegmentConfigRelationId */
	Relation	rel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);
	HeapTuple	tuple;
	SysScanDesc sscan;

	sscan = systable_beginscan(rel, InvalidOid, false, SnapshotNow, 0, NULL);
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		int32		dbid = (int32) ((Form_gp_segment_configuration) GETSTRUCT(tuple))->dbid;

		(void) hash_search(htab, (void *) &dbid, HASH_ENTER, NULL);
	}
	systable_endscan(sscan);

	heap_close(rel, NoLock);

	/* search for available dbid */
	for (int32 dbid = 1;; dbid++)
	{
		if (dbid != (int16) dbid)
			elog(ERROR, "unable to find available dbid");

		if (hash_search(htab, (void *) &dbid, HASH_FIND, NULL) == NULL)
		{
			hash_destroy(htab);
			return dbid;
		}
	}
}


/*
 * Get the highest contentid defined in the system. As above, we
 * AccessExclusiveLock gp_segment_configuration to prevent races but no
 * one should be calling this code concurrently if we've done our job right.
 */
static int16
get_maxcontentid()
{
	Relation	rel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);
	int16		contentid = 0;
	HeapTuple	tuple;
	SysScanDesc sscan;

	sscan = systable_beginscan(rel, InvalidOid, false, SnapshotNow, 0, NULL);
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		contentid = Max(contentid,
						((Form_gp_segment_configuration) GETSTRUCT(tuple))->content);
	}
	systable_endscan(sscan);
	heap_close(rel, NoLock);

	return contentid;
}


/*
 * Verify that a filespace map looks like:
 * ARRAY[
 *   ARRAY[name, path],
 *   ...
 *  ]
 */
static void
check_array_bounds(ArrayType *map)
{
	if (ARR_NDIM(map) != 2)
		elog(ERROR, "filespace map must be a two dimensional array");

	if (ARR_DIMS(map)[1] != 2)
		elog(ERROR, "filespace map must be an array of two element arrays");
}

/*
 * Ensure that the map looks right and that it has the right number of
 * filespace mappings.
 */
static void
check_fsmap(ArrayType *map)
{
	int			n;
	int			an = (ArrayGetNItems(ARR_NDIM(map), ARR_DIMS(map)) / 2);

	check_array_bounds(map);

	/* get number of filespaces defined */
	n = num_filespaces();

	if (n != an)
		elog(ERROR, "system contains %i filespaces but filespace map contains "
			 "only %i filespaces", n, an);
}

/*
 * Check that the code is being called in right context.
 */
static void
mirroring_sanity_check(int flags, const char *func)
{
	if ((flags & MASTER_ONLY) == MASTER_ONLY)
	{
		if (GpIdentity.dbid == UNINITIALIZED_GP_IDENTITY_VALUE)
			elog(ERROR, "%s requires valid GpIdentity dbid", func);

		if (GpIdentity.segindex != MASTER_CONTENT_ID)
			elog(ERROR, "%s must be run on the master", func);
	}

	if ((flags & UTILITY_MODE) == UTILITY_MODE)
	{
		if (Gp_role != GP_ROLE_UTILITY)
			elog(ERROR, "%s must be run in utility mode", func);
	}

	if ((flags & SINGLE_USER_MODE) == SINGLE_USER_MODE)
	{
		if (IsUnderPostmaster)
			elog(ERROR, "%s must be run in single-user mode", func);
	}

	if ((flags & SUPERUSER) == SUPERUSER)
	{
		if (!superuser())
			elog(ERROR, "%s can only be run by a superuser", func);
	}

	if ((flags & READ_ONLY) == READ_ONLY)
	{
		if (gp_set_read_only != true)
			elog(ERROR, "%s can only be run if the system is in read only mode",
				 func);
	}

	if ((flags & SEGMENT_ONLY) == SEGMENT_ONLY)
	{
		if (GpIdentity.dbid == UNINITIALIZED_GP_IDENTITY_VALUE)
			elog(ERROR, "%s requires valid GpIdentity dbid", func);

		if (GpIdentity.segindex == MASTER_CONTENT_ID)
			elog(ERROR, "%s cannot be run on the master", func);
	}

	if ((flags & STANDBY_ONLY) == STANDBY_ONLY)
	{
		if (GpIdentity.dbid == UNINITIALIZED_GP_IDENTITY_VALUE)
			elog(ERROR, "%s requires valid GpIdentity dbid", func);

		if (!dbid_is_master_standby(GpIdentity.dbid))
			elog(ERROR, "%s can only be run on the standby master", func);
	}
}

static bool
must_update_persistent(int16 pridbid, seginfo *i)
{
	/* if we're adding a new primary (for expansion), bail out */
	if (i->db.role == SEGMENT_ROLE_PRIMARY)
		return false;

	/* if we're adding a mirror for host that is not this machine, return */
	if (i->db.role == SEGMENT_ROLE_MIRROR && pridbid != GpIdentity.dbid)
		return false;

	return true;
}

/*
 * Add knowledge of filespaces for a new segment to the standard system
 * catalog.
 */
static void
catalog_filespaces(Relation fsentryrel, int16 dbid, ArrayType *map)
{
	Datum	   *d;
	int			n,
				i;
	Relation	fsrel = heap_open(FileSpaceRelationId, AccessShareLock);

	deconstruct_array(map, TEXTOID, -1, false, 'i', &d, NULL, &n);

	/* need to check that this is an array of name, value pairs */
	for (i = 0; i < n; i += 2)
	{
		char	   *fsname = TextDatumGetCString(d[i]);
		char	   *path;
		Oid			fsoid;

		fsoid = get_filespace_oid(fsrel, (const char *) fsname);

		if (!OidIsValid(fsoid))
			elog(ERROR, "filespace \"%s\" does not exist", fsname);

		Insist(i + 1 < n);
		path = TextDatumGetCString(d[i + 1]);

		add_catalog_filespace_entry(fsentryrel, fsoid, dbid, path);
	}
	heap_close(fsrel, NoLock);
}

/*
 * Add knowledge of filespaces for a new segment to the persistent
 * catalogs.
 */
static void
persist_all_filespaces(int16 pridbid, int16 mirdbid, ArrayType *fsmap)
{
	Datum	   *d;
	int			n,
				i;
	Relation	rel = heap_open(FileSpaceRelationId, AccessShareLock);

	deconstruct_array(fsmap, TEXTOID, -1, false, 'i', &d, NULL, &n);

	/* need to check that this is an array of name, value pairs */
	for (i = 0; i < n; i += 2)
	{
		char	   *fsname = TextDatumGetCString(d[i]);
		char	   *path;
		Oid			fsoid;
		bool		set_mirror_existence;

		/* we do not persist system filespaces */
		if (strcmp(fsname, SYSTEMFILESPACE_NAME) == 0)
			continue;
		fsoid = get_filespace_oid(rel, (const char *) fsname);

		if (!OidIsValid(fsoid))
			elog(ERROR, "filespace \"%s\" does not exist", fsname);

		Insist(i + 1 < n);
		path = TextDatumGetCString(d[i + 1]);

		/* only set the mirror existence on segments */
		set_mirror_existence = (pridbid != MASTER_DBID);
		PersistentFilespace_AddMirror(fsoid, path, pridbid, mirdbid,
									  set_mirror_existence);
	}
	heap_close(rel, NoLock);
}

/*
 * Remove knowledge of filespaces for a given segment from the persistent
 * catalog.
 */
static void
desist_all_filespaces(int16 dbid)
{
	PersistentFilespace_RemoveSegment(dbid, true);
}

/*
 * Either add or remove knowledge of filespaces for a given segment.
 */
static void
update_filespaces(int16 pridbid, seginfo *i, bool add, ArrayType *fsmap)
{
	Assert((add && PointerIsValid(fsmap)) || !add);

	/*
	 * If we're on the master, process the pg_filespace_entry rows for this
	 * segment.
	 */
	if (GpIdentity.segindex == MASTER_CONTENT_ID)
	{
		Relation	rel = heap_open(FileSpaceEntryRelationId,
									RowExclusiveLock);

		if (add)
			catalog_filespaces(rel, i->db.dbid, fsmap);
		else
			dbid_remove_filespace_entries(rel, i->db.dbid);

		heap_close(rel, NoLock);
	}

	if (!must_update_persistent(pridbid, i))
		return;

	/*
	 * Teach the persistent filespace table about us.
	 */
	if (add)
		persist_all_filespaces(pridbid, i->db.dbid, fsmap);
	else
		desist_all_filespaces(i->db.dbid);
}

/* As above, for tablespaces */
static void
update_tablespaces(int16 pridbid, seginfo *i, bool add)
{
	/* if we're adding a new primary (for expansion), bail out */
	if (pridbid == i->db.dbid)
	{
		Insist(i->db.role == SEGMENT_ROLE_PRIMARY);
		return;
	}

	if (pridbid == MASTER_DBID ||
		!must_update_persistent(pridbid, i))
		return;

	/*
	 * Teach the persistent tablespace table about us.
	 */
	if (add)
		PersistentTablespace_AddMirrorAll(pridbid, i->db.dbid);
	else
		PersistentTablespace_RemoveSegment(i->db.dbid,
										   i->db.role == SEGMENT_ROLE_MIRROR);
}

/* As above, for databases. */
static void
update_databases(int16 pridbid, seginfo *i, bool add)
{
	if (pridbid == MASTER_DBID ||
		!must_update_persistent(pridbid, i))
		return;

	/*
	 * Teach the persistent database table about us.
	 */
	if (add)
		PersistentDatabase_AddMirrorAll(pridbid, i->db.dbid);
	else
		PersistentDatabase_RemoveSegment(i->db.dbid,
										 i->db.role == SEGMENT_ROLE_MIRROR);
}

/* As above, for relations */
static void
update_relations(int16 pridbid, seginfo *i, bool add)
{
	if (pridbid == MASTER_DBID ||
		!must_update_persistent(pridbid, i))
		return;

	/*
	 * Teach the persistent relation table about us.
	 */
	if (add)
		PersistentRelation_AddMirrorAll(pridbid, i->db.dbid);
	else
		PersistentRelation_RemoveSegment(i->db.dbid,
										 i->db.role == SEGMENT_ROLE_MIRROR);
}

static void
dispatch_add_to_segment(int16 pridbid, int16 segdbid, ArrayType *fsmap)
{
	/*
	 * We want to dispatch the statement:
	 *
	 * SELECT gp_add_segment_persistent_entries(targetdbid, mirdbid, fsmap);
	 *
	 * targetdbid is the dbid of the primary for the mirror in question.
	 * mirdbid is its mirror.
	 *
	 * Unfortunately, we do not have targetted dispatch at this point in the
	 * code so we must dispatch to all segments.
	 */
	StringInfoData *q = makeStringInfo();
	FmgrInfo	flinfo;
	char	   *a;

	/*
	 * array_out caches data in flinfo, as we cannot just do a
	 * DirectFunctionCall1().
	 */
	fmgr_info(F_ARRAY_OUT, &flinfo);
	a = OutputFunctionCall(&flinfo, PointerGetDatum(fsmap));

	appendStringInfo(q,
					 "SELECT gp_add_segment_persistent_entries(%i::int2, "
					 "%i::int2, '%s');",
					 pridbid,
					 segdbid,
					 a);

	CdbDispatchCommand(q->data,
					   DF_CANCEL_ON_ERROR |
					   DF_WITH_SNAPSHOT,
					   NULL);
	pfree(q->data);
	pfree(q);
}

/*
 * Groups together the adding of knowledge about a new segment to
 * all persistent catalogs.
 */
static void
add_segment_persistent_entries(int16 pridbid, seginfo *i, ArrayType *fsmap)
{
	/*
	 * Add filespace entries in pg_filespace_entry and
	 * gp_persistent_filespace_node
	 */
	update_filespaces(pridbid, i, true, fsmap);

	/*
	 * Update gp_persistent_tablespace_node.
	 */
	update_tablespaces(pridbid, i, true);

	/*
	 * Update gp_persistent_database_node table
	 */
	update_databases(pridbid, i, true);

	/*
	 * Update gp_persistent_relation_node table
	 */
	update_relations(pridbid, i, true);

	/*
	 * If we're adding a new segment mirror, we need to dispatch to that
	 * segment's primary.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && i->db.role == SEGMENT_ROLE_MIRROR)
		dispatch_add_to_segment(pridbid, i->db.dbid, fsmap);
}

/*
 * The opposite of the above, we remove knowledge from the persistent
 * catalogs.
 */
static void
remove_segment_persistent_entries(int16 pridbid, seginfo *i)
{
	/*
	 * Remove filespace references in pg_filespace_entry and
	 * gp_persistent_filespace_node
	 */
	update_filespaces(pridbid, i, false, NULL);

	/*
	 * Update gp_persistent_tablespace_node.
	 */
	update_tablespaces(pridbid, i, false);

	/*
	 * Update gp_persistent_database_node table
	 */
	update_databases(pridbid, i, false);

	/*
	 * Update gp_persistent_relation_node table
	 */
	update_relations(pridbid, i, false);

	/*
	 * If we're removing a segment mirror, we need to dispatch to that
	 * segment's primary.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && i->db.role == SEGMENT_ROLE_MIRROR)
	{
		StringInfoData *q = makeStringInfo();

		appendStringInfo(q,
						 "SELECT gp_remove_segment_persistent_entries(%i::int2,"
						 "%i::int2);",
						 pridbid,
						 i->db.dbid);

		CdbDispatchCommand(q->data,
						   DF_CANCEL_ON_ERROR |
						   DF_WITH_SNAPSHOT,
						   NULL);
		pfree(q->data);
		pfree(q);
	}
}

/*
 * Add a new row to gp_segment_configuration.
 */
static void
add_segment_config(seginfo *i)
{
	Relation	rel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);
	Datum		values[Natts_gp_segment_configuration];
	bool		nulls[Natts_gp_segment_configuration];
	HeapTuple	tuple;

	MemSet(nulls, false, sizeof(nulls));

	values[Anum_gp_segment_configuration_dbid - 1] = Int16GetDatum(i->db.dbid);
	values[Anum_gp_segment_configuration_content - 1] = Int16GetDatum(i->db.segindex);
	values[Anum_gp_segment_configuration_role - 1] = CharGetDatum(i->db.role);
	values[Anum_gp_segment_configuration_preferred_role - 1] =
		CharGetDatum(i->db.preferred_role);
	values[Anum_gp_segment_configuration_mode - 1] =
		CharGetDatum(i->db.mode);
	values[Anum_gp_segment_configuration_status - 1] =
		CharGetDatum(i->db.status);
	values[Anum_gp_segment_configuration_port - 1] =
		Int32GetDatum(i->db.port);
	values[Anum_gp_segment_configuration_hostname - 1] =
		CStringGetTextDatum(i->db.hostname);
	values[Anum_gp_segment_configuration_address - 1] =
		CStringGetTextDatum(i->db.address);

	if (i->db.filerep_port != -1)
		values[Anum_gp_segment_configuration_replication_port - 1] =
			Int32GetDatum(i->db.filerep_port);
	else
		nulls[Anum_gp_segment_configuration_replication_port - 1] = true;

	tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);

	/* insert a new tuple */
	simple_heap_insert(rel, tuple);
	CatalogUpdateIndexes(rel, tuple);

	heap_close(rel, NoLock);
}

/*
 * Master function for adding a new segment
 */
static void
add_segment_config_entry(int16 pridbid, seginfo *i, ArrayType *fsmap)
{
	/* Add gp_segment_configuration entry */
	add_segment_config(i);

	/* and the rest */
	add_segment_persistent_entries(pridbid, i, fsmap);
}

/*
 * Remove a gp_segment_configuration entry
 */
static void
remove_segment_config(int16 dbid)
{
	int			numDel = 0;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Relation	rel;

	rel = heap_open(GpSegmentConfigRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(dbid));

	sscan = systable_beginscan(rel, GpSegmentConfigDbidIndexId, true,
							   SnapshotNow, 1, &scankey);
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(rel, &tuple->t_self);
		numDel++;
	}
	systable_endscan(sscan);

	Insist(numDel > 0);

	heap_close(rel, NoLock);
}

static void
add_segment(seginfo new_segment_information, ArrayType *file_space_map)
{
	int16		primary_dbid = new_segment_information.db.dbid;

	if (new_segment_information.db.role == GP_SEGMENT_CONFIGURATION_ROLE_MIRROR)
	{
		primary_dbid = contentid_get_dbid(new_segment_information.db.segindex, SEGMENT_ROLE_PRIMARY, false	/* false == current, not
										    * preferred, role */ );
		if (!primary_dbid)
			elog(ERROR, "contentid %i does not point to an existing segment",
				 new_segment_information.db.segindex);

		/*
		 * no mirrors should be defined
		 */
		if (segment_has_mirror(new_segment_information.db.segindex))
			elog(ERROR, "segment already has a mirror defined");

		/*
		 * figure out if the preferred role of this mirror needs to be primary
		 * or mirror (no preferred primary -- make this one the preferred
		 * primary)
		 */
		int			preferredPrimaryDbId = contentid_get_dbid(new_segment_information.db.segindex, SEGMENT_ROLE_PRIMARY, true /* preferred role */ );

		if (preferredPrimaryDbId == 0 && new_segment_information.db.preferred_role == GP_SEGMENT_CONFIGURATION_ROLE_MIRROR)
		{
			elog(NOTICE, "override preferred_role of this mirror as primary to support rebalance operation.");
			new_segment_information.db.preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
	}

	add_segment_config_entry(primary_dbid,
							 &new_segment_information, file_space_map);
}

/*
 * Tell the master about a new primary segment.
 *
 * gp_add_segment_primary(hostname, address, port,
 *				  filespace_map)
 *
 * Args:
 *   hostname - host name string
 *   address - either hostname or something else
 *   port - port number
 *   filespace_map - A 2-d mapping of filespace to path on the new node
 *
 * Returns the dbid of the new segment.
 */
Datum
gp_add_segment_primary(PG_FUNCTION_ARGS)
{
	seginfo		new;
	ArrayType  *fsmap;

	MemSet(&new, 0, sizeof(seginfo));

	if (PG_ARGISNULL(0))
		elog(ERROR, "hostname cannot be NULL");
	new.db.hostname = TextDatumGetCString(PG_GETARG_DATUM(0));

	if (PG_ARGISNULL(1))
		elog(ERROR, "address cannot be NULL");
	new.db.address = TextDatumGetCString(PG_GETARG_DATUM(1));

	if (PG_ARGISNULL(2))
		elog(ERROR, "port cannot be NULL");
	new.db.port = PG_GETARG_INT32(2);

	if (PG_ARGISNULL(3))
		elog(ERROR, "filespace_map cannot be NULL");
	fsmap = PG_GETARG_ARRAYTYPE_P(3);

	mirroring_sanity_check(MASTER_ONLY | SUPERUSER, "gp_add_segment_primary");

	new.db.segindex = get_maxcontentid() + 1;
	new.db.dbid = get_availableDbId();
	new.db.role = SEGMENT_ROLE_PRIMARY;
	new.db.preferred_role = SEGMENT_ROLE_PRIMARY;
#ifdef USE_SEGWALREP
	new.db.mode = GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC;
#else
	new.db.mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;
#endif
	new.db.status = GP_SEGMENT_CONFIGURATION_STATUS_UP;
	new.db.filerep_port = -1;

	add_segment(new, fsmap);

	PG_RETURN_INT16(new.db.dbid);
}

/*
 * Currently this is called by `gpinitsystem`.
 *
 * This method shouldn't be called at all. `gp_add_segment_primary()` and
 * `gp_add_segment_mirror()` should be used instead. This is to avoid setting
 * character values of role, preferred_role, mode, status, etc. outside database.
 */
Datum
gp_add_segment(PG_FUNCTION_ARGS)
{
	seginfo		new;
	ArrayType  *fsmap;

	MemSet(&new, 0, sizeof(seginfo));

	if (PG_ARGISNULL(0))
		elog(ERROR, "dbid cannot be NULL");
	new.db.dbid = PG_GETARG_INT16(0);

	if (PG_ARGISNULL(1))
		elog(ERROR, "content cannot be NULL");
	new.db.segindex = PG_GETARG_INT16(1);

	if (PG_ARGISNULL(2))
		elog(ERROR, "role cannot be NULL");
	new.db.role = PG_GETARG_CHAR(2);

	if (PG_ARGISNULL(3))
		elog(ERROR, "preferred_role cannot be NULL");
	new.db.preferred_role = PG_GETARG_CHAR(3);

	if (PG_ARGISNULL(4))
		elog(ERROR, "mode cannot be NULL");
	new.db.mode = PG_GETARG_CHAR(4);

	if (PG_ARGISNULL(5))
		elog(ERROR, "status cannot be NULL");
	new.db.status = PG_GETARG_CHAR(5);

	if (PG_ARGISNULL(6))
		elog(ERROR, "port cannot be NULL");
	new.db.port = PG_GETARG_INT32(6);

	if (PG_ARGISNULL(7))
		elog(ERROR, "hostname cannot be NULL");
	new.db.hostname = TextDatumGetCString(PG_GETARG_DATUM(7));

	if (PG_ARGISNULL(8))
		elog(ERROR, "address cannot be NULL");
	new.db.address = TextDatumGetCString(PG_GETARG_DATUM(8));

	if (PG_ARGISNULL(9))
		new.db.filerep_port = -1;
	else
		new.db.filerep_port = PG_GETARG_INT32(9);

	if (PG_ARGISNULL(10))
		elog(ERROR, "filespace_map cannot be NULL");
	fsmap = PG_GETARG_ARRAYTYPE_P(10);

	mirroring_sanity_check(MASTER_ONLY | SUPERUSER, "gp_add_segment");

#ifdef USE_SEGWALREP
	new.db.mode = GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC;
	elog(NOTICE, "mode is changed to GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC under walrep.");
#endif

	add_segment(new, fsmap);

	PG_RETURN_INT16(new.db.dbid);
}

/*
 * Master function to remove a segment from all catalogs
 */
static void
remove_segment(int16 pridbid, int16 mirdbid)
{
	seginfo    *i;

	/* Check that we're removing a mirror, not a primary */
	i = get_seginfo(mirdbid);

	remove_segment_config(mirdbid);

	remove_segment_persistent_entries(pridbid, i);
}

/*
 * Remove knowledge of a segment from the master.
 *
 * gp_remove_segment(dbid)
 *
 * Args:
 *   dbid - db identifier
 *
 * Returns:
 *   true on success, otherwise error.
 */
Datum
gp_remove_segment(PG_FUNCTION_ARGS)
{
	int16		dbid;

	if (PG_ARGISNULL(0))
		elog(ERROR, "dbid cannot be NULL");

	dbid = PG_GETARG_INT16(0);

	mirroring_sanity_check(MASTER_ONLY | SUPERUSER | UTILITY_MODE,
						   "gp_remove_segment");
	remove_segment(dbid, dbid);

	PG_RETURN_BOOL(true);
}

/*
 * Add a mirror of an existing segment.
 *
 * gp_add_segment_mirror(contentid, hostname, address, port,
 * 						 replication_port, filespace_map)
 */
Datum
gp_add_segment_mirror(PG_FUNCTION_ARGS)
{
	seginfo		new;
	ArrayType  *fsmap;

	if (PG_ARGISNULL(0))
		elog(ERROR, "contentid cannot be NULL");
	new.db.segindex = PG_GETARG_INT16(0);

	if (PG_ARGISNULL(1))
		elog(ERROR, "hostname cannot be NULL");
	new.db.hostname = TextDatumGetCString(PG_GETARG_DATUM(1));

	if (PG_ARGISNULL(2))
		elog(ERROR, "address cannot be NULL");
	new.db.address = TextDatumGetCString(PG_GETARG_DATUM(2));

	if (PG_ARGISNULL(3))
		elog(ERROR, "port cannot be NULL");
	new.db.port = PG_GETARG_INT32(3);

	if (PG_ARGISNULL(4))
		elog(ERROR, "replication_port cannot be NULL");
	new.db.filerep_port = PG_GETARG_INT32(4);

	if (PG_ARGISNULL(5))
		elog(ERROR, "filespace_map cannot be NULL");
	fsmap = PG_GETARG_ARRAYTYPE_P(5);

	mirroring_sanity_check(MASTER_ONLY | SUPERUSER, "gp_add_segment_mirror");

	new.db.dbid = get_availableDbId();
#ifdef USE_SEGWALREP
	new.db.mode = GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC;
	new.db.status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;
#else
	new.db.mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;
	new.db.status = GP_SEGMENT_CONFIGURATION_STATUS_UP;
#endif
	new.db.role = SEGMENT_ROLE_MIRROR;
	new.db.preferred_role = SEGMENT_ROLE_MIRROR;

	add_segment(new, fsmap);

	PG_RETURN_INT16(new.db.dbid);
}

/*
 * Remove a segment mirror.
 *
 * gp_remove_segment_mirror(contentid)
 *
 * Args:
 *   contentid - segment index at which to remove the mirror
 *
 * Returns:
 *   true upon success, otherwise throws error.
 */
Datum
gp_remove_segment_mirror(PG_FUNCTION_ARGS)
{
	int16		contentid = 0;
	Relation	rel;
	int16		pridbid;
	int16		mirdbid;

	if (PG_ARGISNULL(0))
		elog(ERROR, "dbid cannot be NULL");
	contentid = PG_GETARG_INT16(0);

	mirroring_sanity_check(MASTER_ONLY | SUPERUSER, "gp_remove_segment_mirror");

	/* avoid races */
	rel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);

	pridbid = contentid_get_dbid(contentid, SEGMENT_ROLE_PRIMARY, false /* false == current, not
								   * preferred, role */ );

	if (!pridbid)
		elog(ERROR, "no dbid for contentid %i", contentid);

	if (!segment_has_mirror(contentid))
		elog(ERROR, "segment does not have a mirror");

	mirdbid = contentid_get_dbid(contentid, SEGMENT_ROLE_MIRROR, false	/* false == current, not
								   * preferred, role */ );
	if (!mirdbid)
		elog(ERROR, "no mirror dbid for contentid %i", contentid);

	remove_segment(pridbid, mirdbid);

	heap_close(rel, NoLock);

	PG_RETURN_BOOL(true);
}

/*
 * Add a master standby.
 *
 * gp_add_master_standby(hostname, address, filespace_map, [port])
 *
 * Args:
 *  hostname - as above
 *  address - as above
 *  filespace_map - as above
 *  port - the port number of new standby
 *
 * Returns:
 *  dbid of the new standby
 */
Datum
gp_add_master_standby_port(PG_FUNCTION_ARGS)
{
	return gp_add_master_standby(fcinfo);
}

Datum
gp_add_master_standby(PG_FUNCTION_ARGS)
{
	seginfo    *i;
	seginfo		new;
	int			maxdbid;
	int16		master_dbid;
	Relation	gprel;
	ArrayType  *fsmap;

	if (PG_ARGISNULL(0))
		elog(ERROR, "host name cannot be NULL");
	if (PG_ARGISNULL(1))
		elog(ERROR, "address cannot be NULL");
	if (PG_ARGISNULL(2))
		elog(ERROR, "filespace map cannot be NULL");

	mirroring_sanity_check(MASTER_ONLY | UTILITY_MODE,
						   "gp_add_master_standby");

	fsmap = PG_GETARG_ARRAYTYPE_P(2);

	check_fsmap(fsmap);

	/* Check if the system is ok */
	if (standby_exists())
		elog(ERROR, "only a single master standby may be defined");

	/* Lock exclusively to avoid concurrent changes */
	gprel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);

	maxdbid = get_maxdbid();

	/*
	 * Don't reference GpIdentity.dbid, as it is legitimate to set -1 for -b
	 * option in utility mode.  Content ID = -1 AND role =
	 * GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY is the definition of primary
	 * master.
	 */
	master_dbid = contentid_get_dbid(MASTER_CONTENT_ID,
									 SEGMENT_ROLE_PRIMARY, false);
	i = get_seginfo(master_dbid);
	memcpy(&new, i, sizeof(seginfo));

	new.db.dbid = maxdbid + 1;
	new.db.role = SEGMENT_ROLE_MIRROR;
	new.db.preferred_role = SEGMENT_ROLE_MIRROR;
	new.db.mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;
	new.db.status = GP_SEGMENT_CONFIGURATION_STATUS_UP;

	new.db.hostname = TextDatumGetCString(PG_GETARG_TEXT_P(0));

	new.db.address = TextDatumGetCString(PG_GETARG_TEXT_P(1));

	/* Use the new port number if specified */
	if (PG_NARGS() > 3 && !PG_ARGISNULL(3))
		new.db.port = PG_GETARG_INT32(3);

	add_segment_config_entry(GpIdentity.dbid, &new, fsmap);

	heap_close(gprel, NoLock);

	/* Update shared memory for mmxlog */
	SetStandbyDbid(new.db.dbid);

	PG_RETURN_INT16(new.db.dbid);
}

/*
 * Remove the master standby.
 *
 * gp_remove_master_standby()
 *
 * Returns:
 *  true upon success otherwise false
 */
Datum
gp_remove_master_standby(PG_FUNCTION_ARGS)
{
	int16		dbid = master_standby_dbid();

	mirroring_sanity_check(SUPERUSER | MASTER_ONLY | UTILITY_MODE,
						   "gp_remove_master_standby");

	if (!dbid)
		elog(ERROR, "no master standby defined");

	remove_segment(GpIdentity.dbid, dbid);

	/* Update shared memory for mmxlog */
	SetStandbyDbid(InvalidDbid);

	PG_RETURN_BOOL(true);
}

/*
 * Run only on a segment, we use this to update the filespace locations for the
 * PRIMARY as part of building a full segment. Used by gpexpand.
 *
 * gp_prep_new_segment(filespace_map)
 *
 * Args:
 *   filespace_map - as above
 *
 * Returns:
 *   true upon success, otherwise throws error
 */
Datum
gp_prep_new_segment(PG_FUNCTION_ARGS)
{
	int16		pridbid;
	int16		mirdbid;
	ArrayType  *fsmap;

	if (PG_ARGISNULL(0))
		elog(ERROR, "filespace_map cannot be NULL");

	fsmap = PG_GETARG_ARRAYTYPE_P(0);

	/*
	 * The restriction on single-user mode (as opposed to utility mode) is
	 * because the database cannot be fully started under the postmaster when
	 * a database has been created in a filespace other than "pg_system" due
	 * to the need to invalidate the databases cache on startup.  Since this
	 * is the function to fix the filespace locations it MUST be run in a mode
	 * that can startup in this circumstance.
	 */
	mirroring_sanity_check(SUPERUSER | SEGMENT_ONLY | SINGLE_USER_MODE,
						   "gp_prep_new_segment");

	/*
	 * If the guc isn't set then this function is being called incorrectly -
	 * the guc is required to start the database when there is a database
	 * defined in a filespace other than pg_system.
	 */
	if (!gp_before_filespace_setup)
		elog(ERROR, "gp_prep_new_segment requires the database to be started "
			 "with gp_before_filespace_setup=true");

	/*
	 * Fixup the filespace locations for this segment.
	 *
	 * :HACK: Currently there are no functions to set a PRIMARY directory
	 * location but only a mirror location, however the
	 * PersistentFilespace_AddMirrors function only knows which is which based
	 * on what it is told.  So by declaring that the 'primary' is actually the
	 * 'mirror' it lets us do what we need to have happen.  The downside is
	 * that this will set the 'mirror_existence_state', but since we are about
	 * to desist the mirror directory anyways that is only a temporary
	 * problem. :HACK:
	 */
	pridbid = GpIdentity.dbid;
	mirdbid = PersistentFilespace_LookupMirrorDbid(MASTER_DBID);
	persist_all_filespaces(mirdbid, pridbid, fsmap);

	/*
	 * Clear out the existing mirror information from gp_persistent_filespace.
	 */
	desist_all_filespaces(mirdbid);

	/* Return true on success */
	PG_RETURN_BOOL(true);
}

/*
 * Perform operations necessary to mask a standby master the actual
 * master in the persistent catalogs.
 */
static void
persistent_activate_standby(int16 standbyid, int16 newdbid)
{
	PersistentFilespace_ActivateStandby(standbyid, newdbid);
	PersistentTablespace_ActivateStandby(standbyid, newdbid);
	PersistentDatabase_ActivateStandby(standbyid, newdbid);
	PersistentRelation_ActivateStandby(standbyid, newdbid);
}

static void
segment_config_activate_standby(int16 standbydbid, int16 newdbid)
{
	/* we use AccessExclusiveLock to prevent races */
	Relation	rel = heap_open(GpSegmentConfigRelationId, AccessExclusiveLock);
	HeapTuple	tuple;
	ScanKeyData scankey;
	SysScanDesc sscan;
	int			numDel = 0;

	/* first, delete the old master */
	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(newdbid));
	sscan = systable_beginscan(rel, GpSegmentConfigDbidIndexId, true,
							   SnapshotNow, 1, &scankey);
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(rel, &tuple->t_self);
		numDel++;
	}
	systable_endscan(sscan);

	if (0 == numDel)
		elog(ERROR, "cannot find old master, dbid %i", newdbid);

	/* now, set out old dbid to the new dbid */
	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(standbydbid));
	sscan = systable_beginscan(rel, GpSegmentConfigDbidIndexId, true,
							   SnapshotNow, 1, &scankey);

	tuple = systable_getnext(sscan);

	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cannot find standby, dbid %i", standbydbid);

	tuple = heap_copytuple(tuple);
	((Form_gp_segment_configuration) GETSTRUCT(tuple))->dbid = newdbid;
	((Form_gp_segment_configuration) GETSTRUCT(tuple))->role = SEGMENT_ROLE_PRIMARY;
	((Form_gp_segment_configuration) GETSTRUCT(tuple))->preferred_role = SEGMENT_ROLE_PRIMARY;

	simple_heap_update(rel, &tuple->t_self, tuple);
	CatalogUpdateIndexes(rel, tuple);

	systable_endscan(sscan);

	heap_close(rel, NoLock);
}

/*
 * Same as the above, but for pg_filespace_entry.
 */
static void
filespace_entry_activate_standby(int standbydbid, int newdbid)
{
	/*
	 * Heavy lock isn't required here, we've serialised operations with the
	 * lock on gp_segment_configuration.
	 */
	Relation	rel = heap_open(FileSpaceEntryRelationId, RowExclusiveLock);
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;

	/* First, delete the old master (no suitable index for this) */
	ScanKeyInit(&scankey,
				Anum_pg_filespace_entry_fsedbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(newdbid));
	sscan = systable_beginscan(rel, InvalidOid, false,
							   SnapshotNow, 1, &scankey);
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(rel, &tuple->t_self);
	}
	systable_endscan(sscan);

	/* Then, update new master */
	ScanKeyInit(&scankey,
				Anum_pg_filespace_entry_fsedbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(standbydbid));
	sscan = systable_beginscan(rel, InvalidOid, false,
							   SnapshotNow, 1, &scankey);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		tuple = heap_copytuple(tuple);
		((Form_pg_filespace_entry) GETSTRUCT(tuple))->fsedbid = newdbid;
		simple_heap_update(rel, &tuple->t_self, tuple);
		CatalogUpdateIndexes(rel, tuple);
	}

	systable_endscan(sscan);
	heap_close(rel, NoLock);
}

/*
 * Update pg_filespace_entry and gp_segment_configuration to activate a standby.
 * This means deleting references to the old standby.
 */
static void
catalog_activate_standby(int16 standbydbid, int16 olddbid)
{
	segment_config_activate_standby(standbydbid, olddbid);
	filespace_entry_activate_standby(standbydbid, olddbid);
}

/*
 * Activate a standby. To do this, we need to change
 *
 * 1. Check that we're actually the standby
 *    dbid 1)
 * 2. Update pg_filespace_entry, switching our entries from the old dbid
 *    to the new dbid
 * 3. Remove references to the old master
 * 4. Update the persistence tables to remove references to the master,
 *    switching our old dbid for the new one
 *
 * Things are actually a little hairy here. The reason is that in order to
 * read/write the filesystem, we need to lookup the gp_persistent_filespace_node
 * table.
 *
 * gp_activate_standby()
 *
 * Returns:
 *  true upon success, otherwise throws error.
 */
Datum
gp_activate_standby(PG_FUNCTION_ARGS)
{
	int16		olddbid = GpIdentity.dbid;
	int16		newdbid;

	newdbid = contentid_get_dbid(MASTER_CONTENT_ID, SEGMENT_ROLE_PRIMARY, true);

	/*
	 * This call comes from Startup process post checking state in pg_control
	 * file to make sure its standby. If user calls (ideally SHOULD NOT) but
	 * just for troubleshooting or wired case must make usre its only executed
	 * on Standby. So, this checking should return after matching DBIDs only
	 * for StartUp Process, to cover for case of crash after updating the
	 * catalogs during promote.
	 */
	if (am_startup && (newdbid == olddbid))
	{
		/*
		 * Job is already done, nothing needs to be done. We mostly crashed
		 * after updating the catalogs.
		 */
		PG_RETURN_BOOL(true);
	}

	mirroring_sanity_check(SUPERUSER | UTILITY_MODE | STANDBY_ONLY,
						   PG_FUNCNAME_MACRO);

	persistent_activate_standby(olddbid, newdbid);

	catalog_activate_standby(olddbid, newdbid);

	/*
	 * Tell postmaster to change dbid to the new one.  This should come last
	 * after completing catalog change, as the new value will reside after
	 * transaction abort or PANIC.
	 */
	primaryMirrorSetNewDbid(newdbid);

	/* done */
	PG_RETURN_BOOL(true);
}

/*
 * Add entries to persistent tables on a segment.
 *
 * gp_add_segment_persistent_entries(dbid, mirdbid, filespace_map)
 *
 * Args:
 *  dbid of the primary
 *  mirdbid - dbid of the mirror
 *  filespace_map - as above
 *
 * Returns:
 *  true upon success otherwise false
 *
 * Runs only at the segment level.
 */
Datum
gp_add_segment_persistent_entries(PG_FUNCTION_ARGS)
{
	int16		dbid;
	int16		mirdbid;
	ArrayType  *fsmap;
	seginfo		seg;

	if (PG_ARGISNULL(0))
		elog(ERROR, "dbid cannot be NULL");
	if (PG_ARGISNULL(1))
		elog(ERROR, "mirror_dbid cannot be NULL");
	if (PG_ARGISNULL(2))
		elog(ERROR, "filespace_map cannot be NULL");

	mirroring_sanity_check(SEGMENT_ONLY | SUPERUSER,
						   "gp_add_segment_persistent_entries");

	dbid = PG_GETARG_INT16(0);
	mirdbid = PG_GETARG_INT16(1);
	fsmap = PG_GETARG_ARRAYTYPE_P(2);

	elog(LOG, "received request to execute %s on %i, desired site %i",
		 PG_FUNCNAME_MACRO, GpIdentity.dbid, dbid);

	if (dbid != GpIdentity.dbid)
		PG_RETURN_BOOL(true);

	MemSet(&seg, 0, sizeof(seginfo));
	seg.db.dbid = mirdbid;
	seg.db.role = SEGMENT_ROLE_MIRROR;

	add_segment_persistent_entries(dbid, &seg, fsmap);

	/* tell filerep to start a full resync */
	ChangeTracking_MarkFullResync();

	PG_RETURN_BOOL(true);
}

/*
 * Opposite of gp_add_segment_persistent_entries()
 */
Datum
gp_remove_segment_persistent_entries(PG_FUNCTION_ARGS)
{
	int16		dbid;
	int16		mirdbid;
	seginfo		seg;

	if (PG_ARGISNULL(0))
		elog(ERROR, "dbid cannot be NULL");
	if (PG_ARGISNULL(1))
		elog(ERROR, "mirdbid cannot be NULL");

	mirroring_sanity_check(SEGMENT_ONLY | SUPERUSER,
						   "gp_remove_segment_persistent_entries");

	dbid = PG_GETARG_INT16(0);
	mirdbid = PG_GETARG_INT16(1);

	elog(LOG, "received request to execute %s on %i, desired site %i",
		 PG_FUNCNAME_MACRO, GpIdentity.dbid, dbid);

	if (dbid != GpIdentity.dbid)
		PG_RETURN_BOOL(true);

	MemSet(&seg, 0, sizeof(seginfo));
	seg.db.dbid = mirdbid;
	seg.db.role = SEGMENT_ROLE_MIRROR;

	remove_segment_persistent_entries(dbid, &seg);

	PG_RETURN_BOOL(true);
}

Datum
gp_request_fts_probe_scan(PG_FUNCTION_ARGS)
{
	if (Gp_role != GP_ROLE_DISPATCH)
	{
		ereport(ERROR, (errmsg("This function can only be called by master (without utility mode).")));
		PG_RETURN_BOOL(false);
	}

	FtsNotifyProber();

	PG_RETURN_BOOL(true);
}
