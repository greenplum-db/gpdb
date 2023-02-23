/*--------------------------------------------------------------------------
 *
 * aocsam.c
 *	  Append only columnar access methods
 *
 * Portions Copyright (c) 2009-2010, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/aocs/aocsam.c
 *
 *--------------------------------------------------------------------------
 */

#include "postgres.h"

#include "common/relpath.h"
#include "access/aocssegfiles.h"
#include "access/aomd.h"
#include "access/appendonlytid.h"
#include "access/appendonlywriter.h"
#include "access/heapam.h"
#include "access/hio.h"
#include "catalog/catalog.h"
#include "catalog/gp_fastsequence.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly.h"
#include "catalog/pg_attribute_encoding.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "cdb/cdbvars.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/datumstream.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"


static AOCSScanDesc aocs_beginscan_internal(Relation relation,
						AOCSFileSegInfo **seginfo,
						int total_seg,
						Snapshot snapshot,
						Snapshot appendOnlyMetaDataSnapshot,
						bool *proj,
						uint32 flags);

/*
 * Open the segment file for a specified column associated with the datum
 * stream.
 */
static void
open_datumstreamread_segfile(
							 char *basepath, Relation rel,
							 AOCSFileSegInfo *segInfo,
							 DatumStreamRead *ds,
							 int colNo)
{
	int			segNo = segInfo->segno;
	char		fn[MAXPGPATH];
	int32		fileSegNo;
	RelFileNode node = rel->rd_node;
	Oid         relid = RelationGetRelid(rel);

	/* Filenum for the column */
	FileNumber	filenum = GetFilenumForAttribute(relid, colNo + 1);

	AOCSVPInfoEntry *e = getAOCSVPEntry(segInfo, colNo);

	FormatAOSegmentFileName(basepath, segNo, filenum, &fileSegNo, fn);
	Assert(strlen(fn) + 1 <= MAXPGPATH);

	Assert(ds);
	datumstreamread_open_file(ds, fn, e->eof, e->eof_uncompressed, node,
							  fileSegNo, segInfo->formatversion);
}

/*
 * Open all segment files associted with the datum stream.
 *
 * Currently, there is one segment file for each column. This function
 * only opens files for those columns which are in the projection.
 *
 * If blockDirectory is not NULL, the first block info is written to
 * the block directory.
 */
static void
open_all_datumstreamread_segfiles(AOCSScanDesc scan, AOCSFileSegInfo *segInfo)
{
	Relation 		rel = scan->rs_base.rs_rd;
	DatumStreamRead **ds = scan->columnScanInfo.ds;
	AttrNumber 		*proj_atts = scan->columnScanInfo.proj_atts;
	AttrNumber 		num_proj_atts = scan->columnScanInfo.num_proj_atts;
	AppendOnlyBlockDirectory *blockDirectory = scan->blockDirectory;
	char *basepath = relpathbackend(rel->rd_node, rel->rd_backend, MAIN_FORKNUM);

	Assert(proj_atts);
	for (AttrNumber i = 0; i < num_proj_atts; i++)
	{
		AttrNumber	attno = proj_atts[i];

		open_datumstreamread_segfile(basepath, rel, segInfo, ds[attno], attno);
		datumstreamread_block(ds[attno], blockDirectory, attno);

		AOCSScanDesc_UpdateTotalBytesRead(scan, attno);
	}

	pfree(basepath);
}

/*
 * Initialise data streams for every column used in this query. For writes, this
 * means all columns.
 */
static void
open_ds_write(Relation rel, DatumStreamWrite **ds, TupleDesc relationTupleDesc, bool checksum)
{
	int			nvp = relationTupleDesc->natts;
	StdRdOptions **opts = RelationGetAttributeOptions(rel);

	/* open datum streams.  It will open segment file underneath */
	for (int i = 0; i < nvp; ++i)
	{
		Form_pg_attribute attr = TupleDescAttr(relationTupleDesc, i);
		char	   *ct;
		int32		clvl;
		int32		blksz;

		StringInfoData titleBuf;

		/* UNDONE: Need to track and dispose of this storage... */
		initStringInfo(&titleBuf);
		appendStringInfo(&titleBuf,
						 "Write of Append-Only Column-Oriented relation '%s', column #%d '%s'",
						 RelationGetRelationName(rel),
						 i + 1,
						 NameStr(attr->attname));

		/*
		 * We always record all the three column specific attributes for each
		 * column of a column oriented table.  Note: checksum is a table level
		 * attribute.
		 */
		if (opts[i] == NULL || opts[i]->blocksize == 0)
			elog(ERROR, "could not find blocksize option for AOCO column in pg_attribute_encoding");
		ct = opts[i]->compresstype;
		clvl = opts[i]->compresslevel;
		blksz = opts[i]->blocksize;

		ds[i] = create_datumstreamwrite(ct,
										clvl,
										checksum,
										blksz,
										attr,
										RelationGetRelationName(rel),
										/* title */ titleBuf.data,
										XLogIsNeeded() && RelationNeedsWAL(rel));

	}

	for (int i = 0; i < RelationGetNumberOfAttributes(rel); i++)
		pfree(opts[i]);
	pfree(opts);
}

/*
 * Initialise data streams for every column used in this query. For writes, this
 * means all columns.
 */
static void
open_ds_read(Relation rel, DatumStreamRead **ds, TupleDesc relationTupleDesc,
			 AttrNumber *proj_atts, AttrNumber num_proj_atts, bool checksum)
{
	/*
	 *  RelationGetAttributeOptions does not always success return opts. e.g.
	 *  `ALTER TABLE ADD COLUMN` with an illegal option.
	 *
	 *  In this situation, the transaction will abort, and the Relation will be
	 *  free. Upstream have sanity check to promise we must have a worked TupleDesc
	 *  attached the Relation during memory recycle. Otherwise, the query will crash.
	 *
	 *  For some reason, we can not put the option validation check into "perp"
	 *  phase for AOCO table ALTER command.
	 *  (commit: e707c19c885fadffe50095cc699e52af1ee64f4b)
	 *
	 *  So, a fake TupleDesc temporary replace into Relation.
	 */

	TupleDesc orig_att = rel->rd_att;
	if (orig_att->tdrefcount == -1)
	{
		rel->rd_att = CreateTemplateTupleDesc(relationTupleDesc->natts);
		rel->rd_att->tdrefcount = 1;
	}

	StdRdOptions **opts = RelationGetAttributeOptions(rel);

	if (orig_att->tdrefcount == -1)
	{
		pfree(rel->rd_att);
		rel->rd_att = orig_att;
	}

	/*
	 * Clear all the entries to NULL first, as the NULL value is used during
	 * closing
	 */
	for (AttrNumber attno = 0; attno < relationTupleDesc->natts; attno++)
		ds[attno] = NULL;

	/* And then initialize the data streams for those columns we need */
	for (AttrNumber i = 0; i < num_proj_atts; i++)
	{
		AttrNumber			attno = proj_atts[i];
		Form_pg_attribute	attr;
		char			   *ct;
		int32				clvl;
		int32				blksz;
		StringInfoData titleBuf;

		Assert(attno <= relationTupleDesc->natts);
		attr = TupleDescAttr(relationTupleDesc, attno);

		/*
		 * We always record all the three column specific attributes for each
		 * column of a column oriented table.  Note: checksum is a table level
		 * attribute.
		 */
		if (opts[attno] == NULL || opts[attno]->blocksize == 0)
			elog(ERROR, "could not find blocksize option for AOCO column in pg_attribute_encoding");
		ct = opts[attno]->compresstype;
		clvl = opts[attno]->compresslevel;
		blksz = opts[attno]->blocksize;

		/* UNDONE: Need to track and dispose of this storage... */
		initStringInfo(&titleBuf);
		appendStringInfo(&titleBuf, "Scan of Append-Only Column-Oriented relation '%s', column #%d '%s'",
						 RelationGetRelationName(rel),
						 attno + 1,
						 NameStr(attr->attname));

		ds[attno] = create_datumstreamread(ct,
										   clvl,
										   checksum,
										   blksz,
										   attr,
										   RelationGetRelationName(rel),
										    /* title */ titleBuf.data);
	}

	for (int i = 0; i < RelationGetNumberOfAttributes(rel); i++)
		pfree(opts[i]);
	pfree(opts);
}

