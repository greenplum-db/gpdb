/*-------------------------------------------------------------------------
 *
 * gp_compress.h
 *      gpdb compression utilities.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/storage/gp_compress.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef GP_COMPRESS_H
#define GP_COMPRESS_H

#include "fmgr.h"

#include "catalog/pg_compression.h"

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif


extern int gp_trycompress_new(
		 uint8			*sourceData,
		 int32			 sourceLen,
		 uint8			*compressedBuffer,
		 int32			 compressedBufferWithOverrrunLen,
		 int32			 maxCompressedLen,	// The largest compressed result we would like to see.
		 int32			*compressedLen,
		 int				 compressLevel,
		 PGFunction	 compressor,
		 CompressionState *compressionState);


extern void gp_decompress_new(
				uint8			*compressed,
				int32			 compressedLen,
				uint8			*uncompressed,
				int32			 uncompressedLen,
			  PGFunction decompressor,
			  CompressionState *compressionState,
				int64			 bufferCount);

#endif
