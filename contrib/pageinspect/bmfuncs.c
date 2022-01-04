/*
 * contrib/pageinspect/bmfuncs.c
 *
 * Portions Copyright (c) 2021-Present VMware, Inc. or its affiliates.
 */

#include "postgres.h"

#include "pageinspect.h"

#include "access/bitmap_private.h"
#include "access/relation.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/varlena.h"

PG_FUNCTION_INFO_V1(bm_metap);
PG_FUNCTION_INFO_V1(bm_lov_page_items);
PG_FUNCTION_INFO_V1(bm_bitmap_page_header);
PG_FUNCTION_INFO_V1(bm_bitmap_page_items);
PG_FUNCTION_INFO_V1(bm_bitmap_page_items_bytea);

#define IS_INDEX(r) ((r)->rd_rel->relkind == RELKIND_INDEX)
#define IS_BITMAP_INDEX(r) ((r)->rd_rel->relam == BITMAP_AM_OID)

/* note: BlockNumber is unsigned, hence can't be negative */
#define CHECK_RELATION_BLOCK_RANGE(rel, blkno) { \
		if ( RelationGetNumberOfBlocks(rel) <= (BlockNumber) (blkno) ) \
			 elog(ERROR, "block number out of range"); }

/* ------------------------------------------------
 * bm_metap()
 *
 * Get a bitmap index's meta-page information
 *
 * Usage: SELECT * FROM bm_metap('gender')
 * ------------------------------------------------
 */
