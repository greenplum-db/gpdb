/*-------------------------------------------------------------------------
 *
 * nodeAgg.c
 *	  Routines to handle aggregate nodes.
 *
 *	  ExecAgg evaluates each aggregate in the following steps:
 *
 *		 transvalue = initcond
 *		 foreach input_tuple do
 *			transvalue = transfunc(transvalue, input_value(s))
 *		 result = finalfunc(transvalue, direct_argument(s))
 *
 *	  If a finalfunc is not supplied then the result is just the ending
 *	  value of transvalue.
 *
 *	  If a normal aggregate call specifies DISTINCT or ORDER BY, we sort the
 *	  input tuples and eliminate duplicates (if required) before performing
 *	  the above-depicted process.  (However, we don't do that for ordered-set
 *	  aggregates; their "ORDER BY" inputs are ordinary aggregate arguments
 *	  so far as this module is concerned.)
 *
 *	  If transfunc is marked "strict" in pg_proc and initcond is NULL,
 *	  then the first non-NULL input_value is assigned directly to transvalue,
 *	  and transfunc isn't applied until the second non-NULL input_value.
 *	  The agg's first input type and transtype must be the same in this case!
 *
 *	  If transfunc is marked "strict" then NULL input_values are skipped,
 *	  keeping the previous transvalue.	If transfunc is not strict then it
 *	  is called for every input tuple and must deal with NULL initcond
 *	  or NULL input_values for itself.
 *
 *	  If finalfunc is marked "strict" then it is not called when the
 *	  ending transvalue is NULL, instead a NULL result is created
 *	  automatically (this is just the usual handling of strict functions,
 *	  of course).  A non-strict finalfunc can make its own choice of
 *	  what to return for a NULL ending transvalue.
 *
 *	  Ordered-set aggregates are treated specially in one other way: we
 *	  evaluate any "direct" arguments and pass them to the finalfunc along
 *	  with the transition value.
 *
 *	  A finalfunc can have additional arguments beyond the transvalue and
 *	  any "direct" arguments, corresponding to the input arguments of the
 *	  aggregate.  These are always just passed as NULL.  Such arguments may be
 *	  needed to allow resolution of a polymorphic aggregate's result type.
 *
 *	  We compute aggregate input expressions and run the transition functions
 *	  in a temporary econtext (aggstate->tmpcontext).  This is reset at
 *	  least once per input tuple, so when the transvalue datatype is
 *	  pass-by-reference, we have to be careful to copy it into a longer-lived
 *	  memory context, and free the prior value to avoid memory leakage.
 *
 *    Postgres stores transvalues in the memory context aggstate->aggcontext,
 *	  which is also used for the hashtable structures in AGG_HASHED mode.
 *    MPP (in order to support hybrid hash aggregation) stores hash table
 *    entries and associated transition values in aggstate->aggcontext.
 *
 *	  The node's regular econtext (aggstate->ss.ps.ps_ExprContext)
 *	  is used to run finalize functions and compute the output tuple;
 *	  this context can be reset once per output tuple.
 *
 *	  Beginning in PostgreSQL 8.1, the executor's AggState node is passed as
 *	  the fmgr "context" value in all transfunc and finalfunc calls.  It is
 *	  not really intended that the transition functions will look into the
 *	  AggState node, but they can use code like
 *			if (fcinfo->context && IsA(fcinfo->context, AggState))
 *	  to verify that they are being called by nodeAgg.c and not as ordinary
 *	  SQL functions.  The main reason a transition function might want to know
 *	  that is that it can avoid palloc'ing a fixed-size pass-by-ref transition
 *	  value on every call: it can instead just scribble on and return its left
 *	  input.  Ordinarily it is completely forbidden for functions to modify
 *	  pass-by-ref inputs, but in the aggregate case we know the left input is
 *	  either the initial transition value or a previous function result, and
 *	  in either case its value need not be preserved.  See int8inc() for an
 *	  example.	Notice that advance_transition_function() is coded to avoid a
 *	  data copy step when the previous transition value pointer is returned.
 *
 *	  As of 9.4, aggregate transition functions can also use AggGetAggref()
 *	  to get hold of the Aggref expression node for their aggregate call.
 *	  This is mainly intended for ordered-set aggregates, which are not
 *	  supported as window functions.  (A regular aggregate function would
 *	  need some fallback logic to use this, since there's no Aggref node
 *	  for a window function.)
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeAgg.c,v 1.164 2009/01/01 17:23:41 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "executor/executor.h"
#include "executor/execHHashagg.h"
#include "executor/nodeAgg.h"
#include "lib/stringinfo.h"             /* StringInfo */
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/datum.h"

#include "cdb/cdbexplain.h"
#include "cdb/cdbvars.h" /* mpp_hybrid_hash_agg */

#define IS_HASHAGG(aggstate) (((Agg *) (aggstate)->ss.ps.plan)->aggstrategy == AGG_HASHED)

/*
 * AggStatePerAggData -- per-aggregate working state
 * AggStatePerGroupData - per-aggregate-per-group working state
 *
 * Definition moved to nodeAgg.c to provide visibility to execHHashagg.c
 */


/*
 * To implement hashed aggregation, we need a hashtable that stores a
 * representative tuple and an array of AggStatePerGroup structs for each
 * distinct set of GROUP BY column values.	We compute the hash key from
 * the GROUP BY columns.
 */
typedef struct AggHashEntryData *AggHashEntry;

typedef struct AggHashEntryData
{
	TupleHashEntryData shared;	/* common header for hash table entries */
	/* per-aggregate transition status array - must be last! */
	AggStatePerGroupData pergroup[1];	/* VARIABLE LENGTH ARRAY */
} AggHashEntryData;				/* VARIABLE LENGTH STRUCT */

static void advance_transition_function(AggState *aggstate,
							AggStatePerAgg peraggstate,
							AggStatePerGroup pergroupstate,
							FunctionCallInfoData *fcinfo,
							MemoryManagerContainer *mem_manager);
static void process_ordered_aggregate_single(AggState *aggstate,
								 AggStatePerAgg peraggstate,
								 AggStatePerGroup pergroupstate);
static void process_ordered_aggregate_multi(AggState *aggstate,
								AggStatePerAgg peraggstate,
								AggStatePerGroup pergroupstate);
static void finalize_aggregate(AggState *aggstate,
				   AggStatePerAgg peraggstate,
				   AggStatePerGroup pergroupstate,
				   Datum *resultVal, bool *resultIsNull);
static Bitmapset *find_unaggregated_cols(AggState *aggstate);
static bool find_unaggregated_cols_walker(Node *node, Bitmapset **colnos);
static void clear_agg_object(AggState *aggstate);
static TupleTableSlot *agg_retrieve_direct(AggState *aggstate);
static TupleTableSlot *agg_retrieve_hash_table(AggState *aggstate);
static void ExecAggExplainEnd(PlanState *planstate, struct StringInfoData *buf);
static int count_extra_agg_slots(Node *node);
static bool count_extra_agg_slots_walker(Node *node, int *count);


Datum
datumCopyWithMemManager(Datum oldvalue, Datum value, bool typByVal, int typLen,
						MemoryManagerContainer *mem_manager)
{
	Datum		res;

	if (typByVal)
		res = value;
	else
	{
		Size realSize;
		Size old_realSize = 0;
		char *s;

		if (DatumGetPointer(value) == NULL)
			return PointerGetDatum(NULL);

		if (DatumGetPointer(oldvalue) != NULL)
			old_realSize = MAXALIGN(datumGetSize(oldvalue, typByVal, typLen));
		
		realSize = datumGetSize(value, typByVal, typLen);

		if (old_realSize == 0 || old_realSize < realSize)
		{
			int alloc_size = MAXALIGN(mem_manager->realloc_ratio * old_realSize);
			if (alloc_size < realSize)
				alloc_size = MAXALIGN(realSize);
			
			if (mem_manager->free)
			{
				(*mem_manager->free)(mem_manager->manager, DatumGetPointer(oldvalue));
			}
			
			s = (char *) (*mem_manager->alloc)(mem_manager->manager, alloc_size);
		}
			   
		else
			s = (char *) DatumGetPointer(oldvalue);
		
		memcpy(s, DatumGetPointer(value), realSize);
		res = PointerGetDatum(s);
	}
	return res;
}

/*
 * Initialize all aggregates for a new group of input values.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 *
 * Note that the memory allocation is done through provided memory manager.
 */
void
initialize_aggregates(AggState *aggstate,
					  AggStatePerAgg peragg,
					  AggStatePerGroup pergroup,
					  MemoryManagerContainer *mem_manager)
{
	int			aggno;

	for (aggno = 0; aggno < aggstate->numaggs; aggno++)
	{
		AggStatePerAgg peraggstate = &peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];

		/*
		 * Start a fresh sort operation for each DISTINCT/ORDER BY aggregate.
		 */
		if (peraggstate->numSortCols > 0)
		{
			/*
			 * In case of rescan, maybe there could be an uncompleted sort
			 * operation?  Clean it up if so.
			 */
			if (peraggstate->sortstate)
				tuplesort_end(peraggstate->sortstate);

			/*
			 * We use a plain Datum sorter when there's a single input column;
			 * otherwise sort the full tuple.  (See comments for
			 * process_ordered_aggregate_single.)
			 */
			if (peraggstate->numInputs == 1)
			{
				peraggstate->sortstate =
					tuplesort_begin_datum(&aggstate->ss,
										  peraggstate->evaldesc->attrs[0]->atttypid,
										  peraggstate->sortOperators[0],
										  peraggstate->sortNullsFirst[0],
										  PlanStateOperatorMemKB((PlanState *) aggstate), false);
			}
			else
			{
				peraggstate->sortstate =
					tuplesort_begin_heap(&aggstate->ss,
										 peraggstate->evaldesc,
										 peraggstate->numSortCols, peraggstate->sortColIdx,
										 peraggstate->sortOperators,
										 peraggstate->sortNullsFirst,
										 PlanStateOperatorMemKB((PlanState *) aggstate), false);
			}

			/*
			 * CDB: If EXPLAIN ANALYZE, let all of our tuplesort operations
			 * share our Instrumentation object and message buffer.
			 */
			if (aggstate->ss.ps.instrument)
				tuplesort_set_instrument(peraggstate->sortstate,
										 aggstate->ss.ps.instrument,
										 aggstate->ss.ps.cdbexplainbuf);

			/* CDB: Set enhanced sort options. */
			{
				int 		unique = peragg->aggref->aggdistinct &&
									 ( gp_enable_sort_distinct ? 1 : 0) ;
				int 		sort_flags = gp_sort_flags; /* get the guc */
				int         maxdistinct = gp_sort_max_distinct; /* get guc */

				cdb_tuplesort_init(peraggstate->sortstate,
								   unique,
								   sort_flags, maxdistinct);
			}
		}

		/*
		 * (Re)set transValue to the initial value.
		 *
		 * Note that when the initial value is pass-by-ref, we must copy it
		 * (into the aggcontext) since we will pfree the transValue later.
		 */
		if (peraggstate->initValueIsNull)
		{
			/* Clear the old transition value */
			if (!peraggstate->transtypeByVal &&
				!pergroupstate->transValueIsNull &&
				DatumGetPointer(pergroupstate->transValue) != NULL)
				pfree(DatumGetPointer(pergroupstate->transValue));
			pergroupstate->transValue = peraggstate->initValue;
		}
		
		else
		{
			if (!peraggstate->transtypeByVal &&
				!pergroupstate->transValueIsNull &&
				DatumGetPointer(pergroupstate->transValue) != NULL)
				pergroupstate->transValue =
					datumCopyWithMemManager(pergroupstate->transValue,
											peraggstate->initValue,
											peraggstate->transtypeByVal,
											peraggstate->transtypeLen,
											mem_manager);
			else
				pergroupstate->transValue =
					datumCopyWithMemManager(0,
											peraggstate->initValue,
											peraggstate->transtypeByVal,
											peraggstate->transtypeLen,
											mem_manager);
		}
		pergroupstate->transValueIsNull = peraggstate->initValueIsNull;

		/*
		 * If the initial value for the transition state doesn't exist in the
		 * pg_aggregate table then we will let the first non-NULL value
		 * returned from the outer procNode become the initial value. (This is
		 * useful for aggregates like max() and min().) The noTransValue flag
		 * signals that we still need to do this.
		 */
		pergroupstate->noTransValue = peraggstate->initValueIsNull;
	}
}

