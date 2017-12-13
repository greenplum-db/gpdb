/*---------------------------------------------------------------------
 *
 * pg_compression.c
 *	  Interfaces to low level compression functionality.
 *
 * Portions Copyright (c) 2011 EMC Corporation All Rights Reserved
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/pg_compression.c
 *
 *---------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "access/genam.h"
#include "access/reloptions.h"
#include "access/tupdesc.h"
#include "access/tupmacs.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_compression.h"
#include "catalog/dependency.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "storage/gp_compress.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/formatting.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/syscache.h"
#include "utils/faultinjector.h"

/* names we expect to see in ENCODING clauses */
char *storage_directive_names[] = {"compresstype", "compresslevel",
								   "blocksize", NULL};


/* Internal state for zlib */
typedef struct zlib_state
{
	int level;			/* compression level */
	bool compress;		/* compress or decompress? */

	/*
	 * The compression and decompression functions.
	 */
	int (*compress_fn) (Bytef *dest,
						uLongf *destLen,
						const Bytef *source,
						uLong sourceLen,
						int level);

	int (*decompress_fn) (Bytef *dest,
						  uLongf *destLen,
						  const Bytef *source,
						  uLong sourceLen);

} zlib_state;

static NameData
comptype_to_name(char *comptype)
{
	char		   *dct; /* down cased comptype */
	size_t			len;
	NameData		compname;


	if (strlen(comptype) >= NAMEDATALEN)
		elog(ERROR, "compression name \"%s\" exceeds maximum name length "
			 "of %d bytes", comptype, NAMEDATALEN - 1);

	len = strlen(comptype);
	dct = str_tolower(comptype, len);
	len = strlen(dct);

	memcpy(&(NameStr(compname)), dct, len);
	NameStr(compname)[len] = '\0';
	pfree(dct);

	return compname;
}

/*
 * Find the compression implementation (in pg_compression) for a particular
 * compression type.
 *
 * Comparison is case insensitive.
 *
 * NOTE: This function performs a catalog lookup, which can cause cache
 * invalidation. If 'comptype' points to the relcache, i.e.
 * RelationData->rd_appendonly->compresstype, that reference is no longer
 * valid after the call!
 */
PGFunction *
GetCompressionImplementation(char *comptype)
{
	HeapTuple		tuple;
	NameData		compname;
	PGFunction	   *funcs;
	Form_pg_compression ctup;
	FmgrInfo		finfo;
	Relation	comprel;
	ScanKeyData	scankey;
	SysScanDesc scan;

	/*
	 * Many callers pass RelationData->rd_appendonly->compresstype as
	 * the argument. That can become invalid, if heap_open below causes
	 * a relcache invalidation. Call comptype_to_name() on the argument
	 * first, to make a copy of it before we call heap_open().
	 *
	 * This is hazardous to the callers, too, if they try to use the
	 * string after the call for something else, but there isn't much
	 * we can do about it here.
	 */
	compname = comptype_to_name(comptype);

	comprel = heap_open(CompressionRelationId, AccessShareLock);

	comptype = NULL;	/* heap_open might have invalidated this */

	/* SELECT * FROM pg_compression WHERE compname = :1 */
	ScanKeyInit(&scankey,
				Anum_pg_compression_compname,
				BTEqualStrategyNumber, F_NAMEEQ,
				NameGetDatum(&compname));

	scan = systable_beginscan(comprel, CompressionCompnameIndexId, true,
							  SnapshotNow, 1, &scankey);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("unknown compress type \"%s\"",
						NameStr(compname))));

	funcs = palloc0(sizeof(PGFunction) * NUM_COMPRESS_FUNCS);

	ctup = (Form_pg_compression)GETSTRUCT(tuple);

	Insist(OidIsValid(ctup->compconstructor));
	fmgr_info(ctup->compconstructor, &finfo);
	funcs[COMPRESSION_CONSTRUCTOR] = finfo.fn_addr;

	Insist(OidIsValid(ctup->compdestructor));
	fmgr_info(ctup->compdestructor, &finfo);
	funcs[COMPRESSION_DESTRUCTOR] = finfo.fn_addr;

	Insist(OidIsValid(ctup->compcompressor));
	fmgr_info(ctup->compcompressor, &finfo);
	funcs[COMPRESSION_COMPRESS] = finfo.fn_addr;

	Insist(OidIsValid(ctup->compdecompressor));
	fmgr_info(ctup->compdecompressor, &finfo);
	funcs[COMPRESSION_DECOMPRESS] = finfo.fn_addr;

	Insist(OidIsValid(ctup->compvalidator));
	fmgr_info(ctup->compvalidator, &finfo);
	funcs[COMPRESSION_VALIDATOR] = finfo.fn_addr;

	systable_endscan(scan);
	heap_close(comprel, AccessShareLock);

	return funcs;
}

