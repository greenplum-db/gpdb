/*-------------------------------------------------------------------------
 *
 * executor.h
 *	  support for the POSTGRES executor module
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/executor.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "executor/execdesc.h"
#include "nodes/parsenodes.h"

#include "cdb/cdbdef.h"                 /* CdbVisitOpt */

struct ChunkTransportState;             /* #include "cdb/cdbinterconnect.h" */

/*
 * The "eflags" argument to ExecutorStart and the various ExecInitNode
 * routines is a bitwise OR of the following flag bits, which tell the
 * called plan node what to expect.  Note that the flags will get modified
 * as they are passed down the plan tree, since an upper node may require
 * functionality in its subnode not demanded of the plan as a whole
 * (example: MergeJoin requires mark/restore capability in its inner input),
 * or an upper node may shield its input from some functionality requirement
 * (example: Materialize shields its input from needing to do backward scan).
 *
 * EXPLAIN_ONLY indicates that the plan tree is being initialized just so
 * EXPLAIN can print it out; it will not be run.  Hence, no side-effects
 * of startup should occur.  However, error checks (such as permission checks)
 * should be performed.
 *
 * REWIND indicates that the plan node should expect to be rescanned. This
 * implies delaying freeing up resources when EagerFree is called. XXX
 *
 * BACKWARD indicates that the plan node must respect the es_direction flag.
 * When this is not passed, the plan node will only be run forwards.
 *
 * MARK indicates that the plan node must support Mark/Restore calls.
 * When this is not passed, no Mark/Restore will occur.
 *
 * SKIP_TRIGGERS tells ExecutorStart/ExecutorFinish to skip calling
 * AfterTriggerBeginQuery/AfterTriggerEndQuery.  This does not necessarily
 * mean that the plan can't queue any AFTER triggers; just that the caller
 * is responsible for there being a trigger context for them to be queued in.
 *
 * WITH/WITHOUT_OIDS tell the executor to emit tuples with or without space
 * for OIDs, respectively.  These are currently used only for CREATE TABLE AS.
 * If neither is set, the plan may or may not produce tuples including OIDs.
 */
#define EXEC_FLAG_EXPLAIN_ONLY	0x0001	/* EXPLAIN, no ANALYZE */
#define EXEC_FLAG_REWIND		0x0002	/* expect rescan */
#define EXEC_FLAG_BACKWARD		0x0004	/* need backward scan */
#define EXEC_FLAG_MARK			0x0008	/* need mark/restore */
#define EXEC_FLAG_SKIP_TRIGGERS 0x0010	/* skip AfterTrigger calls */
#define EXEC_FLAG_WITH_OIDS		0x0020	/* force OIDs in returned tuples */
#define EXEC_FLAG_WITHOUT_OIDS	0x0040	/* force no OIDs in returned tuples */
#define EXEC_FLAG_WITH_NO_DATA	0x0080	/* rel scannability doesn't matter */


/*
 * ExecEvalExpr was formerly a function containing a switch statement;
 * now it's just a macro invoking the function pointed to by an ExprState
 * node.  Beware of double evaluation of the ExprState argument!
 */
#define ExecEvalExpr(expr, econtext, isNull, isDone) \
	((*(expr)->evalfunc) (expr, econtext, isNull, isDone))

#define RelinfoGetStorage(relinfo) ((relinfo)->ri_RelationDesc->rd_rel->relstorage)

/*
 * Indicate whether an executor node is running in the slice
 * that a QE process is processing.
 *
 * This is currently called inside ExecInitXXX for each executor
 * node.
 *
 * If this is called in QD or utility mode, this will return true.
 */

/* Hook for plugins to get control in ExecutorStart() */
typedef void (*ExecutorStart_hook_type) (QueryDesc *queryDesc, int eflags);
extern PGDLLIMPORT ExecutorStart_hook_type ExecutorStart_hook;

