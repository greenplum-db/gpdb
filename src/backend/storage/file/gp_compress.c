/*-------------------------------------------------------------------------
 *
 * gp_compress.c
 *	  Common compression utilities
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/storage/file/gp_compress.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_compression.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "storage/gp_compress.h"
#include "utils/guc.h"

/*
 * Using the provided compression function this method will try to compress the data.
 * In case an issue occur during the compression it will abort the execution.
 */
void
gp_trycompress(uint8 *sourceData, int32 sourceLen, uint8 *compressedBuffer, int32 compressedBufferLen,
			   int32 *compressedLen, PGFunction compressor, CompressionState *compressionState
)

{
	Assert(PointerIsValid(compressor));
	
	callCompressionActuator( compressor
						   , (const void *)sourceData
						   , (size_t)sourceLen
						   , (char*)compressedBuffer
						   , compressedBufferLen
						   , compressedLen
						   , compressionState
						   );
}

/*---------------------------------------------------------------------------*/
static void gp_decompress_generic(
	  uint8	 *compressed,
	  int32	 compressedLen,
	  uint8	 *uncompressed,
	  int32	 uncompressedLen,
	  PGFunction	 decompressor,
	  CompressionState *compressionState,
	  int64	 bufferCount
	  )
{
	int32				 resultingUncompressedLen;

	callCompressionActuator(decompressor,
							(const void *)compressed,
							(size_t)compressedLen,
							(char*)uncompressed,
							uncompressedLen,
							&resultingUncompressedLen,
							compressionState);


	if (resultingUncompressedLen != uncompressedLen)
		elog(ERROR,
			 "Uncompress returned length %d which is different than the "
			 "expected length %d (block count " INT64_FORMAT ")",
			 resultingUncompressedLen,
			 uncompressedLen,
			 bufferCount);

}  /* end gp_decompress_generic */

/*---------------------------------------------------------------------------*/
void gp_decompress(
		uint8 *compressed,
		int32 compressedLen,
		uint8 *uncompressed,
		int32 uncompressedLen,
		PGFunction decompressor,
		CompressionState *compressionState,
		int64 bufferCount)
{
	unsigned long uncompressedSize;

	int32 resultingUncompressedLen;

	uncompressedSize = (unsigned long)uncompressedLen;

	gp_decompress_generic( compressed
						, compressedLen
						, uncompressed
						, uncompressedLen
						, decompressor
						, compressionState
						, bufferCount
	);

	resultingUncompressedLen = (int32)uncompressedSize;

	if (resultingUncompressedLen != uncompressedLen)
		elog(ERROR,
			 "Uncompress returned length %d which is different than the "
			 "expected length %d (block count " INT64_FORMAT ")",
			 resultingUncompressedLen,
			 uncompressedLen,
			 bufferCount);
}
