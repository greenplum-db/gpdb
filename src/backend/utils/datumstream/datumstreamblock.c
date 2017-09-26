/*-------------------------------------------------------------------------
 *
 * datumstreamblock.c
 *
 * Portions Copyright (c) 2011, EMC, Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/datumstream/datumstreamblock.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "access/tupmacs.h"
#include "access/tuptoaster.h"
#include "utils/datumstreamblock.h"
#include "utils/guc.h"

/*	Forwards. */
static char *VarlenaInfoToBuffer(char *buffer, uint8 * p);

static void DatumStreamBlock_IntegrityCheckOrig(
									uint8 * buffer,
									int32 bufferSize,
									bool minimalIntegrityChecks,
									int32 expectedRowCount,
									DatumStreamTypeInfo * typeInfo,
							   int (*errdetailCallback) (void *errdetailArg),
									void *errdetailArg,
							 int (*errcontextCallback) (void *errcontextArg),
									void *errcontextArg);

static void DatumStreamBlock_IntegrityCheckDense(
									 uint8 * buffer,
									 int32 bufferSize,
									 bool minimalIntegrityChecks,
									 int32 expectedRowCount,
									 DatumStreamTypeInfo * typeInfo,
							   int (*errdetailCallback) (void *errdetailArg),
									 void *errdetailArg,
							 int (*errcontextCallback) (void *errcontextArg),
									 void *errcontextArg);

/* Proper align with zero padding */
static inline char *
att_align_zero(char *data, char alignchar)
{
    size_t  misalignment = (size_t)att_align_nominal(1, alignchar) - 1;

    while ((size_t)data & misalignment)
		*(data++) = 0;

	return data;
}

/*
 * DatumStreamBlockRead.
 */
/*	PERFORMANCE EXPERIMENT: Only do integrity and trace checking for DEBUG builds... */
#ifdef USE_ASSERT_CHECKING
void
DatumStreamBlockRead_PrintVarlenaInfo(
									  DatumStreamBlockRead * dsr,
									  uint8 * p)
{
	elog(LOG, "Read varlena <%s>",
		 VarlenaInfoToString(p));
}
#endif

int
errdetail_datumstreamblockread(
							   DatumStreamBlockRead * dsr)
{
	dsr->errdetailCallback(dsr->errcontextArg);

	return 0;
}

