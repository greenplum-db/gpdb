/*-------------------------------------------------------------------------
 *
 * cdbappendonlyam.h
 *	  append-only relation access method definitions.
 *
 *
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2007, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlyam.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYAM_H
#define CDBAPPENDONLYAM_H

#include "access/htup.h"
#include "access/relscan.h"
#include "access/sdir.h"
#include "access/tupmacs.h"
#include "access/xlogutils.h"
#include "access/xlog.h"
#include "access/appendonly_visimap.h"
#include "executor/tuptable.h"
#include "nodes/primnodes.h"
#include "nodes/bitmapset.h"
#include "storage/block.h"
#include "storage/lmgr.h"
#include "utils/rel.h"
#include "utils/tqual.h"

#include "access/appendonlytid.h"

#include "cdb/cdbbufferedappend.h"
#include "cdb/cdbbufferedread.h"
#include "cdb/cdbvarblock.h"

#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "cdb/cdbappendonlyblockdirectory.h"

#define DEFAULT_COMPRESS_LEVEL				 (0)
#define MIN_APPENDONLY_BLOCK_SIZE			 (8 * 1024)
#define DEFAULT_APPENDONLY_BLOCK_SIZE		(32 * 1024)
#define MAX_APPENDONLY_BLOCK_SIZE			 (2 * 1024 * 1024)
#define DEFAULT_VARBLOCK_TEMPSPACE_LEN   	 (4 * 1024)
#define DEFAULT_FS_SAFE_WRITE_SIZE			 (0)

/*
 * AppendOnlyInsertDescData is used for inserting data into append-only
 * relations. It serves an equivalent purpose as AppendOnlyScanDescData
 * (relscan.h) only that the later is used for scanning append-only 
 * relations. 
 */
typedef struct AppendOnlyInsertDescData
{
	Relation		aoi_rel;
	Snapshot		appendOnlyMetaDataSnapshot;
	MemTupleBinding *mt_bind;
	File			appendFile;
	int				appendFilePathNameMaxLen;
	char			*appendFilePathName;
	int64			insertCount;
	int64			varblockCount;
	int64           rowCount; /* total row count before insert */
	int64           numSequences; /* total number of available sequences */
	int64           lastSequence; /* last used sequence */
	BlockNumber		cur_segno;
	FileSegInfo     *fsInfo;
	VarBlockMaker	varBlockMaker;
	int64			bufferCount;
	int64			blockFirstRowNum;
	bool			shouldCompress;
	bool			usingChecksum;
	bool			useNoToast;
	int32			completeHeaderLen;
	uint8			*tempSpace;
	uint8			*uncompressedBuffer; /* used for compression */

	int32			usableBlockSize;
	int32			maxDataLen;
	int32			tempSpaceLen;

	char						*title;
				/*
				 * A phrase that better describes the purpose of the this open.
				 *
				 * We manage the storage for this.
				 */

	/*
	 * These serve the equivalent purpose of the uppercase constants of the same
	 * name in tuptoaster.h but here we make these values dynamic.
	 */	
	int32			toast_tuple_threshold;
	int32			toast_tuple_target;
	AppendOnlyStorageAttributes storageAttributes;
	AppendOnlyStorageWrite		storageWrite;

	uint8			*nonCompressedData;

	/* The block directory for the appendonly relation. */
	AppendOnlyBlockDirectory blockDirectory;

	bool update_mode;
} AppendOnlyInsertDescData;

typedef AppendOnlyInsertDescData *AppendOnlyInsertDesc;