/*
 * Given new input value(s), advance the transition function of an aggregate.
 *
 * The new values (and null flags) have been preloaded into argument positions
 * 1 and up in fcinfo, so that we needn't copy them again to pass to the
 * transition function.  No other fields of fcinfo are assumed valid.
 *
 * It doesn't matter which memory context this is called in.
 */
static void
advance_transition_function(AggState *aggstate,
							AggStatePerAgg peraggstate,
							AggStatePerGroup pergroupstate,
							FunctionCallInfoData *fcinfo,
							MemoryManagerContainer *mem_manager)
{
	pergroupstate->transValue = 
		invoke_agg_trans_func(aggstate,
							  peraggstate,
							  &(peraggstate->transfn), 
							  peraggstate->numTransInputs,
							  pergroupstate->transValue,
							  &(pergroupstate->noTransValue),
							  &(pergroupstate->transValueIsNull),
							  peraggstate->transtypeByVal,
							  peraggstate->transtypeLen,
							  fcinfo, (void *)aggstate,
							  aggstate->tmpcontext->ecxt_per_tuple_memory,
							  mem_manager);
}

Datum
invoke_agg_trans_func(AggState *aggstate,
					  AggStatePerAgg peraggstate,
					  FmgrInfo *transfn, int numTransInputs, Datum transValue,
					  bool *noTransvalue, bool *transValueIsNull,
					  bool transtypeByVal, int16 transtypeLen, 
					  FunctionCallInfoData *fcinfo, void *funcctx,
					  MemoryContext tuplecontext,
					  MemoryManagerContainer *mem_manager)
{
	MemoryContext oldContext;
	Datum		newVal;
	int			i;

	if (transfn->fn_strict)
	{
		/*
		 * For a strict transfn, nothing happens when there's a NULL input; we
		 * just keep the prior transValue.
		 */
		for (i = 1; i <= numTransInputs; i++)
		{
			if (fcinfo->argnull[i])
				return transValue;
		}
		if (*noTransvalue)
		{
			/*
			 * transValue has not been initialized. This is the first non-NULL
			 * input value. We use it as the initial value for transValue. (We
			 * already checked that the agg's input type is binary-compatible
			 * with its transtype, so straight copy here is OK.)
			 *
			 * We must copy the datum into aggcontext if it is pass-by-ref.
			 * We do not need to pfree the old transValue, since it's NULL.
			 */
			newVal = datumCopyWithMemManager(transValue, fcinfo->arg[1], transtypeByVal,
											 transtypeLen, mem_manager);
			*transValueIsNull = false;
			*noTransvalue = false;
			fcinfo->isnull = false;

			return newVal;
		}
		if (*transValueIsNull)
		{
			/*
			 * Don't call a strict function with NULL inputs.  Note it is
			 * possible to get here despite the above tests, if the transfn is
			 * strict *and* returned a NULL on a prior cycle. If that happens
			 * we will propagate the NULL all the way to the end.
			 */
			fcinfo->isnull = true;
			return transValue;
		}
	}

	/* We run the transition functions in per-input-tuple memory context */
	oldContext = MemoryContextSwitchTo(tuplecontext);

	/* set up aggstate->curperagg for AggGetAggref() */
	aggstate->curperagg = peraggstate;

	/*
	 * OK to call the transition function
	 */
	InitFunctionCallInfoData(*fcinfo, transfn,
							 numTransInputs + 1,
							 (void *) funcctx, NULL);
	fcinfo->arg[0] = transValue;
	fcinfo->argnull[0] = *transValueIsNull;

	newVal = FunctionCallInvoke(fcinfo);

	aggstate->curperagg = NULL;

	/*
	 * If pass-by-ref datatype, must copy the new value into aggcontext and
	 * pfree the prior transValue.	But if transfn returned a pointer to its
	 * first input, we don't need to do anything.
	 */
	if (!transtypeByVal && 
		DatumGetPointer(newVal) != DatumGetPointer(transValue))
	{
		if (!fcinfo->isnull)
		{
			newVal = datumCopyWithMemManager(transValue, newVal, transtypeByVal,
											 transtypeLen, mem_manager);
		}
	}

	*transValueIsNull = fcinfo->isnull;
	if (!fcinfo->isnull)
		*noTransvalue = false;

	MemoryContextSwitchTo(oldContext);
	return newVal;
}

/*
 * Advance all the aggregates for one input tuple.	The input tuple
 * has been stored in tmpcontext->ecxt_outertuple, so that it is accessible
 * to ExecEvalExpr.  pergroup is the array of per-group structs to use
 * (this might be in a hashtable entry).
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
void
advance_aggregates(AggState *aggstate, AggStatePerGroup pergroup,
				   MemoryManagerContainer *mem_manager)
{
	int			aggno;

	for (aggno = 0; aggno < aggstate->numaggs; aggno++)
	{
		Datum value;
		bool isnull;
		AggStatePerAgg peraggstate = &aggstate->peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];
		ExprState  *filter = peraggstate->aggrefstate ? peraggstate->aggrefstate->aggfilter : NULL;
		int			numTransInputs = peraggstate->numTransInputs;
		int			i;
		TupleTableSlot *slot;

		/* Skip anything FILTERed out */
		if (filter)
		{
			Datum		res;
			bool		isnull;

			res = ExecEvalExprSwitchContext(filter, aggstate->tmpcontext,
											&isnull, NULL);
			if (isnull || !DatumGetBool(res))
				continue;
		}

		/* Evaluate the current input expressions for this aggregate */
		slot = ExecProject(peraggstate->evalproj, NULL);
		slot_getallattrs(slot);

		if (peraggstate->numSortCols > 0)
		{
			/* DISTINCT and/or ORDER BY case */
			Assert(slot->PRIVATE_tts_nvalid == peraggstate->numInputs);

			/*
			 * If the transfn is strict, we want to check for nullity before
			 * storing the row in the sorter, to save space if there are a lot
			 * of nulls.  Note that we must only check numTransInputs columns,
			 * not numInputs, since nullity in columns used only for sorting
			 * is not relevant here.
			 */
			if (peraggstate->transfn.fn_strict)
			{
				for (i = 0; i < numTransInputs; i++)
				{
					value = slot_getattr(slot, i+1, &isnull);
					
					if (isnull)
						break; /* arg loop */
				}
				if (i < numTransInputs)
					continue;
			}
			
			/* OK, put the tuple into the tuplesort object */
			if (peraggstate->numInputs == 1)
			{
				value = slot_getattr(slot, 1, &isnull);
				
				tuplesort_putdatum(peraggstate->sortstate,
								   value, isnull);
			}
			else
			{
				tuplesort_puttupleslot(peraggstate->sortstate, slot);
			}
		}
		else
		{
			/* We can apply the transition function immediately */
			FunctionCallInfoData fcinfo;

			/* Load values into fcinfo */
			/* Start from 1, since the 0th arg will be the transition value */
			Assert(slot->PRIVATE_tts_nvalid >= numTransInputs);
			for (i = 0; i < numTransInputs; i++)
			{
				fcinfo.arg[i + 1] = slot_getattr(slot, i+1, &isnull);
				fcinfo.argnull[i + 1] = isnull;
			}

			advance_transition_function(aggstate, peraggstate, pergroupstate,
										&fcinfo, mem_manager);
		}
	}
}


