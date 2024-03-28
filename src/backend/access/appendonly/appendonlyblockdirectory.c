/*-----------------------------------------------------------------------------
 *
 * appendonlyblockdirectory.c
 *    maintain the block directory to blocks in append-only relation files.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonlyblockdirectory.c
 *
 *-----------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "catalog/aoblkdir.h"
#include "catalog/aocatalog.h"
#include "catalog/pg_appendonly.h"
#include "access/heapam.h"
#include "access/genam.h"
#include "parser/parse_oper.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/fmgroids.h"
#include "cdb/cdbappendonlyam.h"
#include "nodes/altertablenodes.h"

int			gp_blockdirectory_entry_min_range = 0;
int			gp_blockdirectory_minipage_size = NUM_MINIPAGE_ENTRIES;

static void load_last_minipage(
				   AppendOnlyBlockDirectory *blockDirectory,
				   int64 lastSequence,
				   int columnGroupNo);
static void init_scankeys(
			  TupleDesc tupleDesc,
			  int nkeys, ScanKey scanKeys,
			  StrategyNumber *strategyNumbers);
static int find_minipage_entry(
					MinipagePerColumnGroup *minipageInfo,
					int64 rowNum);
static void extract_minipage(
				 AppendOnlyBlockDirectory *blockDirectory,
				 HeapTuple tuple,
				 TupleDesc tupleDesc,
				 int columnGroupNo);
static void write_minipage(AppendOnlyBlockDirectory *blockDirectory,
			   int columnGroupNo,
			   MinipagePerColumnGroup *minipageInfo);
static bool
insert_new_entry(AppendOnlyBlockDirectory *blockDirectory,
				 int columnGroupNo,
				 int64 firstRowNum,
				 int64 fileOffset,
				 int64 rowCount);
static void clear_minipage(MinipagePerColumnGroup *minipagePerColumnGroup);

static int findFileSegInfo(AppendOnlyBlockDirectory *blockDirectory,
						   int segmentFileNum);
void
AppendOnlyBlockDirectoryEntry_GetBeginRange(
											AppendOnlyBlockDirectoryEntry *directoryEntry,
											int64 *fileOffset,
											int64 *firstRowNum)
{
	*fileOffset = directoryEntry->range.fileOffset;
	*firstRowNum = directoryEntry->range.firstRowNum;
}

void
AppendOnlyBlockDirectoryEntry_GetEndRange(
										  AppendOnlyBlockDirectoryEntry *directoryEntry,
										  int64 *afterFileOffset,
										  int64 *lastRowNum)
{
	*afterFileOffset = directoryEntry->range.afterFileOffset;
	*lastRowNum = directoryEntry->range.lastRowNum;
}

bool
AppendOnlyBlockDirectoryEntry_RangeHasRow(
										  AppendOnlyBlockDirectoryEntry *directoryEntry,
										  int64 checkRowNum)
{
	return (checkRowNum >= directoryEntry->range.firstRowNum &&
			checkRowNum <= directoryEntry->range.lastRowNum);
}

/*
 * init_internal
 *
 * Initialize the block directory structure.
 */
static void
init_internal(AppendOnlyBlockDirectory *blockDirectory)
{
	MemoryContext oldcxt;
	int			numScanKeys;
	TupleDesc	heapTupleDesc;
	TupleDesc	idxTupleDesc;
	int			groupNo;

	Assert(blockDirectory->blkdirRel != NULL);
	Assert(blockDirectory->blkdirIdx != NULL);

	blockDirectory->memoryContext =
		AllocSetContextCreate(CurrentMemoryContext,
							  "BlockDirectoryContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	oldcxt = MemoryContextSwitchTo(blockDirectory->memoryContext);

	heapTupleDesc = RelationGetDescr(blockDirectory->blkdirRel);
	blockDirectory->values = palloc0(sizeof(Datum) * heapTupleDesc->natts);
	blockDirectory->nulls = palloc0(sizeof(bool) * heapTupleDesc->natts);
	blockDirectory->numScanKeys = 3;
	numScanKeys = blockDirectory->numScanKeys;
	blockDirectory->scanKeys = palloc0(numScanKeys * sizeof(ScanKeyData));

	blockDirectory->strategyNumbers = palloc0(numScanKeys * sizeof(StrategyNumber));
	blockDirectory->strategyNumbers[0] = BTEqualStrategyNumber;
	blockDirectory->strategyNumbers[1] = BTEqualStrategyNumber;
	blockDirectory->strategyNumbers[2] = BTLessEqualStrategyNumber;

	idxTupleDesc = RelationGetDescr(blockDirectory->blkdirIdx);

	init_scankeys(idxTupleDesc, numScanKeys,
				  blockDirectory->scanKeys,
				  blockDirectory->strategyNumbers);

	/* Initialize the last minipage */
	blockDirectory->minipages =
		palloc0(sizeof(MinipagePerColumnGroup) * blockDirectory->numColumnGroups);
	for (int i = 0; i < blockDirectory->num_proj_atts; i++)
	{
		groupNo = blockDirectory->proj_atts[i];
		MinipagePerColumnGroup *minipageInfo =
							&blockDirectory->minipages[groupNo];

		minipageInfo->minipage =
			palloc0(minipage_size(NUM_MINIPAGE_ENTRIES));
		minipageInfo->numMinipageEntries = 0;
		ItemPointerSetInvalid(&minipageInfo->tupleTid);
		minipageInfo->cached_entry_no = InvalidEntryNum;
	}

	MemoryContextSwitchTo(oldcxt);
}

/*
 * init_internal_proj
 *
 * Initialize the projected column number array using the projection bool array.
 * If there is no projection, still initialize a number array that contains every
 * column in the table.
 *
 * If required, also initialize the "anchor column" in a CO table which is a 
 * column which we always fetch first.
 */
static void
init_internal_proj(AppendOnlyBlockDirectory *blockDirectory, bool *proj, bool use_anchor_column)
{
	AOCSFileSegInfo 	**segInfos;
	int32 			nseg;
	Snapshot 		snapshot;

	blockDirectory->proj_atts = (AttrNumber *) palloc0(blockDirectory->numColumnGroups * sizeof(AttrNumber));
	blockDirectory->num_proj_atts = 0;

	/* initialize the projection array */
	for (int colno = 0; colno < blockDirectory->numColumnGroups; colno++)
	{
		if (!proj || proj[colno])
			blockDirectory->proj_atts[blockDirectory->num_proj_atts++] = colno;
	}

	/* initialize the anchor column if needed */
	if (use_anchor_column)
	{
		int 		anchor_colno;

		/* anchor column is only for a CO table */
		Assert(blockDirectory->isAOCol);

		/*
		 * We can't use appendOnlyMetaDataSnapshot as that isn't set up in certain paths (like
		 * for unique checks). So, use a catalog snapshot instead to look up the column.
		 */
		snapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
		segInfos = GetAllAOCSFileSegInfo(blockDirectory->aoRel, snapshot, &nseg, NULL);

		anchor_colno = get_anchor_col(segInfos,
													nseg,
													blockDirectory->aoRel->rd_att->natts,
													blockDirectory->aoRel,
													blockDirectory->proj_atts,
													blockDirectory->num_proj_atts);
		UnregisterSnapshot(snapshot);

		blockDirectory->num_proj_atts = aoco_proj_move_anchor_first(blockDirectory->proj_atts,
													blockDirectory->num_proj_atts,
													anchor_colno);
	}
}

/*
 * AppendOnlyBlockDirectory_Init_forSearch
 *
 * Initialize the block directory to handle the lookup.
 *
 * If the block directory relation for this appendonly relation
 * does not exist before calling this function, set blkdirRel
 * and blkdirIdx to NULL, and return.
 */
void
AppendOnlyBlockDirectory_Init_forSearch(
										AppendOnlyBlockDirectory *blockDirectory,
										Snapshot appendOnlyMetaDataSnapshot,
										FileSegInfo **segmentFileInfo,
										int totalSegfiles,
										Relation aoRel,
										int numColumnGroups,
										bool isAOCol,
										bool *proj)
{
	Oid blkdirrelid;
	Oid blkdiridxid;

	blockDirectory->aoRel = aoRel;
	GetAppendOnlyEntryAuxOids(aoRel, NULL, &blkdirrelid, NULL);

	if (!OidIsValid(blkdirrelid))
	{
		blockDirectory->blkdirRel = NULL;
		blockDirectory->blkdirIdx = NULL;

		return;
	}

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory init for search: "
					  "(totalSegfiles, numColumnGroups, isAOCol)="
					  "(%d, %d, %d)",
					  totalSegfiles, numColumnGroups, isAOCol)));

	blockDirectory->segmentFileInfo = segmentFileInfo;
	blockDirectory->totalSegfiles = totalSegfiles;
	blockDirectory->aoRel = aoRel;
	blockDirectory->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;
	blockDirectory->numColumnGroups = numColumnGroups;
	blockDirectory->isAOCol = isAOCol;
	blockDirectory->currentSegmentFileNum = -1;

	Assert(OidIsValid(blkdirrelid));

	blockDirectory->blkdirRel =
		heap_open(blkdirrelid, AccessShareLock);

	blkdiridxid = AppendonlyGetAuxIndex(blockDirectory->blkdirRel);
	Assert(OidIsValid(blkdiridxid));

	blockDirectory->blkdirIdx =
		index_open(blkdiridxid, AccessShareLock);

	init_internal_proj(blockDirectory, proj, isAOCol);

	init_internal(blockDirectory);
}

