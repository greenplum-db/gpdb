/*-------------------------------------------------------------------------
 *
 * aosegfiles.c
 *	  routines to support manipulation of the pg_aoseg_<oid> relation
 *	  that accompanies each Append Only relation.
 *
 * Portions Copyright (c) 2008, Greenplum Inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/aosegfiles.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "funcapi.h"
#include "miscadmin.h"
#include "access/heapam.h"
#include "access/genam.h"
#include "access/aocssegfiles.h"
#include "access/aosegfiles.h"
#include "access/appendonlytid.h"
#include "access/appendonlywriter.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/pg_am.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/gp_fastsequence.h"
#include "cdb/cdbvars.h"
#include "executor/spi.h"
#include "nodes/makefuncs.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/fmgroids.h"
#include "utils/numeric.h"
#include "utils/visibility_summary.h"

static float8 aorow_compression_ratio_internal(Relation parentrel);
static void UpdateFileSegInfo_internal(Relation parentrel,
						   int segno,
						   int64 eof,
						   int64 eof_uncompressed,
						   int64 tuples_added,
						   int64 varblocks_added,
						   int64 modcount_added,
						   FileSegInfoState newState);
static FileSegInfo **GetAllFileSegInfo_pg_aoseg_rel(char *relationName, Relation pg_aoseg_rel, Snapshot appendOnlyMetaDataSnapshot, int *totalsegs);


/* ------------------------------------------------------------------------
 *
 * FUNCTIONS FOR MANIPULATING AND QUERYING AO SEGMENT FILE CATALOG TABLES
 *
 * ------------------------------------------------------------------------
 */

void
ValidateAppendonlySegmentDataBeforeStorage(int segno)
{
	if (segno >= MAX_AOREL_CONCURRENCY || segno < 0)
		ereport(ERROR, (errmsg("expected segment number to be between zero and the maximum number of concurrent writers, actually %d", segno)));
}

/*
 * InsertFileSegInfo
 *
 * Adds an entry into the pg_aoseg_*  table for this Append
 * Only relation. Use use frozen_heap_insert so the tuple is
 * frozen on insert.
 *
 * Also insert a new entry to gp_fastsequence for this segment file.
 */
void
InsertInitialSegnoEntry(Relation parentrel, int segno)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	HeapTuple	pg_aoseg_tuple;
	Buffer		buf = InvalidBuffer;
	TM_Result	result;
	TM_FailureData hufd;
	int			natts;
	bool	   *nulls;
	Datum	   *values;
	int16		formatVersion;
	Oid segrelid;

	ValidateAppendonlySegmentDataBeforeStorage(segno);

	/* New segments are always created in the latest format */
	formatVersion = AORelationVersion_GetLatest();

	GetAppendOnlyEntryAuxOids(parentrel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

	InsertFastSequenceEntry(segrelid,
							(int64) segno,
							0);

	pg_aoseg_rel = heap_open(segrelid, RowExclusiveLock);

	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);
	natts = pg_aoseg_dsc->natts;
	nulls = palloc(sizeof(bool) * natts);
	values = palloc0(sizeof(Datum) * natts);
	MemSet(nulls, false, sizeof(char) * natts);

	values[Anum_pg_aoseg_segno - 1] = Int32GetDatum(segno);
	values[Anum_pg_aoseg_tupcount - 1] = Int64GetDatum(0);
	values[Anum_pg_aoseg_varblockcount - 1] = Int64GetDatum(0);
	values[Anum_pg_aoseg_eof - 1] = Int64GetDatum(0);
	values[Anum_pg_aoseg_eofuncompressed - 1] = Int64GetDatum(0);
	values[Anum_pg_aoseg_modcount - 1] = Int64GetDatum(0);
	values[Anum_pg_aoseg_formatversion - 1] = Int16GetDatum(formatVersion);
	values[Anum_pg_aoseg_state - 1] = Int16GetDatum(AOSEG_STATE_DEFAULT);

	/*
	 * form the tuple and insert it
	 */
	pg_aoseg_tuple = heap_form_tuple(pg_aoseg_dsc, values, nulls);
	if (!HeapTupleIsValid(pg_aoseg_tuple))
		elog(ERROR, "failed to build AO file segment tuple");

	CatalogTupleInsertFrozen(pg_aoseg_rel, pg_aoseg_tuple);

	/*
	 * Lock the tuple so that a concurrent insert transaction will not
	 * consider this segfile for insertion. This should succeed since
	 * we just inserted the row, and the caller is holding a lock that
	 * prevents concurrent lockers.
	 */
	result = heap_lock_tuple(pg_aoseg_rel, pg_aoseg_tuple,
							 GetCurrentCommandId(true),
							 LockTupleExclusive,
							 LockWaitSkip,
							 false, /* follow_updates */
							 &buf,
							 &hufd);
	if (result != TM_Ok)
		elog(ERROR, "could not lock newly-inserted gp_fastsequence tuple");
	if (BufferIsValid(buf))
		ReleaseBuffer(buf);

	heap_freetuple(pg_aoseg_tuple);
	table_close(pg_aoseg_rel, RowExclusiveLock);
}

/*
 * GetFileSegInfo
 *
 * Get the catalog entry for an appendonly (row-oriented) relation from the
 * pg_aoseg_* relation that belongs to the currently used
 * AppendOnly table.
 *
 * If a caller intends to append to this file segment entry they must have
 * already locked the pg_aoseg tuple earlier, in order to guarantee stability
 * of the pg_aoseg information on this segment file and exclusive right to
 * append data to the segment file. In that case, 'locked' should be passed
 * as true.
 */