/*
 * Run the transition function for a DISTINCT or ORDER BY aggregate
 * with only one input.  This is called after we have completed
 * entering all the input values into the sort object.  We complete the
 * sort, read out the values in sorted order, and run the transition
 * function on each value (applying DISTINCT if appropriate).
 *
 * Note that the strictness of the transition function was checked when
 * entering the values into the sort, so we don't check it again here;
 * we just apply standard SQL DISTINCT logic.
 *
 * The one-input case is handled separately from the multi-input case
 * for performance reasons: for single by-value inputs, such as the
 * common case of count(distinct id), the tuplesort_getdatum code path
 * is around 300% faster.  (The speedup for by-reference types is less
 * but still noticeable.)
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
process_ordered_aggregate_single(AggState *aggstate,
								 AggStatePerAgg peraggstate,
								 AggStatePerGroup pergroupstate)
{
	Datum		oldVal = (Datum) 0;
	bool		oldIsNull = true;
	bool		haveOldVal = false;
	MemoryContext workcontext = aggstate->tmpcontext->ecxt_per_tuple_memory;
	MemoryContext oldContext;
	bool		isDistinct = (peraggstate->numDistinctCols > 0);
	Datum	   *newVal;
	bool	   *isNull;
	FunctionCallInfoData fcinfo;

	Assert(peraggstate->numDistinctCols < 2);

	tuplesort_performsort(peraggstate->sortstate);

	/* Load the column into argument 1 (arg 0 will be transition value) */
	newVal = fcinfo.arg + 1;
	isNull = fcinfo.argnull + 1;

	/*
	 * Note: if input type is pass-by-ref, the datums returned by the sort are
	 * freshly palloc'd in the per-query context, so we must be careful to
	 * pfree them when they are no longer needed.
	 */

	while (tuplesort_getdatum(peraggstate->sortstate, true, newVal, isNull))
	{
		/*
		 * Clear and select the working context for evaluation of the equality
		 * function and transition function.
		 */
		MemoryContextReset(workcontext);
		oldContext = MemoryContextSwitchTo(workcontext);

		/*
		 * If DISTINCT mode, and not distinct from prior, skip it.
		 */
		if (isDistinct &&
			haveOldVal &&
			((oldIsNull && *isNull) ||
			 (!oldIsNull && !*isNull &&
			  DatumGetBool(FunctionCall2(&peraggstate->equalfns[0],
										 oldVal, *newVal)))))
		{
			/* equal to prior, so forget this one */
			if (!peraggstate->inputtypeByVal && !*isNull)
				pfree(DatumGetPointer(*newVal));
		}
		else
		{
			advance_transition_function(aggstate, peraggstate, pergroupstate,
										&fcinfo, &(aggstate->mem_manager));
			/* forget the old value, if any */
			if (!oldIsNull && !peraggstate->inputtypeByVal)
				pfree(DatumGetPointer(oldVal));
			/* and remember the new one for subsequent equality checks */
			oldVal = *newVal;
			oldIsNull = *isNull;
			haveOldVal = true;
		}

		MemoryContextSwitchTo(oldContext);
	}

	if (!oldIsNull && !peraggstate->inputtypeByVal)
		pfree(DatumGetPointer(oldVal));

	tuplesort_end(peraggstate->sortstate);

	peraggstate->sortstate = NULL;
}

/*
 * Run the transition function for a DISTINCT or ORDER BY aggregate
 * with more than one input.  This is called after we have completed
 * entering all the input values into the sort object.  We complete the
 * sort, read out the values in sorted order, and run the transition
 * function on each value (applying DISTINCT if appropriate).
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
process_ordered_aggregate_multi(AggState *aggstate,
								AggStatePerAgg peraggstate,
								AggStatePerGroup pergroupstate)
{
	MemoryContext workcontext = aggstate->tmpcontext->ecxt_per_tuple_memory;
	FunctionCallInfoData fcinfo;
	TupleTableSlot *slot1 = peraggstate->evalslot;
	TupleTableSlot *slot2 = peraggstate->uniqslot;
	int			numTransInputs = peraggstate->numTransInputs;
	int			numDistinctCols = peraggstate->numDistinctCols;
	bool		haveOldValue = false;
	int			i;

	tuplesort_performsort(peraggstate->sortstate);

	ExecClearTuple(slot1);
	if (slot2)
		ExecClearTuple(slot2);

	while (tuplesort_gettupleslot(peraggstate->sortstate, true, slot1))
	{
		/*
		 * Extract the first numTransInputs columns as datums to pass to the
		 * transfn.  (This will help execTuplesMatch too, so we do it
		 * immediately.)
		 */
		slot_getsomeattrs(slot1, numTransInputs);

		if (numDistinctCols == 0 ||
			!haveOldValue ||
			!execTuplesMatch(slot1, slot2,
							 numDistinctCols,
							 peraggstate->sortColIdx,
							 peraggstate->equalfns,
							 workcontext))
		{
			/* Load values into fcinfo */
			/* Start from 1, since the 0th arg will be the transition value */
			for (i = 0; i < numTransInputs; i++)
			{
				fcinfo.arg[i + 1] = slot_get_values(slot1)[i];
				fcinfo.argnull[i + 1] = slot_get_isnull(slot1)[i];
			}

			advance_transition_function(aggstate, peraggstate, pergroupstate,
										&fcinfo, &(aggstate->mem_manager));

			if (numDistinctCols > 0)
			{
				/* swap the slot pointers to retain the current tuple */
				TupleTableSlot *tmpslot = slot2;

				slot2 = slot1;
				slot1 = tmpslot;
				haveOldValue = true;
			}
		}

		/* Reset context each time, unless execTuplesMatch did it for us */
		if (numDistinctCols == 0)
			MemoryContextReset(workcontext);

		ExecClearTuple(slot1);
	}

	if (slot2)
		ExecClearTuple(slot2);

	tuplesort_end(peraggstate->sortstate);

	peraggstate->sortstate = NULL;
}

/*
 * Compute the final value of one aggregate.
 *
 * The finalfunction will be run, and the result delivered, in the
 * output-tuple context; caller's CurrentMemoryContext does not matter.
 */
static void
finalize_aggregate(AggState *aggstate,
				   AggStatePerAgg peraggstate,
				   AggStatePerGroup pergroupstate,
				   Datum *resultVal, bool *resultIsNull)
{
	FunctionCallInfoData fcinfo;
	bool		anynull = false;
	MemoryContext oldContext;
	int			i;
	ListCell   *lc;

	oldContext = MemoryContextSwitchTo(aggstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

	/*
	 * Evaluate any direct arguments.  We do this even if there's no finalfn
	 * (which is unlikely anyway), so that side-effects happen as expected.
	 * The direct arguments go into arg positions 1 and up, leaving position 0
	 * for the transition state value.
	 */
	i = 1;
	foreach(lc, peraggstate->aggrefstate->aggdirectargs)
	{
		ExprState  *expr = (ExprState *) lfirst(lc);

		fcinfo.arg[i] = ExecEvalExpr(expr,
									 aggstate->ss.ps.ps_ExprContext,
									 &fcinfo.argnull[i],
									 NULL);
		anynull |= fcinfo.argnull[i];
		i++;
	}

	/*
	 * Apply the agg's finalfn if one is provided, else return transValue.
	 */
	if (OidIsValid(peraggstate->finalfn_oid))
	{
		int			numFinalArgs = peraggstate->numFinalArgs;

		/* set up aggstate->curperagg for AggGetAggref() */
		aggstate->curperagg = peraggstate;

		InitFunctionCallInfoData(fcinfo, &(peraggstate->finalfn),
								 numFinalArgs,
								 (void *) aggstate, NULL);

		/* Fill in the transition state value */
		fcinfo.arg[0] = pergroupstate->transValue;
		fcinfo.argnull[0] = pergroupstate->transValueIsNull;
		anynull |= pergroupstate->transValueIsNull;

		/* Fill any remaining argument positions with nulls */
		for (; i < numFinalArgs; i++)
		{
			fcinfo.arg[i] = (Datum) 0;
			fcinfo.argnull[i] = true;
			anynull = true;
		}

		if (fcinfo.flinfo->fn_strict && anynull)
		{
			/* don't call a strict function with NULL inputs */
			*resultVal = (Datum) 0;
			*resultIsNull = true;
		}
		else
		{
			*resultVal = FunctionCallInvoke(&fcinfo);
			*resultIsNull = fcinfo.isnull;
		}
		aggstate->curperagg = NULL;
	}
	else
	{
		*resultVal = pergroupstate->transValue;
		*resultIsNull = pergroupstate->transValueIsNull;
	}

	/*
	 * If result is pass-by-ref, make sure it is in the right context.
	 */
	if (!peraggstate->resulttypeByVal && !*resultIsNull &&
		!MemoryContextContainsGenericAllocation(CurrentMemoryContext,
							   DatumGetPointer(*resultVal)))
		*resultVal = datumCopy(*resultVal,
							   peraggstate->resulttypeByVal,
							   peraggstate->resulttypeLen);

	MemoryContextSwitchTo(oldContext);
}

/*
 * find_unaggregated_cols
 *	  Construct a bitmapset of the column numbers of un-aggregated Vars
 *	  appearing in our targetlist and qual (HAVING clause)
 */
static Bitmapset *
find_unaggregated_cols(AggState *aggstate)
{
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
	Bitmapset  *colnos;

	colnos = NULL;
	(void) find_unaggregated_cols_walker((Node *) node->plan.targetlist,
										 &colnos);
	(void) find_unaggregated_cols_walker((Node *) node->plan.qual,
										 &colnos);
	return colnos;
}

static bool
find_unaggregated_cols_walker(Node *node, Bitmapset **colnos)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		/* setrefs.c should have set the varno to OUTER */
		Assert(var->varno == OUTER);
		Assert(var->varlevelsup == 0);
		*colnos = bms_add_member(*colnos, var->varattno);
		return false;
	}
	if (IsA(node, Aggref))		/* do not descend into aggregate exprs */
		return false;
	return expression_tree_walker(node, find_unaggregated_cols_walker,
								  (void *) colnos);
}

/*
 * Create a list of the tuple columns that actually need to be stored in
 * hashtable entries.  The incoming tuples from the child plan node will
 * contain grouping columns, other columns referenced in our targetlist and
 * qual, columns used to compute the aggregate functions, and perhaps just
 * junk columns we don't use at all.  Only columns of the first two types
 * need to be stored in the hashtable, and getting rid of the others can
 * make the table entries significantly smaller.  To avoid messing up Var
 * numbering, we keep the same tuple descriptor for hashtable entries as the
 * incoming tuples have, but set unwanted columns to NULL in the tuples that
 * go into the table.
 *
 * To eliminate duplicates, we build a bitmapset of the needed columns, then
 * convert it to an integer list (cheaper to scan at runtime). The list is
 * in decreasing order so that the first entry is the largest;
 * lookup_hash_entry depends on this to use slot_getsomeattrs correctly.
 *
 * Note: at present, searching the tlist/qual is not really necessary since
 * the parser should disallow any unaggregated references to ungrouped
 * columns.  However, the search will be needed when we add support for
 * SQL99 semantics that allow use of "functionally dependent" columns that
 * haven't been explicitly grouped by.
 */
static List *
find_hash_columns(AggState *aggstate)
{
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
	Bitmapset  *colnos;
	List	   *collist;
	int			i;

	/* Find Vars that will be needed in tlist and qual */
	colnos = find_unaggregated_cols(aggstate);
	/* Add in all the grouping columns */
	for (i = 0; i < node->numCols; i++)
		colnos = bms_add_member(colnos, node->grpColIdx[i]);
	/* Convert to list, using lcons so largest element ends up first */
	collist = NIL;
	while ((i = bms_first_member(colnos)) >= 0)
		collist = lcons_int(i, collist);
	bms_free(colnos);

	return collist;
}

/*
 * Estimate per-hash-table-entry overhead for the planner.
 *
 * Note that the estimate does not include space for pass-by-reference
 * transition data values, nor for the representative tuple of each group.
 */
Size
hash_agg_entry_size(int numAggs)
{
	Size		entrysize;

	/* This must match build_hash_table */
	entrysize = sizeof(AggHashEntryData) +
		(numAggs - 1) *sizeof(AggStatePerGroupData);
	entrysize = MAXALIGN(entrysize);
	/* Account for hashtable overhead (assuming fill factor = 1) */
	entrysize += 3 * sizeof(void *);
	return entrysize;
}

