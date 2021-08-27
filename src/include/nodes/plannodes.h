/*-------------------------------------------------------------------------
 *
 * plannodes.h
 *	  definitions for query plan nodes
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/nodes/plannodes.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PLANNODES_H
#define PLANNODES_H

#include "access/sdir.h"
#include "nodes/bitmapset.h"
#include "nodes/primnodes.h"
#include "parsenodes.h"

typedef struct DirectDispatchInfo
{
     /**
      * if true then this Slice requires an n-gang but the gang can be targeted to
      *   fewer segments than the entire cluster.
      *
      * When true, directDispatchContentId and directDispathCount will combine to indicate
      *    the content ids that need segments.
      */
	bool isDirectDispatch;
    List *contentIds;
} DirectDispatchInfo;

typedef enum PlanGenerator
{
	PLANGEN_PLANNER,			/* plan produced by the planner*/
	PLANGEN_OPTIMIZER,			/* plan produced by the optimizer*/
} PlanGenerator;

/* DML Actions */
typedef enum DMLAction
{
	DML_DELETE,
	DML_INSERT
} DMLAction;

/* ----------------------------------------------------------------
 *						node definitions
 * ----------------------------------------------------------------
 */

/* ----------------
 *		PlannedStmt node
 *
 * The output of the planner is a Plan tree headed by a PlannedStmt node.
 * PlannedStmt holds the "one time" information needed by the executor.
 * ----------------
 */
typedef struct PlannedStmt
{
	NodeTag		type;

	CmdType		commandType;	/* select|insert|update|delete */

	PlanGenerator	planGen;		/* optimizer generation */

	uint32		queryId;		/* query identifier (copied from Query) */

	bool		hasReturning;	/* is it insert|update|delete RETURNING? */

	bool		hasModifyingCTE;	/* has insert|update|delete in WITH? */

	bool		canSetTag;		/* do I set the command result tag? */

	bool		transientPlan;	/* redo plan when TransactionXmin changes? */
	bool		oneoffPlan;		/* redo plan on every execution? */

	bool		simplyUpdatable; /* can be used with CURRENT OF? */

	struct Plan *planTree;		/* tree of Plan nodes */

	List	   *rtable;			/* list of RangeTblEntry nodes */

	/* rtable indexes of target relations for INSERT/UPDATE/DELETE */
	List	   *resultRelations;	/* integer list of RT indexes, or NIL */

	Node	   *utilityStmt;	/* non-null if this is DECLARE CURSOR */

	List	   *subplans;		/* Plan trees for SubPlan expressions */

	Bitmapset  *rewindPlanIDs;	/* indices of subplans that require REWIND */

	/*
	 * If the resultRelation turns out to be the parent of an inheritance
	 * tree, the planner will add all the child tables to the rtable and store
	 * a list of the rtindexes of all the result relations here. This is done
	 * at plan time, not parse time, since we don't want to commit to the
	 * exact set of child tables at parse time. This field used to be in Query.
	 */
	struct PartitionNode *result_partitions;

	List	   *result_aosegnos; /* AO file 'seg' numbers for resultRels to use */

	/*
	 * Relation oids and partitioning metadata for all partitions
	 * that are involved in a query.
	 */
	List	   *queryPartOids;
	List	   *queryPartsMetadata;
	/*
	 * List containing the number of partition selectors for every scan id.
	 * Element #i in the list corresponds to scan id i
	 */
	List	   *numSelectorsPerScanId;

	List	   *rowMarks;		/* a list of PlanRowMark's */

	List	   *relationOids;	/* OIDs of relations the plan depends on */

	List	   *invalItems;		/* other dependencies, as PlanInvalItems */

	int			nParamExec;		/* number of PARAM_EXEC Params used */

	int			nMotionNodes;	/* number of Motion nodes in plan */

	int			nInitPlans;		/* number of initPlans in plan */

	/* 
	 * Cloned from top Query node at the end of planning.
	 * Holds the result distribution policy
	 * for SELECT ... INTO and set operations.
	 */
	struct GpPolicy  *intoPolicy;

	/* What is the memory reserved for this query's execution? */
	uint64		query_mem;

	/*
	 * GPDB: Used to keep target information for CTAS and it is needed
	 * to be dispatched to QEs.
	 */
	IntoClause *intoClause;
	CopyIntoClause *copyIntoClause;
	RefreshClause   *refreshClause;		/* relation to insert into */

	/* 
 	 * GPDB: whether a query is a SPI inner query for extension usage 
 	 */
	int8		metricsQueryType;
} PlannedStmt;

/*
 * Fetch the Plan associated with a SubPlan node in a completed PlannedStmt.
 */
static inline struct Plan *exec_subplan_get_plan(struct PlannedStmt *plannedstmt, SubPlan *subplan)
{
	return (struct Plan *) list_nth(plannedstmt->subplans, subplan->plan_id - 1);
}

/*
 * Rewrite the Plan associated with a SubPlan node in a completed PlannedStmt.
 */
static inline void exec_subplan_put_plan(struct PlannedStmt *plannedstmt, SubPlan *subplan, struct Plan *plan)
{
	ListCell *cell = list_nth_cell(plannedstmt->subplans, subplan->plan_id-1);
	cell->data.ptr_value = plan;
}


/* ----------------
 *		Plan node
 *
 * All plan nodes "derive" from the Plan structure by having the
 * Plan structure as the first field.  This ensures that everything works
 * when nodes are cast to Plan's.  (node pointers are frequently cast to Plan*
 * when passed around generically in the executor)
 *
 * We never actually instantiate any Plan nodes; this is just the common
 * abstract superclass for all Plan-type nodes.
 * ----------------
 */