/*
 * AppendOnlyBlockDirectory_Init_forUniqueChecks
 *
 * Initializes the block directory to handle lookups for uniqueness checks.
 *
 * Note: These lookups will be purely restricted to the block directory relation
 * itself and will not involve the physical AO relation.
 *
 * Note: we defer setting up the appendOnlyMetaDataSnapshot for the block
 * directory to the index_unique_check() table AM call. This is because
 * snapshots used for unique index lookups are special and don't follow the
 * usual allocation or registration mechanism. They may be stack-allocated and a
 * new snapshot object may be passed to every unique index check (this happens
 * when SNAPSHOT_DIRTY is passed). While technically, we could set up the
 * metadata snapshot in advance for SNAPSHOT_SELF, the alternative is fine.
 */
void
AppendOnlyBlockDirectory_Init_forUniqueChecks(
											  AppendOnlyBlockDirectory *blockDirectory,
											  Relation aoRel,
											  int numColumnGroups,
											  Snapshot snapshot)
{
	Oid blkdirrelid;
	Oid blkdiridxid;

	Assert(RelationIsValid(aoRel));

	Assert(snapshot->snapshot_type == SNAPSHOT_DIRTY ||
			snapshot->snapshot_type == SNAPSHOT_SELF);

	GetAppendOnlyEntryAuxOids(aoRel,
							  NULL, &blkdirrelid, NULL);

	if (!OidIsValid(blkdirrelid))
		elog(ERROR, "Could not find block directory for relation: %u", aoRel->rd_id);

	blockDirectory->aoRel = aoRel;
	blockDirectory->isAOCol = RelationIsAoCols(aoRel);

	/* Segfile setup is not necessary as physical AO tuples will not be accessed */
	blockDirectory->segmentFileInfo = NULL;
	blockDirectory->totalSegfiles = -1;
	blockDirectory->currentSegmentFileNum = -1;

	/* Metadata snapshot assignment is deferred to lookup-time */
	blockDirectory->appendOnlyMetaDataSnapshot = InvalidSnapshot;

	blockDirectory->numColumnGroups = numColumnGroups;

	blockDirectory->blkdirRel = heap_open(blkdirrelid, AccessShareLock);

	blkdiridxid = AppendonlyGetAuxIndex(blockDirectory->blkdirRel);
	Assert(OidIsValid(blkdiridxid));

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory init for unique checks"),
			   errdetail("(aoRel = %u, blkdirrel = %u, blkdiridxrel = %u, numColumnGroups = %d)",
						 aoRel->rd_id, blkdirrelid, blkdiridxid, numColumnGroups)));

	blockDirectory->blkdirIdx = index_open(blkdiridxid, AccessShareLock);

	init_internal_proj(blockDirectory, NULL, blockDirectory->isAOCol);

	init_internal(blockDirectory);
}

/*
 * AppendOnlyBlockDirectory_Init_forIndexOnlyScan
 *
 * Initializes the block directory to handle lookups for index-only scan.
 *
 * Note: These lookups will be purely restricted to the block directory relation
 * itself and will not involve the physical AO relation.
 * 
 * Note: the input snapshot should be an MVCC snapshot.
 */
void
AppendOnlyBlockDirectory_Init_forIndexOnlyScan(
											   AppendOnlyBlockDirectory *blockDirectory,
											   Relation aoRel,
											   int numColumnGroups,
											   Snapshot snapshot)
{
	Oid blkdirrelid;
	Oid blkdiridxid;

	Assert(RelationIsValid(aoRel));

	Assert(IsMVCCSnapshot(snapshot));

	GetAppendOnlyEntryAuxOids(aoRel,
							  NULL, &blkdirrelid, NULL);

	if (!OidIsValid(blkdirrelid))
		elog(ERROR, "Could not find block directory for relation: %u", aoRel->rd_id);

	blockDirectory->aoRel = aoRel;
	blockDirectory->isAOCol = RelationIsAoCols(aoRel);

	/* Segfile setup is not necessary as physical AO tuples will not be accessed */
	blockDirectory->segmentFileInfo = NULL;
	blockDirectory->totalSegfiles = -1;
	blockDirectory->currentSegmentFileNum = -1;

	blockDirectory->appendOnlyMetaDataSnapshot = snapshot;

	blockDirectory->numColumnGroups = numColumnGroups;

	blockDirectory->blkdirRel = heap_open(blkdirrelid, AccessShareLock);

	blkdiridxid = AppendonlyGetAuxIndex(blockDirectory->blkdirRel);
	Assert(OidIsValid(blkdiridxid));

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory init for index-only scan"),
			   errdetail("(aoRel = %u, blkdirrel = %u, blkdiridxrel = %u, numColumnGroups = %d)",
						 aoRel->rd_id, blkdirrelid, blkdiridxid, numColumnGroups)));

	blockDirectory->blkdirIdx = index_open(blkdiridxid, AccessShareLock);

	init_internal_proj(blockDirectory, NULL, blockDirectory->isAOCol);

	init_internal(blockDirectory);
}

/*
 * AppendOnlyBlockDirectory_Init_forInsert
 *
 * Initialize the block directory to handle the inserts.
 *
 * If the block directory relation for this appendonly relation
 * does not exist before calling this function, set blkdirRel
 * and blkdirIdx to NULL, and return.
 */
void
AppendOnlyBlockDirectory_Init_forInsert(
										AppendOnlyBlockDirectory *blockDirectory,
										Snapshot appendOnlyMetaDataSnapshot,
										FileSegInfo *segmentFileInfo,
										int64 lastSequence,
										Relation aoRel,
										int segno,
										int numColumnGroups,
										bool isAOCol)
{
	int			groupNo;
	Oid blkdirrelid;
	Oid blkdiridxid;

	blockDirectory->aoRel = aoRel;
	blockDirectory->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;

	GetAppendOnlyEntryAuxOids(aoRel, NULL, &blkdirrelid, NULL);

	if (!OidIsValid(blkdirrelid))
	{
		blockDirectory->blkdirRel = NULL;
		blockDirectory->blkdirIdx = NULL;

		return;
	}

	blockDirectory->segmentFileInfo = NULL;
	blockDirectory->totalSegfiles = -1;
	blockDirectory->currentSegmentFileInfo = segmentFileInfo;

	blockDirectory->currentSegmentFileNum = segno;
	blockDirectory->numColumnGroups = numColumnGroups;
	blockDirectory->isAOCol = isAOCol;

	Assert(OidIsValid(blkdirrelid));

	blockDirectory->blkdirRel =
		heap_open(blkdirrelid, RowExclusiveLock);

	blkdiridxid = AppendonlyGetAuxIndex(blockDirectory->blkdirRel);
	Assert(OidIsValid(blkdiridxid));

	blockDirectory->blkdirIdx =
		index_open(blkdiridxid, RowExclusiveLock);

	blockDirectory->indinfo = CatalogOpenIndexes(blockDirectory->blkdirRel);

	init_internal_proj(blockDirectory, NULL, false);

	init_internal(blockDirectory);

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory init for insert: "
					  "(segno, numColumnGroups, isAOCol, lastSequence)="
					  "(%d, %d, %d, " INT64_FORMAT ")",
					  segno, numColumnGroups, isAOCol, lastSequence)));

	/*
	 * Load the last minipages from the block directory relation.
	 */
	for (groupNo = 0; groupNo < blockDirectory->numColumnGroups; groupNo++)
	{
		load_last_minipage(blockDirectory, lastSequence, groupNo);
	}
}

/*
 * Open block directory relation, initialize scan keys and minipages
 * for COLUMN WRITE operation used in
 * ALTER TABLE ADD COLUMN or ALTER COLUMN TYPE optimizations.
 */
void
AppendOnlyBlockDirectory_Init_writeCols(
									 AppendOnlyBlockDirectory *blockDirectory,
									 Snapshot appendOnlyMetaDataSnapshot,
									 FileSegInfo *segmentFileInfo,
									 Relation aoRel,
									 int segno,
									 int numColumnGroups,
									 bool isAOCol)
{
	Oid blkdirrelid;
	Oid blkdiridxid;

	blockDirectory->aoRel = aoRel;
	blockDirectory->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;

	GetAppendOnlyEntryAuxOids(aoRel, NULL, &blkdirrelid, NULL);

	if (!OidIsValid(blkdirrelid))
	{
		blockDirectory->blkdirRel = NULL;
		blockDirectory->blkdirIdx = NULL;
		blockDirectory->numColumnGroups = 0;
		return;
	}

	blockDirectory->segmentFileInfo = NULL;
	blockDirectory->totalSegfiles = -1;
	blockDirectory->currentSegmentFileInfo = segmentFileInfo;

	blockDirectory->currentSegmentFileNum = segno;
	blockDirectory->numColumnGroups = numColumnGroups;
	blockDirectory->isAOCol = isAOCol;

	Assert(OidIsValid(blkdirrelid));

	/*
	 * TODO: refactor the *_addCol* interface so that opening of
	 * blockdirectory relation and index, init_internal and corresponding
	 * cleanup in *_End_addCol() is called only once during the add-column
	 * operation.  Currently, this is being called for every appendonly
	 * segment.
	 */
	blockDirectory->blkdirRel =
		heap_open(blkdirrelid, RowExclusiveLock);

	blkdiridxid = AppendonlyGetAuxIndex(blockDirectory->blkdirRel);
	Assert(OidIsValid(blkdiridxid));

	blockDirectory->blkdirIdx =
		index_open(blkdiridxid, RowExclusiveLock);

	blockDirectory->indinfo = CatalogOpenIndexes(blockDirectory->blkdirRel);

	init_internal_proj(blockDirectory, NULL, false);

	init_internal(blockDirectory);
}