/*
 * ExecAgg -
 *
 *	  ExecAgg receives tuples from its outer subplan and aggregates over
 *	  the appropriate attribute for each aggregate function use (Aggref
 *	  node) appearing in the targetlist or qual of the node.  The number
 *	  of tuples to aggregate over depends on whether grouped or plain
 *	  aggregation is selected.	In grouped aggregation, we produce a result
 *	  row for each group; in plain aggregation there's a single result row
 *	  for the whole query.	In either case, the value of each aggregate is
 *	  stored in the expression context to be used when ExecProject evaluates
 *	  the result tuple.
 *
 * XXX: Fix BTree code.
 *
 * Streaming bottom: forces end of passes when no tuple for underlying node.  
 *
 * MPP-2614: Btree scan will return null tuple at end of scan.  However,
 * if one calls ExecProNode again on a btree scan, it will restart from
 * beginning even though we did not call rescan.  This is a bug on
 * btree scan, but mask it off here for v3.1.  Really should fix Btree
 * code.
 */
TupleTableSlot *
ExecAgg(AggState *node)
{
	if (node->agg_done)
	{
		ExecEagerFreeAgg(node);
		return NULL;
	}

#if 0
	/*
	 * Check to see if we're still projecting out tuples from a previous agg
	 * tuple (because there is a function-returning-set in the projection
	 * expressions).  If so, try to project another one.
	 */
	if (node->ss.ps.ps_TupFromTlist)
	{
		TupleTableSlot *result;
		ExprDoneCond isDone;

		result = ExecProject(node->ss.ps.ps_ProjInfo, &isDone);
		if (isDone == ExprMultipleResult)
			return result;
		/* Done with that source tuple... */
		node->ss.ps.ps_TupFromTlist = false;
	}
#endif

	if (((Agg *) node->ss.ps.plan)->aggstrategy == AGG_HASHED)
	{
		TupleTableSlot *tuple = NULL;
		bool		streaming = ((Agg *) node->ss.ps.plan)->streaming;

		/*
		 * ExecAgg processing for hashed aggregation -- returns the
		 * next result tuple or NULL. When returning NULL also sets
		 * aggstate to prevent future calls.
		 */

	
		if (node->hhashtable == NULL)
		{
			bool		tupremain;

			node->hhashtable = create_agg_hash_table(node);
			node->hashaggstatus = HASHAGG_BEFORE_FIRST_PASS;
			tupremain = agg_hash_initial_pass(node);

			if (streaming)
			{
				if (tupremain)
					node->hashaggstatus = HASHAGG_STREAMING;
				else
					node->hashaggstatus = HASHAGG_END_OF_PASSES;
			}
			else
				node->hashaggstatus = HASHAGG_BETWEEN_PASSES;
		}

		/*
		 * On each call we either return a tuple corresponding to a hash entry
		 * (consuming the entry) or fall through to a state machine that tries
		 * to make additional hash entries available and continue the loop.
		 * (This may result in reaching the "exit" state and returning a NULL
		 * tuple).
		 */
		for (;;)
		{
			if (!node->hhashtable->is_spilling)
			{
				tuple = agg_retrieve_hash_table(node);
				node->agg_done = false; /* Not done 'til batches used up. */

				if (tuple != NULL)
					return tuple;
			}

			switch (node->hashaggstatus)
			{
				case HASHAGG_BETWEEN_PASSES:
					Assert(!streaming);
					if (agg_hash_next_pass(node))
					{
						node->hashaggstatus = HASHAGG_BETWEEN_PASSES;
						continue;
					}
					node->hashaggstatus = HASHAGG_END_OF_PASSES;
					/*
					 * pass through. Be sure that the next case statement
					 * is HASHAGG_END_OF_PASSES.
					 */

				case HASHAGG_END_OF_PASSES:
					node->agg_done = true;
					/* Append stats before destroying the htable for EXPLAIN ANALYZE */
					if (node->ss.ps.instrument)
					{
						agg_hash_explain(node);
					}
					ExecEagerFreeAgg(node);
					return NULL;

				case HASHAGG_STREAMING:
					Assert(streaming);
					if (!agg_hash_stream(node))
						node->hashaggstatus = HASHAGG_END_OF_PASSES;
					continue;

				case HASHAGG_BEFORE_FIRST_PASS:
				default:
					elog(ERROR, "hybrid hash aggregation sequencing error");
			}
		}
	}
	else
		return agg_retrieve_direct(node);
}

/*
 * clear_agg_object
 * 		Clear necessary memory (pergroup & perpassthrough) when agg context memory get reset & deleted.
 * 		aggstate - pointer to aggstate
 */
static void
clear_agg_object(AggState *aggstate)
{
	int			aggno = 0;

	for (aggno = 0; aggno < aggstate->numaggs; aggno++)
	{
		AggStatePerGroup pergroupstate = &aggstate->pergroup[aggno];

		pergroupstate->transValue = 0;
		pergroupstate->transValueIsNull = true;
		if (NULL != aggstate->perpassthru)
		{
			pergroupstate = &aggstate->perpassthru[aggno];
			pergroupstate->transValue = 0;
			pergroupstate->transValueIsNull = true;
		}
	}
}

/*
 * ExecAgg for non-hashed case
 */