static int
errdetail_datumstreamblockread_callback(
										void *arg)
{
	DatumStreamBlockRead *dsr = (DatumStreamBlockRead *) arg;

	if (strncmp(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockRead data structure not valid (eyecatcher)");

	return errdetail_datumstreamblockread(dsr);
}

int
errcontext_datumstreamblockread(
								DatumStreamBlockRead * dsr)
{
	dsr->errcontextCallback(dsr->errcontextArg);

	return 0;
}

static int
errcontext_datumstreamblockread_callback(
										 void *arg)
{
	DatumStreamBlockRead *dsr = (DatumStreamBlockRead *) arg;

	if (strncmp(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockRead data structure not valid (eyecatcher)");

	return errcontext_datumstreamblockread(dsr);
}

void
DatumStreamBlockRead_ResetOrig(DatumStreamBlockRead * dsr)
{
	/*
	 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for DEBUG
	 * builds...
	 */
#ifdef USE_ASSERT_CHECKING
	if (strncmp(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockRead data structure not valid (eyecatcher)");
#endif

	/*
	 * Put before first entry.	Caller will advance.
	 */
	dsr->nth = -1;
	dsr->physical_datum_index = -1;

	/*
	 * Reset everything else.
	 */
	dsr->physical_datum_count = 0;
	dsr->physical_data_size = 0;
	dsr->logical_row_count = 0;
	dsr->has_null = false;

	dsr->null_bitmap_beginp = NULL;
	dsr->datum_beginp = NULL;
	dsr->datum_afterp = NULL;

	dsr->buffer_beginp = NULL;
	dsr->datump = NULL;
}

void
DatumStreamBlockRead_GetReadyOrig(
								  DatumStreamBlockRead * dsr,
								  uint8 * buffer,
								  int32 bufferSize,
								  int64 firstRowNum,
								  int32 rowCount,
								  bool *hadToAdjustRowCount,
								  int32 * adjustedRowCount)
{
	uint8	   *p;

	int32		unalignedHeaderSize;
	int32		alignedHeaderSize;
	bool		minimalIntegrityChecks;

	DatumStreamBlock_Orig *blockOrig;
	int32		minHeaderSize = sizeof(DatumStreamBlock_Orig);

	/*
	 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for DEBUG
	 * builds...
	 */
#ifdef USE_ASSERT_CHECKING
	if (strncmp(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockRead data structure not valid (eyecatcher)");
#endif

	*hadToAdjustRowCount = false;
	*adjustedRowCount = 0;

#ifdef USE_ASSERT_CHECKING
	minimalIntegrityChecks = false;
#else
	minimalIntegrityChecks = true;
#endif
	if (Debug_datumstream_block_read_check_integrity)
	{
		minimalIntegrityChecks = false;
	}

	dsr->buffer_beginp = buffer;

	/*
	 * Read header information and setup for reading the datum in the block.
	 */

	/*
	 * Advance pointer 'p' as we discover the contents of this block.
	 */
	p = dsr->buffer_beginp;

	if (bufferSize < minHeaderSize)
	{
		ereport(ERROR,
				(errmsg("Bad datum stream Original block header size.  Found %d and expected the size to be at least %d",
						bufferSize,
						minHeaderSize),
				 errdetail_datumstreamblockread(dsr),
				 errcontext_datumstreamblockread(dsr)));
	}

	blockOrig = (DatumStreamBlock_Orig *) p;
	if (blockOrig->version != DatumStreamVersion_Original)
	{
		ereport(ERROR,
				(errmsg("Bad datum stream Original block version.  Found %d and expected %d",
						blockOrig->version,
						DatumStreamVersion_Original),
				 errdetail_datumstreamblockread(dsr),
				 errcontext_datumstreamblockread(dsr)));
	}

	if (!minimalIntegrityChecks)
	{
		DatumStreamBlock_IntegrityCheckOrig(
											buffer,
											bufferSize,
											minimalIntegrityChecks,
											rowCount,
											&dsr->typeInfo,
			 /* errdetailCallback */ errdetail_datumstreamblockread_callback,
											 /* errdetailArg */ (void *) dsr,
		   /* errcontextCallback */ errcontext_datumstreamblockread_callback,
										   /* errcontextArg */ (void *) dsr);
	}

	dsr->logical_row_count = blockOrig->ndatum;

	dsr->physical_datum_count = 0;
	/* Not used for Original */
	dsr->physical_data_size = blockOrig->sz;

	p += sizeof(DatumStreamBlock_Orig);

	/* Set up acc */
	Assert(dsr->nth == -1);
	Assert(dsr->physical_datum_index == -1);

	dsr->has_null = ((blockOrig->flags & DSB_HAS_NULLBITMAP) != 0);
	if (dsr->has_null)
	{
		dsr->null_bitmap_beginp = p;

		DatumStreamBitMapRead_Init(
								   &dsr->null_bitmap,
								   dsr->null_bitmap_beginp,
								   dsr->logical_row_count);
		/*
		 * The field nullsz byte length is MAXALIGN.
		 */
		p += blockOrig->nullsz;
	}

	/*
	 * Pre-4.0 code did not enforce the following assertion.
	 * We identify this exception through firstRow, which is -1
	 * for pre-4.0 blocks (not stored inside the block).
	 */
	Assert(dsr->logical_row_count == rowCount || firstRowNum == -1);

	/*
		 * In pre-4.0 blocks, the maximum value of ndatum is
	 * 32000 while rowCnt cannot exceed 16384, so we set
	 * rowCnt to ndatum to build the block directory
	 * consistently.
	 */
	if (dsr->logical_row_count > rowCount && firstRowNum == -1)
	{
		/* Adjust !!!! */
		*adjustedRowCount = dsr->logical_row_count;
		*hadToAdjustRowCount = true;
	}

	unalignedHeaderSize = (int32) (p - dsr->buffer_beginp);
	alignedHeaderSize = MAXALIGN(unalignedHeaderSize);

	dsr->datum_beginp = dsr->buffer_beginp + alignedHeaderSize;
	dsr->datum_afterp = dsr->datum_beginp + dsr->physical_data_size;

	/*
	 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for DEBUG
	 * builds...
	 */
#ifdef USE_ASSERT_CHECKING
	if (Debug_appendonly_print_scan)
	{
		if (!dsr->has_null)
		{
			ereport(LOG,
			 (errmsg("Datum stream block read unpack Original with NO NULLs "
					 "(logical row count %d, physical data size = %d, "
					 "adjustedRowCount %d, row count adjusted %s, "
					 "unaligned header size %d, aligned header size %d, "
					 "datum begin %p, datum after %p)",
					 dsr->logical_row_count,
					 dsr->physical_data_size,
					 *adjustedRowCount,
					 (*hadToAdjustRowCount ? "true" : "false"),
					 unalignedHeaderSize,
					 alignedHeaderSize,
					 dsr->datum_beginp,
					 dsr->datum_afterp),
			  errdetail_datumstreamblockread(dsr),
			  errcontext_datumstreamblockread(dsr)));
		}
		else
		{
			ereport(LOG,
				(errmsg("Datum stream block read unpack Original with NULLs "
						"(logical row count %d, physical data size = %d, "
						"NULL bit-map MAXALIGN size %d, "
						"adjustedRowCount %d, row count adjusted %s, "
						"unaligned header size %d, aligned header size %d, "
						"null begin %p, datum begin %p, datum after %p)",
						dsr->logical_row_count,
						dsr->physical_data_size,
						blockOrig->nullsz,
						*adjustedRowCount,
						(*hadToAdjustRowCount ? "true" : "false"),
						unalignedHeaderSize,
						alignedHeaderSize,
						dsr->null_bitmap_beginp,
						dsr->datum_beginp,
						dsr->datum_afterp),
				 errdetail_datumstreamblockread(dsr),
				 errcontext_datumstreamblockread(dsr)));
		}
	}
#endif

	dsr->datump = dsr->datum_beginp;
}

void
DatumStreamBlockRead_Init(
						  DatumStreamBlockRead * dsr,
						  DatumStreamTypeInfo * typeInfo,
						  DatumStreamVersion datumStreamVersion,
						  bool rle_can_have_compression,
						  int (*errdetailCallback) (void *errdetailArg),
						  void *errdetailArg,
						  int (*errcontextCallback) (void *errcontextArg),
						  void *errcontextArg)
{
	memcpy(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen);

	memcpy(&dsr->typeInfo, typeInfo, sizeof(DatumStreamTypeInfo));

	dsr->datumStreamVersion = datumStreamVersion;

	dsr->rle_can_have_compression = rle_can_have_compression;

	dsr->errdetailCallback = errdetailCallback;
	dsr->errcontextArg = errcontextArg;
	dsr->errcontextCallback = errcontextCallback;
	dsr->errcontextArg = errcontextArg;

	dsr->memctxt = CurrentMemoryContext;

	Assert(dsr->physical_data_size == 0);

	Assert(dsr->null_bitmap_beginp == NULL);
	Assert(dsr->datum_beginp == NULL);
	Assert(dsr->rle_compress_beginp == NULL);
	Assert(dsr->rle_repeatcountsp == NULL);

	Assert(dsr->nth == 0);
	Assert(dsr->physical_datum_index == 0);
	Assert(dsr->physical_datum_count == 0);

	Assert(!dsr->has_null);

	Assert(dsr->datump == NULL);

	Assert(!dsr->rle_block_was_compressed);

	Assert(dsr->rle_repeatcounts_index == 0);
	Assert(!dsr->rle_in_repeated_item);
	Assert(dsr->rle_repeated_item_count == 0);

	Assert(dsr->delta_beginp == NULL);
	Assert(dsr->delta_deltasp == NULL);
	Assert(*(Datum *) &dsr->delta_datum_p == 0);
	Assert(dsr->delta_bitmap_count == 0);
	Assert(dsr->deltas_count == 0);
	Assert(dsr->deltas_size == 0);
	Assert(dsr->delta_block_was_compressed == false);
	Assert(dsr->delta_item == false);

}

void
DatumStreamBlockRead_Finish(
							DatumStreamBlockRead * dsr)
{
	/* Place holder. */
}

/*
 * Dense routines.
 */
#ifdef USE_ASSERT_CHECKING
void
DatumStreamBlockRead_CheckDenseGetInvariant(
											DatumStreamBlockRead * dsr)
{
	int32		total_datum_index;
	int32		currentDeltaBitMapOnCount;

	total_datum_index = dsr->physical_datum_index;
	if (dsr->delta_block_was_compressed)
	{
		currentDeltaBitMapOnCount = DatumStreamBitMapRead_OnSeenCount(&dsr->delta_bitmap);
		total_datum_index += currentDeltaBitMapOnCount;
	}
	else
	{
		currentDeltaBitMapOnCount = 0;
	}

	if (!dsr->rle_block_was_compressed)
	{
		if (!dsr->has_null)
		{
			if (dsr->nth != total_datum_index)
			{
				ereport(ERROR,
						(errmsg("Nth position %d expected to match physical datum index %d + delta ON count %d when Dense block does not have RLE_TYPE compression and does not have NULLs "
						   "(logical row count %d, physical datum count %d)",
								dsr->nth,
								dsr->physical_datum_index,
								currentDeltaBitMapOnCount,
								dsr->logical_row_count,
								dsr->physical_datum_count),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
		}
		else
		{
			int32		currentNullOnCount;
			int32		expectedNullOnCount;

			currentNullOnCount = DatumStreamBitMapRead_OnSeenCount(&dsr->null_bitmap);

			expectedNullOnCount = dsr->nth - total_datum_index;

			if (currentNullOnCount != expectedNullOnCount)
			{
				ereport(ERROR,
						(errmsg("NULL bit-map ON count does not match when Dense block does not have RLE_TYPE compression.	Found %d, expected %d",
								currentNullOnCount,
								expectedNullOnCount),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
		}
	}
	else
	{
		int32		currentCompressBitMapPosition;
		int32		currentCompressBitMapOffCount;

		currentCompressBitMapPosition = DatumStreamBitMapRead_Position(&dsr->rle_compress_bitmap);

		if (currentCompressBitMapPosition != total_datum_index)
		{
			ereport(ERROR,
					(errmsg("COMPRESS bit-map position %d expected to match physical datum index %d + delta ON count %d when Dense block does not have RLE_TYPE compression and does not have NULLs "
							"(logical row count %d, physical datum count %d)",
							currentCompressBitMapPosition,
							dsr->physical_datum_index,
							currentDeltaBitMapOnCount,
							dsr->logical_row_count,
							dsr->physical_datum_count),
					 errdetail_datumstreamblockread(dsr),
					 errcontext_datumstreamblockread(dsr)));
		}


		currentCompressBitMapOffCount = currentCompressBitMapPosition + 1 -
			DatumStreamBitMapRead_OnSeenCount(&dsr->rle_compress_bitmap);

		if (!dsr->has_null)
		{
			if (dsr->nth != currentCompressBitMapOffCount + dsr->rle_total_repeat_items_read - 1)
			{
				ereport(ERROR,
						(errmsg("Nth position %d expected to match "
								"current COMPRESS bit-map OFF count %d + total repeat items read %d - 1 "
								"when Dense block does not have RLE_TYPE compression and does not have NULLs "
						   "(logical row count %d, physical datum count %d)",
								dsr->nth,
								currentCompressBitMapOffCount,
								dsr->rle_total_repeat_items_read,
								dsr->logical_row_count,
								dsr->physical_datum_count),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
		}
		else
		{
			int32		currentNullBitMapOnCount;

			currentNullBitMapOnCount = DatumStreamBitMapRead_OnSeenCount(&dsr->null_bitmap);

			if (dsr->nth != currentNullBitMapOnCount + currentCompressBitMapOffCount + dsr->rle_total_repeat_items_read - 1)
			{
				ereport(ERROR,
						(errmsg("Nth position %d expected to match current NULL bit-map ON count %d + "
								"current COMPRESS bit-map OFF count %d + total repeat items read %d - 1 "
								"when Dense block does not have RLE_TYPE compression and does not have NULLs "
						   "(logical row count %d, physical datum count %d)",
								dsr->nth,
								currentNullBitMapOnCount,
								currentCompressBitMapOffCount,
								dsr->rle_total_repeat_items_read,
								dsr->logical_row_count,
								dsr->physical_datum_count),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
		}
	}

	if (dsr->delta_block_was_compressed)
	{
		int32		currentDeltaBitMapPosition;

		currentDeltaBitMapPosition = DatumStreamBitMapRead_Position(&dsr->delta_bitmap);
		if (currentDeltaBitMapPosition != total_datum_index)
		{
			ereport(ERROR,
					(errmsg("DELTA bit-map position %d expected to match physical datum index %d + delta ON count %d "
							"(logical row count %d, physical datum count %d)",
							currentDeltaBitMapPosition,
							dsr->physical_datum_index,
							currentDeltaBitMapOnCount,
							dsr->logical_row_count,
							dsr->physical_datum_count),
					 errdetail_datumstreamblockread(dsr),
					 errcontext_datumstreamblockread(dsr)));
		}
	}
}
#endif

void
DatumStreamBlockRead_ResetDense(DatumStreamBlockRead * dsr)
{
	/*
	 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for DEBUG
	 * builds...
	 */
#ifdef USE_ASSERT_CHECKING
	if (strncmp(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockRead data structure not valid (eyecatcher)");
#endif

	/*
	 * Put before first entry.	Caller will advance.
	 */
	dsr->nth = -1;
	dsr->physical_datum_index = -1;

	/*
	 * Reset everything else.
	 */
	dsr->physical_datum_count = 0;
	dsr->physical_data_size = 0;
	dsr->logical_row_count = 0;
	dsr->has_null = false;

	dsr->null_bitmap_beginp = NULL;
	dsr->datum_beginp = NULL;
	dsr->datum_afterp = NULL;
	dsr->rle_compress_beginp = NULL;
	dsr->rle_repeatcountsp = NULL;
	dsr->delta_beginp = NULL;
	dsr->delta_deltasp = NULL;
	*(Datum *) &dsr->delta_datum_p = 0;

	dsr->buffer_beginp = NULL;
	dsr->datump = NULL;

	dsr->rle_block_was_compressed = false;

	dsr->rle_repeatcounts_index = 0;
	dsr->rle_in_repeated_item = false;
	dsr->rle_repeated_item_count = 0;

	dsr->rle_total_repeat_items_read = 0;

	dsr->delta_bitmap_count = 0;
	dsr->deltas_count = 0;
	dsr->deltas_size = 0;

	dsr->delta_block_was_compressed = false;
	dsr->delta_item = false;
}

void
DatumStreamBlockRead_GetReadyDense(
								   DatumStreamBlockRead * dsr,
								   uint8 * buffer,
								   int32 bufferSize,
								   int64 firstRowNum,
								   int32 rowCount,
								   bool *hadToAdjustRowCount,
								   int32 * adjustedRowCount)
{
	uint8	   *p;

	int32		unalignedHeaderSize;
	int32		alignedHeaderSize;
	bool		minimalIntegrityChecks;

	DatumStreamBlock_Dense *blockDense;
	DatumStreamBlock_Rle_Extension *rleExtension;
	DatumStreamBlock_Delta_Extension *deltaExtension;

	/*
	 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for DEBUG
	 * builds...
	 */
#ifdef USE_ASSERT_CHECKING
	if (strncmp(dsr->eyecatcher, DatumStreamBlockRead_Eyecatcher, DatumStreamBlockRead_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockRead data structure not valid (eyecatcher)");
#endif

	*hadToAdjustRowCount = false;
	*adjustedRowCount = 0;

#ifdef USE_ASSERT_CHECKING
	minimalIntegrityChecks = false;
#else
	minimalIntegrityChecks = true;
#endif
	if (Debug_datumstream_block_read_check_integrity)
	{
		minimalIntegrityChecks = false;
	}

	dsr->buffer_beginp = buffer;

	/*
	 * Read header information and setup for reading the datum in the block.
	 */

	/*
	 * Advance pointer 'p' as we discover the contents of this block.
	 */
	p = dsr->buffer_beginp;

	Assert(p == buffer);
	DatumStreamBlock_IntegrityCheckDense(
										 buffer,
										 bufferSize,
										 minimalIntegrityChecks,
										 rowCount,
										 &dsr->typeInfo,
			 /* errdetailCallback */ errdetail_datumstreamblockread_callback,
										  /* errdetailArg */ (void *) dsr,
		   /* errcontextCallback */ errcontext_datumstreamblockread_callback,
										  /* errcontextArg */ (void *) dsr);

	blockDense = (DatumStreamBlock_Dense *) p;

	dsr->logical_row_count = blockDense->logical_row_count;
	Assert(dsr->logical_row_count == rowCount);

	dsr->physical_datum_count = blockDense->physical_datum_count;
	dsr->physical_data_size = blockDense->physical_data_size;

	p += sizeof(DatumStreamBlock_Dense);

	dsr->rle_block_was_compressed = ((blockDense->orig_4_bytes.flags & DSB_HAS_RLE_COMPRESSION) != 0);
	if (dsr->rle_block_was_compressed)
	{
		rleExtension = (DatumStreamBlock_Rle_Extension *) p;
		p += sizeof(DatumStreamBlock_Rle_Extension);

		dsr->rle_norepeats_null_bitmap_count = rleExtension->norepeats_null_bitmap_count;
		dsr->rle_compress_bitmap_count = rleExtension->compress_bitmap_count;
		dsr->rle_repeatcounts_count = rleExtension->repeatcounts_count;
		dsr->rle_repeatcounts_size = rleExtension->repeatcounts_size;
	}
	else
	{
		rleExtension = NULL;
	}

	/* Delta */
	dsr->delta_block_was_compressed = ((blockDense->orig_4_bytes.flags & DSB_HAS_DELTA_COMPRESSION) != 0);
	if (dsr->delta_block_was_compressed)
	{
		deltaExtension = (DatumStreamBlock_Delta_Extension *) p;
		p += sizeof(DatumStreamBlock_Delta_Extension);

		dsr->delta_bitmap_count = deltaExtension->delta_bitmap_count;
		dsr->deltas_count = deltaExtension->deltas_count;
		dsr->deltas_size = deltaExtension->deltas_size;
	}
	else
	{
		deltaExtension = NULL;
	}

	/* Set up acc */
	dsr->nth = -1;				/* put it before first entry.  Caller will
								 * advance */
	dsr->physical_datum_index = -1;
	dsr->has_null = ((blockDense->orig_4_bytes.flags & DSB_HAS_NULLBITMAP) != 0);

	if (dsr->has_null)
	{
		dsr->null_bitmap_beginp = p;

		DatumStreamBitMapRead_Init(
								   &dsr->null_bitmap,
								   dsr->null_bitmap_beginp,
								   (!dsr->rle_block_was_compressed ?
									dsr->logical_row_count :
									dsr->rle_norepeats_null_bitmap_count));

		p += DatumStreamBitMapRead_Size(&dsr->null_bitmap);
	}

	unalignedHeaderSize = (int32) (p - dsr->buffer_beginp);
	alignedHeaderSize = MAXALIGN(unalignedHeaderSize);

	/*
	 * Skip over alignment padding.
	 */
	dsr->datum_beginp = dsr->buffer_beginp + alignedHeaderSize;
	dsr->datum_afterp = dsr->datum_beginp + dsr->physical_data_size;

	if (!dsr->rle_block_was_compressed)
	{
		/*
		 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for
		 * DEBUG builds...
		 */
#ifdef USE_ASSERT_CHECKING
		if (Debug_appendonly_print_scan)
		{
			if (!dsr->has_null)
			{
				ereport(LOG,
				(errmsg("Datum stream block read unpack Dense with NO NULLs "
						"(logical row count %d, physical data size = %d, "
						"unaligned header size %d, aligned header size %d, "
						"datum begin %p, datum after %p)",
						dsr->logical_row_count,
						dsr->physical_data_size,
						unalignedHeaderSize,
						alignedHeaderSize,
						dsr->datum_beginp,
						dsr->datum_afterp),
				 errdetail_datumstreamblockread(dsr),
				 errcontext_datumstreamblockread(dsr)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream read unpack Dense block header for table with NULLs "
								"(logical row count %d, physical data size = %d, NULL bit-map size %d, "
						 "unaligned header size %d, aligned header size %d, "
							"null begin %p, datum begin %p, datum after %p)",
								dsr->logical_row_count,
								dsr->physical_data_size,
								DatumStreamBitMapRead_Size(&dsr->null_bitmap),
								unalignedHeaderSize,
								alignedHeaderSize,
								dsr->null_bitmap_beginp,
								dsr->datum_beginp,
								dsr->datum_afterp),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
		}
#endif
	}
	else
	{
		/*
		 * RLE_TYPE compression was used for this block.
		 */
		dsr->rle_compress_beginp = p;

		DatumStreamBitMapRead_Init(
								   &dsr->rle_compress_bitmap,
								   dsr->rle_compress_beginp,
								   dsr->rle_compress_bitmap_count);

		p += DatumStreamBitMapRead_Size(&dsr->rle_compress_bitmap);

		/*
		 * Start our decompression of repeat counts at beginning.
		 */
		dsr->rle_repeatcountsp = p;

		p += dsr->rle_repeatcounts_size;

		dsr->rle_repeatcounts_index = -1;
		dsr->rle_repeated_item_count = 0;
		dsr->rle_in_repeated_item = false;

		unalignedHeaderSize = p - dsr->buffer_beginp;
		alignedHeaderSize = MAXALIGN(unalignedHeaderSize);

		/*
		 * Skip over alignment padding.
		 */
		dsr->datum_beginp = dsr->buffer_beginp + alignedHeaderSize;
		dsr->datum_afterp = dsr->datum_beginp + dsr->physical_data_size;

		/*
		 * PERFORMANCE EXPERIMENT: Only do integrity and trace checking for
		 * DEBUG builds...
		 */
#ifdef USE_ASSERT_CHECKING
		if (Debug_appendonly_print_scan)
		{
			if (!dsr->has_null)
			{
				ereport(LOG,
						(errmsg("Datum stream block read unpack Dense with NO NULLs and RLE_TYPE compression "
						   "(logical row count %d, physical data size = %d, "
					  "compress bit-map count %d, compress bit-map size %d, "
							"repeat counts count %d, repeat counts size %d, "
						 "unaligned header size %d, aligned header size %d, "
								"datum begin %p, datum after %p)",
								dsr->logical_row_count,
								dsr->physical_data_size,
					  DatumStreamBitMapRead_Count(&dsr->rle_compress_bitmap),
					   DatumStreamBitMapRead_Size(&dsr->rle_compress_bitmap),
								dsr->rle_repeatcounts_count,
								dsr->rle_repeatcounts_size,
								unalignedHeaderSize,
								alignedHeaderSize,
								dsr->datum_beginp,
								dsr->datum_afterp),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream block read unpack Dense with NULLs and RLE_TYPE compression "
						   "(logical row count %d, physical data size = %d, "
								"norepeats NULL bit-map count %d, norepeats NULL bit-map size %d, "
					  "compress bit-map count %d, compress bit-map size %d, "
							"repeat counts count %d, repeat counts size %d, "
						 "unaligned header size %d, aligned header size %d, "
							"null begin %p, datum begin %p, datum after %p)",
								dsr->logical_row_count,
								dsr->physical_data_size,
							  DatumStreamBitMapRead_Count(&dsr->null_bitmap),
								DatumStreamBitMapRead_Size(&dsr->null_bitmap),
					  DatumStreamBitMapRead_Count(&dsr->rle_compress_bitmap),
					   DatumStreamBitMapRead_Size(&dsr->rle_compress_bitmap),
								dsr->rle_repeatcounts_count,
								dsr->rle_repeatcounts_size,
								unalignedHeaderSize,
								alignedHeaderSize,
								dsr->null_bitmap_beginp,
								dsr->datum_beginp,
								dsr->datum_afterp),
						 errdetail_datumstreamblockread(dsr),
						 errcontext_datumstreamblockread(dsr)));
			}
		}
#endif
	}

	if (dsr->delta_block_was_compressed)
	{
		/*
		 * Delta compression was used for this block.
		 */
		dsr->delta_beginp = p;

		DatumStreamBitMapRead_Init(
								   &dsr->delta_bitmap,
								   dsr->delta_beginp,
								   dsr->delta_bitmap_count);

		p += DatumStreamBitMapRead_Size(&dsr->delta_bitmap);

		/*
		 * Start our decompression of deltas at beginning.
		 */
		dsr->delta_deltasp = p;
		p += dsr->deltas_size;
		unalignedHeaderSize = p - dsr->buffer_beginp;
		alignedHeaderSize = MAXALIGN(unalignedHeaderSize);

		/*
		 * Skip over alignment padding.
		 */
		dsr->datum_beginp = dsr->buffer_beginp + alignedHeaderSize;
		dsr->datum_afterp = dsr->datum_beginp + dsr->physical_data_size;

		if (Debug_appendonly_print_scan)
		{
			ereport(LOG,
					(errmsg("Datum stream block read unpack Dense with NULLs and DELTA compression "
							"(logical row count %d, physical data size = %d, "
							"has_nul %s, norepeats NULL bit-map count %d, norepeats NULL bit-map size %d, "
							"delta bit-map count %d, delta bit-map size %d, "
							"deltas count %d, deltas size %d, "
						 "unaligned header size %d, aligned header size %d, "
							"null begin %p, datum begin %p, datum after %p)",
							dsr->logical_row_count,
							dsr->physical_data_size,
							dsr->has_null ? "TRUE" : "FALSE",
							dsr->has_null ? DatumStreamBitMapRead_Count(&dsr->null_bitmap) : 0,
			dsr->has_null ? DatumStreamBitMapRead_Size(&dsr->null_bitmap) : 0,
							DatumStreamBitMapRead_Count(&dsr->delta_bitmap),
							DatumStreamBitMapRead_Size(&dsr->delta_bitmap),
							dsr->deltas_count,
							dsr->deltas_size,
							unalignedHeaderSize,
							alignedHeaderSize,
							dsr->null_bitmap_beginp,
							dsr->datum_beginp,
							dsr->datum_afterp),
					 errdetail_datumstreamblockread(dsr),
					 errcontext_datumstreamblockread(dsr)));
		}
	}
	dsr->datump = dsr->datum_beginp;
}

static int
errdetail_datumstreamblockwrite(
								DatumStreamBlockWrite * dsw)
{
	dsw->errdetailCallback(dsw->errcontextArg);

	return 0;
}

static int
errdetail_datumstreamblockwrite_callback(
										 void *arg)
{
	DatumStreamBlockWrite *dsw = (DatumStreamBlockWrite *) arg;

	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	return errdetail_datumstreamblockwrite(dsw);
}

static int
errcontext_datumstreamblockwrite(
								 DatumStreamBlockWrite * dsw)
{
	dsw->errcontextCallback(dsw->errcontextArg);

	return 0;
}

static int
errcontext_datumstreamblockwrite_callback(
										  void *arg)
{
	DatumStreamBlockWrite *dsw = (DatumStreamBlockWrite *) arg;

	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	return errcontext_datumstreamblockwrite(dsw);
}

#ifdef USE_ASSERT_CHECKING
/*
 * DatumStreamBlockWrite routines.
 */
static void
DatumStreamBlockWrite_CheckDenseInvariant(
										  DatumStreamBlockWrite * dsw)
{
	int32		total_datum_count = 0;

	int32		currentCompressBitMapPosition = 0;
	int32		currentCompressBitMapCount = 0;
	int32		currentCompressBitMapOffCount = 0;

	int32		currentDeltaBitMapPosition = 0;
	int32		currentDeltaBitMapCount = 0;
	int32		currentDeltaBitMapOffCount = 0;
	int32		currentDeltaBitMapOnCount = 0;

	if (dsw->delta_has_compression)
	{
		currentDeltaBitMapPosition = DatumStreamBitMapWrite_Count(&dsw->delta_bitmap) - 1;
		currentDeltaBitMapCount = DatumStreamBitMapWrite_Count(&dsw->delta_bitmap);
		currentDeltaBitMapOffCount =
			currentDeltaBitMapCount
			- DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap);
		currentDeltaBitMapOnCount = DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap);
	}

	total_datum_count = dsw->physical_datum_count + currentDeltaBitMapOnCount;

	if (!dsw->rle_has_compression)
	{
		if (!dsw->has_null)
		{
			if (dsw->nth != total_datum_count)
			{
				ereport(ERROR,
						(errmsg("Nth position %d expected to match physical datum count %d + DELTA On count %d when Dense block does not have RLE_TYPE compression and does not have NULLs",
								dsw->nth,
								dsw->physical_datum_count,
								currentDeltaBitMapOnCount),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
		else
		{
			int32		currentNullOnCount;
			int32		expectedNullOnCount;

			currentNullOnCount = DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap);

			expectedNullOnCount = dsw->nth - total_datum_count;

			if (currentNullOnCount != expectedNullOnCount)
			{
				ereport(ERROR,
						(errmsg("NULL bit-map ON count does not match when Dense block does not have RLE_TYPE compression.  Found %d, expected %d",
								currentNullOnCount,
								expectedNullOnCount),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}

	if (dsw->rle_has_compression)
	{
		currentCompressBitMapPosition = DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap) - 1;
		currentCompressBitMapCount = DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap);
		currentCompressBitMapOffCount = currentCompressBitMapCount -
			DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap);

		if (currentCompressBitMapPosition != (dsw->physical_datum_count - 1) + currentDeltaBitMapOnCount)
		{
			ereport(ERROR,
					(errmsg("COMPRESS bit-map position %d expected to match physical datum count %d - 1 when Dense block does not have RLE_TYPE compression and does not have NULLs "
							"(Nth %d)",
							currentCompressBitMapPosition,
							dsw->physical_datum_count,
							dsw->nth),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}

		if (!dsw->has_null)
		{
			if (dsw->nth != currentCompressBitMapOffCount + dsw->rle_total_repeat_items_written)
			{
				ereport(ERROR,
						(errmsg("Nth position %d expected to match "
								"current COMPRESS bit-map OFF count %d + total repeat items written %d "
								"when Dense block does not have RLE_TYPE compression and does not have NULLs "
								"(physical datum count %d)",
								dsw->nth,
								currentCompressBitMapOffCount,
								dsw->rle_total_repeat_items_written,
								dsw->physical_datum_count),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
		else
		{
			int32		currentNullBitMapOnCount;

			int32		currentNullBitMapCount;

			currentNullBitMapOnCount = DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap);

			if (dsw->nth != currentNullBitMapOnCount + currentCompressBitMapOffCount + dsw->rle_total_repeat_items_written)
			{
				ereport(ERROR,
						(errmsg("Nth position %d expected to match current NULL bit-map ON count %d + "
								"current COMPRESS bit-map OFF count %d + total repeat items written %d "
								"when Dense block does not have RLE_TYPE compression and does not have NULLs "
								"(physical datum count %d)",
								dsw->nth,
								currentNullBitMapOnCount,
								currentCompressBitMapOffCount,
								dsw->rle_total_repeat_items_written,
								dsw->physical_datum_count),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}

			currentNullBitMapCount = DatumStreamBitMapWrite_Count(&dsw->null_bitmap);
			if (currentNullBitMapCount != currentNullBitMapOnCount + currentCompressBitMapCount)
			{
				ereport(ERROR,
						(errmsg("Current NULL bit-map count %d expected to match current NULL bit-map ON count %d + "
								"current COMPRESS bit-map count %d "
								"when Dense block does not have RLE_TYPE compression and does not have NULLs "
				  "(total repeat items written %d, physical datum count %d)",
								currentNullBitMapCount,
								currentNullBitMapOnCount,
								currentCompressBitMapCount,
								dsw->rle_total_repeat_items_written,
								dsw->physical_datum_count),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}

	if (dsw->delta_has_compression)
	{
		if (currentDeltaBitMapPosition != (dsw->physical_datum_count - 1) + currentDeltaBitMapOnCount)
		{
			ereport(ERROR,
					(errmsg("DELTA bit-map position %d expected to match physical datum count %d - 1 + %d DELTA On "
							"count, when Dense block does not have RLE_TYPE compression and does not have NULLs "
							"(Nth %d)",
							currentDeltaBitMapPosition,
							dsw->physical_datum_count,
							currentDeltaBitMapOnCount,
							dsw->nth),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}

		if (dsw->rle_has_compression)
		{
			if (currentDeltaBitMapCount != currentCompressBitMapCount)
			{
				ereport(ERROR,
						(errmsg("Current DELTA bit-map count %d expected to match current COMPRESS bit-map count %d "
				  "(total repeat items written %d, physical datum count %d)",
								currentDeltaBitMapCount,
								currentCompressBitMapCount,
								dsw->rle_total_repeat_items_written,
								dsw->physical_datum_count),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}
}
#endif

static void
DatumStreamBlockWrite_PrintInputVarlenaInfo(
											DatumStreamBlockWrite * dsw,
											Datum originalDatum,
											bool wasExtended)
{
	uint8	   *p;

	p = (uint8 *) DatumGetPointer(originalDatum);

	ereport(LOG,
		  (errmsg("Write input varlena input <%s> (nth %d, was extended %s)",
				  VarlenaInfoToString(p),
				  dsw->nth,
				  (wasExtended ? "true" : "false")),
		   errdetail_datumstreamblockwrite(dsw),
		   errcontext_datumstreamblockwrite(dsw)));
}

static void
DatumStreamBlockWrite_PrintStoredSmallVarlenaInfo(
												  DatumStreamBlockWrite * dsw,
												  uint8 * outputBuffer)
{
	uint8	   *p;

	p = outputBuffer;

	ereport(LOG,
			(errmsg("Write stored small varlena: <%s> (nth %d)",
					VarlenaInfoToString(p),
					dsw->nth),
			 errdetail_datumstreamblockwrite(dsw),
			 errcontext_datumstreamblockwrite(dsw)));
}

static void
DatumStreamBlockWrite_PutFixedLengthTrace(
										  DatumStreamBlockWrite * dsw,
										  uint8 * item_beginp)
{
	if (!dsw->typeInfo->byval)
	{
		ereport(LOG,
				(errmsg("Datum stream block write Original fixed-length non-byval item "
						"(nth %d, physical item index #%d, item size %d, item begin %p, item offset " INT64_FORMAT ", next item begin %p)",
						dsw->nth,
						dsw->physical_datum_count - 1,
						dsw->typeInfo->datumlen,
						item_beginp,
						(int64) (item_beginp - dsw->datum_buffer),
						dsw->datump),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
	}
	else
	{
		Datum		readDatum;

		switch (dsw->typeInfo->datumlen)
		{
			case 1:
				readDatum = *(uint8 *) item_beginp;
				break;
			case 2:
				readDatum = *(uint16 *) item_beginp;
				break;
			case 4:
				readDatum = *(uint32 *) item_beginp;
				break;
			case 8:
				readDatum = *(Datum *) item_beginp;
				break;
			default:
				ereport(FATAL,
						(errmsg("fixed length type has unexpected length %d",
								dsw->typeInfo->datumlen),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
				readDatum = 0;
				break;
				/* Never reaches here. */
		}

		ereport(LOG,
				(errmsg("Datum stream block write Original fixed-length item "
						"(nth %d, physical item index #%d, item size %d, item begin %p, item offset " INT64_FORMAT ", next item begin %p, integer " INT64_FORMAT ")",
						dsw->nth,
						dsw->physical_datum_count - 1,
						dsw->typeInfo->datumlen,
						item_beginp,
						(int64) (item_beginp - dsw->datum_buffer),
						dsw->datump,
						(int64) readDatum),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
	}
}

static void
DatumStreamBlockWrite_PutFixedLength(
									 DatumStreamBlockWrite * dsw,
									 Datum d)
{
	if (!dsw->typeInfo->byval)
	{
		memcpy(dsw->datump, DatumGetPointer(d), dsw->typeInfo->datumlen);
		dsw->datump += dsw->typeInfo->datumlen;
	}
	else
	{
		switch (dsw->typeInfo->datumlen)
		{
			case 1:
				*(dsw->datump) = DatumGetChar(d);
				++dsw->datump;
				break;
			case 2:
				Assert(IsAligned(dsw->datump, 2));
				*(uint16 *) (dsw->datump) = DatumGetUInt16(d);
				dsw->datump += 2;
				break;
			case 4:
				Assert(IsAligned(dsw->datump, 4));
				*(uint32 *) (dsw->datump) = DatumGetUInt32(d);
				dsw->datump += 4;
				break;
			case 8:
				Assert(IsAligned(dsw->datump, 8) || IsAligned(dsw->datump, 4));
				*(Datum *) (dsw->datump) = d;
				dsw->datump += 8;
				break;
			default:
				ereport(FATAL,
						(errmsg("fixed length type has unexpected length %d",
								dsw->typeInfo->datumlen),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
				break;
		}
	}
}

static void
DatumStreamBlockWrite_MakeNullBitMapSpace(
										  DatumStreamBlockWrite * dsw)
{
	int32		newNullBitMapSize;

	if (!dsw->has_null)
	{
		newNullBitMapSize = DatumStreamBitMap_Size(dsw->always_null_bitmap_count + 1);
	}
	else
	{
#ifdef USE_ASSERT_CHECKING
		if (DatumStreamBitMapWrite_Count(&dsw->null_bitmap) != dsw->always_null_bitmap_count)
		{
			pg_usleep(120 * 1000000L);
			ereport(ERROR,
					(errmsg("Datum stream write %s block problem growing NULL bit-map buffer "
				   "(current logical row count %d, physical datum count %d, "
						 "null bit-map count %d, old NULL bit-map count %d)",
						  DatumStreamVersion_String(dsw->datumStreamVersion),
							dsw->nth + 1,
							dsw->physical_datum_count,
							DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
							dsw->always_null_bitmap_count),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
#endif
		newNullBitMapSize = DatumStreamBitMapWrite_NextSize(&dsw->null_bitmap);
	}

	/*
	 * Buffer big enough?
	 */
	if (newNullBitMapSize > dsw->null_bitmap_buffer_size)
	{
		int32		newBufferSize;
		void	   *newBuffer;
		MemoryContext oldCtxt;

		/*
		 * Grow the NULL bit-map.
		 */
		newBufferSize = dsw->null_bitmap_buffer_size * 2;
		if (newBufferSize < newNullBitMapSize)
		{
			newBufferSize = newNullBitMapSize + dsw->initialMaxDatumPerBlock;
		}

		oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
		newBuffer = palloc(newBufferSize);

		DatumStreamBitMapWrite_CopyToLargerBuffer(
												  &dsw->null_bitmap,
												  newBuffer,
												  newBufferSize);
		pfree(dsw->null_bitmap_buffer);
		MemoryContextSwitchTo(oldCtxt);

		dsw->null_bitmap_buffer = newBuffer;
		dsw->null_bitmap_buffer_size = newBufferSize;

		/*
		 * Trace the growth of the NULL bit-map buffer.
		 */
		if (Debug_appendonly_print_insert)
		{
			if (!dsw->rle_has_compression)
			{
				ereport(LOG,
						(errmsg("Datum stream write %s block grew NULL bit-map buffer "
				   "(current logical row count %d, physical datum count %d, "
								"null bit-map count %d, null bit-map ON count %d, null bit-map size %d, null bit-map max size %d)",
						  DatumStreamVersion_String(dsw->datumStreamVersion),
								dsw->nth + 1,
								dsw->physical_datum_count,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
							  DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
						  DatumStreamBitMapWrite_MaxSize(&dsw->null_bitmap)),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream write %s block formatted RLE_TYPE block grew NULL bit-map buffer "
				   "(current logical row count %d, physical datum count %d, "
								"null bit-map count %d, null bit-map ON count %d, null bit-map size %d, null bit-map max size %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, "
							 "repeat counts count %d, repeat count size %d)",
						  DatumStreamVersion_String(dsw->datumStreamVersion),
								dsw->nth + 1,
								dsw->physical_datum_count,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
							  DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_MaxSize(&dsw->null_bitmap),
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}

	if (!dsw->has_null)
	{
		/*
		 * First NULL.	Zero fill the NULL bit-map out and set position.
		 */
		dsw->has_null = true;
		DatumStreamBitMapWrite_ZeroFill(&dsw->null_bitmap, /* bitIndex */ dsw->always_null_bitmap_count);

		/*
		 * Trace the zero fill out of the NULL bit-map buffer.
		 */
		if (Debug_appendonly_print_insert)
		{
			if (!dsw->rle_has_compression)
			{
				ereport(LOG,
						(errmsg("Datum stream write %s block zero fill out NULL bit-map "
								"(current logical row count %d, old NULL bit-map count %d, physical datum count %d, "
								"null bit-map count %d, null bit-map ON count %d, null bit-map size %d)",
						  DatumStreamVersion_String(dsw->datumStreamVersion),
								dsw->nth + 1,
								dsw->always_null_bitmap_count,
								dsw->physical_datum_count,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
							 DatumStreamBitMapWrite_Size(&dsw->null_bitmap)),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream write %s block formatted RLE_TYPE block zero fill out NULL bit-map "
								"(current logical row count %d, old NULL bit-map count %d, physical datum count %d, "
							  "null bit-map count %d, null bit-map size %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, "
							 "repeat counts count %d, repeat count size %d)",
						  DatumStreamVersion_String(dsw->datumStreamVersion),
								dsw->nth + 1,
								dsw->always_null_bitmap_count,
								dsw->physical_datum_count,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
							  DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}
	else
	{
		Assert(dsw->nth > 0);
		Assert(DatumStreamBitMapWrite_Count(&dsw->null_bitmap) == dsw->always_null_bitmap_count);
	}

}

static bool
DatumStreamBlockWrite_OrigHasSpace(
								   DatumStreamBlockWrite * dsw,
								   bool null,
								   int32 sz)
{
	int32		headerSize;
	int32		nullSize;
	int32		currentDataSize;

	int32		newTotalSize;

	bool		result;

	if (dsw->nth + 1 >= dsw->maxDatumPerBlock)
		return false;

	headerSize = sizeof(DatumStreamBlock_Orig);
	if (null || dsw->has_null)
	{
		nullSize = MAXALIGN(DatumStreamBitMap_Size(dsw->always_null_bitmap_count + 1));
	}
	else
	{
		nullSize = 0;
	}

	currentDataSize = (dsw->datump - dsw->datum_buffer);

	newTotalSize = (headerSize + nullSize + currentDataSize + sz);
	result = (newTotalSize < dsw->maxDataBlockSize);

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
				(errmsg("Datum stream block write checking Original new item space for "
						"(nth %d, item size %d, configured datum length %d, item begin %p, item offset " INT64_FORMAT ", physical datum count %d, "
						"headerSize %d, nullSize %d, current dataSize %d, "
						"new total size %d, "
						"maxdatasz %d, "
						"result %s)",
						dsw->nth,
						sz,
						dsw->typeInfo->datumlen,
						dsw->datump,
						(int64) (dsw->datump - dsw->datum_buffer),
						dsw->physical_datum_count,
						headerSize,
						nullSize,
						currentDataSize,
						newTotalSize,
						dsw->maxDataBlockSize,
						(result ? "true" : "false")),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
	}

	return result;
}

static int
DatumStreamBlockWrite_PutOrig(
							  DatumStreamBlockWrite * dsw,
							  Datum d,
							  bool null,
							  void **toFree)
{
	uint8	   *item_beginp;

	Assert(dsw);
	Assert(dsw->maxDataBlockSize > 0);
	*toFree = NULL;

	if (null)
	{
		if (!DatumStreamBlockWrite_OrigHasSpace(dsw, null, 0))
		{
			/*
			 * Too many items, or not enough room to add a NULL bit-map data.
			 */
			return -1;
		}

		DatumStreamBlockWrite_MakeNullBitMapSpace(dsw);

		DatumStreamBitMapWrite_AddBit(&dsw->null_bitmap, /* on */ true);

		dsw->always_null_bitmap_count++;
		dsw->nth++;

		if (Debug_appendonly_print_insert_tuple)
		{
			ereport(LOG,
					(errmsg("Datum stream block write Original NULL "
							"(nth %d)",
							dsw->nth),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
		return 0;
	}

	/*
	 * Not a NULL.	We have an item to add.
	 */
	if (dsw->typeInfo->datumlen < 0)
	{
		/* Variable length. */
		uint8	   *dataStart;
		int32		dataLen;

		int32		sz = 0;
		char	   *p = NULL;
		char		c1 = 0;
		int32		wsz = 0;
		Datum		originalDatum;
		bool		wasExtended = false;

		originalDatum = d;

		/*
		 * If toFree comes back non-NULL, we have created a palloc'd de-toasted and/or
		 * de-compressed varlena copy.
		 */
		if (dsw->typeInfo->datumlen == -1)
		{
			varattrib_untoast_ptr_len(d, (char **) &dataStart,
									  &dataLen,
									  toFree);
			if (*toFree != NULL)
			{
				d = PointerGetDatum(*toFree);
				wasExtended = true;
			}
			else
			{
				wasExtended = false;
			}
		}

		if (Debug_datumstream_write_print_small_varlena_info)
		{
			DatumStreamBlockWrite_PrintInputVarlenaInfo(
														dsw,
														originalDatum,
														wasExtended);
		}

		if (dsw->typeInfo->datumlen == -2)
		{
			sz = strlen(DatumGetCString(d)) + 1;
			p = DatumGetPointer(d);
			wsz = sz;
		}
		else if (VARATT_IS_SHORT(DatumGetPointer(d)))
		{
			sz = VARSIZE_SHORT(DatumGetPointer(d));
			p = DatumGetPointer(d);
			wsz = sz;
		}
		else if (value_type_could_short(DatumGetPointer(d), dsw->typeInfo->typid))
		{
			sz = VARATT_CONVERTED_SHORT_SIZE(DatumGetPointer(d));
			c1 = VARSIZE_TO_SHORT_D(d);
			p = VARDATA(DatumGetPointer(d));
			wsz = sz - 1;
		}
		else
		{
			sz = VARSIZE(DatumGetPointer(d));
			dsw->datump = (uint8 *) att_align_zero((char *) dsw->datump, dsw->typeInfo->align);
			p = DatumGetPointer(d);
			wsz = sz;
		}

		if (!DatumStreamBlockWrite_OrigHasSpace(dsw, /* null */ false, sz))
			return -sz;

		/*
		 * Set item begin pointer after we have done zero padding.
		 */
		item_beginp = dsw->datump;

		if (c1 != 0)
		{
			*(dsw->datump) = (uint8) c1;
			dsw->datump += 1;
		}

		memcpy(dsw->datump, p, wsz);

		if (Debug_datumstream_write_print_small_varlena_info)
		{
			DatumStreamBlockWrite_PrintStoredSmallVarlenaInfo(
															  dsw,
															  item_beginp);
		}

		dsw->datump += wsz;

		if (dsw->has_null)
		{
			DatumStreamBlockWrite_MakeNullBitMapSpace(dsw);
			DatumStreamBitMapWrite_AddBit(&dsw->null_bitmap, /* on */ false);
		}

		dsw->always_null_bitmap_count++;
		dsw->nth++;
		Assert(dsw->nth <= dsw->maxDatumPerBlock);

		++dsw->physical_datum_count;

		if (Debug_appendonly_print_insert_tuple)
		{
			ereport(LOG,
			(errmsg("Datum stream block write Original variable-length item "
					"(nth %d, physical item index #%d, item size %d, item begin %p, item offset " INT64_FORMAT ", next item begin %p, datum buffer after %p)",
					dsw->nth,
					dsw->physical_datum_count - 1,
					sz,
					item_beginp,
					(int64) (item_beginp - dsw->datum_buffer),
					dsw->datump,
					dsw->datum_afterp),
			 errdetail_datumstreamblockwrite(dsw),
			 errcontext_datumstreamblockwrite(dsw)));
		}

		return sz;
	}

	/* Fixed length. */

	item_beginp = dsw->datump;

	if (!DatumStreamBlockWrite_OrigHasSpace(dsw, /* null */ false, dsw->typeInfo->datumlen))
	{
		return -dsw->typeInfo->datumlen;
	}

	DatumStreamBlockWrite_PutFixedLength(dsw, d);

	if (dsw->has_null)
	{
		DatumStreamBlockWrite_MakeNullBitMapSpace(dsw);

		DatumStreamBitMapWrite_AddBit(&dsw->null_bitmap, /* on */ false);
	}

	dsw->always_null_bitmap_count++;
	dsw->nth++;
	Assert(dsw->nth <= dsw->maxDatumPerBlock);

	++dsw->physical_datum_count;

	if (Debug_appendonly_print_insert_tuple)
	{
		DatumStreamBlockWrite_PutFixedLengthTrace(dsw, item_beginp);
	}

	return dsw->typeInfo->datumlen;
}

static void
DatumStreamBlockWrite_MakeCompressBitMapSpace(
											  DatumStreamBlockWrite * dsw)
{
	int32		newCompressBitMapSize;

	Assert(dsw->physical_datum_count >= 1);

	int32		deltaBitMapOnCount = 0;

	if (dsw->delta_has_compression)
	{
		deltaBitMapOnCount = DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap);
	}

	if (!dsw->rle_has_compression)
	{
		newCompressBitMapSize = DatumStreamBitMap_Size(dsw->physical_datum_count + deltaBitMapOnCount);
	}
	else
	{
		Assert(DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap) ==
			   (dsw->physical_datum_count + deltaBitMapOnCount));
		newCompressBitMapSize = DatumStreamBitMapWrite_NextSize(&dsw->rle_compress_bitmap);
	}

	/*
	 * Buffer big enough?
	 */
	if (newCompressBitMapSize > dsw->rle_compress_bitmap_buffer_size)
	{
		int32		newBufferSize;
		void	   *newBuffer;
		MemoryContext oldCtxt;

		/*
		 * Grow the NULL bit-map.
		 */
		newBufferSize = dsw->rle_compress_bitmap_buffer_size * 2;
		if (newBufferSize < newCompressBitMapSize)
		{
			newBufferSize = newCompressBitMapSize + dsw->initialMaxDatumPerBlock;
		}

		oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
		newBuffer = palloc(newBufferSize);

		DatumStreamBitMapWrite_CopyToLargerBuffer(
												  &dsw->rle_compress_bitmap,
												  newBuffer,
												  newBufferSize);
		pfree(dsw->rle_compress_bitmap_buffer);
		MemoryContextSwitchTo(oldCtxt);

		dsw->rle_compress_bitmap_buffer = newBuffer;
		dsw->rle_compress_bitmap_buffer_size = newBufferSize;

		/*
		 * Trace the growth of the NULL bit-map buffer.
		 */
		if (Debug_appendonly_print_insert)
		{
			if (!dsw->has_null)
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted RLE_TYPE block grew COMPRESS bit-map buffer "
				   "(current logical row count %d, physical datum count %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, compression bit-map max size %d, "
							 "repeat counts count %d, repeat count size %d)",
								dsw->nth + 1,
								dsw->physical_datum_count,
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_MaxSize(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted RLE_TYPE block with NULLs grew COMPRESS bit-map buffer "
				   "(current logical row count %d, physical datum count %d, "
								"null bit-map count %d, null bit-map ON count %d, null bit-map size %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, compression bit-map max size %d, "
							 "repeat counts count %d, repeat count size %d)",
								dsw->nth + 1,
								dsw->physical_datum_count,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
							  DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_MaxSize(&dsw->null_bitmap),
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}

	if (!dsw->rle_has_compression)
	{
		/*
		 * First COMPRESS bit-map entry. Zero fill the COMPRESS bit-map out and set the position.
		 */
		DatumStreamBitMapWrite_ZeroFill(&dsw->rle_compress_bitmap,
		  /* bitIndex */ dsw->physical_datum_count - 1 + deltaBitMapOnCount);

		/*
		 * Trace the zero fill out of the NULL bit-map buffer.
		 */
		if (Debug_appendonly_print_insert)
		{
			if (!dsw->has_null)
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted RLE_TYPE block zero fill out COMPRESS bit-map "
								"(current logical row count %d, physical datum count %d, delta bit-map ON count %d,"
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, compression bit-map max size %d, "
							 "repeat counts count %d, repeat count size %d)",
								dsw->nth + 1,
								dsw->physical_datum_count,
								deltaBitMapOnCount,
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_MaxSize(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted RLE_TYPE block with NULLs zero fill out COMPRESS bit-map "
								"(current logical row count %d, physical datum count %d, delta bit-map ON count %d,"
							  "null bit-map count %d, null bit-map size %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, compression bit-map max size %d, "
							 "repeat counts count %d, repeat count size %d)",
								dsw->nth + 1,
								dsw->physical_datum_count,
								deltaBitMapOnCount,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
							  DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_MaxSize(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}
	}
	else
	{
		Assert(dsw->nth > 0);
	}

}

/*
 * Function calculates the space required for storing the 
 * RLE meta-data for current block.
 * Increments headerSize and rleSize to reflect the additional
 * size needed for RLE in block, if any.
 */
static inline void
DatumStreamBlockWrite_DenseRleSpace(
		DatumStreamBlockWrite *dsw, bool null,
		int32 *headerSize, int32 *rleSize)
{
	if (!dsw->rle_has_compression)
	{
		return;
	}

	*headerSize += sizeof(DatumStreamBlock_Rle_Extension);

	if (null)
	{
		/* CURRENT compressbitmap byte size since we don't add bits when NULL. */
		*rleSize += DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap);
	}
	else
	{
		/* NEXT compressbitmap byte size. */
		*rleSize += DatumStreamBitMapWrite_NextSize(&dsw->rle_compress_bitmap);
	}

	/*
	 * CURRENT repeat counts size.
	 */
	*rleSize += dsw->rle_repeatcounts_current_size;

	/*
	 * If last item repeated is true, then to finalize the repeatcounts
	 * anytime later we will need space equivalent to its size in block.
	 * Hence account for its size now else during block write may go
	 * beyond block boundary.
	 */
	if (dsw->rle_last_item_is_repeated)
	{
		*rleSize += DatumStreamInt32Compress_Size(
					dsw->rle_repeatcounts[dsw->rle_repeatcounts_count - 1]);
	}
}

/*
 * Can we add an optional NULL bitmap entry or optionally the compress bit-map and
 * repeat count array for RLE_TYPE?
 */
static bool
DatumStreamBlockWrite_DenseHasSpaceNull(
										DatumStreamBlockWrite * dsw)
{
	int32		headerSize = 0;
	int32		nullSize = 0;
	int32		rleSize = 0;
	int32		deltaSize = 0;
	int32		alignedHeaderSize = 0;
	int32		currentDataSize = 0;
	int32		newTotalSize = 0;
	bool		result = false;

	if (dsw->nth + 1 >= dsw->maxDatumPerBlock)
	{
		return false;
	}

	headerSize = sizeof(DatumStreamBlock_Dense);

	/*
	 * Add in NULL bit-map, extra DatumStreamBlock_Rle struct, compress bit-map, repeated counts...
	 */
	nullSize = DatumStreamBitMap_Size(dsw->always_null_bitmap_count + 1);

	DatumStreamBlockWrite_DenseRleSpace(dsw, true, &headerSize, &rleSize);

	/* Add in Delta Compression structures */
	if (dsw->delta_has_compression)
	{
		/*
		 * Add in Delta bitmap but CURRENT deltas array byte lengths.
		 */
		headerSize += sizeof(DatumStreamBlock_Delta_Extension);

		/*
		 * NEW delta bitmap byte size since we don't add bits when NULL;
		 */
		deltaSize = DatumStreamBitMapWrite_NextSize(&dsw->delta_bitmap);

		/*
		 * CURRENT deltas size.
		 */
		deltaSize += dsw->deltas_current_size;
	}

	/*
	 * Align headers and meta-data (e.g. NULL bit-maps, etc).
	 */
	alignedHeaderSize = MAXALIGN(headerSize + nullSize + rleSize + deltaSize);

	/*
	 * Data.
	 */
	currentDataSize = (dsw->datump - dsw->datum_buffer);

	/*
	 * Total.
	 */
	newTotalSize = (alignedHeaderSize + currentDataSize);
	result = (newTotalSize <= dsw->maxDataBlockSize);

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
			(errmsg("Datum stream block write checking Dense new NULL space "
					"(nth %d, configured datum length %d, item begin %p, item offset " INT64_FORMAT ", physical datum count %d, "
					"has RLE_TYPE compression %s, "
					"has DELTA compression %s, "
		"headerSize %d, nullSize %d, rleSize %d, deltaSize %d, dataSize %d, "
					"aligned header size %d, new total size %d, "
					"maxdatasz %d, "
					"result %s)",
					dsw->nth,
					dsw->typeInfo->datumlen,
					dsw->datump,
					(int64) (dsw->datump - dsw->datum_buffer),
					dsw->physical_datum_count,
					(dsw->rle_has_compression ? "true" : "false"),
					(dsw->delta_has_compression ? "true" : "false"),
					headerSize,
					nullSize,
					rleSize,
					deltaSize,
					currentDataSize,
					alignedHeaderSize,
					newTotalSize,
					dsw->maxDataBlockSize,
					(result ? "true" : "false")),
			 errdetail_datumstreamblockwrite(dsw),
			 errcontext_datumstreamblockwrite(dsw)));
	}

	return result;
}

/*
 * Can we add an optional NULL bitmap entry or optionally the compress bit-map and
 * repeat count array for RLE_TYPE?
 */
static bool
DatumStreamBlockWrite_DenseHasSpaceRepeat(
										  DatumStreamBlockWrite * dsw,
										  bool newRepeat)
{
	int32		headerSize = 0;
	int32		nullSize = 0;
	int32		rleSize = 0;
	int32		deltaSize = 0;
	int32		alignedHeaderSize = 0;
	int32		currentDataSize = 0;
	int32		newTotalSize = 0;
	bool		result = false;
	int32		total_datum_count = 0;

	if (dsw->nth + 1 >= dsw->maxDatumPerBlock)
	{
		return false;
	}

	headerSize = sizeof(DatumStreamBlock_Dense);

	/*
	 * Add in NULL bit-map, extra DatumStreamBlock_Rle struct, compress bit-map, repeated counts...
	 */
	if (dsw->has_null)
	{
		/*
		 * The first item already incremented always_null_bitmap_count.
		 */
		nullSize = DatumStreamBitMap_Size(dsw->always_null_bitmap_count);
	}

	total_datum_count = dsw->physical_datum_count;

	/*
	 * Add in Delta Compression structures
	 */
	if (dsw->delta_has_compression)
	{
		/*
		 * Add in Delta bitmap but CURRENT deltas array byte lengths.
		 */
		headerSize += sizeof(DatumStreamBlock_Delta_Extension);

		/*
		 * NEW delta bitmap byte size since we don't add bits when NULL;
		 */
		deltaSize = DatumStreamBitMapWrite_NextSize(&dsw->delta_bitmap);

		/*
		 * CURRENT deltas size.
		 */
		deltaSize += dsw->deltas_current_size;

		total_datum_count += DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap);
	}

	headerSize += sizeof(DatumStreamBlock_Rle_Extension);
	if (newRepeat)
	{
		/*
		 * NEW compressbitmap byte size.
		 */
		rleSize = DatumStreamBitMap_Size(total_datum_count + 1);
	}
	else
	{
		/*
		 * CURRENT compressbitmap byte size.
		 */
		rleSize = DatumStreamBitMap_Size(total_datum_count);
	}

	/*
	 * NEW repeat counts size.	For simplicity, assume it will be the largest repeated count integer possible.
	 */
	rleSize += dsw->rle_repeatcounts_current_size + Int32Compress_MaxByteLen;

	/*
	 * Align headers and meta-data (e.g. NULL bit-maps, etc).
	 */
	alignedHeaderSize = MAXALIGN(headerSize + nullSize + rleSize + deltaSize);

	/*
	 * Data.
	 */
	currentDataSize = (dsw->datump - dsw->datum_buffer);

	/*
	 * Total.
	 */
	newTotalSize = (alignedHeaderSize + currentDataSize);
	result = (newTotalSize <= dsw->maxDataBlockSize);

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
		   (errmsg("Datum stream block write checking Dense %s Repeat space "
				   "(nth %d, configured datum length %d, item begin %p, item offset " INT64_FORMAT ", physical datum count %d, "
				   "has RLE_TYPE compression %s, "
				   "has DELTA compression %s, "
		"headerSize %d, nullSize %d, rleSize %d, deltaSize %d, dataSize %d, "
				   "aligned header size %d, new total size %d, "
				   "maxdatasz %d, "
				   "result %s)",
				   (newRepeat ? "NEW" : "OLD"),
				   dsw->nth,
				   dsw->typeInfo->datumlen,
				   dsw->datump,
				   (int64) (dsw->datump - dsw->datum_buffer),
				   dsw->physical_datum_count,
				   (dsw->rle_has_compression ? "true" : "false"),
				   (dsw->delta_has_compression ? "true" : "false"),
				   headerSize,
				   nullSize,
				   rleSize,
				   deltaSize,
				   currentDataSize,
				   alignedHeaderSize,
				   newTotalSize,
				   dsw->maxDataBlockSize,
				   (result ? "true" : "false")),
			errdetail_datumstreamblockwrite(dsw),
			errcontext_datumstreamblockwrite(dsw)));
	}

	return result;
}

/*
 * Can we add the delta bit-map and deltas array for RLE_TYPE with Delta?
 */
static bool
DatumStreamBlockWrite_DenseHasSpaceDelta(
										 DatumStreamBlockWrite * dsw)
{
	int32		headerSize = 0;
	int32		nullSize = 0;
	int32		rleSize = 0;
	int32		deltaSize = 0;
	int32		alignedHeaderSize = 0;
	int32		currentDataSize = 0;
	int32		newTotalSize = 0;
	bool		result = false;
	int32		total_datum_count = 0;

	if (dsw->nth + 1 >= dsw->maxDatumPerBlock)
	{
		return false;
	}

	headerSize = sizeof(DatumStreamBlock_Dense);

	/*
	 * Add in NULL bit-map, extra DatumStreamBlock_Rle struct, compress bit-map, repeated counts...
	 */
	if (dsw->has_null)
	{
		/*
		 * Adding delta value will add a false bit to null_bitmap.
		 * So, must account for it.
		 */
		nullSize = DatumStreamBitMap_Size(dsw->always_null_bitmap_count + 1);
	}

	DatumStreamBlockWrite_DenseRleSpace(dsw, false, &headerSize, &rleSize);

	total_datum_count = dsw->physical_datum_count;
	if (dsw->delta_has_compression)
	{
		total_datum_count += DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap);
	}

	/*
	 * Add in Delta bitmap but CURRENT deltas array byte lengths.
	 */
	headerSize += sizeof(DatumStreamBlock_Delta_Extension);

	/*
	 * NEW delta bitmap byte size;
	 */
	deltaSize = DatumStreamBitMap_Size(total_datum_count + 1);

	/*
	 * NEW deltas size. For simplicity, assume it will be the largest delta integer possible.
	 */
	deltaSize += dsw->deltas_current_size + Int32CompressReserved3_MaxByteLen;

	/*
	 * Align headers and meta-data (e.g. NULL bit-maps, etc).
	 */
	alignedHeaderSize = MAXALIGN(headerSize + nullSize + rleSize + deltaSize);

	/*
	 * Data.
	 */
	currentDataSize = (dsw->datump - dsw->datum_buffer);

	/*
	 * Total.
	 */
	newTotalSize = (alignedHeaderSize + currentDataSize);
	result = (newTotalSize <= dsw->maxDataBlockSize);

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
				(errmsg("Datum stream block write checking Dense DELTA space "
						"(nth %d, configured datum length %d, item begin %p, item offset " INT64_FORMAT ", physical datum count %d, "
						"has RLE_TYPE compression %s, "
						"has DELTA compression %s, "
		"headerSize %d, nullSize %d, rleSize %d, deltaSize %d, dataSize %d, "
						"aligned header size %d, new total size %d, "
						"maxdatasz %d, "
						"result %s)",
						dsw->nth,
						dsw->typeInfo->datumlen,
						dsw->datump,
						(int64) (dsw->datump - dsw->datum_buffer),
						dsw->physical_datum_count,
						(dsw->rle_has_compression ? "true" : "false"),
						(dsw->delta_has_compression ? "true" : "false"),
						headerSize,
						nullSize,
						rleSize,
						deltaSize,
						currentDataSize,
						alignedHeaderSize,
						newTotalSize,
						dsw->maxDataBlockSize,
						(result ? "true" : "false")),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
	}

	return result;
}

/*
 * Can we add a compressed bitmap array and repeat count array for RLE_TYPE?
 */
static bool
DatumStreamBlockWrite_DenseHasSpaceItem(
										DatumStreamBlockWrite * dsw,
										int32 sz)
{
	int32		headerSize = 0;
	int32		nullSize = 0;
	int32		rleSize = 0;
	int32		deltaSize = 0;
	int32		alignedHeaderSize = 0;
	int32		currentDataSize = 0;
	int32		newTotalSize = 0;
	bool		result = false;

	if (dsw->nth + 1 >= dsw->maxDatumPerBlock)
	{
		return false;
	}

	headerSize = sizeof(DatumStreamBlock_Dense);

	/*
	 * Add in NULL bit-map, extra DatumStreamBlock_Rle_Extension struct, compress bit-map, repeated counts...
	 */

	if (dsw->has_null)
	{
		nullSize = DatumStreamBitMap_Size(dsw->always_null_bitmap_count + 1);
	}

	DatumStreamBlockWrite_DenseRleSpace(dsw, false, &headerSize, &rleSize);

	if (dsw->delta_has_compression)
	{
		/*
		 * Add in Delta bitmap but CURRENT deltas array byte lengths.
		 */
		headerSize += sizeof(DatumStreamBlock_Delta_Extension);

		/*
		 * NEW delta bitmap byte size since we don't add bits when NULL;
		 */
		deltaSize = DatumStreamBitMapWrite_NextSize(&dsw->delta_bitmap);

		/*
		 * CURRENT deltas size.
		 */
		deltaSize += dsw->deltas_current_size;
	}

	/*
	 * Align headers and meta-data (e.g. NULL bit-maps, etc).
	 */
	alignedHeaderSize = MAXALIGN(headerSize + nullSize + rleSize + deltaSize);

	/*
	 * Data.
	 */
	currentDataSize = (dsw->datump - dsw->datum_buffer);

	/*
	 * Total.
	 */
	newTotalSize = (alignedHeaderSize + currentDataSize + sz);
	result = (newTotalSize <= dsw->maxDataBlockSize);

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
		(errmsg("Datum stream block write checking Dense new item space for "
				"(nth %d, item size %d, configured datum length %d, item begin %p, item offset " INT64_FORMAT ", physical datum count %d, "
				"has RLE_TYPE compression %s, "
				"has DELTA compression %s, "
				"headerSize %d, nullSize %d, rleSize %d, deltaSize %d, current dataSize %d, "
				"aligned header size %d, new total size %d, "
				"maxdatasz %d, "
				"result %s)",
				dsw->nth,
				sz,
				dsw->typeInfo->datumlen,
				dsw->datump,
				(int64) (dsw->datump - dsw->datum_buffer),
				dsw->physical_datum_count,
				(dsw->rle_has_compression ? "true" : "false"),
				(dsw->delta_has_compression ? "true" : "false"),
				headerSize,
				nullSize,
				rleSize,
				deltaSize,
				currentDataSize,
				alignedHeaderSize,
				newTotalSize,
				dsw->maxDataBlockSize,
				(result ? "true" : "false")),
		 errdetail_datumstreamblockwrite(dsw),
		 errcontext_datumstreamblockwrite(dsw)));
	}

	return result;
}

static void
DatumStreamBlockWrite_RleFinalizeRepeatCountSize(
												 DatumStreamBlockWrite * dsw)
{
	Assert(dsw->rle_has_compression);
	Assert(dsw->rle_last_item_is_repeated);

	dsw->rle_last_item_is_repeated = false;
	dsw->rle_last_item = NULL;
	dsw->rle_last_item_size = 0;

	/*
	 * Look at how large the current repeat count is and add in its size.
	 */
	dsw->rle_repeatcounts_current_size +=
		DatumStreamInt32Compress_Size(
					 dsw->rle_repeatcounts[dsw->rle_repeatcounts_count - 1]);

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
		(errmsg("Datum stream block write Dense finalized repeated item for "
		"(nth %d, item size %d, item begin %p, item offset " INT64_FORMAT ", "
				"repeat index %d, repeat count %d, repeat counts size %d)",
				dsw->nth,
				dsw->typeInfo->datumlen,
				dsw->datump,
				(int64) (dsw->datump - dsw->datum_buffer),
				dsw->rle_repeatcounts_count - 1,
				dsw->rle_repeatcounts[dsw->rle_repeatcounts_count - 1],
				dsw->rle_repeatcounts_current_size),
		 errdetail_datumstreamblockwrite(dsw),
		 errcontext_datumstreamblockwrite(dsw)));
	}

}

static void
DatumStreamBlockWrite_DenseIncrNull(
									DatumStreamBlockWrite * dsw)
{
	Assert(dsw->has_null);

	DatumStreamBitMapWrite_AddBit(&dsw->null_bitmap, /* on */ true);

	/*
	 * Always maintain this NULL bit-map counter even if we don't have RLE_TYPE compression yet.
	 */
	dsw->always_null_bitmap_count++;

	/*
	 * Maintain compression data structures.
	 */
	if (dsw->rle_want_compression)
	{
		if (dsw->rle_last_item_is_repeated)
		{
			Assert(dsw->rle_has_compression);
			DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
		}

		dsw->rle_last_item = NULL;
		dsw->rle_last_item_size = 0;
	}

	/*
	 * Total number of items represented in this block.
	 */
	++dsw->nth;
	Assert(dsw->nth <= dsw->maxDatumPerBlock);
}

static void
DatumStreamBlockWrite_DenseIncrItem(
									DatumStreamBlockWrite * dsw,
									uint8 * rle_last_item,
									int32 rle_last_item_size)
{
	if (dsw->has_null)
	{
		DatumStreamBlockWrite_MakeNullBitMapSpace(dsw);

		DatumStreamBitMapWrite_AddBit(&dsw->null_bitmap, /* on */ false);
	}

	/*
	 * Always maintain this NULL bit-map counter even if we don't have NULLs yet and/or RLE_TYPE compression yet.
	 */
	dsw->always_null_bitmap_count++;

	/*
	 * Maintain RLE compression data structures.
	 */
	if (dsw->rle_want_compression)
	{
		if (dsw->rle_last_item_is_repeated)
		{
			Assert(dsw->rle_has_compression);
			DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
		}

		dsw->rle_last_item = rle_last_item;
		dsw->rle_last_item_size = rle_last_item_size;

		if (dsw->rle_has_compression)
		{
			DatumStreamBlockWrite_MakeCompressBitMapSpace(dsw);

			/*
			 * New items start off with their bit as OFF.
			 */
			DatumStreamBitMapWrite_AddBit(&dsw->rle_compress_bitmap, /* on */ false);
		}
	}

	/*
	 * Total number of items represented in this block.
	 */
	++dsw->nth;
	Assert(dsw->nth <= dsw->maxDatumPerBlock);

	++dsw->physical_datum_count;
}

static void
DatumStreamBlockWrite_RleIncrRepeated(
									  DatumStreamBlockWrite * dsw)
{
	/*
	 * DO NOT advance the null bit-map since it was advanced on the first item.
	 */

	if (!dsw->rle_last_item_is_repeated)
	{
		/*
		 * First time this item has repeated.
		 */
		dsw->rle_last_item_is_repeated = true;

		dsw->rle_total_repeat_items_written++;
		/* Bump here to count the first item. */

		if (!dsw->rle_has_compression)
		{
			/*
			 * Zero fill out, if necessary.
			 */
			DatumStreamBlockWrite_MakeCompressBitMapSpace(dsw);

			DatumStreamBitMapWrite_AddBit(&dsw->rle_compress_bitmap, /* on */ true);

			/*
			 * Set rle_has_compression after calling
			 * ~_MakeBitMapSpaceForCompress above.
			 */
			dsw->rle_has_compression = true;
		}
		else
		{
			/*
			 * Set the repeated item bit (that was initialized as OFF in ~_DenseIncrItem) in
			 * the COMPRESS bit-map.
			 */
			DatumStreamBitMapWrite_Set(&dsw->rle_compress_bitmap);
		}

		if (dsw->rle_repeatcounts_count + 1 >= dsw->rle_repeatcounts_maxcount)
		{
			int32		oldBufferSize;
			int32		newBufferSize;
			void	   *newBuffer;
			MemoryContext oldCtxt;

			/*
			 * Grow the repeat counts array.
			 */
			oldBufferSize = dsw->rle_repeatcounts_maxcount * Int32Compress_MaxByteLen;
			newBufferSize = oldBufferSize * 2;

			oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
			newBuffer = palloc(newBufferSize);

			memcpy(newBuffer, dsw->rle_repeatcounts, oldBufferSize);

			pfree(dsw->rle_repeatcounts);
			MemoryContextSwitchTo(oldCtxt);

			dsw->rle_repeatcounts = newBuffer;
			dsw->rle_repeatcounts_maxcount *= 2;
		}

		/*
		 * Initialize the repeat count.
		 */
		dsw->rle_repeatcounts[dsw->rle_repeatcounts_count] = 1;
		dsw->rle_repeatcounts_count++;
	}
	else
	{
		dsw->rle_repeatcounts[dsw->rle_repeatcounts_count - 1]++;
	}

	/*
	 * In the end, we use rle_savings to estimate the eofUncompress.
	 */
	dsw->savings += dsw->rle_last_item_size;

	/*
	 * Advance our overall count of items.
	 *
	 * DO NOT increment always_null_bitmap_count here.
	 */
	++dsw->nth;
	dsw->rle_total_repeat_items_written++;
	/* Count new repeated item. */

	Assert(dsw->nth <= dsw->maxDatumPerBlock);
}

/*
 * If we have space, mark the previous RLE_TYPE entry as repeated.
 */
static bool
DatumStreamBlockWrite_RleMarkRepeat(
									DatumStreamBlockWrite * dsw)
{
	/*
	 * Allocate the compress bit-map and repeat count array if necessary.  And,
	 * see if there is room for both and room including a new compress bit and
	 * new count.
	 */
	if (!DatumStreamBlockWrite_DenseHasSpaceRepeat(
												   dsw,
							/* newRepeat */ !dsw->rle_last_item_is_repeated))
	{
		return false;
	}

	DatumStreamBlockWrite_RleIncrRepeated(dsw);
	return true;
}

static void
DatumStreamBlockWrite_MakeDeltaCompressBitMapSpace(
												 DatumStreamBlockWrite * dsw)
{
	int32		newCompressBitMapSize;

	Assert(dsw->physical_datum_count >= 1);

	if (!dsw->delta_has_compression)
	{
		newCompressBitMapSize = DatumStreamBitMap_Size(dsw->physical_datum_count);
	}
	else
	{
		newCompressBitMapSize = DatumStreamBitMapWrite_NextSize(&dsw->delta_bitmap);
	}

	/*
	 * Buffer big enough?
	 */
	if (newCompressBitMapSize > dsw->delta_bitmap_buffer_size)
	{
		int32		newBufferSize;
		void	   *newBuffer;
		MemoryContext oldCtxt;

		/*
		 * Grow the Delta bit-map.
		 */
		newBufferSize = dsw->delta_bitmap_buffer_size * 2;
		if (newBufferSize < newCompressBitMapSize)
		{
			newBufferSize = newCompressBitMapSize + dsw->initialMaxDatumPerBlock;
		}

		oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
		newBuffer = palloc(newBufferSize);

		DatumStreamBitMapWrite_CopyToLargerBuffer(
												  &dsw->delta_bitmap,
												  newBuffer,
												  newBufferSize);
		pfree(dsw->delta_bitmap_buffer);
		MemoryContextSwitchTo(oldCtxt);

		dsw->delta_bitmap_buffer = newBuffer;
		dsw->delta_bitmap_buffer_size = newBufferSize;

		/*
		 * Trace the growth of the Delta bit-map buffer.
		 */
		if (Debug_appendonly_print_insert)
		{
			ereport(LOG,
					(errmsg("Datum stream write Dense block formatted RLE_TYPE block grew DELTA COMPRESS bit-map buffer "
				   "(current logical row count %d, physical datum count %d, "
							"delta bit-map count %d, delta bit-map ON count %d, delta bit-map size %d, delta bit-map max size %d, "
							"repeat counts count %d, repeat count size %d)",
							dsw->nth + 1,
							dsw->physical_datum_count,
							DatumStreamBitMapWrite_Count(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap),
							DatumStreamBitMapWrite_Size(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_MaxSize(&dsw->delta_bitmap),
							dsw->deltas_count,
							dsw->deltas_current_size),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
	}

	if (!dsw->delta_has_compression)
	{
		/*
		 * First delta bit-map entry. Zero fill the delta COMPRESS bit-map out and set the position.
		 */
		DatumStreamBitMapWrite_ZeroFill(&dsw->delta_bitmap, /* bitIndex */ dsw->physical_datum_count);

		/*
		 * Trace the zero fill out of the NULL bit-map buffer.
		 */
		if (Debug_appendonly_print_insert)
		{
			ereport(LOG,
					(errmsg("Datum stream write Dense block formatted RLE_TYPE DELTA block zero fill out DELTA bit-map "
				   "(current logical row count %d, physical datum count %d, "
							"delta bit-map count %d, delta bit-map ON count %d, delta bit-map size %d, delta bit-map max size %d, "
							"deltas count %d, deltas size %d)",
							dsw->nth + 1,
							dsw->physical_datum_count,
							DatumStreamBitMapWrite_Count(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap),
							DatumStreamBitMapWrite_Size(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_MaxSize(&dsw->delta_bitmap),
							dsw->deltas_count,
							dsw->deltas_current_size),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
	}
	else
	{
		Assert(dsw->nth > 0);
		if (Debug_appendonly_print_insert)
		{
			ereport(LOG,
					(errmsg("Datum stream write Dense block formatted RLE_TYPE DELTA block DELTA bit-map "
				   "(current logical row count %d, physical datum count %d, "
							"delta bit-map count %d, delta bit-map ON count %d, delta bit-map size %d, delta bit-map max size %d, "
							"deltas count %d, deltas size %d)",
							dsw->nth + 1,
							dsw->physical_datum_count,
							DatumStreamBitMapWrite_Count(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap),
							DatumStreamBitMapWrite_Size(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_MaxSize(&dsw->delta_bitmap),
							dsw->deltas_count,
							dsw->deltas_current_size),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
	}
}

static inline void
DatumStreamBlockWrite_DeltaMaintain(
									DatumStreamBlockWrite * dsw,
									Datum d)
{
	if (!dsw->delta_want_compression)
		return;

	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
			(errmsg("Datum stream insert DELTA Maintain = " INT64_FORMAT, d),
			 errdetail_datumstreamblockwrite(dsw),
			 errcontext_datumstreamblockwrite(dsw)));
	}

	/*
	 * Maintain Delta compression data structures.
	 */
	switch (dsw->typeInfo->datumlen)
	{
		case 4:
			*(uint32 *) (&dsw->compare_item) = DatumGetUInt32(d);
			break;
		case 8:
			dsw->compare_item = d;
			break;
		default:
			ereport(FATAL,
					(errmsg("DELTA Compression maintain, fixed length type has unexpected length %d",
							dsw->typeInfo->datumlen),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			break;
	}

	/*
	 * Add a 0 bit in Delta Bitmap
	 * Zero fill out, if necessary.
	 */
	if (dsw->delta_has_compression)
	{
		DatumStreamBlockWrite_MakeDeltaCompressBitMapSpace(dsw);
		DatumStreamBitMapWrite_AddBit(&dsw->delta_bitmap, /* on */ false);
	}
}

static inline void
DatumStreamBlockWrite_DeltaAdd(
							   DatumStreamBlockWrite * dsw,
							   int64 delta,
							   bool sign)
{
	if (Debug_appendonly_print_insert_tuple)
	{
		ereport(LOG,
		(errmsg("Datum stream insert DELTA Add = " INT64_FORMAT " sign = %d",
				delta, sign),
		 errdetail_datumstreamblockwrite(dsw),
		 errcontext_datumstreamblockwrite(dsw)));
	}

	/*
	 * Zero fill out, if necessary.
	 */
	DatumStreamBlockWrite_MakeDeltaCompressBitMapSpace(dsw);
	/* Set delta_has_compression after calling ~_MakeDeltaBitMapSpace above. */
	dsw->delta_has_compression = true;

	/*
	 * Maintain NULL data structures.
	 */
	if (dsw->has_null)
	{
		DatumStreamBlockWrite_MakeNullBitMapSpace(dsw);

		DatumStreamBitMapWrite_AddBit(&dsw->null_bitmap, /* on */ false);
	}

	/*
	 * Always maintain this NULL bit-map counter even if we don't have NULLs yet and/or RLE_TYPE compression yet.
	 */
	dsw->always_null_bitmap_count++;

	/*
	 * Maintain RLE compression data structures.
	 */
	Assert(dsw->rle_want_compression == true);
	if (dsw->rle_last_item_is_repeated)
	{
		Assert(dsw->rle_has_compression);
		DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
	}

	/* RLE last item now should be the current datum */
	dsw->rle_last_item = (uint8 *) & dsw->compare_item;
	dsw->rle_last_item_size = dsw->typeInfo->datumlen;

	if (dsw->rle_has_compression)
	{
		DatumStreamBlockWrite_MakeCompressBitMapSpace(dsw);

		/*
		 * New items start off with their bit as OFF.
		 */
		DatumStreamBitMapWrite_AddBit(&dsw->rle_compress_bitmap, /* on */ false);
	}

	/*
	 * Add a set bit to delta bitmap... This needs to happen after
	 * MakeCompressBitMapSpace
	 */
	DatumStreamBitMapWrite_AddBit(&dsw->delta_bitmap, /* on */ true);

	if (dsw->deltas_count + 1 >= dsw->deltas_maxcount)
	{
		int32		oldBufferSize;
		int32		newBufferSize;
		void	   *newBuffer;
		MemoryContext oldCtxt;

		/*
		 * Grow the delta and delta_sign array.
		 */
		oldBufferSize = dsw->deltas_maxcount * Int32Compress_MaxByteLen;
		newBufferSize = oldBufferSize * 2;

		oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
		newBuffer = palloc(newBufferSize);

		memcpy(newBuffer, dsw->deltas, oldBufferSize);

		pfree(dsw->deltas);
		MemoryContextSwitchTo(oldCtxt);

		dsw->deltas = newBuffer;

		/* Grow the delta_sign buffer as well */
		oldBufferSize = dsw->deltas_maxcount * sizeof(bool);
		newBufferSize = oldBufferSize * 2;

		oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
		newBuffer = palloc(newBufferSize);

		memcpy(newBuffer, dsw->delta_sign, oldBufferSize);

		pfree(dsw->delta_sign);
		MemoryContextSwitchTo(oldCtxt);

		dsw->delta_sign = newBuffer;


		dsw->deltas_maxcount *= 2;
	}

	/*
	 * Store the delta and sign.
	 */
	dsw->deltas[dsw->deltas_count] = delta;
	dsw->delta_sign[dsw->deltas_count] = sign;

	dsw->deltas_current_size +=
		DatumStreamInt32CompressReserved3_Size(dsw->deltas[dsw->deltas_count]);
	dsw->deltas_count++;

	/*
	 * In the end, we use delta_savings to estimate the eofUncompress.
	 */
	dsw->savings += dsw->typeInfo->datumlen;;

	/*
	 * Advance our overall count of items.
	 *
	 */
	++dsw->nth;

	Assert(dsw->nth <= dsw->maxDatumPerBlock);
}

/*
 * Delta compression is applied only if delta between the adjacent tuples can
 * be stored max by 4 bytes. Since upper 3 bits are reserved, leaves room of
 * 29 bits max value for delta to be stored. If delta turns out to be larger
 * than this value, delta compression is not applied for this tuple instead
 * actual value is directly stored.
 */
#define MAX_DELTA_SUPPORTED_DELTA_COMPRESSION 0x1FFFFFFF

static Delta_Compression_status
DatumStreamBlockWrite_PerformDeltaCompression(
											  DatumStreamBlockWrite * dsw,
											  Datum d)
{
	int64		delta = 0;
	bool		positive_delta = true;

	if (!dsw->delta_want_compression)
	{
		return DELTA_COMPRESSION_NOT_APPLIED;
	}

	/*
	 * Check if its first non-NULL datum of the block then
	 * just need to add a starting 0 bit in deltaBit map and return
	 * as first value will always be stored as physical datum
	 */
	if (dsw->not_first_datum == false)
	{
		dsw->not_first_datum = true;
		return DELTA_COMPRESSION_NOT_APPLIED;
	}

	/* Here means, we have compare value stored to calculate the Delta */
	switch (dsw->typeInfo->datumlen)
	{
		case 4:
			if (DatumGetUInt32(dsw->compare_item) <= DatumGetUInt32(d))
			{
				/* Positive delta */
				delta = DatumGetUInt32(d) - DatumGetUInt32(dsw->compare_item);
				positive_delta = true;
			}
			else
			{
				/* Negative Delta */
				delta = DatumGetUInt32(dsw->compare_item) - DatumGetUInt32(d);
				positive_delta = false;
			}
			break;

		case 8:
			if (dsw->compare_item <= d)
			{
				/* Positive delta */
				delta = d - dsw->compare_item;
				positive_delta = true;
			}
			else
			{
				/* Negative Delta */
				delta = dsw->compare_item - d;
				positive_delta = false;
			}

			break;
		default:
			ereport(FATAL,
					(errmsg("DELTA compression fixed length type has unexpected length %d",
							dsw->typeInfo->datumlen),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			break;
	}

	/*
	 * Check if delta value fits the storage reserved for it. Important is to
	 * also check for overflow case, where delta goes negative. As logic above
	 * always subtracts smaller number from larger, delta must be positive
	 * except overflow case.
	 */
	if (delta < 0 || delta > MAX_DELTA_SUPPORTED_DELTA_COMPRESSION)
	{
		return DELTA_COMPRESSION_NOT_APPLIED;
	}

	if (!DatumStreamBlockWrite_DenseHasSpaceDelta(dsw))
	{
		return DELTA_COMPRESSION_ERROR;
	}

	/* Update the compare_item now to be new value */
	switch (dsw->typeInfo->datumlen)
	{
		case 4:
			*(uint32 *) (&dsw->compare_item) = DatumGetUInt32(d);
			break;
		case 8:
			dsw->compare_item = d;
			break;
		default:
			ereport(FATAL,
					(errmsg("DELTA Compression fixed length type has unexpected length %d",
							dsw->typeInfo->datumlen),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			break;
	}

	DatumStreamBlockWrite_DeltaAdd(dsw, delta, positive_delta);

	return DELTA_COMPRESSION_OK;
}

/*
 * The Dense and optially RLE_TYPE version of datumstream_put.
 */
static int
DatumStreamBlockWrite_PutDense(
							   DatumStreamBlockWrite * dsw,
							   Datum d,
							   bool null,
							   void **toFree)
{
	uint8	   *item_beginp;

	bool		havePreviousValueToLookAt;
	bool		isEqual;
	uint8	   *rle_last_item;

	Assert(dsw);
	*toFree = NULL;

	Delta_Compression_status delta_status;

#ifdef USE_ASSERT_CHECKING
	DatumStreamBlockWrite_CheckDenseInvariant(dsw);
#endif

	if (null)
	{
		if (!DatumStreamBlockWrite_DenseHasSpaceNull(dsw))
		{
			/*
			 * Too many items, or not enough room to add a NULL bit-map data.
			 */
			return -1;
		}

		DatumStreamBlockWrite_MakeNullBitMapSpace(dsw);

		DatumStreamBlockWrite_DenseIncrNull(dsw);

		if (Debug_appendonly_print_insert_tuple)
		{
			ereport(LOG,
					(errmsg("Datum stream insert Dense NULL for "
							"(nth %d, new NULL bit-map count %d)",
							dsw->nth,
							dsw->always_null_bitmap_count),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
		return 0;
	}

	/*
	 * Not a NULL.	We have an item to add.
	 */
	/*
	 * But first, do we have a previous value to look at?
	 */
	havePreviousValueToLookAt =
		(dsw->rle_want_compression &&
		 dsw->rle_last_item != NULL);

	/*
	 * All the DeltaRange datatypes supported have FIXED length and
	 * hence don't need to check for the same in the below Variable length for DeltaRange
	 */

	if (dsw->typeInfo->datumlen < 0)
	{
		/* Variable length. */
		uint8	   *dataStart;
		int32		dataLen;

		int32		sz = 0;
		char	   *p = NULL;
		char		c1 = 0;
		int32		wsz = 0;
		Datum		originalDatum;
		bool		wasExtended = false;

		Datum		storedDatum;
		uint8	   *storedDataStart;
		int32		storedDataLen;
		void	   *storedToFree;

		/* Variable length */
		originalDatum = d;

		/*
		 * If toFree comes back non-NULL, we have created a palloc'd de-toasted and/or
		 * de-compressed varlena copy.
		 */
		if (dsw->typeInfo->datumlen == -1)
		{
			varattrib_untoast_ptr_len(
									  d,
									  (char **) &dataStart,
									  &dataLen,
									  toFree);
			if (*toFree != NULL)
			{
				d = PointerGetDatum(*toFree);
				wasExtended = true;
			}
			else
			{
				wasExtended = false;
			}
		}
		else
		{
			dataLen = 0;
			dataStart = NULL;
		}

		if (Debug_datumstream_write_print_small_varlena_info)
		{
			DatumStreamBlockWrite_PrintInputVarlenaInfo(
														dsw,
														originalDatum,
														wasExtended);
		}

		if (dsw->rle_last_item_is_repeated &&
			dsw->rle_repeatcounts[dsw->rle_repeatcounts_count - 1] >= MAXREPEAT_COUNT)
		{
			/*
			 * Reguardless of whether new is equal, we need to finalized the repeated item.
			 */
			Assert(dsw->rle_has_compression);
			DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
		}
		else if (havePreviousValueToLookAt)
		{
			if (Debug_appendonly_print_insert_tuple)
			{
				ereport(LOG,
						(errmsg("Datum stream insert has previous Dense variable-length item to look at "
								"(control block %p, nth %d, new item size %d, new item %p, last item size %d, last item %p, data buffer %p, (next item) datum pointer %p, data buffer after %p)",
								dsw,
								dsw->nth,
								dataLen,
								dataStart,
								dsw->rle_last_item_size,
								dsw->rle_last_item,
								dsw->datum_buffer,
								dsw->datump,
								dsw->datum_afterp),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			if (dataLen != dsw->rle_last_item_size || !dataStart)
			{
				isEqual = false;
			}
			else
			{
				isEqual =
					(memcmp(dsw->rle_last_item, dataStart, dsw->rle_last_item_size) == 0);
			}

			if (Debug_appendonly_print_insert_tuple)
			{
				ereport(LOG,
						(errmsg("Datum stream insert Dense variable-length possible repeated item %s "
								"(control block %p, nth %d)",
								(isEqual ? "EQUAL" : "NOT EQUAL"),
								dsw,
								dsw->nth),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}

			if (isEqual)
			{
				if (!DatumStreamBlockWrite_RleMarkRepeat(dsw))
				{
					return -1;
				}

				return 0;
			}

			/*
			 * NOT EQUAL.
			 */
			if (dsw->rle_last_item_is_repeated)
			{
				Assert(dsw->rle_has_compression);
				DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
			}
		}

		if (dsw->typeInfo->datumlen == -2)
		{
			sz = strlen(DatumGetCString(d)) + 1;
			p = DatumGetPointer(d);
			wsz = sz;
		}
		else if (VARATT_IS_SHORT(DatumGetPointer(d)))
		{
			sz = VARSIZE_SHORT(DatumGetPointer(d));
			p = DatumGetPointer(d);
			wsz = sz;
		}
		else if (value_type_could_short(DatumGetPointer(d), dsw->typeInfo->typid))
		{
			sz = VARATT_CONVERTED_SHORT_SIZE(DatumGetPointer(d));
			c1 = VARSIZE_TO_SHORT_D(d);
			p = VARDATA(DatumGetPointer(d));
			wsz = sz - 1;
		}
		else
		{
			sz = VARSIZE(DatumGetPointer(d));
			dsw->datump = (uint8 *) att_align_zero((char *) dsw->datump, dsw->typeInfo->align);
			p = DatumGetPointer(d);
			wsz = sz;
		}

		if (!DatumStreamBlockWrite_DenseHasSpaceItem(dsw, sz))
		{
			return -sz;
		}

		/*
		 * Set item begin pointer after we have done zero padding.
		 */
		item_beginp = dsw->datump;

		if (c1 != 0)
		{
			*(dsw->datump) = c1;
			dsw->datump += 1;
		}

		memcpy(dsw->datump, p, wsz);

		if (Debug_datumstream_write_print_small_varlena_info)
		{
			DatumStreamBlockWrite_PrintStoredSmallVarlenaInfo(
															  dsw,
															  item_beginp);
		}

		dsw->datump += wsz;

		storedDatum = PointerGetDatum(item_beginp);

		if (dsw->typeInfo->datumlen == -1)
		{
			varattrib_untoast_ptr_len(
									  storedDatum,
									  (char **) &storedDataStart,
									  &storedDataLen,
									  &storedToFree);
			Assert(storedToFree == NULL);
		}
		else
		{
			Assert(dsw->typeInfo->datumlen == -2);
			storedDataStart = (uint8 *) DatumGetCString(storedDatum);
			storedDataLen = strlen(DatumGetCString(storedDatum)) + 1;
		}
		DatumStreamBlockWrite_DenseIncrItem(
											dsw,
											storedDataStart,
											storedDataLen);

		if (Debug_appendonly_print_insert_tuple)
		{
			ereport(LOG,
			   (errmsg("Datum stream block write Dense variable-length item "
					   "(nth %d, item size %d, item begin %p, item offset " INT64_FORMAT ", next item begin %p, datum buffer after %p)",
					   dsw->nth,
					   sz,
					   item_beginp,
					   (int64) (item_beginp - dsw->datum_buffer),
					   dsw->datump,
					   dsw->datum_afterp),
				errdetail_datumstreamblockwrite(dsw),
				errcontext_datumstreamblockwrite(dsw)));
		}

		return sz;
	}

	/* Fixed length */

	item_beginp = dsw->datump;

	if (dsw->rle_last_item_is_repeated &&
	dsw->rle_repeatcounts[dsw->rle_repeatcounts_count - 1] >= MAXREPEAT_COUNT)
	{
		/*
		 * Reguardless of whether new is equal, we need to finalized the repeated item.
		 */
		Assert(dsw->rle_has_compression);
		DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
	}
	else if (havePreviousValueToLookAt)
	{
		/*
		 * Do we have a fixed length REPEATED value?
		 */
		if (dsw->rle_last_item_size != dsw->typeInfo->datumlen)
		{
			elog(ERROR, "Last data item size %d doesn't match type datum length %d",
				 dsw->rle_last_item_size,
				 dsw->typeInfo->datumlen);
		}

		Assert(dsw->rle_last_item_size == dsw->typeInfo->datumlen);

		if (!dsw->typeInfo->byval)
		{
			isEqual =
				(memcmp(dsw->rle_last_item, DatumGetPointer(d),
						dsw->typeInfo->datumlen) == 0);
		}
		else
		{
			switch (dsw->typeInfo->datumlen)
			{
				case 1:
					isEqual = (*(dsw->rle_last_item) == DatumGetChar(d));
					break;
				case 2:
					Assert(IsAligned(dsw->datump, 2));
					isEqual = (*(uint16 *) (dsw->rle_last_item) == DatumGetUInt16(d));
					break;
				case 4:
					Assert(IsAligned(dsw->datump, 4));
					isEqual = (*(uint32 *) (dsw->rle_last_item) == DatumGetUInt32(d));
					break;
				case 8:
					Assert(IsAligned(dsw->datump, 8) || IsAligned(dsw->datump, 4));
					isEqual = (*(Datum *) (dsw->rle_last_item) == d);
					break;
				default:
					ereport(FATAL,
							(errmsg("fixed length type has strange length %d",
									dsw->typeInfo->datumlen),
							 errdetail_datumstreamblockwrite(dsw),
							 errcontext_datumstreamblockwrite(dsw)));
					isEqual = false;
					break;
			}
		}

		if (isEqual)
		{
			if (!DatumStreamBlockWrite_RleMarkRepeat(dsw))
			{
				/*
				 * Not enough space to add meta data to record a repeat.
				 */
				return -1;
			}

			return 0;
		}

		/*
		 * NOT EQUAL.
		 */
		if (Debug_appendonly_print_insert_tuple)
		{
			ereport(LOG,
					(errmsg("Datum stream insert Dense fixed-length possible repeated item NOT EQUAL "
							"(nth %d, item size %d, item begin %p, next item begin %p, datum buffer after %p)",
							dsw->nth,
							dsw->typeInfo->datumlen,
							item_beginp,
							dsw->datump,
							dsw->datum_afterp),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}

		if (dsw->rle_last_item_is_repeated)
		{
			Assert(dsw->rle_has_compression);
			DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
		}

		/*
		 * Fall through and see if there is enough space for new item.
		 */
	}

	if (dsw->delta_want_compression)
	{
		/*
		 * RLE done with its checks and if we are here means not repeated/exceding the MAX_REPEAT_COUNT
		 * in either case, now should check for Delta if supported and if it falls within range,
		 * should store Delta instead of storing the pysical datum
		 */
		delta_status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, d);
		/* Should return from here if we have performed DeltaCompression */
		switch (delta_status)
		{
			case DELTA_COMPRESSION_OK:
				return 0;
			case DELTA_COMPRESSION_ERROR:
				return -1;
			case DELTA_COMPRESSION_NOT_APPLIED:
				break;
				/* FALL through */
		}
	}

	if (!DatumStreamBlockWrite_DenseHasSpaceItem(dsw, dsw->typeInfo->datumlen))
	{
		/*
		 * Not enough space for the new item.
		 */
		return -dsw->typeInfo->datumlen;
	}

	/*
	 * Remember the beginning of the new item for DatumStreamBlockWrite_DenseIncrItem below.
	 */
	rle_last_item = dsw->datump;

	DatumStreamBlockWrite_PutFixedLength(dsw, d);

	DatumStreamBlockWrite_DenseIncrItem(dsw, rle_last_item, dsw->typeInfo->datumlen);

	if (dsw->delta_want_compression)
	{
		DatumStreamBlockWrite_DeltaMaintain(dsw, d);
	}

	if (Debug_appendonly_print_insert_tuple)
	{
		DatumStreamBlockWrite_PutFixedLengthTrace(dsw, item_beginp);
	}

	return dsw->typeInfo->datumlen;
}

int
DatumStreamBlockWrite_Put(
						  DatumStreamBlockWrite * dsw,
						  Datum datum,
						  bool null,
						  void **toFree)
{
	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	switch (dsw->datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			return DatumStreamBlockWrite_PutOrig(dsw, datum, null, toFree);

		case DatumStreamVersion_Dense:
		case DatumStreamVersion_Dense_Enhanced:
			{
				int			result;

				result = DatumStreamBlockWrite_PutDense(dsw, datum, null, toFree);

#ifdef USE_ASSERT_CHECKING
				/*
				 * Check afterwards to verify invariants of latest write.
				 */
				DatumStreamBlockWrite_CheckDenseInvariant(dsw);
#endif
				return result;
			}

		default:
			ereport(FATAL,
					(errmsg("Unexpected datum stream version %d",
							dsw->datumStreamVersion),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			return 0;
			/* Never reaches here. */
	}
}

int
DatumStreamBlockWrite_Nth(DatumStreamBlockWrite * dsw)
{
	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	return dsw->nth;
}

void
DatumStreamBlockWrite_GetReady(
							   DatumStreamBlockWrite * dsw)
{
	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	dsw->nth = 0;
	dsw->physical_datum_count = 0;

	dsw->always_null_bitmap_count = 0;

	dsw->remember_savings = dsw->savings;

	dsw->has_null = false;
	DatumStreamBitMapWrite_Init(
								&dsw->null_bitmap,
								dsw->null_bitmap_buffer,
								dsw->null_bitmap_buffer_size);


	switch (dsw->datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			dsw->datump = dsw->datum_buffer;
			break;

		case DatumStreamVersion_Dense:
		case DatumStreamVersion_Dense_Enhanced:
			dsw->datump = dsw->datum_buffer;

			if (dsw->rle_want_compression)
			{
				/* Set up for RLETYPE compression */
				dsw->rle_has_compression = false;

				dsw->rle_last_item = NULL;
				dsw->rle_last_item_size = 0;
				dsw->rle_last_item_is_repeated = false;

				dsw->rle_total_repeat_items_written = 0;

				DatumStreamBitMapWrite_Init(
											&dsw->rle_compress_bitmap,
											dsw->rle_compress_bitmap_buffer,
									   dsw->rle_compress_bitmap_buffer_size);

				dsw->rle_repeatcounts_count = 0;
				dsw->rle_repeatcounts_current_size = 0;
			}

			if (dsw->delta_want_compression)
			{
				/* Set up for RLETYPE with delta compression */
				dsw->delta_has_compression = false;

				DatumStreamBitMapWrite_Init(
											&dsw->delta_bitmap,
											dsw->delta_bitmap_buffer,
											dsw->delta_bitmap_buffer_size);

				dsw->deltas_count = 0;
				dsw->deltas_current_size = 0;

				dsw->not_first_datum = false;
				dsw->compare_item = 0;
			}

			break;

		default:
			ereport(FATAL,
					(errmsg("Unexpected datum stream version %d",
							dsw->datumStreamVersion),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			break;
			/* Never reached. */
	}
}

static int64
DatumStreamBlockWrite_BlockOrig(
								DatumStreamBlockWrite * dsw,
								uint8 * buffer)
{
	uint8	   *p;
	DatumStreamBlock_Orig block;
	int32		unalignedNullSize;
	int32		rowCount;
	int64		writesz;
	bool		minimalIntegrityChecks;

	p = buffer;

	/* First write header */
	block.version = DatumStreamVersion_Original;
	block.flags = dsw->has_null ? DSB_HAS_NULLBITMAP : 0;
	block.ndatum = dsw->nth;
	block.unused = 0;
/* NOTE:Unfortunately, this was not zeroed in the earlier releases of the code. */

	/* compress null bitmaps */
	if (!dsw->has_null)
	{
		unalignedNullSize = 0;
		block.nullsz = 0;
	}
	else
	{
		Assert(
			   DatumStreamBitMapWrite_Count(&dsw->null_bitmap) == dsw->nth);

		unalignedNullSize = DatumStreamBitMapWrite_Size(&dsw->null_bitmap);
		block.nullsz = MAXALIGN(unalignedNullSize);
	}

	block.sz = dsw->datump - dsw->datum_buffer;

	/*
	 * Serialize the different data in to the write buffer.
	 */
	memcpy(p, &block, sizeof(DatumStreamBlock_Orig));
	p += sizeof(DatumStreamBlock_Orig);

	/* Next write the null bitmap */
	if (dsw->has_null)
	{
		int32		nullPadSize;
		int32		pad;

		memcpy(p, dsw->null_bitmap_buffer, unalignedNullSize);
		p += unalignedNullSize;

		/*
		 * Zero pad after the NULL bit-map to just before the aligned datum data.
		 */
		nullPadSize = block.nullsz - unalignedNullSize;
		for (pad = 0; pad < nullPadSize; pad++)
		{
			*(p++) = 0;
		}
	}

	/* Next write data */
	memcpy(p, dsw->datum_buffer, block.sz);
	p += block.sz;

	/* Calculate write size. */
	writesz = p - buffer;
	rowCount = dsw->nth;

	if (Debug_appendonly_print_insert)
	{
		if (!dsw->has_null)
		{
			ereport(LOG,
					(errmsg("Datum stream write Original block formatted with NO NULLs "
				 "(total length = %d, logical row count %d, data length %d)",
							(int32) writesz,
							rowCount,
							block.sz),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
		else
		{
			ereport(LOG,
					(errmsg("Datum stream write Original block formatted with NULLs for "
							"(total length = %d, logical row count %d, null bit-map ON count %d, null bit-map aligned length %d, data length %d)",
							(int32) writesz,
							rowCount,
							DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
							block.nullsz,
							block.sz),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
	}


#ifdef USE_ASSERT_CHECKING
	minimalIntegrityChecks = false;
#else
	minimalIntegrityChecks = true;
#endif
	if (Debug_datumstream_block_write_check_integrity)
	{
		minimalIntegrityChecks = false;
	}
	DatumStreamBlock_IntegrityCheckOrig(
										buffer,
										(int32) writesz,
										minimalIntegrityChecks,
										rowCount,
										dsw->typeInfo,
			/* errdetailCallback */ errdetail_datumstreamblockwrite_callback,
										 /* errdetailArg */ (void *) dsw,
		  /* errcontextCallback */ errcontext_datumstreamblockwrite_callback,
										 /* errcontextArg */ (void *) dsw);

	return writesz;
}

static int64
DatumStreamBlockWrite_BlockDense(
								 DatumStreamBlockWrite * dsw,
								 uint8 * buffer)
{
	int64		writesz = 0;
	uint8	   *p = NULL;
	DatumStreamBlock_Dense dense;
	DatumStreamBlock_Rle_Extension rle_extension;
	DatumStreamBlock_Delta_Extension delta_extension;
	int32		headerSize;
	int32		nullSize;
	int32		rleSize;
	int32		deltaSize;
	int32		metadataSize;
	int32		metadataMaxAlignSize;
	int32		nullPadSize;
	int32		pad;
	int32		rowCount;
	int32		totalRepeatCountsSize;
	int32		totalDeltasSize;
	int64		formattedMetadataSize;
	bool		minimalIntegrityChecks;

	totalRepeatCountsSize = 0;
	totalDeltasSize = 0;

	/*
	 * Maintain compression data structures.
	 */
	if (dsw->rle_last_item_is_repeated)
	{
		Assert(dsw->rle_has_compression);
		DatumStreamBlockWrite_RleFinalizeRepeatCountSize(dsw);
	}

	p = buffer;

	/* First fill in orig header portion */
	dense.orig_4_bytes.version = dsw->datumStreamVersion;
	dense.orig_4_bytes.flags = dsw->has_null ? DSB_HAS_NULLBITMAP : 0;
	if (dsw->rle_has_compression)
	{
		dense.orig_4_bytes.flags |= DSB_HAS_RLE_COMPRESSION;
	}

	if (dsw->delta_has_compression)
	{
		dense.orig_4_bytes.flags |= DSB_HAS_DELTA_COMPRESSION;
	}

	dense.logical_row_count = dsw->nth;
	dense.physical_datum_count = dsw->physical_datum_count;
	dense.physical_data_size = dsw->datump - dsw->datum_buffer;

	headerSize = sizeof(DatumStreamBlock_Dense);

	/*
	 * Add in NULL bit-map, extra DatumStreamBlock_Rle struct, compress bit-map, repeated counts...
	 */

	if (dsw->has_null)
	{
		nullSize = DatumStreamBitMapWrite_Size(&dsw->null_bitmap);
	}
	else
	{
		nullSize = 0;
	}

	if (dsw->rle_has_compression)
	{
		/*
		 * Add in compress bitmap and repeat counts byte lengths,
		 * if we have done compression in this block.
		 */
		headerSize += sizeof(DatumStreamBlock_Rle_Extension);

		if (dsw->has_null)
		{
			rle_extension.norepeats_null_bitmap_count =
				DatumStreamBitMapWrite_Count(&dsw->null_bitmap);
		}
		else
		{
			rle_extension.norepeats_null_bitmap_count = 0;
		}

		/*
		 * Compress bit-map byte size since we don't add bits when NULL;
		 */
		rleSize = DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap);

		rle_extension.compress_bitmap_count =
			DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap);

		/*
		 * Repeat counts count and size.
		 */
		rleSize += dsw->rle_repeatcounts_current_size;

		rle_extension.repeatcounts_count = dsw->rle_repeatcounts_count;
		rle_extension.repeatcounts_size = dsw->rle_repeatcounts_current_size;

		/*
		 * We charge the compression metadata size against the RLE_TYPE savings.
		 */
		dsw->savings -= (sizeof(DatumStreamBlock_Rle_Extension) + rleSize);
	}
	else
	{
		rleSize = 0;
	}

	/*
	 * Add in extra DatumStreamBlock_Delta struct, Delta bit-map, deltas...
	 */

	if (dsw->delta_has_compression)
	{
		/*
		 * Add in Delta bitmap and deltas byte lengths,
		 * if we have done compression in this block.
		 */
		headerSize += sizeof(DatumStreamBlock_Delta_Extension);

		/*
		 * Delta bit-map byte size since we don't add bits when NULL;
		 */
		deltaSize = DatumStreamBitMapWrite_Size(&dsw->delta_bitmap);

		delta_extension.delta_bitmap_count =
			DatumStreamBitMapWrite_Count(&dsw->delta_bitmap);

		/*
		 * Repeat counts count and size.
		 */
		deltaSize += dsw->deltas_current_size;

		delta_extension.deltas_count = dsw->deltas_count;
		delta_extension.deltas_size = dsw->deltas_current_size;

		/*
		 * We charge the compression metadata size against the RLE_TYPE with Delta savings.
		 */
		dsw->savings -= (sizeof(DatumStreamBlock_Delta_Extension) + deltaSize);
	}
	else
	{
		deltaSize = 0;
	}

	/*
	 * Align headers and meta-data (e.g. NULL bit-maps, etc).
	 */
	metadataSize = headerSize + nullSize + rleSize + deltaSize;
	metadataMaxAlignSize = MAXALIGN(metadataSize);

	memcpy(p, &dense, sizeof(DatumStreamBlock_Dense));
	p += sizeof(DatumStreamBlock_Dense);

	if (dsw->rle_has_compression)
	{
		memcpy(p, &rle_extension, sizeof(DatumStreamBlock_Rle_Extension));
		p += sizeof(DatumStreamBlock_Rle_Extension);
	}

	if (dsw->delta_has_compression)
	{
		memcpy(p, &delta_extension, sizeof(DatumStreamBlock_Delta_Extension));
		p += sizeof(DatumStreamBlock_Delta_Extension);
	}

	if (dsw->has_null)
	{
		memcpy(p, dsw->null_bitmap_buffer, DatumStreamBitMapWrite_Size(&dsw->null_bitmap));
		p += DatumStreamBitMapWrite_Size(&dsw->null_bitmap);
	}

	if (dsw->rle_has_compression)
	{
		int			i;

		/*
		 * Compress bit-map.
		 */
		memcpy(p, dsw->rle_compress_bitmap_buffer, DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap));
		p += DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap);

		/*
		 * Write out optimal Repeat Count integer sizes.
		 */
		Assert(totalRepeatCountsSize == 0);
		for (i = 0; i < dsw->rle_repeatcounts_count; i++)
		{
			int			byteLen;

			Assert(totalRepeatCountsSize +
				   DatumStreamInt32Compress_Size(dsw->rle_repeatcounts[i])
				   <= dsw->rle_repeatcounts_current_size);

			byteLen = DatumStreamInt32Compress_Encode(p, dsw->rle_repeatcounts[i]);
			p += byteLen;
			totalRepeatCountsSize += byteLen;
		}
	}

	/* Add Delta compress bitmap and Deltas */
	if (dsw->delta_has_compression)
	{
		int			i;

		/*
		 * Compress bit-map.
		 */
		memcpy(p, dsw->delta_bitmap_buffer, DatumStreamBitMapWrite_Size(&dsw->delta_bitmap));
		p += DatumStreamBitMapWrite_Size(&dsw->delta_bitmap);

		/*
		 * Write out optimal Deltas integer sizes.
		 */
		Assert(totalDeltasSize == 0);
		for (i = 0; i < dsw->deltas_count; i++)
		{
			int			byteLen;

			Assert(totalDeltasSize +
				   DatumStreamInt32CompressReserved3_Size(dsw->deltas[i])
				   <= dsw->deltas_current_size);

			byteLen = DatumStreamInt32CompressReserved3_Encode(p, dsw->deltas[i], dsw->delta_sign[i]);
			p += byteLen;
			totalDeltasSize += byteLen;
		}
	}

	/*
	 * Were our meta-data size calculations correct?
	 */
	formattedMetadataSize = (p - buffer);
	if (formattedMetadataSize != metadataSize)
	{
		ereport(ERROR,
				(errmsg("Formatted datum stream write metasize size different (expected %d, found " INT64_FORMAT ")",
						metadataSize,
						formattedMetadataSize),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
	}

	/*
	 * Zero pad after metadata to just before the aligned datum data.
	 */
	nullPadSize = metadataMaxAlignSize - metadataSize;
	for (pad = 0; pad < nullPadSize; pad++)
	{
		*(p++) = 0;
	}

	/* Next write data */
	if (metadataMaxAlignSize + dense.physical_data_size > dsw->maxDataBlockSize)
	{
		ereport(ERROR,
				(errmsg("Formatted datum stream MAXALIGN metadata size %d + physical datum size %d "
						"(total %d, metadata size %d, header size %d, null size %d, RLE_TYPE size %d) would exceed maximum data blocksize %d)",
						metadataMaxAlignSize,
						dense.physical_data_size,
						metadataMaxAlignSize + dense.physical_data_size,
						metadataSize,
						headerSize,
						nullSize,
						rleSize,
						dsw->maxDataBlockSize),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
	}

	memcpy(p, dsw->datum_buffer, dense.physical_data_size);
	p += dense.physical_data_size;

	/* Calculate write size. */
	writesz = p - buffer;
	rowCount = dsw->nth;

	if (Debug_appendonly_print_insert)
	{
		if (!dsw->rle_has_compression)
		{
			if (!dsw->has_null)
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted with NO NULLs "
								"(maximum length %d, total length = %d, logical row count and physical datum count %d, "
								"metadata size %d, metadata size MAXALIGN %d, header size %d, null size %d, RLE_TYPE size %d, "
								"physical data size %d)",
								dsw->maxDataBlockSize,
								(int32) writesz,
								rowCount,
								metadataSize,
								metadataMaxAlignSize,
								headerSize,
								nullSize,
								rleSize,
								dense.physical_data_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			else
			{
				ereport(LOG,
				(errmsg("Datum stream write Dense block formatted with NULLs "
						"(maximum length %d, total length = %d, logical row count %d, physical datum count %d, "
						"null bit-map count %d, null bit-map ON count %d, null bit-map size %d, "
						"metadata size %d, metadata size MAXALIGN %d, header size %d, null size %d, RLE_TYPE size %d, "
						"physical data size %d)",
						dsw->maxDataBlockSize,
						(int32) writesz,
						rowCount,
						dsw->physical_datum_count,
						DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
						DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
						DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
						metadataSize,
						metadataMaxAlignSize,
						headerSize,
						nullSize,
						rleSize,
						dense.physical_data_size),
				 errdetail_datumstreamblockwrite(dsw),
				 errcontext_datumstreamblockwrite(dsw)));
			}
		}
		else
		{
			if (!dsw->has_null)
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted RLE_TYPE block with NO NULLs "
								"(maximum length %d, total length = %d, logical row count %d, physical datum count %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, "
								"repeat counts count %d, repeat counts size %d, output repeat counts size %d, "
							  "metadata size %d, metadata size MAXALIGN %d, "
								"additional savings " INT64_FORMAT ", total savings " INT64_FORMAT ", "
								"physical data size %d)",
								dsw->maxDataBlockSize,
								(int32) writesz,
								rowCount,
								dsw->physical_datum_count,
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size,
								totalRepeatCountsSize,
								metadataSize,
								metadataMaxAlignSize,
								dsw->savings - dsw->remember_savings,
								dsw->savings,
								dense.physical_data_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			else
			{
				ereport(LOG,
						(errmsg("Datum stream write Dense block formatted RLE_TYPE block with NULLs "
								"(maximum length %d, total length = %d, logical row count %d, physical datum count %d, "
								"null bit-map count %d, null bit-map ON count %d, null bit-map size %d, "
								"compression bit-map count %d, compression bit-map ON count %d, compression bit-map size %d, "
								"repeat counts count %d, repeat count size %d, output repeat counts size %d, "
							  "metadata size %d, metadata size MAXALIGN %d, "
								"additional savings " INT64_FORMAT ", total savings " INT64_FORMAT ", "
								"physical data size %d)",
								dsw->maxDataBlockSize,
								(int32) writesz,
								rowCount,
								dsw->physical_datum_count,
							 DatumStreamBitMapWrite_Count(&dsw->null_bitmap),
						   DatumStreamBitMapWrite_OnCount(&dsw->null_bitmap),
							  DatumStreamBitMapWrite_Size(&dsw->null_bitmap),
					 DatumStreamBitMapWrite_Count(&dsw->rle_compress_bitmap),
				   DatumStreamBitMapWrite_OnCount(&dsw->rle_compress_bitmap),
					  DatumStreamBitMapWrite_Size(&dsw->rle_compress_bitmap),
								dsw->rle_repeatcounts_count,
								dsw->rle_repeatcounts_current_size,
								totalRepeatCountsSize,
								metadataSize,
								metadataMaxAlignSize,
								dsw->savings - dsw->remember_savings,
								dsw->savings,
								dense.physical_data_size),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
		}

		if (dsw->delta_has_compression)
		{
			ereport(LOG,
					(errmsg("Datum stream write Dense block formatted RLE_TYPE with DELTA compression "
							"delta bit-map count %d, delta bit-map ON count %d, delta bit-map size %d, "
				   "deltas count %d, deltas size %d, output deltas size %d)",
							DatumStreamBitMapWrite_Count(&dsw->delta_bitmap),
						  DatumStreamBitMapWrite_OnCount(&dsw->delta_bitmap),
							DatumStreamBitMapWrite_Size(&dsw->delta_bitmap),
							dsw->deltas_count,
							dsw->deltas_current_size,
							totalDeltasSize),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
		}
	}

#ifdef USE_ASSERT_CHECKING
	minimalIntegrityChecks = false;
#else
	minimalIntegrityChecks = true;
#endif
	if (Debug_datumstream_block_write_check_integrity)
	{
		minimalIntegrityChecks = false;
	}
	DatumStreamBlock_IntegrityCheckDense(
										 buffer,
										 (int32) writesz,
										 minimalIntegrityChecks,
										 rowCount,
										 dsw->typeInfo,
			/* errdetailCallback */ errdetail_datumstreamblockwrite_callback,
										  /* errdetailArg */ (void *) dsw,
		  /* errcontextCallback */ errcontext_datumstreamblockwrite_callback,
										  /* errcontextArg */ (void *) dsw);

	return writesz;
}

int64
DatumStreamBlockWrite_Block(
							DatumStreamBlockWrite * dsw,
							uint8 * buffer)
{
	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	switch (dsw->datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			return DatumStreamBlockWrite_BlockOrig(dsw, buffer);

		case DatumStreamVersion_Dense:
		case DatumStreamVersion_Dense_Enhanced:
			return DatumStreamBlockWrite_BlockDense(dsw, buffer);

		default:
			ereport(FATAL,
					(errmsg("Unexpected datum stream version %d",
							dsw->datumStreamVersion),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			return 0;
			/* Never reaches here. */
	}
}

void
DatumStreamBlockWrite_Init(
						   DatumStreamBlockWrite * dsw,
						   DatumStreamTypeInfo * typeInfo,
						   DatumStreamVersion datumStreamVersion,
						   bool rle_want_compression,
						   bool delta_want_compression,
						   int32 initialMaxDatumPerBlock,
						   int32 maxDatumPerBlock,
						   int32 maxDataBlockSize,
						   int (*errdetailCallback) (void *errdetailArg),
						   void *errdetailArg,
						   int (*errcontextCallback) (void *errcontextArg),
						   void *errcontextArg)
{
	memcpy(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen);

	dsw->typeInfo = typeInfo;

	dsw->datumStreamVersion = datumStreamVersion;

	dsw->rle_want_compression = rle_want_compression;
	dsw->delta_want_compression = delta_want_compression;

	dsw->initialMaxDatumPerBlock = initialMaxDatumPerBlock;
	dsw->maxDatumPerBlock = maxDatumPerBlock;

	dsw->maxDataBlockSize = maxDataBlockSize;

	dsw->errdetailCallback = errdetailCallback;
	dsw->errcontextArg = errcontextArg;
	dsw->errcontextCallback = errcontextCallback;
	dsw->errcontextArg = errcontextArg;

	dsw->memctxt = CurrentMemoryContext;

	/*
	 * Now start setting up our write buffers, etc.
	 */
	dsw->datum_buffer_size = dsw->maxDataBlockSize;
	dsw->datum_buffer = palloc(dsw->datum_buffer_size);
	dsw->datum_afterp = dsw->datum_buffer + dsw->datum_buffer_size;

	switch (dsw->datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			/*
			 * The maximum number of NULLs is regulated by the maximum number of rows that
			 * can be put in the Append-Only Small Content block header.
			 */
			if (Debug_datumstream_write_use_small_initial_buffers)
			{
				dsw->null_bitmap_buffer_size = 64;
			}
			else
			{
				dsw->null_bitmap_buffer_size = (dsw->initialMaxDatumPerBlock + 1) / 8;
			}
			dsw->null_bitmap_buffer = palloc(dsw->null_bitmap_buffer_size);

			if (Debug_appendonly_print_insert)
			{
				ereport(LOG,
						(errmsg("Datum stream block write created "
						"(maximum usable block space = %d, datum_buffer %p, "
					 "datumlen = %d, typid = %u, align '%c', by value = %s)",
								dsw->maxDataBlockSize,
								dsw->datum_buffer,
								dsw->typeInfo->datumlen,
								(uint32) dsw->typeInfo->typid,
								dsw->typeInfo->align,
								(dsw->typeInfo->byval ? "true" : "false")),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			break;

		case DatumStreamVersion_Dense:
		case DatumStreamVersion_Dense_Enhanced:
			if (Debug_datumstream_write_use_small_initial_buffers)
			{
				dsw->null_bitmap_buffer_size = 64;
			}
			else
			{
				dsw->null_bitmap_buffer_size = (dsw->initialMaxDatumPerBlock + 1) / 8;
			}
			dsw->null_bitmap_buffer = palloc(dsw->null_bitmap_buffer_size);

			if (dsw->rle_want_compression)
			{
				/*
				 * Start with lower than MAX, since MAX is huge.
				 */
				if (Debug_datumstream_write_use_small_initial_buffers)
				{
					dsw->rle_compress_bitmap_buffer_size = 8;
				}
				else
				{
					dsw->rle_compress_bitmap_buffer_size = (dsw->initialMaxDatumPerBlock + 1) / 8;
				}
				dsw->rle_compress_bitmap_buffer = palloc(dsw->rle_compress_bitmap_buffer_size);

				/*
				 * Worst case repeat is 2 different repeated values for whole block.
				 */
				dsw->rle_repeatcounts_count = 0;
				if (Debug_datumstream_write_use_small_initial_buffers)
				{
					dsw->rle_repeatcounts_maxcount = 16;
				}
				else
				{
					dsw->rle_repeatcounts_maxcount = dsw->initialMaxDatumPerBlock / 2;
				}
				dsw->rle_repeatcounts =
					palloc(dsw->rle_repeatcounts_maxcount * Int32Compress_MaxByteLen);
			}
			else
			{
				Assert(dsw->rle_compress_bitmap_buffer_size == 0);
				Assert(dsw->rle_compress_bitmap_buffer == NULL);
				Assert(dsw->rle_repeatcounts_count == 0);
				Assert(dsw->rle_repeatcounts_maxcount == 0);
				Assert(dsw->rle_repeatcounts == NULL);
			}

			if (dsw->delta_want_compression)
			{
				/*
				 * Start with lower than MAX, since MAX is huge.
				 */
				if (Debug_datumstream_write_use_small_initial_buffers)
				{
					dsw->delta_bitmap_buffer_size = 8;
				}
				else
				{
					dsw->delta_bitmap_buffer_size = (dsw->initialMaxDatumPerBlock + 1) / 8;
				}
				dsw->delta_bitmap_buffer = palloc(dsw->delta_bitmap_buffer_size);

				/*
				 * Worst case delta encoding is all datums in block having small
				 * Start with lower than MAX, since MAX is huge.
				 */
				dsw->deltas_count = 0;
				if (Debug_datumstream_write_use_small_initial_buffers)
				{
					dsw->deltas_maxcount = 16;
				}
				else
				{
					dsw->deltas_maxcount = dsw->initialMaxDatumPerBlock;
				}
				dsw->deltas =
					palloc(dsw->deltas_maxcount * Int32Compress_MaxByteLen);
				dsw->delta_sign =
					palloc(dsw->deltas_maxcount * sizeof(bool));
			}
			else
			{
				Assert(dsw->delta_bitmap_buffer_size == 0);
				Assert(dsw->delta_bitmap_buffer == NULL);
				Assert(dsw->deltas_count == 0);
				Assert(dsw->deltas_maxcount == 0);
				Assert(dsw->deltas == NULL);
				Assert(dsw->delta_sign == NULL);
			}

			if (Debug_appendonly_print_insert)
			{
				ereport(LOG,
						(errmsg("Datum stream block write created "
						"(maximum usable block space = %d, datum_buffer %p, "
					 "datumlen = %d, typid = %u, align '%c', by value = %s)",
								dsw->maxDataBlockSize,
								dsw->datum_buffer,
								dsw->typeInfo->datumlen,
								(uint32) dsw->typeInfo->typid,
								dsw->typeInfo->align,
								(dsw->typeInfo->byval ? "true" : "false")),
						 errdetail_datumstreamblockwrite(dsw),
						 errcontext_datumstreamblockwrite(dsw)));
			}
			break;

		default:
			ereport(FATAL,
					(errmsg("Unexpected datum stream version %d",
							dsw->datumStreamVersion),
					 errdetail_datumstreamblockwrite(dsw),
					 errcontext_datumstreamblockwrite(dsw)));
			return;
			/* Never reached. */
	}

	/* Set up our write block information */
	DatumStreamBlockWrite_GetReady(dsw);
}

void
DatumStreamBlockWrite_Finish(
							 DatumStreamBlockWrite * dsw)
{
	MemoryContext oldCtxt;

	if (strncmp(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen) != 0)
		elog(FATAL, "DatumStreamBlockWrite data structure not valid (eyecatcher)");

	oldCtxt = MemoryContextSwitchTo(dsw->memctxt);
	if (dsw->null_bitmap_buffer != NULL)
		pfree(dsw->null_bitmap_buffer);

	if (dsw->datum_buffer != NULL)
		pfree(dsw->datum_buffer);

	if (dsw->rle_compress_bitmap_buffer != NULL)
		pfree(dsw->rle_compress_bitmap_buffer);

	if (dsw->rle_repeatcounts != NULL)
		pfree(dsw->rle_repeatcounts);

	if (dsw->delta_bitmap_buffer != NULL)
		pfree(dsw->delta_bitmap_buffer);

	if (dsw->deltas != NULL)
		pfree(dsw->deltas);

	if (dsw->delta_sign != NULL)
		pfree(dsw->delta_sign);

	MemoryContextSwitchTo(oldCtxt);
}

/*
 * DatumStreamBlock.
 */
static int32
DatumStreamBlock_IntegrityCheckVarlena(
									   uint8 * physicalData,
									   int32 physicalDataSize,
									   DatumStreamVersion datumStreamVersion,
									   DatumStreamTypeInfo * typeInfo,
							   int (*errdetailCallback) (void *errdetailArg),
									   void *errdetailArg,
							 int (*errcontextCallback) (void *errcontextArg),
									   void *errcontextArg)
{
	int32		count;
	int32		currentOffset;
	uint8	   *p;

	Assert(typeInfo->datumlen == -1);
	Assert(physicalDataSize >= 0);
	if (physicalDataSize == 0)
	{
		return 0;
	}

	count = 0;

	p = physicalData;
	currentOffset = 0;
	while (true)
	{
		int32		remainingSize;
		int32		varLen;

		remainingSize = physicalDataSize - currentOffset;
		Assert(remainingSize > 0);

		/*
		 * Verify and move past any possible zero paddings AFTER PREVIOUS varlena data.
		 */
		if (currentOffset > 0 && *p == 0)
		{
			uint8	   *afterPadding;
			int32		saveBeginOffset;

			/*
			 * Note that SHORT varlena has the high bit of the first byte as 1, so
			 * we will not go here if item begins with SHORT header.
			 */

			afterPadding = (uint8 *) att_align_nominal(p, typeInfo->align);
			saveBeginOffset = currentOffset;
			while (p < afterPadding)
			{
				if (*p != 0)
				{
					ereport(ERROR,
							(errmsg("Bad datum stream %s variable-length item zero padding byte at offset %d is not zero (begin offset %d, physical item index #%d)",
							   DatumStreamVersion_String(datumStreamVersion),
									currentOffset,
									saveBeginOffset,
									count),
							 errdetailCallback(errdetailArg),
							 errcontextCallback(errcontextArg)));
				}
				p++;
				currentOffset++;
				remainingSize--;
				if (remainingSize <= 0)
				{
					break;
				}
			}
		}

		if (currentOffset >= physicalDataSize)
		{
			break;
		}

		/*
		 * Enough room for minimum varlena?
		 */
		if (VARATT_IS_EXTERNAL(p))
		{
			ereport(ERROR,
					(errmsg("Bad datum stream %s variable-length item at physical offset %d is corrupt (varlena is EXTERNAL, physical item index #%d)",
							DatumStreamVersion_String(datumStreamVersion),
							currentOffset,
							count),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
			varLen = 0;
			/* Never reaches here. */
		}
		else if (VARATT_IS_SHORT(p))
		{
			Assert(remainingSize >= 1);

			varLen = VARSIZE_SHORT(p);

			/*
			 * Check SHORT varlena length.
			 */
			if (varLen < VARHDRSZ_SHORT)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream %s variable-length item at physical offset %d.  SHORT varlena VARSIZE too short (size %d, remaining size %d, physical size %d, physical item index #%d, varlena: %s)",
								DatumStreamVersion_String(datumStreamVersion),
								currentOffset,
								varLen,
								remainingSize,
								physicalDataSize,
								count,
								VarlenaInfoToString(p)),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
			if (varLen > remainingSize)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream %s variable-length item at physical offset %d.  SHORT varlena size bad (size %d, remaining size %d, physical size %d, physical item index #%d, varlena: %s)",
								DatumStreamVersion_String(datumStreamVersion),
								currentOffset,
								varLen,
								remainingSize,
								physicalDataSize,
								count,
								VarlenaInfoToString(p)),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}
		else
		{
			if (remainingSize < VARHDRSZ)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream %s variable-length item at physical offset %d.  Remaining length %d too short for regular varlena 4 byte header (physical item index #%d)",
								DatumStreamVersion_String(datumStreamVersion),
								currentOffset,
								remainingSize,
								count),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

			if (currentOffset % 1 != 0)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream %s variable-length item at physical offset %d.  Not aligned on at least a 2 byte boundary for regular varlena header (physical item index #%d)",
								DatumStreamVersion_String(datumStreamVersion),
								currentOffset,
								count),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

			varLen = VARSIZE_ANY(p);

			if (varLen < VARHDRSZ)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream %s variable-length item at physical offset %d.  Regular varlena VARSIZE too short (length %d, remaining size %d, physical size %d, physical item index #%d, varlena: %s)",
								DatumStreamVersion_String(datumStreamVersion),
								currentOffset,
								varLen,
								remainingSize,
								physicalDataSize,
								count,
								VarlenaInfoToString(p)),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
			if (varLen > remainingSize)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream %s variable-length item at physical offset %d.  Regular varlena length bad (length %d, remaining size %d, physical size %d, physical item index #%d, varlena: %s)",
								DatumStreamVersion_String(datumStreamVersion),
								currentOffset,
								varLen,
								remainingSize,
								physicalDataSize,
								count,
								VarlenaInfoToString(p)),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}

		p += varLen;
		currentOffset += varLen;

		if (currentOffset >= physicalDataSize)
		{
			Assert(currentOffset == physicalDataSize);
			break;
		}

		count++;
	}

	return count;
}

static void
DatumStreamBlock_IntegrityCheckOrig(
									uint8 * buffer,
									int32 bufferSize,
									bool minimalIntegrityChecks,
									int32 expectedRowCount,
									DatumStreamTypeInfo * typeInfo,
							   int (*errdetailCallback) (void *errdetailArg),
									void *errdetailArg,
							 int (*errcontextCallback) (void *errcontextArg),
									void *errcontextArg)
{
	int32		minHeaderSize = sizeof(DatumStreamBlock_Orig);

	DatumStreamBlock_Orig *blockOrig;

	int32		headerSize;
	uint8	   *p;

	bool		hasNull;

	if (bufferSize < minHeaderSize)
	{
		ereport(ERROR,
				(errmsg("Bad datum stream Original block header size.  Found %d and expected the size to be at least %d",
						bufferSize,
						minHeaderSize),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	blockOrig = (DatumStreamBlock_Orig *) buffer;
	headerSize = minHeaderSize;
	p = buffer + headerSize;

	if (blockOrig->version != DatumStreamVersion_Original)
	{
		ereport(ERROR,
				(errmsg("Bad datum stream Original block version.  Found %d and expected %d",
						blockOrig->version,
						DatumStreamVersion_Original),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	if (minimalIntegrityChecks)
	{
		return;
	}

	/* CONSIDER: Verify reserved flags are Zero */

	hasNull = ((blockOrig->flags & DSB_HAS_NULLBITMAP) != 0);

	/* UNDONE: Add a whole bunch of other checking... */

	if (hasNull)
	{
		/*
		 * The field nullsz byte length is MAXALIGN.
		 */
		p += blockOrig->nullsz;
	}

	if (typeInfo->datumlen == -1)
	{
		/*
		 * Variable length items (i.e. varlena).
		 */
		DatumStreamBlock_IntegrityCheckVarlena(
											   p,
											   blockOrig->sz,
											   DatumStreamVersion_Original,
											   typeInfo,
											   errdetailCallback,
											   errdetailArg,
											   errcontextCallback,
											   errcontextArg);
	}
}

static void
DatumStreamBlock_IntegrityCheckDenseDelta(
						   DatumStreamBlock_Delta_Extension * deltaExtension,
										  uint8 * p,
										  int32 bufferSize,
										  int32 headerSize,
							   DatumStreamBlock_Rle_Extension * rleExtension,
							   int (*errdetailCallback) (void *errdetailArg),
										  void *errdetailArg,
							 int (*errcontextCallback) (void *errcontextArg),
										  void *errcontextArg)
{
	int32		deltaBitMapSize;
	int32		actualDeltasOnCount;
	int32		totalDeltasSize;
	int32		alignedHeaderSize;
	int			i;

	Assert(deltaExtension != NULL);
	Assert(p != NULL);

	if (deltaExtension->delta_bitmap_count <= 0)
	{
		ereport(ERROR,
				(errmsg("DELTA bit-map count is negative or 0 and is expected to be greater than 0. (%d)",
						deltaExtension->delta_bitmap_count),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	if (deltaExtension->deltas_count <= 0)
	{
		ereport(ERROR,
				(errmsg("DELTA deltas count is negative or 0 and is expected to be greater than 0. (%d)",
						deltaExtension->deltas_count),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	if (deltaExtension->deltas_size <= 0)
	{
		ereport(ERROR,
				(errmsg("DELTA deltas size is negative or 0 and is expected to be greater than 0. (%d)",
						deltaExtension->deltas_size),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	if (rleExtension != NULL)
	{
		if (deltaExtension->delta_bitmap_count != rleExtension->compress_bitmap_count)
		{
			ereport(ERROR,
					(errmsg("DELTA delta_bitmap count (%d) is expected to equal to RLE_TYPE compress bitmap count (%d)",
							deltaExtension->delta_bitmap_count,
							rleExtension->compress_bitmap_count),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}
	}

	deltaBitMapSize = DatumStreamBitMap_Size(deltaExtension->delta_bitmap_count);
	headerSize += deltaBitMapSize;

	if (bufferSize < headerSize)
	{
		ereport(ERROR,
				(errmsg("Expected RLE_TYPE header with DELTA size %d including NULL bit-map is larger than buffer size %d",
						headerSize,
						bufferSize),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	actualDeltasOnCount = DatumStreamBitMap_CountOn(p, deltaExtension->delta_bitmap_count);

	if (actualDeltasOnCount != deltaExtension->deltas_count)
	{
		ereport(ERROR,
				(errmsg("DELTA extension header bit-map ON count does not match DELTA bit-map ON count.  Found %d, expected %d",
						actualDeltasOnCount,
						deltaExtension->deltas_count),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	p += deltaBitMapSize;

	headerSize += deltaExtension->deltas_size;

	alignedHeaderSize = MAXALIGN(headerSize);

	if (bufferSize < alignedHeaderSize)
	{
		ereport(ERROR,
				(errmsg("Expected RLE_TYPE DELTA header size %d including deltas size is larger than buffer size %d",
						alignedHeaderSize,
						bufferSize),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	totalDeltasSize = 0;
	for (i = 0; i < deltaExtension->deltas_count; i++)
	{
		int32		deltasCount;
		int			byteLen;
		bool		sign;

		deltasCount = DatumStreamInt32CompressReserved3_Decode(p, &byteLen, &sign);
		/* UNDONE: Range check repeatCount */

		totalDeltasSize += byteLen;
		p += byteLen;
	}

	if (totalDeltasSize != deltaExtension->deltas_size)
	{
		ereport(ERROR,
				(errmsg("Bad DELTA type deltas size.  Found %d, expected %d",
						totalDeltasSize,
						deltaExtension->deltas_size),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}
}

static void
DatumStreamBlock_IntegrityCheckDense(
									 uint8 * buffer,
									 int32 bufferSize,
									 bool minimalIntegrityChecks,
									 int32 expectedRowCount,
									 DatumStreamTypeInfo * typeInfo,
							   int (*errdetailCallback) (void *errdetailArg),
									 void *errdetailArg,
							 int (*errcontextCallback) (void *errcontextArg),
									 void *errcontextArg)
{
	int32		minHeaderSize = sizeof(DatumStreamBlock_Dense);

	DatumStreamBlock_Dense *blockDense;

	int32		headerSize;
	uint8	   *p;

	bool		hasNull;
	bool		hasRleCompression;
	bool		hasDeltaCompression;

	int32		alignedHeaderSize;
	int32		deltaOnCount;
	DatumStreamBlock_Delta_Extension *deltaExtension;
	DatumStreamBlock_Rle_Extension *rleExtension;

	deltaExtension = NULL;
	rleExtension = NULL;

	alignedHeaderSize = 0;

	if (bufferSize < minHeaderSize)
	{
		ereport(ERROR,
				(errmsg("Bad datum stream Dense block header size.  Found %d and expected the size to be at least %d",
						bufferSize,
						minHeaderSize),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	blockDense = (DatumStreamBlock_Dense *) buffer;
	headerSize = minHeaderSize;
	p = buffer + headerSize;

	if ((blockDense->orig_4_bytes.version != DatumStreamVersion_Dense) &&
	 (blockDense->orig_4_bytes.version != DatumStreamVersion_Dense_Enhanced))
	{
		ereport(ERROR,
				(errmsg("Bad datum stream Dense block version.  Found %d and expected %d",
						blockDense->orig_4_bytes.version,
						DatumStreamVersion_Dense_Enhanced),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	if (minimalIntegrityChecks)
	{
		return;
	}

	/* CONSIDER: Verify reserved flags are Zero */

	hasNull = ((blockDense->orig_4_bytes.flags & DSB_HAS_NULLBITMAP) != 0);
	hasRleCompression = ((blockDense->orig_4_bytes.flags & DSB_HAS_RLE_COMPRESSION) != 0);
	hasDeltaCompression = ((blockDense->orig_4_bytes.flags & DSB_HAS_DELTA_COMPRESSION) != 0);

	/*
	 * Verify logical row count.
	 */
	if (blockDense->logical_row_count < 0)
	{
		ereport(ERROR,
				(errmsg("Logical row count is negative and is expected to be greater than 0. (%d)",
						blockDense->logical_row_count),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}
	if (blockDense->logical_row_count == 0)
	{
		ereport(ERROR,
				(errmsg("Logical row count is zero and is expected to be at greater than 0"),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}
	if (blockDense->logical_row_count != expectedRowCount)
	{
		ereport(ERROR,
				(errmsg("Logical row count does not match expected value (found %d, expected %d)",
						blockDense->logical_row_count,
						expectedRowCount),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	/*
	 * Verify physical datum count.
	 */
	if (blockDense->physical_datum_count < 0)
	{
		ereport(ERROR,
				(errmsg("Physical datum count is negative and is expected to be at least greater than or equal to 0. (%d)",
						blockDense->physical_datum_count),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	/*
	 * Verify physical data size.
	 */
	if (blockDense->physical_data_size < 0)
	{
		ereport(ERROR,
				(errmsg("Physical data size is negative and is expected to be at least greater than or equal to 0. (%d)",
						blockDense->physical_data_size),
				 errdetailCallback(errdetailArg),
				 errcontextCallback(errcontextArg)));
	}

	if (blockDense->physical_datum_count > 0)
	{
		if (blockDense->physical_data_size == 0)
		{
			ereport(ERROR,
					(errmsg("Physical data size is zero and is expected to be at least greater than 0 since physical datum count is %d",
							blockDense->physical_datum_count),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		if (blockDense->physical_data_size > bufferSize)
		{
			ereport(ERROR,
			  (errmsg("Physical data size %d is greater than buffer size %d",
					  blockDense->physical_data_size,
					  bufferSize),
			   errdetailCallback(errdetailArg),
			   errcontextCallback(errcontextArg)));
		}

		/*
		 * This check will make it safer to do multiplication of datum count and datum length.
		 */
		if (blockDense->physical_datum_count > blockDense->physical_data_size)
		{
			ereport(ERROR,
					(errmsg("More physical items %d than physical bytes %d",
							blockDense->physical_datum_count,
							blockDense->physical_data_size),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		if (typeInfo->datumlen >= 0)
		{
			int64		calculatedDataSize;

			/*
			 * Fixed-length items.
			 */
			calculatedDataSize = ((int64) blockDense->physical_datum_count) * typeInfo->datumlen;

			if (calculatedDataSize > blockDense->physical_data_size)
			{
				ereport(ERROR,
						(errmsg("Physical size doesn't match calculations for %d count of fixed-size %d items "
								"(found " INT64_FORMAT ", expected %d)",
								blockDense->physical_datum_count,
								typeInfo->datumlen,
								calculatedDataSize,
								blockDense->physical_data_size),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}
	}
	else
	{
		Assert(blockDense->physical_datum_count == 0);
		if (blockDense->physical_data_size != 0)
		{
			ereport(ERROR,
					(errmsg("Physical data size %d is expected to be at 0 since physical datum count is 0",
							blockDense->physical_data_size),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}
	}

	if (!hasRleCompression)
	{
		int32		total_datum_count;

		if (hasDeltaCompression)
		{
			headerSize += sizeof(DatumStreamBlock_Delta_Extension);

			if (bufferSize < headerSize)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream DELTA block header extension size. Found %d and expected the size to be at least %d",
								bufferSize,
								headerSize),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

			deltaExtension = (DatumStreamBlock_Delta_Extension *) p;
			p += sizeof(DatumStreamBlock_Delta_Extension);
			deltaOnCount = deltaExtension->deltas_count;
		}
		else
		{
			deltaOnCount = 0;
		}
		total_datum_count = blockDense->physical_datum_count + deltaOnCount;

		if (!hasNull)
		{
			alignedHeaderSize = MAXALIGN(headerSize);

			if (blockDense->logical_row_count != total_datum_count)
			{
				ereport(ERROR,
						(errmsg("Logical row count expected to match physical datum count when block does not have NULLs "
								"(logical row count %d, physical datum count %d + deltaOnCount %d)",
								blockDense->logical_row_count,
								blockDense->physical_datum_count,
								deltaOnCount),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}
		else
		{
			int32		nullBitMapSize;

			int32		actualNullOnCount;
			int32		expectedNullOnCount;

			nullBitMapSize = DatumStreamBitMap_Size(blockDense->logical_row_count);
			headerSize += nullBitMapSize;

			if (!hasDeltaCompression)
			{
				alignedHeaderSize = MAXALIGN(headerSize);

				if (bufferSize < alignedHeaderSize)
				{
					ereport(ERROR,
							(errmsg("Expected header size %d including NULL bit-map is larger than buffer size %d",
									alignedHeaderSize,
									bufferSize),
							 errdetailCallback(errdetailArg),
							 errcontextCallback(errcontextArg)));
				}
			}
			else
			{
				if (bufferSize < headerSize)
				{
					ereport(ERROR,
							(errmsg("Expected header size %d including NULL bit-map is larger than buffer size %d",
									headerSize,
									bufferSize),
							 errdetailCallback(errdetailArg),
							 errcontextCallback(errcontextArg)));
				}
			}

			actualNullOnCount = DatumStreamBitMap_CountOn(p, blockDense->logical_row_count);

			expectedNullOnCount = blockDense->logical_row_count - total_datum_count;

			if (actualNullOnCount != expectedNullOnCount)
			{
				ereport(ERROR,
						(errmsg("NULL bit-map ON count does not match.  Found %d, expected %d",
								actualNullOnCount,
								expectedNullOnCount),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

			p += nullBitMapSize;
		}

		/* CONSIDER: Verify zero padding */
	}
	else
	{
		DatumStreamBlock_Rle_Extension *rleExtension;

		int32		actualNullOnCount;

		int32		compressBitMapSize;

		int32		actualCompressOnCount;

		int32		totalRepeatCount;
		int32		totalRepeatCountsSize;

		int			i;

		int32		expectedNullOnCount;

		headerSize += sizeof(DatumStreamBlock_Rle_Extension);

		if (bufferSize < headerSize)
		{
			ereport(ERROR,
					(errmsg("Bad datum stream RLE_TYPE block header extension size.	Found %d and expected the size to be at least %d",
							bufferSize,
							headerSize),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		rleExtension = (DatumStreamBlock_Rle_Extension *) p;

		if (!hasNull)
		{
			if (rleExtension->norepeats_null_bitmap_count != 0)
			{
				ereport(ERROR,
						(errmsg("RLE_TYPE NULL bit-map count is expected to be 0 when there are no NULLs. (%d)",
								rleExtension->norepeats_null_bitmap_count),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}
		else
		{
			if (rleExtension->norepeats_null_bitmap_count <= 0)
			{
				ereport(ERROR,
						(errmsg("RLE_TYPE NULL bit-map count is negative or 0 and is expected to be greater than 0 when there are NULLs. (%d)",
								rleExtension->norepeats_null_bitmap_count),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}

		if (rleExtension->compress_bitmap_count <= 0)
		{
			ereport(ERROR,
					(errmsg("RLE_TYPE COMPRESS bit-map count is negative or 0 and is expected to be greater than 0. (%d)",
							rleExtension->compress_bitmap_count),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		if (rleExtension->repeatcounts_count <= 0)
		{
			ereport(ERROR,
					(errmsg("RLE_TYPE repeats count is negative or 0 and is expected to be greater than 0. (%d)",
							rleExtension->repeatcounts_count),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		if (rleExtension->repeatcounts_size <= 0)
		{
			ereport(ERROR,
					(errmsg("RLE_TYPE repeats size is negative or 0 and is expected to be greater than 0. (%d)",
							rleExtension->repeatcounts_size),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		p += sizeof(DatumStreamBlock_Rle_Extension);

		if (hasDeltaCompression)
		{
			headerSize += sizeof(DatumStreamBlock_Delta_Extension);

			if (bufferSize < headerSize)
			{
				ereport(ERROR,
						(errmsg("Bad datum stream RLE_TYPE DELTA block header extension size. Found %d and expected the size to be at least %d",
								bufferSize,
								headerSize),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

			deltaExtension = (DatumStreamBlock_Delta_Extension *) p;
			p += sizeof(DatumStreamBlock_Delta_Extension);
		}

		if (!hasNull)
		{
			actualNullOnCount = 0;
		}
		else
		{
			int32		nullBitMapSize;

			nullBitMapSize = DatumStreamBitMap_Size(rleExtension->norepeats_null_bitmap_count);
			headerSize += nullBitMapSize;

			if (bufferSize < headerSize)
			{
				ereport(ERROR,
						(errmsg("Bad NULL bit-map size %d with RLE_TYPE compression extension header is larger than buffer size %d",
								headerSize,
								bufferSize),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

			actualNullOnCount = DatumStreamBitMap_CountOn(p, rleExtension->norepeats_null_bitmap_count);

			p += nullBitMapSize;
		}

		compressBitMapSize = DatumStreamBitMap_Size(rleExtension->compress_bitmap_count);
		headerSize += compressBitMapSize;

		if (bufferSize < headerSize)
		{
			ereport(ERROR,
					(errmsg("Expected RLE_TYPE header size %d including NULL bit-map is larger than buffer size %d",
							headerSize,
							bufferSize),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		actualCompressOnCount = DatumStreamBitMap_CountOn(p, rleExtension->compress_bitmap_count);

		if (actualCompressOnCount != rleExtension->repeatcounts_count)
		{
			ereport(ERROR,
					(errmsg("RLE_TYPE COMPRESS bit-map ON count does not match COMPRESS bit-map ON count.  Found %d, expected %d",
							actualCompressOnCount,
							rleExtension->repeatcounts_count),
					 errdetailCallback(errdetailArg),
					 errcontextCallback(errcontextArg)));
		}

		p += compressBitMapSize;

		headerSize += rleExtension->repeatcounts_size;

		if (!hasDeltaCompression)
		{
			alignedHeaderSize = MAXALIGN(headerSize);

			if (bufferSize < alignedHeaderSize)
			{
				ereport(ERROR,
						(errmsg("Expected RLE_TYPE header size %d including repeat counts is larger than buffer size %d",
								alignedHeaderSize,
								bufferSize),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}
		else
		{
			if (bufferSize < headerSize)
			{
				ereport(ERROR,
						(errmsg("Expected RLE_TYPE header size %d including repeat counts is larger than buffer size %d",
								headerSize,
								bufferSize),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}

		}

		totalRepeatCount = 0;
		totalRepeatCountsSize = 0;
		for (i = 0; i < rleExtension->repeatcounts_count; i++)
		{
			int32		repeatCount;
			int			byteLen;

			repeatCount = DatumStreamInt32Compress_Decode(p, &byteLen);
			/* UNDONE: Range check repeatCount */

			totalRepeatCount += repeatCount;
			totalRepeatCountsSize += byteLen;
			p += byteLen;
		}

		if (totalRepeatCountsSize != rleExtension->repeatcounts_size)
		{
			ereport(ERROR,
			(errmsg("Bad RLE_TYPE repeats count size.  Found %d, expected %d",
					totalRepeatCountsSize,
					rleExtension->repeatcounts_size),
			 errdetailCallback(errdetailArg),
			 errcontextCallback(errcontextArg)));
		}

		expectedNullOnCount =
			blockDense->logical_row_count -
			rleExtension->compress_bitmap_count -
			totalRepeatCount;

		if (!hasNull)
		{
			Assert(actualNullOnCount == 0);
			if (expectedNullOnCount != 0)
			{
				ereport(ERROR,
						(errmsg("Logical row count, COMPRESS bit-map count, and total repeat count for RLE_TYPE compression do not add up properly. Found %d, expected %d. "
								"(logical row count %d, COMPRESS bit-map count %d, total repeat count %d)",
								actualNullOnCount,
								expectedNullOnCount,
								blockDense->logical_row_count,
								rleExtension->compress_bitmap_count,
								totalRepeatCount),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}
		else
		{
			if (actualNullOnCount != expectedNullOnCount)
			{
				ereport(ERROR,
						(errmsg("NULL bit-map ON count for RLE_TYPE compression does not match.	Found %d, expected %d. "
								"(logical row count %d, COMPRESS bit-map count %d, total repeat count %d)",
								actualNullOnCount,
								expectedNullOnCount,
								blockDense->logical_row_count,
								rleExtension->compress_bitmap_count,
								totalRepeatCount),
						 errdetailCallback(errdetailArg),
						 errcontextCallback(errcontextArg)));
			}
		}

		/* UNDONE: Verify zero padding */
	}

	if (hasDeltaCompression)
	{
		DatumStreamBlock_IntegrityCheckDenseDelta(
												  deltaExtension,
												  p,
												  bufferSize,
												  headerSize,
												  rleExtension,
												  errdetailCallback,
												  errdetailArg,
												  errcontextCallback,
												  errcontextArg);
	}

	if (typeInfo->datumlen == -1)
	{
		/*
		 * Variable-length items.
		 */

		DatumStreamBlock_IntegrityCheckVarlena(
											   buffer + alignedHeaderSize,
											   blockDense->physical_data_size,
											blockDense->orig_4_bytes.version,
											   typeInfo,
											   errdetailCallback,
											   errdetailArg,
											   errcontextCallback,
											   errcontextArg);
	}
}

char *
DatumStreamVersion_String(DatumStreamVersion datumStreamVersion)
{
	switch (datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			return "Original";
		case DatumStreamVersion_Dense:
			return "Dense";
		case DatumStreamVersion_Dense_Enhanced:
			return "Dense_Enhanced";
		default:
			return "Unknown";
	}
}

/*
 * varlena header info to string.
 */
static char *
VarlenaInfoToBuffer(char *buffer, uint8 * p)
{
	uint32		alignment = (uint32) ((uint8 *) INTALIGN(p) - p);

	if (VARATT_IS_EXTERNAL(p))
	{
		struct varatt_external *ext = (struct varatt_external *) p;
		bool		externalIsCompressed = (ext->va_extsize != ext->va_rawsize - VARHDRSZ);

		sprintf(buffer,
			 "external (header ptr %p, header alignment %u, header 0x%.8x): "
				"va_rawsize: %d, va_extsize %d, valueid %u, toastrelid %u (compressed %s)",
				p,
				alignment,
				*((uint32 *) p),
				ext->va_rawsize,
				ext->va_extsize,
				ext->va_valueid,
				ext->va_toastrelid,
				(externalIsCompressed ? "true" : "false"));
	}
	else if (VARATT_IS_SHORT(p))
	{
		sprintf(buffer,
				"short (header ptr %p, header alignment %u, header 0x%.2x): "
				"VARSIZE_SHORT %d, VARDATA_SHORT %p",
				p,
				alignment,
				(uint32) * ((uint8 *) p),
				(int32) VARSIZE_SHORT(p),
				VARDATA_SHORT(p));
	}
	else if (VARATT_IS_COMPRESSED(p))
	{
		varattrib_4b *comp = (varattrib_4b *) p;

		sprintf(buffer,
		   "compressed (header ptr %p, header alignment %u, header 0x%.8x): "
				"va_rawsize: %d, "
				"VARSIZE_ANY %d, VARDATA_ANY %p",
				p,
				alignment,
				*((uint32 *) p),
				(int32) comp->va_compressed.va_rawsize,
				(int32) VARSIZE_ANY(p),
				VARDATA_ANY(p));
	}
	else
	{
		sprintf(buffer,
			  "regular (header ptr %p, header alignment %u, header 0x%.8x): "
				"VARSIZE_ANY %d, VARDATA_ANY %p",
				p,
				alignment,
				*((uint32 *) p),
				(int32) VARSIZE_ANY(p),
				VARDATA_ANY(p));
	}

	return buffer;
}

static char varlenaInfoBuffer[100];

char *
VarlenaInfoToString(uint8 * p)
{
	return VarlenaInfoToBuffer(varlenaInfoBuffer, p);
}