Datum
bm_metap(PG_FUNCTION_ARGS)
{
	text	   *relname = PG_GETARG_TEXT_PP(0);
	Datum		result;
	Relation	rel;
	RangeVar   *relrv;
	BMMetaPageData *metad;
	TupleDesc	tupleDesc;
	int			j;
	char	   *values[5];
	Buffer		buffer;
	HeapTuple	tuple;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to use pageinspect functions"))));

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
	rel = relation_openrv(relrv, AccessShareLock);

	if (!IS_INDEX(rel) || !IS_BITMAP_INDEX(rel))
		elog(ERROR, "relation \"%s\" is not a bitmap index",
			 RelationGetRelationName(rel));

	/*
	 * Reject attempts to read non-local temporary relations; we would be
	 * likely to get wrong data since we have no visibility into the owning
	 * session's local buffers.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot access temporary tables of other sessions")));

	buffer = ReadBuffer(rel, BM_METAPAGE);
	LockBuffer(buffer, BUFFER_LOCK_SHARE);

	metad = _bitmap_get_metapage_data(rel, buffer);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	j = 0;
	values[j++] = psprintf("%d", metad->bm_magic);
	values[j++] = psprintf("%d", metad->bm_version);
	values[j++] = psprintf("%d", metad->bm_lov_heapId);
	values[j++] = psprintf("%d", metad->bm_lov_indexId);
	values[j] = psprintf("%d", metad->bm_lov_lastpage);

	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupleDesc),
								   values);

	result = HeapTupleGetDatum(tuple);

	UnlockReleaseBuffer(buffer);
	relation_close(rel, AccessShareLock);

	PG_RETURN_DATUM(result);
}

/*-------------------------------------------------------
 * bm_get_word_text()
 *
 * Get a user-friendly print version of a bitmap word (uint64),
 * i.e. bytes are printed according to host endianness.
 * e.g. 31 becomes '00 00 00 00 00 00 00 1f'.
 * The length of the word is always 24 (including ending '\0').
 * ------------------------------------------------------
 */
static char* 
bm_get_word_text(BM_HRL_WORD *word)
{
	char 			*wtext;
	int 			wlen = 24;
	char 			*ptr;
	int 			off;

	ptr = (char*)word;
	wtext = palloc0(wlen);
	/* Be aware of endianness */
	if (htonl(42) == 42)
	{
		/* Big Endian */
		for (off = 0; off <= 7; off++)
		{
			if (off > 0)
				*wtext++ = ' ';
			sprintf(wtext, "%02x", *(ptr + off) & 0xff);
			wtext += 2;
		}
	}
	else
	{
		/* Little Endian */
		for (off = 7; off >= 0; off--)
		{
			if (off < 7)
				*wtext++ = ' ';
			sprintf(wtext, "%02x", *(ptr + off) & 0xff);
			wtext += 2;
		}
	}

	return wtext - (wlen - 1);
}

/*-------------------------------------------------------
 * bm_print_lov_item()
 *
 * Form a tuple describing an LOV item at a given offset
 * ------------------------------------------------------
 */
static Datum
bm_print_lov_item(FuncCallContext *fctx, Page page, OffsetNumber offset)
{
	char		*values[9];
	HeapTuple	tuple;
	ItemId		id;
	BMLOVItem	lovItem;
	int			j;

	id = PageGetItemId(page, offset);

	if (!ItemIdIsValid(id))
		elog(ERROR, "invalid ItemId");

	lovItem = (BMLOVItem) PageGetItem(page, id);

	j = 0;
	values[j++] = psprintf("%d", offset);
	values[j++] = psprintf("%u", lovItem->bm_lov_head);
	values[j++] = psprintf("%u", lovItem->bm_lov_tail);
	values[j++] = psprintf("%s", bm_get_word_text(&lovItem->bm_last_compword));
	values[j++] = psprintf("%s", bm_get_word_text(&lovItem->bm_last_word));
	values[j++] = psprintf("%lu", lovItem->bm_last_tid_location);
	values[j++] = psprintf("%lu", lovItem->bm_last_setbit);
	values[j++] = psprintf("%d", (lovItem->lov_words_header & 2) == 2);
	values[j]   = psprintf("%d", (lovItem->lov_words_header & 1) == 1);

	tuple = BuildTupleFromCStrings(fctx->attinmeta, values);

	return HeapTupleGetDatum(tuple);
}

/*
 * cross-call data structure for bm_lov_page_items() SRF
 */
struct user_args_lov_items
{
	Page		page;
	OffsetNumber offset;
};

/*-------------------------------------------------------
 * bm_lov_page_items()
 *
 * Get LOV items present in a bitmap LOV page
 *
 * Usage: SELECT * FROM bm_lov_page_items('t1_pkey', 1);
 *-------------------------------------------------------
 */
Datum
bm_lov_page_items(PG_FUNCTION_ARGS)
{
	text	   *relname = PG_GETARG_TEXT_PP(0);
	uint32		blkno = PG_GETARG_UINT32(1);
	Datum		result;
	FuncCallContext *fctx;
	MemoryContext              mctx;
	struct user_args_lov_items *uargs;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					(errmsg("must be superuser to use pageinspect functions"))));

	if (SRF_IS_FIRSTCALL())
	{
		RangeVar   *relrv;
		Relation	rel;
		Buffer		buffer;
		TupleDesc	tupleDesc;

		fctx = SRF_FIRSTCALL_INIT();

		relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
		rel = relation_openrv(relrv, AccessShareLock);

		if (!IS_INDEX(rel) || !IS_BITMAP_INDEX(rel))
			elog(ERROR, "relation \"%s\" is not a bitmap index",
				 RelationGetRelationName(rel));

		/*
		 * Reject attempts to read non-local temporary relations; we would be
		 * likely to get wrong data since we have no visibility into the
		 * owning session's local buffers.
		 */
		if (RELATION_IS_OTHER_TEMP(rel))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("cannot access temporary tables of other sessions")));

		if (blkno == 0)
			elog(ERROR, "block 0 is a meta page");

		CHECK_RELATION_BLOCK_RANGE(rel, blkno);

		buffer = ReadBuffer(rel, blkno);
		LockBuffer(buffer, BUFFER_LOCK_SHARE);

		/*
		 * We copy the page into local storage to avoid holding pin on the
		 * buffer longer than we must, and possibly failing to release it at
		 * all if the calling query doesn't fetch all rows.
		 */
		mctx = MemoryContextSwitchTo(fctx->multi_call_memory_ctx);

		uargs = palloc(sizeof(struct user_args_lov_items));

		uargs->page = palloc(BLCKSZ);
		memcpy(uargs->page, BufferGetPage(buffer), BLCKSZ);

		UnlockReleaseBuffer(buffer);
		relation_close(rel, AccessShareLock);

		/*
		 * Ensure that we are dealing with a LOV item page - they don't have a
		 * special section.
		 */
		if (PageGetSpecialSize(uargs->page))
			elog(ERROR, "block %d is not an LOV page, it is a bitmap page", blkno);

		uargs->offset = FirstOffsetNumber;

		fctx->max_calls = PageGetMaxOffsetNumber(uargs->page);

		/* Build a tuple descriptor for our result type */
		if (get_call_result_type(fcinfo, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");

		fctx->attinmeta = TupleDescGetAttInMetadata(tupleDesc);

		fctx->user_fctx = uargs;

		MemoryContextSwitchTo(mctx);
	}

	fctx = SRF_PERCALL_SETUP();
	uargs = fctx->user_fctx;

	if (fctx->call_cntr < fctx->max_calls)
	{
		result = bm_print_lov_item(fctx, uargs->page, uargs->offset);
		uargs->offset++;
		SRF_RETURN_NEXT(fctx, result);
	}
	else
	{
		pfree(uargs->page);
		pfree(uargs);
		SRF_RETURN_DONE(fctx);
	}
}