FileSegInfo *
GetFileSegInfo(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot, int segno,
			   bool locked)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	SysScanDesc aoscan;
	HeapTuple	tuple = NULL;
	HeapTuple	fstuple = NULL;
	int			tuple_segno = InvalidFileSegNumber;
	bool		isNull;
	FileSegInfo *fsinfo;
	Oid segrelid;

	GetAppendOnlyEntryAuxOids(parentrel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

	/*
	 * Check the pg_aoseg relation to be certain the ao table segment file is
	 * there.
	 */
	pg_aoseg_rel = table_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	/* Do heap scan on pg_aoseg relation */
	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, appendOnlyMetaDataSnapshot, 0, NULL);
	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(tuple, Anum_pg_aoseg_segno, pg_aoseg_dsc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&tuple->t_self))));

		/* Check for duplicate aoseg entries with the same segno */
		if (segno == tuple_segno)
		{
			if (fstuple != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("found two entries in \"%s\" with segno %d: "
								"(ctid %s with eof " INT64_FORMAT ") and (ctid %s with eof " INT64_FORMAT ")",
								RelationGetRelationName(pg_aoseg_rel),
								segno,
								ItemPointerToString(&fstuple->t_self),
								DatumGetInt64(
											  fastgetattr(fstuple, Anum_pg_aoseg_eof, pg_aoseg_dsc, &isNull)),
								ItemPointerToString2(&tuple->t_self),
								DatumGetInt64(
											  fastgetattr(tuple, Anum_pg_aoseg_eof, pg_aoseg_dsc, &isNull)))));
			else
				fstuple = heap_copytuple(tuple);
		}
	}

	if (!HeapTupleIsValid(fstuple))
	{
		/* This segment file does not have an entry. */
		systable_endscan(aoscan);
		table_close(pg_aoseg_rel, AccessShareLock);

		if (locked)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("could not find segno %d for relation %s",
							segno, RelationGetRelationName(parentrel))));
		return NULL;
	}

	if (locked && !pg_aoseg_tuple_is_locked_by_me(fstuple))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("segno %d for relation %s is not locked for us",
						segno, RelationGetRelationName(parentrel))));

	fsinfo = (FileSegInfo *) palloc0(sizeof(FileSegInfo));

	/* get the eof */
	fsinfo->eof = DatumGetInt64(
								fastgetattr(fstuple, Anum_pg_aoseg_eof, pg_aoseg_dsc, &isNull));

	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid eof value: NULL")));

	if (fsinfo->eof < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("invalid eof " INT64_FORMAT " for relation %s",
						fsinfo->eof, RelationGetRelationName(parentrel))));

	/* get the tupcount */
	fsinfo->total_tupcount = DatumGetInt64(
										   fastgetattr(fstuple, Anum_pg_aoseg_tupcount, pg_aoseg_dsc, &isNull));

	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid tupcount value: NULL")));

	/* get the varblock count */
	fsinfo->varblockcount = DatumGetInt64(
										  fastgetattr(fstuple, Anum_pg_aoseg_varblockcount, pg_aoseg_dsc, &isNull));

	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid varblockcount value: NULL")));

	/* get the modcount */
	fsinfo->modcount = DatumGetInt64(
									 fastgetattr(fstuple, Anum_pg_aoseg_modcount, pg_aoseg_dsc, &isNull));

	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid modcount value: NULL")));

	/* get the file format version number */
	fsinfo->formatversion = DatumGetInt16(
										  fastgetattr(fstuple, Anum_pg_aoseg_formatversion, pg_aoseg_dsc, &isNull));

	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid formatversion value: NULL")));
	AORelationVersion_CheckValid(fsinfo->formatversion);

	/* get the state */
	fsinfo->state = DatumGetInt16(
								  fastgetattr(fstuple, Anum_pg_aoseg_state, pg_aoseg_dsc, &isNull));

	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid state value: NULL")));

	/* get the uncompressed eof */
	fsinfo->eof_uncompressed = DatumGetInt64(
											 fastgetattr(fstuple, Anum_pg_aoseg_eofuncompressed, pg_aoseg_dsc, &isNull));
	if (isNull)
	{
		/*
		 * NULL is allowed. Tables that were created before the release of the
		 * eof_uncompressed catalog column will have a NULL instead of a
		 * value.
		 */
		fsinfo->eof_uncompressed = InvalidUncompressedEof;
	}

	fsinfo->segno = segno;

	/* Finish up scan and close appendonly catalog. */
	heap_freetuple(fstuple);
	systable_endscan(aoscan);
	table_close(pg_aoseg_rel, AccessShareLock);

	return fsinfo;
}


/*
 * GetAllFileSegInfo
 *
 * Get all catalog entries for an appendonly relation from the
 * pg_aoseg_* relation that belongs to the currently used
 * AppendOnly table. This is basically a physical snapshot that a
 * scanner can use to scan all the data in a local segment database.
 */