static void
close_ds_read(DatumStreamRead **ds, AttrNumber natts)
{
	for (AttrNumber attno = 0; attno < natts; attno++)
	{
		if (ds[attno])
		{
			destroy_datumstreamread(ds[attno]);
			ds[attno] = NULL;
		}
	}
}

static void
close_ds_write(DatumStreamWrite **ds, int nvp)
{
	int			i;

	for (i = 0; i < nvp; ++i)
	{
		if (ds[i])
		{
			destroy_datumstreamwrite(ds[i]);
			ds[i] = NULL;
		}
	}
}

static void
initscan_with_colinfo(AOCSScanDesc scan)
{
	MemoryContext	oldCtx;
	AttrNumber		natts;

	Assert(scan->columnScanInfo.relationTupleDesc);
	natts = scan->columnScanInfo.relationTupleDesc->natts;

	oldCtx = MemoryContextSwitchTo(scan->columnScanInfo.scanCtx);

	if (scan->columnScanInfo.ds == NULL)
		scan->columnScanInfo.ds = (DatumStreamRead **)
								  palloc0(natts * sizeof(DatumStreamRead *));

	if (scan->columnScanInfo.proj_atts == NULL)
	{
		scan->columnScanInfo.num_proj_atts = natts;
		scan->columnScanInfo.proj_atts = (AttrNumber *)
										 palloc0(natts * sizeof(AttrNumber));

		for (AttrNumber attno = 0; attno < natts; attno++)
			scan->columnScanInfo.proj_atts[attno] = attno;
	}

	open_ds_read(scan->rs_base.rs_rd, scan->columnScanInfo.ds,
				 scan->columnScanInfo.relationTupleDesc,
				 scan->columnScanInfo.proj_atts, scan->columnScanInfo.num_proj_atts,
				 scan->checksum);

	MemoryContextSwitchTo(oldCtx);

	scan->cur_seg = -1;
	scan->cur_seg_row = 0;

	ItemPointerSet(&scan->cdb_fake_ctid, 0, 0);

	scan->totalBytesRead = 0;

	pgstat_count_heap_scan(scan->rs_base.rs_rd);
}

static int
open_next_scan_seg(AOCSScanDesc scan)
{
	while (++scan->cur_seg < scan->total_seg)
	{
		AOCSFileSegInfo *curSegInfo = scan->seginfo[scan->cur_seg];

		if (curSegInfo->total_tupcount > 0)
		{
			bool		emptySeg = false;

			/*
			 * If the segment is entirely empty, nothing to do.
			 *
			 * We assume the corresponding segments for every column to be in
			 * the same state. So somewhat arbitrarily, we check the state of
			 * the first column we'll be accessing.
			 */

			/*
			 * subtle: we must check for AWAITING_DROP before calling getAOCSVPEntry().
			 * ALTER TABLE ADD COLUMN does not update vpinfos on AWAITING_DROP segments.
			 */
			if (curSegInfo->state == AOSEG_STATE_AWAITING_DROP)
				emptySeg = true;
			else
			{
				AOCSVPInfoEntry *e;

				e = getAOCSVPEntry(curSegInfo, scan->columnScanInfo.proj_atts[0]);
				if (e->eof == 0)
					elog(ERROR, "inconsistent segment state for relation %s, segment %d, tuple count " INT64_FORMAT,
						 RelationGetRelationName(scan->rs_base.rs_rd),
						 curSegInfo->segno,
						 curSegInfo->total_tupcount);
			}

			if (!emptySeg)
			{

				/*
				 * If the scan also builds the block directory, initialize it
				 * here.
				 */
				if (scan->blockDirectory)
				{
					AppendOnlyBlockDirectory_Init_forInsert(scan->blockDirectory,
															scan->appendOnlyMetaDataSnapshot,
															(FileSegInfo *) curSegInfo,
															0 /* lastSequence */ ,
															scan->rs_base.rs_rd,
															curSegInfo->segno,
															scan->columnScanInfo.relationTupleDesc->natts,
															true);
				}

				open_all_datumstreamread_segfiles(scan, curSegInfo);

				return scan->cur_seg;
			}
		}
	}

	return -1;
}

static void
close_cur_scan_seg(AOCSScanDesc scan)
{
	if (scan->cur_seg < 0)
		return;

	/*
	 * If rescan is called before we lazily initialized then there is nothing to
	 * do
	 */
	if (scan->columnScanInfo.relationTupleDesc == NULL)
		return;

	for (AttrNumber attno = 0;
		 attno < scan->columnScanInfo.relationTupleDesc->natts;
		 attno++)
	{
		if (scan->columnScanInfo.ds[attno])
			datumstreamread_close_file(scan->columnScanInfo.ds[attno]);
	}

	if (scan->blockDirectory)
		AppendOnlyBlockDirectory_End_forInsert(scan->blockDirectory);
}

/*
 * aocs_beginrangescan
 *
 * begins range-limited relation scan
 */
AOCSScanDesc
aocs_beginrangescan(Relation relation,
					Snapshot snapshot,
					Snapshot appendOnlyMetaDataSnapshot,
					int *segfile_no_arr, int segfile_count)
{
	AOCSFileSegInfo **seginfo;
	int			i;

	RelationIncrementReferenceCount(relation);

	seginfo = palloc0(sizeof(AOCSFileSegInfo *) * segfile_count);

	for (i = 0; i < segfile_count; i++)
	{
		seginfo[i] = GetAOCSFileSegInfo(relation, appendOnlyMetaDataSnapshot,
										segfile_no_arr[i], false);
	}
	return aocs_beginscan_internal(relation,
								   seginfo,
								   segfile_count,
								   snapshot,
								   appendOnlyMetaDataSnapshot,
								   NULL,
								   0);
}

AOCSScanDesc
aocs_beginscan(Relation relation,
			   Snapshot snapshot,
			   bool *proj,
			   uint32 flags)
{
	AOCSFileSegInfo	  **seginfo;
	Snapshot			aocsMetaDataSnapshot;
	int32				total_seg;

	RelationIncrementReferenceCount(relation);

	/*
	 * The append-only meta data should never be fetched with
	 * SnapshotAny as bogus results are returned.
	 * We use SnapshotSelf for metadata, as regular MVCC snapshot can hide newly
	 * globally inserted tuples from global index build process.
	 */
	if (snapshot != SnapshotAny)
		aocsMetaDataSnapshot = snapshot;
	else
		aocsMetaDataSnapshot = SnapshotSelf;

	seginfo = GetAllAOCSFileSegInfo(relation, aocsMetaDataSnapshot, &total_seg, NULL);
	return aocs_beginscan_internal(relation,
								   seginfo,
								   total_seg,
								   snapshot,
								   aocsMetaDataSnapshot,
								   proj,
								   flags);
}

/*
 * begin the scan over the given relation.
 */
static AOCSScanDesc
aocs_beginscan_internal(Relation relation,
						AOCSFileSegInfo **seginfo,
						int total_seg,
						Snapshot snapshot,
						Snapshot appendOnlyMetaDataSnapshot,
						bool *proj,
						uint32 flags)
{
	AOCSScanDesc	scan;
	AttrNumber		natts;
	Oid				visimaprelid;

	scan = (AOCSScanDesc) palloc0(sizeof(AOCSScanDescData));
	scan->rs_base.rs_rd = relation;
	scan->rs_base.rs_snapshot = snapshot;
	scan->rs_base.rs_flags = flags;
	scan->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;
	scan->seginfo = seginfo;
	scan->total_seg = total_seg;
	scan->columnScanInfo.scanCtx = CurrentMemoryContext;

	/* relationTupleDesc will be inited by the slot when needed */
	scan->columnScanInfo.relationTupleDesc = NULL;

	/*
	 * We get an array of booleans to indicate which columns are needed. But
	 * if you have a very wide table, and you only select a few columns from
	 * it, just scanning the boolean array to figure out which columns are
	 * needed can incur a noticeable overhead in aocs_getnext. So convert it
	 * into an array of the attribute numbers of the required columns.
	 *
	 * However, if no array is given, then let it get lazily initialized when
	 * needed since all the attributes will be fetched.
	 */
	if (proj)
	{
		natts = RelationGetNumberOfAttributes(relation);
		scan->columnScanInfo.proj_atts = (AttrNumber *)
										 palloc0(natts * sizeof(AttrNumber));
		scan->columnScanInfo.num_proj_atts = 0;

		for (AttrNumber i = 0; i < natts; i++)
		{
			if (proj[i])
				scan->columnScanInfo.proj_atts[scan->columnScanInfo.num_proj_atts++] = i;
		}
	}

	scan->columnScanInfo.ds = NULL;

	GetAppendOnlyEntryAttributes(RelationGetRelid(relation),
								 NULL,
								 NULL,
								 &scan->checksum,
								 NULL);

	GetAppendOnlyEntryAuxOids(relation,
							  NULL, NULL,
							  &visimaprelid);

	if (scan->total_seg != 0)
		AppendOnlyVisimap_Init(&scan->visibilityMap,
							   visimaprelid,
							   AccessShareLock,
							   appendOnlyMetaDataSnapshot);

	return scan;
}