static bool
set_directoryentry_range(
						 AppendOnlyBlockDirectory *blockDirectory,
						 int columnGroupNo,
						 int entry_no,
						 AppendOnlyBlockDirectoryEntry *directoryEntry)
{
	MinipagePerColumnGroup *minipageInfo =
	&blockDirectory->minipages[columnGroupNo];
	FileSegInfo *fsInfo;
	AOCSFileSegInfo *aocsFsInfo = NULL;
	MinipageEntry *entry;
	MinipageEntry *next_entry = NULL;

	Assert(entry_no >= 0 && ((uint32) entry_no) < minipageInfo->numMinipageEntries);

	fsInfo = blockDirectory->currentSegmentFileInfo;
	Assert(fsInfo != NULL);

	if (blockDirectory->isAOCol)
	{
		aocsFsInfo = (AOCSFileSegInfo *) fsInfo;
	}

	entry = &(minipageInfo->minipage->entry[entry_no]);
	if (((uint32) entry_no) < minipageInfo->numMinipageEntries - 1)
	{
		next_entry = &(minipageInfo->minipage->entry[entry_no + 1]);
	}

	directoryEntry->range.fileOffset = entry->fileOffset;
	directoryEntry->range.firstRowNum = entry->firstRowNum;
	if (next_entry != NULL)
	{
		directoryEntry->range.afterFileOffset = next_entry->fileOffset;
	}
	else
	{
		if (!blockDirectory->isAOCol)
		{
			directoryEntry->range.afterFileOffset = fsInfo->eof;
		}

		else
		{
			directoryEntry->range.afterFileOffset =
				aocsFsInfo->vpinfo.entry[columnGroupNo].eof;
		}
	}

	directoryEntry->range.lastRowNum = entry->firstRowNum + entry->rowCount - 1;
	if (next_entry == NULL && gp_blockdirectory_entry_min_range != 0)
	{
		directoryEntry->range.lastRowNum = (~(((int64) 1) << 63));	/* set to the maximal
																	 * value */
	}

	/*
	 * When crashes during inserts, or cancellation during inserts, the block
	 * directory may contain out-of-date entries. We check for the end of file
	 * here. If the requested directory entry is after the end of file, return
	 * false.
	 */
	if ((!blockDirectory->isAOCol &&
		 directoryEntry->range.fileOffset > fsInfo->eof) ||
		(blockDirectory->isAOCol &&
		 directoryEntry->range.fileOffset >
		 aocsFsInfo->vpinfo.entry[columnGroupNo].eof))
		return false;

	if ((!blockDirectory->isAOCol &&
		 directoryEntry->range.afterFileOffset > fsInfo->eof))
	{
		directoryEntry->range.afterFileOffset = fsInfo->eof;
	}

	if (blockDirectory->isAOCol &&
		directoryEntry->range.afterFileOffset >
		aocsFsInfo->vpinfo.entry[columnGroupNo].eof)
	{
		directoryEntry->range.afterFileOffset =
			aocsFsInfo->vpinfo.entry[columnGroupNo].eof;
	}

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory find entry: "
					  "(columnGroupNo, firstRowNum, fileOffset, lastRowNum, afterFileOffset) = "
					  "(%d, " INT64_FORMAT ", " INT64_FORMAT ", " INT64_FORMAT ", " INT64_FORMAT ")",
					  columnGroupNo, directoryEntry->range.firstRowNum,
					  directoryEntry->range.fileOffset, directoryEntry->range.lastRowNum,
					  directoryEntry->range.afterFileOffset)));

	return true;
}

/*
 * AppendOnlyBlockDirectory_GetEntry
 *
 * Find a directory entry for the given AOTupleId in the block directory.
 * If such an entry is found, return true. Otherwise, return false.
 *
 * The range for directoryEntry is assigned accordingly in this function.
 *
 * The block directory for the appendonly table should exist before calling
 * this function.
 */
bool
AppendOnlyBlockDirectory_GetEntry(
								  AppendOnlyBlockDirectory *blockDirectory,
								  AOTupleId *aoTupleId,
								  int columnGroupNo,
								  AppendOnlyBlockDirectoryEntry *directoryEntry,
								  int64 *attnum_to_rownum)
{
	int			segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	int64		rowNum = AOTupleIdGet_rowNum(aoTupleId);
	Relation	blkdirRel = blockDirectory->blkdirRel;
	Relation	blkdirIdx = blockDirectory->blkdirIdx;
	int			numScanKeys = blockDirectory->numScanKeys;
	ScanKey		scanKeys = blockDirectory->scanKeys;

	TupleDesc	heapTupleDesc;
	int 		fsInfoIdx;
	FileSegInfo *fsInfo = NULL;
	SysScanDesc idxScanDesc;
	HeapTuple	tuple = NULL;
	MinipagePerColumnGroup *minipageInfo =
	&blockDirectory->minipages[columnGroupNo];
	int			entry_no = InvalidEntryNum;
	int			tmpGroupNo;

	if (blkdirRel == NULL || blkdirIdx == NULL)
	{
		Assert(RelationIsValid(blockDirectory->aoRel));

		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("block directory for append-only relation '%s' does not exist",
						RelationGetRelationName(blockDirectory->aoRel))));
		return false;
	}

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory get entry: "
					  "(columnGroupNo, segmentFileNum, rowNum) = "
					  "(%d, %d, " INT64_FORMAT ")",
					  columnGroupNo, segmentFileNum, rowNum)));

	/*
	 * If the segment file number is the same as
	 * blockDirectory->currentSegmentFileNum, the in-memory minipage may
	 * contain such an entry. We search the in-memory minipage first. If such
	 * an entry can not be found, we search for the appropriate minipage by
	 * using the block directory btree index.
	 */
	if (segmentFileNum == blockDirectory->currentSegmentFileNum &&
		minipageInfo->numMinipageEntries > 0)
	{
		Assert(blockDirectory->currentSegmentFileInfo != NULL);

		MinipageEntry *firstentry =
		&minipageInfo->minipage->entry[0];

		if (rowNum >= firstentry->firstRowNum)
		{
			/*
			 * Check if the existing minipage contains the requested rowNum.
			 * If so, just get it.
			 */
			entry_no = find_minipage_entry(minipageInfo, rowNum);
			if (entry_no != InvalidEntryNum)
			{
				return set_directoryentry_range(blockDirectory,
												columnGroupNo,
												entry_no,
												directoryEntry);

			}

			/*
			 * The given rowNum may point to a tuple that does not exist in
			 * the AO table any more, either because of cancellation of an
			 * insert, or due to crashes during an insert. If this is the
			 * case, rowNum may be smaller than the highest entry in the
			 * in-memory minipage (rowNum has fallen in a hole between two
			 * minipage entries in the in-memory minipage). If it is smaller,
			 * we can safely conclude that it doesn't exist, and return early.
			 */
			else
			{
				MinipageEntry *entry =
				&minipageInfo->minipage->entry[minipageInfo->numMinipageEntries - 1];

				if (rowNum < entry->firstRowNum + entry->rowCount - 1)
				{
					SIMPLE_FAULT_INJECTOR("AppendOnlyBlockDirectory_GetEntry_inter_entry_hole");
					return false;
				}
			}
		}
	}

	fsInfoIdx = findFileSegInfo(blockDirectory, segmentFileNum);
	fsInfo = blockDirectory->segmentFileInfo[fsInfoIdx];

	Assert(fsInfo != NULL);

	/*
	 * Search the btree index to find the minipage that contains the rowNum.
	 * We find the minipages for all column groups, since currently we will
	 * need to access all columns at the same time.
	 */
	heapTupleDesc = RelationGetDescr(blkdirRel);

	Assert(numScanKeys == 3);

	for (int i = 0; i < blockDirectory->num_proj_atts; i++)
	{
		tmpGroupNo = blockDirectory->proj_atts[i];
		if (attnum_to_rownum && AO_ATTR_VAL_IS_MISSING(rowNum, tmpGroupNo, segmentFileNum, attnum_to_rownum))
		{
			/*
			 * Ignore if the value doesn't exist in the column file of the
			 * CO table, since there won't be a blkdir entry for that. 
			 * This should be only relevant to CO tables.
			 */
			Assert(blockDirectory->isAOCol);
			continue;
		}

		/*
		 * Set up the scan keys values. The keys have already been set up in
		 * init_internal() with the following strategy:
		 * (=segmentFileNum, =columnGroupNo, <=rowNum)
		 * See init_internal().
		 */
		Assert(scanKeys != NULL);
		scanKeys[0].sk_argument = Int32GetDatum(segmentFileNum);
		scanKeys[1].sk_argument = Int32GetDatum(tmpGroupNo);
		scanKeys[2].sk_argument = Int64GetDatum(rowNum);

		idxScanDesc = systable_beginscan_ordered(blkdirRel, blkdirIdx,
												 blockDirectory->appendOnlyMetaDataSnapshot,
												 numScanKeys, scanKeys);

		tuple = systable_getnext_ordered(idxScanDesc, BackwardScanDirection);

		SIMPLE_FAULT_INJECTOR("AppendOnlyBlockDirectory_GetEntry_sysscan");

		if (tuple != NULL)
		{
			/*
			 * MPP-17061: we need to update currentSegmentFileNum &
			 * currentSegmentFileInfo at the same time when we load the
			 * minipage for the block directory entry we found, otherwise we
			 * would risk having inconsistency between
			 * currentSegmentFileNum/currentSegmentFileInfo and minipage
			 * contents, which would cause wrong block header offset being
			 * returned in following block directory entry look up.
			 */
			blockDirectory->currentSegmentFileNum = segmentFileNum;
			blockDirectory->currentSegmentFileInfo = fsInfo;

			extract_minipage(blockDirectory,
							 tuple,
							 heapTupleDesc,
							 tmpGroupNo);
		}
		else
		{
			/* MPP-17061: index look up failed, row is invisible */
			systable_endscan_ordered(idxScanDesc);
			return false;
		}

		systable_endscan_ordered(idxScanDesc);
	}

	{
		MinipagePerColumnGroup *minipageInfo;

		minipageInfo = &blockDirectory->minipages[columnGroupNo];

		/*
		 * Perform a binary search over the minipage to find the entry about
		 * the AO block.
		 */
		entry_no = find_minipage_entry(minipageInfo, rowNum);

		/* If there are no entries, return false. */
		if (entry_no == -1 && minipageInfo->numMinipageEntries == 0)
			return false;

		if (entry_no == -1)
		{
			/*
			 * Since the last few blocks may not be logged in the block
			 * directory, we always use the last entry.
			 *
			 * FIXME: If we didn't find a suitable entry, why even use the last
			 * entry? Currently, as it stands we would most likely return
			 * true from this function. This will lead to us having to do a
			 * fetch of the tuple from the physical file in the layer above (see
			 * scanToFetchTuple()), where we would ultimately find the tuple
			 * missing. Would it be correct to set the directory entry here to
			 * be the last one (for caching purposes) and return false, in order
			 * to avoid this physical file read?
			 */
			entry_no = minipageInfo->numMinipageEntries - 1;
		}
		return set_directoryentry_range(blockDirectory,
										columnGroupNo,
										entry_no,
										directoryEntry);
	}

	return false;
}