static TupleTableSlot *
agg_retrieve_direct(AggState *aggstate)
{
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
	PlanState  *outerPlan;
	ExprContext *econtext;
	ExprContext *tmpcontext;
	Datum	   *aggvalues;
	bool	   *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	AggStatePerGroup perpassthru;
	TupleTableSlot *outerslot = NULL;
	TupleTableSlot *firstSlot;
	int			aggno;

	bool        passthru_ready = false;
	bool        has_partial_agg = aggstate->has_partial_agg;

	uint64      input_grouping = node->inputGrouping;
	bool        input_has_grouping = node->inputHasGrouping;
	bool        is_final_rollup_agg =
		(node->lastAgg ||
		 (input_has_grouping && node->numNullCols == 0));
	bool        is_middle_rollup_agg =
		(input_has_grouping && node->numNullCols > 0);

	/*
	 * get state info from node
	 */
	outerPlan = outerPlanState(aggstate);
	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	/* tmpcontext is the per-input-tuple expression context */
	tmpcontext = aggstate->tmpcontext;
	peragg = aggstate->peragg;
	pergroup = aggstate->pergroup;
	perpassthru = aggstate->perpassthru;
	firstSlot = aggstate->ss.ss_ScanTupleSlot;

	if (aggstate->agg_done)
		return NULL;

	/*
	 * We loop retrieving tuples until we find one that matches
	 * aggstate->ss.ps.qual, or find a pass-thru tuple when this Agg
	 * node is part of a ROLLUP query.
	 */
	while (!aggstate->agg_done)
	{
		/* Indicate if an input tuple may need to be pass-thru. */
		bool maybe_passthru = false;

		/*
		 * grp_firstTuple represents three types of input tuples:
		 *   (1) an input tuple that is the first tuple in a new
		 *       group and needs to be aggregated.
		 *   (2) an input tuple that only needs to be pass-thru.
		 *   (3) an input tuple that needs to be aggregated and
		 *       be pass-thru.
		 *
		 * When this value is set here, it means that we have previously
		 * read a tuple that crossed the grouping boundary, and this is that
		 * tuple. It could be any of the above three types.
		 *
		 * When this value is not set, there are two cases:
		 *  (1) If aggstate->has_parital_agg is false, then the first tuple
		 *      in a new group is not read.
		 *  (2) If aggstate->has_parital_agg is true, then the first tuple
		 *      in a new group has been read and aggregated into the result.
		 */
		if (aggstate->grp_firstTuple != NULL)
			maybe_passthru = true;

		/*
		 * If we don't already have the first tuple of the new group,
		 * fetch it from the outer plan.
		 */
		if (!aggstate->has_partial_agg && aggstate->grp_firstTuple == NULL)
		{
			outerslot = ExecProcNode(outerPlan);
			if (!TupIsNull(outerslot))
			{
				/*
				 * Make a copy of the first tuple; we will use this for
				 * comparison (in group mode) and for projection.
				 */

				slot_getallattrs(outerslot);
				aggstate->grp_firstTuple = memtuple_form_to(firstSlot->tts_mt_bind,
						slot_get_values(outerslot),
						slot_get_isnull(outerslot),
						NULL, NULL, false);
			}
			else
			{
				/* outer plan produced no tuples at all */
				aggstate->agg_done = true;
				/* If we are grouping, we should produce no tuples too */
				if (node->aggstrategy != AGG_PLAIN)
					return NULL;
			}
		}

		if (!aggstate->has_partial_agg)
		{
			/*
			 * Clear the per-output-tuple context for each group, as well as
			 * aggcontext (which contains any pass-by-ref transvalues of the old
			 * group).	We also clear any child contexts of the aggcontext; some
			 * aggregate functions store working state in such contexts.
			 *
			 * We use ReScanExprContext not just ResetExprContext because we want
			 * any registered shutdown callbacks to be called.	That allows
			 * aggregate functions to ensure they've cleaned up any non-memory
			 * resources.
			 */
			ReScanExprContext(econtext);

			MemoryContextResetAndDeleteChildren(aggstate->aggcontext);

			/* Clear necessary memory (pergroup & perpassthrough) when aggcontext get reset & deleted */
			clear_agg_object(aggstate);

			/*
			 * Initialize working state for a new input tuple group
			 */
			initialize_aggregates(aggstate, peragg, pergroup, &(aggstate->mem_manager));
		}

		/* Process the remaining tuples in the new group. */
		if (aggstate->has_partial_agg || aggstate->grp_firstTuple != NULL)
		{
			int outer_grouping = input_grouping;

			if (!aggstate->has_partial_agg)
			{
				/*
				 * Store the copied first input tuple in the tuple table slot
				 * reserved for it. The tuple will be deleted when it is cleared
				 * from the slot.
				 */

				ExecStoreMinimalTuple(aggstate->grp_firstTuple,
							   firstSlot,
							   true);
				aggstate->grp_firstTuple = NULL;        /* don't keep two pointers */

				/*
				 * Determine if this first tuple is a simple pass-thru tuple,
				 * or need to be aggregated.
				 */
				outer_grouping =
					tuple_grouping(firstSlot, node->numCols-node->numNullCols,
								   input_grouping, input_has_grouping,
								   (node->numCols-node->numNullCols) > 0 ?
								   node->grpColIdx[node->numCols-node->numNullCols-2]: 0);

				Assert(outer_grouping <= input_grouping);
			}

			if (outer_grouping < input_grouping)
			{
				/* This tuple is a pass-thru tuple. */
				passthru_ready = true;
				outerslot = firstSlot;
			}

			else
			{
				/*
				 * This tuple needs to be aggregated and may need to be
				 * passthru.
				 */
				if (is_middle_rollup_agg &&
					!aggstate->has_partial_agg)
					maybe_passthru = true;

				/* set up for first advance aggregates call */
				tmpcontext->ecxt_outertuple = firstSlot;

				/*
				 * Process each outer-plan tuple, and then fetch the next one,
				 * until we exhause the outer plan or cross a group boundary.
				 */
				for (;;)
				{
					if (!aggstate->has_partial_agg)
					{
						has_partial_agg = true;
						call_AdvanceAggregates(aggstate, pergroup, &(aggstate->mem_manager));
					}

					/* Reset per-input-tuple context after each tuple */
					ResetExprContext(tmpcontext);

					if (is_middle_rollup_agg &&
						maybe_passthru)
					{
						outerslot = firstSlot;
						passthru_ready = true;
						aggstate->has_partial_agg = has_partial_agg;
						break;
					}

					outerslot = ExecProcNode(outerPlan);
					if (TupIsNull(outerslot))
					{
						/* no more outer-plan tuples avaiable */
						aggstate->agg_done = true;
						break;
					}

					/* set up for next advance aggregates call */
					tmpcontext->ecxt_outertuple = outerslot;

					/*
					 * If we are grouping, check whether we've crossed a group
					 * boundary.
					 */
					if (node->aggstrategy == AGG_SORTED)
					{
						bool tuple_match;
						int match_numcols;

						int outer_grouping =
							tuple_grouping(outerslot, node->numCols-node->numNullCols,
										   input_grouping, input_has_grouping,
										   (node->numCols-node->numNullCols) > 0 ?
										   node->grpColIdx[node->numCols-node->numNullCols-2]: 0);

						Assert(outer_grouping <= input_grouping);

						if (outer_grouping == input_grouping)
							match_numcols = node->numCols - node->numNullCols;
						else
							/* ignore Grouping/GroupId column */
							match_numcols = node->numCols - node->numNullCols - 2;

						tuple_match = execTuplesMatch(firstSlot, outerslot,
													  match_numcols, node->grpColIdx,
													  aggstate->eqfunctions,
													  tmpcontext->ecxt_per_tuple_memory);

						if (!tuple_match)
						{
							/* We've crossed a group boundary. */
							/* Make a copy of the first tuple for the new group. */
							aggstate->grp_firstTuple = ExecCopySlotMemTuple(outerslot);
							aggstate->has_partial_agg = false;
							break;
						}

						if (is_middle_rollup_agg || outer_grouping < input_grouping)
						{
							/*
							 * We have a pass-through tuple. We will advance
							 * the aggregate result before passing it thru.
							 */
							if (aggstate->has_partial_agg &&
								outer_grouping == input_grouping)
							{
								has_partial_agg = true;
								tmpcontext->ecxt_outertuple = outerslot;
								call_AdvanceAggregates(aggstate, pergroup, &(aggstate->mem_manager));
							}

							passthru_ready = true;
							aggstate->has_partial_agg = has_partial_agg;
							break;
						}
					}

				}

			}
		}

		/*
		 * We found a pass-thru tuple. When this tuple appears in the middle
		 * Agg node in a ROLLUP, we can simply return outerslot. If this appears
		 * in the final Agg node in a ROLLUP, we need to finalize its
		 * aggregate value.
		 *
		 * If this pass-thru tuple needs to be aggregated into the result, at this
		 * point, it is done.
		 */
		if (passthru_ready)
		{
			if (!node->lastAgg && is_middle_rollup_agg)
				return outerslot;

			/* At this point we are done scanning the agg state because a null outerslot was produced above */
			if(aggstate->agg_done)
				return NULL;

			/*
			 * For the top-level of a rollup, we need to finalize
			 * the aggregate value. First, we reset the context,
			 * and prepare the aggregate value.
			 */
			ResetExprContext(econtext);
			initialize_aggregates(aggstate, peragg, perpassthru, &(aggstate->mem_manager));

			/* finalize the pass-through tuple */
			ResetExprContext(tmpcontext);
			tmpcontext->ecxt_outertuple = outerslot;

			call_AdvanceAggregates(aggstate, perpassthru, &(aggstate->mem_manager));
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.	(If we are not
		 * grouping, and there are no input rows at all, we will come here
		 * with an empty firstSlot ... but if not grouping, there can't be any
		 * references to non-aggregated input columns, so no problem.)
		 */
		if (passthru_ready)
			econtext->ecxt_outertuple = outerslot;
		else
			econtext->ecxt_outertuple = firstSlot;

		/*
		 * Done scanning input tuple group. Finalize each aggregate
		 * calculation, and stash results in the per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++)
		{
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate;

			if (!passthru_ready)
				pergroupstate = &pergroup[aggno];
			else
				pergroupstate = &perpassthru[aggno];

			if (peraggstate->numSortCols > 0)
			{
				if (peraggstate->numInputs == 1)
					process_ordered_aggregate_single(aggstate,
													 peraggstate,
													 pergroupstate);
				else
					process_ordered_aggregate_multi(aggstate,
													peraggstate,
													pergroupstate);

			}

			finalize_aggregate(aggstate, peraggstate, pergroupstate,
							   &aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * We obtain GROUP_ID from the input tuples when this is
		 * the middle Agg or final Agg in a ROLLUP.
		 */
		if ((is_final_rollup_agg ||
			(passthru_ready && is_middle_rollup_agg)) &&
			input_has_grouping)
			econtext->group_id =
				get_grouping_groupid(econtext->ecxt_outertuple,
									 node->grpColIdx[node->numCols-node->numNullCols-1]);
		else
			econtext->group_id = node->rollupGSTimes;

		/* Set GROUPING value */
		if ((is_final_rollup_agg ||
			 (passthru_ready && is_middle_rollup_agg)) &&
			input_has_grouping)
			econtext->grouping =
				get_grouping_groupid(econtext->ecxt_outertuple,
									 node->grpColIdx[node->numCols-node->numNullCols-2]);
		else
			econtext->grouping = node->grouping;

		/*
		 * When some grouping columns do not appear in this Agg node,
		 * We modify the input tuple before doing projection.
		 */
		if (!passthru_ready && node->numNullCols > 0)
		{
			/*
			 * For a ROLLUP query, we may need to modify the input tuple
			 * to match with the current rollup level. We initialize those
			 * arrays that are needed for this process. (Only do this once.)
			 */
			if (aggstate->num_attrs == 0 && node->numNullCols > 0)
			{
				int num_grpattrs = node->numCols;
				int attno;

				aggstate->num_attrs =
				    firstSlot->tts_tupleDescriptor->natts;
				aggstate->replValues =
					palloc0(aggstate->num_attrs * sizeof(Datum));
				aggstate->replIsnull = palloc0(aggstate->num_attrs * sizeof(bool));
				aggstate->doReplace = palloc0(aggstate->num_attrs * sizeof(bool));

				Assert(num_grpattrs >= node->numNullCols);

				for (attno=num_grpattrs - node->numNullCols; attno<num_grpattrs; attno++)
				{
					aggstate->replIsnull[node->grpColIdx[attno] - 1] = true;
					aggstate->doReplace[node->grpColIdx[attno] - 1] = true;
				}
			}

			/*
			 * Modify the input tuple when this node requires some grouping
			 * columns to be set to NULL.
			 */
			if (node->numNullCols > 0)
			{
				ExecModifyMemTuple(econtext->ecxt_outertuple,
							aggstate->replValues,
							aggstate->replIsnull,
							aggstate->doReplace
							);
			}
		}

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false))
		{
			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.
			 */
			TupleTableSlot *result;

			result = ExecProject(aggstate->ss.ps.ps_ProjInfo, NULL);

			return result;
		}
	}

	/* No more groups */
	return NULL;
}

/*
 * ExecAgg for hashed case: retrieve groups from hash table
 */
static TupleTableSlot *
agg_retrieve_hash_table(AggState *aggstate)
{
	ExprContext *econtext;
	Datum	   *aggvalues;
	bool	   *aggnulls;
	AggStatePerAgg peragg;
	AggStatePerGroup pergroup;
	TupleTableSlot *firstSlot;
	int			aggno;
	Agg		   *node = (Agg *) aggstate->ss.ps.plan;
	bool        input_has_grouping = node->inputHasGrouping;
	bool        is_final_rollup_agg =
		(node->lastAgg ||
		 (input_has_grouping && node->numNullCols == 0));

	/*
	 * get state info from node
	 */
	/* econtext is the per-output-tuple expression context */
	econtext = aggstate->ss.ps.ps_ExprContext;
	aggvalues = econtext->ecxt_aggvalues;
	aggnulls = econtext->ecxt_aggnulls;
	peragg = aggstate->peragg;
	firstSlot = aggstate->ss.ss_ScanTupleSlot;

	if (aggstate->agg_done)
		return NULL;

	/*
	 * We loop retrieving groups until we find one satisfying
	 * aggstate->ss.ps.qual
	 */
	while (!aggstate->agg_done)
	{
		HashAggEntry *entry = agg_hash_iter(aggstate);

		if (entry == NULL)
		{
			aggstate->agg_done = TRUE;

			return NULL;
		}

		/*
		 * Clear the per-output-tuple context for each group
		 *
		 * We intentionally don't use ReScanExprContext here; if any aggs have
		 * registered shutdown callbacks, they mustn't be called yet, since we
		 * might not be done with that agg.
		 */
		ResetExprContext(econtext);

		/*
		 * Store the copied first input tuple in the tuple table slot reserved
		 * for it, so that it can be used in ExecProject.
		 */
		ExecStoreMinimalTuple((MemTuple)entry->tuple_and_aggs, firstSlot, false);
		pergroup = (AggStatePerGroup)((char *)entry->tuple_and_aggs + 
					      MAXALIGN(memtuple_get_size((MemTuple)entry->tuple_and_aggs)));

		/*
		 * Finalize each aggregate calculation, and stash results in the
		 * per-output-tuple context.
		 */
		for (aggno = 0; aggno < aggstate->numaggs; aggno++)
		{
			AggStatePerAgg peraggstate = &peragg[aggno];
			AggStatePerGroup pergroupstate = &pergroup[aggno];

			Assert(peraggstate->numSortCols == 0);
			finalize_aggregate(aggstate, peraggstate, pergroupstate,
							   &aggvalues[aggno], &aggnulls[aggno]);
		}

		/*
		 * Use the representative input tuple for any references to
		 * non-aggregated input columns in the qual and tlist.
		 */
		econtext->ecxt_outertuple = firstSlot;

		if (is_final_rollup_agg && input_has_grouping)
		{
			econtext->group_id =
				get_grouping_groupid(econtext->ecxt_outertuple,
					 node->grpColIdx[node->numCols - node->numNullCols - 1]);
			econtext->grouping =
				get_grouping_groupid(econtext->ecxt_outertuple,
					 node->grpColIdx[node->numCols - node->numNullCols - 2]);
		}
		else
		{
			econtext->group_id = node->rollupGSTimes;
			econtext->grouping = node->grouping;
		}

		/*
		 * Check the qual (HAVING clause); if the group does not match, ignore
		 * it and loop back to try to process another group.
		 */
		if (ExecQual(aggstate->ss.ps.qual, econtext, false))
		{
			/*
			 * Form and return a projection tuple using the aggregate results
			 * and the representative input tuple.
			 */
			TupleTableSlot *result;

			result = ExecProject(aggstate->ss.ps.ps_ProjInfo, NULL);

			return result;
		}
	}

	/* No more groups */
	return NULL;
}