FileSegInfo **
GetAllFileSegInfo(Relation parentrel,
				  Snapshot appendOnlyMetaDataSnapshot,
				  int *totalsegs)
{
	Relation	pg_aoseg_rel;
	FileSegInfo **result;
	Oid segrelid;

	GetAppendOnlyEntryAuxOids(parentrel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

	if (segrelid == InvalidOid)
		elog(ERROR, "could not find pg_aoseg aux table for AO table \"%s\"",
			 RelationGetRelationName(parentrel));

	Assert(RelationIsAoRows(parentrel));

	pg_aoseg_rel = table_open(segrelid, AccessShareLock);

	result = GetAllFileSegInfo_pg_aoseg_rel(RelationGetRelationName(parentrel),
											pg_aoseg_rel,
											appendOnlyMetaDataSnapshot,
											totalsegs);

	table_close(pg_aoseg_rel, AccessShareLock);

	return result;
}


/*
 * The comparison routine that sorts an array of FileSegInfos
 * in the ascending order of the segment number.
 */
static int
aoFileSegInfoCmp(const void *left, const void *right)
{
	FileSegInfo *leftSegInfo = *((FileSegInfo **) left);
	FileSegInfo *rightSegInfo = *((FileSegInfo **) right);

	if (leftSegInfo->segno < rightSegInfo->segno)
		return -1;

	if (leftSegInfo->segno > rightSegInfo->segno)
		return 1;

	return 0;
}

static FileSegInfo **
GetAllFileSegInfo_pg_aoseg_rel(char *relationName,
							   Relation pg_aoseg_rel,
							   Snapshot appendOnlyMetaDataSnapshot,
							   int *totalsegs)
{
	TupleDesc	pg_aoseg_dsc;
	HeapTuple	tuple;
	SysScanDesc aoscan;
	FileSegInfo **allseginfo;
	int			seginfo_no;
	int			seginfo_slot_no = AO_FILESEGINFO_ARRAY_SIZE;
	Datum		segno,
				eof,
				eof_uncompressed,
				tupcount,
				varblockcount,
				modcount,
				formatversion,
				state;
	bool		isNull;

	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	/*
	 * MPP-16407: Initialize the segment file information array, we first
	 * allocate 8 slot for the array, then array will be dynamically expanded
	 * later if necessary.
	 */
	allseginfo = (FileSegInfo **) palloc0(sizeof(FileSegInfo *) * seginfo_slot_no);
	seginfo_no = 0;

	/*
	 * Now get the actual segfile information
	 */
	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, appendOnlyMetaDataSnapshot, 0, NULL);
	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		/* dynamically expand space for FileSegInfo* array */
		if (seginfo_no >= seginfo_slot_no)
		{
			seginfo_slot_no *= 2;
			allseginfo = (FileSegInfo **) repalloc(allseginfo, sizeof(FileSegInfo *) * seginfo_slot_no);
		}

		FileSegInfo *oneseginfo;

		allseginfo[seginfo_no] = (FileSegInfo *) palloc0(sizeof(FileSegInfo));
		oneseginfo = allseginfo[seginfo_no];

		GetTupleVisibilitySummary(
								  tuple,
								  &oneseginfo->tupleVisibilitySummary);

		segno = fastgetattr(tuple, Anum_pg_aoseg_segno, pg_aoseg_dsc, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value: NULL")));
		oneseginfo->segno = DatumGetInt32(segno);

		/* get the eof */
		eof = fastgetattr(tuple, Anum_pg_aoseg_eof, pg_aoseg_dsc, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid eof value: NULL")));
		oneseginfo->eof += DatumGetInt64(eof);

		/* get the tupcount */
		tupcount = fastgetattr(tuple, Anum_pg_aoseg_tupcount, pg_aoseg_dsc, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid tupcount value: NULL")));
		oneseginfo->total_tupcount += DatumGetInt64(tupcount);

		/* get the varblock count */
		varblockcount = fastgetattr(tuple, Anum_pg_aoseg_varblockcount, pg_aoseg_dsc, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid varblockcount value: NULL")));
		oneseginfo->varblockcount += DatumGetInt64(varblockcount);

		/* get the modcount */
		modcount = fastgetattr(tuple, Anum_pg_aoseg_modcount, pg_aoseg_dsc, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid modcount value: NULL")));
		oneseginfo->modcount += DatumGetInt64(modcount);

		/* get the file format version number */
		formatversion = fastgetattr(tuple, Anum_pg_aoseg_formatversion, pg_aoseg_dsc, &isNull);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid formatversion value: NULL")));

		AORelationVersion_CheckValid(formatversion);
		oneseginfo->formatversion = DatumGetInt16(formatversion);

		/* get the state */
		state = fastgetattr(tuple, Anum_pg_aoseg_state, pg_aoseg_dsc, &isNull);
		Assert(!isNull || appendOnlyMetaDataSnapshot == SnapshotAny);
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid state value: NULL")));
		else
			oneseginfo->state = DatumGetInt16(state);

		/* get the uncompressed eof */
		eof_uncompressed = fastgetattr(tuple, Anum_pg_aoseg_eofuncompressed, pg_aoseg_dsc, &isNull);
		if (isNull)
		{
			/*
			 * NULL is allowed. Tables that were created before the release of
			 * the eof_uncompressed catalog column will have a NULL instead of
			 * a value.
			 */
			oneseginfo->eof_uncompressed = InvalidUncompressedEof;
		}
		else
			oneseginfo->eof_uncompressed += DatumGetInt64(eof_uncompressed);

		elogif(Debug_appendonly_print_scan, LOG,
			   "Append-only found existing segno %d with eof " INT64_FORMAT " for table '%s'",
			   oneseginfo->segno,
			   oneseginfo->eof,
			   relationName);
		seginfo_no++;

		CHECK_FOR_INTERRUPTS();
	}
	systable_endscan(aoscan);

	*totalsegs = seginfo_no;

	if (*totalsegs == 0)
	{
		pfree(allseginfo);
		return NULL;
	}

	/*
	 * Sort allseginfo by the order of segment file number.
	 *
	 * Currently this is only needed when building a bitmap index to guarantee
	 * the tids are in the ascending order. But since this array is pretty
	 * small, we just sort the array for all cases.
	 */
	qsort((char *) allseginfo, *totalsegs, sizeof(FileSegInfo *), aoFileSegInfoCmp);

	return allseginfo;
}

/*
 * Change an pg_aoseg row from DEFAULT to AWAITING_DROP.
 */
void
MarkFileSegInfoAwaitingDrop(Relation parentrel, int segno)
{
	if (Debug_appendonly_print_compaction)
		elog(LOG,
			 "changing state of segfile %d of table '%s' to AWAITING_DROP",
			 segno,
			 RelationGetRelationName(parentrel));

	UpdateFileSegInfo_internal(parentrel,
							   segno,
							   -1,
							   -1,
							   0,
							   0,
							   0,
							   AOSEG_STATE_AWAITING_DROP);
}

void
IncrementFileSegInfoModCount(Relation parentrel,
							 int segno)
{
	elogif(Debug_appendonly_print_compaction, LOG,
		   "Increment segfile info modcount: segno %d, table '%s'",
		   segno,
		   RelationGetRelationName(parentrel));

	UpdateFileSegInfo_internal(parentrel,
							   segno,
							   -1,
							   -1,
							   0,
							   0,
							   1,
							   AOSEG_STATE_USECURRENT);
}


/*
 * Reset state of an  pg_aoseg row from AWAITING_DROP to DEFAULT state.
 *
 * Also clears tupcount, varblockcount, and EOFs, and sets formatversion to
 * the latest version. 'modcount' is not changed.
 *
 * The caller should have checked that the segfile is no longer needed by
 * any running transaction. It is not necessary to hold a lock on the segfile
 * row, though.
 */
void
ClearFileSegInfo(Relation parentrel, int segno)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	TableScanDesc aoscan;
	HeapTuple	tuple = NULL;
	HeapTuple	new_tuple;
	int			tuple_segno = InvalidFileSegNumber;
	Datum	   *new_record;
	bool	   *new_record_nulls;
	bool	   *new_record_repl;
	bool		isNull;
	Oid segrelid;

	GetAppendOnlyEntryAuxOids(parentrel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

	Assert(RelationIsAoRows(parentrel));

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Clear seg file info: segno %d table '%s'",
		   segno,
		   RelationGetRelationName(parentrel));

	/*
	 * Open the aoseg relation and scan for tuple.
	 */
	pg_aoseg_rel = table_open(segrelid, RowExclusiveLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = table_beginscan_catalog(pg_aoseg_rel, 0, NULL);
	while (segno != tuple_segno && (tuple = heap_getnext(aoscan, ForwardScanDirection)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(tuple, Anum_pg_aoseg_segno, pg_aoseg_dsc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&tuple->t_self))));
	}

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("append-only table \"%s\" file segment \"%d\" entry "
						"does not exist", RelationGetRelationName(parentrel),
						segno)));

	new_record = palloc0(sizeof(Datum) * pg_aoseg_dsc->natts);
	new_record_nulls = palloc0(sizeof(bool) * pg_aoseg_dsc->natts);
	new_record_repl = palloc0(sizeof(bool) * pg_aoseg_dsc->natts);
	new_record[Anum_pg_aoseg_eof - 1] = Int64GetDatum(0);
	new_record_repl[Anum_pg_aoseg_eof - 1] = true;
	new_record[Anum_pg_aoseg_tupcount - 1] = Int64GetDatum(0);
	new_record_repl[Anum_pg_aoseg_tupcount - 1] = true;
	new_record[Anum_pg_aoseg_varblockcount - 1] = Int64GetDatum(0);
	new_record_repl[Anum_pg_aoseg_varblockcount - 1] = true;
	new_record[Anum_pg_aoseg_eofuncompressed - 1] = Int64GetDatum(0);
	new_record_repl[Anum_pg_aoseg_eofuncompressed - 1] = true;

	/* When the segment is later recreated, it will be in new format */
	new_record[Anum_pg_aoseg_formatversion - 1] = Int16GetDatum(AORelationVersion_GetLatest());
	new_record_repl[Anum_pg_aoseg_formatversion - 1] = true;

	/* We do not reset the modcount here */

	new_record[Anum_pg_aoseg_state - 1] = Int16GetDatum(AOSEG_STATE_DEFAULT);
	new_record_repl[Anum_pg_aoseg_state - 1] = true;

	new_tuple = heap_modify_tuple(tuple, pg_aoseg_dsc, new_record,
								  new_record_nulls, new_record_repl);

	simple_heap_update(pg_aoseg_rel, &tuple->t_self, new_tuple);
	heap_freetuple(new_tuple);

	heap_endscan(aoscan);
	table_close(pg_aoseg_rel, RowExclusiveLock);

	pfree(new_record);
	pfree(new_record_nulls);
	pfree(new_record_repl);
}

