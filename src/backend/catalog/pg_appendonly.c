/*-------------------------------------------------------------------------
 *
 * pg_appendonly.c
 *	  routines to support manipulation of the pg_appendonly relation
 *
 * Portions Copyright (c) 2008, Greenplum Inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/pg_appendonly.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_appendonly.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "catalog/gp_fastsequence.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"

/*
 * Adds an entry into the pg_appendonly catalog table. The entry
 * includes the new relfilenode of the appendonly relation that 
 * was just created and an initial eof and reltuples values of 0
 */
void
InsertAppendOnlyEntry(Oid relid, 
					  int blocksize, 
					  int safefswritesize, 
					  int compresslevel,
					  bool checksum,
                      bool columnstore,
					  char* compresstype,
					  Oid segrelid,
					  Oid blkdirrelid,
					  Oid blkdiridxid,
					  Oid visimaprelid,
					  Oid visimapidxid)
{
	Relation	pg_appendonly_rel;
	HeapTuple	pg_appendonly_tuple = NULL;
	NameData	compresstype_name;
	bool	   *nulls;
	Datum	   *values;
	int			natts = 0;

    /*
     * Open and lock the pg_appendonly catalog.
     */
	pg_appendonly_rel = table_open(AppendOnlyRelationId, RowExclusiveLock);

	natts = Natts_pg_appendonly;
	values = palloc0(sizeof(Datum) * natts);
	nulls = palloc0(sizeof(bool) * natts);

	/*
	 * GPDB_12_MERGE_FIXME:
	 *		Consider not storing the parsed values for blocksize, compresstype,
	 *		compresslevel and checksum as those are also present in StdRdOptions
	 *		of Relcache.
	 */

	if (compresstype)
		namestrcpy(&compresstype_name, compresstype);
	else
		namestrcpy(&compresstype_name, "");
	values[Anum_pg_appendonly_relid - 1] = ObjectIdGetDatum(relid);
	values[Anum_pg_appendonly_blocksize - 1] = Int32GetDatum(blocksize);
	values[Anum_pg_appendonly_safefswritesize - 1] = Int32GetDatum(safefswritesize);
	values[Anum_pg_appendonly_compresslevel - 1] = Int32GetDatum(compresslevel);
	values[Anum_pg_appendonly_checksum - 1] = BoolGetDatum(checksum);
	values[Anum_pg_appendonly_compresstype - 1] = NameGetDatum(&compresstype_name);
	values[Anum_pg_appendonly_columnstore - 1] = BoolGetDatum(columnstore);
	values[Anum_pg_appendonly_segrelid - 1] = ObjectIdGetDatum(segrelid);
	values[Anum_pg_appendonly_blkdirrelid - 1] = ObjectIdGetDatum(blkdirrelid);
	values[Anum_pg_appendonly_blkdiridxid - 1] = ObjectIdGetDatum(blkdiridxid);
	values[Anum_pg_appendonly_visimaprelid - 1] = ObjectIdGetDatum(visimaprelid);
	values[Anum_pg_appendonly_visimapidxid - 1] = ObjectIdGetDatum(visimapidxid);

	/*
	 * form the tuple and insert it
	 */
	pg_appendonly_tuple = heap_form_tuple(RelationGetDescr(pg_appendonly_rel), values, nulls);

	/* insert a new tuple */
	CatalogTupleInsert(pg_appendonly_rel, pg_appendonly_tuple);

	/*
     * Close the pg_appendonly_rel relcache entry without unlocking.
     * We have updated the catalog: consequently the lock must be 
	 * held until end of transaction.
     */
    table_close(pg_appendonly_rel, NoLock);

	pfree(values);
	pfree(nulls);

}