/*
 * Using the block directory, figure out the (segfile, offset) from which we
 * should start scanning, given the first tuple we want scanned is from the
 * logical heap block 'blkno'.
 *
 * To support ao_column tables, we also have the 'columnGroupNo' argument to get
 * the coordinates to start the scan for a particular column (for ao_row tables,
 * this is always trivially 0).
 *
 * When the routine returns, 'fsInfoIdx' and 'dirEntry' capture the
 * (segfile, offset) information that we need.
 *
 * We perform a sysscan of the block directory to find the block directory row
 * that encompasses the first possible row in 'blkno' (we call it
 * rangeFirstRowNum). Thereafter, like AppendOnlyBlockDirectory_GetEntry(), we
 * extract the minipage from that block directory row, and search for the entry.
 *
 * Note: There may be holes into which rangeFirstRowNum may fall. If the hole is
 * at the beginning of the segfile, we return false. If the hole is in between
 * two entries, we return the second one (as the first one won't have the
 * rangeFirstRowNum and the second one may contain rows from our desired range).
 * If the hole is at the end of the minipage, we have no choice but to return
 * the last entry in the minipage as a safe start point for the scan (returning
 * the first entry of the "next" minipage is non-trivial). This will eventually
 * lead us to scan some tuples outside our range and discarding them.
 *
 * Note: We could return a minipage entry whose fileOffset is beyond the eof of
 * the file, in case the entry is out of sync with the seginfo. We will handle
 * that later, while positioning the scan. We return true in this case.
 */

bool
AppendOnlyBlockDirectory_GetEntryForPartialScan(AppendOnlyBlockDirectory *blockDirectory,
												BlockNumber blkno,
												int columnGroupNo,
												AppendOnlyBlockDirectoryEntry *dirEntry,
												int *fsInfoIdx)
{
	int			segmentFileNum;
	uint64 		rangeFirstRowNum;
	Relation	blkdirRel;
	Relation	blkdirIdx;
	ScanKey 	scanKeys;
	SysScanDesc idxScanDesc;
	HeapTuple	tuple = NULL;
	FileSegInfo *fsInfo = NULL;
	TupleDesc 	tupleDesc;

	Assert(blockDirectory);
	Assert(RelationIsValid(blockDirectory->blkdirRel));
	Assert(RelationIsValid(blockDirectory->blkdirIdx));
	Assert(blkno != InvalidBlockNumber);
	Assert(columnGroupNo >= 0 && columnGroupNo < blockDirectory->numColumnGroups);
	Assert(dirEntry);

	blkdirRel = blockDirectory->blkdirRel;
	blkdirIdx = blockDirectory->blkdirIdx;
	segmentFileNum = AOSegmentGet_segno(blkno);
	rangeFirstRowNum = AOHeapBlockGet_startRowNum(blkno);
	scanKeys = blockDirectory->scanKeys;

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory get entry for partial scan: "
					  "(columnGroupNo, segmentFileNum, rangeFirstRowNum) = "
					  "(%d, %d, " INT64_FORMAT ")",
				  columnGroupNo, segmentFileNum, rangeFirstRowNum)));

	/*
	 * Set up the scan keys values. The keys have already been set up in
	 * init_internal() with the following strategy:
	 * (=segmentFileNum, =columnGroupNo, <=rowNum)
	 * See init_internal().
	 */
	Assert(scanKeys != NULL);
	scanKeys[0].sk_argument = Int32GetDatum(segmentFileNum);
	scanKeys[1].sk_argument = Int32GetDatum(columnGroupNo);
	scanKeys[2].sk_argument = Int64GetDatum(rangeFirstRowNum);

	*fsInfoIdx = findFileSegInfo(blockDirectory, segmentFileNum);
	fsInfo = blockDirectory->segmentFileInfo[*fsInfoIdx];

	tupleDesc = RelationGetDescr(blkdirRel);
	idxScanDesc = systable_beginscan_ordered(blkdirRel, blkdirIdx,
											 blockDirectory->appendOnlyMetaDataSnapshot,
											 3, scanKeys);
	tuple = systable_getnext_ordered(idxScanDesc, BackwardScanDirection);

	if (!tuple)
	{
		/* rangeFirstRowNum falls in hole before the first minipage entry. */
		systable_endscan_ordered(idxScanDesc);
		return false;
	}
	else
	{
		MinipagePerColumnGroup 	*minipageInfo;
		Minipage 				*minipage;
		int 					entry_no = 0;
		int 					result_entry_no;

		blockDirectory->currentSegmentFileNum = segmentFileNum;
		blockDirectory->currentSegmentFileInfo = fsInfo;

		extract_minipage(blockDirectory,
						 tuple,
						 tupleDesc,
						 columnGroupNo);
		minipageInfo = &blockDirectory->minipages[columnGroupNo];
		minipage = minipageInfo->minipage;

		systable_endscan_ordered(idxScanDesc);

		if (minipageInfo->numMinipageEntries == 0)
			return false;

		/* Find 1st entry with firstRowNum greater than rangeFirstRowNum */
		while (entry_no < minipageInfo->numMinipageEntries &&
				minipage->entry[entry_no].firstRowNum <= rangeFirstRowNum)
			entry_no++;

		Assert(entry_no >= 1);

		if (entry_no < minipageInfo->numMinipageEntries)
		{
			if (rangeFirstRowNum >= minipage->entry[entry_no - 1].firstRowNum &&
				rangeFirstRowNum < minipage->entry[entry_no - 1].firstRowNum +
					minipage->entry[entry_no - 1].rowCount)
			{
				/*
				 * Found the minipage entry encompassing rangeFirstRowNum, we
				 * can start the scan there.
				 */
				result_entry_no = entry_no - 1;
			}
			else
			{
				/*
				 * rangeFirstRowNum into a hole between (entry_no - 1) and
				 * (entry_no). We can start the scan from (entry_no) in this
				 * case. Maybe it will contain subsequent row numbers in our
				 * partial scan range, maybe it won't - it's safe to start there.
				 */
				result_entry_no = entry_no;
			}
		}
		else
		{
			/*
			 * We fell into a hole at the end of the minipage, beyond the last
			 * entry. It is safe to start from the last entry in the minipage.
			 * (Although that will lead to same wasted effort, it's done to keep
			 * things simple here - it's difficult to extract the next minipage
			 * here, without knowing it's coordinates)
			 */
			result_entry_no = entry_no - 1;
		}

		/*
		 * We ignore the return value of set_directoryentry_range(). Even if
		 * the dirEntry's fileOffset points past the eof, it is okay. We will
		 * handle that later.
		 */
		set_directoryentry_range(blockDirectory,
								 columnGroupNo,
								 result_entry_no,
								 dirEntry);
		return true;
	}
}

/*
 * AppendOnlyBlockDirectory_CoversTuple
 *
 * Check if there exists a visible block directory entry that represents a range
 * in which this tid resides.
 *
 * Currently used by index fetches to perform unique constraint validation and
 * for index only scans.
 *
 * The check can be satisfied either by looking at the currently cached minipage
 * or by performing a sysscan of the block directory relation
 * (see blkdir_entry_exists()).
 *
 * For a unique index check, we cannot consult the cache and a fresh per-tuple
 * sysscan must be performed. The sysscan populates the xmin/xmax of the
 * snapshot used to scan, which is a requirement when SNAPSHOT_DIRTY is used
 * (in unique checks). See _bt_check_unique() and SNAPSHOT_DIRTY for details.
 * Similarly, if the snapshot is of type SELF or of type ANY, we would want to
 * bypass the cache in order to see any updated rows.
 *
 * Note about AOCO tables:
 * For AOCO tables, there are multiple block directory entries for each tid.
 * However, it is currently sufficient to check the block directory entry for
 * just one of these columns. We do so for the "anchor column" which is
 * picked using the same logic as regular table scan. Note that if we write a 
 * placeholder row for the anchor column being picked, there is a
 * guarantee that if there is a conflict on the placeholder row, the covering
 * block directory entry will be based on the same column i (as columnar DDL
 * changes need exclusive locks and placeholder rows can't be seen after tx end)
 * (We could just have checked the covers condition for column 0, as block
 * directory entries are inserted even for dropped columns. But, this may change
 * one day, and we want our code to be future-proof)
 */
