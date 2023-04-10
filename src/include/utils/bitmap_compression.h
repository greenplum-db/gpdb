/*------------------------------------------------------------------------------
 *
 * bitmap_compression
 *
 * A custom bitmap compression algorithm is based on the special assumption 
 * that a bitmap will either contain long ranges of 0-bits or 1-bits. 
 * Therefore the compression is a simple 32-bit block-based entropy encoder 
 * with predefined probabilities. 
 * 
 * Due to the predefined entropy distribution, this is not a general purpose
 * bitmap compression. Similarly, bitmap compression algorithms in the 
 * research literature are often focused on the compression of bitmaps
 * in bitmap indexes. These algorithms have build in assumptions not
 * given in the use case this compression algorithm was developed for.
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/bitmap_compression.h
 *
 *------------------------------------------------------------------------------
*/
#ifndef BITMAP_COMPRESSION_H
#define BITMAP_COMPRESSION_H

#include "utils/bitstream.h"
#include "nodes/bitmapset.h"

/*
 * The compression type, which determines which compression
 * algorithm should be used.
 *
 * All compression types share a common header with the following spec:
 * 1 bit: Compression type flag
 * 3 bit: Reserved for future use
 * 12 bit: Number of 32-bit blocks compressed
 */ 
typedef enum BitmapCompressionType
{
	/*
	 * With BITMAP_COMPRESSION_TYPE_NO, no compression
	 * is performed. After the common header is added, the bitmap data is 
	 * simply copied to the output.
	 */ 
	BITMAP_COMPRESSION_TYPE_NO,

	/*
	 * The default compressor is a combination of a special handling for
	 * all-0 and all-1 blocks together with run-length encoding. After the
	 * common header the format is a repetition of the following bit pattern:
	 * 00: Indicates an all zero block
	 * 01: Indicate an all one block
	 * 11: Indicate a raw block. The following 32 bit contain the value of
	 *     the block
	 * 10: Indicate a Run-length Encoding (RLE). The following 8-bit contain the
	 *     number of times contents of the last block is repeated. However,
	 *     the actual number of repetitions is one larger, d.h. a RLE repetition
	 *     counter of 0 indicates one repetition and therefore two blocks with
	 *     the same content.
	 */
	BITMAP_COMPRESSION_TYPE_DEFAULT
} BitmapCompressionType;

#define BITMAP_COMPRESSION_HEADER_SIZE (2)

/*
 * State structure for the bitmap decompression.
 */ 
typedef struct BitmapDecompressState
{
	/*
	 * the compression data bitstream.
	 */ 
	Bitstream bitstream;

	/*
	 * the compression type used
	 */ 
	BitmapCompressionType compressionType;

	/*
	 * the number of bitmap blocks to decompression
	 */ 
	int blockCount;
} BitmapDecompressState;

bool BitmapDecompress_Init(
	BitmapDecompressState *state,
	unsigned char *inData, int inDataSize);

int BitmapDecompress_GetBlockCount(
	BitmapDecompressState *state);

BitmapCompressionType BitmapDecompress_GetCompressionType(
	BitmapDecompressState *state);

int BitmapDecompress_HasError(
	BitmapDecompressState *state);

void BitmapDecompress_Decompress(
		BitmapDecompressState *state,
		uint32 *bitmapData,
		int bitmapDataSize);

int Bitmap_Compress(
		BitmapCompressionType compressionType,
		uint32 *bitmapData,
		int bitmapDataSize,
		unsigned char *outData,
		int maxOutDataSize);

void BitmapDecompress_CalculateBlockCounts(
	BitmapDecompressState *decompressState,
	int *onDiskBlockCount,
	int *bmsWordCount);

void BitmapCompress_CalculateBlockCounts(
	Bitmapset *bitmap,
	int *onDiskBlockCount,
	int *bmsWordCount);

#endif