void
GetAppendOnlyEntryAttributes(Oid relid,
							 int32 *blocksize,
							 int32 *safefswritesize,
							 int16 *compresslevel,
							 bool *checksum,
							 NameData *compresstype)
{
	Relation	pg_appendonly;
	TupleDesc	tupDesc;
	ScanKeyData key[1];
	SysScanDesc scan;
	HeapTuple	tuple;
	bool isNull;
	Datum dat;

	pg_appendonly = table_open(AppendOnlyRelationId, AccessShareLock);
	tupDesc = RelationGetDescr(pg_appendonly);

	ScanKeyInit(&key[0],
				Anum_pg_appendonly_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pg_appendonly, AppendOnlyRelidIndexId, true,
							  NULL, 1, key);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("missing pg_appendonly entry for relation \"%s\"",
						get_rel_name(relid))));

	if (blocksize != NULL)
	{
		dat = heap_getattr(tuple,
							  Anum_pg_appendonly_blocksize,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segrelid value: NULL")));

		*blocksize = DatumGetInt32(dat);
	}

	if (safefswritesize != NULL)
	{
		dat = heap_getattr(tuple,
							  Anum_pg_appendonly_safefswritesize,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segrelid value: NULL")));

		*safefswritesize = DatumGetInt32(dat);
	}

	if (compresslevel != NULL)
	{
		dat = heap_getattr(tuple,
							  Anum_pg_appendonly_compresslevel,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segrelid value: NULL")));

		*compresslevel = DatumGetInt16(dat);
	}

	if (checksum != NULL)
	{
		dat = heap_getattr(tuple,
							  Anum_pg_appendonly_checksum,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segrelid value: NULL")));

		*checksum = DatumGetBool(dat);
	}

	if (compresstype != NULL)
	{
		dat = heap_getattr(tuple,
							  Anum_pg_appendonly_compresstype,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segrelid value: NULL")));

		*compresstype = *DatumGetName(dat);
	}

	/* Finish up scan and close pg_appendonly catalog. */
	systable_endscan(scan);
	table_close(pg_appendonly, AccessShareLock);
}

/*
 * Get the OIDs of the auxiliary relations and their indexes for an appendonly
 * relation.
 *
 * The OIDs will be retrieved only when the corresponding output variable is
 * not NULL.
 *
 * 'appendOnlyMetaDataSnapshot' can be passed as NULL, which means use the
 * latest snapshot, like in systable_beginscan.
 */
void
GetAppendOnlyEntryAuxOids(Oid relid,
						  Snapshot appendOnlyMetaDataSnapshot,
						  Oid *segrelid,
						  Oid *blkdirrelid,
						  Oid *blkdiridxid,
						  Oid *visimaprelid,
						  Oid *visimapidxid)
{
	Relation	pg_appendonly;
	TupleDesc	tupDesc;
	ScanKeyData key[1];
	SysScanDesc scan;
	HeapTuple	tuple;
	Datum auxOid;
	bool isNull;
	
	/*
	 * Check the pg_appendonly relation to be certain the ao table 
	 * is there. 
	 */
	pg_appendonly = table_open(AppendOnlyRelationId, AccessShareLock);
	tupDesc = RelationGetDescr(pg_appendonly);

	ScanKeyInit(&key[0],
				Anum_pg_appendonly_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pg_appendonly, AppendOnlyRelidIndexId, true,
							  appendOnlyMetaDataSnapshot, 1, key);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("missing pg_appendonly entry for relation \"%s\"",
						get_rel_name(relid))));

	if (segrelid != NULL)
	{
		auxOid = heap_getattr(tuple,
							  Anum_pg_appendonly_segrelid,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segrelid value: NULL")));	
		
		*segrelid = DatumGetObjectId(auxOid);
	}

	if (blkdirrelid != NULL)
	{
		auxOid = heap_getattr(tuple,
							  Anum_pg_appendonly_blkdirrelid,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid blkdirrelid value: NULL")));	
		
		*blkdirrelid = DatumGetObjectId(auxOid);
	}

	if (blkdiridxid != NULL)
	{
		auxOid = heap_getattr(tuple,
							  Anum_pg_appendonly_blkdiridxid,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid blkdiridxid value: NULL")));	
		
		*blkdiridxid = DatumGetObjectId(auxOid);
	}

	if (visimaprelid != NULL)
	{
		auxOid = heap_getattr(tuple,
							  Anum_pg_appendonly_visimaprelid,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid visimaprelid value: NULL")));	
		
		*visimaprelid = DatumGetObjectId(auxOid);
	}

	if (visimapidxid != NULL)
	{
		auxOid = heap_getattr(tuple,
							  Anum_pg_appendonly_visimapidxid,
							  tupDesc,
							  &isNull);
		Assert(!isNull);
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid visimapidxid value: NULL")));	
		
		*visimapidxid = DatumGetObjectId(auxOid);
	}
	/* Finish up scan and close pg_appendonly catalog. */
	systable_endscan(scan);
	table_close(pg_appendonly, AccessShareLock);
}

/*
 * Update the segrelid and/or blkdirrelid if the input new values
 * are valid OIDs.
 */
void
UpdateAppendOnlyEntryAuxOids(Oid relid,
							 Oid newSegrelid,
							 Oid newBlkdirrelid,
							 Oid newBlkdiridxid,
							 Oid newVisimaprelid,
							 Oid newVisimapidxid)
{
	Relation	pg_appendonly;
	ScanKeyData key[1];
	SysScanDesc scan;
	HeapTuple	tuple, newTuple;
	Datum		newValues[Natts_pg_appendonly];
	bool		newNulls[Natts_pg_appendonly];
	bool		replace[Natts_pg_appendonly];
	
	/*
	 * Check the pg_appendonly relation to be certain the ao table 
	 * is there. 
	 */
	pg_appendonly = table_open(AppendOnlyRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_appendonly_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pg_appendonly, AppendOnlyRelidIndexId, true,
							  NULL, 1, key);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("missing pg_appendonly entry for relation \"%s\"",
						get_rel_name(relid))));

	MemSet(newValues, 0, sizeof(newValues));
	MemSet(newNulls, false, sizeof(newNulls));
	MemSet(replace, false, sizeof(replace));

	if (OidIsValid(newSegrelid))
	{
		replace[Anum_pg_appendonly_segrelid - 1] = true;
		newValues[Anum_pg_appendonly_segrelid - 1] = newSegrelid;
	}

	if (OidIsValid(newBlkdirrelid))
	{
		replace[Anum_pg_appendonly_blkdirrelid - 1] = true;
		newValues[Anum_pg_appendonly_blkdirrelid - 1] = newBlkdirrelid;
	}
	
	if (OidIsValid(newBlkdiridxid))
	{
		replace[Anum_pg_appendonly_blkdiridxid - 1] = true;
		newValues[Anum_pg_appendonly_blkdiridxid - 1] = newBlkdiridxid;
	}
	
	if (OidIsValid(newVisimaprelid))
	{
		replace[Anum_pg_appendonly_visimaprelid - 1] = true;
		newValues[Anum_pg_appendonly_visimaprelid - 1] = newVisimaprelid;
	}
	
	if (OidIsValid(newVisimapidxid))
	{
		replace[Anum_pg_appendonly_visimapidxid - 1] = true;
		newValues[Anum_pg_appendonly_visimapidxid - 1] = newVisimapidxid;
	}
	newTuple = heap_modify_tuple(tuple, RelationGetDescr(pg_appendonly),
								 newValues, newNulls, replace);
	CatalogTupleUpdate(pg_appendonly, &newTuple->t_self, newTuple);

	heap_freetuple(newTuple);

	/* Finish up scan and close appendonly catalog. */
	systable_endscan(scan);
	table_close(pg_appendonly, RowExclusiveLock);

	/* Also cause flush the relcache entry for the parent relation. */
	CacheInvalidateRelcacheByRelid(relid);
}

/*
 * Remove all pg_appendonly entries that the table we are DROPing
 * refers to (using the table's relfilenode)
 *
 * The gp_fastsequence entries associate with the table is also
 * deleted here.
 */
void
RemoveAppendonlyEntry(Oid relid)
{
	Relation	pg_appendonly_rel;
	ScanKeyData key[1];
	SysScanDesc scan;
	HeapTuple	tuple;
	Oid aosegrelid = InvalidOid;
	
	/*
	 * now remove the pg_appendonly entry 
	 */
	pg_appendonly_rel = table_open(AppendOnlyRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_appendonly_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pg_appendonly_rel, AppendOnlyRelidIndexId, true,
							  NULL, 1, key);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("appendonly table relid \"%d\" does not exist in "
						"pg_appendonly", relid)));
	
	{
		bool isNull;
		Datum datum = heap_getattr(tuple,
								   Anum_pg_appendonly_segrelid,
								   RelationGetDescr(pg_appendonly_rel),
								   &isNull);

		Assert(!isNull);
		aosegrelid = DatumGetObjectId(datum);
		Assert(OidIsValid(aosegrelid));
	}

	/* Piggyback here to remove gp_fastsequence entries */
	RemoveFastSequenceEntry(aosegrelid);

	/*
	 * Delete the appendonly table entry from the catalog (pg_appendonly).
	 */
	simple_heap_delete(pg_appendonly_rel, &tuple->t_self);
	
	/* Finish up scan and close appendonly catalog. */
	systable_endscan(scan);
	table_close(pg_appendonly_rel, NoLock);
}

static void
TransferDependencyLink(
	Oid baseOid, 
	Oid oid,
	const char *tabletype)
{
	ObjectAddress 	baseobject;
	ObjectAddress	newobject;
	long			count;

	MemSet(&baseobject, 0, sizeof(ObjectAddress));
	MemSet(&newobject, 0, sizeof(ObjectAddress));

	Assert(OidIsValid(baseOid));
	Assert(OidIsValid(oid));

	/* Delete old dependency */
	count = deleteDependencyRecordsFor(RelationRelationId, oid, false);
	if (count != 1)
		elog(LOG, "expected one dependency record for %s table, oid %u, found %ld",
			 tabletype, oid, count);

	/* Register new dependencies */
	baseobject.classId = RelationRelationId;
	baseobject.objectId = baseOid;
	newobject.classId = RelationRelationId;
	newobject.objectId = oid;
	
	recordDependencyOn(&newobject, &baseobject, DEPENDENCY_INTERNAL);
}

static HeapTuple 
GetAppendEntryForMove(
	Relation	pg_appendonly_rel,
	TupleDesc	pg_appendonly_dsc,
	Oid 		relId,
	Oid 		*aosegrelid,
	Oid 		*aoblkdirrelid,
	Oid         *aovisimaprelid)
{
	ScanKeyData key[1];
	SysScanDesc scan;
	HeapTuple	tuple;
	bool		isNull;

	ScanKeyInit(&key[0],
				Anum_pg_appendonly_relid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relId));

	scan = systable_beginscan(pg_appendonly_rel, AppendOnlyRelidIndexId, true,
							  NULL, 1, key);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
	{
		systable_endscan(scan);
		return tuple;
	}

    *aosegrelid = heap_getattr(tuple,
							   Anum_pg_appendonly_segrelid,
							   pg_appendonly_dsc,
							   &isNull);
    Assert(!isNull);
	if(isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid segrelid value: NULL")));	

    *aoblkdirrelid = heap_getattr(tuple,
								  Anum_pg_appendonly_blkdirrelid,
								  pg_appendonly_dsc,
								  &isNull);
    Assert(!isNull);
	if(isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid blkdirrelid value: NULL")));	
 
	*aovisimaprelid = heap_getattr(tuple,
								   Anum_pg_appendonly_visimaprelid,
								   pg_appendonly_dsc,
								   &isNull);
    Assert(!isNull);
	if(isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid visimaprelid value: NULL")));	

	tuple = heap_copytuple(tuple);

	/* Finish up the scan. */
	systable_endscan(scan);

	return tuple;
}

/*
 * Swap pg_appendonly entries between tables.
 */
void
SwapAppendonlyEntries(Oid entryRelId1, Oid entryRelId2)
{
	Relation	pg_appendonly_rel;
	TupleDesc	pg_appendonly_dsc;
	HeapTuple	tupleCopy1;
	HeapTuple	tupleCopy2;
	Datum 		*newValues;
	bool 		*newNulls;
	bool 		*replace;
	Oid			aosegrelid1;
	Oid			aoblkdirrelid1;
	Oid			aovisimaprelid1;
	Oid			aosegrelid2;
	Oid			aoblkdirrelid2;
	Oid			aovisimaprelid2;

	pg_appendonly_rel = table_open(AppendOnlyRelationId, RowExclusiveLock);
	pg_appendonly_dsc = RelationGetDescr(pg_appendonly_rel);
	
	tupleCopy1 = GetAppendEntryForMove(
							pg_appendonly_rel,
							pg_appendonly_dsc,
							entryRelId1,
							&aosegrelid1,
							&aoblkdirrelid1,
							&aovisimaprelid1);

	tupleCopy2 = GetAppendEntryForMove(
							pg_appendonly_rel,
							pg_appendonly_dsc,
							entryRelId2,
							&aosegrelid2,
							&aoblkdirrelid2,
							&aovisimaprelid2);

	if (!HeapTupleIsValid(tupleCopy1) || !HeapTupleIsValid(tupleCopy2))
	{
		if (HeapTupleIsValid(tupleCopy1) || HeapTupleIsValid(tupleCopy2))
			elog(ERROR, "swapping pg_appendonly entries is not permitted for non-appendoptimized tables");
		table_close(pg_appendonly_rel, NoLock);
		return;
	}

	/* Since gp_fastsequence entry is referenced by aosegrelid, it rides along  */
	simple_heap_delete(pg_appendonly_rel, &tupleCopy1->t_self);
	simple_heap_delete(pg_appendonly_rel, &tupleCopy2->t_self);

	/*
	 * (Re)insert.
	 */
	newValues = palloc0(pg_appendonly_dsc->natts * sizeof(Datum));
	newNulls = palloc0(pg_appendonly_dsc->natts * sizeof(bool));
	replace = palloc0(pg_appendonly_dsc->natts * sizeof(bool));

	replace[Anum_pg_appendonly_relid - 1] = true;
	newValues[Anum_pg_appendonly_relid - 1] = entryRelId2;

	tupleCopy1 = heap_modify_tuple(tupleCopy1, pg_appendonly_dsc,
								  newValues, newNulls, replace);

	CatalogTupleInsert(pg_appendonly_rel, tupleCopy1);

	heap_freetuple(tupleCopy1);

	newValues[Anum_pg_appendonly_relid - 1] = entryRelId1;

	tupleCopy2 = heap_modify_tuple(tupleCopy2, pg_appendonly_dsc,
								  newValues, newNulls, replace);

	CatalogTupleInsert(pg_appendonly_rel, tupleCopy2);

	heap_freetuple(tupleCopy2);

	table_close(pg_appendonly_rel, NoLock);

	pfree(newValues);
	pfree(newNulls);
	pfree(replace);

	if ((aosegrelid1 || aosegrelid2) && (aosegrelid1 != aosegrelid2))
	{
		if (OidIsValid(aosegrelid1))
		{
			TransferDependencyLink(entryRelId2, aosegrelid1, "aoseg");
		}
		if (OidIsValid(aosegrelid2))
		{
			TransferDependencyLink(entryRelId1, aosegrelid2, "aoseg");
		}
	}
	
	if ((aoblkdirrelid1 || aoblkdirrelid2) && (aoblkdirrelid1 != aoblkdirrelid2))
	{
		if (OidIsValid(aoblkdirrelid1))
		{
			TransferDependencyLink(entryRelId2, aoblkdirrelid1, "aoblkdir");
		}
		if (OidIsValid(aoblkdirrelid2))
		{
			TransferDependencyLink(entryRelId1, aoblkdirrelid2, "aoblkdir");
		}
	}
	
	if ((aovisimaprelid1 || aovisimaprelid2) && (aovisimaprelid1 != aovisimaprelid2))
	{
		if (OidIsValid(aovisimaprelid1))
		{
			TransferDependencyLink(entryRelId2, aovisimaprelid1, "aovisimap");
		}
		if (OidIsValid(aovisimaprelid2))
		{
			TransferDependencyLink(entryRelId1, aovisimaprelid2, "aovisimap");
		}
	}
}

