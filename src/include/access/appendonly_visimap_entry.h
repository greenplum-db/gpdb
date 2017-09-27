/*------------------------------------------------------------------------------
 *
 * appendonly_visimap_entry
 *   responsibly for the internal handling and the usage
 *   of a single tuple in the visimap relation.
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/appendonly_visimap_entry.h
 *
 *------------------------------------------------------------------------------
*/
#ifndef APPENDONLY_VISIMAP_ENTRY_H
#define APPENDONLY_VISIMAP_ENTRY_H

#include "postgres.h"
#include "access/appendonlytid.h"
#include "access/htup.h"
#include "nodes/bitmapset.h"
#include "utils/tqual.h"
#include "utils/bitmap_compression.h"

#define APPENDONLY_VISIMAP_DATA_BUFFER_SIZE (offsetof(AppendOnlyVisimapData, data) + \
			APPENDONLY_VISIMAP_MAX_BITMAP_SIZE + \
			BITMAP_COMPRESSION_HEADER_SIZE)

/*
 * On-disk (or in-tuple) data structure of the visibility map
 * data.
 */
typedef struct AppendOnlyVisimapData
{
	/*
	 * Total length. Must be the first. Will later be set by
	 * SET_VARDATA_SIZE()
	 */
	int32		_len;

	/*
	 * Version number for the VisiMap format. Currently only 1 is supported.
	 */
	int32		version;

	/* Possibily compressed bitmap data */
	unsigned char data[1];
} AppendOnlyVisimapData;

typedef struct AppendOnlyVisimapEntry
{
	/*
	 * Current on-disk representation of the visibility entry.
	 *
	 * The contents is only defined to offsets smaller than VARSIZE(data);
	 *
	 */
	AppendOnlyVisimapData *data;

	/*
	 * Current uncompressed bitmap for the visibility map entry.
	 */
	Bitmapset  *bitmap;

	/*
	 * Current segment file number. -1 indicates not set.
	 */
	int32		segmentFileNum;

	/*
	 * first row number covered by the current visibility map entry. -1
	 * indicates not set.
	 */
	int64		firstRowNum;

	/*
	 * true if the entry has been changed and needs to be persisted.
	 */
	bool		dirty;

	/**
	 * tuple id of the last loaded visibility map entry.
	 * Is Invalid iff there is no current table or if the
	 * current table has not been loaded using AppendOnlyVisimapStore_Find.
	 */
	ItemPointerData tupleTid;

	/*
	 * Memory context to use for all allocations.
	 */
	MemoryContext memoryContext;

} AppendOnlyVisimapEntry;

void AppendOnlyVisimapEntry_Init(
							AppendOnlyVisimapEntry *visi_map_entry,
							MemoryContext memoryContext);

void AppendOnlyVisimapEntry_Reset(
							 AppendOnlyVisimapEntry *visi_map_entry);

void AppendOnlyVisimapEntry_Copyout(
							   AppendOnlyVisimapEntry *visi_map_entry,
							   HeapTuple tuple,
							   TupleDesc tupleDesc);

void AppendOnlyVisimapEntry_New(
						   AppendOnlyVisimapEntry *visiMapEntry,
						   AOTupleId *tupleId);

void AppendOnlyVisimapEntry_Write(
							 AppendOnlyVisimapEntry *visiMapEntry,
							 Datum *value,
							 bool *is_null);

void AppendOnlyVisimapEntry_WriteData(
								 AppendOnlyVisimapEntry *visiMapEntry);

bool AppendOnlyVisimapEntry_CoversTuple(
								   AppendOnlyVisimapEntry *visiMapEntry,
								   AOTupleId *aoTupleId);

bool AppendOnlyVisimapEntry_IsVisible(
								 AppendOnlyVisimapEntry *visiMapEntry,
								 AOTupleId *aoTupleId);

HTSU_Result AppendOnlyVisimapEntry_HideTuple(
								 AppendOnlyVisimapEntry *visiMapEntry,
								 AOTupleId *aoTupleId);

void AppendOnlyVisimapEntry_Finish(
							  AppendOnlyVisimapEntry *visiMapEntry);

bool AppendOnlyVisimapEntry_HasChanged(
								  AppendOnlyVisimapEntry *visiMapEntry);

bool AppendOnlyVisimapEntry_IsValid(
							   AppendOnlyVisimapEntry *visiMapEntry);

int64 AppendOnlyVisimapEntry_GetFirstRowNum(
									  AppendOnlyVisimapEntry *visiMapEntry,
									  AOTupleId *tupleId);

bool AppendOnlyVisimapEntry_GetNextInvisible(
										AppendOnlyVisimapEntry *visiMapEntry,
										AOTupleId *tupleId);

int64 AppendOnlyVisimapEntry_GetHiddenTupleCount(
										   AppendOnlyVisimapEntry *visiMapEntry);

void AppendOnlyVisiMapEnty_ReadData(
							   AppendOnlyVisimapEntry *visiMapEntry, size_t dataSize);
#endif
