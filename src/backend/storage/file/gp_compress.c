/*-------------------------------------------------------------------------
 *
 * gp_compress.c
 *	  Common compression utilities
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "utils/resowner.h"

#ifdef HAVE_LIBZSTD
#include <zstd.h>
#endif

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

/*
 * Support for tracking ZSTD handles with resource owners.
 */
#ifdef HAVE_LIBZSTD

static dlist_head open_zstd_handles;
static bool zstd_resowner_callback_registered;

static void zstd_free_callback(ResourceReleasePhase phase,
				   bool isCommit,
				   bool isTopLevel,
				   void *arg);

zstd_context *
zstd_alloc_context(void)
{
	zstd_context *ctx;

	if (!zstd_resowner_callback_registered)
	{
		RegisterResourceReleaseCallback(zstd_free_callback, NULL);
		zstd_resowner_callback_registered = true;
	}

	ctx = MemoryContextAlloc(TopMemoryContext, sizeof(zstd_context));
	ctx->cctx = NULL;
	ctx->dctx = NULL;
	ctx->owner = CurrentResourceOwner;
	dlist_push_head(&open_zstd_handles, &ctx->node);

	return ctx;
}

void
zstd_free_context(zstd_context *context)
{
	if (context->cctx)
		ZSTD_freeCCtx(context->cctx);
	if (context->dctx)
		ZSTD_freeDCtx(context->dctx);

	dlist_delete(&context->node);

	pfree(context);
}

/* Close any open ZSTD handles on abort. */
static void
zstd_free_callback(ResourceReleasePhase phase,
				   bool isCommit,
				   bool isTopLevel,
				   void *arg)
{
	dlist_mutable_iter miter;

	if (phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	dlist_foreach_modify(miter, &open_zstd_handles)
	{
		zstd_context *context = dlist_container(zstd_context, node, miter.cur);

		if (context->owner == CurrentResourceOwner)
		{
			if (isCommit)
				elog(WARNING, "zstd context reference leak: context %p still referenced", context);
			zstd_free_context(context);
		}
	}
}

#endif	/* HAVE_LIBZSTD */
