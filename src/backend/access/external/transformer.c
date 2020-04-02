/*-------------------------------------------------------------------------
 *
 * transformer.h
 *	  External table data transformer routines.
 *
 * The main purpose of the transformer is to resolve abortion in ETL tasks.
 *
 * A transformer is used to transform a readable external table's unmatched
 * data source into the external table definition data. And handle any
 * possible errors during the transform phase through the single row error handler.
 *
 * The usage of the transformer is:
 * An external table EXTTBL(id int, value boolean).
 * A data source that has the same definition with a table
 * TEMPLATE(c1 int, c2 text, c3 json).
 *
 * The CREATE EXTERNAL statement should look like this:
 * CREATE EXTERNAL TABLE EXTTBL (id int, value boolean)
 * LOCATION (
 * 	'data source'
 * )
 * FORMAT 'any format'
 * OPTIONS (
 * 	transform_from 'TEMPLATE',
 * 	transform_expression '(*), (***)',
 * 	transform_arguments '*[, *...]')
 * LOG ERRORS SEGMENT REJECT LIMIT 5;
 *
 * (NOTE: the reason we use OPTIONS here is once we move the whole external
 * table catalog to FDW, the external table options will move to FDW foreign
 * table options to reduce extra work.)
 *
 * When executing an ETL job that retrieves data through the EXTTBL external
 * table, it actually doing the following steps:
 * 1. COPY from the data source by passing the TEMPLATE table definition.
 * Here, TEMPLATE is only used to tell copy that the data source's column
 * definition. So we required SELECT permission for TEMPLATE.
 * 2. Format the data from data source with the TEMPLATE table definition.
 * 3. Transform the data from TEMPLATE table definition to EXTTBL definition
 * through the "transform_expression" and "transform_arguments".
 *
 * "transform_from" is used to specify the data source definition through a table.
 * "transform_expression" is used to specify the SQL expression that transforms data.
 * "transform_arguments" is used to fill the expression arguments with the tuple
 * column data returned from step 1, 2. It the index of the TEMPLATE table
 * columns definition.
 * The reason why these options are set through OPTIONS filed is when migrating
 * external table to foreign data wrapper(FDW) interface, there should be not
 * much effort for the transformer.
 *
 * For example:
 * OPTIONS (
 * 	transform_from 'TEMPLATE',
 * 	transform_expression '$1, ($2->>1)::boolean',
 * 	transform_arguments '1, 3')
 *
 * It means use TEMPLATE's definition as data source columns definition.
 * Executes SPI plan for "SELECT c1, (c3->>1)::boolean" to transform tuples
 * into (id int, value boolean).
 *
 * Portions Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    src/backend/access/external/transformer.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/fileam.h"
#include "access/transformer.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "cdb/cdbsreh.h"
#include "commands/defrem.h"
#include "executor/spi.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

static int bms_nth_member(Bitmapset* a, int nth);
static TransformerInfo *get_transformer_info_from_options(List *options);
static SPIPlanPtr build_transform_plan(TransformerInfo *transformer);

/*
 * retireve nth member in bit map set.
 */
static int
bms_nth_member(Bitmapset* a, int nth)
{
	int idx = 0;
	int member = -1;

	while ((member = bms_next_member(a, member)) >= 0)
	{
		if (++idx == nth)
		{
			return member;
		}
	}

	return -2;
}

/*
 * get_transformer_info_from_options
 *
 * Build TransformerInfo from options.
 */
static TransformerInfo *
get_transformer_info_from_options(List *options)
{
	ListCell		   *cell;
	TransformerInfo	   *transformer = NULL;
	Oid					fromoid = InvalidOid;
	char			   *expression = NULL;
	List			   *args = NIL;
	bool				handleerror = false;

	foreach(cell, options)
	{
		DefElem    *def = (DefElem *) lfirst(cell);

		if (strcmp(def->defname, "transform_from") == 0)
		{
			fromoid =  parseTransformFromRel(defGetString(def));
		}
		else if (strcmp(def->defname, "transform_expression") == 0)
		{
			expression = defGetString(def);
		}
		else if (strcmp(def->defname, "transform_arguments") == 0)
		{
			args = parseTransformArguments(defGetString(def));
		}
		else if (strcmp(def->defname, "transform_handle_error") == 0)
		{
			handleerror = defGetBoolean(def);
		}
	}

	if (OidIsValid(fromoid))
	{
		transformer = palloc0(sizeof(TransformerInfo));
		/*  */
		transformer->fromoid = fromoid;
		transformer->fromrel = NULL;
		transformer->expression = expression;
		transformer->arguments = args;
		transformer->handleerror = handleerror;
		transformer->nexttupleidx = 0;
	}

	return transformer;
}

/*
 * initTransformerInfo - init TransformerInfo.
 */