/* Hook for plugins to get control in ExecutorRun() */
typedef void (*ExecutorRun_hook_type) (QueryDesc *queryDesc,
												   ScanDirection direction,
												   long count);
extern PGDLLIMPORT ExecutorRun_hook_type ExecutorRun_hook;

/* Hook for plugins to get control in ExecutorFinish() */
typedef void (*ExecutorFinish_hook_type) (QueryDesc *queryDesc);
extern PGDLLIMPORT ExecutorFinish_hook_type ExecutorFinish_hook;

/* Hook for plugins to get control in ExecutorEnd() */
typedef void (*ExecutorEnd_hook_type) (QueryDesc *queryDesc);
extern PGDLLIMPORT ExecutorEnd_hook_type ExecutorEnd_hook;

/* Hook for plugins to get control in ExecCheckRTPerms() */
typedef bool (*ExecutorCheckPerms_hook_type) (List *, bool);
extern PGDLLIMPORT ExecutorCheckPerms_hook_type ExecutorCheckPerms_hook;


/*
 * prototypes from functions in execAmi.c
 */
extern void ExecReScan(PlanState *node);
extern void ExecMarkPos(PlanState *node);
extern void ExecRestrPos(PlanState *node);
extern bool ExecSupportsMarkRestore(NodeTag plantype);
extern bool ExecSupportsBackwardScan(Plan *node);
extern bool ExecMaterializesOutput(NodeTag plantype);

/*
 * prototypes from functions in execCurrent.c
 */
extern void getCurrentOf(CurrentOfExpr *cexpr,
			  ExprContext *econtext,
			  Oid table_oid,
			  ItemPointer current_tid,
			  int *current_gp_segment_id,
			  Oid *current_table_oid,
			  char **cursor_name_p);
extern bool execCurrentOf(CurrentOfExpr *cexpr,
			  ExprContext *econtext,
			  Oid table_oid,
			  ItemPointer current_tid);

/*
 * prototypes from functions in execGrouping.c
 */
extern bool execTuplesMatch(TupleTableSlot *slot1,
				TupleTableSlot *slot2,
				int numCols,
				AttrNumber *matchColIdx,
				FmgrInfo *eqfunctions,
				MemoryContext evalContext);
extern bool execTuplesUnequal(TupleTableSlot *slot1,
				  TupleTableSlot *slot2,
				  int numCols,
				  AttrNumber *matchColIdx,
				  FmgrInfo *eqfunctions,
				  MemoryContext evalContext);
extern FmgrInfo *execTuplesMatchPrepare(int numCols,
					   Oid *eqOperators);
extern void execTuplesHashPrepare(int numCols,
					  Oid *eqOperators,
					  FmgrInfo **eqFunctions,
					  FmgrInfo **hashFunctions);
extern TupleHashTable BuildTupleHashTable(int numCols, AttrNumber *keyColIdx,
					FmgrInfo *eqfunctions,
					FmgrInfo *hashfunctions,
					long nbuckets, Size entrysize,
					MemoryContext tablecxt,
					MemoryContext tempcxt);
extern TupleHashEntry LookupTupleHashEntry(TupleHashTable hashtable,
					 TupleTableSlot *slot,
					 bool *isnew);
extern TupleHashEntry FindTupleHashEntry(TupleHashTable hashtable,
				   TupleTableSlot *slot,
				   FmgrInfo *eqfunctions,
				   FmgrInfo *hashfunctions);

/*
 * prototypes from functions in execJunk.c
 */
extern JunkFilter *ExecInitJunkFilter(List *targetList, bool hasoid,
				   TupleTableSlot *slot);
extern JunkFilter *ExecInitJunkFilterConversion(List *targetList,
							 TupleDesc cleanTupType,
							 TupleTableSlot *slot);
extern AttrNumber ExecFindJunkAttribute(JunkFilter *junkfilter,
					  const char *attrName);
extern AttrNumber ExecFindJunkAttributeInTlist(List *targetlist,
							 const char *attrName);