typedef struct Plan
{
	NodeTag		type;

	/* Plan node id */
	int			plan_node_id;	/* unique across entire final plan tree */

	/*
	 * estimated execution costs for plan (see costsize.c for more info)
	 */
	Cost		startup_cost;	/* cost expended before fetching any tuples */
	Cost		total_cost;		/* total cost (assuming all tuples fetched) */

	/*
	 * planner's estimate of result size of this plan step
	 */
	double		plan_rows;		/* number of rows plan is expected to emit */
	int			plan_width;		/* average row width in bytes */

	/*
	 * Common structural data for all Plan types.
	 */
	List	   *targetlist;		/* target list to be computed at this node */
	List	   *qual;			/* implicitly-ANDed qual conditions */
	struct Plan *lefttree;		/* input plan tree(s) */
	struct Plan *righttree;
	List	   *initPlan;		/* Init Plan nodes (un-correlated expr
								 * subselects) */

	/*
	 * Information for management of parameter-change-driven rescanning
	 *
	 * extParam includes the paramIDs of all external PARAM_EXEC params
	 * affecting this plan node or its children.  setParam params from the
	 * node's initPlans are not included, but their extParams are.
	 *
	 * allParam includes all the extParam paramIDs, plus the IDs of local
	 * params that affect the node (i.e., the setParams of its initplans).
	 * These are _all_ the PARAM_EXEC params that affect this node.
	 */
	Bitmapset  *extParam;
	Bitmapset  *allParam;

	/*
	 * MPP needs to keep track of the characteristics of flow of output
	 * tuple of Plan nodes.
	 */
	Flow		*flow;			/* Flow description.  Initially NULL.
	 * Set during parallelization.
	 */

	/*
	 * CDB:  How should this plan tree be dispatched?  Initially this is set
	 * to DISPATCH_UNDETERMINED and, in non-root nodes, may remain so.
	 * However, in Plan nodes at the root of any separately dispatchable plan
	 * fragment, it must be set to a specific dispatch type.
	 */
	DispatchMethod dispatch;

	/*
	 * CDB: if we're going to direct dispatch, point it at a particular id.
	 *
	 * For motion nodes, this direct dispatch data is for the slice rooted at the
	 *   motion node (the sending side!)
	 * For other nodes, it is for the slice rooted at this plan so it must be a root
	 *   plan for a query
	 * Note that for nodes that are internal to a slice then this data is not
	 *   set.
	 */
	DirectDispatchInfo directDispatch;

	/*
	 * CDB: Now many motion nodes are there in the Plan.  How many init plans?
	 * Additional plan tree global significant only in the root node.
	 */
	int nMotionNodes;
	int nInitPlans;

	/*
	 * CDB: This allows the slice table to accompany the plan as it
	 * moves around the executor. This is anoter plan tree global that
	 * should be non-NULL only in the top node of a dispatchable tree.
	 * It could (and should) move to a TopPlan node if we ever do that.
	 *
	 * Currently, the slice table should not be installed on the QD.
	 * Rather is it shipped to QEs as a separate parameter to MPPEXEC.
	 * The implementation of MPPEXEC, which runs on the QEs, installs
	 * the slice table in the plan as required there.
	 */
	Node *sliceTable;

	/**
	 * How much memory (in KB) should be used to execute this plan node?
	 */
	uint64 operatorMemKB;

	/*
	 * The parent motion node of a plan node.
	 */
	struct Plan *motionNode;
} Plan;

/* ----------------
 *	these are defined to avoid confusion problems with "left"
 *	and "right" and "inner" and "outer".  The convention is that
 *	the "left" plan is the "outer" plan and the "right" plan is
 *	the inner plan, but these make the code more readable.
 * ----------------
 */
#define innerPlan(node)			(((Plan *)(node))->righttree)
#define outerPlan(node)			(((Plan *)(node))->lefttree)


/* ----------------
 *	 Result node -
 *		If no outer plan, evaluate a variable-free targetlist.
 *		If outer plan, return tuples from outer plan (after a level of
 *		projection as shown by targetlist).
 *
 * If resconstantqual isn't NULL, it represents a one-time qualification
 * test (i.e., one that doesn't depend on any variables from the outer plan,
 * so needs to be evaluated only once).
 *
 * If numHashFilterCols is non-zero, we compute a cdbhash value based
 * on the columns listed in hashFilterColIdx for each input row. If the
 * target segment based on the hash doesn't match the current execution
 * segment, the row is discarded.
 * ----------------
 */
typedef struct Result
{
	Plan		plan;
	Node	   *resconstantqual;

	int			numHashFilterCols;
	AttrNumber *hashFilterColIdx;
	Oid		   *hashFilterFuncs;
} Result;

/* ----------------
 * Repeat node -
 *   Repeatly output the results of the subplan.
 *
 * The repetition for each result tuple from the subplan is determined
 * by the value from a specified column.
 * ----------------
 */
typedef struct Repeat
{
	Plan plan;

	/*
	 * An expression to represent the number of times an input tuple to
	 * be repeatly outputted by this node.
	 *
	 * Currently, this expression should result in an integer.
	 */
	Expr *repeatCountExpr;

	/*
	 * The GROUPING value. This is used for grouping extension
	 * distinct-qualified queries. The distinct-qualified plan generated
	 * through cdbgroup.c may have a Join Plan node on the top, which
	 * can not properly handle GROUPING values. We let the Repeat
	 * node to handle this case.
	 */
	uint64 grouping;
} Repeat;

/* ----------------
 *	 ModifyTable node -
 *		Apply rows produced by subplan(s) to result table(s),
 *		by inserting, updating, or deleting.
 *
 * Note that rowMarks and epqParam are presumed to be valid for all the
 * subplan(s); they can't contain any info that varies across subplans.
 * ----------------
 */
typedef struct ModifyTable
{
	Plan		plan;
	CmdType		operation;		/* INSERT, UPDATE, or DELETE */
	bool		canSetTag;		/* do we set the command tag/es_processed? */
	List	   *resultRelations;	/* integer list of RT indexes */
	int			resultRelIndex; /* index of first resultRel in plan's list */
	List	   *plans;			/* plan(s) producing source data */
	List	   *withCheckOptionLists;	/* per-target-table WCO lists */
	List	   *returningLists; /* per-target-table RETURNING tlists */
	List	   *fdwPrivLists;	/* per-target-table FDW private data lists */
	List	   *rowMarks;		/* PlanRowMarks (non-locking only) */
	int			epqParam;		/* ID of Param for EvalPlanQual re-eval */
	List	   *action_col_idxes;
	List	   *ctid_col_idxes;
	List	   *oid_col_idxes;
} ModifyTable;

/* ----------------
 *	 Append node -
 *		Generate the concatenation of the results of sub-plans.
 * ----------------
 */
typedef struct Append
{
	Plan		plan;
	List	   *appendplans;
} Append;

/* ----------------
 *	 MergeAppend node -
 *		Merge the results of pre-sorted sub-plans to preserve the ordering.
 * ----------------
 */
typedef struct MergeAppend
{
	Plan		plan;
	List	   *mergeplans;
	/* remaining fields are just like the sort-key info in struct Sort */
	int			numCols;		/* number of sort-key columns */
	AttrNumber *sortColIdx;		/* their indexes in the target list */
	Oid		   *sortOperators;	/* OIDs of operators to sort them by */
	Oid		   *collations;		/* OIDs of collations */
	bool	   *nullsFirst;		/* NULLS FIRST/LAST directions */
} MergeAppend;

/*
 * Sequence node
 *   Execute a list of subplans in the order of left-to-right, and return
 * the results of the last subplan.
 */
typedef struct Sequence
{
	Plan plan;
	List *subplans;
} Sequence;