void
aocs_rescan(AOCSScanDesc scan)
{
	close_cur_scan_seg(scan);
	if (scan->columnScanInfo.ds)
		close_ds_read(scan->columnScanInfo.ds, scan->columnScanInfo.relationTupleDesc->natts);
	initscan_with_colinfo(scan);
}

void
aocs_endscan(AOCSScanDesc scan)
{
	close_cur_scan_seg(scan);

	if (scan->columnScanInfo.ds)
	{
		Assert(scan->columnScanInfo.proj_atts);

		close_ds_read(scan->columnScanInfo.ds, scan->columnScanInfo.relationTupleDesc->natts);
		pfree(scan->columnScanInfo.ds);
		scan->columnScanInfo.ds = NULL;
	}

	if (scan->columnScanInfo.relationTupleDesc)
	{
		Assert(scan->columnScanInfo.proj_atts);

		ReleaseTupleDesc(scan->columnScanInfo.relationTupleDesc);
		scan->columnScanInfo.relationTupleDesc = NULL;
	}

	if (scan->columnScanInfo.proj_atts)
		pfree(scan->columnScanInfo.proj_atts);

	for (int i = 0; i < scan->total_seg; ++i)
	{
		if (scan->seginfo[i])
		{
			pfree(scan->seginfo[i]);
			scan->seginfo[i] = NULL;
		}
	}

	if (scan->seginfo)
		pfree(scan->seginfo);

	if (scan->total_seg != 0)
		AppendOnlyVisimap_Finish(&scan->visibilityMap, AccessShareLock);

	RelationDecrementReferenceCount(scan->rs_base.rs_rd);

	pfree(scan);
}

bool
aocs_getnext(AOCSScanDesc scan, ScanDirection direction, TupleTableSlot *slot)
{
	Datum	   *d = slot->tts_values;
	bool	   *null = slot->tts_isnull;

	AOTupleId	aoTupleId;
	int64		rowNum = INT64CONST(-1);
	int			err = 0;
	bool		isSnapshotAny = (scan->rs_base.rs_snapshot == SnapshotAny);
	AttrNumber	natts;

	Assert(ScanDirectionIsForward(direction));

	if (scan->columnScanInfo.relationTupleDesc == NULL)
	{
		scan->columnScanInfo.relationTupleDesc = slot->tts_tupleDescriptor;
		/* Pin it! ... and of course release it upon destruction / rescan */
		PinTupleDesc(scan->columnScanInfo.relationTupleDesc);
		initscan_with_colinfo(scan);
	}

	natts = slot->tts_tupleDescriptor->natts;
	Assert(natts <= scan->columnScanInfo.relationTupleDesc->natts);

	while (1)
	{
		AOCSFileSegInfo *curseginfo;

ReadNext:
		/* If necessary, open next seg */
		if (scan->cur_seg < 0 || err < 0)
		{
			err = open_next_scan_seg(scan);
			if (err < 0)
			{
				/* No more seg, we are at the end */
				ExecClearTuple(slot);
				scan->cur_seg = -1;
				return false;
			}
			scan->cur_seg_row = 0;
		}

		Assert(scan->cur_seg >= 0);
		curseginfo = scan->seginfo[scan->cur_seg];

		/* Read from cur_seg */
		for (AttrNumber i = 0; i < scan->columnScanInfo.num_proj_atts; i++)
		{
			AttrNumber	attno = scan->columnScanInfo.proj_atts[i];

			err = datumstreamread_advance(scan->columnScanInfo.ds[attno]);
			Assert(err >= 0);
			if (err == 0)
			{
				err = datumstreamread_block(scan->columnScanInfo.ds[attno], scan->blockDirectory, attno);
				if (err < 0)
				{
					/*
					 * Ha, cannot read next block, we need to go to next seg
					 */
					close_cur_scan_seg(scan);
					goto ReadNext;
				}

				AOCSScanDesc_UpdateTotalBytesRead(scan, attno);

				err = datumstreamread_advance(scan->columnScanInfo.ds[attno]);
				Assert(err > 0);
			}

			/*
			 * Get the column's datum right here since the data structures
			 * should still be hot in CPU data cache memory.
			 */
			datumstreamread_get(scan->columnScanInfo.ds[attno], &d[attno], &null[attno]);

			if (rowNum == INT64CONST(-1) &&
				scan->columnScanInfo.ds[attno]->blockFirstRowNum != INT64CONST(-1))
			{
				Assert(scan->columnScanInfo.ds[attno]->blockFirstRowNum > 0);
				rowNum = scan->columnScanInfo.ds[attno]->blockFirstRowNum +
					datumstreamread_nth(scan->columnScanInfo.ds[attno]);
			}
		}

		scan->cur_seg_row++;
		if (rowNum == INT64CONST(-1))
		{
			AOTupleIdInit(&aoTupleId, curseginfo->segno, scan->cur_seg_row);
		}
		else
		{
			AOTupleIdInit(&aoTupleId, curseginfo->segno, rowNum);
		}

		if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&scan->visibilityMap, &aoTupleId))
		{
			/*
			 * The tuple is invisible.
			 * In `analyze`, we can simply return false
			 */
			if ((scan->rs_base.rs_flags & SO_TYPE_ANALYZE) != 0)
				return false;

			rowNum = INT64CONST(-1);
			goto ReadNext;
		}
		scan->cdb_fake_ctid = *((ItemPointer) &aoTupleId);

		slot->tts_nvalid = natts;
		slot->tts_tid = scan->cdb_fake_ctid;
		return true;
	}

	Assert(!"Never here");
	return false;
}


/* Open next file segment for write.  See SetCurrentFileSegForWrite */
/* XXX Right now, we put each column to different files */
static void
OpenAOCSDatumStreams(AOCSInsertDesc desc)
{
	RelFileNodeBackend rnode;
	char	   *basepath;
	char		fn[MAXPGPATH];
	int32		fileSegNo;

	AOCSFileSegInfo *seginfo;

	TupleDesc	tupdesc = RelationGetDescr(desc->aoi_rel);
	int			nvp = tupdesc->natts;
	int			i;

	desc->ds = (DatumStreamWrite **) palloc0(sizeof(DatumStreamWrite *) * nvp);

	open_ds_write(desc->aoi_rel, desc->ds, tupdesc,
				  desc->checksum);

	/* Now open seg info file and get eof mark. */
	seginfo = GetAOCSFileSegInfo(desc->aoi_rel,
								 desc->appendOnlyMetaDataSnapshot,
								 desc->cur_segno,
								 true);

	desc->fsInfo = seginfo;

	/* Never insert into a segment that is awaiting a drop */
	if (desc->fsInfo->state == AOSEG_STATE_AWAITING_DROP)
		elog(ERROR,
			 "cannot insert into segno (%d) for AO relid %d that is in state AOSEG_STATE_AWAITING_DROP",
			 desc->cur_segno, RelationGetRelid(desc->aoi_rel));

	desc->rowCount = seginfo->total_tupcount;

	rnode.node = desc->aoi_rel->rd_node;
	rnode.backend = desc->aoi_rel->rd_backend;
	basepath = relpath(rnode, MAIN_FORKNUM);

	for (i = 0; i < nvp; ++i)
	{
		AOCSVPInfoEntry *e = getAOCSVPEntry(seginfo, i);

		/* Filenum for the column */
		FileNumber filenum = GetFilenumForAttribute(RelationGetRelid(desc->aoi_rel), i + 1);

		FormatAOSegmentFileName(basepath, seginfo->segno, filenum, &fileSegNo, fn);
		Assert(strlen(fn) + 1 <= MAXPGPATH);

		datumstreamwrite_open_file(desc->ds[i], fn, e->eof, e->eof_uncompressed,
								   &rnode,
								   fileSegNo, seginfo->formatversion);
	}

	pfree(basepath);
}

