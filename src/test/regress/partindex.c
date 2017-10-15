#include "postgres.h"

#include "funcapi.h"
#include "cdb/cdbpartition.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/spi.h"
#include "optimizer/prep.h"
#include "optimizer/planmain.h"
#include "utils/lsyscache.h"

extern Datum gp_build_logical_index_info(PG_FUNCTION_ARGS);
extern Datum gp_get_physical_index_relid(PG_FUNCTION_ARGS);

/*
 * number of output columns for the UDF for retrieving indexes on partitioned
 * tables
 */
#define NUM_COLS 9

/*
 * gp_build_logical_index_info
 *   Set returning function - returns index information on a partitioned-
 *   table. One row per logical index in the partitioning hierarchy
 *   is returned. Additional information is returned for indexes on default
 *   partitions.
 *
 *   Each physical index with the same index key, index predicate, index-
 *   expression, and uniqueness attribute is considered the same logical
 *   index.
 *
 *   This function is only added to test BuildLogicalIndexInfo
 */
PG_FUNCTION_INFO_V1(gp_build_logical_index_info);
Datum
gp_build_logical_index_info(PG_FUNCTION_ARGS)
{
	Oid		relid = PG_GETARG_OID(0);
	FuncCallContext	*funcctx;
	MemoryContext	oldcontext;
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	bool		nulls[NUM_COLS];
	LogicalIndexes	*partsLI;

	if (SRF_IS_FIRSTCALL())
	{
		/* create a function context */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch memory context for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(NUM_COLS, false);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "logicalIndexId",
					OIDOID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "nColumns",
					INT2OID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "indexKeys",
					TEXTOID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "indIsUnique",
					BOOLOID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "indPred",
					TEXTOID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "indExprs",
					TEXTOID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "partConsBin",
					TEXTOID, -1, 0);

		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "defaultLevels",
					TEXTOID, -1, 0);
		
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "indType",
				INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		partsLI = (LogicalIndexes *)palloc(sizeof(LogicalIndexes));
		funcctx->user_fctx = (void *) partsLI;

		/* do the actual work */
		partsLI = BuildLogicalIndexInfo(relid);

		funcctx->user_fctx = (void *) partsLI;

		if (partsLI)
			funcctx->max_calls = partsLI->numLogicalIndexes;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	partsLI = (LogicalIndexes *)funcctx->user_fctx;
	
	if (funcctx->call_cntr < funcctx->max_calls)
	{
		/* fetch each tuple, and return */
		Datum values[NUM_COLS];
		Datum result;
		char *c;
		text *t;
		StringInfoData keys;
		int i;

		LogicalIndexInfo *li = partsLI->logicalIndexInfo[funcctx->call_cntr];

		for (int i = 0; i < NUM_COLS; i++)
			nulls[i] = false;

		values[0] = ObjectIdGetDatum(li->logicalIndexOid);

		values[1] = Int16GetDatum(li->nColumns);

		initStringInfo(&keys);
		for (i = 0; i < li->nColumns; i++)
			appendStringInfo(&keys, "%d ",li->indexKeys[i]);
							
		t = cstring_to_text(keys.data);
		values[2] = PointerGetDatum(t);

		values[3] = BoolGetDatum(li->indIsUnique);

		if (li->indPred)
		{
			c = nodeToString(li->indPred);
			t = cstring_to_text(c);
			values[4] = PointerGetDatum(t);
		}
		else
			nulls[4] = true;

		if (li->indExprs)
		{
			c = nodeToString(li->indExprs);
			t = cstring_to_text(c);
			values[5] = PointerGetDatum(t);
		}
		else
			nulls[5] = true;


		if (li->partCons)
		{
			/* get the expr form -- for readability */
			c = deparse_expression(li->partCons,
			deparse_context_for(get_rel_name(relid),
						relid),
						false, false);
			t = cstring_to_text(c);
			values[6] = PointerGetDatum(t);
		}
		else
			nulls[6] = true;

		if (li->defaultLevels)
		{
			c = nodeToString(li->defaultLevels); 
			t = cstring_to_text(c);
			values[7] = PointerGetDatum(t);
		}
		else
			nulls[7] = true;

		values[8] = li->indType;
		nulls[8] = false;
		
		/* build tuple */
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		/* make the tuple into a datum */
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * function wrapper for testing getPhysicalIndexRelid
 */
PG_FUNCTION_INFO_V1(gp_get_physical_index_relid);
Datum
gp_get_physical_index_relid(PG_FUNCTION_ARGS)
{
	Oid                     rootOid = PG_GETARG_OID(0);
	Oid                     partOid = PG_GETARG_OID(1);
	LogicalIndexInfo	logicalIndexInfo;
	Oid                     resultOid;
	int2vector		*indexKeys;
	text			*inText;
	Relation		rel;

	logicalIndexInfo.nColumns = 0;
	logicalIndexInfo.indexKeys = NULL;
	logicalIndexInfo.indPred = NIL;
	logicalIndexInfo.indExprs = NIL;

	if (!PG_ARGISNULL(2))
	{
		indexKeys = (int2vector *)PG_GETARG_POINTER(2);
		logicalIndexInfo.nColumns = indexKeys->dim1;
		logicalIndexInfo.indexKeys = (AttrNumber *)palloc0(indexKeys->dim1 * sizeof(AttrNumber));
		
		for (int i = 0; i < indexKeys->dim1; i++)
			logicalIndexInfo.indexKeys[i] = indexKeys->values[i];
	}

	if (!PG_ARGISNULL(3))
	{
		Node	   *indPred;

		inText = PG_GETARG_TEXT_P(3);

		indPred = stringToNode(text_to_cstring(inText));

		/* Perform the same normalization as relcache.c does. */
		indPred = eval_const_expressions(NULL, indPred);
		indPred = (Node *) canonicalize_qual((Expr *) indPred);
		set_coercionform_dontcare(indPred);
		indPred = (Node *) make_ands_implicit((Expr *) indPred);
		fix_opfuncids(indPred);

		logicalIndexInfo.indPred = (List *) indPred;
	}

	if (!PG_ARGISNULL(4))
	{
		Node	   *indExprs;

		inText = PG_GETARG_TEXT_P(4);

		indExprs = stringToNode(text_to_cstring(inText));

		/* Perform the same normalization as relcache.c does. */
		indExprs = eval_const_expressions(NULL, indExprs);
		set_coercionform_dontcare(indExprs);
		fix_opfuncids(indExprs);

		logicalIndexInfo.indExprs = (List *) indExprs;
	}	

	logicalIndexInfo.indIsUnique = PG_GETARG_BOOL(5);

	AttrNumber *attMap = IndexScan_GetColumnMapping(rootOid, partOid);

	rel = heap_open(partOid, AccessShareLock);

	/*
	 * The varno is hard-coded to 1 as the original getPhysicalIndexRelid was
	 * using a hard-coded 1 for varattno mapping of logicalIndexInfo.
	 */
	IndexScan_MapLogicalIndexInfo(&logicalIndexInfo, attMap, 1);
	/* do the actual work */
	resultOid = getPhysicalIndexRelid(rel, &logicalIndexInfo);

	if (attMap)
	{
		pfree(attMap);
	}

	heap_close(rel, AccessShareLock);

	return ObjectIdGetDatum(resultOid);
}
