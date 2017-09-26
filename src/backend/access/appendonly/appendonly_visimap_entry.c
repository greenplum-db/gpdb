/*------------------------------------------------------------------------------
 *
 * appendonly_visimap
 *   maintain a visibility bitmap entry
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonly_visimap_entry.c
 *
 *------------------------------------------------------------------------------
*/
#include "postgres.h"
#include "access/appendonly_visimap.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "utils/bitstream.h"
#include "utils/guc.h"
#include "utils/bitmap_compression.h"
#include "catalog/aovisimap.h"

/*
 * Frees the data allocated by the visimap entry.
 *
 * No other function should be called on the visibility map entry
 * after this function has been called.
 */
void
AppendOnlyVisimapEntry_Finish(
							  AppendOnlyVisimapEntry *visiMapEntry)
{
	Assert(visiMapEntry);

	if (visiMapEntry->data)
	{
		pfree(visiMapEntry->data);
		visiMapEntry->data = NULL;
	}
	bms_free(visiMapEntry->bitmap);
	visiMapEntry->bitmap = NULL;
}

/*
 * Inits the visimap entry data structure.
 ;
 * Assumes a zero-allocated visimap entry data structure.
 *
 * Until appendonly_visimap_copyout or appendonly_visimap_clear is called,
 * the data structure
 * is not usable for visibility checks or updates.
 */
void
AppendOnlyVisimapEntry_Init(
							AppendOnlyVisimapEntry *visiMapEntry,
							MemoryContext memoryContext)
{
	Assert(visiMapEntry);

	visiMapEntry->dirty = false;
	visiMapEntry->data = palloc0(APPENDONLY_VISIMAP_DATA_BUFFER_SIZE);
	SET_VARSIZE(visiMapEntry->data, 0);
	visiMapEntry->segmentFileNum = -1;
	visiMapEntry->firstRowNum = -1;
	visiMapEntry->memoryContext = memoryContext;
	visiMapEntry->bitmap = NULL;
}

/*
 * Resets the visibility map data structure.
 *
 * It puts the entry into the identical state as after a
 * call to AppendOnlyVisimapEntry_Init.
 */
void
AppendOnlyVisimapEntry_Reset(
							 AppendOnlyVisimapEntry *visiMapEntry)
{
	Assert(visiMapEntry);

	visiMapEntry->dirty = false;
	visiMapEntry->segmentFileNum = -1;
	visiMapEntry->firstRowNum = -1;

	bms_free(visiMapEntry->bitmap);
	visiMapEntry->bitmap = NULL;
}

/*
 * Initializes a previously unused entry that covers the given tuple id.
 * The tuple is not marked as updated as no state has been changed yet.
 *
 * Note that the firstRowNum is not the rowNum of the tuple id.
 */
void
AppendOnlyVisimapEntry_New(
						   AppendOnlyVisimapEntry *visiMapEntry,
						   AOTupleId *tupleId)
{
	Assert(visiMapEntry);
	Assert(tupleId);
	Assert(!visiMapEntry->dirty);

	bms_free(visiMapEntry->bitmap);
	visiMapEntry->bitmap = NULL;

	visiMapEntry->segmentFileNum = AOTupleIdGet_segmentFileNum(tupleId);
	visiMapEntry->firstRowNum = AppendOnlyVisimapEntry_GetFirstRowNum(visiMapEntry,
																	  tupleId);

	ItemPointerSetInvalid(&visiMapEntry->tupleTid);

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map entry: New entry "
		   "(segNum, firstRowNum) = (%u, " INT64_FORMAT ")",
		   visiMapEntry->segmentFileNum,
		   visiMapEntry->firstRowNum);
}

