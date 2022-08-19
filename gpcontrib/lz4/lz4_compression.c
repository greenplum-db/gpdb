/*---------------------------------------------------------------------
 *
 * lz4_compression.c
 *
 * IDENTIFICATION
 *	    gpcontrib/lz4/lz4_compression.c
 *
 *---------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "catalog/pg_compression.h"
#include "fmgr.h"
#include "storage/gp_compress.h"
#include "utils/builtins.h"

#include <lz4.h>
#include <lz4hc.h>

Datum		lz4_constructor(PG_FUNCTION_ARGS);
Datum		lz4_destructor(PG_FUNCTION_ARGS);
Datum		lz4_compress(PG_FUNCTION_ARGS);
Datum		lz4_decompress(PG_FUNCTION_ARGS);
Datum		lz4_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(lz4_constructor);
PG_FUNCTION_INFO_V1(lz4_destructor);
PG_FUNCTION_INFO_V1(lz4_compress);
PG_FUNCTION_INFO_V1(lz4_decompress);
PG_FUNCTION_INFO_V1(lz4_validator);

#ifndef UNIT_TESTING
PG_MODULE_MAGIC;
#endif

/* Internal state for lz4 */
typedef struct lz4_state
{
	int			level;			/* Compression level */
	bool		compress;		/* Compress if true, decompress otherwise */
} lz4_state;

Datum
lz4_constructor(PG_FUNCTION_ARGS)
{
	/* PG_GETARG_POINTER(0) is TupleDesc that is currently unused. */
	StorageAttributes	   *sa = (StorageAttributes *) PG_GETARG_POINTER(1);
	CompressionState	   *cs = palloc0(sizeof(CompressionState));
	lz4_state			   *state = palloc0(sizeof(lz4_state));
	bool					compress = PG_GETARG_BOOL(2);

	if (!PointerIsValid(sa->comptype))
		elog(ERROR, "lz4_constructor called with no compression type");

	cs->opaque = (void *) state;
	cs->desired_sz = NULL;

	if (sa->complevel == 0)
		sa->complevel = 1;

	state->level = sa->complevel;
	state->compress = compress;

	PG_RETURN_POINTER(cs);
}

Datum
lz4_destructor(PG_FUNCTION_ARGS)
{
	CompressionState	   *cs = (CompressionState *) PG_GETARG_POINTER(0);

	if (cs != NULL && cs->opaque != NULL)
	{
		lz4_state *state = (lz4_state *) cs->opaque;
		pfree(state);
	}

	PG_RETURN_VOID();
}

/*
 * lz4 compression implementation
 *
 * Note that when compression fails due to algorithm inefficiency,
 * dst_used is set to src_sz, but the output buffer contents are left undefined,
 * upper layer callers should `memcpy` the uncompressed data to the output
 * buffer.
 */
Datum
lz4_compress(PG_FUNCTION_ARGS)
{
	const void		   *src = PG_GETARG_POINTER(0);
	int32				src_sz = PG_GETARG_INT32(1);
	void			   *dst = PG_GETARG_POINTER(2);
	int32				dst_sz = PG_GETARG_INT32(3);
	int32			   *dst_used = (int32 *) PG_GETARG_POINTER(4);
	CompressionState   *cs = (CompressionState *) PG_GETARG_POINTER(5);
	lz4_state		   *state = (lz4_state *) cs->opaque;

	int dst_length_used;

	/* lz4hc treats level 0/1/2 the same
	 * see https://github.com/lz4/lz4/blob/v1.9.4/lib/lz4hc.c#L817
	 * we steal level 1 from High Compression mode as the fast mode
	 */
	if (state->level == 1)
	{
		dst_length_used = LZ4_compress_fast(src, dst, src_sz, dst_sz, state->level);
	}
	else
	{
		dst_length_used = LZ4_compress_HC(src, dst, src_sz, dst_sz, state->level);
	}

	if (dst_length_used == 0)
		*dst_used = src_sz;
	else
		*dst_used = dst_length_used;

	PG_RETURN_VOID();
}

Datum
lz4_decompress(PG_FUNCTION_ARGS)
{
	const void		   *src = PG_GETARG_POINTER(0);
	int32				src_sz = PG_GETARG_INT32(1);
	void			   *dst = PG_GETARG_POINTER(2);
	int32				dst_sz = PG_GETARG_INT32(3);
	int32			   *dst_used = (int32 *) PG_GETARG_POINTER(4);

	int result;

	if (src_sz <= 0)
		elog(ERROR, "invalid source buffer size %d", src_sz);
	if (dst_sz <= 0)
		elog(ERROR, "invalid destination buffer size %d", dst_sz);

	result = LZ4_decompress_safe(src, dst, src_sz, dst_sz);
	if (result < 0)
		elog(ERROR, "decompress failed with error code: %d", result);

	*dst_used = result;

	PG_RETURN_VOID();
}

Datum
lz4_validator(PG_FUNCTION_ARGS)
{
	PG_RETURN_VOID();
}