/*
 * Update the eof and filetupcount of an append only table.
 *
 * The parameters eof and eof_uncompressed should not be negative.
 * tuples_added and varblocks_added can be negative.
 */
void
UpdateFileSegInfo(Relation parentrel,
				  int segno,
				  int64 eof,
				  int64 eof_uncompressed,
				  int64 tuples_added,
				  int64 varblocks_added,
				  int64 modcount_added,
				  FileSegInfoState newState)
{
	Assert(eof >= 0);
	Assert(eof_uncompressed >= 0);

	elog(DEBUG3, "UpdateFileSegInfo called. segno = %d", segno);

	UpdateFileSegInfo_internal(parentrel,
							   segno,
							   eof,
							   eof_uncompressed,
							   tuples_added,
							   varblocks_added,
							   modcount_added,
							   newState);
}

/*
 * Update the eof and filetupcount of an append only table.
 *
 * The parameters eof and eof_uncompressed can be negative.
 * In this case, the columns are not updated.
 *
 * The parameters tuples_added and varblocks_added can be negative, e.g. after
 * a AO compaction.
 */
static void
UpdateFileSegInfo_internal(Relation parentrel,
						   int segno,
						   int64 eof,
						   int64 eof_uncompressed,
						   int64 tuples_added,
						   int64 varblocks_added,
						   int64 modcount_added,
						   FileSegInfoState newState)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	TableScanDesc aoscan;
	HeapTuple	tuple = NULL;
	HeapTuple	new_tuple;
	int			tuple_segno = InvalidFileSegNumber;
	int64		filetupcount;
	int64		filevarblockcount;
	int64		new_tuple_count;
	int64		new_varblock_count;
	int64		new_modcount;
	int64		old_eof;
	int64		old_eof_uncompressed;
	int64		old_modcount;
	Datum	   *new_record;
	bool	   *new_record_nulls;
	bool	   *new_record_repl;
	bool		isNull;
	Oid segrelid;

	GetAppendOnlyEntryAuxOids(parentrel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

	Assert(RelationIsAoRows(parentrel));
	Assert(newState >= AOSEG_STATE_USECURRENT && newState <= AOSEG_STATE_AWAITING_DROP);

	/*
	 * Open the aoseg relation and scan for tuple.
	 */
	pg_aoseg_rel = table_open(segrelid, RowExclusiveLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = table_beginscan_catalog(pg_aoseg_rel, 0, NULL);
	while (segno != tuple_segno && (tuple = heap_getnext(aoscan, ForwardScanDirection)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(tuple, Anum_pg_aoseg_segno, pg_aoseg_dsc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&tuple->t_self))));
	}

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("append-only table \"%s\" file segment \"%d\" entry does not exist",
						RelationGetRelationName(parentrel), segno)));

	/*
	 * Verify that the caller locked the segment earlier. In principle, if
	 * the caller is holding an AccessExclusiveLock on the table, locking
	 * individual tuples would not be necessary, but all current callers
	 * diligently lock the tuples anyway, so we can perform this sanity check
	 * here.
	 */
	if (!pg_aoseg_tuple_is_locked_by_me(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("cannot update pg_aoseg entry for segno %d for relation %s, it is not locked for us",
						segno, RelationGetRelationName(parentrel))));

	new_record = palloc0(sizeof(Datum) * pg_aoseg_dsc->natts);
	new_record_nulls = palloc0(sizeof(bool) * pg_aoseg_dsc->natts);
	new_record_repl = palloc0(sizeof(bool) * pg_aoseg_dsc->natts);

	old_eof = DatumGetInt64(fastgetattr(tuple,
										Anum_pg_aoseg_eof,
										pg_aoseg_dsc,
										&isNull));
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid pg_aoseg eof value: NULL")));

	/*
	 * For AO by design due to append-only nature, new end-of-file (EOF) to be
	 * recorded in aoseg table has to be greater than currently stored EOF
	 * value, as new writes must move it forward only. If new end-of-file
	 * value is less than currently stored end-of-file something is incorrect
	 * and updating the same will yield incorrect result during reads. Hence
	 * abort the write transaction trying to update the incorrect EOF value.
	 */
	if (eof < 0)
	{
		eof = old_eof;
	}
	else if (eof < old_eof)
	{
		elog(ERROR, "Unexpected compressed EOF for relation %s, relfilenode %u, segment file %d. "
			 "EOF " INT64_FORMAT " to be updated cannot be smaller than current EOF " INT64_FORMAT " in pg_aoseg",
			 RelationGetRelationName(parentrel), parentrel->rd_node.relNode,
			 segno, eof, old_eof);
	}

	old_eof_uncompressed = DatumGetInt64(fastgetattr(tuple,
													 Anum_pg_aoseg_eofuncompressed,
													 pg_aoseg_dsc,
													 &isNull));
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid pg_aoseg eofuncompressed value: NULL")));

	if (eof_uncompressed < 0)
	{
		eof_uncompressed = old_eof_uncompressed;
	}
	else if (eof_uncompressed < old_eof_uncompressed)
	{
		elog(ERROR, "Unexpected EOF for relation %s, relfilenode %u, segment file %d."
			 "EOF " INT64_FORMAT " to be updated cannot be smaller than current EOF " INT64_FORMAT " in pg_aoseg",
			 RelationGetRelationName(parentrel), parentrel->rd_node.relNode,
			 segno, eof_uncompressed, old_eof_uncompressed);
	}

	/* get the current tuple count so we can add to it */
	filetupcount = DatumGetInt64(fastgetattr(tuple,
											 Anum_pg_aoseg_tupcount,
											 pg_aoseg_dsc,
											 &isNull));
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid pg_aoseg filetupcount value: NULL")));

	/* calculate the new tuple count */
	new_tuple_count = filetupcount + tuples_added;
	Assert(new_tuple_count >= 0);

	/* get the current varblock count so we can add to it */
	filevarblockcount = DatumGetInt64(fastgetattr(tuple,
												  Anum_pg_aoseg_varblockcount,
												  pg_aoseg_dsc,
												  &isNull));
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid pg_aoseg varblockcount value: NULL")));

	/* calculate the new tuple count */
	new_varblock_count = filevarblockcount + varblocks_added;
	Assert(new_varblock_count >= 0);

	/* get the current modcount so we can add to it */
	old_modcount = DatumGetInt64(fastgetattr(tuple,
											 Anum_pg_aoseg_modcount,
											 pg_aoseg_dsc,
											 &isNull));
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid pg_aoseg modcount value: NULL")));

	/* calculate the new mod count */
	new_modcount = old_modcount + modcount_added;
	Assert(new_modcount >= 0);

	/*
	 * Build a tuple to update
	 */
	new_record[Anum_pg_aoseg_eof - 1] = Int64GetDatum(eof);
	new_record_repl[Anum_pg_aoseg_eof - 1] = true;

	new_record[Anum_pg_aoseg_tupcount - 1] = Int64GetDatum(new_tuple_count);
	new_record_repl[Anum_pg_aoseg_tupcount - 1] = true;

	new_record[Anum_pg_aoseg_varblockcount - 1] = Int64GetDatum(new_varblock_count);
	new_record_repl[Anum_pg_aoseg_varblockcount - 1] = true;

	new_record[Anum_pg_aoseg_modcount - 1] = Int64GetDatum(new_modcount);
	new_record_repl[Anum_pg_aoseg_modcount - 1] = true;

	new_record[Anum_pg_aoseg_eofuncompressed - 1] = Int64GetDatum(eof_uncompressed);
	new_record_repl[Anum_pg_aoseg_eofuncompressed - 1] = true;

	if (newState > 0)
	{
		new_record[Anum_pg_aoseg_state - 1] = Int16GetDatum(newState);
		new_record_repl[Anum_pg_aoseg_state - 1] = true;
	}

	/*
	 * update the tuple in the pg_aoseg table
	 */
	new_tuple = heap_modify_tuple(tuple, pg_aoseg_dsc, new_record,
								  new_record_nulls, new_record_repl);

	simple_heap_update(pg_aoseg_rel, &tuple->t_self, new_tuple);

	heap_freetuple(new_tuple);

	/* Finish up scan */
	heap_endscan(aoscan);
	table_close(pg_aoseg_rel, RowExclusiveLock);

	pfree(new_record);
	pfree(new_record_nulls);
	pfree(new_record_repl);
}