/* Invokes a compression constructor */
CompressionState *
callCompressionConstructor(PGFunction constructor,
						   TupleDesc tupledesc,
						   StorageAttributes *sa,
						   bool is_compress)
{
	return (CompressionState *) DatumGetPointer(DirectFunctionCall3(constructor,
											 PointerGetDatum(tupledesc),
											 PointerGetDatum(sa),
											 BoolGetDatum(is_compress)));

}

void
callCompressionDestructor(PGFunction destructor, CompressionState *state)
{
	DirectFunctionCall1(destructor, PointerGetDatum(state));
}

/* Actually call a compression (or decompression) function */
void
callCompressionActuator(PGFunction func , const void *src , int32 src_sz,
						char *dst, int32 dst_sz, int32 *dst_used,
						CompressionState *state)
{

  (void)DirectFunctionCall6(func, PointerGetDatum(src), Int32GetDatum(src_sz),
							PointerGetDatum(dst), Int32GetDatum(dst_sz),
							PointerGetDatum(dst_used), PointerGetDatum(state));


}

void
callCompressionValidator(PGFunction func, char *comptype, int32 complevel,
						 int32 blocksize, Oid typid)
{
	StorageAttributes sa;

	sa.comptype = comptype;
	sa.complevel = complevel;
	sa.blocksize = blocksize;
	sa.typid = typid;
	(void)DirectFunctionCall1(func, PointerGetDatum(&sa));
}

Datum
zlib_constructor(PG_FUNCTION_ARGS)
{

	/* PG_GETARG_POINTER(0) is TupleDesc that is currently unused.
	 * It is passed as NULL */

	StorageAttributes *sa = (StorageAttributes *) PG_GETARG_POINTER(1);
	CompressionState *cs	   = palloc0(sizeof(CompressionState));
	zlib_state	   *state	= palloc0(sizeof(zlib_state));
	bool			  compress = PG_GETARG_BOOL(2);

	cs->opaque = (void *) state;
	cs->desired_sz = NULL;

	Insist(PointerIsValid(sa->comptype));

	if (sa->complevel == 0)
		sa->complevel = 1;

	state->level = sa->complevel;
	state->compress = compress;
	state->compress_fn = compress2;
	state->decompress_fn = uncompress;

	PG_RETURN_POINTER(cs);

}

Datum
zlib_destructor(PG_FUNCTION_ARGS)
{
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(0);

	if (cs != NULL && cs->opaque != NULL)
 	{
		pfree(cs->opaque);
	}

	PG_RETURN_VOID();
}

Datum
zlib_compress(PG_FUNCTION_ARGS)
{
	const void	   *src	  = PG_GETARG_POINTER(0);
	int32			 src_sz   = PG_GETARG_INT32(1);
	void			 *dst	  = PG_GETARG_POINTER(2);
	int32			 dst_sz   = PG_GETARG_INT32(3);
	int32			*dst_used = (int32 *) PG_GETARG_POINTER(4);
	CompressionState *cs	   = (CompressionState *) PG_GETARG_POINTER(5);
	zlib_state	   *state	= (zlib_state *) cs->opaque;
	int				last_error;

	unsigned long amount_available_used = dst_sz;

	last_error = state->compress_fn((unsigned char *) dst,
									&amount_available_used, src, src_sz,
									state->level);

	*dst_used = amount_available_used;

	if (last_error != Z_OK)
	{
		switch (last_error)
		{
			case Z_MEM_ERROR:
				elog(ERROR, "out of memory");
				break;

			case Z_BUF_ERROR:
				/*
				 * zlib returns this when it couldn't compress the data
				 * to a size smaller than the input.
				 *
				 * The caller expects to detect this themselves so we set
				 * dst_used accordingly.
				 */
				*dst_used = src_sz;
				break;

			default:
				/* shouldn't get here */
				Insist(false);
				break;
		}
	}

	PG_RETURN_VOID();
}