static Datum
AppendOnlyVisimap_GetAttrNotNull(HeapTuple t, TupleDesc td, int attr)
{
	Datum		d;
	bool		isNull;

	d = fastgetattr(t, attr, td, &isNull);
	if (isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid value: NULL")));
	return d;
}

void
AppendOnlyVisiMapEnty_ReadData(
							   AppendOnlyVisimapEntry *visiMapEntry, size_t dataSize)
{
	int			newWordCount;

	Assert(visiMapEntry);
	Assert(CurrentMemoryContext == visiMapEntry->memoryContext);

	BitmapDecompressState decompressState;

	BitmapDecompress_Init(&decompressState,
						  visiMapEntry->data->data,
						  dataSize);

	if (BitmapDecompress_HasError(&decompressState))
	{
		elog(ERROR, "error occured during visimap bitmap decompression");
	}

	bms_free(visiMapEntry->bitmap);

	newWordCount =
		BitmapDecompress_GetBlockCount(&decompressState);
	if (newWordCount > 0)
	{
		visiMapEntry->bitmap = palloc0(
									   offsetof(Bitmapset, words) + (newWordCount * sizeof(bitmapword)));
		visiMapEntry->bitmap->nwords = newWordCount;
		BitmapDecompress_Decompress(
									&decompressState,
									visiMapEntry->bitmap->words,
									newWordCount);
	}
	else if (newWordCount == 0)
	{
		visiMapEntry->bitmap = NULL;
	}
	else
	{
		elog(ERROR,
			 "illegal visimap block count: visimap block count %d", newWordCount);
	}

}

/*
 * Reads the visibility information from a aovisimap tuple.
 *
 * Should only be called with values and nulls provides
 * by a successful read from the aovisimap table using
 * an AppendOnlyVisimapIndex data structure.
 */
void
AppendOnlyVisimapEntry_Copyout(
							   AppendOnlyVisimapEntry *visiMapEntry,
							   HeapTuple tuple,
							   TupleDesc tupleDesc)
{
	struct varlena *value;
	struct varlena *detoast_value;
	MemoryContext oldContext;
	size_t		dataSize;
	Datum		d;
	bool		isNull;

	Assert(visiMapEntry);
	Assert(!visiMapEntry->dirty);
	Assert(tuple);
	Assert(tupleDesc);
	Assert(!visiMapEntry->dirty);	/* entry should not contain dirty data */

	d = AppendOnlyVisimap_GetAttrNotNull(tuple, tupleDesc, Anum_pg_aovisimap_segno);
	visiMapEntry->segmentFileNum = DatumGetInt64(d);

	d = AppendOnlyVisimap_GetAttrNotNull(tuple, tupleDesc, Anum_pg_aovisimap_firstrownum);
	visiMapEntry->firstRowNum = DatumGetInt64(d);

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map entry: copy out: "
		   "segNo %u firstRowNum " INT64_FORMAT,
		   visiMapEntry->segmentFileNum,
		   visiMapEntry->firstRowNum);

	d = fastgetattr(tuple, Anum_pg_aovisimap_visimap, tupleDesc, &isNull);
	if (isNull)
	{
		/*
		 * when the visimap data is NULL, all entries are visible.
		 */
		bms_free(visiMapEntry->bitmap);
		visiMapEntry->bitmap = NULL;
	}
	else
	{
		value = (struct varlena *) DatumGetPointer(d);
		detoast_value = pg_detoast_datum(value);

		oldContext = MemoryContextSwitchTo(visiMapEntry->memoryContext);

		/* Reuse the data buffer if possible */
		Assert(visiMapEntry->data);
		Assert(APPENDONLY_VISIMAP_DATA_BUFFER_SIZE >= VARSIZE(detoast_value));
		memcpy(visiMapEntry->data, detoast_value, VARSIZE(detoast_value));

		dataSize = VARSIZE(detoast_value) -
			offsetof(AppendOnlyVisimapData, data);
		AppendOnlyVisiMapEnty_ReadData(visiMapEntry, dataSize);

		MemoryContextSwitchTo(oldContext);

		if (detoast_value != value)
		{
			pfree(detoast_value);
			detoast_value = NULL;
		}
	}
}

/*
 * Returns the hidden tuple count value from a visimap entry heap tuple.
 *
 */
int64
AppendOnlyVisimapEntry_GetHiddenTupleCount(
										   AppendOnlyVisimapEntry *visiMapEntry)
{
	Assert(visiMapEntry);

	return bms_num_members(visiMapEntry->bitmap);
}