/* -----------------
 * ExecInitAgg
 *
 *	Creates the run-time information for the agg node produced by the
 *	planner and initializes its outer subtree
 * -----------------
 */
AggState *
ExecInitAgg(Agg *node, EState *estate, int eflags)
{
	AggState   *aggstate;
	AggStatePerAgg peragg;
	Plan	   *outerPlan;
	ExprContext *econtext;
	int			numaggs,
				aggno;
	ListCell   *l;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	aggstate = makeNode(AggState);
	aggstate->ss.ps.plan = (Plan *) node;
	aggstate->ss.ps.state = estate;

	aggstate->aggs = NIL;
	aggstate->numaggs = 0;
	aggstate->eqfunctions = NULL;
	aggstate->hashfunctions = NULL;
	aggstate->peragg = NULL;
	aggstate->curperagg = NULL;
	aggstate->agg_done = false;
	aggstate->has_partial_agg = false;
	aggstate->pergroup = NULL;
	aggstate->grp_firstTuple = NULL;
	aggstate->hashtable = NULL;

	/*
	 * Create expression contexts.	We need two, one for per-input-tuple
	 * processing and one for per-output-tuple processing.	We cheat a little
	 * by using ExecAssignExprContext() to build both.
	 */
	ExecAssignExprContext(estate, &aggstate->ss.ps);
	aggstate->tmpcontext = aggstate->ss.ps.ps_ExprContext;
	ExecAssignExprContext(estate, &aggstate->ss.ps);

	/*
	 * We also need a long-lived memory context for holding hashtable data
	 * structures and transition values.  NOTE: the details of what is stored
	 * in aggcontext and what is stored in the regular per-query memory
	 * context are driven by a simple decision: we want to reset the
	 * aggcontext in ExecReScanAgg to recover no-longer-wanted space.
	 */
	aggstate->aggcontext =
		AllocSetContextCreate(CurrentMemoryContext,
							  "AggContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

#define AGG_NSLOTS 3

	/*
	 * tuple table initialization
	 */
	aggstate->ss.ss_ScanTupleSlot = ExecInitExtraTupleSlot(estate);
	ExecInitResultTupleSlot(estate, &aggstate->ss.ps);
	aggstate->hashslot = ExecInitExtraTupleSlot(estate);

	/*
	 * initialize child expressions
	 *
	 * Note: ExecInitExpr finds Aggrefs for us, and also checks that no aggs
	 * contain other agg calls in their arguments.	This would make no sense
	 * under SQL semantics anyway (and it's forbidden by the spec). Because
	 * that is true, we don't need to worry about evaluating the aggs in any
	 * particular order.
	 */
	aggstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) aggstate);
	aggstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->plan.qual,
					 (PlanState *) aggstate);

	/*
	 * CDB: Offer extra info for EXPLAIN ANALYZE.
	 */
	if (estate->es_instrument)
	{
		/* Allocate string buffer. */
		aggstate->ss.ps.cdbexplainbuf = makeStringInfo();

		/* Request a callback at end of query. */
		aggstate->ss.ps.cdbexplainfun = ExecAggExplainEnd;
	}

	/*
	 * initialize child nodes
	 */
	outerPlan = outerPlan(node);
	outerPlanState(aggstate) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * initialize source tuple type.
	 */
	ExecAssignScanTypeFromOuterPlan(&aggstate->ss);

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&aggstate->ss.ps);
	ExecAssignProjectionInfo(&aggstate->ss.ps, NULL);

#if 0
	aggstate->ss.ps.ps_TupFromTlist = false;