bool
AppendOnlyBlockDirectory_CoversTuple(
									 AppendOnlyBlockDirectory *blockDirectory,
									 AOTupleId *aoTupleId)
{
	Relation	aoRel = blockDirectory->aoRel;

	Assert(RelationIsValid(aoRel));

	if (RelationIsAoRows(aoRel))
		return blkdir_entry_exists(blockDirectory, aoTupleId, 0);
	else
		return blkdir_entry_exists(blockDirectory,
								   aoTupleId,
								   blockDirectory->proj_atts[ANCHOR_COL_IN_PROJ]);
}

/*
 * Does a visible block directory entry exist for a given aotid and column no?
 * Currently used to satisfy unique constraint checks and index only scans.
 *
 * The snapshot type dictates whether cached minipages in the 'blockDirectory'
 * structure can be consulted - a dirty blkdir snapshot demands a fresh sysscan
 * every time, so that the snapshot's fields are populated as a side effect.
 * SNAPSHOT_SELF or SNAPSHOT_ANY also would mean that we shouldn't consult the
 * cache, in order to see the latest updates.
 */
bool
blkdir_entry_exists(AppendOnlyBlockDirectory *blockDirectory,
					AOTupleId *aoTupleId,
					int columnGroupNo)
{
	int			segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	int64		rowNum = AOTupleIdGet_rowNum(aoTupleId);
	Relation	blkdirRel = blockDirectory->blkdirRel;
	Relation	blkdirIdx = blockDirectory->blkdirIdx;
	ScanKey		scanKeys = blockDirectory->scanKeys;
	HeapTuple	tuple;
	SysScanDesc idxScanDesc;
	bool      found = false;
	TupleDesc blkdirTupleDesc;

	Assert(RelationIsValid(blkdirRel));

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory covers tuple check: "
					  "(columnGroupNo, segmentFileNum, rowNum) = "
					  "(%d, %d, " INT64_FORMAT ")",
				  0, segmentFileNum, rowNum)));

	/*
	 * Check the cached minipage first to see if the row number exists. If not,
	 * proceed to perform a sysscan of the block directory.
	 */
	if (blockDirectory->appendOnlyMetaDataSnapshot && 
		IsMVCCSnapshot(blockDirectory->appendOnlyMetaDataSnapshot) &&
		blockDirectory->currentSegmentFileNum == segmentFileNum)
	{
		MinipagePerColumnGroup *minipageInfo =
			&blockDirectory->minipages[columnGroupNo];

		if (minipageInfo && minipageInfo->minipage)
		{
			int entry_no = find_minipage_entry(minipageInfo, rowNum);
			if (entry_no != InvalidEntryNum)
			{
				ereportif(Debug_appendonly_print_blockdirectory, LOG,
						  (errmsg("Append-only block directory covers tuple check cache hit: "
								  "(columnGroupNo, segmentFileNum, rowNum) = "
								  "(%d, %d, " INT64_FORMAT ")",
							  0, segmentFileNum, rowNum)));
				return true;
			}
		}
	}

	blkdirTupleDesc = RelationGetDescr(blkdirRel);

	/*
	 * Set up the scan keys values. The keys have already been set up in
	 * init_internal() with the following strategy:
	 * (=segmentFileNum, =columnGroupNo, <=rowNum)
	 * See init_internal().
	 */
	Assert(scanKeys != NULL);
	Assert(blockDirectory->numScanKeys == 3);
	scanKeys[0].sk_argument = Int32GetDatum(segmentFileNum);
	scanKeys[1].sk_argument = Int32GetDatum(columnGroupNo);
	scanKeys[2].sk_argument = Int64GetDatum(rowNum);
	idxScanDesc = systable_beginscan_ordered(blkdirRel, blkdirIdx,
											 blockDirectory->appendOnlyMetaDataSnapshot,
											 blockDirectory->numScanKeys,
											 scanKeys);

	/*
	 *
	 * Loop until:
	 *
	 * (1) No rows are returned from the sysscan, as there is no visible row
	 * satisfying the criteria. This is what happens when there is no uniqueness
	 * conflict, when we call this in the context of a uniqueness check. This
	 * can also happen when the tid being looked up from an index only scan is
	 * not visible.
	 *
	 * (2) We find a row such that: rowNum ∈ [firstRowNum, firstRowNum + rowCount)
	 *   (a) The row is a regular block directory row covering the rowNum.
	 *   (b) The row is a placeholder block directory row, inserted by
	 *       AppendOnlyBlockDirectory_InsertPlaceholder(), which will always
	 *       cover the rowNum by virtue of it's rowCount = AOTupleId_MaxRowNum.
	 *       (unique indexes only)
	 */
	while (HeapTupleIsValid(tuple = systable_getnext_ordered(idxScanDesc, BackwardScanDirection)))
	{
		/*
		 * Once we have found a matching row, we must also ensure that we check
		 * for a block directory entry, in this row's minipage, that has a range
		 * that covers the rowNum.
		 *
		 * This is necessary for aborted transactions where the index entry
		 * might still be live. In such a case, since our search criteria lacks
		 * a lastRowNum, we will match rows where:
		 * firstRowNum < lastRowNum < rowNum
		 * Such rows will obviously not cover the rowNum, thus making inspection
		 * of the row's minipage a necessity.
		 */
		MinipagePerColumnGroup *minipageInfo;
		int entry_no;

		BlockNumber blockNumber = ItemPointerGetBlockNumberNoCheck(&tuple->t_self);
		OffsetNumber offsetNumber = ItemPointerGetOffsetNumberNoCheck(&tuple->t_self);
		elogif(Debug_appendonly_print_blockdirectory, LOG,
			   "For segno = %d, rownum = %ld, tid returned: (%u,%u) "
			   "tuple (xmin, xmax) = (%lu, %lu), snaptype = %d",
			   segmentFileNum, rowNum, blockNumber, offsetNumber,
			   (unsigned long) HeapTupleHeaderGetRawXmin(tuple->t_data),
			   (unsigned long) HeapTupleHeaderGetRawXmax(tuple->t_data),
			   blockDirectory->appendOnlyMetaDataSnapshot->snapshot_type);

		blockDirectory->currentSegmentFileNum = segmentFileNum;
		extract_minipage(blockDirectory,
						 tuple,
						 blkdirTupleDesc,
						 columnGroupNo);

		minipageInfo = &blockDirectory->minipages[columnGroupNo];
		entry_no = find_minipage_entry(minipageInfo, rowNum);
		if (entry_no != InvalidEntryNum)
		{
			found = true;
			break;
		}
	}

	systable_endscan_ordered(idxScanDesc);

	return found;
}

/*
 * AppendOnlyBlockDirectory_InsertEntry
 *
 * Insert an entry to the block directory. This entry is appended to the
 * in-memory minipage. If the minipage is full, it is written to the block
 * directory relation on disk. After that, the new entry is added to the
 * new in-memory minipage.
 *
 * To reduce the size of a block directory, this function ignores new entries
 * when the range between the offset value of the latest existing entry and
 * the offset of the new entry is smaller than gp_blockdirectory_entry_min_range
 * (if it is set). Otherwise, the latest existing entry is updated with new
 * rowCount value, and the given new entry is appended to the in-memory minipage.
 *
 * If the block directory for the appendonly relation does not exist,
 * this function simply returns.
 *
 * If rowCount is 0, simple return false.
 */
bool
AppendOnlyBlockDirectory_InsertEntry(AppendOnlyBlockDirectory *blockDirectory,
									 int columnGroupNo,
									 int64 firstRowNum,
									 int64 fileOffset,
									 int64 rowCount)
{
	return insert_new_entry(blockDirectory, columnGroupNo, firstRowNum,
							fileOffset, rowCount);
}

/*
 * AppendOnlyBlockDirectory_DeleteSegmentFile
 *
 * Delete an entry from the block directory for given segment file and columnGroupNo
 * of an append-only relation.
 * If the block directory for the appendonly relation does not exist,
 * this function simply returns.
 */
void
AppendOnlyBlockDirectory_DeleteSegmentFile(AppendOnlyBlockDirectory *blockDirectory,
										   int columnGroupNo,
										   int segno,
										   Snapshot snapshot)
{
	ScanKeyData scanKey[2];
	SysScanDesc indexScan;
	HeapTuple	tuple;
	if (blockDirectory->blkdirRel == NULL)
		return;

	ScanKeyInit(&scanKey[0],
				Anum_pg_aoblkdir_segno,				/* segno */
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(segno));
	ScanKeyInit(&scanKey[1],
				Anum_pg_aoblkdir_columngroupno,/* columnGroupNo */
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(columnGroupNo));

	indexScan = systable_beginscan_ordered(blockDirectory->blkdirRel,
										   blockDirectory->blkdirIdx,
										   snapshot,
										   2 /* nkeys */,
										   scanKey);

	while ((tuple = systable_getnext_ordered(indexScan, ForwardScanDirection)) != NULL)
	{
		CatalogTupleDelete(blockDirectory->blkdirRel, &tuple->t_self);
	}
	systable_endscan_ordered(indexScan);
}

/*
 * Helper method used to insert a new minipage entry in the block
 * directory relation.  Refer to AppendOnlyBlockDirectory_InsertEntry()
 * for more details.
 *
 * 1. Checks if the current minipage is full. If yes, it writes the current
 * minipage to the block directory relation and empty the in-memory area. This
 * could mean a new block directory tuple is inserted OR an old tuple is updated.
 *
 * 2. "Inserts" the new entry in the current in-mem minipage -> just sets the
 * in-memory area with the supplied function args.
 *
 */
