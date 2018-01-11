/*------------------------------------------------------------------------------
 *
 * bitmap_compression.c
 *	  Compression code tailored to compression of bitmap
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/bitmap_compression.c
 *
 *------------------------------------------------------------------------------
*/
#include "postgres.h"
#include "utils/bitmap_compression.h"
#include "utils/bitstream.h"
#include "utils/guc.h"

typedef enum BitmapCompressionFlag
{
	BITMAP_COMPRESSION_FLAG_ZERO = 0x00,
	BITMAP_COMPRESSION_FLAG_ONE = 0x01,
	BITMAP_COMPRESSION_FLAG_RLE = 0x02,
	BITMAP_COMPRESSION_FLAG_RAW = 0x03
} BitmapCompressionFlag;

/*
 * Initializes a new decompression run
 */ 
bool
BitmapDecompress_Init(
	BitmapDecompressState *state,
	unsigned char *inData, int inDataSize)
{
	uint32 tmp;

	Bitstream_Init(&state->bitstream, inData, inDataSize);

	if (!Bitstream_Get(&state->bitstream, 1, &tmp))
		return false;
	state->compressionType = tmp;

	if (!Bitstream_Skip(&state->bitstream, 3)) 
		return false;

	if (!Bitstream_Get(&state->bitstream, 12, &tmp))
		return false;
	state->blockCount = tmp;
	return true;
}

/*
 * returns the number of compression bitmap blocks
 */ 
int
BitmapDecompress_GetBlockCount(
	BitmapDecompressState *state)
{
	return state->blockCount;
}

/*
 * returns the used compression type
 */ 
BitmapCompressionType
BitmapDecompress_GetCompressionType(
	BitmapDecompressState *state)
{
	return state->compressionType;
}

/*
 * returns true iff the compression had an error
 */ 
int
BitmapDecompress_HasError(
	BitmapDecompressState *state)
{
	return Bitstream_HasError(&state->bitstream);
}

/*
 * Performs the bitmap decompression.
 *
 * bitmapDataSize in uint32-words.
 */
void
BitmapDecompress_Decompress(
		BitmapDecompressState *state,
		uint32* bitmap,
		int bitmapDataSize)
{
	uint32 lastBlockData, flag, rleRepeatCount;
	int i;
	bool failed = false;

	Assert(state);
	Assert(Bitstream_GetOffset(&state->bitstream) == 16U);

	if (state->blockCount < 0 || state->blockCount > bitmapDataSize)
	{
		elog(ERROR, "invalid block count for bitmap during decompression: "
				"block count %d, compression type %d", 
				state->blockCount, state->compressionType);
	}

	if (state->compressionType == BITMAP_COMPRESSION_TYPE_NO)
	{
		memcpy(bitmap, 
				Bitstream_GetAlignedData(&state->bitstream, 16), 
				state->blockCount * sizeof(uint32));
	}
	else if (state->compressionType == BITMAP_COMPRESSION_TYPE_DEFAULT)
	{
		lastBlockData = 0;
		rleRepeatCount = 0;
		for (i = 0; i < state->blockCount; i++)
		{
			if (rleRepeatCount == 0)
			{

				if (!Bitstream_Get(&state->bitstream, 2, &flag))
				{
					failed = true;
					break;
				}

				switch (flag)
				{
					case BITMAP_COMPRESSION_FLAG_ZERO:
						bitmap[i] = 0;
						break;
					case BITMAP_COMPRESSION_FLAG_ONE:
						bitmap[i] = 0xFFFFFFFF;
						break;
					case BITMAP_COMPRESSION_FLAG_RAW:
						if (!Bitstream_Get(&state->bitstream, 32, &bitmap[i]))
						{
							failed = true;
							break;
						}
						break;
					case BITMAP_COMPRESSION_FLAG_RLE:
						Assert(i != 0);
						if (!Bitstream_Get(&state->bitstream, 8, &rleRepeatCount))
						{
							failed = true;
							break;
						}
						bitmap[i] = lastBlockData;
						break;
					default:
						Insist(0);
				}
				lastBlockData = bitmap[i];
			}
			else
			{
				/* In an RLE block */
				bitmap[i] = lastBlockData;
				rleRepeatCount--;
			}
		}
		if (rleRepeatCount > 0)
		{
			elog(ERROR, "illegal RLE state after bitmap decompression: "
					"block count %d, compression type %d, rle repeat count %u",
					state->blockCount, state->compressionType, rleRepeatCount);
		}

		if (failed)
		{
			elog(ERROR, "bitstream read error seen during decompression: "
					"block count %d, compression type %d",
					state->blockCount, state->compressionType);
		}
	}
	else
	{
		elog(ERROR, "illegal compression type during bitmap decompression: "
				"compression type %d", state->compressionType);
	}
}