/*-------------------------------------------------------
 * bm_bitmap_page_header()
 *
 * Get the header information for a bitmap page. This
 * corresponds to the opaque section from the page
 * header.
 *
 * Usage: SELECT * FROM bm_bitmap_page_header('bm_index', 5);
 *-------------------------------------------------------
 */
Datum
bm_bitmap_page_header(PG_FUNCTION_ARGS)
{
	text	   *relname = PG_GETARG_TEXT_PP(0);
	uint32		blkno = PG_GETARG_UINT32(1);
	Datum		result;
	RangeVar   *relrv;
	Relation	rel;
	Buffer 		buffer;
	Page 		page;
	BMBitmapOpaque bm_opaque;
	TupleDesc	tupleDesc;
	int 		j;
	char	   *values[3];
	HeapTuple	tuple;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					(errmsg("must be superuser to use pageinspect functions"))));

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
	rel = relation_openrv(relrv, AccessShareLock);

	if (!IS_INDEX(rel) || !IS_BITMAP_INDEX(rel))
		elog(ERROR, "relation \"%s\" is not a bitmap index",
			 RelationGetRelationName(rel));

	/*
	 * Reject attempts to read non-local temporary relations; we would be
	 * likely to get wrong data since we have no visibility into the owning
	 * session's local buffers.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("cannot access temporary tables of other sessions")));

	CHECK_RELATION_BLOCK_RANGE(rel, blkno);

	if (blkno == 0)
		elog(ERROR, "block 0 is a meta page");

	buffer = ReadBuffer(rel, blkno);
	LockBuffer(buffer, BUFFER_LOCK_SHARE);

	page = BufferGetPage(buffer);

	/*
	 * Ensure that we are dealing with a bitmap page - they must have a special
	 * section.
	 */
	if (PageGetSpecialSize(page) <= 0)
		elog(ERROR, "block %d is not a bitmap page, it is a LOV item page", blkno);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	bm_opaque = (BMBitmapOpaque) PageGetSpecialPointer(page);

	j = 0;
	values[j++] = psprintf("%u", bm_opaque->bm_hrl_words_used);
	values[j++] = psprintf("%u", bm_opaque->bm_bitmap_next);
	values[j] = psprintf("%lu", bm_opaque->bm_last_tid_location);

	tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(tupleDesc),
								   values);

	result = HeapTupleGetDatum(tuple);

	UnlockReleaseBuffer(buffer);
	relation_close(rel, AccessShareLock);

	PG_RETURN_DATUM(result);
}

/*-------------------------------------------------------
 * bm_print_content_word()
 *
 * Print content word in given bitmap page along with its
 * compression status.
 * ------------------------------------------------------
 */
static Datum
bm_print_content_word(FuncCallContext *fctx, Page page, int word_num)
{
	char			*values[3];
	HeapTuple		tuple;
	int				j;
	BMBitmap 		bitmap = (BMBitmap) PageGetContentsMaxAligned(page);

	j = 0;
	values[j++] = psprintf("%d", word_num);
	values[j++] = psprintf("%d", IS_FILL_WORD(bitmap->hwords, word_num));
	values[j] = psprintf("%s", bm_get_word_text(&bitmap->cwords[word_num]));

	tuple = BuildTupleFromCStrings(fctx->attinmeta, values);

	return HeapTupleGetDatum(tuple);
}

/*
 * cross-call data structure for bm_bitmap_page_items() SRF
 */
struct user_args_page_items
{
	Page   page;
	uint32 word_num;
};

/*-------------------------------------------------------
 * bm_bitmap_page_items()
 *
 * Get the content words from a bitmap page along with
 * their compression statuses.
 *
 * Usage: SELECT * FROM bm_bitmap_page_items('t1_pkey', 5);
 *-------------------------------------------------------
 */