/*
 * GetSegFilesTotals
 *
 * Get the total bytes, tuples, and varblocks for a specific AO table
 * from the pg_aoseg table on this local segdb.
 */
FileSegTotals *
GetSegFilesTotals(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot)
{

	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	HeapTuple	tuple;
	SysScanDesc aoscan;
	FileSegTotals *result;
	Datum		eof,
				eof_uncompressed,
				tupcount,
				varblockcount,
				state;
	bool		isNull;
	Oid			segrelid;

	Assert(RelationIsAoRows(parentrel));

	result = (FileSegTotals *) palloc0(sizeof(FileSegTotals));

	GetAppendOnlyEntryAuxOids(parentrel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

	if (segrelid == InvalidOid)
		elog(ERROR, "could not find pg_aoseg aux table for AO table \"%s\"",
			 RelationGetRelationName(parentrel));

	pg_aoseg_rel = table_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false,
								appendOnlyMetaDataSnapshot, 0, NULL);

	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		eof = fastgetattr(tuple, Anum_pg_aoseg_eof, pg_aoseg_dsc, &isNull);
		tupcount = fastgetattr(tuple, Anum_pg_aoseg_tupcount, pg_aoseg_dsc, &isNull);
		varblockcount = fastgetattr(tuple, Anum_pg_aoseg_varblockcount, pg_aoseg_dsc, &isNull);
		eof_uncompressed = fastgetattr(tuple, Anum_pg_aoseg_eofuncompressed, pg_aoseg_dsc, &isNull);
		state = fastgetattr(tuple, Anum_pg_aoseg_state, pg_aoseg_dsc, &isNull);

		if (isNull)
			result->totalbytesuncompressed = InvalidUncompressedEof;
		else
			result->totalbytesuncompressed += DatumGetInt64(eof_uncompressed);

		result->totalbytes += DatumGetInt64(eof);

		if (DatumGetInt16(state) != AOSEG_STATE_AWAITING_DROP)
		{
			result->totaltuples += DatumGetInt64(tupcount);
		}
		result->totalvarblocks += DatumGetInt64(varblockcount);
		result->totalfilesegs++;

		CHECK_FOR_INTERRUPTS();
	}

	systable_endscan(aoscan);
	table_close(pg_aoseg_rel, AccessShareLock);

	return result;
}

