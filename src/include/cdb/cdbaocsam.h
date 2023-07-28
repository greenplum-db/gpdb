/*-------------------------------------------------------------------------
 *
 * cdbaocsam.h
 *	  append-only columnar relation access method definitions.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbaocsam.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDB_AOCSAM_H
#define CDB_AOCSAM_H

#include "access/relscan.h"
#include "access/sdir.h"
#include "access/tableam.h"
#include "access/tupmacs.h"
#include "access/xlogutils.h"
#include "access/appendonlytid.h"
#include "access/appendonly_visimap.h"
#include "executor/tuptable.h"
#include "nodes/primnodes.h"
#include "storage/block.h"
#include "utils/rel.h"
#include "utils/snapshot.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "utils/datumstream.h"
#include "nodes/execnodes.h"

/*
 * AOCSInsertDescData is used for inserting data into append-only columnar
 * relations. It serves an equivalent purpose as AOCSScanDescData
 * only that the later is used for scanning append-only columnar
 * relations.
 */
struct DatumStream;
struct AOCSFileSegInfo;

typedef struct AOCSInsertDescData
{
	Relation	aoi_rel;
	Snapshot	appendOnlyMetaDataSnapshot;
	AOCSFileSegInfo *fsInfo;
	int64		insertCount;
	int64		varblockCount;
	int64		rowCount; /* total row count before insert */
	int64		numSequences; /* total number of available sequences */
	int64		lastSequence; /* last used sequence */
	int32		cur_segno;

	char *compType;
	int32 compLevel;
	int32 blocksz;
	bool  checksum;
	bool  skipModCountIncrement;

    Oid         segrelid;
    Oid         blkdirrelid;
    Oid         visimaprelid;
	struct DatumStreamWrite **ds;

	AppendOnlyBlockDirectory blockDirectory;
} AOCSInsertDescData;

typedef AOCSInsertDescData *AOCSInsertDesc;

/*
 * Scan descriptors
 */

/*
 * AOCS relations do not have a direct access to TID's. In order to scan via
 * TID's the blockdirectory is used and a distinct scan descriptor that is
 * closer to an index scan than a relation scan is needed. This is different
 * from heap relations where the same descriptor is used for all scans.
 *
 * Likewise the tableam API always expects the same TableScanDescData extended
 * structure to be used for all scans. However, for bitmapheapscans on AOCS
 * relations, a distinct descriptor is needed and a different method to
 * initialize it is used, (table_beginscan_bm_ecs).
 *
 * This enum is used by the aocsam_handler to distiguish between the different
 * TableScanDescData structures internaly in the aocsam_handler.
 */
enum AOCSScanDescIdentifier
{
	AOCSSCANDESCDATA,		/* public */
	AOCSBITMAPSCANDATA		/* am private */
};

/*
 * Used for fetch individual tuples from specified by TID of append only relations
 * using the AO Block Directory.
 */
typedef struct AOCSFetchDescData
{
	Relation		relation;
	Snapshot		appendOnlyMetaDataSnapshot;

	/*
	 * Snapshot to use for non-metadata operations.
	 * Usually snapshot = appendOnlyMetaDataSnapshot, but they
	 * differ e.g. if gp_select_invisible is set.
	 */ 
	Snapshot    snapshot;

	MemoryContext	initContext;

	int				totalSegfiles;
	struct AOCSFileSegInfo **segmentFileInfo;

	/*
	 * Array containing the maximum row number in each aoseg (to be consulted
	 * during fetch). This is a sparse array as not all segments are involved
	 * in a scan. Sparse entries are marked with InvalidAORowNum.
	 *
	 * Note:
	 * If we have no updates and deletes, the total_tupcount is equal to the
	 * maximum row number. But after some updates and deletes, the maximum row
	 * number is always much bigger than total_tupcount, so this carries the
	 * last sequence from gp_fastsequence.
	 */
	int64			lastSequence[AOTupleId_MultiplierSegmentFileNum];

	char			*segmentFileName;
	int				segmentFileNameMaxLen;
	char            *basepath;

	AppendOnlyBlockDirectory	blockDirectory;

	DatumStreamFetchDesc *datumStreamFetchDesc;

	int64	skipBlockCount;

	AppendOnlyVisimap visibilityMap;

	Oid segrelid;
} AOCSFetchDescData;

typedef AOCSFetchDescData *AOCSFetchDesc;

/*
 * Used for scan of appendoptimized column oriented relations, should be used in
 * the tableam api related code and under it.
 */