static inline void
SetBlockFirstRowNums(DatumStreamWrite **datumStreams,
					 int numDatumStreams,
					 int64 blockFirstRowNum)
{
	int			i;

	Assert(datumStreams != NULL);

	for (i = 0; i < numDatumStreams; i++)
	{
		Assert(datumStreams[i] != NULL);

		datumStreams[i]->blockFirstRowNum = blockFirstRowNum;
	}
}


AOCSInsertDesc
aocs_insert_init(Relation rel, int segno, int64 num_rows)
{
    NameData    nd;
	AOCSInsertDesc desc;
	TupleDesc	tupleDesc;
	int64		firstSequence = 0;

	desc = (AOCSInsertDesc) palloc0(sizeof(AOCSInsertDescData));
	desc->aoi_rel = rel;
	desc->appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));

	/*
	 * Writers uses this since they have exclusive access to the lock acquired
	 * with LockRelationAppendOnlySegmentFile for the segment-file.
	 */

	tupleDesc = RelationGetDescr(desc->aoi_rel);

	Assert(segno >= 0);
	desc->cur_segno = segno;

    GetAppendOnlyEntryAttributes(rel->rd_id,
                                 &desc->blocksz,
                                 (int16 *)&desc->compLevel,
                                 &desc->checksum,
                                 &nd);
    desc->compType = NameStr(nd);

    GetAppendOnlyEntryAuxOids(rel,
                              &desc->segrelid, &desc->blkdirrelid,
                              &desc->visimaprelid);

	OpenAOCSDatumStreams(desc);

	/*
	 * Obtain the next list of fast sequences for this relation.
	 *
	 * Even in the case of no indexes, we need to update the fast sequences,
	 * since the table may contain indexes at some point of time.
	 */
	firstSequence = GetFastSequences(desc->segrelid, segno, num_rows);
	desc->numSequences = num_rows;
	Assert(firstSequence > desc->rowCount);
	desc->lastSequence = firstSequence - 1;

	SetBlockFirstRowNums(desc->ds, tupleDesc->natts, desc->lastSequence + 1);

	/* Initialize the block directory. */
	tupleDesc = RelationGetDescr(rel);
	AppendOnlyBlockDirectory_Init_forInsert(&(desc->blockDirectory),
											desc->appendOnlyMetaDataSnapshot,	/* CONCERN: Safe to
																				 * assume all block
																				 * directory entries for
																				 * segment are "covered"
																				 * by same exclusive
																				 * lock. */
											(FileSegInfo *) desc->fsInfo, desc->lastSequence,
											rel, segno, tupleDesc->natts, true);

	return desc;
}


void
aocs_insert_values(AOCSInsertDesc idesc, Datum *d, bool *null, AOTupleId *aoTupleId)
{
	Relation	rel = idesc->aoi_rel;
	int			i;

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   "appendonly_insert",
								   DDLNotSpecified,
								   "",	/* databaseName */
								   RelationGetRelationName(idesc->aoi_rel));	/* tableName */
#endif

	/* As usual, at this moment, we assume one col per vp */
	for (i = 0; i < RelationGetNumberOfAttributes(rel); ++i)
	{
		void	   *toFree1;
		Datum		datum = d[i];
		int			err = datumstreamwrite_put(idesc->ds[i], datum, null[i], &toFree1);

		if (toFree1 != NULL)
		{
			/*
			 * Use the de-toasted and/or de-compressed as datum instead.
			 */
			datum = PointerGetDatum(toFree1);
		}
		if (err < 0)
		{
			int			itemCount = datumstreamwrite_nth(idesc->ds[i]);
			void	   *toFree2;

			/* write the block up to this one */
			datumstreamwrite_block(idesc->ds[i], &idesc->blockDirectory, i, false);
			if (itemCount > 0)
			{
				/*
				 * since we have written all up to the new tuple, the new
				 * blockFirstRowNum is the inserted tuple's row number
				 */
				idesc->ds[i]->blockFirstRowNum = idesc->lastSequence + 1;
			}

			Assert(idesc->ds[i]->blockFirstRowNum == idesc->lastSequence + 1);


			/* now write this new item to the new block */
			err = datumstreamwrite_put(idesc->ds[i], datum, null[i], &toFree2);
			Assert(toFree2 == NULL);
			if (err < 0)
			{
				Assert(!null[i]);
				err = datumstreamwrite_lob(idesc->ds[i],
										   datum,
										   &idesc->blockDirectory,
										   i,
										   false);
				Assert(err >= 0);

				/*
				 * A lob will live by itself in the block so this assignment
				 * is for the block that contains tuples AFTER the one we are
				 * inserting
				 */
				idesc->ds[i]->blockFirstRowNum = idesc->lastSequence + 2;
			}
		}

		if (toFree1 != NULL)
			pfree(toFree1);
	}

	idesc->insertCount++;
	idesc->lastSequence++;
	if (idesc->numSequences > 0)
		(idesc->numSequences)--;

	Assert(idesc->numSequences >= 0);

	AOTupleIdInit(aoTupleId, idesc->cur_segno, idesc->lastSequence);

	/*
	 * If the allocated fast sequence numbers are used up, we request for a
	 * next list of fast sequence numbers.
	 */
	if (idesc->numSequences == 0)
	{
		int64 firstSequence PG_USED_FOR_ASSERTS_ONLY = GetFastSequences(idesc->segrelid,
																		idesc->cur_segno,
																		NUM_FAST_SEQUENCES);

		Assert(firstSequence == idesc->lastSequence + 1);
		idesc->numSequences = NUM_FAST_SEQUENCES;
	}
}

void
aocs_insert_finish(AOCSInsertDesc idesc)
{
	Relation	rel = idesc->aoi_rel;
	int			i;

	for (i = 0; i < rel->rd_att->natts; ++i)
	{
		datumstreamwrite_block(idesc->ds[i], &idesc->blockDirectory, i, false);
		datumstreamwrite_close_file(idesc->ds[i]);
	}

	AppendOnlyBlockDirectory_End_forInsert(&(idesc->blockDirectory));

	UpdateAOCSFileSegInfo(idesc);

	UnregisterSnapshot(idesc->appendOnlyMetaDataSnapshot);

	pfree(idesc->fsInfo);

	close_ds_write(idesc->ds, rel->rd_att->natts);
}

static void
positionFirstBlockOfRange(DatumStreamFetchDesc datumStreamFetchDesc)
{
	AppendOnlyBlockDirectoryEntry_GetBeginRange(
												&datumStreamFetchDesc->currentBlock.blockDirectoryEntry,
												&datumStreamFetchDesc->scanNextFileOffset,
												&datumStreamFetchDesc->scanNextRowNum);
}

static void
positionLimitToEndOfRange(DatumStreamFetchDesc datumStreamFetchDesc)
{
	AppendOnlyBlockDirectoryEntry_GetEndRange(
											  &datumStreamFetchDesc->currentBlock.blockDirectoryEntry,
											  &datumStreamFetchDesc->scanAfterFileOffset,
											  &datumStreamFetchDesc->scanLastRowNum);
}


static void
positionSkipCurrentBlock(DatumStreamFetchDesc datumStreamFetchDesc)
{
	datumStreamFetchDesc->scanNextFileOffset =
		datumStreamFetchDesc->currentBlock.fileOffset +
		datumStreamFetchDesc->currentBlock.overallBlockLen;

	datumStreamFetchDesc->scanNextRowNum =
		datumStreamFetchDesc->currentBlock.lastRowNum + 1;
}

/*
 * Fetch the tuple's datum from the block indicated by the block directory entry
 * that covers the tuple, given the colno.
 */