PG_FUNCTION_INFO_V1(gp_aoseg_history);

extern Datum gp_aoseg_history(PG_FUNCTION_ARGS);

Datum
gp_aoseg_history(PG_FUNCTION_ARGS)
{
	int			aoRelOid = PG_GETARG_OID(0);

	typedef struct Context
	{
		Oid			aoRelOid;

		FileSegInfo **aoSegfileArray;

		int			totalAoSegFiles;

		int			segfileArrayIndex;
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		Relation	aocsRel;
		Relation	pg_aoseg_rel;
		Oid segrelid;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(19);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "gp_tid",
						   TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "gp_xmin",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "gp_xmin_status",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "gp_xmin_commit_distrib_id",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "gp_xmax",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "gp_xmax_status",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "gp_xmax_commit_distrib_id",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "gp_command_id",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "gp_infomask",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 11, "gp_update_tid",
						   TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 12, "gp_visibility",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 13, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 14, "tupcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 15, "eof",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 16, "eof_uncompressed",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 17, "modcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 18, "formatversion",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 19, "state",
						   INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->aoRelOid = aoRelOid;

		aocsRel = table_open(aoRelOid, AccessShareLock);
		if (!RelationIsAoRows(aocsRel))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("'%s' is not an append-only row relation",
							RelationGetRelationName(aocsRel))));

		GetAppendOnlyEntryAuxOids(aocsRel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

		pg_aoseg_rel = table_open(segrelid, AccessShareLock);

		context->aoSegfileArray =
			GetAllFileSegInfo_pg_aoseg_rel(
										   RelationGetRelationName(aocsRel),
										   pg_aoseg_rel,
										   SnapshotAny, //Get ALL tuples from pg_aoseg_ % including aborted and in - progress ones.
										   & context->totalAoSegFiles);

		table_close(pg_aoseg_rel, AccessShareLock);
		table_close(aocsRel, AccessShareLock);

		/* Iteration position. */
		context->segfileArrayIndex = 0;

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	/*
	 * Process each column for each segment file.
	 */
	while (true)
	{
		Datum		values[19];
		bool		nulls[19];
		HeapTuple	tuple;
		Datum		result;

		struct FileSegInfo *aoSegfile;

		if (context->segfileArrayIndex >= context->totalAoSegFiles)
		{
			break;
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		aoSegfile = context->aoSegfileArray[context->segfileArrayIndex];

		values[0] = Int32GetDatum(GpIdentity.segindex);
		GetTupleVisibilitySummaryDatums(&values[1],
										&nulls[1],
										&aoSegfile->tupleVisibilitySummary);

		values[12] = Int32GetDatum(aoSegfile->segno);
		values[13] = Int64GetDatum(aoSegfile->total_tupcount);
		values[14] = Int64GetDatum(aoSegfile->eof);
		values[15] = Int64GetDatum(aoSegfile->eof_uncompressed);
		values[16] = Int64GetDatum(aoSegfile->modcount);
		values[17] = Int16GetDatum(aoSegfile->formatversion);
		values[18] = Int16GetDatum(aoSegfile->state);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		/* Indicate we emitted one segment file. */
		context->segfileArrayIndex++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

PG_FUNCTION_INFO_V1(gp_aoseg);

extern Datum gp_aoseg(PG_FUNCTION_ARGS);

/*
 * UDF to get aoseg information by relation OID/name
 */
Datum
gp_aoseg(PG_FUNCTION_ARGS)
{
	Oid			aoRelOid = PG_GETARG_OID(0);

	typedef struct Context
	{
		Oid			aoRelOid;

		FileSegInfo **aoSegfileArray;

		int			totalAoSegFiles;

		int			segfileArrayIndex;
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		Relation	aocsRel;
		Relation	pg_aoseg_rel;
		Oid			segrelid;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(9);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "eof",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "tupcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "varblockcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "eofuncompressed",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "modcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "formatversion",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "state",
						   INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->aoRelOid = aoRelOid;

		aocsRel = table_open(aoRelOid, AccessShareLock);
		if (!RelationIsAoRows(aocsRel))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("'%s' is not an append-only row relation",
							RelationGetRelationName(aocsRel))));

		GetAppendOnlyEntryAuxOids(aocsRel->rd_id, NULL, &segrelid, NULL, NULL, NULL, NULL);

		pg_aoseg_rel = table_open(segrelid, AccessShareLock);

		Snapshot	snapshot;
		snapshot = RegisterSnapshot(GetLatestSnapshot());
		context->aoSegfileArray =
			GetAllFileSegInfo_pg_aoseg_rel(RelationGetRelationName(aocsRel),
										   pg_aoseg_rel,
										   snapshot,
										   &context->totalAoSegFiles);
		UnregisterSnapshot(snapshot);

		table_close(pg_aoseg_rel, AccessShareLock);
		table_close(aocsRel, AccessShareLock);

		/* Iteration position. */
		context->segfileArrayIndex = 0;

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	/*
	 * Process each column for each segment file.
	 */
	while (true)
	{
		Datum		values[9];
		bool		nulls[9];
		HeapTuple	tuple;
		Datum		result;

		struct FileSegInfo *aoSegfile;

		if (context->segfileArrayIndex >= context->totalAoSegFiles)
		{
			break;
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		aoSegfile = context->aoSegfileArray[context->segfileArrayIndex];

		values[0] = Int32GetDatum(GpIdentity.segindex);
		values[1] = Int32GetDatum(aoSegfile->segno);
		values[2] = Int64GetDatum(aoSegfile->eof);
		values[3] = Int64GetDatum(aoSegfile->total_tupcount);
		values[4] = Int64GetDatum(aoSegfile->varblockcount);
		values[5] = Int64GetDatum(aoSegfile->eof_uncompressed);
		values[6] = Int64GetDatum(aoSegfile->modcount);
		values[7] = Int64GetDatum(aoSegfile->formatversion);
		values[8] = Int16GetDatum(aoSegfile->state);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		/* Indicate we emitted one segment file. */
		context->segfileArrayIndex++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	if (context->aoSegfileArray)
	{
		FreeAllSegFileInfo(context->aoSegfileArray, context->totalAoSegFiles);
		pfree(context->aoSegfileArray);
		context->aoSegfileArray = NULL;
		context->totalAoSegFiles = 0;
	}
	pfree(context);
	funcctx->user_fctx = NULL;

	SRF_RETURN_DONE(funcctx);
}

typedef struct
{
	uint64 index;
	/* there is a chance the count will return more than 2^32 rows
	 * plus SPI_processed is 64 bit anyway */
	uint64 rows;
} QueryInfo;


/**************************************************************
 * get_ao_distribution
 *
 * given an AO table name or oid, show the total distribution
 * of rows across all segment databases in the system.
 **************************************************************/
Datum
get_ao_distribution(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	FuncCallContext *funcctx;
	MemoryContext oldcontext;
	AclResult	aclresult;
	QueryInfo  *query_block = NULL;
	char	   *sqlstmt;
	Relation	parentrel;
	Relation	aosegrel;
	int			ret;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	/*
	 * stuff done only on the first call of the function. In here we execute
	 * the query, gather the result rows and keep them in our context so that
	 * we could return them in the next calls to this func.
	 */
	if (SRF_IS_FIRSTCALL())
	{
		bool		connected = false;
		Oid			segrelid;

		funcctx = SRF_FIRSTCALL_INIT();

		/* open the parent (main) relation */
		parentrel = table_open(relid, AccessShareLock);

		/*
		 * check permission to SELECT from this table (this function is
		 * effectively a form of COUNT(*) FROM TABLE
		 */
		aclresult = pg_class_aclcheck(parentrel->rd_id, GetUserId(),
									  ACL_SELECT);

		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult,
						   OBJECT_TABLE,
						   RelationGetRelationName(parentrel));

		/*
		 * verify this is an AO relation
		 */
		if (!RelationIsAppendOptimized(parentrel))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("'%s' is not an append-only relation",
							RelationGetRelationName(parentrel))));

		GetAppendOnlyEntryAuxOids(RelationGetRelid(parentrel), NULL,
								  &segrelid, NULL, NULL, NULL, NULL);
		Assert(OidIsValid(segrelid));

		/*
		 * assemble our query string
		 */
		aosegrel = table_open(segrelid, AccessShareLock);

		/*
		 * NOTE: we don't need quoting here. The aux AO segment heap table's name
		 * should follow the pattern "pg_asoeg.pg_aoseg_<oid>".
		 */
		sqlstmt = psprintf("select gp_segment_id,sum(tupcount)::bigint "
						   "from gp_dist_random('%s.%s') "
						   "group by (gp_segment_id)",
						   get_namespace_name(RelationGetNamespace(aosegrel)),
						   RelationGetRelationName(aosegrel));

		table_close(aosegrel, AccessShareLock);

		PG_TRY();
		{

			if (SPI_OK_CONNECT != SPI_connect())
			{
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("unable to obtain AO relation information from segment databases"),
						 errdetail("SPI_connect failed in get_ao_distribution")));
			}
			connected = true;

			/* Do the query. */
			ret = SPI_execute(sqlstmt, false, 0);

			if (ret > 0 && SPI_tuptable != NULL)
			{
				QueryInfo  *query_block_state = NULL;

				/*
				 * switch to memory context appropriate for multiple function
				 * calls
				 */
				oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

				funcctx->tuple_desc = BlessTupleDesc(SPI_tuptable->tupdesc);

				/*
				 * Allocate cross-call state, so that we can keep track of
				 * where we're at in the processing.
				 */
				query_block_state = (QueryInfo *) palloc0(sizeof(QueryInfo));
				funcctx->user_fctx = (int *) query_block_state;

				query_block_state->index = 0;
				query_block_state->rows = SPI_processed;
				MemoryContextSwitchTo(oldcontext);
			}
		}

		/* Clean up in case of error. */
		PG_CATCH();
		{
			if (connected)
				SPI_finish();

			/* Carry on with error handling. */
			PG_RE_THROW();
		}
		PG_END_TRY();

		pfree(sqlstmt);
		table_close(parentrel, AccessShareLock);
	}

	/*
	 * Per-call operations
	 */

	funcctx = SRF_PERCALL_SETUP();

	query_block = (QueryInfo *) funcctx->user_fctx;
	if (query_block->index < query_block->rows)
	{
		/*
		 * Get heaptuple from SPI, then deform it, and reform it using our
		 * tuple desc. If we don't do this, but rather try to pass the tuple
		 * from SPI directly back, we get an error because the tuple desc that
		 * is associated with the SPI call has not been blessed.
		 */
		HeapTuple	tuple = SPI_tuptable->vals[query_block->index++];
		TupleDesc	tupleDesc = funcctx->tuple_desc;

		Datum	   *values = (Datum *) palloc(tupleDesc->natts * sizeof(Datum));
		bool	   *nulls = (bool *) palloc(tupleDesc->natts * sizeof(bool));

		HeapTuple	res = NULL;
		Datum		result;

		heap_deform_tuple(tuple, tupleDesc, values, nulls);

		res = heap_form_tuple(tupleDesc, values, nulls);

		pfree(values);
		pfree(nulls);

		/* make the tuple into a datum */
		result = HeapTupleGetDatum(res);

		SRF_RETURN_NEXT(funcctx, result);
	}

	/*
	 * do when there is no more left
	 */
	pfree(query_block);

	SPI_finish();

	funcctx->user_fctx = NULL;

	SRF_RETURN_DONE(funcctx);
}