extern Datum ExecGetJunkAttribute(TupleTableSlot *slot, AttrNumber attno,
					 bool *isNull);
extern TupleTableSlot *ExecFilterJunk(JunkFilter *junkfilter,
			   TupleTableSlot *slot);


/*
 * prototypes from functions in execMain.c
 */

/* AttrMap	
 *
 * A mapping of attribute numbers from one relation to another.
 * Both relations must have the same number of live (non-dropped)
 * attributes in the same order, but both may have holes (dropped
 * attributes) mixed in.  This structure maps live attributes in
 * the "base" relation to live attributes in the "other" relation.
 *
 * AttrMap is used for partitioned table processing because the
 * part relations that hold the data may have different "hole
 * patterns" than the partitioned relation as a whole.  But some
 * cataloged information refers only to the whole and, thus,
 * must be remapped to be used.
 *
 * live_count	number of live attributes in the "base" relation
 *				(and in the "other" relation)
 * attr_max		maximum attribute number in map; live attribute
 *				numbers in "other" range from 1 to attr_max.
 *				This is less than or equal to the number of 
 *              attributes in "other" which may have any number
 *				of trailing holes.
 * attr_count	number of attributes (live or holes) in "base".
 *				One less than the number elements in attr_map,
 *				since attr_map[0]is always 0.
 * attr_map[i]	attr number in "other" corresponding to attr 
 *				number i in "base", or 0 if the attribute is
 *				a hole.  (Note, however, attr_map[0] == 0.  
 *				It is wasted to allow us to use attr numbers
 *				as indexes.  Zero in attr_map stands for "no 
 *				live attribute".)
 *
 * To discard an AttrMap, just pfree it.
 */
typedef struct AttrMap 
{
	int live_count;
	int attr_max;
	int attr_count;
	AttrNumber attr_map[1];
} AttrMap;

extern void ExecutorStart(QueryDesc *queryDesc, int eflags);
extern void standard_ExecutorStart(QueryDesc *queryDesc, int eflags);
extern void ExecutorRun(QueryDesc *queryDesc,
			ScanDirection direction, int64 count);
extern void standard_ExecutorRun(QueryDesc *queryDesc,
					 ScanDirection direction, long count);
extern void ExecutorFinish(QueryDesc *queryDesc);
extern void standard_ExecutorFinish(QueryDesc *queryDesc);
extern void ExecutorEnd(QueryDesc *queryDesc);
extern void standard_ExecutorEnd(QueryDesc *queryDesc);
extern void ExecutorRewind(QueryDesc *queryDesc);
extern bool ExecCheckRTPerms(List *rangeTable, bool ereport_on_violation);
extern bool ExecCheckRTEPerms(RangeTblEntry *rte);
extern void CheckValidResultRel(Relation resultRel, CmdType operation);
extern void InitResultRelInfo(ResultRelInfo *resultRelInfo,
				  Relation resultRelationDesc,
				  Index resultRelationIndex,
				  int instrument_options);
extern ResultRelInfo *ExecGetTriggerResultRel(EState *estate, Oid relid);
extern bool ExecContextForcesOids(PlanState *planstate, bool *hasoids);
extern void ExecConstraints(ResultRelInfo *resultRelInfo,
				TupleTableSlot *slot, EState *estate);
extern void ExecWithCheckOptions(ResultRelInfo *resultRelInfo,
					 TupleTableSlot *slot, EState *estate);
extern ExecRowMark *ExecFindRowMark(EState *estate, Index rti);
extern ExecAuxRowMark *ExecBuildAuxRowMark(ExecRowMark *erm, List *targetlist);
extern TupleTableSlot *EvalPlanQual(EState *estate, EPQState *epqstate,
			 Relation relation, Index rti, int lockmode,
			 ItemPointer tid, TransactionId priorXmax);