void
AppendOnlyVisimapEntry_WriteData(
								 AppendOnlyVisimapEntry *visiMapEntry)
{
	int			bitmapSize,
				compressedBitmapSize;

	Assert(visiMapEntry);
	Assert(CurrentMemoryContext == visiMapEntry->memoryContext);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));

	bitmapSize = (visiMapEntry->bitmap ? (visiMapEntry->bitmap->nwords * sizeof(uint32)) : 0);
	bitmapSize += BITMAP_COMPRESSION_HEADER_SIZE;

	Assert(visiMapEntry->data);
	Assert(APPENDONLY_VISIMAP_DATA_BUFFER_SIZE >= bitmapSize);
	visiMapEntry->data->version = 1;

	compressedBitmapSize = Bitmap_Compress(
										   BITMAP_COMPRESSION_TYPE_DEFAULT,
										   (visiMapEntry->bitmap ? visiMapEntry->bitmap->words : NULL),
										   (visiMapEntry->bitmap ? visiMapEntry->bitmap->nwords : 0),
										   visiMapEntry->data->data,
										   bitmapSize);
	Assert(compressedBitmapSize >= BITMAP_COMPRESSION_HEADER_SIZE);
	SET_VARSIZE(visiMapEntry->data,
				offsetof(AppendOnlyVisimapData, data) + compressedBitmapSize);

}

/**
 * Persist the entry information to heap tuple value/nulls.
 * Should only be called after a call to AppendOnlyVisimapEntry_copyout
 * or AppendOnlyVisimapEntry_clear.
 *
 * May be called when visimap entry is not updated. However, that is usually
 * wasteful.
 */
void
AppendOnlyVisimapEntry_Write(
							 AppendOnlyVisimapEntry *visiMapEntry,
							 Datum *values,
							 bool *nulls)
{
	MemoryContext oldContext;

	Assert(visiMapEntry);
	Assert(values);
	Assert(nulls);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map entry: write (segno, firstRowNum) = "
		   "(%d, " INT64_FORMAT ")",
		   visiMapEntry->segmentFileNum, visiMapEntry->firstRowNum);

	values[Anum_pg_aovisimap_segno - 1] = Int32GetDatum(visiMapEntry->segmentFileNum);
	nulls[Anum_pg_aovisimap_segno - 1] = false;
	values[Anum_pg_aovisimap_firstrownum - 1] = Int64GetDatum(visiMapEntry->firstRowNum);
	nulls[Anum_pg_aovisimap_firstrownum - 1] = false;

	if (bms_is_empty(visiMapEntry->bitmap))
	{
		nulls[Anum_pg_aovisimap_visimap - 1] = true;
	}
	else
	{
		nulls[Anum_pg_aovisimap_visimap - 1] = false;

		oldContext = MemoryContextSwitchTo(visiMapEntry->memoryContext);

		AppendOnlyVisimapEntry_WriteData(visiMapEntry);
		values[Anum_pg_aovisimap_visimap - 1] = PointerGetDatum(visiMapEntry->data);

		MemoryContextSwitchTo(oldContext);
	}
	visiMapEntry->dirty = false;
}

/*
 * Returns true iff all entries in the visimap entry are visible.
 */
static bool
AppendOnlyVisimapEntry_AreAllVisible(
									 AppendOnlyVisimapEntry *visiMapEntry)
{
	Assert(visiMapEntry);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));

	return bms_is_empty(visiMapEntry->bitmap);
}

/**
 * Helper function to get the rownum offset (from the beginning of the
 * visibility map entry).
 *
 * Assumes that the current visibility map entry covers the row number.
 */
static void
AppendOnlyVisimapEntry_GetRownumOffset(
									   AppendOnlyVisimapEntry *visiMapEntry,
									   int64 rowNum,
									   int64 *rowNumOffset)
{
	Assert(visiMapEntry);
	Assert(rowNum >= 0);
	Assert(rowNumOffset);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));

	*rowNumOffset = rowNum - visiMapEntry->firstRowNum;
	Assert(rowNumOffset >= 0);
}