static void
fetchFromCurrentBlock(AOCSFetchDesc aocsFetchDesc,
					  int64 rowNum,
					  TupleTableSlot *slot,
					  int colno)
{
	DatumStreamFetchDesc datumStreamFetchDesc =
	aocsFetchDesc->datumStreamFetchDesc[colno];
	DatumStreamRead *datumStream = datumStreamFetchDesc->datumStream;
	Datum		value;
	bool		null;
	int			rowNumInBlock = rowNum - datumStreamFetchDesc->currentBlock.firstRowNum;

	Assert(rowNumInBlock >= 0);

	/*
	 * MPP-17061: gotContents could be false in the case of aborted rows. As
	 * described in the repro in MPP-17061, if aocs_fetch is trying to fetch
	 * an invisible/aborted row, it could set the block header metadata of
	 * currentBlock to the next CO block, but without actually reading in next
	 * CO block's content.
	 */
	if (datumStreamFetchDesc->currentBlock.gotContents == false)
	{
		datumstreamread_block_content(datumStream);
		datumStreamFetchDesc->currentBlock.gotContents = true;
	}

	datumstreamread_find(datumStream, rowNumInBlock);

	if (slot != NULL)
	{
		Datum	   *values = slot->tts_values;
		bool	   *nulls = slot->tts_isnull;

		datumstreamread_get(datumStream, &(values[colno]), &(nulls[colno]));

	}
	else
	{
		datumstreamread_get(datumStream, &value, &null);
	}
}

static bool
scanToFetchValue(AOCSFetchDesc aocsFetchDesc,
				 int64 rowNum,
				 TupleTableSlot *slot,
				 int colno)
{
	DatumStreamFetchDesc 			datumStreamFetchDesc = aocsFetchDesc->datumStreamFetchDesc[colno];
	DatumStreamRead 				*datumStream = datumStreamFetchDesc->datumStream;
	AOFetchBlockMetadata 			*currentBlock = &datumStreamFetchDesc->currentBlock;
	AppendOnlyBlockDirectoryEntry 	*entry = &currentBlock->blockDirectoryEntry;
	bool							found;

	found = datumstreamread_find_block(datumStream,
									   datumStreamFetchDesc,
									   rowNum);
	if (!found)
	{
		if (AppendOnlyBlockDirectoryEntry_RangeHasRow(entry, rowNum))
		{
			/*
			 * We fell into a hole inside the resolved block directory entry
			 * we obtained from AppendOnlyBlockDirectory_GetEntry().
			 * This should not be happening for versions >= GP7. Scream
			 * appropriately. See AppendOnlyBlockDirectoryEntry for details.
			 */
			ereportif(aocsFetchDesc->relation->rd_appendonly->version >= AORelationVersion_GP7,
					  ERROR,
					  (errcode(ERRCODE_INTERNAL_ERROR),
					   errmsg("datum with row number %ld and col no %d not found in block directory entry range", rowNum, colno),
					   errdetail("block directory entry: (fileOffset = %ld, firstRowNum = %ld, "
								 "afterFileOffset = %ld, lastRowNum = %ld)",
								 entry->range.fileOffset,
								 entry->range.firstRowNum,
								 entry->range.afterFileOffset,
								 entry->range.lastRowNum)));
		}
		else
		{
			/*
			 * The resolved block directory entry we obtained from
			 * AppendOnlyBlockDirectory_GetEntry() has range s.t.
			 * firstRowNum < lastRowNum < rowNum
			 * This can happen when rowNum maps to an aborted transaction, and
			 * we find an earlier committed block directory row due to the
			 * <= scan condition in AppendOnlyBlockDirectory_GetEntry().
			 */
		}
	}
	else
		fetchFromCurrentBlock(aocsFetchDesc, rowNum, slot, colno);

	return found;
}

static void
closeFetchSegmentFile(DatumStreamFetchDesc datumStreamFetchDesc)
{
	Assert(datumStreamFetchDesc->currentSegmentFile.isOpen);

	datumstreamread_close_file(datumStreamFetchDesc->datumStream);
	datumStreamFetchDesc->currentSegmentFile.isOpen = false;
}

static bool
openFetchSegmentFile(AOCSFetchDesc aocsFetchDesc,
					 int openSegmentFileNum,
					 int colNo)
{
	int			i;

	AOCSFileSegInfo *fsInfo;
	int			segmentFileNum;
	int64		logicalEof;
	DatumStreamFetchDesc datumStreamFetchDesc = aocsFetchDesc->datumStreamFetchDesc[colNo];

	Assert(!datumStreamFetchDesc->currentSegmentFile.isOpen);

	i = 0;
	while (true)
	{
		if (i >= aocsFetchDesc->totalSegfiles)
			return false;
		/* Segment file not visible in catalog information. */

		fsInfo = aocsFetchDesc->segmentFileInfo[i];
		segmentFileNum = fsInfo->segno;
		if (openSegmentFileNum == segmentFileNum)
		{
			break;
		}
		i++;
	}

	/*
	 * Don't try to open a segment file when its EOF is 0, since the file may
	 * not exist. See MPP-8280. Also skip the segment file if it is awaiting a
	 * drop.
	 *
	 * Check for awaiting-drop first, before accessing the vpinfo, because
	 * vpinfo might not be valid on awaiting-drop segment after adding a column.
	 */
	if (fsInfo->state == AOSEG_STATE_AWAITING_DROP)
		return false;

	AOCSVPInfoEntry *entry = getAOCSVPEntry(fsInfo, colNo);
	logicalEof = entry->eof;
	if (logicalEof == 0)
		return false;

	open_datumstreamread_segfile(aocsFetchDesc->basepath, aocsFetchDesc->relation,
								 fsInfo,
								 datumStreamFetchDesc->datumStream,
								 colNo);

	datumStreamFetchDesc->currentSegmentFile.num = openSegmentFileNum;
	datumStreamFetchDesc->currentSegmentFile.logicalEof = logicalEof;

	datumStreamFetchDesc->currentSegmentFile.isOpen = true;

	return true;
}

/*
 * Note: we don't reset the block directory entry here. This is crucial, so we
 * can use the block directory entry later on. See comment in AOFetchBlockMetadata
 * FIXME: reset other fields here.
 */
static void
resetCurrentBlockInfo(AOFetchBlockMetadata *currentBlock)
{
	currentBlock->valid = false;
	currentBlock->firstRowNum = 0;
	currentBlock->lastRowNum = 0;
}

/*
 * Initialize the fetch descriptor.
 */