extern HeapTuple EvalPlanQualFetch(EState *estate, Relation relation,
				  int lockmode, bool noWait, ItemPointer tid, TransactionId priorXmax);
extern void EvalPlanQualInit(EPQState *epqstate, EState *estate,
				 Plan *subplan, List *auxrowmarks, int epqParam);
extern void EvalPlanQualSetPlan(EPQState *epqstate,
					Plan *subplan, List *auxrowmarks);
extern void EvalPlanQualSetTuple(EPQState *epqstate, Index rti,
					 HeapTuple tuple);
extern HeapTuple EvalPlanQualGetTuple(EPQState *epqstate, Index rti);

#define EvalPlanQualSetSlot(epqstate, slot)  ((epqstate)->origslot = (slot))
extern void EvalPlanQualFetchRowMarks(EPQState *epqstate);
extern TupleTableSlot *EvalPlanQualNext(EPQState *epqstate);
extern void EvalPlanQualBegin(EPQState *epqstate, EState *parentestate);
extern void EvalPlanQualEnd(EPQState *epqstate);

extern Oid GetIntoRelOid(QueryDesc *queryDesc);

extern AttrMap *makeAttrMap(int base_count, AttrNumber *base_map);
extern AttrNumber attrMap(AttrMap *map, AttrNumber anum);
extern Node *attrMapExpr(AttrMap *map, Node *expr);
extern bool map_part_attrs(Relation base, Relation part, AttrMap **map_ptr, bool throwerror);
extern PartitionState *createPartitionState(PartitionNode *partsAndRules, int resultPartSize);
extern TupleTableSlot *reconstructMatchingTupleSlot(TupleTableSlot *slot,
													ResultRelInfo *resultRelInfo);
extern List *InitializePartsMetadata(Oid rootOid);

/*
 * prototypes from functions in execProcnode.c
 */
extern PlanState *ExecInitNode(Plan *node, EState *estate, int eflags);
extern void ExecSliceDependencyNode(PlanState *node);
extern TupleTableSlot *ExecProcNode(PlanState *node);
extern Node *MultiExecProcNode(PlanState *node);
extern void ExecEndNode(PlanState *node);

extern void ExecSquelchNode(PlanState *node);
extern void ExecUpdateTransportState(PlanState *node, struct ChunkTransportState *state);

typedef enum
{
	GP_IGNORE,
	GP_ROOT_SLICE,
	GP_NON_ROOT_ON_QE
} GpExecIdentity;

/* PlanState tree walking functions in execProcnode.c */
CdbVisitOpt
planstate_walk_node(PlanState      *planstate,
			        CdbVisitOpt   (*walker)(PlanState *planstate, void *context),
			        void           *context);

/*
 * prototypes from functions in execQual.c
 */
extern Datum GetAttributeByNum(HeapTupleHeader tuple, AttrNumber attrno,
				  bool *isNull);
extern Datum GetAttributeByName(HeapTupleHeader tuple, const char *attname,
				   bool *isNull);
extern void init_fcache(Oid foid, Oid input_collation, FuncExprState *fcache,
			MemoryContext fcacheCxt, bool needDescForSets);
extern ExprDoneCond ExecEvalFuncArgs(FunctionCallInfo fcinfo,
									 List *argList, 
									 ExprContext *econtext);
extern Tuplestorestate *ExecMakeTableFunctionResult(ExprState *funcexpr,
							ExprContext *econtext,
							MemoryContext argContext,
							TupleDesc expectedDesc,
							bool randomAccess,
							uint64 operatorMemKB);
extern Datum ExecEvalExprSwitchContext(ExprState *expression, ExprContext *econtext,
						  bool *isNull, ExprDoneCond *isDone);
extern ExprState *ExecInitExpr(Expr *node, PlanState *parent);
extern ExprState *ExecPrepareExpr(Expr *node, EState *estate);
extern bool ExecQual(List *qual, ExprContext *econtext, bool resultForNull);
extern int	ExecTargetListLength(List *targetlist);
extern int	ExecCleanTargetListLength(List *targetlist);
extern TupleTableSlot *ExecProject(ProjectionInfo *projInfo,
			ExprDoneCond *isDone);