TransformerInfo *
initTransformerInfo(List *options)
{
	TransformerInfo *transformer = get_transformer_info_from_options(options);
	if (transformer)
	{
		Form_pg_attribute	   *attr;
		AttrNumber				num_phys_attrs;
		int						attnum;
		Oid						in_func_oid;
		int						arglen = list_length(transformer->arguments);

		/* We only use the transfrom_from relation to transform tuples. */
		transformer->fromrel = heap_open(transformer->fromoid, AccessShareLock);
		RelationIncrementReferenceCount(transformer->fromrel);
		transformer->tupledesc = RelationGetDescr(transformer->fromrel);
		attr = transformer->tupledesc->attrs;
		num_phys_attrs = transformer->tupledesc->natts;

		transformer->values = (Datum *) palloc(num_phys_attrs * sizeof(Datum));
		transformer->nulls = (bool *) palloc(num_phys_attrs * sizeof(bool));

		/*
		* Pick up the required catalog information for each attribute in the
		* relation, including the input function and the element type (to pass to
		* the input function).
		*/
		transformer->in_functions = (FmgrInfo *) palloc0(num_phys_attrs * sizeof(FmgrInfo));
		transformer->typioparams = (Oid *) palloc0(num_phys_attrs * sizeof(Oid));

		for (attnum = 1; attnum <= num_phys_attrs; attnum++)
		{
			/* We don't need info for dropped attributes */
			if (attr[attnum - 1]->attisdropped)
				continue;

			/* Skip dropped types */
			transformer->attrsidx = bms_add_member(transformer->attrsidx, attnum);
			getTypeInputInfo(attr[attnum - 1]->atttypid,
							&in_func_oid, &transformer->typioparams[attnum - 1]);
			fmgr_info(in_func_oid, &transformer->in_functions[attnum - 1]);
		}

		transformer->argtypes = palloc0(sizeof(Oid) * arglen);
		transformer->paramvalues = palloc0(sizeof(Datum) * arglen);
		transformer->paramnulls = palloc0(sizeof(char) * arglen);
	}

	return transformer;
}

/*
 * freeTransformerInfo - free TransformerInfo.
 */
void
freeTransformerInfo(TransformerInfo *transformer)
{
	if (!transformer)
		return;
	SPI_finish();
	/*
	* decrement relation reference count and free transformer storage
	*/
	heap_close(transformer->fromrel, AccessShareLock);
	RelationDecrementReferenceCount(transformer->fromrel);

	if (transformer->values)
	{
		pfree(transformer->values);
		transformer->values = NULL;
	}
	if (transformer->nulls)
	{
		pfree(transformer->nulls);
		transformer->nulls = NULL;
	}
	if (transformer->in_functions)
	{
		pfree(transformer->in_functions);
		transformer->in_functions = NULL;
	}
	if (transformer->typioparams)
	{
		pfree(transformer->typioparams);
		transformer->typioparams = NULL;
	}
	if (transformer->attrsidx)
	{
		pfree(transformer->attrsidx);
		transformer->attrsidx = NULL;
	}
	if (transformer->argtypes)
	{
		pfree(transformer->argtypes);
		transformer->argtypes = NULL;
	}
	if (transformer->query)
	{
		pfree(transformer->query);
		transformer->query = NULL;
	}
	if (transformer->spiplan)
	{
		SPI_freeplan(transformer->spiplan);
		transformer->spiplan = NULL;
	}
	if (transformer->paramvalues)
	{
		pfree(transformer->paramvalues);
		transformer->paramvalues = NULL;
	}
	if (transformer->paramnulls)
	{
		pfree(transformer->paramnulls);
		transformer->paramnulls = NULL;
	}
	pfree(transformer);
}

/*
 * retrieveTransformedTuple - retrieve tuples from SPI tuptable.
 *
 * If the transform expression contains set return function.
 * It'll return multiple tuples for a single input tuple.
 * So return the tuple one by one untill reach the last one.
 */
HeapTuple
retrieveTransformedTuple(TransformerInfo *transformer)
{
	if (SPI_processed > 0 && transformer->nexttupleidx < SPI_processed && SPI_tuptable)
	{
		return SPI_copytuple(SPI_tuptable->vals[transformer->nexttupleidx++]);
	}
	SPI_freetuptable(SPI_tuptable);
	return NULL;
}

/*
 * build_transform_plan
 *
 * build the transform SPI plan.
 */
static SPIPlanPtr
build_transform_plan(TransformerInfo *transformer)
{
	ListCell			   *cell;
	SPIPlanPtr				plan = NULL;
	int						i = 0;
	Form_pg_attribute	   *attr;

	attr = transformer->tupledesc->attrs;
	/* Should consider sql escape */
	transformer->query = psprintf("SELECT %s", transformer->expression);

	foreach(cell, transformer->arguments)
	{
		int idx = lfirst_int(cell);
		int attnum = bms_nth_member(transformer->attrsidx, idx);
		if (attnum < 0)
			ereport(ERROR,
			(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
			errmsg("'transform_arguments' attribute index '%d' does not exist in '%s' definition.",
					idx, RelationGetRelationName(transformer->fromrel))));
		transformer->argtypes[i++] = attr[attnum - 1]->atttypid;
	}

	if (SPI_connect() < 0)
		ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			errmsg("SPI_connect failed")));

	plan = SPI_prepare(transformer->query, list_length(transformer->arguments),
					   transformer->argtypes);

	if (plan == NULL || (plan = SPI_saveplan(plan)) == NULL)
		ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			errmsg("SPI_prepare_failed")));

	return plan;
}