AOCSFetchDesc
aocs_fetch_init(Relation relation,
				Snapshot snapshot,
				Snapshot appendOnlyMetaDataSnapshot,
				bool *proj)
{
	AOCSFetchDesc aocsFetchDesc;
	int			colno;
	char	   *basePath = relpathbackend(relation->rd_node, relation->rd_backend, MAIN_FORKNUM);
	TupleDesc	tupleDesc = RelationGetDescr(relation);
	StdRdOptions **opts = RelationGetAttributeOptions(relation);
	int			segno;

	/*
	 * increment relation ref count while scanning relation
	 *
	 * This is just to make really sure the relcache entry won't go away while
	 * the scan has a pointer to it.  Caller should be holding the rel open
	 * anyway, so this is redundant in all normal scenarios...
	 */
	RelationIncrementReferenceCount(relation);

	aocsFetchDesc = (AOCSFetchDesc) palloc0(sizeof(AOCSFetchDescData));
	aocsFetchDesc->relation = relation;

	aocsFetchDesc->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;
	aocsFetchDesc->snapshot = snapshot;


	aocsFetchDesc->initContext = CurrentMemoryContext;

	aocsFetchDesc->segmentFileNameMaxLen = AOSegmentFilePathNameLen(relation) + 1;
	aocsFetchDesc->segmentFileName =
		(char *) palloc(aocsFetchDesc->segmentFileNameMaxLen);
	aocsFetchDesc->segmentFileName[0] = '\0';
	aocsFetchDesc->basepath = basePath;

	Assert(proj);

    bool checksum = true;
    Oid visimaprelid;
    GetAppendOnlyEntryAuxOids(relation,
                              &aocsFetchDesc->segrelid, NULL,
                              &visimaprelid);

    GetAppendOnlyEntryAttributes(relation->rd_id,
                                 NULL,
                                 NULL,
                                 &checksum,
                                 NULL);

	aocsFetchDesc->segmentFileInfo =
		GetAllAOCSFileSegInfo(relation, appendOnlyMetaDataSnapshot, &aocsFetchDesc->totalSegfiles, NULL);

	/* 
	 * Initialize lastSequence only for segments which we got above is sufficient,
	 * rather than all AOTupleId_MultiplierSegmentFileNum ones that introducing
	 * too many unnecessary calls in most cases.
	 */
	memset(aocsFetchDesc->lastSequence, -1, sizeof(aocsFetchDesc->lastSequence));
	for (int i = -1; i < aocsFetchDesc->totalSegfiles; i++)
	{
		/* always initailize segment 0 */
		segno = (i < 0 ? 0 : aocsFetchDesc->segmentFileInfo[i]->segno);
		/* set corresponding bit for target segment */
		aocsFetchDesc->lastSequence[segno] = ReadLastSequence(aocsFetchDesc->segrelid, segno);
	}

	AppendOnlyBlockDirectory_Init_forSearch(
											&aocsFetchDesc->blockDirectory,
											appendOnlyMetaDataSnapshot,
											(FileSegInfo **) aocsFetchDesc->segmentFileInfo,
											aocsFetchDesc->totalSegfiles,
											aocsFetchDesc->relation,
											relation->rd_att->natts,
											true,
											proj);

	Assert(relation->rd_att != NULL);

	aocsFetchDesc->datumStreamFetchDesc = (DatumStreamFetchDesc *)
		palloc0(relation->rd_att->natts * sizeof(DatumStreamFetchDesc));

	for (colno = 0; colno < relation->rd_att->natts; colno++)
	{

		aocsFetchDesc->datumStreamFetchDesc[colno] = NULL;
		if (proj[colno])
		{
			char	   *ct;
			int32		clvl;
			int32		blksz;

			StringInfoData titleBuf;

			/*
			 * We always record all the three column specific attributes for
			 * each column of a column oriented table. Note: checksum is a
			 * table level attribute.
			 */
			Assert(opts[colno]);
			ct = opts[colno]->compresstype;
			clvl = opts[colno]->compresslevel;
			blksz = opts[colno]->blocksize;

			/* UNDONE: Need to track and dispose of this storage... */
			initStringInfo(&titleBuf);
			appendStringInfo(&titleBuf, "Fetch from Append-Only Column-Oriented relation '%s', column #%d '%s'",
							 RelationGetRelationName(relation),
							 colno + 1,
							 NameStr(TupleDescAttr(tupleDesc, colno)->attname));

			aocsFetchDesc->datumStreamFetchDesc[colno] = (DatumStreamFetchDesc)
				palloc0(sizeof(DatumStreamFetchDescData));

			aocsFetchDesc->datumStreamFetchDesc[colno]->datumStream =
				create_datumstreamread(ct,
									   clvl,
									   checksum,
									   blksz,
									   TupleDescAttr(tupleDesc, colno),
									   relation->rd_rel->relname.data,
									    /* title */ titleBuf.data);

		}
		if (opts[colno])
			pfree(opts[colno]);
	}
	if (opts)
		pfree(opts);
	AppendOnlyVisimap_Init(&aocsFetchDesc->visibilityMap,
						   visimaprelid,
						   AccessShareLock,
						   appendOnlyMetaDataSnapshot);

	return aocsFetchDesc;
}

/*
 * Fetch the tuple based on the given tuple id.
 *
 * If the 'slot' is not NULL, the tuple will be assigned to the slot.
 *
 * Return true if the tuple is found. Otherwise, return false.
 */
bool
aocs_fetch(AOCSFetchDesc aocsFetchDesc,
		   AOTupleId *aoTupleId,
		   TupleTableSlot *slot)
{
	int			segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	int64		rowNum = AOTupleIdGet_rowNum(aoTupleId);
	int			numCols = aocsFetchDesc->relation->rd_att->natts;
	int			colno;
	bool		found = true;
	bool		isSnapshotAny = (aocsFetchDesc->snapshot == SnapshotAny);

	Assert(numCols > 0);

	Assert(segmentFileNum >= 0);

	if (aocsFetchDesc->lastSequence[segmentFileNum] == InvalidAORowNum)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Row No. %ld in segment file No. %d is out of scanning scope for target relfilenode %u.",
				 		rowNum, segmentFileNum, aocsFetchDesc->relation->rd_node.relNode)));

	/*
	 * if the rowNum is bigger than lastsequence, skip it.
	 */
	if (rowNum > aocsFetchDesc->lastSequence[segmentFileNum])
	{
		if (slot != NULL)
			slot = ExecClearTuple(slot);
		return false;
	}

	/*
	 * Go through columns one by one. Check if the current block has the
	 * requested tuple. If so, fetch it. Otherwise, read the block that
	 * contains the requested tuple.
	 */
	for (colno = 0; colno < numCols; colno++)
	{
		DatumStreamFetchDesc datumStreamFetchDesc = aocsFetchDesc->datumStreamFetchDesc[colno];

		/* If this column does not need to be fetched, skip it. */
		if (datumStreamFetchDesc == NULL)
			continue;

		elogif(Debug_appendonly_print_datumstream, LOG,
			   "aocs_fetch filePathName %s segno %u rowNum  " INT64_FORMAT
			   " firstRowNum " INT64_FORMAT " lastRowNum " INT64_FORMAT " ",
			   datumStreamFetchDesc->datumStream->ao_read.bufferedRead.filePathName,
			   datumStreamFetchDesc->currentSegmentFile.num,
			   rowNum,
			   datumStreamFetchDesc->currentBlock.firstRowNum,
			   datumStreamFetchDesc->currentBlock.lastRowNum);

		/*
		 * If the current block has the requested tuple, read it.
		 */
		if (datumStreamFetchDesc->currentSegmentFile.isOpen &&
			datumStreamFetchDesc->currentSegmentFile.num == segmentFileNum &&
			aocsFetchDesc->blockDirectory.currentSegmentFileNum == segmentFileNum &&
			datumStreamFetchDesc->currentBlock.valid)
		{
			if (rowNum >= datumStreamFetchDesc->currentBlock.firstRowNum &&
				rowNum <= datumStreamFetchDesc->currentBlock.lastRowNum)
			{
				if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&aocsFetchDesc->visibilityMap, aoTupleId))
				{
					found = false;
					break;
				}

				fetchFromCurrentBlock(aocsFetchDesc, rowNum, slot, colno);
				continue;
			}

			/*
			 * Otherwise, fetch the right block.
			 */
			if (AppendOnlyBlockDirectoryEntry_RangeHasRow(
														  &(datumStreamFetchDesc->currentBlock.blockDirectoryEntry),
														  rowNum))
			{
				/*
				 * The tuple is covered by the current Block Directory entry,
				 * but is it before or after our current block?
				 */
				if (rowNum < datumStreamFetchDesc->currentBlock.firstRowNum)
				{
					/*
					 * Set scan range to prior block
					 */
					positionFirstBlockOfRange(datumStreamFetchDesc);

					datumStreamFetchDesc->scanAfterFileOffset =
						datumStreamFetchDesc->currentBlock.fileOffset;
					datumStreamFetchDesc->scanLastRowNum =
						datumStreamFetchDesc->currentBlock.firstRowNum - 1;
				}
				else
				{
					/*
					 * Set scan range to following blocks.
					 */
					positionSkipCurrentBlock(datumStreamFetchDesc);
					positionLimitToEndOfRange(datumStreamFetchDesc);
				}

				if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&aocsFetchDesc->visibilityMap, aoTupleId))
				{
					found = false;
					break;
				}

				if (!scanToFetchValue(aocsFetchDesc, rowNum, slot, colno))
				{
					found = false;
					break;
				}

				continue;
			}
		}

		/*
		 * Open or switch open, if necessary.
		 */
		if (datumStreamFetchDesc->currentSegmentFile.isOpen &&
			segmentFileNum != datumStreamFetchDesc->currentSegmentFile.num)
		{
			closeFetchSegmentFile(datumStreamFetchDesc);

			Assert(!datumStreamFetchDesc->currentSegmentFile.isOpen);
		}

		if (!datumStreamFetchDesc->currentSegmentFile.isOpen)
		{
			if (!openFetchSegmentFile(aocsFetchDesc,
									  segmentFileNum,
									  colno))
			{
				found = false;
				/* Segment file not in aoseg table.. */
				/* Must be aborted or deleted and reclaimed. */
				break;
			}

			/* Reset currentBlock info */
			resetCurrentBlockInfo(&(datumStreamFetchDesc->currentBlock));
		}

		/*
		 * Need to get the Block Directory entry that covers the TID.
		 */
		if (!AppendOnlyBlockDirectory_GetEntry(&aocsFetchDesc->blockDirectory,
											   aoTupleId,
											   colno,
											   &datumStreamFetchDesc->currentBlock.blockDirectoryEntry))
		{
			found = false;		/* Row not represented in Block Directory. */
			/* Must be aborted or deleted and reclaimed. */
			break;
		}

		if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&aocsFetchDesc->visibilityMap, aoTupleId))
		{
			found = false;
			break;
		}

		/*
		 * Set scan range covered by new Block Directory entry.
		 */
		positionFirstBlockOfRange(datumStreamFetchDesc);

		positionLimitToEndOfRange(datumStreamFetchDesc);

		if (!scanToFetchValue(aocsFetchDesc, rowNum, slot, colno))
		{
			found = false;
			break;
		}
	}

	if (found)
	{
		if (slot != NULL)
		{
			slot->tts_nvalid = colno;
			slot->tts_tid = *(ItemPointer)(aoTupleId);
		}
	}
	else
	{
		if (slot != NULL)
			slot = ExecClearTuple(slot);
	}

	return found;
}

