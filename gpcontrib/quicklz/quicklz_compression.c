/*
 * quicklz_compression.c
 *	Interfaces to quicklz compression functionality.
 *
 * Portions Copyright (c) 2011, EMC Corporation
 * Portions Copyright (c) 2013-Present, Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	gpcontrib/quicklz/quicklz_compression.c
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "catalog/pg_compression.h"

#include "quicklz.h"

PG_MODULE_MAGIC;

Datum quicklz_constructor(PG_FUNCTION_ARGS);
Datum quicklz_destructor(PG_FUNCTION_ARGS);
Datum quicklz_compress(PG_FUNCTION_ARGS);
Datum quicklz_decompress(PG_FUNCTION_ARGS);
Datum quicklz_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(quicklz_constructor);
PG_FUNCTION_INFO_V1(quicklz_destructor);
PG_FUNCTION_INFO_V1(quicklz_compress);
PG_FUNCTION_INFO_V1(quicklz_decompress);
PG_FUNCTION_INFO_V1(quicklz_validator);

/* Internal state for quicklz */
typedef struct quicklz_state
{
	void *scratch;

	int level;
	bool compress;

	/*
	 * The actual algorithms. Allows us to handle quicklz1 and quicklz3
	 * conveniently.
	 */
	size_t (*compress_fn)(int level, const void *, char *, size_t, void *);
	size_t (*decompress_fn)(int level, const char *, void *, void *);
} quicklz_state;

/*
 * quicklz helper function.
 */
static size_t
quicklz_desired_sz(size_t input)
{
	/*
	 * From the QuickLZ manual:
	 *
	 *   "The destination buffer must be at least size + 400 bytes large because
	 *   incompressible data may increase in size."
	 *
	 */
	return input + 400;
}

/*
 * Wrap up the qlz functions since C's type checking is getting in the
 * way.
 */
static size_t
quicklz_compressor(int level, const void *source, char *destination,
				   size_t size, void *state)
{
	if (level == 1)
	{
		return qlz_compress(source, destination, size,
							 (qlz_state_compress *)state);
	}
	Insist(false);
}

static size_t
quicklz_decompressor(int level, const char *source, void *destination,
					 void *state)
{
	if (level == 1)
	{
		return qlz_decompress(source, destination,
							   (qlz_state_decompress *)state);
	}
	Insist(false);
}

/* ---------------------------------------------------------------------
 * Quicklz constructor and destructor
 * ---------------------------------------------------------------------
 */
Datum
quicklz_constructor(PG_FUNCTION_ARGS)
{
#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet("malloc_failure",
					DDLNotSpecified,
					"", // databaseName
					""); // tableName
#endif

	/* PG_GETARG_POINTER(0) is TupleDesc that is currently unused.
	 * It is passed as NULL */

	StorageAttributes *sa	= (StorageAttributes *) PG_GETARG_POINTER(1);
	CompressionState *cs 	= palloc0(sizeof(CompressionState));
	quicklz_state *state	= palloc0(sizeof(quicklz_state));
	bool compress			= PG_GETARG_BOOL(2);
	size_t scratchlen		= 0;

	cs->opaque = (void *)state;

	Insist(PointerIsValid(sa->comptype));
	Insist(strcmp(sa->comptype, "quicklz") == 0);
	Insist(sa->complevel == 1);

	state->level = sa->complevel;
	state->compress = compress;
	if (sa->complevel == 1)
	{
		state->compress_fn = quicklz_compressor;
		state->decompress_fn = quicklz_decompressor;
		if (compress)
			scratchlen = sizeof(qlz_state_compress);
		else
			scratchlen = sizeof(qlz_state_decompress);
	}
	else
		Insist(false); /* shouldn't get here but code defensively */

	state->scratch = palloc0(scratchlen);

	cs->desired_sz = quicklz_desired_sz;

	PG_RETURN_POINTER(cs);
}

Datum
quicklz_destructor(PG_FUNCTION_ARGS)
{
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(0);

	if (cs != NULL)
	{
		quicklz_state *state = (quicklz_state *) cs->opaque;
		if (state != NULL)
		{
			if (state->scratch != NULL)
			{
				pfree(state->scratch);
			}
			pfree(cs->opaque);
		}
	}

	PG_RETURN_VOID();
}

/* ---------------------------------------------------------------------
 * SQL invokable compression and decompression routines for built in
 * compression algorithms. All routines have the same SQL signature:
 *
 * void fun(internal, int, internal, int, internal, internal)
 *
 * If we were to think of this as a C function it would be more like:
 *
 * void fun(void *src, size_t src_sz, void *dst, size_t dst_sz,
 *		  size_t *dst_used, void *opaque)
 *
 * The meaning of each argument is as follows:
 * src - A pointer to data to be compressed/decompressed
 * src_sz - The number of bytes to compress/decompress
 * dst - A pointer to pre-allocated memory. The data compressed or
 * 		 decompressed by the function are written here.
 * dst_sz - The amount of memory in bytes allocated at dst
 * dst_used - The number of bytes written. If dst_sz was too small to
 *			store the data, this is set to zero.
 * opaque - Internal to the compression function.
 */
Datum
quicklz_compress(PG_FUNCTION_ARGS)
{
	const void *src 		  = PG_GETARG_POINTER(0);
	int32 src_sz				= PG_GETARG_INT32(1);
	void *dst					  = PG_GETARG_POINTER(2);
	int32 dst_sz				= PG_GETARG_INT32(3);
	int32 *dst_used			  = (int32 *) PG_GETARG_POINTER(4);
	CompressionState *cs 	= (CompressionState *)PG_GETARG_POINTER(5);
	quicklz_state *state	= (quicklz_state *)cs->opaque;

	Insist(dst_sz >= quicklz_desired_sz(src_sz));

	*dst_used = state->compress_fn(state->level, src, dst, (size_t)src_sz,
								   state->scratch);

	PG_RETURN_VOID();
}

Datum
quicklz_decompress(PG_FUNCTION_ARGS)
{
	const char *src 	  	= PG_GETARG_POINTER(0);
	int32 src_sz				= PG_GETARG_INT32(1);
	void *dst					= PG_GETARG_POINTER(2);
	int32 dst_sz			 	= PG_GETARG_INT32(3);
	int32 *dst_used			  = (int32 *) PG_GETARG_POINTER(4);
	CompressionState *cs 	= (CompressionState *)PG_GETARG_POINTER(5);
	quicklz_state *state	= (quicklz_state *)cs->opaque;

	Insist(src_sz > 0 && dst_sz > 0);
	*dst_used = state->decompress_fn(state->level, src, dst, state->scratch);

	PG_RETURN_VOID();
}

Datum
quicklz_validator(PG_FUNCTION_ARGS)
{
	PG_RETURN_VOID();
}