static bool
insert_new_entry(AppendOnlyBlockDirectory *blockDirectory,
				 int columnGroupNo,
				 int64 firstRowNum,
				 int64 fileOffset,
				 int64 rowCount)
{
	MinipageEntry *entry = NULL;
	MinipagePerColumnGroup *minipageInfo;

	if (rowCount == 0)
		return false;

	if (blockDirectory->blkdirRel == NULL ||
		blockDirectory->blkdirIdx == NULL)
		return false;

	minipageInfo = &blockDirectory->minipages[columnGroupNo];
	Assert(minipageInfo->numMinipageEntries <= (uint32) NUM_MINIPAGE_ENTRIES);

	/*
	 * Before we insert the new entry into the current minipage, we should
	 * check if the current minipage is full. If so, we write out the current
	 * minipage to the block directory relation and clear out the last minipage
	 * in-mem, making the current in-mem minipage empty and ready to hold the
	 * new entry (and beyond).
	 */
	if (IsMinipageFull(minipageInfo))
	{
		write_minipage(blockDirectory, columnGroupNo, minipageInfo);
		clear_minipage(minipageInfo);
		SIMPLE_FAULT_INJECTOR("insert_new_entry_curr_minipage_full");
	}

	/* Now insert the new entry */
	Assert(minipageInfo->numMinipageEntries < (uint32) gp_blockdirectory_minipage_size);
	entry = &(minipageInfo->minipage->entry[minipageInfo->numMinipageEntries]);
	entry->firstRowNum = firstRowNum;
	entry->fileOffset = fileOffset;
	entry->rowCount = rowCount;

	minipageInfo->numMinipageEntries++;

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory insert entry: "
					  "(firstRowNum, columnGroupNo, fileOffset, rowCount) = (" INT64_FORMAT
					  ", %d, " INT64_FORMAT ", " INT64_FORMAT ") at index %d",
					  entry->firstRowNum, columnGroupNo, entry->fileOffset, entry->rowCount,
					  minipageInfo->numMinipageEntries - 1)));

	return true;
}

/*
 * AppendOnlyBlockDirectory_DeleteSegmentFiles
 *
 * Deletes all block directory entries for given segment file of an
 * append-only relation.
 */
void
AppendOnlyBlockDirectory_DeleteSegmentFiles(Oid blkdirrelid,
											Snapshot snapshot,
											int segno)
{
	Oid blkdiridxid;

	if (!OidIsValid(blkdirrelid))
		return;

	Relation	blkdirRel = table_open(blkdirrelid, RowExclusiveLock);

	blkdiridxid = AppendonlyGetAuxIndex(blkdirRel);
	Assert(OidIsValid(blkdiridxid));
	Relation	blkdirIdx = index_open(blkdiridxid, RowExclusiveLock);

	ScanKeyData scanKey;
	SysScanDesc indexScan;
	HeapTuple	tuple;

	ScanKeyInit(&scanKey,
				Anum_pg_aoblkdir_segno,				/* segno */
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(segno));

	indexScan = systable_beginscan_ordered(blkdirRel,
										   blkdirIdx,
										   snapshot,
										   1 /* nkeys */,
										   &scanKey);

	while ((tuple = systable_getnext_ordered(indexScan, ForwardScanDirection)) != NULL)
	{
		CatalogTupleDelete(blkdirRel, &tuple->t_self);
	}
	systable_endscan_ordered(indexScan);

	index_close(blkdirIdx, RowExclusiveLock);
	table_close(blkdirRel, RowExclusiveLock);

}

/*
 * init_scankeys
 *
 * Initialize the scan keys.
 */
static void
init_scankeys(TupleDesc tupleDesc,
			  int nkeys, ScanKey scanKeys,
			  StrategyNumber *strategyNumbers)
{
	int			keyNo;

	Assert(nkeys <= tupleDesc->natts);

	for (keyNo = 0; keyNo < nkeys; keyNo++)
	{
		Oid			atttypid = TupleDescAttr(tupleDesc, keyNo)->atttypid;
		ScanKey		scanKey = (ScanKey) (((char *) scanKeys) +
										 keyNo * sizeof(ScanKeyData));
		RegProcedure opfuncid;
		StrategyNumber strategyNumber = strategyNumbers[keyNo];

		Assert(strategyNumber <= BTMaxStrategyNumber &&
			   strategyNumber != InvalidStrategy);

		if (strategyNumber == BTEqualStrategyNumber)
		{
			Oid			eq_opr;

			get_sort_group_operators(atttypid,
									 false, true, false,
									 NULL, &eq_opr, NULL, NULL);
			opfuncid = get_opcode(eq_opr);
			ScanKeyEntryInitialize(scanKey,
								   0,	/* sk_flag */
								   keyNo + 1,	/* attribute number to scan */
								   BTEqualStrategyNumber,	/* strategy */
								   InvalidOid,	/* strategy subtype */
								   InvalidOid,	/* collation */
								   opfuncid,	/* reg proc to use */
								   0	/* constant */
				);
		}
		else
		{
			Oid			gtOid,
						leOid;

			get_sort_group_operators(atttypid,
									 false, false, true,
									 NULL, NULL, &gtOid, NULL);
			leOid = get_negator(gtOid);
			opfuncid = get_opcode(leOid);

			ScanKeyEntryInitialize(scanKey,
								   0,	/* sk_flag */
								   keyNo + 1,	/* attribute number to scan */
								   strategyNumber,	/* strategy */
								   InvalidOid,	/* strategy subtype */
								   InvalidOid,	/* collation */
								   opfuncid,	/* reg proc to use */
								   0	/* constant */
				);
		}
	}
}


/*
 * extract_minipage
 *
 * Extract the minipage info from the given tuple. The tupleTid
 * is also set here.
 */
static void
extract_minipage(AppendOnlyBlockDirectory *blockDirectory,
				 HeapTuple tuple,
				 TupleDesc tupleDesc,
				 int columnGroupNo)
{
	Datum	   *values = blockDirectory->values;
	bool	   *nulls = blockDirectory->nulls;
	MinipagePerColumnGroup *minipageInfo = &blockDirectory->minipages[columnGroupNo];

	heap_deform_tuple(tuple, tupleDesc, values, nulls);

	Assert(blockDirectory->currentSegmentFileNum ==
		   DatumGetInt32(values[Anum_pg_aoblkdir_segno - 1]));

	/*
	 * Copy out the minipage
	 */
	copy_out_minipage(minipageInfo,
					  values[Anum_pg_aoblkdir_minipage - 1],
					  nulls[Anum_pg_aoblkdir_minipage - 1]);

	ItemPointerCopy(&tuple->t_self, &minipageInfo->tupleTid);
}

/*
 * load_last_minipage
 *
 * Search through the block directory btree to find the last row that
 * contains the last minipage.
 */
static void
load_last_minipage(AppendOnlyBlockDirectory *blockDirectory,
				   int64 lastSequence,
				   int columnGroupNo)
{
	Relation	blkdirRel = blockDirectory->blkdirRel;
	Relation	blkdirIdx = blockDirectory->blkdirIdx;
	TupleDesc	heapTupleDesc;
	SysScanDesc idxScanDesc;
	HeapTuple	tuple = NULL;
	MemoryContext oldcxt;
	int			numScanKeys = blockDirectory->numScanKeys;
	ScanKey		scanKeys = blockDirectory->scanKeys;

#ifdef USE_ASSERT_CHECKING
	StrategyNumber *strategyNumbers = blockDirectory->strategyNumbers;
#endif							/* USE_ASSERT_CHECKING */

	Assert(blockDirectory->aoRel != NULL);
	Assert(blockDirectory->blkdirRel != NULL);
	Assert(blockDirectory->blkdirIdx != NULL);

	oldcxt = MemoryContextSwitchTo(blockDirectory->memoryContext);

	heapTupleDesc = RelationGetDescr(blkdirRel);

	Assert(numScanKeys == 3);
	Assert(blockDirectory->currentSegmentFileInfo != NULL);

	/* Setup the scan keys for the scan. */
	Assert(scanKeys != NULL);
	Assert(strategyNumbers != NULL);
	if (lastSequence == 0)
		lastSequence = 1;

	scanKeys[0].sk_argument =
		Int32GetDatum(blockDirectory->currentSegmentFileNum);
	scanKeys[1].sk_argument = Int32GetDatum(columnGroupNo);
	scanKeys[2].sk_argument = Int64GetDatum(lastSequence);

	/*
	 * Search the btree to find the entry in the block directory that contains
	 * the last minipage.
	 */
	idxScanDesc = systable_beginscan_ordered(blkdirRel, blkdirIdx,
											 blockDirectory->appendOnlyMetaDataSnapshot,
											 numScanKeys, scanKeys);

	tuple = systable_getnext_ordered(idxScanDesc, BackwardScanDirection);
	if (tuple != NULL)
	{
		extract_minipage(blockDirectory,
						 tuple,
						 heapTupleDesc,
						 columnGroupNo);
	}

	systable_endscan_ordered(idxScanDesc);

	MemoryContextSwitchTo(oldcxt);

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory load last minipage: "
					  "(columnGroupNo, lastSequence, nEntries) = (%d, " INT64_FORMAT ", %u)",
					  columnGroupNo, lastSequence,
					  blockDirectory->minipages[columnGroupNo].numMinipageEntries)));

}

/*
 * find_minipage_entry
 *
 * Find the minipage entry that covers the given rowNum.
 * If such an entry does not exists, -1 is returned. Otherwise
 * the index to such an entry in the minipage array is returned,
 * and it is stored in cached_entry_no for caching purpose.
 */