void
aocs_fetch_finish(AOCSFetchDesc aocsFetchDesc)
{
	int			colno;
	Relation	relation = aocsFetchDesc->relation;

	Assert(relation != NULL && relation->rd_att != NULL);

	for (colno = 0; colno < relation->rd_att->natts; colno++)
	{
		DatumStreamFetchDesc datumStreamFetchDesc = aocsFetchDesc->datumStreamFetchDesc[colno];

		if (datumStreamFetchDesc != NULL)
		{
			Assert(datumStreamFetchDesc->datumStream != NULL);
			datumstreamread_close_file(datumStreamFetchDesc->datumStream);
			destroy_datumstreamread(datumStreamFetchDesc->datumStream);
			datumStreamFetchDesc->datumStream = NULL;
			pfree(datumStreamFetchDesc);
			aocsFetchDesc->datumStreamFetchDesc[colno] = NULL;
		}
	}
	pfree(aocsFetchDesc->datumStreamFetchDesc);

	AppendOnlyBlockDirectory_End_forSearch(&aocsFetchDesc->blockDirectory);

	if (aocsFetchDesc->segmentFileInfo)
	{
		FreeAllAOCSSegFileInfo(aocsFetchDesc->segmentFileInfo, aocsFetchDesc->totalSegfiles);
		pfree(aocsFetchDesc->segmentFileInfo);
		aocsFetchDesc->segmentFileInfo = NULL;
	}

	RelationDecrementReferenceCount(aocsFetchDesc->relation);

	pfree(aocsFetchDesc->segmentFileName);
	pfree(aocsFetchDesc->basepath);

	AppendOnlyVisimap_Finish(&aocsFetchDesc->visibilityMap, AccessShareLock);
}



/*
 * appendonly_delete_init
 *
 * before using appendonly_delete() to delete tuples from append-only segment
 * files, we need to call this function to initialize the delete desc
 * data structured.
 */
AOCSDeleteDesc
aocs_delete_init(Relation rel)
{
	/*
	 * Get the pg_appendonly information
	 */
	Oid visimaprelid;
	AOCSDeleteDesc aoDeleteDesc = palloc0(sizeof(AOCSDeleteDescData));

	aoDeleteDesc->aod_rel = rel;

    Snapshot snapshot = GetCatalogSnapshot(InvalidOid);

    GetAppendOnlyEntryAuxOids(rel,
                              NULL, NULL,
                              &visimaprelid);

	AppendOnlyVisimap_Init(&aoDeleteDesc->visibilityMap,
						   visimaprelid,
						   RowExclusiveLock,
						   snapshot);

	AppendOnlyVisimapDelete_Init(&aoDeleteDesc->visiMapDelete,
								 &aoDeleteDesc->visibilityMap);

	return aoDeleteDesc;
}

void
aocs_delete_finish(AOCSDeleteDesc aoDeleteDesc)
{
	Assert(aoDeleteDesc);

	AppendOnlyVisimapDelete_Finish(&aoDeleteDesc->visiMapDelete);
	AppendOnlyVisimap_Finish(&aoDeleteDesc->visibilityMap, NoLock);

	pfree(aoDeleteDesc);
}

TM_Result
aocs_delete(AOCSDeleteDesc aoDeleteDesc,
			AOTupleId *aoTupleId)
{
	Assert(aoDeleteDesc);
	Assert(aoTupleId);

	elogif(Debug_appendonly_print_delete, LOG,
		   "AOCS delete tuple from table '%s' (AOTupleId %s)",
		   NameStr(aoDeleteDesc->aod_rel->rd_rel->relname),
		   AOTupleIdToString(aoTupleId));

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   "appendonly_delete",
								   DDLNotSpecified,
								   "",	/* databaseName */
								   RelationGetRelationName(aoDeleteDesc->aod_rel)); /* tableName */
#endif

	return AppendOnlyVisimapDelete_Hide(&aoDeleteDesc->visiMapDelete, aoTupleId);
}

/*
 * Initialize a scan on varblock headers in an AOCS segfile.  The
 * segfile is identified by colno.
 */
AOCSHeaderScanDesc
aocs_begin_headerscan(Relation rel, int colno)
{
	AOCSHeaderScanDesc hdesc;
	AppendOnlyStorageAttributes ao_attr;
	StdRdOptions **opts = RelationGetAttributeOptions(rel);

	Assert(opts[colno]);

    GetAppendOnlyEntryAttributes(rel->rd_id,
                                 NULL,
                                 NULL,
                                 &ao_attr.checksum,
                                 NULL);

	/*
	 * We are concerned with varblock headers only, not their content.
	 * Therefore, don't waste cycles in decompressing the content.
	 */
	ao_attr.compress = false;
	ao_attr.compressType = NULL;
	ao_attr.compressLevel = 0;
	ao_attr.overflowSize = 0;
	hdesc = palloc(sizeof(AOCSHeaderScanDescData));
	AppendOnlyStorageRead_Init(&hdesc->ao_read,
							   NULL, //current memory context
							   opts[colno]->blocksize,
							   RelationGetRelationName(rel),
							   "ALTER TABLE ADD COLUMN scan",
							   &ao_attr);
	hdesc->colno = colno;
	hdesc->relid = RelationGetRelid(rel);

	for (int i = 0; i < RelationGetNumberOfAttributes(rel); i++)
		pfree(opts[i]);
	pfree(opts);

	return hdesc;
}

/*
 * Open AOCS segfile for scanning varblock headers.
 */
void
aocs_headerscan_opensegfile(AOCSHeaderScanDesc hdesc,
							AOCSFileSegInfo *seginfo,
							char *basepath)
{
	AOCSVPInfoEntry *vpe;
	char		fn[MAXPGPATH];
	int32		fileSegNo;

	/* Filenum for the column */
	FileNumber	filenum = GetFilenumForAttribute(hdesc->relid, hdesc->colno + 1);

	/* Close currently open segfile, if any. */
	AppendOnlyStorageRead_CloseFile(&hdesc->ao_read);
	FormatAOSegmentFileName(basepath, seginfo->segno,
							filenum, &fileSegNo, fn);
	Assert(strlen(fn) + 1 <= MAXPGPATH);
	vpe = getAOCSVPEntry(seginfo, hdesc->colno);
	AppendOnlyStorageRead_OpenFile(&hdesc->ao_read, fn, seginfo->formatversion,
								   vpe->eof);
}

bool
aocs_get_nextheader(AOCSHeaderScanDesc hdesc)
{
	if (hdesc->ao_read.current.firstRowNum > 0)
		AppendOnlyStorageRead_SkipCurrentBlock(&hdesc->ao_read);

	return AppendOnlyStorageRead_ReadNextBlock(&hdesc->ao_read);
}

void
aocs_end_headerscan(AOCSHeaderScanDesc hdesc)
{
	AppendOnlyStorageRead_CloseFile(&hdesc->ao_read);
	AppendOnlyStorageRead_FinishSession(&hdesc->ao_read);
	pfree(hdesc);
}

/*
 * Initialize one datum stream per new column for writing.
 */