/* ----------------
 *	RecursiveUnion node -
 *		Generate a recursive union of two subplans.
 *
 * The "outer" subplan is always the non-recursive term, and the "inner"
 * subplan is the recursive term.
 * ----------------
 */
typedef struct RecursiveUnion
{
	Plan		plan;
	int			wtParam;		/* ID of Param representing work table */
	/* Remaining fields are zero/null in UNION ALL case */
	int			numCols;		/* number of columns to check for
								 * duplicate-ness */
	AttrNumber *dupColIdx;		/* their indexes in the target list */
	Oid		   *dupOperators;	/* equality operators to compare with */
	long		numGroups;		/* estimated number of groups in input */
} RecursiveUnion;

/* ----------------
 *	 BitmapAnd node -
 *		Generate the intersection of the results of sub-plans.
 *
 * The subplans must be of types that yield tuple bitmaps.  The targetlist
 * and qual fields of the plan are unused and are always NIL.
 * ----------------
 */
typedef struct BitmapAnd
{
	Plan		plan;
	List	   *bitmapplans;
} BitmapAnd;

/* ----------------
 *	 BitmapOr node -
 *		Generate the union of the results of sub-plans.
 *
 * The subplans must be of types that yield tuple bitmaps.  The targetlist
 * and qual fields of the plan are unused and are always NIL.
 * ----------------
 */
typedef struct BitmapOr
{
	Plan		plan;
	List	   *bitmapplans;
} BitmapOr;

/*
 * ==========
 * Scan nodes
 * ==========
 */
typedef struct Scan
{
	Plan		plan;
	Index		scanrelid;		/* relid is index into the range table */
} Scan;

/* ----------------
 *		sequential scan node
 * ----------------
 */
typedef Scan SeqScan;

/* ----------------
 *		index type information
 */
typedef enum LogicalIndexType
{
	INDTYPE_BTREE = 0,
	INDTYPE_BITMAP = 1,
	INDTYPE_GIST = 2,
	INDTYPE_GIN = 3
} LogicalIndexType;

typedef struct LogicalIndexInfo
{
	Oid	logicalIndexOid;	/* OID of the logical index */
	int	nColumns;		/* Number of columns in the index */
	AttrNumber	*indexKeys;	/* column numbers of index keys */
	List	*indPred;		/* predicate if partial index, or NIL */
	List	*indExprs;		/* index on expressions */
	bool	indIsUnique;		/* unique index */
	LogicalIndexType indType;  /* index type: btree or bitmap */
	Node	*partCons;		/* concatenated list of check constraints
					 * of each partition on which this index is defined */
	List	*defaultLevels;		/* Used to identify a default partition */
} LogicalIndexInfo;

/* ----------------
 *		index scan node
 *
 * indexqualorig is an implicitly-ANDed list of index qual expressions, each
 * in the same form it appeared in the query WHERE condition.  Each should
 * be of the form (indexkey OP comparisonval) or (comparisonval OP indexkey).
 * The indexkey is a Var or expression referencing column(s) of the index's
 * base table.  The comparisonval might be any expression, but it won't use
 * any columns of the base table.  The expressions are ordered by index
 * column position (but items referencing the same index column can appear
 * in any order).  indexqualorig is used at runtime only if we have to recheck
 * a lossy indexqual.
 *
 * indexqual has the same form, but the expressions have been commuted if
 * necessary to put the indexkeys on the left, and the indexkeys are replaced
 * by Var nodes identifying the index columns (their varno is INDEX_VAR and
 * their varattno is the index column number).
 *
 * indexorderbyorig is similarly the original form of any ORDER BY expressions
 * that are being implemented by the index, while indexorderby is modified to
 * have index column Vars on the left-hand side.  Here, multiple expressions
 * must appear in exactly the ORDER BY order, and this is not necessarily the
 * index column order.  Only the expressions are provided, not the auxiliary
 * sort-order information from the ORDER BY SortGroupClauses; it's assumed
 * that the sort ordering is fully determinable from the top-level operators.
 * indexorderbyorig is unused at run time, but is needed for EXPLAIN.
 * (Note these fields are used for amcanorderbyop cases, not amcanorder cases.)
 *
 * indexorderdir specifies the scan ordering, for indexscans on amcanorder
 * indexes (for other indexes it should be "don't care").
 * ----------------
 */
typedef struct IndexScan
{
	Scan		scan;
	Oid			indexid;		/* OID of index to scan */
	List	   *indexqual;		/* list of index quals (usually OpExprs) */
	List	   *indexqualorig;	/* the same in original form */
	List	   *indexorderby;	/* list of index ORDER BY exprs */
	List	   *indexorderbyorig;		/* the same in original form */
	ScanDirection indexorderdir;	/* forward or backward or don't care */
} IndexScan;

/*
 * DynamicIndexScan
 *   Scan a list of indexes that will be determined at run time.
 *   The primary application of this operator is to be used
 *   for partition tables.
*/
typedef struct DynamicIndexScan
{
	/* Fields shared with a normal IndexScan. Must be first! */
	IndexScan	indexscan;

	/*
	 * Index to arrays in EState->dynamicTableScanInfo, that contain information
	 * about the partitiones that need to be scanned.
	 */
	int32 		partIndex;
	int32 		partIndexPrintable;

	/* logical index to use */
	LogicalIndexInfo *logicalIndexInfo;
} DynamicIndexScan;

/* ----------------
 *		index-only scan node
 *
 * IndexOnlyScan is very similar to IndexScan, but it specifies an
 * index-only scan, in which the data comes from the index not the heap.
 * Because of this, *all* Vars in the plan node's targetlist, qual, and
 * index expressions reference index columns and have varno = INDEX_VAR.
 * Hence we do not need separate indexqualorig and indexorderbyorig lists,
 * since their contents would be equivalent to indexqual and indexorderby.
 *
 * To help EXPLAIN interpret the index Vars for display, we provide
 * indextlist, which represents the contents of the index as a targetlist
 * with one TLE per index column.  Vars appearing in this list reference
 * the base table, and this is the only field in the plan node that may
 * contain such Vars.
 *
 * GPDB: We need indexqualorig to determine direct dispatch, however there
 * is no need to dispatch it.
 * ----------------
 */
typedef struct IndexOnlyScan
{
	Scan		scan;
	Oid			indexid;		/* OID of index to scan */
	List	   *indexqual;		/* list of index quals (usually OpExprs) */
	List	   *indexqualorig;	/* the same in original form (GPDB keeps it) */
	List	   *indexorderby;	/* list of index ORDER BY exprs */
	List	   *indextlist;		/* TargetEntry list describing index's cols */
	ScanDirection indexorderdir;	/* forward or backward or don't care */
} IndexOnlyScan;