/**************************************************************
 * get_ao_compression_ratio
 *
 * Given an append-only table oid or name calculate the effective
 * compression ratio for this append only table stored data.
 * If this info is not available (pre 3.3 created tables) then
 * return -1.
 **************************************************************/
Datum
get_ao_compression_ratio(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	Relation	parentrel;
	float8		result;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	/* open the parent (main) relation */
	parentrel = table_open(relid, AccessShareLock);

	if (!RelationIsAppendOptimized(parentrel))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("'%s' is not an append-only relation",
						RelationGetRelationName(parentrel))));

	if (RelationIsAoRows(parentrel))
		result = aorow_compression_ratio_internal(parentrel);
	else
		result = aocol_compression_ratio_internal(parentrel);

	table_close(parentrel, AccessShareLock);

	PG_RETURN_FLOAT8(result);
}

static float8
aorow_compression_ratio_internal(Relation parentrel)
{
	StringInfoData sqlstmt;
	Relation	aosegrel;
	bool		connected = false;
	int			proc;	/* 32 bit, only holds number of segments */
	int			ret;
	float8		compress_ratio = -1;	/* the default, meaning "not
										 * available" */
	Oid			segrelid = InvalidOid;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	GetAppendOnlyEntryAuxOids(RelationGetRelid(parentrel), NULL,
							  &segrelid,
							  NULL, NULL, NULL, NULL);
	Assert(OidIsValid(segrelid));

	/*
	 * assemble our query string
	 */
	aosegrel = table_open(segrelid, AccessShareLock);
	initStringInfo(&sqlstmt);
	appendStringInfo(&sqlstmt, "select sum(eof), sum(eofuncompressed) "
					 "from gp_dist_random('%s.%s')",
					 get_namespace_name(RelationGetNamespace(aosegrel)),
					 RelationGetRelationName(aosegrel));

	table_close(aosegrel, AccessShareLock);

	PG_TRY();
	{

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("unable to obtain AO relation information from segment databases"),
					 errdetail("SPI_connect failed in get_ao_compression_ratio.")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, false, 0);
		proc = (int) SPI_processed;

		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc	tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			HeapTuple	tuple = tuptable->vals[0];
			int64		eof;
			int64		eof_uncomp;

			/* we expect only 1 tuple */
			Assert(proc == 1);

			/*
			 * Get totals from QE's and calculate the compression ratio. In
			 * tables upgraded from GPDB 3.3 the eofuncompressed column could
			 * contain NULL, this is fixed in more recent upgrades.
			 */
			char	   *attr1 = SPI_getvalue(tuple, tupdesc, 1);
			char	   *attr2 = SPI_getvalue(tuple, tupdesc, 2);

			if (NULL == attr1 || NULL == attr2)
			{
				SPI_finish();
				return 1;
			}

			if (scanint8(attr1, true, &eof) &&
				scanint8(attr2, true, &eof_uncomp))
			{
				/* guard against division by zero */
				if (eof > 0)
				{
					/* calculate the compression ratio */
					compress_ratio = (float8) eof_uncomp / (float8) eof;

					/* format to 2 digit decimal precision */
					compress_ratio = round(compress_ratio * 100.0) / 100.0;
				}
			}
		}

		connected = false;
		SPI_finish();
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();


	pfree(sqlstmt.data);

	return compress_ratio;
}

