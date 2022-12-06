#include "postgres.h"

#include "access/aocssegfiles.h"
#include "access/tuptoaster.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_type.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h"
#include "commands/vacuum.h"
#include "storage/bufmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "miscadmin.h"
#include "funcapi.h"

/**
 * Statistics related parameters.
 */

bool			gp_statistics_pullup_from_child_partition = FALSE;
bool			gp_statistics_use_fkeys = FALSE;

typedef struct
{
	/* Table being sampled */
	Relation	onerel;

	/* Sampled rows and estimated total number of rows in the table. */
	HeapTuple  *sample_rows;
	int			num_sample_rows;
	double		totalrows;
	double		totaldeadrows;

	/*
	 * Result tuple descriptor. Each returned row consists of three "fixed"
	 * columns, plus all the columns of the sampled table (excluding dropped
	 * columns).
	 */
	TupleDesc	outDesc;
#define NUM_SAMPLE_FIXED_COLS 3

	/* SRF state, to track which rows have already been returned. */
	int			index;
	bool		summary_sent;
} gp_acquire_sample_rows_context;

/*
 * gp_acquire_sample_rows - Acquire a sample set of rows from table.
 *
 * This is a SQL callable wrapper around the internal acquire_sample_rows()
 * function in analyze.c. It allows collecting a sample across all segments,
 * from the dispatcher.
 *
 * acquire_sample_rows() actually has three return values: the set of sample
 * rows, and two double values: 'totalrows' and 'totaldeadrows'. It's a bit
 * difficult to return that from a SQL function, so bear with me. This function
 * is a set-returning function, and returns the sample rows, as you might
 * expect. But to return the extra 'totalrows' and 'totaldeadrows' values,
 * it always also returns one extra row, the "summary row". The summary row
 * is all NULLs for the actual table columns, but contains two other columns
 * instead, "totalrows" and "totaldeadrows". Those columns are NULL in all
 * the actual sample rows.
 *
 * To make things even more complicated, each sample row contains one extra
 * column too: oversized_cols_bitmap. It's a bitmap indicating which attributes
 * on the sample row were omitted, because they were "too large". The omitted
 * attributes are returned as NULLs, and the bitmap can be used to distinguish
 * real NULLs from values that were too large to be included in the sample. The
 * bitmap is represented as a text column, with '0' or '1' for every column.
 *
 * So overall, this returns a result set like this:
 *
 * postgres=# select * from pg_catalog.gp_acquire_sample_rows('foo'::regclass, 400, 'f') as (
 *     -- special columns
 *     totalrows pg_catalog.float8,
 *     totaldeadrows pg_catalog.float8,
 *     oversized_cols_bitmap pg_catalog.text,
 *     -- columns matching the table
 *     id int4,
 *     t text
 *  );
 *  totalrows | totaldeadrows | oversized_cols_bitmap | id  |    t    
 * -----------+---------------+-----------------------+-----+---------
 *            |               |                       |   1 | foo
 *            |               |                       |   2 | bar
 *            |               | 01                    |  50 | 
 *            |               |                       | 100 | foo 100
 *          2 |             0 |                       |     | 
 *          1 |             0 |                       |     | 
 *          1 |             0 |                       |     | 
 * (7 rows)
 *
 * The first four rows form the actual sample. One of the columns contained
 * an oversized text datum. The function is marked as EXECUTE ON SEGMENTS in
 * the catalog so you get one summary row *for each segment*.
 */