#endif

	/*
	 * get the count of aggregates in targetlist and quals
	 */
	numaggs = aggstate->numaggs;
	Assert(numaggs == list_length(aggstate->aggs));
	if (numaggs <= 0)
	{
		/*
		 * This is not an error condition: we might be using the Agg node just
		 * to do hash-based grouping.  Even in the regular case,
		 * constant-expression simplification could optimize away all of the
		 * Aggrefs in the targetlist and qual.	So keep going, but force local
		 * copy of numaggs positive so that palloc()s below don't choke.
		 */
		numaggs = 1;
	}

	/*
	 * If we are grouping, precompute fmgr lookup data for inner loop. We need
	 * both equality and hashing functions to do it by hashing, but only
	 * equality if not hashing.
	 */
	if (node->numCols > 0)
	{
		if (node->aggstrategy == AGG_HASHED)
			execTuplesHashPrepare(node->numCols,
								  node->grpOperators,
								  &aggstate->eqfunctions,
								  &aggstate->hashfunctions);
		else
			aggstate->eqfunctions =
				execTuplesMatchPrepare(node->numCols,
									   node->grpOperators);
	}

	/*
	 * Set up aggregate-result storage in the output expr context, and also
	 * allocate my private per-agg working storage
	 */
	econtext = aggstate->ss.ps.ps_ExprContext;
	econtext->ecxt_aggvalues = (Datum *) palloc0(sizeof(Datum) * numaggs);
	econtext->ecxt_aggnulls = (bool *) palloc0(sizeof(bool) * numaggs);

	peragg = (AggStatePerAgg) palloc0(sizeof(AggStatePerAggData) * numaggs);
	aggstate->peragg = peragg;

	if (node->aggstrategy == AGG_HASHED)
	{
		aggstate->hash_needed = find_hash_columns(aggstate);
	}
	else
	{
		AggStatePerGroup pergroup;

		pergroup = (AggStatePerGroup) palloc0(sizeof(AggStatePerGroupData) * numaggs);
		aggstate->pergroup = pergroup;
	}

	/*
	 * Perform lookups of aggregate function info, and initialize the
	 * unchanging fields of the per-agg data.  We also detect duplicate
	 * aggregates (for example, "SELECT sum(x) ... HAVING sum(x) > 0"). When
	 * duplicates are detected, we only make an AggStatePerAgg struct for the
	 * first one.  The clones are simply pointed at the same result entry by
	 * giving them duplicate aggno values.
	 */
	aggno = -1;
	foreach(l, aggstate->aggs)
	{
		AggrefExprState *aggrefstate = (AggrefExprState *) lfirst(l);
		Aggref	   *aggref = (Aggref *) aggrefstate->xprstate.expr;
		AggStatePerAgg peraggstate;
		Oid			inputTypes[FUNC_MAX_ARGS];
		int			numArguments;
		int			numDirectArgs;
		int			numInputs;
		int			numSortCols;
		int			numDistinctCols;
		List	   *sortlist;
		HeapTuple	aggTuple;
		Form_pg_aggregate aggform;
		Oid			aggtranstype;
		AclResult	aclresult;
		Oid			transfn_oid = InvalidOid,
					finalfn_oid = InvalidOid;
		Expr	   *transfnexpr = NULL,
				   *finalfnexpr = NULL,
				   *prelimfnexpr = NULL;
		Datum		textInitVal;
		int			i;
		ListCell   *lc;

		/* Planner should have assigned aggregate to correct level */
		Assert(aggref->agglevelsup == 0);

		/* Look for a previous duplicate aggregate */
		for (i = 0; i <= aggno; i++)
		{
			if (equal(aggref, peragg[i].aggref) &&
				!contain_volatile_functions((Node *) aggref))
				break;
		}
		if (i <= aggno)
		{
			/* Found a match to an existing entry, so just mark it */
			aggrefstate->aggno = i;
			continue;
		}

		/* Nope, so assign a new PerAgg record */
		peraggstate = &peragg[++aggno];

		/* Mark Aggref state node with assigned index in the result array */
		aggrefstate->aggno = aggno;

		/* Begin filling in the peraggstate data */
		peraggstate->aggrefstate = aggrefstate;
		peraggstate->aggref = aggref;
		peraggstate->sortstate = NULL;

		/* Fetch the pg_aggregate row */
		aggTuple = SearchSysCache1(AGGFNOID,
								   ObjectIdGetDatum(aggref->aggfnoid));
		if (!HeapTupleIsValid(aggTuple))
			elog(ERROR, "cache lookup failed for aggregate %u",
				 aggref->aggfnoid);
		aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);

		/* Check permission to call aggregate function */
		aclresult = pg_proc_aclcheck(aggref->aggfnoid, GetUserId(),
									 ACL_EXECUTE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_PROC,
						   get_func_name(aggref->aggfnoid));

		switch (aggref->aggstage)		/* MPP */
		{
			case AGGSTAGE_NORMAL:		/* Single-stage aggregation */
				peraggstate->transfn_oid = transfn_oid = aggform->aggtransfn;
				peraggstate->finalfn_oid = finalfn_oid = aggform->aggfinalfn;
				break;

			case AGGSTAGE_PARTIAL:		/* Two-stage aggregation --
										 * preliminary stage */
				/* the perliminary stage for two-stage aggregation */
				peraggstate->transfn_oid = transfn_oid = aggform->aggtransfn;
				peraggstate->finalfn_oid = finalfn_oid = InvalidOid;
				break;

			case AGGSTAGE_INTERMEDIATE:
				peraggstate->transfn_oid = transfn_oid = aggform->aggprelimfn;
				peraggstate->finalfn_oid = finalfn_oid = InvalidOid;
				break;

			case AGGSTAGE_FINAL:		/* Two-stage aggregation - final stage */
				peraggstate->transfn_oid = transfn_oid = aggform->aggprelimfn;
				peraggstate->finalfn_oid = finalfn_oid = aggform->aggfinalfn;
				break;
		}

		peraggstate->prelimfn_oid = aggform->aggprelimfn;

		/* Check that aggregate owner has permission to call component fns */
		{
			HeapTuple	procTuple;
			Oid			aggOwner;

			procTuple = SearchSysCache(PROCOID,
									   ObjectIdGetDatum(aggref->aggfnoid),
									   0, 0, 0);
			if (!HeapTupleIsValid(procTuple))
				elog(ERROR, "cache lookup failed for function %u",
					 aggref->aggfnoid);
			aggOwner = ((Form_pg_proc) GETSTRUCT(procTuple))->proowner;
			ReleaseSysCache(procTuple);

			aclresult = pg_proc_aclcheck(transfn_oid, aggOwner,
										 ACL_EXECUTE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_PROC,
							   get_func_name(transfn_oid));
			if (OidIsValid(finalfn_oid))
			{
				aclresult = pg_proc_aclcheck(finalfn_oid, aggOwner,
											 ACL_EXECUTE);
				if (aclresult != ACLCHECK_OK)
					aclcheck_error(aclresult, ACL_KIND_PROC,
								   get_func_name(finalfn_oid));
			}
			if (OidIsValid(peraggstate->prelimfn_oid))
			{
				aclresult = pg_proc_aclcheck(peraggstate->prelimfn_oid, aggOwner,
											 ACL_EXECUTE);
				if (aclresult != ACLCHECK_OK)
					aclcheck_error(aclresult, ACL_KIND_PROC,
								   get_func_name(peraggstate->prelimfn_oid));
			}
		}

		/*
		 * Get actual datatypes of the (nominal) aggregate inputs.	These
		 * could be different from the agg's declared input types, when the
		 * agg accepts ANY or a polymorphic type.
		 */
		numArguments = get_aggregate_argtypes(aggref, inputTypes);
		peraggstate->numArguments = numArguments;

		/* Count the "direct" arguments, if any */
		numDirectArgs = list_length(aggref->aggdirectargs);

		/* Count the number of aggregated input columns */
		numInputs = list_length(aggref->args);
		peraggstate->numInputs = numInputs;

		/* Detect how many arguments to pass to the transfn */
		if (AGGKIND_IS_ORDERED_SET(aggref->aggkind))
			peraggstate->numTransInputs = numInputs;
		else
			peraggstate->numTransInputs = numArguments;

		/* Detect how many arguments to pass to the finalfn */
		if (aggform->aggfinalextra)
			peraggstate->numFinalArgs = numArguments + 1;
		else
			peraggstate->numFinalArgs = numDirectArgs + 1;

		/* resolve actual type of transition state, if polymorphic */
		aggtranstype = resolve_aggregate_transtype(aggref->aggfnoid,
												   aggform->aggtranstype,
												   inputTypes,
												   numArguments);

		/* build expression trees using actual argument & result types */
		build_aggregate_fnexprs(inputTypes,
								numArguments,
								numDirectArgs,
								peraggstate->numFinalArgs,
								aggref->aggvariadic,
								aggtranstype,
								aggref->aggtype,
								transfn_oid,
								finalfn_oid,
								peraggstate->prelimfn_oid /* prelim */ ,
								InvalidOid /* invtrans */ ,
								InvalidOid /* invprelim */ ,
								&transfnexpr,
								&finalfnexpr,
								&prelimfnexpr,
								NULL,
								NULL);

		fmgr_info(transfn_oid, &peraggstate->transfn);
		peraggstate->transfn.fn_expr = (Node *) transfnexpr;

		if (OidIsValid(finalfn_oid))
		{
			fmgr_info(finalfn_oid, &peraggstate->finalfn);
			peraggstate->finalfn.fn_expr = (Node *) finalfnexpr;
		}

		if (OidIsValid(peraggstate->prelimfn_oid))
		{
			fmgr_info(peraggstate->prelimfn_oid, &peraggstate->prelimfn);
			peraggstate->prelimfn.fn_expr = (Node *) prelimfnexpr;
		}

		get_typlenbyval(aggref->aggtype,
						&peraggstate->resulttypeLen,
						&peraggstate->resulttypeByVal);
		get_typlenbyval(aggtranstype,
						&peraggstate->transtypeLen,
						&peraggstate->transtypeByVal);

		/*
		 * initval is potentially null, so don't try to access it as a struct
		 * field. Must do it the hard way with SysCacheGetAttr.
		 */
		textInitVal = SysCacheGetAttr(AGGFNOID, aggTuple,
									  Anum_pg_aggregate_agginitval,
									  &peraggstate->initValueIsNull);

		if (peraggstate->initValueIsNull)
			peraggstate->initValue = (Datum) 0;
		else
			peraggstate->initValue = GetAggInitVal(textInitVal,
												   aggtranstype);

		/*
		 * If the transfn is strict and the initval is NULL, make sure input
		 * type and transtype are the same (or at least binary-compatible), so
		 * that it's OK to use the first aggregated input value as the initial
		 * transValue.	This should have been checked at agg definition time,
		 * but just in case...
		 */
		if (peraggstate->transfn.fn_strict && peraggstate->initValueIsNull)
		{
			if (numArguments <= numDirectArgs ||
				!IsBinaryCoercible(inputTypes[numDirectArgs], aggtranstype))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
						 errmsg("aggregate %u needs to have compatible input type and transition type %d %d %d",
								aggref->aggfnoid, inputTypes[0], aggtranstype, numArguments)));
		}

		/*
		 * Get a tupledesc corresponding to the aggregated inputs (including
		 * sort expressions) of the agg.
		 */
		peraggstate->evaldesc = ExecTypeFromTL(aggref->args, false);

		/* Create slot we're going to do argument evaluation in */
		peraggstate->evalslot = ExecInitExtraTupleSlot(estate);
		ExecSetSlotDescriptor(peraggstate->evalslot, peraggstate->evaldesc);

		/* Set up projection info for evaluation */
		peraggstate->evalproj = ExecBuildProjectionInfo(aggrefstate->args,
														aggstate->tmpcontext,
														peraggstate->evalslot,
														NULL);

		/*
		 * If we're doing either DISTINCT or ORDER BY for a plain agg, then we
		 * have a list of SortGroupClause nodes; fish out the data in them and
		 * stick them into arrays.	We ignore ORDER BY for an ordered-set agg,
		 * however; the agg's transfn and finalfn are responsible for that.
		 *
		 * Note that by construction, if there is a DISTINCT clause then the
		 * ORDER BY clause is a prefix of it (see transformDistinctClause).
		 */
		if (AGGKIND_IS_ORDERED_SET(aggref->aggkind))
		{
			sortlist = NIL;
			numSortCols = numDistinctCols = 0;
		}
		else if (aggref->aggdistinct)
		{
			sortlist = aggref->aggdistinct;
			numSortCols = numDistinctCols = list_length(sortlist);
			Assert(numSortCols >= list_length(aggref->aggorder));
		}
		else
		{
			sortlist = aggref->aggorder;
			numSortCols = list_length(sortlist);
			numDistinctCols = 0;
		}

		peraggstate->numSortCols = numSortCols;
		peraggstate->numDistinctCols = numDistinctCols;

		if (numSortCols > 0)
		{
			/*
			 * We don't implement DISTINCT or ORDER BY aggs in the HASHED case
			 * (yet)
			 */
			Assert(node->aggstrategy != AGG_HASHED);

			/* If we have only one input, we need its len/byval info. */
			if (numInputs == 1)
			{
				get_typlenbyval(inputTypes[numDirectArgs],
								&peraggstate->inputtypeLen,
								&peraggstate->inputtypeByVal);
			}
			else if (numDistinctCols > 0)
			{
				/* we will need an extra slot to store prior values */
				peraggstate->uniqslot = ExecInitExtraTupleSlot(estate);
				ExecSetSlotDescriptor(peraggstate->uniqslot,
									  peraggstate->evaldesc);
			}

			/* Extract the sort information for use later */
			peraggstate->sortColIdx =
				(AttrNumber *) palloc(numSortCols * sizeof(AttrNumber));
			peraggstate->sortOperators =
				(Oid *) palloc(numSortCols * sizeof(Oid));
			peraggstate->sortNullsFirst =
				(bool *) palloc(numSortCols * sizeof(bool));

			i = 0;
			foreach(lc, sortlist)
			{
				SortGroupClause *sortcl = (SortGroupClause *) lfirst(lc);
				TargetEntry *tle = get_sortgroupclause_tle(sortcl,
														   aggref->args);

				/* the parser should have made sure of this */
				Assert(OidIsValid(sortcl->sortop));

				peraggstate->sortColIdx[i] = tle->resno;
				peraggstate->sortOperators[i] = sortcl->sortop;
				peraggstate->sortNullsFirst[i] = sortcl->nulls_first;
				i++;
			}
			Assert(i == numSortCols);
		}

		if (aggref->aggdistinct)
		{
			Assert(numArguments > 0);

			/*
			 * We need the equal function for each DISTINCT comparison we will
			 * make.
			 */
			peraggstate->equalfns =
				(FmgrInfo *) palloc(numDistinctCols * sizeof(FmgrInfo));

			i = 0;
			foreach(lc, aggref->aggdistinct)
			{
				SortGroupClause *sortcl = (SortGroupClause *) lfirst(lc);

				fmgr_info(get_opcode(sortcl->eqop), &peraggstate->equalfns[i]);
				i++;
			}
			Assert(i == numDistinctCols);
		}

		ReleaseSysCache(aggTuple);
	}

	/* Update numaggs to match number of unique aggregates found */
	aggstate->numaggs = aggno + 1;

	/* MPP */
	aggstate->hhashtable = NULL;

	/* ROLLUP */
	aggstate->perpassthru = NULL;

	if (node->inputHasGrouping)
	{
		AggStatePerGroup perpassthru;

		perpassthru = (AggStatePerGroup) palloc0(sizeof(AggStatePerGroupData) * numaggs);
		aggstate->perpassthru = perpassthru;

	}

	aggstate->num_attrs = 0;


	/* Set the default memory manager */
	aggstate->mem_manager.alloc = cxt_alloc;
	aggstate->mem_manager.free = cxt_free;
	aggstate->mem_manager.manager = aggstate->aggcontext;
	aggstate->mem_manager.realloc_ratio = 1;

	return aggstate;
}

Datum
GetAggInitVal(Datum textInitVal, Oid transtype)
{
	Oid			typinput,
				typioparam;
	char	   *strInitVal;
	Datum		initVal;

	getTypeInputInfo(transtype, &typinput, &typioparam);
	strInitVal = TextDatumGetCString(textInitVal);
	initVal = OidInputFunctionCall(typinput, strInitVal,
								   typioparam, -1);
	pfree(strInitVal);
	return initVal;
}