typedef struct AOCSScanDescData
{
	TableScanDescData rs_base;	/* AM independent part of the descriptor */

	/* AM dependant part of the descriptor */
	enum AOCSScanDescIdentifier descIdentifier;

	/* synthetic system attributes */
	ItemPointerData cdb_fake_ctid;

	/*
	 * used by `analyze`
	 */

	/*
	 * targrow: the output of the Row-based sampler (Alogrithm S), denotes a
	 * rownumber in the flattened row number space that is the target of a sample,
	 * which starts from 0.
	 * In other words, if we have seg0 rownums: [1, 100], seg1 rownums: [1, 200]
	 * If targrow = 150, then we are referring to seg1's rownum=51.
	 */
	int64			targrow;

	/*
	 * segfirstrow: pointing to the next starting row which is used to check
	 * the distance to `targrow`
	 */
	int64			segfirstrow;

	/*
	 * segrowsprocessed: track the rows processed under the current segfile.
	 * Don't miss updating it accordingly when "segfirstrow" is updated.
	 */
	int64			segrowsprocessed;

	AOBlkDirScan	blkdirscan;
	AOCSFetchDesc	aocsfetch;
	bool 			*proj;

	/*
	 * Part of the struct to be used only inside aocsam.c
	 */

	/*
	 * Snapshot to use for metadata operations.
	 * Usually snapshot = appendOnlyMetaDataSnapshot, but they
	 * differ e.g. if gp_select_invisible is set.
	 */ 
	Snapshot	appendOnlyMetaDataSnapshot;

	/*
	 * Anonymous struct containing column level informations. In AOCS relations,
	 * it is possible to only scan a subset of the columns. That subset is
	 * recorderd in the proj_atts array. If all the columns are required, then
	 * is populated from the relation's tuple descriptor.
	 *
	 * The tuple descriptor for the scan can be different from the tuple
	 * descriptor of the relation as held in rs_base. Such a scenario occurs
	 * during some ALTER TABLE operations. In all cases, it is the caller's
	 * responsibility to provide a valid tuple descriptor for the scan. It will
	 * get acquired from the slot.
	 *
	 * The proj_atts array if empty, and the datumstreams, will get initialized in
	 * relation to the tuple descriptor, when it becomes available.
	 */
	struct {
		/*
		 * Used during lazy initialization since at that moment, the context is the
		 * per tuple context, we need to keep a reference to the context used in
		 * begin_scan
		 */
		MemoryContext	scanCtx;

		TupleDesc	relationTupleDesc;

		/* Column numbers (zero based) of columns we need to fetch */
		AttrNumber		   *proj_atts;
		AttrNumber			num_proj_atts;

		struct DatumStreamRead **ds;
	} columnScanInfo;

	struct AOCSFileSegInfo **seginfo;
	int32					 total_seg;
	int32					 cur_seg;

	/*
	 * The only relation wide Storage Option, the rest are aquired in a per
	 * column basis and there is no need to keep track of.
	 */
	bool checksum;

	/*
	 * The block directory info.
	 *
	 * For CO tables the block directory is built during the first index
	 * creation. If set indicates whether to build block directory while
	 * scanning.
	 */
	AppendOnlyBlockDirectory *blockDirectory;
	AppendOnlyVisimap visibilityMap;

	/*
	 * The total number of bytes read, compressed, across all segment files, and
	 * across all columns projected, so far. It is used for scan progress reporting.
	 */
	int64		totalBytesRead;
} AOCSScanDescData;

typedef AOCSScanDescData *AOCSScanDesc;

/*
 * AOCSDeleteDescData is used for delete data from AOCS relations.
 * It serves an equivalent purpose as AppendOnlyScanDescData
 * (relscan.h) only that the later is used for scanning append-only
 * relations.
 */
typedef struct AOCSDeleteDescData
{
	/*
	 * Relation to delete from
	 */
	Relation	aod_rel;

	/*
	 * visibility map
	 */
	AppendOnlyVisimap visibilityMap;

	/*
	 * Visimap delete support structure. Used to handle out-of-order deletes
	 */
	AppendOnlyVisimapDelete visiMapDelete;

}			AOCSDeleteDescData;
typedef struct AOCSDeleteDescData *AOCSDeleteDesc;

typedef struct AOCSUniqueCheckDescData
{
	AppendOnlyBlockDirectory *blockDirectory;
	AppendOnlyVisimap 		 *visimap;
} AOCSUniqueCheckDescData;

typedef struct AOCSUniqueCheckDescData *AOCSUniqueCheckDesc;

typedef struct AOCSIndexOnlyDescData
{
	AppendOnlyBlockDirectory *blockDirectory;
	AppendOnlyVisimap 		 *visimap;
} AOCSIndexOnlyDescData, *AOCSIndexOnlyDesc;

/*
 * Descriptor for fetches from table via an index.
 */
typedef struct IndexFetchAOCOData
{
	IndexFetchTableData xs_base;		/* AM independent part of the descriptor */

	AOCSFetchDesc       aocofetch;		/* used only for index scans */

	AOCSIndexOnlyDesc	indexonlydesc;	/* used only for index only scans */

	bool                *proj;
} IndexFetchAOCOData;

typedef struct AOCSHeaderScanDescData
{
	Oid   relid;  /* relid of the relation */
	int32 colno;  /* chosen column number to read headers from */

	AppendOnlyStorageRead ao_read;

} AOCSHeaderScanDescData;

typedef AOCSHeaderScanDescData *AOCSHeaderScanDesc;

/* Indicate what operation this is for. */
typedef enum AOCSWriteColumnOperation
{
	AOCSADDCOLUMN,  /* ADD COLUMN */
	AOCSREWRITECOLUMN /* ALTER COLUMN TYPE */
} AOCSWriteColumnOperation;