static bool 
Bitmap_EncodeRLE(Bitstream* bitstream,
		int rleRepeatCount,
		int lastBlockFlag)
{
	int i;

	if (lastBlockFlag == BITMAP_COMPRESSION_FLAG_RAW || rleRepeatCount > 4)
	{
		if (!Bitstream_Put(bitstream, BITMAP_COMPRESSION_FLAG_RLE, 2))
			return false;
		if (!Bitstream_Put(bitstream, rleRepeatCount - 1, 8))
			return false;
	} 
	else
	{
		Assert(lastBlockFlag != BITMAP_COMPRESSION_FLAG_RLE && lastBlockFlag != BITMAP_COMPRESSION_FLAG_RAW);
		/* If the number of repeated is not high enough,
		 * it is better to write the encoded one/zeros out
		 */
		for (i = 0; i < rleRepeatCount; i++)
		{
			if (!Bitstream_Put(bitstream, lastBlockFlag, 2))
				return false;
		}
	}
	return true;
}

static bool 
Bitmap_Compress_Default(
		uint32* bitmap,
		int blockCount,
		Bitstream *bitstream)
{
	uint32 lastBlockData;
	int lastBlockFlag;
	int rleRepeatCount;
	int i;

	lastBlockData = 0;
	lastBlockFlag = 0;
	rleRepeatCount = 0;
	for (i = 0; i < blockCount; i++)
	{
		uint32 blockData = bitmap[i];
		if (blockData == lastBlockData && rleRepeatCount <= 255 && i > 0)
		{
			rleRepeatCount++;
		}
		else
		{
			if (rleRepeatCount > 0)
			{
				if (!Bitmap_EncodeRLE(bitstream, rleRepeatCount,
						lastBlockFlag))
					return false;
				rleRepeatCount = 0;
			}

			if (blockData == 0)
			{
				if (!Bitstream_Put(bitstream, BITMAP_COMPRESSION_FLAG_ZERO, 2))
					return false;
				lastBlockFlag = BITMAP_COMPRESSION_FLAG_ZERO;
			}
			else if (blockData == 0xFFFFFFFFU)
			{
				if (!Bitstream_Put(bitstream, BITMAP_COMPRESSION_FLAG_ONE, 2))
					return false;
				lastBlockFlag = BITMAP_COMPRESSION_FLAG_ONE;
			}
			else
			{
				if (!Bitstream_Put(bitstream, BITMAP_COMPRESSION_FLAG_RAW, 2))
					return false;
				if (!Bitstream_Put(bitstream, blockData, 32))
					return false;
				lastBlockFlag = BITMAP_COMPRESSION_FLAG_RAW;
			}

			lastBlockData = blockData;
		}
	}

	/* Write last RLE block */
	if (rleRepeatCount > 0)
	{
		if (!Bitmap_EncodeRLE(bitstream, rleRepeatCount,
					lastBlockFlag))
			return false;
	}
	return true;

}

static bool
Bitmap_Compress_Write_Header(BitmapCompressionType compressionType,
		int blockCount, Bitstream *bitstream)
{
	if(!Bitstream_Put(bitstream, compressionType, 1))
		return false;
	if(!Bitstream_Skip(bitstream, 3))
		return false;
	if (!Bitstream_Put(bitstream, blockCount, 12))
		return false;
	Assert(Bitstream_GetOffset(bitstream) == 16U);
	return true;
}

/*
 * Compresses the given bitmap data.
 * 
 * bitmapDataSize in uint32-words.
 */ 
int
Bitmap_Compress(
		BitmapCompressionType compressionType,
		uint32* bitmap,
		int bitmapDataSize,
		unsigned char *outData,
		int maxOutDataSize)
{
	Bitstream bitstream;
	int blockCount;

	Assert(maxOutDataSize >= (bitmapDataSize * sizeof(uint32) + 2));

	memset(outData, 0, maxOutDataSize);
	blockCount = bitmapDataSize;

	/* Header 
	 */
	Bitstream_Init(&bitstream, outData, maxOutDataSize);
	if (!Bitmap_Compress_Write_Header(compressionType,blockCount, &bitstream))
		elog(ERROR, "Failed to write bitmap compression header");

	/* bitmap content */
	switch (compressionType)
	{
		case BITMAP_COMPRESSION_TYPE_NO:
			// By assertion I know that I have sufficient space for this
			if (bitmapDataSize == 0)
			{
				/* we only have the header */
				return 2;
			}
			memcpy(Bitstream_GetAlignedData(&bitstream, 16), 
					bitmap, bitmapDataSize * sizeof(uint32));
			
			return (bitmapDataSize * sizeof(uint32)) + 2;
		case BITMAP_COMPRESSION_TYPE_DEFAULT:
			if (!Bitmap_Compress_Default(bitmap, blockCount,
						&bitstream))
			{
				/* This may happen when the input bitmap is not nicely compressible */
				/* Fall back */

				memset(outData, 0, maxOutDataSize);
				return Bitmap_Compress(
						BITMAP_COMPRESSION_TYPE_NO,
						bitmap,
						bitmapDataSize,
						outData,
						maxOutDataSize);
			}
			else
			{
				return Bitstream_GetLength(&bitstream);
			}
		default:
			elog(ERROR, "illegal compression type during bitmap compression: "
				"compression type %d", compressionType);
			return 0;
	}
}
