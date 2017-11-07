/*-------------------------------------------------------------------------
 *
 * nodeAgg.h
 *	  prototypes for nodeAgg.c
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeAgg.h,v 1.29 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEAGG_H
#define NODEAGG_H

#include "fmgr.h"
#include "executor/tuptable.h"
#include "nodes/execnodes.h"
#include "nodes/primnodes.h"

extern int	ExecCountSlotsAgg(Agg *node);
extern AggState *ExecInitAgg(Agg *node, EState *estate, int eflags);
extern struct TupleTableSlot *ExecAgg(AggState *node);
extern void ExecEndAgg(AggState *node);
extern void ExecReScanAgg(AggState *node, ExprContext *exprCtxt);

extern Size hash_agg_entry_size(int numAggs);

extern Datum aggregate_dummy(PG_FUNCTION_ARGS);

/* MPP needs to see these in execHHashAgg.c */

/*
 * AggStatePerAggData - per-aggregate working state for the Agg scan
 */
typedef struct AggStatePerAggData
{
	/*
	 * These values are set up during ExecInitAgg() and do not change
	 * thereafter:
	 */

	/* Links to Aggref expr and state nodes this working state is for */
	AggrefExprState *aggrefstate;
	Aggref	   *aggref;

	/* Links to PercentileExpr expr and state nodes this working state is for */
	PercentileExprState *percstate;
	PercentileExpr	   *perc;

	/*
	 * number of input arguments for aggregate.  It's usually length of
	 * the argument list supplied in SQL, but in case of PercentileExpr,
	 * it includes sort list, pcExpr and tcExpr.
	 */
	int			numArguments;
	
	/* number of inputs including ORDER BY expressions */
	int			numInputs;
	
	/* Oids of transfer functions */
	Oid			transfn_oid;
	Oid         prelimfn_oid;
	Oid			finalfn_oid;	/* may be InvalidOid */

	/*
	 * fmgr lookup data for transfer functions --- only valid when
	 * corresponding oid is not InvalidOid.  Note in particular that fn_strict
	 * flags are kept here.
	 */
	FmgrInfo	transfn;
	FmgrInfo    prelimfn;
	FmgrInfo	finalfn;
	
	/* --- Ordered Aggregate Additions ( --- */
	
	/* number of sorting columns */
	int			numSortCols;
	
	/* deconstructed sorting information (arrays of length numSortCols) */
	AttrNumber *sortColIdx;
	Oid		   *sortOperators;
	bool	   *sortNullsFirst;

	/* --- Ordered Aggregate Additions ) --- */

	/*
	 * fmgr lookup data for input type's equality operator --- only set/used
	 * when aggregate has DISTINCT flag. (In PG 9, equalfns is a vector.)
	 */
	FmgrInfo	equalfn;

	/*
	 * initial value from pg_aggregate entry
	 */
	Datum		initValue;
	bool		initValueIsNull;

	/*
	 * We need the len and byval info for the agg's input, result, and
	 * transition data types in order to know how to copy/delete values.
	 */
	int16		inputtypeLen,
				resulttypeLen,
				transtypeLen;
	bool		inputtypeByVal,
				resulttypeByVal,
				transtypeByVal;

	/*
	 * Stuff for evaluation of inputs.	We used to just use ExecEvalExpr, but
	 * with the addition of ORDER BY we now need at least a slot for passing
	 * data to the sort object, which requires a tupledesc, so we might as
	 * well go whole hog and use ExecProject too.
	 */
	TupleDesc	evaldesc;		/* descriptor of input tuples */
	ProjectionInfo *evalproj;	/* projection machinery */
	
	/*
	 * Slot for holding the evaluated input arguments.  This is set up
	 * during ExecInitAgg() and then used for each input row.
	 */
	TupleTableSlot *evalslot;	/* current input tuple */
	
	/*
	 * These values are working state that is initialized at the start of an
	 * input tuple group and updated for each input tuple.
	 *
	 * For a simple (non DISTINCT) aggregate, we just feed the input values
	 * straight to the transition function.  If it's DISTINCT, we pass the
	 * input values into a Tuplesort object; then at completion of the input
	 * tuple group, we scan the sorted values, eliminate duplicates, and run
	 * the transition function on the rest.
	 */

	void *sortstate;	/* sort object, if DISTINCT or ORDER BY */
} AggStatePerAggData;

/*
 * AggStatePerGroupData - per-aggregate-per-group working state
 *
 * These values are working state that is initialized at the start of
 * an input tuple group and updated for each input tuple.
 *
 * In AGG_PLAIN and AGG_SORTED modes, we have a single array of these
 * structs (pointed to by aggstate->pergroup); we re-use the array for
 * each input group, if it's AGG_SORTED mode.  In AGG_HASHED mode, the
 * hash table contains an array of these structs for each tuple group.
 *
 * Logically, the sortstate field belongs in this struct, but we do not
 * keep it here for space reasons: we don't support DISTINCT aggregates
 * in AGG_HASHED mode, so there's no reason to use up a pointer field
 * in every entry of the hashtable.
 */
typedef struct AggStatePerGroupData
{
	Datum		transValue;		/* current transition value */
	bool		transValueIsNull;

	bool		noTransValue;	/* true if transValue not set yet */

	/*
	 * Note: noTransValue initially has the same value as transValueIsNull,
	 * and if true both are cleared to false at the same time.	They are not
	 * the same though: if transfn later returns a NULL, we want to keep that
	 * NULL and not auto-replace it with a later input value. Only the first
	 * non-NULL input will be auto-substituted.
	 */
} AggStatePerGroupData;

extern void 
initialize_aggregates(AggState *aggstate,
					  AggStatePerAgg peragg,
					  AggStatePerGroup pergroup,
					  MemoryManagerContainer *mem_manager);
extern void 
advance_aggregates(AggState *aggstate, AggStatePerGroup pergroup,
				   MemoryManagerContainer *mem_manager);

extern Oid resolve_polymorphic_transtype(Oid aggtranstype, Oid aggfnoid,
										 Oid *inputTypes);

extern Datum GetAggInitVal(Datum textInitVal, Oid transtype);

extern Datum invoke_agg_trans_func(FmgrInfo *transfn, int numargs, 
								   Datum transValue, bool *noTransvalue, 
								   bool *transValueIsNull, bool transtypeByVal,
								   int16 transtypeLen,
								   FunctionCallInfoData *fcinfo, void *funcctx,
								   MemoryContext tuplecontext,
								   MemoryManagerContainer *mem_manager);

extern Datum datumCopyWithMemManager(Datum oldvalue, Datum value, bool typByVal, int typLen,
									 MemoryManagerContainer *mem_manager);
extern void ExecEagerFreeAgg(AggState *aggstate);

extern List *combineAggrefArgs(Aggref *aggref, List **sort_clauses);
extern List *combinePercentileArgs(PercentileExpr *p);

#endif   /* NODEAGG_H */
