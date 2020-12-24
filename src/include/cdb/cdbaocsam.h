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

    Oid         segrelid;
    Oid         blkdirrelid;
    Oid         visimaprelid;
    Oid         visimapidxid;
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
	int64 total_row;
	int64 cur_seg_row;

	/*
	 * Only used by `analyze`
	 */
	int64		nextTupleId;
	int64		targetTupleId;

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
} AOCSScanDescData;

typedef AOCSScanDescData *AOCSScanDesc;

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
	 * The largest row number of this aoseg. Maximum row number is required in
	 * function "aocs_fetch". If we have no updates and deletes, the
	 * total_tupcount is equal to the maximum row number. But after some updates
	 * and deletes, the maximum row number always much bigger than
	 * total_tupcount. The appendonly_insert function will get fast sequence and
	 * use it as the row number. So the last sequence will be the correct
	 * maximum row number.
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

typedef struct AOCSUpdateDescData *AOCSUpdateDesc;
typedef struct AOCSDeleteDescData *AOCSDeleteDesc;

/*
 * Descriptor for fetches from table via an index.
 */
typedef struct IndexFetchAOCOData
{
	IndexFetchTableData xs_base;	/* AM independent part of the descriptor */

	AOCSFetchDesc       aocofetch;

	bool                *proj;
} IndexFetchAOCOData;

/*
 * GPDB_12_MERGE_FIXME:
 * Descriptor for fetches from table via bitmap. In upstream the code goes
 * through table_beginscan() and it should be the same struct in all cases.
 * However in GPDB extra info is needed which should not be initialized or
 * computed for all scan calls. A new method has been added (with a MERGE_FIXME)
 * which is only used for bitmap scans. Take advantage of it and create a new
 * struct to contain only the information needed. 
 */


typedef struct AOCSHeaderScanDescData
{
	int32 colno;  /* chosen column number to read headers from */

	AppendOnlyStorageRead ao_read;

} AOCSHeaderScanDescData;

typedef AOCSHeaderScanDescData *AOCSHeaderScanDesc;

typedef struct AOCSAddColumnDescData
{
	Relation rel;

	AppendOnlyBlockDirectory blockDirectory;

	DatumStreamWrite **dsw;
	/* array of datum stream write objects, one per new column */

	int num_newcols;

	int32 cur_segno;

} AOCSAddColumnDescData;
typedef AOCSAddColumnDescData *AOCSAddColumnDesc;

/* ----------------
 *		function prototypes for appendoptimized columnar access method
 * ----------------
 */

extern AOCSScanDesc aocs_beginscan(Relation relation, Snapshot snapshot,
								   bool *proj, uint32 flags);
extern AOCSScanDesc aocs_beginrangescan(Relation relation, Snapshot snapshot,
										Snapshot appendOnlyMetaDataSnapshot,
										int *segfile_no_arr, int segfile_count);

extern void aocs_rescan(AOCSScanDesc scan);
extern void aocs_endscan(AOCSScanDesc scan);

extern bool aocs_getnext(AOCSScanDesc scan, ScanDirection direction, TupleTableSlot *slot);
extern AOCSInsertDesc aocs_insert_init(Relation rel, int segno);
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

extern AOCSUpdateDesc aocs_update_init(Relation rel, int segno);
extern void aocs_update_finish(AOCSUpdateDesc desc);
extern TM_Result aocs_update(AOCSUpdateDesc desc, TupleTableSlot *slot,
			AOTupleId *oldTupleId, AOTupleId *newTupleId);

extern AOCSDeleteDesc aocs_delete_init(Relation rel);
extern TM_Result aocs_delete(AOCSDeleteDesc desc, 
		AOTupleId *aoTupleId);
extern void aocs_delete_finish(AOCSDeleteDesc desc);

extern AOCSHeaderScanDesc aocs_begin_headerscan(
		Relation rel, int colno);
extern void aocs_headerscan_opensegfile(
		AOCSHeaderScanDesc hdesc, AOCSFileSegInfo *seginfo, char *basepath);
extern bool aocs_get_nextheader(AOCSHeaderScanDesc hdesc);
extern void aocs_end_headerscan(AOCSHeaderScanDesc hdesc);
extern AOCSAddColumnDesc aocs_addcol_init(
		Relation rel, int num_newcols);
extern void aocs_addcol_newsegfile(
		AOCSAddColumnDesc desc, AOCSFileSegInfo *seginfo, char *basepath,
		RelFileNodeBackend relfilenode);
extern void aocs_addcol_closefiles(AOCSAddColumnDesc desc);
extern void aocs_addcol_endblock(AOCSAddColumnDesc desc, int64 firstRowNum);
extern void aocs_addcol_insert_datum(AOCSAddColumnDesc desc,
									   Datum *d, bool *isnull);
extern void aocs_addcol_finish(AOCSAddColumnDesc desc);
extern void aocs_addcol_emptyvpe(
		Relation rel, AOCSFileSegInfo **segInfos,
		int32 nseg, int num_newcols);
extern void aocs_addcol_setfirstrownum(AOCSAddColumnDesc desc,
		int64 firstRowNum);

extern void aoco_dml_init(Relation relation, CmdType operation);
extern void aoco_dml_finish(Relation relation, CmdType operation);

#endif   /* AOCSAM_H */