/* ----------------
 *		bitmap index scan node
 *
 * BitmapIndexScan delivers a bitmap of potential tuple locations;
 * it does not access the heap itself.  The bitmap is used by an
 * ancestor BitmapHeapScan node, possibly after passing through
 * intermediate BitmapAnd and/or BitmapOr nodes to combine it with
 * the results of other BitmapIndexScans.
 *
 * The fields have the same meanings as for IndexScan, except we don't
 * store a direction flag because direction is uninteresting.
 *
 * In a BitmapIndexScan plan node, the targetlist and qual fields are
 * not used and are always NIL.  The indexqualorig field is unused at
 * run time too, but is saved for the benefit of EXPLAIN, as well
 * as for the use of the planner when doing clause examination on plans
 * (such as for targeted dispatch)
 * ----------------
 */
typedef struct BitmapIndexScan
{
	Scan		scan;
	Oid			indexid;		/* OID of index to scan */
	List	   *indexqual;		/* list of index quals (OpExprs) */
	List	   *indexqualorig;	/* the same in original form */
} BitmapIndexScan;

/*
 * DynamicBitmapIndexScan
 *   Scan a list of indexes that will be determined at run time.
 *   For use with partitioned tables.
*/
typedef struct DynamicBitmapIndexScan
{
	/* Fields shared with a normal BitmapIndexScan. Must be first! */
	BitmapIndexScan biscan;

	/*
	 * Index to arrays in EState->dynamicTableScanInfo, that contain information
	 * about the partitiones that need to be scanned.
	 */
	int32 		partIndex;
	int32 		partIndexPrintable;

	/* logical index to use */
	LogicalIndexInfo *logicalIndexInfo;
} DynamicBitmapIndexScan;

/* ----------------
 *		bitmap sequential scan node
 *
 * This needs a copy of the qual conditions being used by the input index
 * scans because there are various cases where we need to recheck the quals;
 * for example, when the bitmap is lossy about the specific rows on a page
 * that meet the index condition.
 * ----------------
 */
typedef struct BitmapHeapScan
{
	Scan		scan;
	List	   *bitmapqualorig; /* index quals, in standard expr form */
} BitmapHeapScan;

/*
 * DynamicBitmapHeapScan
 *   Scan a list of tables that will be determined at run time.
 *
 * Dynamic counterpart of a BitmapHeapScan, for use with partitioned tables.
 */
typedef struct DynamicBitmapHeapScan
{
	BitmapHeapScan bitmapheapscan;

	/*
	 * Index to arrays in EState->dynamicTableScanInfo, that contain information
	 * about the partitiones that need to be scanned.
	 */
	int32 		partIndex;
	int32 		partIndexPrintable;
} DynamicBitmapHeapScan;

/*
 * DynamicSeqScan
 *   Scan a list of tables that will be determined at run time.
 */
typedef struct DynamicSeqScan
{
	/* Fields shared with a normal SeqScan. Must be first! */
	SeqScan		seqscan;

	/*
	 * Index to arrays in EState->dynamicTableScanInfo, that contain information
	 * about the partitiones that need to be scanned.
	 */
	int32 		partIndex;
	int32 		partIndexPrintable;
} DynamicSeqScan;

/* ----------------
 *		tid scan node
 *
 * tidquals is an implicitly OR'ed list of qual expressions of the form
 * "CTID = pseudoconstant" or "CTID = ANY(pseudoconstant_array)".
 * ----------------
 */
typedef struct TidScan
{
	Scan		scan;
	List	   *tidquals;		/* qual(s) involving CTID = something */
} TidScan;

/* ----------------
 *		subquery scan node
 *
 * SubqueryScan is for scanning the output of a sub-query in the range table.
 * We often need an extra plan node above the sub-query's plan to perform
 * expression evaluations (which we can't push into the sub-query without
 * risking changing its semantics).  Although we are not scanning a physical
 * relation, we make this a descendant of Scan anyway for code-sharing
 * purposes.
 *
 * Note: we store the sub-plan in the type-specific subplan field, not in
 * the generic lefttree field as you might expect.  This is because we do
 * not want plan-tree-traversal routines to recurse into the subplan without
 * knowing that they are changing Query contexts.
 * ----------------
 */
typedef struct SubqueryScan
{
	Scan		scan;
	Plan	   *subplan;
} SubqueryScan;

/* ----------------
 *		FunctionScan node
 * ----------------
 */
typedef struct FunctionScan
{
	Scan		scan;
	List	   *functions;		/* list of RangeTblFunction nodes */
	bool		funcordinality; /* WITH ORDINALITY */
	Param      *param;			/* used when funtionscan run as initplan */
	bool		resultInTupleStore; /* function result stored in tuplestore */
	int			initplanId;			/* initplan id for function execute on initplan */
} FunctionScan;

/* ----------------
 *      TableFunctionScan node
 *
 * This is similar to a FunctionScan, but we only support one function,
 * and WITH ORDINALITY is not supported.
 *
 * ----------------
 */
typedef struct TableFunctionScan
{
	Scan		scan;
	struct RangeTblFunction *function;
} TableFunctionScan;

/* ----------------
 *		ValuesScan node
 * ----------------
 */
typedef struct ValuesScan
{
	Scan		scan;
	List	   *values_lists;	/* list of expression lists */
} ValuesScan;

/* ----------------
 *		CteScan node
 * ----------------
 */
typedef struct CteScan
{
	Scan		scan;
	int			ctePlanId;		/* ID of init SubPlan for CTE */
	int			cteParam;		/* ID of Param representing CTE output */
} CteScan;

/* ----------------
 *		WorkTableScan node
 * ----------------
 */
typedef struct WorkTableScan
{
	Scan		scan;
	int			wtParam;		/* ID of Param representing work table */
} WorkTableScan;

/* ----------------
 * External Scan node
 *
 * Field scan.scanrelid is the index of the external relation for
 * this node.
 *
 * Field filenames is a list of N string node pointers (or NULL)
 * where N is number of segments in the array. The pointer in
 * position I is NULL or points to the string node containing the
 * file name for segment I.
 * ----------------
 */
typedef struct ExternalScan
{
	Scan		scan;
	List		*uriList;       /* data uri or null for each segment  */
	char	   *fmtOptString;	/* data format options                */
	char		fmtType;        /* data format type                   */
	bool		isMasterOnly;   /* true for EXECUTE on master seg only */
	int			rejLimit;       /* reject limit (-1 for no sreh)      */
	bool		rejLimitInRows; /* true if ROWS false if PERCENT      */
	bool		logErrors;      /* true to log errors into file       */
	int			encoding;		/* encoding of external table data    */
	uint32      scancounter;	/* counter incr per scan node created */

} ExternalScan;

