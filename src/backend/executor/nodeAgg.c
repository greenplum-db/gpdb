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
 *		 result = finalfunc(transvalue)
 *
 *	  If a finalfunc is not supplied then the result is just the ending
 *	  value of transvalue.
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
 *	  In Greenplum 4.2.2, we add PercentileExpr support along with Aggref.
 *	  It is used to implement inverse distribution function support, namely
 *	  percentile_cont, percentile_disc and median.  The semantics for them
 *	  is almost same as Aggref, where the aggregate process is handled by
 *	  an individual function and the expression node only returns a pre-computed
 *	  result.  PercentileExpr is used in Agg node because we cannot change
 *	  the catalog in this release, and it may be removed and integrated to
 *	  standard Aggref itself.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeAgg.c,v 1.160 2008/08/25 22:42:32 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
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
#include "parser/parse_oper.h"
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
		invoke_agg_trans_func(&(peraggstate->transfn), 
							  peraggstate->numArguments,
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
invoke_agg_trans_func(FmgrInfo *transfn, int numArguments, Datum transValue,
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
		for (i = 1; i <= numArguments; i++)
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

	/*
	 * OK to call the transition function
	 */
	InitFunctionCallInfoData(*fcinfo, transfn,
							 numArguments + 1,
							 (void *) funcctx, NULL);
	fcinfo->arg[0] = transValue;
	fcinfo->argnull[0] = *transValueIsNull;

	newVal = FunctionCallInvoke(fcinfo);

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
		int			nargs;
		Aggref	   *aggref = peraggstate->aggref;
		PercentileExpr *perc = peraggstate->perc;
		int			i;
		TupleTableSlot *slot;

		/* Skip anything FILTERed out */
		if (filter)
		{
			bool		isnull;
			Datum		res;

			res = ExecEvalExprSwitchContext(filter, aggstate->tmpcontext,
											&isnull, NULL);
			if (isnull || !DatumGetBool(res))
				continue;
		}

		if (aggref)
			nargs = list_length(aggref->args);
		else
		{
			Assert (perc);
			nargs = list_length(perc->args);
		}

		/* Evaluate the current input expressions for this aggregate */
		slot = ExecProject(peraggstate->evalproj, NULL);
		slot_getallattrs(slot);

		if (peraggstate->numSortCols > 0)
		{
			/* DISTINCT and/or ORDER BY case */
			Assert(slot->PRIVATE_tts_nvalid == peraggstate->numInputs);
			Assert(!perc);

			/*
			 * If the transfn is strict, we want to check for nullity before
			 * storing the row in the sorter, to save space if there are a lot
			 * of nulls.  Note that we must only check numArguments columns,
			 * not numInputs, since nullity in columns used only for sorting
			 * is not relevant here.
			 */
			if (peraggstate->transfn.fn_strict)
			{
				for (i = 0; i < nargs; i++)
				{
					value = slot_getattr(slot, i+1, &isnull);
					
					if (isnull)
						break; /* arg loop */
				}
				if (i < nargs)
					continue; /* aggno loop */
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
			Assert(slot->PRIVATE_tts_nvalid >= nargs);
			if (aggref)
			{
				for (i = 0; i < nargs; i++)
				{
					fcinfo.arg[i + 1] = slot_getattr(slot, i+1, &isnull);
					fcinfo.argnull[i + 1] = isnull;
				}

			}
			else
			{
				/*
				 * In case of percentile functions, put everything into
				 * fcinfo's argument since there should be the required
				 * attributes as arguments in the tuple.
				 */
				int		natts;

				Assert(perc);
				natts = slot->tts_tupleDescriptor->natts;
				for (i = 0; i < natts; i++)
				{
					fcinfo.arg[i + 1] = slot_getattr(slot, i + 1, &isnull);
					fcinfo.argnull[i + 1] = isnull;
				}
			}
			advance_transition_function(aggstate, peraggstate, pergroupstate,
										&fcinfo, mem_manager);
		}
	} /* aggno loop */
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
	bool		isDistinct = peraggstate->aggref->aggdistinct;
	Datum	   *newVal;
	bool	   *isNull;
	FunctionCallInfoData fcinfo;

	Assert(peraggstate->numInputs == 1);

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
			  DatumGetBool(FunctionCall2(&peraggstate->equalfn,
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
 * Run the transition function for an ORDER BY aggregate with more than one
 * input.  In PG DISTINCT aggregates may also have multiple columns, but in
 * GPDB, only ORDER BY aggregates do.  This is called after we have completed
 * entering all the input values into the sort object.  We complete the
 * sort, read out the values in sorted order, and run the transition
 * function on each value.
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
	TupleTableSlot *slot = peraggstate->evalslot;
	int			numArguments = peraggstate->numArguments;
	int			i;

	tuplesort_performsort(peraggstate->sortstate);

	ExecClearTuple(slot);

	while (tuplesort_gettupleslot(peraggstate->sortstate, true, slot))
	{
		/*
		 * Extract the first numArguments as datums to pass to the transfn.
		 * (This will help execTuplesMatch too, so do it immediately.)
		 */
		slot_getsomeattrs(slot, numArguments);

		/* Load values into fcinfo */
		/* Start from 1, since the 0th arg will be the transition value */
		for (i = 0; i < numArguments; i++)
		{
			fcinfo.arg[i + 1] = slot_get_values(slot)[i];
			fcinfo.argnull[i + 1] = slot_get_isnull(slot)[i];
		}

		advance_transition_function(aggstate, peraggstate, pergroupstate,
									&fcinfo, &(aggstate->mem_manager));

		/* Reset context each time */
		MemoryContextReset(workcontext);

		ExecClearTuple(slot);
	}

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
	MemoryContext oldContext;

	oldContext = MemoryContextSwitchTo(aggstate->ss.ps.ps_ExprContext->ecxt_per_tuple_memory);

	/*
	 * Apply the agg's finalfn if one is provided, else return transValue.
	 */
	if (OidIsValid(peraggstate->finalfn_oid))
	{
		FunctionCallInfoData fcinfo;

		InitFunctionCallInfoData(fcinfo, &(peraggstate->finalfn), 1,
								 (void *) aggstate, NULL);
		fcinfo.arg[0] = pergroupstate->transValue;
		fcinfo.argnull[0] = pergroupstate->transValueIsNull;
		if (fcinfo.flinfo->fn_strict && pergroupstate->transValueIsNull)
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
	ProjectionInfo *projInfo;
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
	projInfo = aggstate->ss.ps.ps_ProjInfo;
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

				Gpmon_Incr_Rows_In(GpmonPktFromAggState(aggstate));
				CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);
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
			 * Clear the per-output-tuple context for each group
			 */
			ResetExprContext(econtext);

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

					Gpmon_Incr_Rows_In(GpmonPktFromAggState(aggstate));
					CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);
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
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */
			Gpmon_Incr_Rows_Out(GpmonPktFromAggState(aggstate));
			CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);

			return ExecProject(projInfo, NULL);
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
	ProjectionInfo *projInfo;
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
	projInfo = aggstate->ss.ps.ps_ProjInfo;
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

			Assert(!peraggstate->aggref->aggdistinct);
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
			 * and the representative input tuple.	Note we do not support
			 * aggregates returning sets ...
			 */
			Gpmon_Incr_Rows_Out(GpmonPktFromAggState(aggstate));
			CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);
			return ExecProject(projInfo, NULL);
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

	/*
	 * get the count of aggregates in targetlist and quals
	 */
	numaggs = aggstate->numaggs;
	Assert(numaggs == list_length(aggstate->aggs) + list_length(aggstate->percs));
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
		List	   *inputTargets = NIL;
		List	   *inputSortClauses = NIL;
		Oid		   *inputTypes = NULL;
		int			numInputs;
		int			numArguments;
		int			numSortCols;
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

		/* Fill in the peraggstate data */
		peraggstate->aggrefstate = aggrefstate;
		peraggstate->aggref = aggref;
		numArguments = list_length(aggref->args);
		peraggstate->numArguments = numArguments;

		/*
		 * Use these information from ExecInitExpr for per agg info.
		 */
		inputTargets = aggrefstate->inputTargets;
		inputSortClauses = aggrefstate->inputSortClauses;
		numInputs = list_length(inputTargets);
		numSortCols = list_length(inputSortClauses);

		peraggstate->numSortCols = numSortCols;
		peraggstate->numInputs = numInputs;
		
		/* MPP has some restrictions. */
		Assert(!(aggref->aggdistinct && aggref->aggorder));
		Assert(numArguments == 1 || !aggref->aggdistinct);

		/*
		 * Get actual datatypes of the inputs.	These could be different from
		 * the agg's declared input types, when the agg accepts ANY or a
		 * polymorphic type.
		 *
		 * The result will have argument types at 0 through numArguments-1 and
		 * sort key types mixed in or at numArguments through numInputs.
		 */
		inputTypes = (Oid *) palloc0(sizeof(Oid) * (numInputs));
		i = 0;
		foreach(lc, inputTargets)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);

			inputTypes[i++] = exprType((Node *) tle->expr);
		}

		aggTuple = SearchSysCache(AGGFNOID,
								  ObjectIdGetDatum(aggref->aggfnoid),
								  0, 0, 0);
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

		/* check if the transition type is polymorphic and if so resolve it */
		aggtranstype = resolve_polymorphic_transtype(aggform->aggtranstype,
													 aggref->aggfnoid,
													 inputTypes);

		/* build expression trees using actual argument & result types */
		build_aggregate_fnexprs(inputTypes,
								numArguments,
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
		 * that it's OK to use the first input value as the initial
		 * transValue.  This should have been checked at agg definition time,
		 * but just in case...
		 */
		if (peraggstate->transfn.fn_strict && peraggstate->initValueIsNull)
		{
			if (numArguments < 1 ||
				!IsBinaryCoercible(inputTypes[0], aggtranstype))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
						 errmsg("aggregate %u needs to have compatible input type and transition type",
								aggref->aggfnoid)));
		}

		/*
		 * Get a tupledesc corresponding to the inputs (including sort
		 * expressions) of the agg.
		 */
		peraggstate->evaldesc = ExecTypeFromTL(inputTargets, false);

		/* Create slot we're going to do argument evaluation in */
		peraggstate->evalslot = ExecInitExtraTupleSlot(estate);
		ExecSetSlotDescriptor(peraggstate->evalslot, peraggstate->evaldesc);

		/* Set up projection info for evaluation */
		peraggstate->evalproj = ExecBuildProjectionInfo(aggrefstate->args,
														aggstate->tmpcontext,
														peraggstate->evalslot,
														NULL);

		/*
		 * If we're doing either DISTINCT or ORDER BY, then we have a list of
		 * SortGroupClause nodes; fish out the data in them and stick them
		 * into arrays.
		 *
		 * Note that by construction, if there is a DISTINCT clause then the
		 * ORDER BY clause is a prefix of it (see transformDistinctClause).
		 */
		if (aggref->aggdistinct)
		{
			TargetEntry *tle;
			SortClause *sc;
			Oid			eq_function;

			/*
			 * GPDB 4 doesh't implement DISTINCT aggs for aggs having more
			 * than than one argument, nor does it allow an ordered aggregate
			 * to specify distinct, but PG 9 does.  The SQL standard allows
			 * the one-arg-for-DISTINCT restriction, but we really we ought to
			 * implement it the way PG 9 does eventually.
			 *
			 * For now we use the scalar equalfn field of AggStatePerAggData
			 * for DQAs instead of treating DQAs more generally.
			 */
			if (numArguments != 1)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("DISTINCT is supported only for single-argument aggregates")));

			/*
			 * Look up the sorting and comparison operators to use.  XXX it's
			 * pretty bletcherous to be making this sort of semantic decision
			 * in the executor.  Probably the parser should decide this and
			 * record it in the Aggref node ... or at latest, do it in the
			 * planner.
			 */
			eq_function = equality_oper_funcid(inputTypes[0]);
			fmgr_info(eq_function, &(peraggstate->equalfn));

			tle = (TargetEntry *) linitial(inputTargets);
			tle->ressortgroupref = 1;

			sc = makeNode(SortClause);
			sc->tleSortGroupRef = tle->ressortgroupref;
			sc->sortop = ordering_oper_opid(inputTypes[0]);

			sortlist = list_make1(sc);
			numSortCols = 1;
		}
		else if (aggref->aggorder)
		{
			sortlist = aggref->aggorder->sortClause;
			numSortCols = list_length(sortlist);
		}
		else
		{
			sortlist = NULL;
			numSortCols = 0;
		}


		peraggstate->numSortCols = numSortCols;

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
				get_typlenbyval(inputTypes[0],
								&peraggstate->inputtypeLen,
								&peraggstate->inputtypeByVal);
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
				SortClause *sortcl = (SortClause *) lfirst(lc);
				TargetEntry *tle = get_sortgroupclause_tle(sortcl,
														   inputTargets);

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
			Oid			eqfunc;

			Assert(numArguments == 1);
			Assert(numSortCols == 1);

			/*
			 * We need the equal function for the DISTINCT comparison we will
			 * make.
			 */
			eqfunc = equality_oper_funcid(inputTypes[0]);
			fmgr_info(eqfunc, &peraggstate->equalfn);
		}

		ReleaseSysCache(aggTuple);
	}

	/*
	 * Process percentile expressions.  These are treated separately from
	 * Aggref expressions at the moment as we cannot change the catalog, but
	 * this will be incorporated into the existing Aggref architecture when we
	 * can change the catalog.  The operation for percentile functions is very
	 * similar to the Aggref operation except that there is no function oid
	 * for transition function.  We manually manipulate FmgrInfo without the
	 * oid. In case the Agg handles PercentileExpr, there shouldn't be Aggref
	 * in conjunction with PercentileExpr in the target list (and havingQual),
	 * or vice versa, from the current design of percentile functions.
	 * However, we don't assert anything to keep that assumption, for the
	 * later extensibility.
	 */
	foreach(l, aggstate->percs)
	{
		PercentileExprState *percstate = (PercentileExprState *) lfirst(l);
		PercentileExpr *perc = (PercentileExpr *) percstate->xprstate.expr;
		AggStatePerAgg peraggstate;
		FmgrInfo   *transfn;
		int			numArguments;
		int			i;
		Oid			trans_argtypes[FUNC_MAX_ARGS];
		ListCell   *lc;
		Expr	   *dummy_expr;

		/* Look for a previous duplicate aggregate */
		for (i = 0; i <= aggno; i++)
		{
			/*
			 * In practice, percentile expression doesn't contain volatile
			 * functions since everything is evaluated and becomes Var during
			 * the preprocess such as ordering operations. However, adding a
			 * check for volatile may be robust and consistent with Aggref
			 * initialization.
			 */
			if (equal(perc, peragg[i].perc) &&
				!contain_volatile_functions((Node *) perc))
				break;
		}
		if (i <= aggno)
		{
			/* Found a match to an existing entry, so just mark it */
			percstate->aggno = i;
			continue;
		}

		/* Nope, so assign a new PerAgg record */
		peraggstate = &peragg[++aggno];

		/* Mark Aggref state node with assigned index in the result array */
		percstate->aggno = aggno;

		/* Fill in the peraggstate data */
		peraggstate->percstate = percstate;
		peraggstate->perc = perc;
		/*
		 * numArguments = arg + ORDER BY + pc + tc
		 * See notes on percentile_cont_trans() and ExecInitExpr() for
		 * PercentileExpr.
		 */
		numArguments = list_length(perc->args) + list_length(perc->sortClause) + 2;
		peraggstate->numArguments = numArguments;

		/*
		 * Set up transfn.  In general, we should use fmgr_info, but we don't
		 * have the catalog function (thus no oid for functions) due to the
		 * difficulity of changing the catalog at the moment.  This should
		 * be cleaned when we can change the catalog.
		 */
		transfn = &peraggstate->transfn;
		transfn->fn_nargs = list_length(perc->args) + 1;
		transfn->fn_strict = false;
		transfn->fn_retset = false;
		transfn->fn_mcxt = CurrentMemoryContext;
		transfn->fn_addr = perc->perckind == PERC_DISC ?
			percentile_disc_trans : percentile_cont_trans;
		transfn->fn_oid = InvalidOid;

		/*
		 * trans type is the same as result type, as they don't have final
		 * func.
		 */
		trans_argtypes[0] = perc->perctype;
		i = 1;

		/*
		 * Literal arguments.
		 */
		foreach(lc, perc->args)
		{
			Node	   *arg = lfirst(lc);

			trans_argtypes[i++] = exprType(arg);
		}

		/*
		 * ORDER BY arguments.
		 */
		foreach(lc, perc->sortTargets)
		{
			TargetEntry *tle = lfirst(lc);

			trans_argtypes[i++] = exprType((Node *) tle->expr);
		}

		/*
		 * Peer count and total count.
		 */
		trans_argtypes[i++] = INT8OID;
		trans_argtypes[i++] = INT8OID;

		/*
		 * Build FuncExpr for the transition function.
		 */
		build_aggregate_fnexprs(trans_argtypes,
								i,
								perc->perctype,
								perc->perctype,
								InvalidOid,
								InvalidOid,
								InvalidOid,
								InvalidOid,
								InvalidOid,
								(Expr **) &transfn->fn_expr,
								&dummy_expr, NULL, NULL, NULL);

		get_typlenbyval(perc->perctype,
						&peraggstate->resulttypeLen,
						&peraggstate->resulttypeByVal);
		get_typlenbyval(perc->perctype,
						&peraggstate->transtypeLen,
						&peraggstate->transtypeByVal);

		/*
		 * Hard code for the known information.
		 */
		peraggstate->initValueIsNull = true;
		peraggstate->initValue = (Datum) 0;

		peraggstate->finalfn_oid = InvalidOid;
		peraggstate->prelimfn_oid = InvalidOid;

		/*
		 * Get a tupledesc corresponding to the inputs (including sort
		 * expressions) of the agg.
		 */
		peraggstate->evaldesc = ExecTypeFromTL(percstate->tlist, false);

		/* Create slot we're going to do argument evaluation in */
		peraggstate->evalslot = ExecInitExtraTupleSlot(estate);
		ExecSetSlotDescriptor(peraggstate->evalslot, peraggstate->evaldesc);

		/* Set up projection info for evaluation */
		peraggstate->evalproj = ExecBuildProjectionInfo(percstate->args,
														aggstate->tmpcontext,
														peraggstate->evalslot,
														NULL);
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

	initGpmonPktForAgg((Plan *) node, &aggstate->ss.ps.gpmon_pkt, estate);

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

/*
 * TODO: remove the macro after we upgrade GPDB to PG8.4 due to WindowAggState
 *		 is not supported yet.
 */
/* GPDB_84_MERGE_FIXME */
#if 0
	if (fcinfo->context && IsA(fcinfo->context, WindowAggState))
	{
		if (aggcontext)
			*aggcontext = ((WindowAggState *) fcinfo->context)->wincontext;
		return AGG_CONTEXT_WINDOW;
	}
#endif

	/* this is just to prevent "uninitialized variable" warnings */
	if (aggcontext)
		*aggcontext = NULL;
	return 0;
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

void
initGpmonPktForAgg(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, Agg));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate);
}

