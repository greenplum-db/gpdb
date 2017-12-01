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
#include "utils/builtins.h"

#ifdef HAVE_LIBZSTD
/* Zstandard library is provided */

#include <zstd.h>

/* Internal state for zstd */
typedef struct zstd_state
{
	int			level;			/* Compression level */

	bool		compress;		/* Compress if true, decompress otherwise */

	ZSTD_CCtx  *zstd_compress_context;	/* ZSTD compression context */

	ZSTD_DCtx  *zstd_decompress_context;	/* ZSTD decompression context */

} zstd_state;

Datum
zstd_constructor(PG_FUNCTION_ARGS)
{
	/* PG_GETARG_POINTER(0) is TupleDesc that is currently unused. */

	StorageAttributes *sa = (StorageAttributes *) PG_GETARG_POINTER(1);
	CompressionState *cs = palloc0(sizeof(CompressionState));
	zstd_state *state = palloc0(sizeof(zstd_state));
	bool		compress = PG_GETARG_BOOL(2);

	cs->opaque = (void *) state;
	cs->desired_sz = NULL;

	Insist(PointerIsValid(sa->comptype));

	if (sa->complevel == 0)
		sa->complevel = 1;

	state->level = sa->complevel;
	state->compress = compress;
	state->zstd_compress_context = ZSTD_createCCtx();
	state->zstd_decompress_context = ZSTD_createDCtx();

	PG_RETURN_POINTER(cs);
}

Datum
zstd_destructor(PG_FUNCTION_ARGS)
{
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(0);
	zstd_state *state = (zstd_state *) cs->opaque;

	if (cs != NULL && cs->opaque != NULL)
	{
		ZSTD_freeCCtx(state->zstd_compress_context);
		ZSTD_freeDCtx(state->zstd_decompress_context);
		pfree(cs->opaque);
	}

	PG_RETURN_VOID();
}

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

	dst_length_used = ZSTD_compressCCtx(state->zstd_compress_context,
										dst, dst_sz,
										src, src_sz,
										state->level);

	if (ZSTD_isError(dst_length_used))
	{
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

	Insist(src_sz > 0 && dst_sz > 0);

	dst_length_used = ZSTD_decompressDCtx(state->zstd_decompress_context,
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


#else							/* HAVE_LIBZSTD */
/* Zstandard library is not provided; use dummy functions instead */

Datum
zstd_constructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "Zstandard library is not available in the current build");
	PG_RETURN_VOID();
}

Datum
zstd_destructor(PG_FUNCTION_ARGS)
{
	PG_RETURN_VOID();
}

Datum
zstd_compress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "Zstandard library is not available in the current build");
	PG_RETURN_VOID();
}

Datum
zstd_decompress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "Zstandard library is not available in the current build");
	PG_RETURN_VOID();
}

Datum
zstd_validator(PG_FUNCTION_ARGS)
{
	elog(ERROR, "Zstandard library is not available in the current build");
	PG_RETURN_VOID();
}

#endif							/* HAVE_LIBZSTD */