/* ----------------
 *		ForeignScan node
 *
 * fdw_exprs and fdw_private are both under the control of the foreign-data
 * wrapper, but fdw_exprs is presumed to contain expression trees and will
 * be post-processed accordingly by the planner; fdw_private won't be.
 * Note that everything in both lists must be copiable by copyObject().
 * One way to store an arbitrary blob of bytes is to represent it as a bytea
 * Const.  Usually, though, you'll be better off choosing a representation
 * that can be dumped usefully by nodeToString().
 * ----------------
 */
typedef struct ForeignScan
{
	Scan		scan;
	List	   *fdw_exprs;		/* expressions that FDW may evaluate */
	List	   *fdw_private;	/* private data for FDW */
	bool		fsSystemCol;	/* true if any "system column" is needed */
} ForeignScan;


/*
 * ==========
 * Join nodes
 * ==========
 */

/* ----------------
 *		Join node
 *
 * jointype:	rule for joining tuples from left and right subtrees
 * joinqual:	qual conditions that came from JOIN/ON or JOIN/USING
 *				(plan.qual contains conditions that came from WHERE)
 *
 * When jointype is INNER, joinqual and plan.qual are semantically
 * interchangeable.  For OUTER jointypes, the two are *not* interchangeable;
 * only joinqual is used to determine whether a match has been found for
 * the purpose of deciding whether to generate null-extended tuples.
 * (But plan.qual is still applied before actually returning a tuple.)
 * For an outer join, only joinquals are allowed to be used as the merge
 * or hash condition of a merge or hash join.
 * ----------------
 */
typedef struct Join
{
	Plan		plan;
	JoinType	jointype;
	List	   *joinqual;		/* JOIN quals (in addition to plan.qual) */

	bool		prefetch_inner; /* to avoid deadlock in MPP */
	bool		prefetch_joinqual; /* to avoid deadlock in MPP */
	bool		prefetch_qual; /* to avoid deadlock in MPP */
} Join;

/* ----------------
 *		nest loop join node
 *
 * The nestParams list identifies any executor Params that must be passed
 * into execution of the inner subplan carrying values from the current row
 * of the outer subplan.  Currently we restrict these values to be simple
 * Vars, but perhaps someday that'd be worth relaxing.  (Note: during plan
 * creation, the paramval can actually be a PlaceHolderVar expression; but it
 * must be a Var with varno OUTER_VAR by the time it gets to the executor.)
 * ----------------
 */
typedef struct NestLoop
{
	Join		join;
	List	   *nestParams;		/* list of NestLoopParam nodes */

	bool		shared_outer;
	bool		singleton_outer; /*CDB-OLAP true => outer is plain Agg */
} NestLoop;

typedef struct NestLoopParam
{
	NodeTag		type;
	int			paramno;		/* number of the PARAM_EXEC Param to set */
	Var		   *paramval;		/* outer-relation Var to assign to Param */
} NestLoopParam;

/* ----------------
 *		merge join node
 *
 * The expected ordering of each mergeable column is described by a btree
 * opfamily OID, a collation OID, a direction (BTLessStrategyNumber or
 * BTGreaterStrategyNumber) and a nulls-first flag.  Note that the two sides
 * of each mergeclause may be of different datatypes, but they are ordered the
 * same way according to the common opfamily and collation.  The operator in
 * each mergeclause must be an equality operator of the indicated opfamily.
 * ----------------
 */
typedef struct MergeJoin
{
	Join		join;
	List	   *mergeclauses;	/* mergeclauses as expression trees */
	/* these are arrays, but have the same length as the mergeclauses list: */
	Oid		   *mergeFamilies;	/* per-clause OIDs of btree opfamilies */
	Oid		   *mergeCollations;	/* per-clause OIDs of collations */
	int		   *mergeStrategies;	/* per-clause ordering (ASC or DESC) */
	bool	   *mergeNullsFirst;	/* per-clause nulls ordering */
	bool		unique_outer; /*CDB-OLAP true => outer is unique in merge key */
} MergeJoin;

/* ----------------
 *		hash join (probe) node
 *
 * CDB:	In order to support hash join on IS NOT DISTINCT FROM (as well as =),
 *		field hashqualclauses is added to hold the expression that tests for
 *		a match.  This is normally identical to hashclauses (which holds the
 *		equality test), but differs in case of non-equijoin comparisons.
 *		Field hashclauses is retained for use in hash table operations.
 * ----------------
 */
typedef struct HashJoin
{
	Join		join;
	List	   *hashclauses;
	List	   *hashqualclauses;
} HashJoin;

/*
 * Share type of sharing a node.
 */
typedef enum ShareType
{
	SHARE_NOTSHARED,
	SHARE_MATERIAL,          	/* Sharing a material node */
	SHARE_MATERIAL_XSLICE,		/* Sharing a material node, across slice */
	SHARE_SORT,					/* Sharing a sort */
	SHARE_SORT_XSLICE			/* Sharing a sort, across slice */
	/* Other types maybe added later, like sharing a hash */
} ShareType;

#define SHARE_ID_NOT_SHARED (-1)
#define SHARE_ID_NOT_ASSIGNED (-2)

extern int get_plan_share_id(Plan *p);
extern void set_plan_share_id(Plan *p, int share_id);
extern ShareType get_plan_share_type (Plan *p);
extern void set_plan_share_type(Plan *p, ShareType st);
extern void set_plan_share_type_xslice(Plan *p);
extern int get_plan_driver_slice(Plan *p);
extern void set_plan_driver_slice(Plan *P, int slice);
extern void incr_plan_nsharer_xslice(Plan *p);

/* ----------------
 *		shareinputscan node
 * ----------------
 */
typedef struct ShareInputScan
{
	Scan 		scan; /* The ShareInput */

	ShareType 	share_type;
	int 		share_id;
	int 		driver_slice;   	/* slice id that will execute the underlying material/sort */
} ShareInputScan;

/* ----------------
 *		materialization node
 * ----------------
 */
typedef struct Material
{
	Plan		plan;
	bool		cdb_strict;
	bool		cdb_shield_child_from_rescans;

	/* Material can be shared */
	ShareType 	share_type;
	int 		share_id;
	int         driver_slice; 					/* slice id that will execute this material */
	int         nsharer;						/* number of sharer */
	int 		nsharer_xslice;					/* number of sharer cross slice */
} Material;

/* ----------------
 *		sort node
 * ----------------
 */