Datum
bm_bitmap_page_items(PG_FUNCTION_ARGS)
{
	text	   *relname = PG_GETARG_TEXT_PP(0);
	uint32		blkno = PG_GETARG_UINT32(1);
	Datum		result;
	FuncCallContext *fctx;
	MemoryContext              mctx;
	struct user_args_page_items *uargs;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					(errmsg("must be superuser to use pageinspect functions"))));

	if (SRF_IS_FIRSTCALL())
	{
		RangeVar   		*relrv;
		Relation		rel;
		Buffer			buffer;
		TupleDesc		tupleDesc;
		BMBitmapOpaque 	bm_opaque;

		fctx = SRF_FIRSTCALL_INIT();

		relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
		rel = relation_openrv(relrv, AccessShareLock);

		if (!IS_INDEX(rel) || !IS_BITMAP_INDEX(rel))
			elog(ERROR, "relation \"%s\" is not a bitmap index",
				 RelationGetRelationName(rel));

		/*
		 * Reject attempts to read non-local temporary relations; we would be
		 * likely to get wrong data since we have no visibility into the
		 * owning session's local buffers.
		 */
		if (RELATION_IS_OTHER_TEMP(rel))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("cannot access temporary tables of other sessions")));

		if (blkno == 0)
			elog(ERROR, "block 0 is a meta page");

		CHECK_RELATION_BLOCK_RANGE(rel, blkno);

		buffer = ReadBuffer(rel, blkno);
		LockBuffer(buffer, BUFFER_LOCK_SHARE);

		/*
		 * We copy the page into local storage to avoid holding pin on the
		 * buffer longer than we must, and possibly failing to release it at
		 * all if the calling query doesn't fetch all rows.
		 */
		mctx = MemoryContextSwitchTo(fctx->multi_call_memory_ctx);

		uargs = palloc(sizeof(struct user_args_page_items));

		uargs->page = palloc(BLCKSZ);
		memcpy(uargs->page, BufferGetPage(buffer), BLCKSZ);

		UnlockReleaseBuffer(buffer);
		relation_close(rel, AccessShareLock);

		/*
		 * Ensure that we are dealing with a bitmap page - they must have a special
		 * section.
		 */
		if (PageGetSpecialSize(uargs->page) <= 0)
			elog(ERROR, "block %d is not a bitmap page, it is a LOV item page", blkno);

		uargs->word_num = 0;
		bm_opaque = (BMBitmapOpaque) PageGetSpecialPointer(uargs->page);
		fctx->max_calls = bm_opaque->bm_hrl_words_used;

		/* Build a tuple descriptor for our result type */
		if (get_call_result_type(fcinfo, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");

		fctx->attinmeta = TupleDescGetAttInMetadata(tupleDesc);

		fctx->user_fctx = uargs;

		MemoryContextSwitchTo(mctx);
	}

	fctx = SRF_PERCALL_SETUP();
	uargs = fctx->user_fctx;

	if (fctx->call_cntr < fctx->max_calls)
	{
		result = bm_print_content_word(fctx, uargs->page, uargs->word_num);
		uargs->word_num++;
		SRF_RETURN_NEXT(fctx, result);
	}
	else
	{
		pfree(uargs->page);
		pfree(uargs);
		SRF_RETURN_DONE(fctx);
	}
}

/*-------------------------------------------------------
 * bm_bitmap_page_items_bytea()
 *
 * Get the content words from a bitmap page along with
 * their compression statuses.
 *
 * Usage: SELECT * FROM bm_bitmap_page_items(get_raw_page('t1_pkey', 5));
 *-------------------------------------------------------
 */
Datum
bm_bitmap_page_items_bytea(PG_FUNCTION_ARGS)
{
	bytea 		*raw_page = PG_GETARG_BYTEA_P(0);
	Datum		result;
	FuncCallContext *fctx;
	MemoryContext              mctx;
	struct user_args_page_items *uargs;
	int 		raw_page_size;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					(errmsg("must be superuser to use pageinspect functions"))));

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc		tupleDesc;
		BMBitmapOpaque 	bm_opaque;

		raw_page_size = VARSIZE(raw_page) - VARHDRSZ;

		if (raw_page_size < SizeOfPageHeaderData)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("input page too small (%d bytes)", raw_page_size)));

		fctx = SRF_FIRSTCALL_INIT();

		/*
		 * We copy the page into local storage to avoid holding pin on the
		 * buffer longer than we must, and possibly failing to release it at
		 * all if the calling query doesn't fetch all rows.
		 */
		mctx = MemoryContextSwitchTo(fctx->multi_call_memory_ctx);

		uargs = palloc(sizeof(struct user_args_page_items));

		uargs->page = VARDATA(raw_page);

		/*
		 * Ensure that we are dealing with a bitmap page - they must have a special
		 * section.
		 */
		if (PageGetSpecialSize(uargs->page) <= 0)
			elog(ERROR, "page is not a bitmap page");

		uargs->word_num = 0;
		bm_opaque = (BMBitmapOpaque) PageGetSpecialPointer(uargs->page);
		fctx->max_calls = bm_opaque->bm_hrl_words_used;

		/* Build a tuple descriptor for our result type */
		if (get_call_result_type(fcinfo, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");

		fctx->attinmeta = TupleDescGetAttInMetadata(tupleDesc);

		fctx->user_fctx = uargs;

		MemoryContextSwitchTo(mctx);
	}

	fctx = SRF_PERCALL_SETUP();
	uargs = fctx->user_fctx;

	if (fctx->call_cntr < fctx->max_calls)
	{
		result = bm_print_content_word(fctx, uargs->page, uargs->word_num);
		uargs->word_num++;
		SRF_RETURN_NEXT(fctx, result);
	}
	else
	{
		pfree(uargs);
		SRF_RETURN_DONE(fctx);
	}
}