/*----------
 * Support for running SPI operations inside subtransactions
 *
 * Intended usage pattern is:
 *
 *	MemoryContext oldcontext = CurrentMemoryContext;
 *	ResourceOwner oldowner = CurrentResourceOwner;
 *
 *	...
 *	transformer_subtrans_begin(oldcontext);
 *	PG_TRY();
 *	{
 *		do something risky;
 *		transformer_subtrans_commit(oldcontext, oldowner);
 *	}
 *	PG_CATCH();
 *	{
 *		transformer_subtrans_abort(interp, oldcontext, oldowner);
 *	}
 *	PG_END_TRY();
 *----------
 */
static void
transformer_subtrans_begin(MemoryContext oldcontext)
{
	BeginInternalSubTransaction(NULL);

	/* Want to run inside function's memory context */
	MemoryContextSwitchTo(oldcontext);
}

static void
transformer_subtrans_commit(MemoryContext oldcontext, ResourceOwner oldowner)
{
	/* Commit the inner transaction, return to outer xact context */
	ReleaseCurrentSubTransaction();
	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = oldowner;

	/*
	 * AtEOSubXact_SPI() should not have popped any SPI context, but just in
	 * case it did, make sure we remain connected.
	 */
	SPI_restore_connection();
}

static void
transformer_subtrans_abort(MemoryContext oldcontext, ResourceOwner oldowner)
{
	/* Abort the inner transaction */
	RollbackAndReleaseCurrentSubTransaction();
	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = oldowner;

	/*
	 * If AtEOSubXact_SPI() popped any SPI context of the subxact, it will
	 * have left us in a disconnected state.  We need this hack to return to
	 * connected state.
	 */
	SPI_restore_connection();
}

/*
 * executeTupleTransform - execute the tuple transform.
 *
 * Build a spi plan for reuse, so for incoming tuples, we can save the
 * planing time.
 *
 * When executing spi plan for each tuple, handle the single row error.
 */
HeapTuple
executeTupleTransform(TransformerInfo *transformer, HeapTuple tuple, CopyState cstate, bool *errskipped /* out */)
{
	ListCell	   *cell;
	int				i = 0;
	HeapTuple		result;
	int				execresult;
	MemoryContext	oldcontext = CurrentMemoryContext;
	ResourceOwner	oldowner = CurrentResourceOwner;

	*errskipped = false;
	if (!transformer || !tuple)
		return tuple;

	if (!transformer->spiplan)
		transformer->spiplan = build_transform_plan(transformer);

	foreach(cell, transformer->arguments)
	{
		int		idx = lfirst_int(cell);
		int		attnum = bms_nth_member(transformer->attrsidx, idx);
		bool	isnull;

		Assert(attnum > 0);
		transformer->paramvalues[i] = heap_getattr(tuple, attnum, transformer->tupledesc, &isnull);
		transformer->paramnulls[i] = isnull ? 'n' : ' ' ;
		i++;
	}

	transformer_subtrans_begin(oldcontext);
	PG_TRY();
	{
		execresult = SPI_execute_plan(transformer->spiplan, transformer->paramvalues,
								  transformer->paramnulls, false, 0);

        transformer_subtrans_commit(oldcontext, oldowner);
	}
	PG_CATCH();
	{
		*errskipped = true;
		/* handle single row error for each tuple transform */
		MemoryContextSwitchTo(oldcontext);
		if (transformer->handleerror)
		{
			HANDLE_ALL_SINGLE_ROW_ERROR(cstate); /* handle all error codes */
		}
		else
		{
			HANDLE_DATA_ERROR(cstate);	/* only handle ERRCODE_DATA_EXCEPTION */
		}
		FlushErrorState();

		transformer_subtrans_abort(oldcontext, oldowner);
	}
	PG_END_TRY();

	heap_freetuple(tuple);
	if (*errskipped)
	{
		SPI_freetuptable(SPI_tuptable);
		ErrorIfRejectLimitReached(cstate->cdbsreh);
		return NULL;
	}

	if (SPI_OK_SELECT != execresult)
		ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			 errmsg("SPI execute failed with result %d", execresult)));

	/* Should not fall into this error */
	if (SPI_tuptable == NULL || SPI_processed < 1)
		ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			 errmsg("No tuple returned from 'transform_expression'.")));

	result = SPI_copytuple(SPI_tuptable->vals[0]);
	if (SPI_processed == 1)
		SPI_freetuptable(SPI_tuptable);
	transformer->nexttupleidx = 1;
	return result;
}