AOCSAddColumnDesc
aocs_addcol_init(Relation rel,
				 int num_newcols)
{
	char	   *ct;
	int32		clvl;
	int32		blksz;
	AOCSAddColumnDesc desc;
	int			i;
	int			iattr;
	StringInfoData titleBuf;
	bool        checksum;

	desc = palloc(sizeof(AOCSAddColumnDescData));
	desc->num_newcols = num_newcols;
	desc->rel = rel;
	desc->cur_segno = -1;

	/*
	 * Rewrite catalog phase of alter table has updated catalog with info for
	 * new columns, which is available through rel.
	 */
	StdRdOptions **opts = RelationGetAttributeOptions(rel);

	desc->dsw = palloc(sizeof(DatumStreamWrite *) * desc->num_newcols);

    GetAppendOnlyEntryAttributes(rel->rd_id,
                                 NULL,
                                 NULL,
                                 &checksum,
                                 NULL);

	iattr = rel->rd_att->natts - num_newcols;
	for (i = 0; i < num_newcols; ++i, ++iattr)
	{
		Form_pg_attribute attr = TupleDescAttr(rel->rd_att, iattr);

		initStringInfo(&titleBuf);
		appendStringInfo(&titleBuf, "ALTER TABLE ADD COLUMN new segfile");

		Assert(opts[iattr]);
		ct = opts[iattr]->compresstype;
		clvl = opts[iattr]->compresslevel;
		blksz = opts[iattr]->blocksize;
		desc->dsw[i] = create_datumstreamwrite(ct, clvl, checksum, blksz,
											   attr, RelationGetRelationName(rel),
											   titleBuf.data,
											   XLogIsNeeded() && RelationNeedsWAL(rel));
	}

	for (i = 0; i < RelationGetNumberOfAttributes(rel); i++)
		pfree(opts[i]);
	pfree(opts);

	return desc;
}

/*
 * Create new physical segfiles for each newly added column.
 */
void
aocs_addcol_newsegfile(AOCSAddColumnDesc desc,
					   AOCSFileSegInfo *seginfo,
					   char *basepath,
					   RelFileNodeBackend relfilenode)
{
	int32		fileSegNo;
	char		fn[MAXPGPATH];
	int			i;
	Snapshot	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));

	/* Column numbers of newly added columns start from here. */
	AttrNumber	colno = desc->rel->rd_att->natts - desc->num_newcols;

	if (desc->dsw[0]->need_close_file)
	{
		aocs_addcol_closefiles(desc);
		AppendOnlyBlockDirectory_End_addCol(&desc->blockDirectory);
	}
	AppendOnlyBlockDirectory_Init_addCol(&desc->blockDirectory,
										 appendOnlyMetaDataSnapshot,
										 (FileSegInfo *) seginfo,
										 desc->rel,
										 seginfo->segno,
										 desc->num_newcols,
										 true /* isAOCol */ );
	for (i = 0; i < desc->num_newcols; ++i, ++colno)
	{
		int			version;

		/* New filenum for the column */
		FileNumber  filenum = GetFilenumForAttribute(RelationGetRelid(desc->rel), colno + 1);

		/* Always write in the latest format */
		version = AOSegfileFormatVersion_GetLatest();

		FormatAOSegmentFileName(basepath, seginfo->segno, filenum,
								&fileSegNo, fn);
		Assert(strlen(fn) + 1 <= MAXPGPATH);
		datumstreamwrite_open_file(desc->dsw[i], fn,
								   0 /* eof */ , 0 /* eof_uncompressed */ ,
								   &relfilenode, fileSegNo,
								   version);
		desc->dsw[i]->blockFirstRowNum = 1;
	}
	desc->cur_segno = seginfo->segno;
	UnregisterSnapshot(appendOnlyMetaDataSnapshot);
}

void
aocs_addcol_closefiles(AOCSAddColumnDesc desc)
{
	int			i;
	AttrNumber	colno = desc->rel->rd_att->natts - desc->num_newcols;

	for (i = 0; i < desc->num_newcols; ++i)
	{
		datumstreamwrite_block(desc->dsw[i], &desc->blockDirectory, i + colno, true);
		datumstreamwrite_close_file(desc->dsw[i]);
	}
	/* Update pg_aocsseg_* with eof of each segfile we just closed. */
	AOCSFileSegInfoAddVpe(desc->rel, desc->cur_segno, desc,
						  desc->num_newcols, false /* non-empty VPEntry */ );
}

void
aocs_addcol_setfirstrownum(AOCSAddColumnDesc desc, int64 firstRowNum)
{
       int                     i;
       for (i = 0; i < desc->num_newcols; ++i)
       {
               /*
                * Next block's first row number.
                */
               desc->dsw[i]->blockFirstRowNum = firstRowNum;
       }
}


/*
 * Force writing new varblock in each segfile open for insert.
 */
void
aocs_addcol_endblock(AOCSAddColumnDesc desc, int64 firstRowNum)
{
	int			i;
	AttrNumber	colno = desc->rel->rd_att->natts - desc->num_newcols;

	for (i = 0; i < desc->num_newcols; ++i)
	{
		datumstreamwrite_block(desc->dsw[i], &desc->blockDirectory, i + colno, true);

		/*
		 * Next block's first row number.  In this case, the block being ended
		 * has less number of rows than its capacity.
		 */
		desc->dsw[i]->blockFirstRowNum = firstRowNum;
	}
}

/*
 * Insert one new datum for each new column being added.  This is
 * derived from aocs_insert_values().
 */
void
aocs_addcol_insert_datum(AOCSAddColumnDesc desc, Datum *d, bool *isnull)
{
	void	   *toFree1;
	void	   *toFree2;
	Datum		datum;
	int			err;
	int			i;
	int			itemCount;

	/* first column's number */
	AttrNumber	colno = desc->rel->rd_att->natts - desc->num_newcols;

	for (i = 0; i < desc->num_newcols; ++i)
	{
		datum = d[i];
		err = datumstreamwrite_put(desc->dsw[i], datum, isnull[i], &toFree1);
		if (toFree1 != NULL)
		{
			/*
			 * Use the de-toasted and/or de-compressed as datum instead.
			 */
			datum = PointerGetDatum(toFree1);
		}
		if (err < 0)
		{
			/*
			 * We have reached max number of datums that can be accommodated
			 * in current varblock.
			 */
			itemCount = datumstreamwrite_nth(desc->dsw[i]);
			/* write the block up to this one */
			datumstreamwrite_block(desc->dsw[i], &desc->blockDirectory, i + colno, true);
			if (itemCount > 0)
			{
				/* Next block's first row number */
				desc->dsw[i]->blockFirstRowNum += itemCount;
			}

			/* now write this new item to the new block */
			err = datumstreamwrite_put(desc->dsw[i], datum, isnull[i],
									   &toFree2);
			Assert(toFree2 == NULL);
			if (err < 0)
			{
				Assert(!isnull[i]);
				err = datumstreamwrite_lob(desc->dsw[i],
										   datum,
										   &desc->blockDirectory,
										   i + colno,
										   true);
				Assert(err >= 0);

				/*
				 * Have written the block above with column value
				 * corresponding to a row, so now update the first row number
				 * to correctly reflect for next block.
				 */
				desc->dsw[i]->blockFirstRowNum++;
			}
		}
		if (toFree1 != NULL)
			pfree(toFree1);
	}
}

void
aocs_addcol_finish(AOCSAddColumnDesc desc)
{
	int			i;

	aocs_addcol_closefiles(desc);
	AppendOnlyBlockDirectory_End_addCol(&desc->blockDirectory);
	for (i = 0; i < desc->num_newcols; ++i)
		destroy_datumstreamwrite(desc->dsw[i]);
	pfree(desc->dsw);
	desc->dsw = NULL;

	pfree(desc);
}

/*
 * Add empty VPEs (eof=0) to pg_aocsseg_* catalog, corresponding to
 * each new column being added.
 */
void
aocs_addcol_emptyvpe(Relation rel,
					 AOCSFileSegInfo **segInfos, int32 nseg,
					 int num_newcols)
{
	int			i;

	for (i = 0; i < nseg; ++i)
	{
		if (Gp_role == GP_ROLE_DISPATCH || segInfos[i]->total_tupcount == 0)
		{
			/*
			 * On QD, all tuples in pg_aocsseg_* catalog have eof=0. On QE,
			 * tuples with eof=0 may exist in pg_aocsseg_* already, caused by
			 * VACUUM.  We need to add corresponding tuples with eof=0 for
			 * each newly added column on QE.
			 */
			AOCSFileSegInfoAddVpe(rel, segInfos[i]->segno, NULL,
								  num_newcols, true /* empty VPEntry */ );
		}
	}
}