extern Datum ExecEvalFunctionArgToConst(FuncExpr *fexpr, int argno, bool *isnull);
extern void GetNeededColumnsForScan(Node *expr, bool *mask, int n);
extern bool isJoinExprNull(List *joinExpr, ExprContext *econtext);

/*
 * prototypes from functions in execScan.c
 */
typedef TupleTableSlot *(*ExecScanAccessMtd) (ScanState *node);
typedef bool (*ExecScanRecheckMtd) (ScanState *node, TupleTableSlot *slot);

extern TupleTableSlot *ExecScan(ScanState *node, ExecScanAccessMtd accessMtd,
		 ExecScanRecheckMtd recheckMtd);
extern void ExecAssignScanProjectionInfo(ScanState *node);
extern void ExecScanReScan(ScanState *node);

/*
 * prototypes from functions in execTuples.c
 */
extern void ExecInitResultTupleSlot(EState *estate, PlanState *planstate);
extern void ExecInitScanTupleSlot(EState *estate, ScanState *scanstate);
extern TupleTableSlot *ExecInitExtraTupleSlot(EState *estate);
extern TupleTableSlot *ExecInitNullTupleSlot(EState *estate,
					  TupleDesc tupType);
extern TupleDesc ExecTypeFromTL(List *targetList, bool hasoid);
extern TupleDesc ExecCleanTypeFromTL(List *targetList, bool hasoid);
extern TupleDesc ExecTypeFromExprList(List *exprList);
extern void ExecTypeSetColNames(TupleDesc typeInfo, List *namesList);
extern void UpdateChangedParamSet(PlanState *node, Bitmapset *newchg);

typedef struct TupOutputState
{
	TupleTableSlot *slot;
	DestReceiver *dest;
} TupOutputState;

extern TupOutputState *begin_tup_output_tupdesc(DestReceiver *dest,
						 TupleDesc tupdesc);
extern void do_tup_output(TupOutputState *tstate, Datum *values, bool *isnull);
extern void do_text_output_multiline(TupOutputState *tstate, const char *txt);
extern void end_tup_output(TupOutputState *tstate);

/*
 * Write a single line of text given as a C string.
 *
 * Should only be used with a single-TEXT-attribute tupdesc.
 */
#define do_text_output_oneline(tstate, str_to_emit) \
	do { \
		Datum	values_[1]; \
		bool	isnull_[1]; \
		values_[0] = PointerGetDatum(cstring_to_text(str_to_emit)); \
		isnull_[0] = false; \
		do_tup_output(tstate, values_, isnull_); \
		pfree(DatumGetPointer(values_[0])); \
	} while (0)


/*
 * prototypes from functions in execUtils.c
 */
extern EState *CreateExecutorState(void);
extern void FreeExecutorState(EState *estate);
extern void CloseResultRelInfo(ResultRelInfo *resultRelInfo);
extern ExprContext *CreateExprContext(EState *estate);
extern ExprContext *CreateStandaloneExprContext(void);
extern void FreeExprContext(ExprContext *econtext, bool isCommit);
extern void ReScanExprContext(ExprContext *econtext);
extern void ResetExprContext(ExprContext *econtext);

extern ExprContext *MakePerTupleExprContext(EState *estate);

/* Get an EState's per-output-tuple exprcontext, making it if first use */
#define GetPerTupleExprContext(estate) \
	((estate)->es_per_tuple_exprcontext ? \
	 (estate)->es_per_tuple_exprcontext : \
	 MakePerTupleExprContext(estate))

#define GetPerTupleMemoryContext(estate) \
	(GetPerTupleExprContext(estate)->ecxt_per_tuple_memory)

