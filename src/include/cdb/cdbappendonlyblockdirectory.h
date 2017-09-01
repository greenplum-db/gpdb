/*------------------------------------------------------------------------------
 *
 * cdbappendonlyblockdirectory.h
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlyblockdirectory.h
 *
 *------------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYBLOCKDIRECTORY_H
#define CDBAPPENDONLYBLOCKDIRECTORY_H

#include "access/aosegfiles.h"
#include "access/aocssegfiles.h"
#include "access/appendonlytid.h"
#include "access/skey.h"

extern int gp_blockdirectory_entry_min_range;
extern int gp_blockdirectory_minipage_size;

typedef struct AppendOnlyBlockDirectoryEntry
{
	/*
	 * The range of blocks covered by the Block Directory entry.
	 */
	struct range
	{
		int64		fileOffset;
		int64		firstRowNum;

		int64		afterFileOffset;
		int64		lastRowNum;
	} range;

} AppendOnlyBlockDirectoryEntry;

/*
 * The entry in the minipage.
 */
typedef struct MinipageEntry
{
	int64 firstRowNum;
	int64 fileOffset;
	int64 rowCount;
} MinipageEntry;

/*
 * Define a varlena type for a minipage.
 */
typedef struct Minipage
{
	/* Total length. Must be the first. */
	int32 _len;
	int32 version;
	uint32 nEntry;
	
	/* Varlena array */
	MinipageEntry entry[1];
} Minipage;

/*
 * Define the relevant info for a minipage for each
 * column group.
 */
typedef struct MinipagePerColumnGroup
{
	Minipage *minipage;
	uint32 numMinipageEntries;
	ItemPointerData tupleTid;
} MinipagePerColumnGroup;

/*
 * I don't know the ideal value here. But let us put approximate
 * 8 minipages per heap page.
 */
#define NUM_MINIPAGE_ENTRIES (((MaxHeapTupleSize)/8 - sizeof(HeapTupleHeaderData) - 64 * 3)\
							  / sizeof(MinipageEntry))

/*
 * Define a structure for the append-only relation block directory.
 */
typedef struct AppendOnlyBlockDirectory
{
	Relation aoRel;
	Snapshot appendOnlyMetaDataSnapshot;
	Relation blkdirRel;
	Relation blkdirIdx;
	int numColumnGroups;
	bool isAOCol;
	bool *proj; /* projected columns, used only if isAOCol = TRUE */

	MemoryContext memoryContext;
	
	int				totalSegfiles;
	FileSegInfo 	**segmentFileInfo;

	/*
	 * Current segment file number.
	 */
	int currentSegmentFileNum;
	FileSegInfo *currentSegmentFileInfo;

	/*
	 * Last minipage that contains an array of MinipageEntries.
	 */
	MinipagePerColumnGroup *minipages;

	/*
	 * Some temporary space to help form tuples to be inserted into
	 * the block directory, and to help the index scan.
	 */
	Datum *values;
	bool *nulls;
	int numScanKeys;
	ScanKey scanKeys;
	StrategyNumber *strategyNumbers;

}	AppendOnlyBlockDirectory;


typedef struct CurrentBlock
{
	AppendOnlyBlockDirectoryEntry blockDirectoryEntry;

	bool have;

	int64 fileOffset;
	
	int32 overallBlockLen;
	
	int64 firstRowNum;
	int64 lastRowNum;
	
	bool isCompressed;
	bool isLargeContent;
	
	bool		gotContents;
} CurrentBlock;

typedef struct CurrentSegmentFile
{
	bool isOpen;
	
	int num;
	
	int64 logicalEof;
} CurrentSegmentFile;

extern void AppendOnlyBlockDirectoryEntry_GetBeginRange(
	AppendOnlyBlockDirectoryEntry	*directoryEntry,
	int64							*fileOffset,
	int64							*firstRowNum);
extern void AppendOnlyBlockDirectoryEntry_GetEndRange(
	AppendOnlyBlockDirectoryEntry	*directoryEntry,
	int64							*afterFileOffset,
	int64							*lastRowNum);
extern bool AppendOnlyBlockDirectoryEntry_RangeHasRow(
	AppendOnlyBlockDirectoryEntry	*directoryEntry,
	int64							checkRowNum);
extern bool AppendOnlyBlockDirectory_GetEntry(
	AppendOnlyBlockDirectory		*blockDirectory,
	AOTupleId 						*aoTupleId,
	int                             columnGroupNo,
	AppendOnlyBlockDirectoryEntry	*directoryEntry);
extern void AppendOnlyBlockDirectory_Init_forInsert(
	AppendOnlyBlockDirectory *blockDirectory,
	Snapshot appendOnlyMetaDataSnapshot,
	FileSegInfo *segmentFileInfo,
	int64 lastSequence,
	Relation aoRel,
	int segno,
	int numColumnGroups,
	bool isAOCol);
extern void AppendOnlyBlockDirectory_Init_forSearch(
	AppendOnlyBlockDirectory *blockDirectory,
	Snapshot appendOnlyMetaDataSnapshot,
	FileSegInfo **segmentFileInfo,
	int totalSegfiles,
	Relation aoRel,
	int numColumnGroups,
	bool isAOCol,
	bool *proj);
extern void AppendOnlyBlockDirectory_Init_addCol(
	AppendOnlyBlockDirectory *blockDirectory,
	Snapshot appendOnlyMetaDataSnapshot,
	FileSegInfo *segmentFileInfo,
	Relation aoRel,
	int segno,
	int numColumnGroups,
	bool isAOCol);
extern bool AppendOnlyBlockDirectory_InsertEntry(
	AppendOnlyBlockDirectory *blockDirectory,
	int columnGroupNo,
	int64 firstRowNum,
	int64 fileOffset,
	int64 rowCount,
	bool addColAction);
extern bool AppendOnlyBlockDirectory_addCol_InsertEntry(
	AppendOnlyBlockDirectory *blockDirectory,
	int columnGroupNo,
	int64 firstRowNum,
	int64 fileOffset,
	int64 rowCount);
extern bool AppendOnlyBlockDirectory_DeleteEntry(
	AppendOnlyBlockDirectory *blockDirectory,
	AOTupleId *aoTupleId);
extern bool AppendOnlyBlockDirectory_DeleteEntryForUpdate(
	AppendOnlyBlockDirectory *visibilityBlockDirectory,
	AppendOnlyBlockDirectory *insertBlockDirectory,
	AOTupleId* aoTupleId);
extern void AppendOnlyBlockDirectory_End_forInsert(
	AppendOnlyBlockDirectory *blockDirectory);
extern void AppendOnlyBlockDirectory_End_forSearch(
	AppendOnlyBlockDirectory *blockDirectory);
extern void AppendOnlyBlockDirectory_End_addCol(
	AppendOnlyBlockDirectory *blockDirectory);
extern void AppendOnlyBlockDirectory_DeleteSegmentFile(
	Relation aoRel,
		Snapshot snapshot,
		int segno,
		int columnGroupNo);
#endif