/*
 * Returns true iff the current visibility map entry covers
 * the tuple id.
 *
 * Should only be called with a initialized, not-finished visi map entry.
 *
 * May be called before AppendOnlyVisimapEntry_Copyout or
 * AppendOnlyVisimapEntry_New. In this case, the visimap entry
 * does not cover the tuple.
 */
bool
AppendOnlyVisimapEntry_CoversTuple(
								   AppendOnlyVisimapEntry *visiMapEntry,
								   AOTupleId *tupleId)
{
	int			rowNum;

	Assert(visiMapEntry);
	Assert(tupleId);

	if (!AppendOnlyVisimapEntry_IsValid(visiMapEntry))
	{
		return false;
	}
	if (visiMapEntry->segmentFileNum !=
		AOTupleIdGet_segmentFileNum(tupleId))
	{
		return false;
	}
	rowNum = AOTupleIdGet_rowNum(tupleId);
	return (visiMapEntry->firstRowNum <= rowNum)
		&& ((visiMapEntry->firstRowNum + APPENDONLY_VISIMAP_MAX_RANGE) > rowNum);
}

/*
 * Returns the matching first row number of a given
 * AO tuple id.
 */
int64
AppendOnlyVisimapEntry_GetFirstRowNum(
									  AppendOnlyVisimapEntry *visiMapEntry,
									  AOTupleId *tupleId)
{
	(void) visiMapEntry;
	int			rowNum;

	rowNum = AOTupleIdGet_rowNum(tupleId);
	return (rowNum / APPENDONLY_VISIMAP_MAX_RANGE) * APPENDONLY_VISIMAP_MAX_RANGE;
}

/**
 * Checks if a row is visible (according to the bitmap).
 *
 * Should only be called after a call to AppendOnlyVisimapEntry_Copyout or
 * AppendOnlyVisimapEntry_New.
 * Should only be called if current visimap entry covers the tuple id.
 *
 * The final visibility also depends on other information, e.g. if the
 * original transaction has been aborted. Such information is
 * not stored in the visimap.
 */
bool
AppendOnlyVisimapEntry_IsVisible(
								 AppendOnlyVisimapEntry *visiMapEntry,
								 AOTupleId *tupleId)
{
	int64		rowNum,
				rowNumOffset;
	bool		visibilityBit;

	Assert(visiMapEntry);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));
	Assert(AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId));

	rowNum = AOTupleIdGet_rowNum(tupleId);

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map entry: Check row visibility: "
		   "firstRowNum " INT64_FORMAT ", rowNum " INT64_FORMAT,
		   visiMapEntry->firstRowNum, rowNum);

	if (AppendOnlyVisimapEntry_AreAllVisible(visiMapEntry))
	{
		elogif(Debug_appendonly_print_visimap, LOG,
			   "Append-only visi map entry: All entries are visibile: "
			   "(firstRowNum, rowNum) = (" INT64_FORMAT ", " INT64_FORMAT ")",
			   visiMapEntry->firstRowNum, rowNum);
		return true;
	}
	Assert(rowNum >= visiMapEntry->firstRowNum);

	rowNumOffset = 0;
	AppendOnlyVisimapEntry_GetRownumOffset(visiMapEntry,
										   rowNum, &rowNumOffset);

	visibilityBit = !bms_is_member(rowNumOffset,
								   visiMapEntry->bitmap);

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map entry: (firstRowNum, rowNum, visible) = "
		   "(" INT64_FORMAT ", " INT64_FORMAT ", %d)",
		   visiMapEntry->firstRowNum, rowNum, (int) visibilityBit);

	return visibilityBit;
}

/*
 * The minimal size (in uint32's elements) the entry array needs to have to
 * cover the given offset
 */
static uint32
AppendOnlyVisimapEntry_GetMinimalSizeToCover(int64 offset)
{
	uint32		minSize;

	Assert(offset >= 0);

	minSize = (offset / BITS_PER_BITMAPWORD) + 1;

	/* Round up to the nearest multiple of two */
	minSize--;
	minSize |= minSize >> 1;
	minSize |= minSize >> 2;
	minSize |= minSize >> 4;
	minSize |= minSize >> 8;
	minSize |= minSize >> 16;
	minSize++;
	return minSize;
}