typedef struct AppendOnlyExecutorReadBlock
{
	MemoryContext	memoryContext;

	AppendOnlyStorageRead	*storageRead;

	/*
	 * When reading a segfile that's using version < AORelationVersion_PG83,
	 * that is, was created before GPDB 5.0 and upgraded with pg_upgrade, we need
	 * to convert numeric attributes on the fly to new format. numericAtts
	 * is an array of attribute numbers (0-based), of all numeric columns (including
	 * domains over numerics). This array is created lazily when first needed.
	 */
	int			   *numericAtts;
	int				numNumericAtts;

	int				segmentFileNum;

	int64			totalRowsScannned;

	int64			blockFirstRowNum;
	int64			headerOffsetInFile;
	uint8			*dataBuffer;
	int32			dataLen;
	int 			executorBlockKind;
	int 			rowCount;
	bool			isLarge;
	bool			isCompressed;

	uint8			*uncompressedBuffer; /* for decompression */

	uint8			*largeContentBuffer;
	int32			largeContentBufferLen;

	VarBlockReader  varBlockReader;
	int				readerItemCount;
	int				currentItemCount;
	
	uint8			*singleRow;
	int32			singleRowLen;
} AppendOnlyExecutorReadBlock;

/*
 * used for scan of append only relations using BufferedRead and VarBlocks
 */
typedef struct AppendOnlyScanDescData
{
	/* scan parameters */
	Relation	aos_rd;				/* target relation descriptor */
	Snapshot	appendOnlyMetaDataSnapshot;

	/*
	 * Snapshot to use for non-metadata operations.
	 * Usually snapshot = appendOnlyMetaDataSnapshot, but they
	 * differ e.g. if gp_select_invisible is set.
	 */ 
	Snapshot    snapshot;

	Index       aos_scanrelid;
	int			aos_nkeys;			/* number of scan keys */
	ScanKey		aos_key;			/* array of scan key descriptors */
	
	/* file segment scan state */
	int			aos_filenamepath_maxlen;
	char		*aos_filenamepath;
									/* the current segment file pathname. */
	int			aos_total_segfiles;	/* the relation file segment number */
	int			aos_segfiles_processed; /* num of segfiles already processed */
	FileSegInfo **aos_segfile_arr;	/* array of all segfiles information */
	bool		aos_need_new_segfile;
	bool		aos_done_all_segfiles;
	
	MemoryContext	aoScanInitContext; /* mem context at init time */

	int32			usableBlockSize;
	int32			maxDataLen;

	AppendOnlyExecutorReadBlock	executorReadBlock;

	/* current scan state */
	bool		bufferDone;

	bool	initedStorageRoutines;

	AppendOnlyStorageAttributes	storageAttributes;
	AppendOnlyStorageRead		storageRead;

	char						*title;
				/*
				 * A phrase that better describes the purpose of the this open.
				 *
				 * We manage the storage for this.
				 */
	
	/*
	 * The block directory info.
	 *
	 * For AO tables, the block directory is built during the first index
	 * creation. If set indicates whether to build block directory while
	 * scanning.
	 */
	AppendOnlyBlockDirectory *blockDirectory;

	/**
	 * The visibility map is used during scans
	 * to check tuple visibility using visi map.
	 */ 
	AppendOnlyVisimap visibilityMap;

}	AppendOnlyScanDescData;

typedef AppendOnlyScanDescData *AppendOnlyScanDesc;

/*
 * Statistics on the latest fetch.
 */
typedef struct AppendOnlyFetchDetail
{
	int64		rangeFileOffset;
	int64		rangeFirstRowNum;
	int64		rangeAfterFileOffset;
	int64		rangeLastRowNum;
					/*
					 * The range covered by the Block Directory.
					 */
	
	int64		skipBlockCount;
					/*
					 * Number of blocks skipped since the previous block processed in
					 * the range.
					 */
	
	int64		blockFileOffset;
	int32		blockOverallLen;
	int64		blockFirstRowNum;
	int64		blockLastRowNum;
	bool		isCompressed;
	bool		isLargeContent;
					/*
					 * The last block processed.
					 */

} AppendOnlyFetchDetail;


/*
 * Used for fetch individual tuples from specified by TID of append only relations 
 * using the AO Block Directory, BufferedRead and VarBlocks
 */