Datum
gp_acquire_sample_rows(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx = NULL;
	gp_acquire_sample_rows_context *ctx;
	MemoryContext oldcontext;
	Oid			relOid = PG_GETARG_OID(0);
	int32		targrows = PG_GETARG_INT32(1);
	bool		inherited = PG_GETARG_BOOL(2);
	HeapTuple  *sample_rows;
	TupleDesc	relDesc;
	TupleDesc	outDesc;
	int			live_natts;

	if (targrows < 1)
		elog(ERROR, "invalid targrows argument");

	if (SRF_IS_FIRSTCALL())
	{
		double		totalrows;
		double		totaldeadrows;
		Relation	onerel;
		int			attno;
		int			num_sample_rows;
		int			outattno;

		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function
		 * calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		if (!pg_class_ownercheck(relOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
						   get_rel_name(relOid));

		onerel = relation_open(relOid, AccessShareLock);
		relDesc = RelationGetDescr(onerel);

		/* Count the number of non-dropped cols */
		live_natts = 0;
		for (attno = 1; attno <= relDesc->natts; attno++)
		{
			Form_pg_attribute relatt = (Form_pg_attribute) relDesc->attrs[attno - 1];

			if (relatt->attisdropped)
				continue;
			live_natts++;
		}

		outDesc = CreateTemplateTupleDesc(NUM_SAMPLE_FIXED_COLS + live_natts, false);

		/* First, some special cols: */

		/* These two are only set in the last, summary row */
		TupleDescInitEntry(outDesc,
						   1,
						   "totalrows",
						   FLOAT8OID,
						   -1,
						   0);
		TupleDescInitEntry(outDesc,
						   2,
						   "totaldeadrows",
						   FLOAT8OID,
						   -1,
						   0);

		/* extra column to indicate oversize cols */
		TupleDescInitEntry(outDesc,
						   3,
						   "oversized_cols_bitmap",
						   TEXTOID,
						   -1,
						   0);

		outattno = NUM_SAMPLE_FIXED_COLS + 1;
		for (attno = 1; attno <= relDesc->natts; attno++)
		{
			Form_pg_attribute relatt = (Form_pg_attribute) relDesc->attrs[attno - 1];
			Oid			typid;

			if (relatt->attisdropped)
				continue;

			typid = gp_acquire_sample_rows_col_type(relatt->atttypid);

			TupleDescInitEntry(outDesc,
							   outattno++,
							   NameStr(relatt->attname),
							   typid,
							   relatt->atttypmod,
							   0);
		}

		BlessTupleDesc(outDesc);
		funcctx->tuple_desc = outDesc;

		/*
		 * Collect the actual sample. (We do this only after blessing the output
		 * tuple, to avoid the very expensive work of scanning the table, if we're
		 * going to error out because of incorrect column definition, anyway.
		 * ANALYZE should always get this right, but makes testing manually a bit
		 * more comfortable.)
		 */
		sample_rows = (HeapTuple *) palloc0(targrows * sizeof(HeapTuple));
		if (inherited)
		{
			num_sample_rows =
				acquire_inherited_sample_rows(onerel, DEBUG1,
											  sample_rows, targrows,
											  &totalrows, &totaldeadrows);
		}
		else
		{
			num_sample_rows =
				acquire_sample_rows(onerel, DEBUG1, sample_rows, targrows,
									&totalrows, &totaldeadrows);
		}

		/* Construct the context to keep across calls. */
		ctx = (gp_acquire_sample_rows_context *) palloc(sizeof(gp_acquire_sample_rows_context));
		ctx->onerel = onerel;
		funcctx->user_fctx = ctx;
		ctx->outDesc = outDesc;
		ctx->sample_rows = sample_rows;
		ctx->num_sample_rows = num_sample_rows;
		ctx->totalrows = totalrows;
		ctx->totaldeadrows = totaldeadrows;

		ctx->index = 0;
		ctx->summary_sent = false;

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	ctx = funcctx->user_fctx;
	relDesc = RelationGetDescr(ctx->onerel);
	outDesc = ctx->outDesc;

	Datum	   *outvalues = (Datum *) palloc(outDesc->natts * sizeof(Datum));
	bool	   *outnulls = (bool *) palloc(outDesc->natts * sizeof(bool));
	HeapTuple	res;

	/* First return all the sample rows */
	if (ctx->index < ctx->num_sample_rows)
	{
		HeapTuple	relTuple = ctx->sample_rows[ctx->index];
		int			attno;
		int			outattno;
		Bitmapset  *toolarge = NULL;
		Datum	   *relvalues = (Datum *) palloc(relDesc->natts * sizeof(Datum));
		bool	   *relnulls = (bool *) palloc(relDesc->natts * sizeof(bool));

		heap_deform_tuple(relTuple, relDesc, relvalues, relnulls);

		outattno = NUM_SAMPLE_FIXED_COLS + 1;
		for (attno = 1; attno <= relDesc->natts; attno++)
		{
			Form_pg_attribute relatt = (Form_pg_attribute) relDesc->attrs[attno - 1];
			bool		is_toolarge = false;
			Datum		relvalue;
			bool		relnull;

			if (relatt->attisdropped)
				continue;
			relvalue = relvalues[attno - 1];
			relnull = relnulls[attno - 1];

			/* Is this attribute "too large" to return? */
			if (relDesc->attrs[attno - 1]->attlen == -1 && !relnull)
			{
				Size		toasted_size = toast_datum_size(relvalue);

				if (toasted_size > WIDTH_THRESHOLD)
				{
					toolarge = bms_add_member(toolarge, outattno - NUM_SAMPLE_FIXED_COLS);
					is_toolarge = true;
					relvalue = (Datum) 0;
					relnull = true;
				}
			}
			outvalues[outattno - 1] = relvalue;
			outnulls[outattno - 1] = relnull;
			outattno++;
		}

		/*
		 * If any of the attributes were oversized, construct the text datum
		 * to represent the bitmap.
		 */
		if (toolarge)
		{
			char	   *toolarge_str;
			int			i;
			int			live_natts = outDesc->natts - NUM_SAMPLE_FIXED_COLS;

			toolarge_str = palloc((live_natts + 1) * sizeof(char));
			for (i = 0; i < live_natts; i++)
				toolarge_str[i] = bms_is_member(i + 1, toolarge) ? '1' : '0';
			toolarge_str[i] = '\0';

			outvalues[2] = CStringGetTextDatum(toolarge_str);
			outnulls[2] = false;
		}
		else
		{
			outvalues[2] = (Datum) 0;
			outnulls[2] = true;
		}
		outvalues[0] = (Datum) 0;
		outnulls[0] = true;
		outvalues[1] = (Datum) 0;
		outnulls[1] = true;

		res = heap_form_tuple(outDesc, outvalues, outnulls);

		ctx->index++;

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(res));
	}
	else if (!ctx->summary_sent)
	{
		/* Done returning the sample. Return the summary row, and we're done. */
		int			outattno;

		outvalues[0] = Float8GetDatum(ctx->totalrows);
		outnulls[0] = false;
		outvalues[1] = Float8GetDatum(ctx->totaldeadrows);
		outnulls[1] = false;

		outvalues[2] = (Datum) 0;
		outnulls[2] = true;
		for (outattno = NUM_SAMPLE_FIXED_COLS + 1; outattno <= outDesc->natts; outattno++)
		{
			outvalues[outattno - 1] = (Datum) 0;
			outnulls[outattno - 1] = true;
		}

		res = heap_form_tuple(outDesc, outvalues, outnulls);

		ctx->summary_sent = true;

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(res));
	}

	relation_close(ctx->onerel, AccessShareLock);

	pfree(ctx);
	funcctx->user_fctx = NULL;

	SRF_RETURN_DONE(funcctx);
}

/*
 * Companion to gp_acquire_sample_rows().
 *
 * gp_acquire_sample_rows() returns a different datatype for some
 * columns in the table. This does the mapping. It's in a function, so
 * that it can be used both by gp_acquire_sample_rows() itself, as well
 * as its callers.
 */
Oid
gp_acquire_sample_rows_col_type(Oid typid)
{
	switch (typid)
	{
		case REGPROCOID:
			/*
			 * repproc isn't round-trippable, if there are overloaded
			 * functions. Treat it as plain oid.
			 */
			return OIDOID;

		case PGNODETREEOID:
			/*
			 * Input function of pg_node_tree doesn't allow loading
			 * back values. Treat it as text.
			 */
			return TEXTOID;
	}
	return typid;
}