/**
 * Hides the given tuple id in the bitmap.
 *
 * Should only be called if the current entry covers the tuple id.
 *
 * This function is only modifying the bitmap. The caller needs to take
 * care that change is persisted.
 */
HTSU_Result
AppendOnlyVisimapEntry_HideTuple(
								 AppendOnlyVisimapEntry *visiMapEntry,
								 AOTupleId *tupleId)
{
	int64		rowNum,
				rowNumOffset;
	MemoryContext oldContext;
	HTSU_Result result;

	Assert(visiMapEntry);
	Assert(tupleId);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));
	Assert(AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId));

	rowNum = AOTupleIdGet_rowNum(tupleId);

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map entry: Hide tuple: "
		   "firstRowNum " INT64_FORMAT ", rowNum " INT64_FORMAT,
		   visiMapEntry->firstRowNum, rowNum);

	rowNumOffset = 0;
	AppendOnlyVisimapEntry_GetRownumOffset(visiMapEntry,
										   rowNum,
										   &rowNumOffset);

	oldContext = MemoryContextSwitchTo(visiMapEntry->memoryContext);

	/*
	 * enlarge the bitmap by a power of two. this avoids the O(n*n) resizing
	 * policy of the original bitmap set
	 */
	if (!bms_covers_member(visiMapEntry->bitmap, rowNumOffset))
		visiMapEntry->bitmap =
			bms_resize(visiMapEntry->bitmap,
					   AppendOnlyVisimapEntry_GetMinimalSizeToCover(rowNumOffset));

	if (!bms_is_member(rowNumOffset, visiMapEntry->bitmap))
	{
		visiMapEntry->bitmap = bms_add_member(visiMapEntry->bitmap, rowNumOffset);
		result = HeapTupleMayBeUpdated;
	}
	else if (visiMapEntry->dirty)
	{
		/* The bit was already set and it was this command */
		result = HeapTupleSelfUpdated;
	}
	else
	{
		/* The bit was already set before */
		result = HeapTupleUpdated;
	}

	MemoryContextSwitchTo(oldContext);
	visiMapEntry->dirty = true;

	return result;
}

/**
 * Returns true iff the visi map entry needs to be persisted.
 */
bool
AppendOnlyVisimapEntry_HasChanged(
								  AppendOnlyVisimapEntry *visiMapEntry)
{
	Assert(visiMapEntry);
	return visiMapEntry->dirty;
}

/*
 * Returns true iff the entry contains valid data.
 * That is either CopyOut or New has been called.
 */
bool
AppendOnlyVisimapEntry_IsValid(
							   AppendOnlyVisimapEntry *visiMapEntry)
{
	Assert(visiMapEntry);
	return (visiMapEntry->segmentFileNum >= 0 &&
			visiMapEntry->firstRowNum >= 0);
}

/*
 * Return the next invisible tuple id in the given visibility map.
 */
bool
AppendOnlyVisimapEntry_GetNextInvisible(
										AppendOnlyVisimapEntry *visiMapEntry,
										AOTupleId *tupleId)
{
	int64		currentBitmapOffset,
				rowNum;
	int			offset;

	Assert(visiMapEntry);
	Assert(AppendOnlyVisimapEntry_IsValid(visiMapEntry));
	Assert(tupleId);

	currentBitmapOffset = -1;	/* before the first */
	if (AppendOnlyVisimapEntry_CoversTuple(
										   visiMapEntry, tupleId))
	{
		AppendOnlyVisimapEntry_GetRownumOffset(
											   visiMapEntry,
											   AOTupleIdGet_rowNum(tupleId),
											   &currentBitmapOffset);
	}
	currentBitmapOffset++;

	offset = bms_first_from(visiMapEntry->bitmap,
							currentBitmapOffset);
	if (offset >= 0)
	{
		rowNum = visiMapEntry->firstRowNum + offset;
		AOTupleIdInit_Init(tupleId);
		AOTupleIdInit_segmentFileNum(
									 tupleId,
									 visiMapEntry->segmentFileNum);
		AOTupleIdInit_rowNum(
							 tupleId,
							 rowNum);
		return true;
	}
	else
	{
		return false;
	}
}