static int
find_minipage_entry(MinipagePerColumnGroup *minipageInfo,
					int64 rowNum)
{
	int			start_no,
				end_no;
	int			entry_no = InvalidEntryNum;
	MinipageEntry *entry;
	Minipage *minipage = minipageInfo->minipage;
	uint32 numEntries = minipageInfo->numMinipageEntries;

	/* fast lookup in cached entry */
	if (minipageInfo->cached_entry_no != InvalidEntryNum)
	{
		Assert(minipageInfo->cached_entry_no < numEntries);

		entry_no = minipageInfo->cached_entry_no;
		entry = &(minipage->entry[entry_no]);

		if (entry->firstRowNum <= rowNum &&
			entry->firstRowNum + entry->rowCount > rowNum)
		{
			Assert(entry->firstRowNum > 0);
			Assert(entry->rowCount > 0);
	
			return entry_no;
		}
	}

	start_no = 0;
	end_no = numEntries - 1;
	while (start_no <= end_no)
	{
		entry_no = start_no + (end_no - start_no + 1) / 2;
		Assert(entry_no >= start_no && entry_no <= end_no);

		entry = &(minipage->entry[entry_no]);

		Assert(entry->firstRowNum > 0);
		Assert(entry->rowCount > 0);

		if (entry->firstRowNum <= rowNum &&
			entry->firstRowNum + entry->rowCount > rowNum)
			break;
		else if (entry->firstRowNum > rowNum)
		{
			end_no = entry_no - 1;
		}
		else
		{
			start_no = entry_no + 1;
		}
	}

	if (start_no <= end_no)
		minipageInfo->cached_entry_no = entry_no;
	else
		minipageInfo->cached_entry_no = InvalidEntryNum;

	return minipageInfo->cached_entry_no;
}

/*
 * write_minipage
 *
 * Write the in-memory minipage to the block directory relation.
 */
static void
write_minipage(AppendOnlyBlockDirectory *blockDirectory,
			   int columnGroupNo, MinipagePerColumnGroup *minipageInfo)
{
	HeapTuple	tuple;
	MemoryContext oldcxt;
	Datum	   *values = blockDirectory->values;
	bool	   *nulls = blockDirectory->nulls;
	Relation	blkdirRel = blockDirectory->blkdirRel;
	CatalogIndexState indinfo = blockDirectory->indinfo;
	TupleDesc	heapTupleDesc = RelationGetDescr(blkdirRel);

	Assert(minipageInfo->numMinipageEntries > 0);

	oldcxt = MemoryContextSwitchTo(blockDirectory->memoryContext);

	Assert(blkdirRel != NULL);

	values[Anum_pg_aoblkdir_segno - 1] =
		Int32GetDatum(blockDirectory->currentSegmentFileNum);
	nulls[Anum_pg_aoblkdir_segno - 1] = false;

	values[Anum_pg_aoblkdir_columngroupno - 1] =
		Int32GetDatum(columnGroupNo);
	nulls[Anum_pg_aoblkdir_columngroupno - 1] = false;

	values[Anum_pg_aoblkdir_firstrownum - 1] =
		Int64GetDatum(minipageInfo->minipage->entry[0].firstRowNum);
	nulls[Anum_pg_aoblkdir_firstrownum - 1] = false;

	SET_VARSIZE(minipageInfo->minipage,
				minipage_size(minipageInfo->numMinipageEntries));
	minipageInfo->minipage->nEntry = minipageInfo->numMinipageEntries;
	values[Anum_pg_aoblkdir_minipage - 1] =
		PointerGetDatum(minipageInfo->minipage);
	nulls[Anum_pg_aoblkdir_minipage - 1] = false;

	tuple = heaptuple_form_to(heapTupleDesc,
							  values,
							  nulls,
							  NULL,
							  NULL);

	/*
	 * Write out the minipage to the block directory relation. If this
	 * minipage is already in the relation, we update the row. Otherwise, a
	 * new row is inserted.
	 */
	if (ItemPointerIsValid(&minipageInfo->tupleTid))
	{
		ereportif(Debug_appendonly_print_blockdirectory, LOG,
				  (errmsg("Append-only block directory update a minipage: "
						  "(segno, columnGroupNo, nEntries, firstRowNum) = "
						  "(%d, %d, %u, " INT64_FORMAT ")",
						  blockDirectory->currentSegmentFileNum,
						  columnGroupNo, minipageInfo->numMinipageEntries,
						  minipageInfo->minipage->entry[0].firstRowNum)));

		CatalogTupleUpdateWithInfo(blkdirRel, &minipageInfo->tupleTid, tuple,
								   indinfo);
	}
	else
	{
		ereportif(Debug_appendonly_print_blockdirectory, LOG,
				  (errmsg("Append-only block directory insert a minipage: "
						  "(segno, columnGroupNo, nEntries, firstRowNum) = "
						  "(%d, %d, %u, " INT64_FORMAT ")",
						  blockDirectory->currentSegmentFileNum,
						  columnGroupNo, minipageInfo->numMinipageEntries,
						  minipageInfo->minipage->entry[0].firstRowNum)));

		CatalogTupleInsertWithInfo(blkdirRel, tuple, indinfo);
	}

	/* memorize updated/inserted tuple header info */
	ItemPointerCopy(&tuple->t_self, &minipageInfo->tupleTid);

	heap_freetuple(tuple);

	MemoryContextSwitchTo(oldcxt);
}

static void
clear_minipage(MinipagePerColumnGroup *minipagePerColumnGroup)
{
	MemSet(minipagePerColumnGroup->minipage->entry, 0,
		   minipagePerColumnGroup->numMinipageEntries * sizeof(MinipageEntry));
	minipagePerColumnGroup->numMinipageEntries = 0;
	ItemPointerSetInvalid(&minipagePerColumnGroup->tupleTid);
	minipagePerColumnGroup->cached_entry_no = InvalidEntryNum;
}

/*
 * AppendOnlyBlockDirectory_InsertPlaceholder
 *
 * We perform uniqueness checks by looking up block directory rows that cover
 * the rowNum indicated by the aotid obtained from the index. See
 * AppendOnlyBlockDirectory_CoversTuple() for details.
 *
 * However, there are multiple time windows in which there are no covering block
 * directory entries in the table for already inserted data rows. Such time
 * windows start from when a data row is inserted and lasts till the block
 * directory row covering it is written to the block directory table (see
 * write_minipage()). Block directory rows are written only when:
 * 	(i) the current in-memory minipage is full
 * 	(ii) at end of command.
 *
 * So we insert a placeholder entry in the current block directory row and
 * persist the row before the first insert to cover rows in the range:
 * [firstRowNum, lastRowNum], starting at firstOffset in the relfile
 * corresponding to columnGroupNo.
 *
 * firstRowNum is the rowNum assigned to the 1st insert of the insert command.
 * lastRowNum is the last rowNum that will be entered by the insert command,
 * which is something unknown to us. So, to cover all such windows during the
 * insert command's execution, we insert an entry with a placeholder
 * rowcount = AOTupleId_MaxRowNum into the current minipage and write it to the
 * relation (by reusing the machinery in write_minipage()). Such a row whose
 * last entry is a placeholder entry is called a placeholder row. This entry
 * will cover up to lastRowNum, whatever its value may be, for all such time
 * windows during the insert command.
 *
 * Safety:
 * (1) The placeholder upper bound is not a concern as this row will be consulted
 * ONLY by SNAPSHOT_DIRTY (for uniqueness checks) and will be ignored by regular
 * MVCC processing (for index scans). Eventually, it will be rendered invisible
 * as it will be updated by a subsequent write_minipage() or by virtue of abort.
 *
 * (2) There is no way a placeholder row will detect spurious conflicts due to
 * its loose upper bound, in the same segment file, to which it maps. This is
 * because there can be no other rows inserted into a segment file other than
 * the insert operation that is currently in progress on the file.
 */
void
AppendOnlyBlockDirectory_InsertPlaceholder(AppendOnlyBlockDirectory *blockDirectory,
									  int64 firstRowNum,
									  int64 fileOffset,
									  int columnGroupNo)
{
	MinipagePerColumnGroup *minipagePerColumnGroup;

	Assert(firstRowNum > 0);
	Assert(fileOffset >= 0);
	Assert(RelationIsValid(blockDirectory->blkdirRel));
	Assert(columnGroupNo >= 0 &&
		columnGroupNo < blockDirectory->aoRel->rd_att->natts);

	minipagePerColumnGroup = &blockDirectory->minipages[columnGroupNo];

	/* insert placeholder entry with a max row count */
	insert_new_entry(blockDirectory, columnGroupNo, firstRowNum, fileOffset,
					 AOTupleId_MaxRowNum);
	/* insert placeholder row containing placeholder entry */
	write_minipage(blockDirectory, columnGroupNo, minipagePerColumnGroup);
	/*
	 * Delete the placeholder entry as it has no business being in memory.
	 * Removing it from the current minipage will make rest of the processing
	 * for the current command behave as if it never existed. The absence of
	 * this entry will help effectively "update" it once it's replacement entry
	 * is created in memory, in a subsequent call to insert_new_entry(),
	 * followed by a write_minipage() which will make this "update" persistent.
	 */
	minipagePerColumnGroup->numMinipageEntries--;
	/*
	 * Increment the command counter, as we will be updating this temp row later
	 * on in write_minipage().
	 */
	CommandCounterIncrement();
}

