/*---------------------------------------------------------------------
 *
 * zstd_compression.c
 *
 * IDENTIFICATION
 *	    src/backend/catalog/zstd_compression.c
 *
 *---------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "catalog/pg_compression.h"
#include "fmgr.h"
#include "storage/gp_compress.h"
#include "utils/builtins.h"

#include <zstd.h>
#include <zstd_errors.h>

Datum		zstd_constructor(PG_FUNCTION_ARGS);
Datum		zstd_destructor(PG_FUNCTION_ARGS);
Datum		zstd_compress(PG_FUNCTION_ARGS);
Datum		zstd_decompress(PG_FUNCTION_ARGS);
Datum		zstd_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(zstd_constructor);
PG_FUNCTION_INFO_V1(zstd_destructor);
PG_FUNCTION_INFO_V1(zstd_compress);
PG_FUNCTION_INFO_V1(zstd_decompress);
PG_FUNCTION_INFO_V1(zstd_validator);

#ifndef UNIT_TESTING
PG_MODULE_MAGIC;
#endif

/* Internal state for zstd */
typedef struct zstd_state
{
	int			level;			/* Compression level */
	bool		compress;		/* Compress if true, decompress otherwise */

	zstd_context *ctx;			/* ZSTD compression/decompresion contexts */
} zstd_state;

Datum
zstd_constructor(PG_FUNCTION_ARGS)
{
	/* PG_GETARG_POINTER(0) is TupleDesc that is currently unused. */

	StorageAttributes *sa = (StorageAttributes *) PG_GETARG_POINTER(1);
	CompressionState *cs = palloc0(sizeof(CompressionState));
	zstd_state *state = palloc0(sizeof(zstd_state));
	bool		compress = PG_GETARG_BOOL(2);

	if (!PointerIsValid(sa->comptype))
		elog(ERROR, "zstd_constructor called with no compression type");

	cs->opaque = (void *) state;
	cs->desired_sz = NULL;

	if (sa->complevel == 0)
		sa->complevel = 1;

	state->level = sa->complevel;
	state->compress = compress;

	state->ctx = zstd_alloc_context();
	state->ctx->cctx = ZSTD_createCCtx();
	state->ctx->dctx = ZSTD_createDCtx();

	PG_RETURN_POINTER(cs);
}

Datum
zstd_destructor(PG_FUNCTION_ARGS)
{
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(0);

	if (cs != NULL && cs->opaque != NULL)
	{
		zstd_state *state = (zstd_state *) cs->opaque;

		zstd_free_context(state->ctx);
		pfree(state);
	}

	PG_RETURN_VOID();
}

/*
 * zstd compression implementation
 *
 * Note that when compression fails due to algorithm inefficiency,
 * dst_used is set so src_sz, but the output buffer contents are left unchanged
 */
Datum
zstd_compress(PG_FUNCTION_ARGS)
{
	/* FIXME: Change types to ZSTD::size_t */
	const void *src = PG_GETARG_POINTER(0);
	int32		src_sz = PG_GETARG_INT32(1);
	void	   *dst = PG_GETARG_POINTER(2);
	int32		dst_sz = PG_GETARG_INT32(3);
	int32	   *dst_used = (int32 *) PG_GETARG_POINTER(4);
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(5);
	zstd_state *state = (zstd_state *) cs->opaque;

	unsigned long dst_length_used;

	dst_length_used = ZSTD_compressCCtx(state->ctx->cctx,
										dst, dst_sz,
										src, src_sz,
										state->level);

	if (ZSTD_isError(dst_length_used))
	{
		if (ZSTD_getErrorCode(dst_length_used) == ZSTD_error_dstSize_tooSmall)
		{
			/*
			 * This error is returned when "compressed" output is bigger than
			 * uncompressed input. The caller can detect this by checking
			 * dst_used >= src_size
			 */
			dst_length_used = src_sz;
		}
		else
			elog(ERROR, "%s", ZSTD_getErrorName(dst_length_used));
	}

	*dst_used = (int32) dst_length_used;

	PG_RETURN_VOID();
}

Datum
zstd_decompress(PG_FUNCTION_ARGS)
{
	/* FIXME: Change types to ZSTD::size_t */
	const void *src = PG_GETARG_POINTER(0);
	int32		src_sz = PG_GETARG_INT32(1);
	void	   *dst = PG_GETARG_POINTER(2);
	int32		dst_sz = PG_GETARG_INT32(3);
	int32	   *dst_used = (int32 *) PG_GETARG_POINTER(4);
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(5);
	zstd_state *state = (zstd_state *) cs->opaque;

	unsigned long dst_length_used;

	if (src_sz <= 0)
		elog(ERROR, "invalid source buffer size %d", src_sz);
	if (dst_sz <= 0)
		elog(ERROR, "invalid destination buffer size %d", dst_sz);

	dst_length_used = ZSTD_decompressDCtx(state->ctx->dctx,
										  dst, dst_sz,
										  src, src_sz);

	if (ZSTD_isError(dst_length_used))
	{
		elog(ERROR, "%s", ZSTD_getErrorName(dst_length_used));
	}

	*dst_used = (int32) dst_length_used;

	PG_RETURN_VOID();
}

Datum
zstd_validator(PG_FUNCTION_ARGS)
{
	PG_RETURN_VOID();
}