/*
 * Combine the argument and sortkey expressions of an Aggref
 * node into a single target list (of TargetEntry*) and, if
 * needed, an associated sort key list (of SortClause*).
 *
 * The explicit result is a  palloc'd target list incorporating
 * the underlying expressions by reference.  (Everything but
 * the expressions is newly allocated.)
 *
 * The implicit result, if requested by passing a non-null
 * pointer in sort_clauses, is a palloc'd sort key list.
 */
List *
combineAggrefArgs(Aggref *aggref, List **sort_clauses)
{
	ListCell   *lc;
	TargetEntry *tle;
	List	   *inputTargets = NIL;
	List	   *inputSorts = NIL;
	int			i = 0;

	/* In GPDB, can't have it both ways. */
	Assert(!aggref->aggdistinct || aggref->aggorder == NULL);

	/* Target list for cataloged aggregate arguments. */
	foreach(lc, aggref->args)
	{
		TargetEntry *tle = makeNode(TargetEntry);

		tle->expr = (Expr *) lfirst(lc);
		tle->resno = ++i;
		inputTargets = lappend(inputTargets, tle);
	}

	if (aggref->aggorder != NULL)
	{
		/* Add targets and sort clauses for call supplied ordering. */
		inputSorts = aggref->aggorder->sortClause;
		if (sort_clauses != NULL)
			inputSorts = (List *) copyObject(inputSorts);

		foreach(lc, inputSorts)
		{
			SortClause *sc = (SortClause *) lfirst(lc);
			TargetEntry *newtle;

			tle = get_sortgroupclause_tle(sc, aggref->aggorder->sortTargets);

			/*
			 * XXX Is it worth looking for tle->expr in the tlist so far to
			 * avoid copy?
			 */
			newtle = makeNode(TargetEntry);
			newtle->expr = tle->expr;	/* by reference */
			newtle->resno = ++i;
			newtle->resname = tle->resname ? pstrdup(tle->resname) : NULL;
			newtle->ressortgroupref = tle->ressortgroupref;

			inputTargets = lappend(inputTargets, newtle);
		}
	}
	else if (aggref->aggdistinct)
	{
		SortClause *sc;

		/* In GPDB, DISTINCT implies single argument. */
		Assert(list_length(inputTargets) == 1);


		/* Add targets and sort clauses for implied DISTINCT ordering. */
		tle = (TargetEntry *) linitial(inputTargets);
		tle->ressortgroupref = 1;

		if (sort_clauses != NULL)
		{
			sc = makeNode(SortClause);
			sc->tleSortGroupRef = tle->ressortgroupref;
			inputSorts = list_make1(sc);
		}
	}

	if (sort_clauses != NULL)
		*sort_clauses = inputSorts;

	return inputTargets;
}