Datum
zlib_decompress(PG_FUNCTION_ARGS)
{
	const char	   *src	= PG_GETARG_POINTER(0);
	int32			src_sz = PG_GETARG_INT32(1);
	void		   *dst	= PG_GETARG_POINTER(2);
	int32			dst_sz = PG_GETARG_INT32(3);
	int32		   *dst_used = (int32 *) PG_GETARG_POINTER(4);
	CompressionState *cs = (CompressionState *) PG_GETARG_POINTER(5);
	zlib_state	   *state = (zlib_state *) cs->opaque;
	int				last_error;
	unsigned long amount_available_used = dst_sz;

	Insist(src_sz > 0 && dst_sz > 0);


	last_error = state->decompress_fn(dst, &amount_available_used,
									  (const Bytef *) src, src_sz);

	*dst_used = amount_available_used;

	if (last_error != Z_OK)
	{
		switch (last_error)
		{
			case Z_MEM_ERROR:
				elog(ERROR, "out of memory");
				break;

			case Z_BUF_ERROR:

				/*
				 * This would be a bug. We should have given a buffer big
				 * enough in the decompress case.
				 */
				elog(ERROR, "buffer size %d insufficient for compressed data",
					 dst_sz);
				break;

			case Z_DATA_ERROR:
				/*
				 * zlib data structures corrupted.
				 *
				 * Check out the error message: kind of like 'catalog
				 * convergence' for data corruption :-).
				 */
				elog(ERROR, "zlib encountered data in an unexpected format");

			default:
				/* shouldn't get here */
				Insist(false);
				break;
		}
	}

	PG_RETURN_VOID();
}

Datum
zlib_validator(PG_FUNCTION_ARGS)
{
	PG_RETURN_VOID();
}

Datum
rle_type_constructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "rle_type block compression not supported");
	PG_RETURN_VOID();
}

Datum
rle_type_destructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "rle_type block compression not supported");
	PG_RETURN_VOID();
}

Datum
rle_type_compress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "rle_type block compression not supported");
	PG_RETURN_VOID();
}

Datum
rle_type_decompress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "rle_type block compression not supported");
	PG_RETURN_VOID();
}

Datum
rle_type_validator(PG_FUNCTION_ARGS)
{
	elog(ERROR, "rle_type block compression not supported");
	PG_RETURN_VOID();
}

/* Dummy routines to implement compresstype=none */
Datum
dummy_compression_constructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "dummy compression called directly");
	PG_RETURN_VOID();
}

Datum
dummy_compression_destructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "dummy compression called directly");
	PG_RETURN_VOID();
}

Datum
dummy_compression_compress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "dummy compression called directly");
	PG_RETURN_VOID();
}

Datum
dummy_compression_decompress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "dummy compression called directly");
	PG_RETURN_VOID();
}

Datum
dummy_compression_validator(PG_FUNCTION_ARGS)
{
	elog(ERROR, "dummy compression called directly");
	PG_RETURN_VOID();
}

/*
 * Does a compression algorithm exist by the name of `compresstype'?
 */
bool
compresstype_is_valid(char *comptype)
{
	bool found = false;
	int i;

	/*
	 * Hard-coding compresstypes is bad, agreed.  But there isn't a
	 * better way in sight at this point.  Lookup into pg_compression
	 * catalog table is not possible because this method is used to
	 * validate value of gp_default_storage_options GUC (among other
	 * things).  If the GUC is set in postgresql.conf, we get here
	 * before IsNormalProcessingMode() is true.
	 *
	 * Whenever the list of supported compresstypes is changed, this
	 * must change!
	 */
	static const char *const valid_comptypes[] =
			{"quicklz", "zlib", "rle_type", "none", "zstd"};
	for (i = 0; !found && i < ARRAY_SIZE(valid_comptypes); ++i)
	{
		if (pg_strcasecmp(valid_comptypes[i], comptype) == 0)
			found = true;
	}
	return found;
}

/*
 * Make encoding (compresstype = none, blocksize=...) based on
 * currently configured defaults.
 */
List *
default_column_encoding_clause(void)
{
	const StdRdOptions *ao_opts = currentAOStorageOptions();
	DefElem *e1, *e2, *e3;
	if (ao_opts->compresstype)
	{
		e1 = makeDefElem("compresstype",
						 (Node *)makeString(pstrdup(ao_opts->compresstype)));
	}
	else
	{
		e1 = makeDefElem("compresstype", (Node *)makeString("none"));
	}
	e2 = makeDefElem("blocksize",
					 (Node *)makeInteger(ao_opts->blocksize));
	e3 = makeDefElem("compresslevel",
					 (Node *)makeInteger(ao_opts->compresslevel));
	return list_make3(e1, e2, e3);
}

bool
is_storage_encoding_directive(char *name)
{
	int i = 0;

	while (storage_directive_names[i])
	{
		if (strcmp(name, storage_directive_names[i]) == 0)
			return true;
		i++;
	}
	return false;
}