typedef struct AppendOnlyFetchDescData
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

	AppendOnlyStorageAttributes	storageAttributes;
	AppendOnlyStorageRead		storageRead;

	char						*title;
				/*
				 * A phrase that better describes the purpose of the this open.
				 *
				 * We manage the storage for this.
				 */


	int				totalSegfiles;
	FileSegInfo 	**segmentFileInfo;

	char			*segmentFileName;
	int				segmentFileNameMaxLen;

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

	int32			usableBlockSize;

	AppendOnlyBlockDirectory	blockDirectory;

	AppendOnlyExecutorReadBlock executorReadBlock;

	CurrentSegmentFile currentSegmentFile;
	
	int64		scanNextFileOffset;
	int64		scanNextRowNum;

	int64		scanAfterFileOffset;
	int64		scanLastRowNum;

	CurrentBlock currentBlock;

	int64	skipBlockCount;

	AppendOnlyVisimap visibilityMap;

}	AppendOnlyFetchDescData;

typedef AppendOnlyFetchDescData *AppendOnlyFetchDesc;

typedef struct AppendOnlyUpdateDescData *AppendOnlyUpdateDesc;
typedef struct AppendOnlyDeleteDescData *AppendOnlyDeleteDesc;


/* ----------------
 *		function prototypes for appendonly access method
 * ----------------
 */

extern AppendOnlyScanDesc appendonly_beginscan(Relation relation,
											   Snapshot snapshot,
											   Snapshot appendOnlyMetaDataSnapshot,
											   int nkeys, 
											   ScanKey key);
extern AppendOnlyScanDesc appendonly_beginrangescan(Relation relation, 
		Snapshot snapshot,
		Snapshot appendOnlyMetaDataSnapshot, 
		int *segfile_no_arr, int segfile_count,
		int nkeys, ScanKey keys);
extern void appendonly_rescan(AppendOnlyScanDesc scan, ScanKey key);
extern void appendonly_endscan(AppendOnlyScanDesc scan);
extern bool appendonly_getnext(AppendOnlyScanDesc scan,
							   ScanDirection direction,
							   TupleTableSlot *slot);
extern AppendOnlyFetchDesc appendonly_fetch_init(
	Relation 	relation,
	Snapshot    snapshot,
	Snapshot 	appendOnlyMetaDataSnapshot);
extern bool appendonly_fetch(
	AppendOnlyFetchDesc aoFetchDesc,
	AOTupleId *aoTid,
	TupleTableSlot *slot);
extern void appendonly_fetch_finish(AppendOnlyFetchDesc aoFetchDesc);
extern AppendOnlyInsertDesc appendonly_insert_init(Relation rel, int segno, bool update_mode);
extern Oid appendonly_insert(
		AppendOnlyInsertDesc aoInsertDesc, 
		MemTuple instup, 
		Oid tupleOid,
		AOTupleId *aoTupleId);
extern void appendonly_insert_finish(AppendOnlyInsertDesc aoInsertDesc);
extern BlockNumber RelationGuessNumberOfBlocks(double totalbytes);

extern AppendOnlyDeleteDesc appendonly_delete_init(Relation rel, Snapshot appendOnlyMetaDataSnapshot);
extern HTSU_Result appendonly_delete(
		AppendOnlyDeleteDesc aoDeleteDesc,
		AOTupleId* aoTupleId);
extern void appendonly_delete_finish(AppendOnlyDeleteDesc aoDeleteDesc);

extern AppendOnlyUpdateDesc appendonly_update_init(Relation rel, Snapshot appendOnlyMetaDataSnapshot, int segno);
extern HTSU_Result appendonly_update(
		AppendOnlyUpdateDesc aoUpdateDesc,
		MemTuple memTuple,
		AOTupleId* aoTupleId,
		AOTupleId* newAoTupleId);

extern void appendonly_update_finish(AppendOnlyUpdateDesc aoUpdateDesc);

#endif   /* CDBAPPENDONLYAM_H */