void
AppendOnlyBlockDirectory_End_forInsert(
									   AppendOnlyBlockDirectory *blockDirectory)
{
	int			groupNo;

	if (blockDirectory->blkdirRel == NULL ||
		blockDirectory->blkdirIdx == NULL)
		return;
	for (groupNo = 0; groupNo < blockDirectory->numColumnGroups; groupNo++)
	{
		MinipagePerColumnGroup *minipageInfo =
		&blockDirectory->minipages[groupNo];

		if (minipageInfo->numMinipageEntries > 0)
		{
			write_minipage(blockDirectory, groupNo, minipageInfo);
			ereportif(Debug_appendonly_print_blockdirectory, LOG,
					  (errmsg("Append-only block directory end of insert write minipage: "
							  "(columnGroupNo, nEntries) = (%d, %u)",
							  groupNo, minipageInfo->numMinipageEntries)));
		}
	}

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory end for insert: "
					  "(segno, numColumnGroups, isAOCol)="
					  "(%d, %d, %d)",
					  blockDirectory->currentSegmentFileNum,
					  blockDirectory->numColumnGroups,
					  blockDirectory->isAOCol)));

	index_close(blockDirectory->blkdirIdx, RowExclusiveLock);
	heap_close(blockDirectory->blkdirRel, RowExclusiveLock);
	CatalogCloseIndexes(blockDirectory->indinfo);

	MemoryContextDelete(blockDirectory->memoryContext);
}

void
AppendOnlyBlockDirectory_End_forSearch(
									   AppendOnlyBlockDirectory *blockDirectory)
{
	if (blockDirectory->blkdirRel == NULL)
		return;

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory end for search: "
					  "(totalSegfiles, numColumnGroups, isAOCol)="
					  "(%d, %d, %d)",
					  blockDirectory->totalSegfiles,
					  blockDirectory->numColumnGroups,
					  blockDirectory->isAOCol)));

	if (blockDirectory->blkdirIdx)
		index_close(blockDirectory->blkdirIdx, AccessShareLock);
	heap_close(blockDirectory->blkdirRel, AccessShareLock);

	MemoryContextDelete(blockDirectory->memoryContext);
}

void
AppendOnlyBlockDirectory_End_writeCols(
	AppendOnlyBlockDirectory *blockDirectory, List *newvals)
{
	ListCell *lc;

	if (blockDirectory->blkdirRel == NULL ||
		blockDirectory->blkdirIdx == NULL)
		return;

	foreach(lc, newvals)
	{
		NewColumnValue *newval = lfirst(lc);
		int colno = newval->attnum - 1;
		MinipagePerColumnGroup *minipageInfo =
								   &blockDirectory->minipages[colno];

		if (minipageInfo->numMinipageEntries > 0)
		{
			write_minipage(blockDirectory, colno, minipageInfo);
			ereportif(Debug_appendonly_print_blockdirectory, LOG,
					  (errmsg("Append-only block directory end of insert write"
							  " minipage: (columnGroupNo, nEntries) = (%d, %u)",
							  colno, minipageInfo->numMinipageEntries)));
		}
	}

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory end for insert: "
					  "(segno, numColumnGroups, isAOCol)="
					  "(%d, %d, %d)",
					  blockDirectory->currentSegmentFileNum,
					  blockDirectory->numColumnGroups,
					  blockDirectory->isAOCol)));

	/*
	 * We already hold transaction-scope exclusive lock on the AOCS relation.
	 * Let's defer release of locks on block directory as well until the end
	 * of alter-table transaction.
	 */
	index_close(blockDirectory->blkdirIdx, NoLock);
	heap_close(blockDirectory->blkdirRel, NoLock);
	CatalogCloseIndexes(blockDirectory->indinfo);

	MemoryContextDelete(blockDirectory->memoryContext);
}

void
AppendOnlyBlockDirectory_End_forUniqueChecks(AppendOnlyBlockDirectory *blockDirectory)
{
	Assert(RelationIsValid(blockDirectory->blkdirRel));
	Assert(RelationIsValid(blockDirectory->blkdirIdx));

	/* This must have been reset after each uniqueness check */
	Assert(blockDirectory->appendOnlyMetaDataSnapshot == InvalidSnapshot);

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory end for unique checks"),
				  errdetail("(aoRel = %u, blkdirrel = %u, blkdiridxrel = %u)",
							blockDirectory->aoRel->rd_id,
							blockDirectory->blkdirRel->rd_id,
							blockDirectory->blkdirIdx->rd_id)));

	index_close(blockDirectory->blkdirIdx, AccessShareLock);
	heap_close(blockDirectory->blkdirRel, AccessShareLock);

	MemoryContextDelete(blockDirectory->memoryContext);
}

void
AppendOnlyBlockDirectory_End_forIndexOnlyScan(AppendOnlyBlockDirectory *blockDirectory)
{
	Assert(RelationIsValid(blockDirectory->blkdirRel));
	Assert(RelationIsValid(blockDirectory->blkdirIdx));

	ereportif(Debug_appendonly_print_blockdirectory, LOG,
			  (errmsg("Append-only block directory end for index-only scan"),
				  errdetail("(aoRel = %u, blkdirrel = %u, blkdiridxrel = %u)",
							blockDirectory->aoRel->rd_id,
							blockDirectory->blkdirRel->rd_id,
							blockDirectory->blkdirIdx->rd_id)));

	index_close(blockDirectory->blkdirIdx, AccessShareLock);
	heap_close(blockDirectory->blkdirRel, AccessShareLock);

	MemoryContextDelete(blockDirectory->memoryContext);
}

/*
 * AOBlkDirScan_GetRowNum
 * 
 * Given a target logical row number,
 * returns the corresponding physical rowNum,
 * or -1 if not found.
 * 
 * targrow: 0-based target logical row number
 * startrow: pointer of the start point stepping to targrow
 * targsegno: the segfile number in which targrow locates
 * colgroupno: current coloumn group number, always 0 for ao_row
 */
int64
AOBlkDirScan_GetRowNum(AOBlkDirScan blkdirscan,
					   int targsegno,
					   int colgroupno,
					   int64 targrow,
					   int64 *startrow)
{
	HeapTuple tuple;
	TupleDesc tupdesc;
	AppendOnlyBlockDirectory *blkdir = blkdirscan->blkdir;
	int mpentryi;
	MinipagePerColumnGroup *mpinfo;
	int64 rownum = -1;

	Assert(targsegno >= 0);
	Assert(blkdir != NULL);

	if (blkdirscan->segno != targsegno || blkdirscan->colgroupno != colgroupno)
	{
		if (blkdirscan->sysscan != NULL)
			systable_endscan_ordered(blkdirscan->sysscan);

		ScanKeyData	scankeys[2];
		
		ScanKeyInit(&scankeys[0],
					Anum_pg_aoblkdir_segno,
					BTEqualStrategyNumber,
					F_INT4EQ,
					Int32GetDatum(targsegno));
		
		ScanKeyInit(&scankeys[1],
					Anum_pg_aoblkdir_columngroupno,
					BTEqualStrategyNumber,
					F_INT4EQ,
					Int32GetDatum(colgroupno));
		
		blkdirscan->sysscan = systable_beginscan_ordered(blkdir->blkdirRel,
														 blkdir->blkdirIdx,
														 blkdir->appendOnlyMetaDataSnapshot,
														 2, /* nkeys */
														 scankeys);
		blkdirscan->segno = targsegno;
		blkdirscan->colgroupno = colgroupno;
		/* reset to InvalidEntryNum for new Minipage entry array */
		blkdirscan->mpentryno = InvalidEntryNum;
	}

	mpentryi = blkdirscan->mpentryno;
	mpinfo = &blkdir->minipages[colgroupno];

	while (true)
	{
		if (mpentryi == InvalidEntryNum)
		{
			tuple = systable_getnext_ordered(blkdirscan->sysscan, ForwardScanDirection);
			if (HeapTupleIsValid(tuple))
			{
				tupdesc = RelationGetDescr(blkdir->blkdirRel);
				extract_minipage(blkdir, tuple, tupdesc, colgroupno);
				/* new minipage */
				mpentryi = 0;
				mpinfo = &blkdir->minipages[colgroupno];
			}
			else
			{	
				/* done this < segno, colgroupno > */
				systable_endscan_ordered(blkdirscan->sysscan);
				blkdirscan->sysscan = NULL;
				blkdirscan->segno = -1;
				blkdirscan->colgroupno = 0;
				/* always set both vars in pairs for safe */
				mpentryi = InvalidEntryNum;
				mpinfo = NULL;

				goto out;
			}
		}

		Assert(mpentryi != InvalidEntryNum);
		Assert(mpinfo != NULL);

		for (int i = mpentryi; i < mpinfo->numMinipageEntries; i++)
		{
			Minipage *mp = mpinfo->minipage;
			MinipageEntry *entry = &(mp->entry[i]);

			Assert(entry->firstRowNum > 0);
			Assert(entry->rowCount > 0);

			if (*startrow + entry->rowCount - 1 >= targrow)
			{
				rownum = entry->firstRowNum + (targrow - *startrow);
				mpentryi = i;
				goto out;
			}

			*startrow += entry->rowCount;
		}

		/* done this minipage */
		mpentryi = InvalidEntryNum;
		mpinfo = NULL;
	}

out:
	/* set the result of minipage entry lookup */
	blkdirscan->mpentryno = mpentryi;

	return rownum;
}

/*
 * Locate the index of the fileseginfo struct in the block directory's fileseg
 * array, given a 'segmentFileNum'.
 */
static int
findFileSegInfo(AppendOnlyBlockDirectory *blockDirectory, int segmentFileNum)
{
	FileSegInfo *fsInfo;
	int 		i;
	Assert(segmentFileNum != InvalidFileSegNumber &&
		segmentFileNum <= AOTupleId_MaxSegmentFileNum);

	for (i = 0; i < blockDirectory->totalSegfiles; i++)
	{
		fsInfo = blockDirectory->segmentFileInfo[i];

		if (!blockDirectory->isAOCol && segmentFileNum == fsInfo->segno)
			break;
		else if (blockDirectory->isAOCol && segmentFileNum ==
			((AOCSFileSegInfo *) fsInfo)->segno)
			break;
	}

	return i;
}