/*
 * Standard API to count tuple table slots used by an execution
 * instance of an Agg node.
 *
 * GPDB precomputes tuple table size, but use of projection means
 * aggregates use a slot.  Since the count is needed earlier, we
 * than the determination of then number of different aggregate
 * call that happens during initializaiton, we just count Aggref
 * nodes.  This may be an over count (in case some aggregate
 * calls are duplicated), but shouldn't be too bad.
 */
int
ExecCountSlotsAgg(Agg *node)
{
	int			nextraslots = 0;

	nextraslots += count_extra_agg_slots((Node *) node->plan.targetlist);
	nextraslots += count_extra_agg_slots((Node *) node->plan.qual);

	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		nextraslots +			/* may be high due to duplicate Aggref nodes. */
		AGG_NSLOTS;
}

void
ExecEndAgg(AggState *node)
{
	PlanState  *outerPlan;

	ExecEagerFreeAgg(node);

	/* And ensure any agg shutdown callbacks have been called */
	ReScanExprContext(node->ss.ps.ps_ExprContext);

	/*
	 * Free both the expr contexts.
	 */
	ExecFreeExprContext(&node->ss.ps);
	node->ss.ps.ps_ExprContext = node->tmpcontext;
	ExecFreeExprContext(&node->ss.ps);

	/* clean up tuple table */
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	if (node->num_attrs > 0)
	{
		pfree(node->replValues);
		pfree(node->replIsnull);
		pfree(node->doReplace);
	}

	MemoryContextDelete(node->aggcontext);

	outerPlan = outerPlanState(node);
	ExecEndNode(outerPlan);

	EndPlanStateGpmonPkt(&node->ss.ps);
}

void
ExecReScanAgg(AggState *node, ExprContext *exprCtxt)
{
	ExprContext *econtext = node->ss.ps.ps_ExprContext;

	ExecEagerFreeAgg(node);

	/*
	 * Release all temp storage. Note that with AGG_HASHED, the hash table is
	 * allocated in a sub-context of the aggcontext. We're going to rebuild
	 * the hash table from scratch, so we need to use
	 * MemoryContextResetAndDeleteChildren() to avoid leaking the old hash
	 * table's memory context header.
	 */
	MemoryContextResetAndDeleteChildren(node->aggcontext);

	/* Re-initialize some variables */
	node->agg_done = false;

	ExecClearTuple(node->ss.ss_ScanTupleSlot);
	node->has_partial_agg = false;

	/* Forget current agg values */
	MemSet(econtext->ecxt_aggvalues, 0, sizeof(Datum) * node->numaggs);
	MemSet(econtext->ecxt_aggnulls, 0, sizeof(bool) * node->numaggs);

	if (!IS_HASHAGG(node))
	{
		/*
		 * Reset the per-group state (in particular, mark transvalues null)
		 */
		MemSet(node->pergroup, 0,
			   sizeof(AggStatePerGroupData) * node->numaggs);
	}

	if (((Agg *) node->ss.ps.plan)->inputHasGrouping)
	{
		/*
		 * Reset the per-passthru state (in particular, mark transvalues null)
		 */
		MemSet(node->perpassthru, 0,
			   sizeof(AggStatePerGroupData) * node->numaggs);
	}

	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (((PlanState *) node)->lefttree->chgParam == NULL)
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
}


/*
 * ExecAggExplainEnd
 *		Called before ExecutorEnd to finish EXPLAIN ANALYZE reporting.
 */
void
ExecAggExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	AggState   *aggstate = (AggState *) planstate;

	/* Report executor memory used by our memory context. */
	planstate->instrument->execmemused +=
		(double) MemoryContextGetPeakSpace(aggstate->aggcontext);
}	/* ExecAggExplainEnd */

/*
 * AggCheckCallContext - test if a SQL function is being called as an aggregate
 *
 * The transition and/or final functions of an aggregate may want to verify
 * that they are being called as aggregates, rather than as plain SQL
 * functions.  They should use this function to do so.  The return value
 * is nonzero if being called as an aggregate, or zero if not.  (Specific
 * nonzero values are AGG_CONTEXT_AGGREGATE or AGG_CONTEXT_WINDOW, but more
 * values could conceivably appear in future.)
 *
 * If aggcontext isn't NULL, the function also stores at *aggcontext the
 * identity of the memory context that aggregate transition values are
 * being stored in.
 */
int
AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext)
{
	if (fcinfo->context && IsA(fcinfo->context, AggState))
	{
		if (aggcontext)
			*aggcontext = ((AggState *) fcinfo->context)->aggcontext;
		return AGG_CONTEXT_AGGREGATE;
	}
	if (fcinfo->context && IsA(fcinfo->context, WindowAggState))
	{
		if (aggcontext)
			*aggcontext = ((WindowAggState *) fcinfo->context)->aggcontext;
		return AGG_CONTEXT_WINDOW;
	}

	/* this is just to prevent "uninitialized variable" warnings */
	if (aggcontext)
		*aggcontext = NULL;
	return 0;
}

/*
 * AggGetAggref - allow an aggregate support function to get its Aggref
 *
 * If the function is being called as an aggregate support function,
 * return the Aggref node for the aggregate call.  Otherwise, return NULL.
 *
 * Note that if an aggregate is being used as a window function, this will
 * return NULL.  We could provide a similar function to return the relevant
 * WindowFunc node in such cases, but it's not needed yet.
 */
Aggref *
AggGetAggref(FunctionCallInfo fcinfo)
{
	if (fcinfo->context && IsA(fcinfo->context, AggState))
	{
		AggStatePerAgg curperagg = ((AggState *) fcinfo->context)->curperagg;

		if (curperagg)
			return curperagg->aggref;
	}
	return NULL;
}

/*
 * AggGetPerTupleEContext - fetch per-input-tuple ExprContext
 *
 * This is useful in agg final functions; the econtext returned is the
 * same per-tuple context that the transfn was called in (which can
 * safely get reset during the final function).
 *
 * As above, this is currently not useful for aggs called as window functions.
 */
ExprContext *
AggGetPerTupleEContext(FunctionCallInfo fcinfo)
{
	if (fcinfo->context && IsA(fcinfo->context, AggState))
	{
		AggState   *aggstate = (AggState *) fcinfo->context;

		return aggstate->tmpcontext;
	}
	return NULL;
}

/*
 * AggGetPerAggEContext - fetch per-output-tuple ExprContext
 *
 * This is useful for aggs to register shutdown callbacks, which will ensure
 * that non-memory resources are freed.
 *
 * As above, this is currently not useful for aggs called as window functions.
 */
ExprContext *
AggGetPerAggEContext(FunctionCallInfo fcinfo)
{
	if (fcinfo->context && IsA(fcinfo->context, AggState))
	{
		AggState   *aggstate = (AggState *) fcinfo->context;

		return aggstate->ss.ps.ps_ExprContext;
	}
	return NULL;
}


/*
 * aggregate_dummy - dummy execution routine for aggregate functions
 *
 * This function is listed as the implementation (prosrc field) of pg_proc
 * entries for aggregate functions.  Its only purpose is to throw an error
 * if someone mistakenly executes such a function in the normal way.
 *
 * Perhaps someday we could assign real meaning to the prosrc field of
 * an aggregate?
 */
Datum
aggregate_dummy(PG_FUNCTION_ARGS)
{
	elog(ERROR, "aggregate function %u called as normal function",
		 fcinfo->flinfo->fn_oid);
	return (Datum) 0;			/* keep compiler quiet */
}

/* resolve actual type of transition state, if polymorphic */
Oid
resolve_polymorphic_transtype(Oid aggtranstype, Oid aggfnoid,
							  Oid *inputTypes)
{
	if (IsPolymorphicType(aggtranstype))
	{
		/* have to fetch the agg's declared input types... */
		Oid		   *declaredArgTypes;
		int			agg_nargs;

		(void) get_func_signature(aggfnoid, &declaredArgTypes, &agg_nargs);
		aggtranstype = enforce_generic_type_consistency(inputTypes,
														declaredArgTypes,
														agg_nargs,
														aggtranstype,
														false);
		pfree(declaredArgTypes);
	}
	return aggtranstype;
}

/*
 * tuple_grouping - return the GROUPING value for an input tuple.
 *
 * This is used for a ROLLUP.
 */
int64
tuple_grouping(TupleTableSlot *outerslot, int numGroupCols,
			   int input_grouping, bool input_has_grouping,
			   int grpingIdx)
{
	Datum		grping_datum;
	int64		grping;
	bool		isnull;

	/* Simple return 0 if input_grouping is 0. */
	if (input_grouping == 0)
		return 0;

	grping_datum = slot_getattr(outerslot, grpingIdx, &isnull);

	Assert(!isnull);

	grping = DatumGetInt64(grping_datum);

	return grping;
}

/*
 * get_grouping_groupid() -- return either grouping or group_id
 * as given in 'grping_attno'.
 */
uint64
get_grouping_groupid(TupleTableSlot *slot, int grping_attno)
{
	bool		isnull;
	uint64		grouping;

	/* Obtain grouping or group_id from input */
	Datum		grping_datum = slot_getattr(slot,
											grping_attno,
											&isnull);

	Assert(!isnull);
	grouping = DatumGetInt64(grping_datum);

	return grouping;
}

/*
 * Subroutines for ExecCountSlotsAgg.
 */
int
count_extra_agg_slots(Node *node)
{
	int			count = 0;

	count_extra_agg_slots_walker(node, &count);
	return count;
}

bool
count_extra_agg_slots_walker(Node *node, int *count)
{
	if (node == NULL)
		return false;

	if (IsA(node, Aggref))
	{
		(*count)++;
	}

	return expression_tree_walker(node, count_extra_agg_slots_walker, (void *) count);
}

void
ExecEagerFreeAgg(AggState *node)
{
	/* Close any open tuplesorts */
	for (int aggno = 0; aggno < node->numaggs; aggno++)
	{
		AggStatePerAgg peraggstate = &node->peragg[aggno];

		if (!peraggstate->sortstate)
		{
			continue;
		}

		tuplesort_end(peraggstate->sortstate);

		peraggstate->sortstate = NULL;
	}

	if (IS_HASHAGG(node))
	{
		destroy_agg_hash_table(node);

		/**
		 * Clean out the tuple descriptor.
		 */
		if (node->hashslot
			&& node->hashslot->tts_tupleDescriptor)
		{
			ReleaseTupleDesc(node->hashslot->tts_tupleDescriptor);
			node->hashslot->tts_tupleDescriptor = NULL;
		}
	}

	/* We don't need to ReScanExprContext here; ExecReScan already did it */

	/* Release first tuple of group, if we have made a copy. */
	if (node->grp_firstTuple != NULL)
	{
		pfree(node->grp_firstTuple);
		node->grp_firstTuple = NULL;
	}
}
