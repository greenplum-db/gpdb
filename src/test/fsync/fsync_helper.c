#include "postgres.h"
#include "funcapi.h"
#include "nodes/pg_list.h"
#include "storage/buf_internals.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum dirty_buffers(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(dirty_buffers);
Datum
dirty_buffers(PG_FUNCTION_ARGS)
{
	FuncCallContext    *funcctx;
	MemoryContext		oldcontext;
	List               *dirty_tags;
	BufferTag          *tag;
	AttrNumber          attno;
#define natts 4 /* (RelFileNode, segno) */

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		int i;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		tupdesc = CreateTemplateTupleDesc(natts);
		attno = 1;
		TupleDescInitEntry(tupdesc, attno++, "tablespace", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, attno++, "database", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, attno++, "relfilenode", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, attno++, "block", INT4OID, -1, 0);
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		dirty_tags = NIL;
		for (i = 0; i < NBuffers; i++)
		{
			BufferDesc *bufHdr = GetBufferDescriptor(i);

			if ((pg_atomic_read_u32(&bufHdr->state) & (BM_DIRTY | BM_JUST_DIRTIED)) != 0)
			{
				tag = (BufferTag *) palloc(sizeof(BufferTag));
				/*
				 * XXX: this reads the tag without a lock. That's not cool, but it
				 * works enough for our test.
				 */
				*tag = bufHdr->tag;
				dirty_tags = lappend(dirty_tags, tag);
			}
		}

		funcctx->user_fctx = (void *) dirty_tags;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	dirty_tags = (List *) funcctx->user_fctx;
	if (list_length(dirty_tags) > 0)
	{
		Datum            result;
		Datum            values[natts];
		bool             nulls[natts];
		HeapTuple        tuple;

		tag = (BufferTag *) linitial(dirty_tags);
		attno = 0;
		values[attno++] = tag->rnode.spcNode;
		values[attno++] = tag->rnode.dbNode;
		values[attno++] = tag->rnode.relNode;
		values[attno++] = tag->blockNum;
		memset(nulls, 0, natts * sizeof(bool));
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		dirty_tags = list_delete(dirty_tags, (void *)tag);
		funcctx->user_fctx = (void *) dirty_tags;
		SRF_RETURN_NEXT(funcctx, result);
	}
	SRF_RETURN_DONE(funcctx);
}