/*
 * Combine the argument and ordering expression with the peer count
 * and the total count, to create the TupleTableSlot for this
 * expression.  This is similar to ordered aggregate Aggref,
 * but the difference is that PercentileExpr will accept those
 * additional values as the arguments to the transition function.
 */
List *
combinePercentileArgs(PercentileExpr *p)
{
	List	   *tlist;
	ListCell   *l;
	AttrNumber	resno;
	TargetEntry *tle;

	tlist = NIL;
	resno = 1;
	foreach(l, p->args)
	{
		Expr	   *arg = lfirst(l);

		tle = makeTargetEntry((Expr *) arg,
							  resno++,
							  NULL,
							  false);
		tlist = lappend(tlist, tle);
	}

	/*
	 * Extract ordering expressions from sortTargets.
	 */
	foreach(l, p->sortClause)
	{
		SortClause *sc = lfirst(l);
		TargetEntry *sc_tle;

		sc_tle = get_sortgroupclause_tle(sc, p->sortTargets);
		tle = flatCopyTargetEntry(sc_tle);
		tle->resno = resno++;

		tlist = lappend(tlist, tle);
	}

	/*
	 * peer count expresssion.
	 */
	Assert(p->pcExpr);
	tle = makeTargetEntry((Expr *) p->pcExpr,
						  resno++,
						  "peer_count",
						  false);
	tlist = lappend(tlist, tle);

	/*
	 * total count expresssion.
	 */
	Assert(p->tcExpr);
	tle = makeTargetEntry((Expr *) p->tcExpr,
						  resno++,
						  "total_count",
						  false);
	tlist = lappend(tlist, tle);

	return tlist;
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
	else if (IsA(node, PercentileExpr))
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

	/* Release first tuple of group, if we have made a copy. */
	if (node->grp_firstTuple != NULL)
	{
		pfree(node->grp_firstTuple);
		node->grp_firstTuple = NULL;
	}
}