typedef struct Sort
{
	Plan		plan;
	int			numCols;		/* number of sort-key columns */
	AttrNumber *sortColIdx;		/* their indexes in the target list */
	Oid		   *sortOperators;	/* OIDs of operators to sort them by */
	Oid		   *collations;		/* OIDs of collations */
	bool	   *nullsFirst;		/* NULLS FIRST/LAST directions */
    /* CDB */
	bool		noduplicates;   /* TRUE if sort should discard duplicates */

	/* Sort node can be shared */
	ShareType 	share_type;
	int 		share_id;
	int 		driver_slice;   /* slice id that will execute this sort */
	int         nsharer;        /* number of sharer */
	int 		nsharer_xslice;					/* number of sharer cross slice */
} Sort;

/* ---------------
 *		aggregate node
 *
 * An Agg node implements plain or grouped aggregation.  For grouped
 * aggregation, we can work with presorted input or unsorted input;
 * the latter strategy uses an internal hashtable.
 *
 * Notice the lack of any direct info about the aggregate functions to be
 * computed.  They are found by scanning the node's tlist and quals during
 * executor startup.  (It is possible that there are no aggregate functions;
 * this could happen if they get optimized away by constant-folding, or if
 * we are using the Agg node to implement hash-based grouping.)
 * ---------------
 */
typedef enum AggStrategy
{
	AGG_PLAIN,					/* simple agg across all input rows */
	AGG_SORTED,					/* grouped agg, input must be sorted */
	AGG_HASHED					/* grouped agg, use internal hashtable */
} AggStrategy;

typedef struct Agg
{
	Plan		plan;
	AggStrategy aggstrategy;
	int			numCols;		/* number of grouping columns */
	AttrNumber *grpColIdx;		/* their indexes in the target list */
	bool		combineStates;	/* input tuples contain transition states */
	bool		finalizeAggs;	/* should we call the finalfn on agg states? */
	Oid		   *grpOperators;	/* equality operators to compare with */
	long		numGroups;		/* estimated number of groups in input */
	int			transSpace;		/* est storage per group for byRef transition values */

	/*
	 * The following is used by ROLLUP.
	 */

	/*
	 * The number of grouping columns for this node whose values should be null for
	 * this Agg node.
	 */
	int         numNullCols;

    /*
	 * Indicate the GROUPING value of input tuples for this Agg node.
	 * For example of ROLLUP(a,b,c), there are four Agg nodes:
	 *
	 *   Agg(a,b,c) ==> Agg(a,b) ==> Agg(a) ==> Agg()
	 *
	 * The GROUPING value of input tuples for Agg(a,b,c) is 0, and the values
	 * for Agg(a,b), Agg(a), Agg() are 0, 1, 3, respectively.
	 *
	 * We also use the value "-1" to indicate an Agg node is the final
	 * one that brings back all rollup results from different segments. This final
	 * Agg node is very similar to the non-rollup Agg node, except that we need
	 * a way to know this to properly set GROUPING value during execution.
	 *
	 * For a non-rollup Agg node, this value is 0.
	 */
	uint64 inputGrouping;

	/* The value of GROUPING for this rollup-aware node. */
	uint64 grouping;

	/*
	 * Indicate if input tuples contain values for GROUPING column.
	 *
	 * This is used to determine if the node that generates inputs
	 * for this Agg node is also an Agg node. That is, this Agg node
	 * is one of the list of Agg nodes for a ROLLUP. One exception is
	 * the first Agg node in the list, whose inputs do not have a
	 * GROUPING column.
	 */
	bool inputHasGrouping;

	/*
	 * How many times the aggregates in this rollup level will be output
	 * for a given query. Used only for ROLLUP queries.
	 */
	int         rollupGSTimes;

	/*
	 * Indicate if this Agg node is the last one in a rollup.
	 */
	bool        lastAgg;

	/* Stream entries when out of memory instead of spilling to disk */
	bool 		streaming;
	Bitmapset  *aggParams;		/* IDs of Params used in Aggref inputs */
	/* Note: planner provides numGroups & aggParams only in AGG_HASHED case */
} Agg;

/* ----------------
 *		window aggregate node
 *
 * A WindowAgg node implements window functions over zero or more
 * ordering/framing specifications within a partition specification on
 * appropriately ordered input.
 *
 * For example, if there are window functions
 *
 *   over (partition by a,b orderby c) and
 *   over (partition by a,b order by c,d,e)
 *
 * then the input (outer plan) of the window node will be sorted by
 * (a,b,c,d,e) -- the common partition key (a,b) and the partial
 * ordering keys (c) and (d,e).
 *
 * A Window node contains no direct information about the window
 * functions it computes.  Those functions are found by scanning
 * the node's targetlist for WindowFunc nodes during executor startup.
 * There need not be any, but there's no good reason for the planner
 * to construct a WindowAgg node without at least one WindowFunc.
 *
 * A WindowFunc is related to its WindowAgg node by the fact that it is
 * contained by it.
 *
 * ----------------
 */
typedef struct WindowAgg
{
	Plan		plan;
	Index		winref;			/* ID referenced by window functions */
	int			partNumCols;	/* number of columns in partition clause */
	AttrNumber *partColIdx;		/* their indexes in the target list */
	Oid		   *partOperators;	/* equality operators for partition columns */
	int			ordNumCols;		/* number of columns in ordering clause */
	AttrNumber *ordColIdx;		/* their indexes in the target list */
	Oid		   *ordOperators;	/* equality operators for ordering columns */

	/*
	 * GPDB: Information on the first ORDER BY column. This is different from
	 * simply taking the first element of the ordColIdx/ordOperators fields,
	 * because those arrays don't include any columns that are also present
	 * in the PARTITION BY. For example, in "OVER (PARTITION BY foo ORDER BY
	 * foo, bar)", ordColIdx/ordOperators would not include column 'foo'. But
	 * for computing with RANGE BETWEEN values correctly, we need the first
	 * actual ORDER BY column, even if it's redundant with the PARTITION BY.
	 * firstOrder* has that information. Also, we need a sort operator, not
	 * equality operator, here.
	 */
	AttrNumber	firstOrderCol;
	Oid			firstOrderCmpOperator; /* ordering op */
	bool		firstOrderNullsFirst;

	int			frameOptions;	/* frame_clause options, see WindowDef */
	Node	   *startOffset;	/* expression for starting bound, if any */
	Node	   *endOffset;		/* expression for ending bound, if any */
} WindowAgg;

/* ----------------
 *		unique node
 * ----------------
 */
typedef struct Unique
{
	Plan		plan;
	int			numCols;		/* number of columns to check for uniqueness */
	AttrNumber *uniqColIdx;		/* their indexes in the target list */
	Oid		   *uniqOperators;	/* equality operators to compare with */
} Unique;

