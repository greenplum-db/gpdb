/*-------------------------------------------------------------------------
 *
 * gp_compress.h
 *      gpdb compression utilities.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/storage/gp_compress.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef GP_COMPRESS_H
#define GP_COMPRESS_H

#ifdef HAVE_LIBZSTD
#include "zstd.h"
#endif

#include "fmgr.h"

#include "catalog/pg_compression.h"
#include "lib/ilist.h"
#include "utils/resowner.h"


extern void gp_trycompress(
		 uint8			*sourceData,
		 int32			 sourceLen,
		 uint8			*compressedBuffer,
		 int32			 compressedBufferWithOverrrunLen,
		 int32			*compressedLen,
		 PGFunction	 compressor,
		 CompressionState *compressionState);


extern void gp_decompress(
		uint8 *compressed,
		int32 compressedLen,
		uint8 *uncompressed,
		int32 uncompressedLen,
		PGFunction decompressor,
		CompressionState *compressionState,
		int64 bufferCount);

/*
 * We use ZStandard compression in a few different places. These functions
 * provide support for tracking ZSTD compression/decompression contexts
 * with ResourceOwners, so that they are not leaked on abort.
 *
 * To use:
 *
 * zstd_context *ctx = call zstd_alloc_context();
 *
 * ctx->cctx = ZSTD_createCCtx();
 * if (!ctx->cctx)
 *     elog(ERROR, "out of memory");
 *
 * <use the context using normal ZSTD functions>
 *
 * zstd_free_context(ctx);
 *
 * If the transaction is aborted, the handle will be automatically closed,
 * when the resource owner is destroyed.
 */
#ifdef HAVE_LIBZSTD

typedef struct
{
	ZSTD_CCtx  *cctx;
	ZSTD_DCtx  *dctx;

	ResourceOwner owner;
	dlist_node	node;
} zstd_context;

extern void zstd_free_context(zstd_context *context);
extern zstd_context *zstd_alloc_context(void);

#endif	/* HAVE_LIBZSTD */


#endif