typedef struct AOCSWriteColumnDescData
{
	Relation rel;

	AppendOnlyBlockDirectory blockDirectory;

	DatumStreamWrite **dsw;
	/* array of datum stream write objects, one per new column */

	int num_cols_to_write;

	int32 cur_segno;

	List *newcolvals;

	AOCSWriteColumnOperation op;
} AOCSWriteColumnDescData;
typedef AOCSWriteColumnDescData *AOCSWriteColumnDesc;

/* ----------------
 *		function prototypes for appendoptimized columnar access method
 * ----------------
 */

extern AOCSScanDesc aocs_beginscan(Relation relation, Snapshot snapshot,
								   bool *proj, uint32 flags);
extern AOCSScanDesc
aocs_beginrangescan(Relation relation,
					Snapshot snapshot,
					Snapshot appendOnlyMetaDataSnapshot,
					int *segfile_no_arr,
					int segfile_count,
					bool *proj);

extern void aocs_rescan(AOCSScanDesc scan);
extern void aocs_endscan(AOCSScanDesc scan);

extern bool aocs_getnext(AOCSScanDesc scan, ScanDirection direction, TupleTableSlot *slot);
extern AOCSInsertDesc aocs_insert_init(Relation rel, int segno, int64 num_rows);
extern void aocs_insert_values(AOCSInsertDesc idesc, Datum *d, bool *null, AOTupleId *aoTupleId);
static inline void aocs_insert(AOCSInsertDesc idesc, TupleTableSlot *slot)
{
	slot_getallattrs(slot);
	aocs_insert_values(idesc, slot->tts_values, slot->tts_isnull, (AOTupleId *) &slot->tts_tid);
}
extern void aocs_insert_finish(AOCSInsertDesc idesc);
extern AOCSFetchDesc aocs_fetch_init(Relation relation,
									 Snapshot snapshot,
									 Snapshot appendOnlyMetaDataSnapshot,
									 bool *proj);
extern bool aocs_fetch(AOCSFetchDesc aocsFetchDesc,
					   AOTupleId *aoTupleId,
					   TupleTableSlot *slot);
extern void aocs_fetch_finish(AOCSFetchDesc aocsFetchDesc);
extern AOCSIndexOnlyDesc aocs_index_only_init(Relation relation,
											  Snapshot snapshot);
extern bool aocs_index_only_check(AOCSIndexOnlyDesc indexonlydesc,
								  AOTupleId *aotid,
								  Snapshot snapshot);
extern void aocs_index_only_finish(AOCSIndexOnlyDesc indexonlydesc);
extern AOCSDeleteDesc aocs_delete_init(Relation rel);
extern TM_Result aocs_delete(AOCSDeleteDesc desc, 
		AOTupleId *aoTupleId);
extern void aocs_delete_finish(AOCSDeleteDesc desc);

extern AOCSHeaderScanDesc aocs_begin_headerscan(
		Relation rel, int colno);
extern bool aocs_get_target_tuple(AOCSScanDesc aoscan, int64 targrow, TupleTableSlot *slot);
extern AOCSWriteColumnDesc aocs_writecol_init(Relation rel, List *newvals, AOCSWriteColumnOperation op);
extern void aocs_writecol_add(Oid relid, List *newvals, List *constraints, TupleDesc oldDesc);
extern void aocs_writecol_rewrite(Oid relid, List *newvals, TupleDesc oldDesc);

extern void aoco_dml_init(Relation relation);
extern void aoco_dml_finish(Relation relation);

extern bool aocs_positionscan(AOCSScanDesc aoscan,
							  AppendOnlyBlockDirectoryEntry *dirEntry,
							  int colIdx,
							  int fsInfoIdx);

/*
 * Update total bytes read for the entire scan. If the block was compressed,
 * update it with the compressed length. If the block was not compressed, update
 * it with the uncompressed length.
 */
static inline void
AOCSScanDesc_UpdateTotalBytesRead(AOCSScanDesc scan, AttrNumber attno)
{
	Assert(scan->columnScanInfo.ds[attno]);
	Assert(scan->columnScanInfo.ds[attno]->ao_read.isActive);

	if (scan->columnScanInfo.ds[attno]->ao_read.current.isCompressed)
		scan->totalBytesRead += scan->columnScanInfo.ds[attno]->ao_read.current.compressedLen;
	else
		scan->totalBytesRead += scan->columnScanInfo.ds[attno]->ao_read.current.uncompressedLen;
}

static inline int64
AOCSScanDesc_TotalTupCount(AOCSScanDesc scan)
{
	Assert(scan != NULL);

	int64 totalrows = 0;
	AOCSFileSegInfo **seginfo = scan->seginfo;

    for (int i = 0; i < scan->total_seg; i++)
    {
	    if (seginfo[i]->state != AOSEG_STATE_AWAITING_DROP)
		    totalrows += seginfo[i]->total_tupcount;
    }

    return totalrows;
}

#endif   /* AOCSAM_H */