/* Reset an EState's per-output-tuple exprcontext, if one's been created */
#define ResetPerTupleExprContext(estate) \
	do { \
		if ((estate)->es_per_tuple_exprcontext) \
			ResetExprContext((estate)->es_per_tuple_exprcontext); \
	} while (0)

extern void ExecAssignExprContext(EState *estate, PlanState *planstate);
extern void ExecAssignResultType(PlanState *planstate, TupleDesc tupDesc);
extern void ExecAssignResultTypeFromTL(PlanState *planstate);
extern TupleDesc ExecGetResultType(PlanState *planstate);
extern ProjectionInfo *ExecBuildProjectionInfo(List *targetList,
						ExprContext *econtext,
						TupleTableSlot *slot,
						TupleDesc inputDesc);
extern void ExecAssignProjectionInfo(PlanState *planstate,
						 TupleDesc inputDesc);
extern void ExecFreeExprContext(PlanState *planstate);
extern TupleDesc ExecGetScanType(ScanState *scanstate);
extern void ExecAssignScanType(ScanState *scanstate, TupleDesc tupDesc);
extern void ExecAssignScanTypeFromOuterPlan(ScanState *scanstate);

extern bool ExecRelationIsTargetRelation(EState *estate, Index scanrelid);

extern Relation ExecOpenScanRelation(EState *estate, Index scanrelid, int eflags);
extern Relation ExecOpenScanExternalRelation(EState *estate, Index scanrelid);
extern void ExecCloseScanRelation(Relation scanrel);
extern void ExecCloseScanAppendOnlyRelation(Relation scanrel);

extern void ExecOpenIndices(ResultRelInfo *resultRelInfo);
extern void ExecCloseIndices(ResultRelInfo *resultRelInfo);
extern List *ExecInsertIndexTuples(TupleTableSlot *slot, ItemPointer tupleid,
					  EState *estate);
extern bool check_exclusion_constraint(Relation heap, Relation index,
						   IndexInfo *indexInfo,
						   ItemPointer tupleid,
						   Datum *values, bool *isnull,
						   EState *estate,
						   bool newIndex, bool errorOK);

extern void RegisterExprContextCallback(ExprContext *econtext,
							ExprContextCallbackFunction function,
							Datum arg);
extern void UnregisterExprContextCallback(ExprContext *econtext,
							  ExprContextCallbackFunction function,
							  Datum arg);

/* Share input utilities defined in execUtils.c */
extern ShareNodeEntry * ExecGetShareNodeEntry(EState *estate, int shareid, bool fCreate);

extern void fake_outer_params(JoinState *node);
extern void ExecPrefetchQual(JoinState *node, bool isJoinQual);

/* ResultRelInfo and Append Only segment assignment */
void ResultRelInfoSetSegno(ResultRelInfo *resultRelInfo, List *mapping);

/* Additions for MPP Slice table utilities defined in execUtils.c */
extern GpExecIdentity getGpExecIdentity(QueryDesc *queryDesc,
										  ScanDirection direction,
										  EState	   *estate);
extern void mppExecutorFinishup(QueryDesc *queryDesc);
extern void mppExecutorCleanup(QueryDesc *queryDesc);


/* prototypes defined in nodeAgg.c for rollup-aware Agg/Group nodes. */
extern int64 tuple_grouping(TupleTableSlot *outerslot, int numGroupCols,
							int input_grouping, bool input_has_grouping,
							int grpingIdx);
extern uint64 get_grouping_groupid(TupleTableSlot *slot,
								   int grping_idx);

extern ResultRelInfo *targetid_get_partition(Oid targetid, EState *estate, bool openIndices);
extern ResultRelInfo *slot_get_partition(TupleTableSlot *slot, EState *estate);
extern ResultRelInfo *values_get_partition(Datum *values, bool *nulls,
					 TupleDesc desc, EState *estate, bool openIndices);

extern void SendAOTupCounts(EState *estate);

#endif   /* EXECUTOR_H  */