/* ----------------
 *		hash build node
 *
 * If the executor is supposed to try to apply skew join optimization, then
 * skewTable/skewColumn/skewInherit identify the outer relation's join key
 * column, from which the relevant MCV statistics can be fetched.  Also, its
 * type information is provided to save a lookup.
 * ----------------
 */
typedef struct Hash
{
	Plan		plan;
	bool		rescannable;            /* CDB: true => save rows for rescan */
	Oid			skewTable;		/* outer join key's table OID, or InvalidOid */
	AttrNumber	skewColumn;		/* outer join key's column #, or zero */
	bool		skewInherit;	/* is outer join rel an inheritance tree? */
	Oid			skewColType;	/* datatype of the outer key column */
	int32		skewColTypmod;	/* typmod of the outer key column */
	/* all other info is in the parent HashJoin node */
} Hash;

/* ----------------
 *		setop node
 * ----------------
 */
typedef enum SetOpCmd
{
	SETOPCMD_INTERSECT,
	SETOPCMD_INTERSECT_ALL,
	SETOPCMD_EXCEPT,
	SETOPCMD_EXCEPT_ALL
} SetOpCmd;

typedef enum SetOpStrategy
{
	SETOP_SORTED,				/* input must be sorted */
	SETOP_HASHED				/* use internal hashtable */
} SetOpStrategy;

typedef struct SetOp
{
	Plan		plan;
	SetOpCmd	cmd;			/* what to do */
	SetOpStrategy strategy;		/* how to do it */
	int			numCols;		/* number of columns to check for
								 * duplicate-ness */
	AttrNumber *dupColIdx;		/* their indexes in the target list */
	Oid		   *dupOperators;	/* equality operators to compare with */
	AttrNumber	flagColIdx;		/* where is the flag column, if any */
	int			firstFlag;		/* flag value for first input relation */
	long		numGroups;		/* estimated number of groups in input */
} SetOp;

/* ----------------
 *		lock-rows node
 *
 * rowMarks identifies the rels to be locked by this node; it should be
 * a subset of the rowMarks listed in the top-level PlannedStmt.
 * epqParam is a Param that all scan nodes below this one must depend on.
 * It is used to force re-evaluation of the plan during EvalPlanQual.
 * ----------------
 */
typedef struct LockRows
{
	Plan		plan;
	List	   *rowMarks;		/* a list of PlanRowMark's */
	int			epqParam;		/* ID of Param for EvalPlanQual re-eval */
} LockRows;

/* ----------------
 *		limit node
 *
 * Note: as of Postgres 8.2, the offset and count expressions are expected
 * to yield int8, rather than int4 as before.
 * ----------------
 */
typedef struct Limit
{
	Plan		plan;
	Node	   *limitOffset;	/* OFFSET parameter, or NULL if none */
	Node	   *limitCount;		/* COUNT parameter, or NULL if none */
} Limit;

/* -------------------------
 *		motion node structs
 * -------------------------
 */
typedef enum MotionType
{
	MOTIONTYPE_HASH,		/* Use hashing to select a segindex destination */
	MOTIONTYPE_FIXED,		/* Send tuples to a fixed set of segindexes */
	MOTIONTYPE_EXPLICIT		/* Send tuples to the segment explicitly specified in their segid column */
} MotionType;

/*
 * Motion Node
 *
 */
typedef struct Motion
{
	Plan		plan;

	MotionType  motionType;
	bool		sendSorted;			/* if true, output should be sorted */
	int			motionID;			/* required by AMS  */

	/* For Hash */
	List		*hashExprs;			/* list of hash expressions */
	Oid			*hashFuncs;			/* corresponding hash functions */

	/*
	 * The isBroadcast field is only used for motionType=MOTIONTYPE_FIXED,
	 * if it is other kind of motion, please do not access this field.
	 * The field is set true for Broadcast motion, and set false for
	 * Gather motion.
	 *
	 * TODO: Historically, broadcast motion and gather motion's motiontype
	 * are both MOTIONTYPE_FIXED. It is not a good idea. They should belong
	 * to different motiontypes. We should refactor the motion types in future.
	 */
	bool 	  	isBroadcast;

	/* For Explicit */
	AttrNumber segidColIdx;			/* index of the segid column in the target list */

	/* The following field is only used when sendSorted == true */
	int			numSortCols;	/* number of sort-key columns */
	AttrNumber *sortColIdx;		/* their indexes in the target list */
	Oid		   *sortOperators;	/* OIDs of operators to sort them by */
	Oid		   *collations;		/* OIDs of collations */
	bool	   *nullsFirst;		/* NULLS FIRST/LAST directions */
} Motion;

/*
 * DML Node
 */
typedef struct DML
{
	Plan		plan;
	Index		scanrelid;		/* index into the range table */
	AttrNumber	actionColIdx;	/* index of action column into the target list */
	AttrNumber	ctidColIdx;		/* index of ctid column into the target list */
	AttrNumber	tupleoidColIdx;	/* index of tuple oid column into the target list */

} DML;

/*
 * SplitUpdate Node
 *
 */
typedef struct SplitUpdate
{
	Plan		plan;
	AttrNumber	actionColIdx;		/* index of action column into the target list */
	AttrNumber	ctidColIdx;			/* index of ctid column into the target list */
	AttrNumber	tupleoidColIdx;		/* index of tuple oid column into the target list */
	List		*insertColIdx;		/* list of columns to INSERT into the target list */
	List		*deleteColIdx;		/* list of columns to DELETE into the target list */
} SplitUpdate;

/*
 * AssertOp Node
 *
 */
typedef struct AssertOp
{
	Plan 			plan;
	int				errcode;		/* SQL error code */
	List 			*errmessage;	/* error message */

} AssertOp;

/*
 * RowTrigger Node
 *
 */
typedef struct RowTrigger
{
	Plan		plan;
	Oid			relid;				/* OID of target relation */
	int 		eventFlags;			/* TriggerEvent bit flags (see trigger.h).*/
	List		*oldValuesColIdx;	/* list of old columns */
	List		*newValuesColIdx;	/* list of new columns */

} RowTrigger;