void
FreeAllSegFileInfo(FileSegInfo **allSegInfo, int totalSegFiles)
{
	Assert(allSegInfo);

	for (int file_no = 0; file_no < totalSegFiles; file_no++)
	{
		Assert(allSegInfo[file_no] != NULL);

		pfree(allSegInfo[file_no]);
	}
}

bool
pg_aoseg_tuple_could_be_updated(Relation relation, HeapTuple tuple)
{
	CommandId	cid = GetCurrentCommandId(false);
	Buffer		buffer;
	TM_Result	result;

	buffer = ReadBuffer(relation, ItemPointerGetBlockNumber(&tuple->t_self));
	LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

	result = HeapTupleSatisfiesUpdate(relation, tuple, cid, buffer);

	UnlockReleaseBuffer(buffer);

	return (result == TM_Ok);
}

bool
pg_aoseg_tuple_is_locked_by_me(HeapTuple tuple)
{
	TransactionId rawxmax;

	/*
	 * If we had updated this tuple earlier in this transaction, it is
	 * implicitly locked for us.
	 */
	if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple->t_data)))
		return true;

	/*
	 * Have we locked the tuple?
	 *
	 * This roughly corresponds to the checks in heap_lock_tuple().
	 * Unfortunately the full logic is a bit complicated. This would fail if
	 * another transaction has key-share locked the tuple, but we don't expect
	 * anyone to do that on the pg_aoseg tables.
	 *
	 * Note that we consider a tuple that we have updated, rather than just locked,
	 * as 'false' here. That would imply that we tried to update an outdated version
	 * of the tuple. That would fail in heap_update() anyway.
	 */
	rawxmax = HeapTupleHeaderGetRawXmax(tuple->t_data);
	if (!(tuple->t_data->t_infomask & HEAP_XMAX_INVALID) &&
		!(tuple->t_data->t_infomask & HEAP_XMAX_IS_MULTI) &&
		(tuple->t_data->t_infomask & HEAP_XMAX_LOCK_ONLY) &&
		TransactionIdIsCurrentTransactionId(rawxmax))
	{
		return true;
	}

	return false;
}