/*
 * RowMarkType -
 *	  enums for types of row-marking operations
 *
 * The first four of these values represent different lock strengths that
 * we can take on tuples according to SELECT FOR [KEY] UPDATE/SHARE requests.
 * We only support these on regular tables.  For foreign tables, any locking
 * that might be done for these requests must happen during the initial row
 * fetch; there is no mechanism for going back to lock a row later (and thus
 * no need for EvalPlanQual machinery during updates of foreign tables).
 * This means that the semantics will be a bit different than for a local
 * table; in particular we are likely to lock more rows than would be locked
 * locally, since remote rows will be locked even if they then fail
 * locally-checked restriction or join quals.  However, the alternative of
 * doing a separate remote query to lock each selected row is extremely
 * unappealing, so let's do it like this for now.
 *
 * When doing UPDATE, DELETE, or SELECT FOR UPDATE/SHARE, we have to uniquely
 * identify all the source rows, not only those from the target relations, so
 * that we can perform EvalPlanQual rechecking at need.  For plain tables we
 * can just fetch the TID, much as for a target relation; this case is
 * represented by ROW_MARK_REFERENCE.  Otherwise (for example for VALUES or
 * FUNCTION scans) we have to copy the whole row value.  ROW_MARK_COPY is
 * pretty inefficient, since most of the time we'll never need the data; but
 * fortunately the case is not performance-critical in practice.  Note that
 * we use ROW_MARK_COPY for non-target foreign tables, even if the FDW has a
 * concept of rowid and so could theoretically support some form of
 * ROW_MARK_REFERENCE.  Although copying the whole row value is inefficient,
 * it's probably still faster than doing a second remote fetch, so it doesn't
 * seem worth the extra complexity to permit ROW_MARK_REFERENCE.
 */
typedef enum RowMarkType
{
	ROW_MARK_EXCLUSIVE,			/* obtain exclusive tuple lock */
	ROW_MARK_NOKEYEXCLUSIVE,	/* obtain no-key exclusive tuple lock */
	ROW_MARK_SHARE,				/* obtain shared tuple lock */
	ROW_MARK_KEYSHARE,			/* obtain keyshare tuple lock */
	ROW_MARK_REFERENCE,			/* just fetch the TID */
	ROW_MARK_COPY,				/* physically copy the row value */
	ROW_MARK_TABLE_SHARE,		/* (GPDB) Acquire RowShareLock on table,
								 * but no tuple locks */
	ROW_MARK_TABLE_EXCLUSIVE	/* (GPDB) Acquire ExclusiveLock on table,
								 * blocking all other updates */
} RowMarkType;

#define RowMarkRequiresRowShareLock(marktype)  ((marktype) <= ROW_MARK_KEYSHARE)

/*
 * PlanRowMark -
 *	   plan-time representation of FOR [KEY] UPDATE/SHARE clauses
 *
 * When doing UPDATE, DELETE, or SELECT FOR UPDATE/SHARE, we create a separate
 * PlanRowMark node for each non-target relation in the query.  Relations that
 * are not specified as FOR UPDATE/SHARE are marked ROW_MARK_REFERENCE (if
 * regular tables) or ROW_MARK_COPY (if not).
 *
 * Initially all PlanRowMarks have rti == prti and isParent == false.
 * When the planner discovers that a relation is the root of an inheritance
 * tree, it sets isParent true, and adds an additional PlanRowMark to the
 * list for each child relation (including the target rel itself in its role
 * as a child).  The child entries have rti == child rel's RT index and
 * prti == parent's RT index, and can therefore be recognized as children by
 * the fact that prti != rti.
 *
 * The planner also adds resjunk output columns to the plan that carry
 * information sufficient to identify the locked or fetched rows.  For
 * regular tables (markType != ROW_MARK_COPY), these columns are named
 *		tableoid%u			OID of table
 *		ctid%u				TID of row
 * The tableoid column is only present for an inheritance hierarchy.
 * When markType == ROW_MARK_COPY, there is instead a single column named
 *		wholerow%u			whole-row value of relation
 * In all three cases, %u represents the rowmark ID number (rowmarkId).
 * This number is unique within a plan tree, except that child relation
 * entries copy their parent's rowmarkId.  (Assigning unique numbers
 * means we needn't renumber rowmarkIds when flattening subqueries, which
 * would require finding and renaming the resjunk columns as well.)
 * Note this means that all tables in an inheritance hierarchy share the
 * same resjunk column names.  However, in an inherited UPDATE/DELETE the
 * columns could have different physical column numbers in each subplan.
 */
typedef struct PlanRowMark
{
	NodeTag		type;
	Index		rti;			/* range table index of markable relation */
	Index		prti;			/* range table index of parent relation */
	Index		rowmarkId;		/* unique identifier for resjunk columns */
	RowMarkType markType;		/* see enum above */
	bool		noWait;			/* NOWAIT option */
	bool		isParent;		/* true if this is a "dummy" parent entry */
} PlanRowMark;


/*
 * Plan invalidation info
 *
 * We track the objects on which a PlannedStmt depends in two ways:
 * relations are recorded as a simple list of OIDs, and everything else
 * is represented as a list of PlanInvalItems.  A PlanInvalItem is designed
 * to be used with the syscache invalidation mechanism, so it identifies a
 * system catalog entry by cache ID and hash value.
 */
typedef struct PlanInvalItem
{
	NodeTag		type;
	int			cacheId;		/* a syscache ID, see utils/syscache.h */
	uint32		hashValue;		/* hash value of object's cache lookup key */
} PlanInvalItem;

/* ----------------
 * PartitionSelector node
 *
 * PartitionSelector finds a set of leaf partition OIDs given the root table
 * OID and optionally selection predicates.
 *
 * It hides the logic of partition selection and propagation instead of
 * polluting the plan with it to make a plan look consistent and easy to
 * understand. It will be easy to locate where partition selection happens
 * in a plan.
 *
 * A PartitionSelection can work in three different ways:
 *
 * 1. Dynamic selection, based on tuples that pass through it.
 * 2. Dynamic selection, with a projected tuple.
 * 3. Static selection, performed at beginning of execution.
 *
 * ----------------
 */
typedef struct PartitionSelector
{
	Plan		plan;
	Oid 		relid;  				/* OID of target relation */
	int 		nLevels;				/* number of partition levels */
	int32 		scanId; 				/* id of the corresponding dynamic scan */
	int32		selectorId;				/* id of this partition selector */

	/* Fields for dynamic selection */
	List		*levelEqExpressions;	/* list of list of equality expressions used for individual levels */
	List		*levelExpressions;  	/* predicates used for individual levels */
	Node		*residualPredicate; 	/* residual predicate (to be applied at the end) */
	Node		*propagationExpression; /* propagation expression */
	Node		*printablePredicate;	/* printable predicate (for explain purposes) */

	/*
	 * Fields for dynamic selection, by projecting input tuples to a tuple
	 * that has the partitioning key values in the same positions as in the
	 * partitioned table.
	 */
	List	    *partTabTargetlist;

	/* Fields for static selection */
	bool		staticSelection;    	/* static selection performed? */
	List		*staticPartOids;    	/* list of statically selected parts */
	List		*staticScanIds;     	/* scan ids used to propagate statically selected part oids */

} PartitionSelector;

#endif   /* PLANNODES_H */
