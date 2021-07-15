/*-------------------------------------------------------------------------
 *
 * copyfuncs.c
 *	  Copy functions for Postgres tree nodes.
 *
 * NOTE: we currently support copying all node types found in parse and
 * plan trees.  We do not support copying executor state trees; there
 * is no need for that, and no point in maintaining all the code that
 * would be needed.  We also do not support copying Path trees, mainly
 * because the circular linkages between RelOptInfo and Path nodes can't
 * be handled easily in a simple depth-first traversal.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/nodes/copyfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/gp_policy.h"
#include "catalog/heap.h"
#include "miscadmin.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "utils/datum.h"
#include "cdb/cdbgang.h"


/*
 * Macros to simplify copying of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.  Note that these
 * hard-wire the convention that the local variables in a Copy routine are
 * named 'newnode' and 'from'.
 */

/* Copy a simple scalar field (int, float, bool, enum, etc) */
#define COPY_SCALAR_FIELD(fldname) \
	(newnode->fldname = from->fldname)

/* Copy a field that is a pointer to some kind of Node or Node tree */
#define COPY_NODE_FIELD(fldname) \
	(newnode->fldname = copyObject(from->fldname))

/* Copy a field that is a pointer to a Bitmapset */
#define COPY_BITMAPSET_FIELD(fldname) \
	(newnode->fldname = bms_copy(from->fldname))

/* Copy a field that is a pointer to a C string, or perhaps NULL */
#define COPY_STRING_FIELD(fldname) \
	(newnode->fldname = from->fldname ? pstrdup(from->fldname) : (char *) NULL)

/* Copy a field that is a pointer to a simple palloc'd object of size sz */
#define COPY_POINTER_FIELD(fldname, sz) \
	do { \
		Size	_size = (sz); \
		newnode->fldname = palloc(_size); \
		memcpy(newnode->fldname, from->fldname, _size); \
	} while (0)

#define COPY_BINARY_FIELD(fldname, sz) \
	do { \
		Size _size = (sz); \
		memcpy(&newnode->fldname, &from->fldname, _size); \
	} while (0)

/* Copy a field that is a varlena datum */
#define COPY_VARLENA_FIELD(fldname, len) \
	do { \
		if (from->fldname) \
		{ \
			newnode->fldname = (bytea *) DatumGetPointer( \
					datumCopy(PointerGetDatum(from->fldname), false, len)); \
		} \
	} while (0)

/* Copy a parse location field (for Copy, this is same as scalar case) */
#define COPY_LOCATION_FIELD(fldname) \
	(newnode->fldname = from->fldname)


/* ****************************************************************
 *					 plannodes.h copy functions
 * ****************************************************************
 */

/*
 * _copyPlannedStmt
 */
static PlannedStmt *
_copyPlannedStmt(const PlannedStmt *from)
{
	PlannedStmt *newnode = makeNode(PlannedStmt);

	COPY_SCALAR_FIELD(commandType);
	COPY_SCALAR_FIELD(planGen);
	COPY_SCALAR_FIELD(queryId);
	COPY_SCALAR_FIELD(hasReturning);
	COPY_SCALAR_FIELD(hasModifyingCTE);
	COPY_SCALAR_FIELD(canSetTag);
	COPY_SCALAR_FIELD(transientPlan);
	COPY_SCALAR_FIELD(oneoffPlan);
	COPY_SCALAR_FIELD(simplyUpdatable);
	COPY_NODE_FIELD(planTree);
	COPY_NODE_FIELD(rtable);
	COPY_NODE_FIELD(resultRelations);
	COPY_NODE_FIELD(utilityStmt);
	COPY_NODE_FIELD(subplans);
	COPY_BITMAPSET_FIELD(rewindPlanIDs);

	COPY_NODE_FIELD(result_partitions);
	COPY_NODE_FIELD(result_aosegnos);
	COPY_NODE_FIELD(queryPartOids);
	COPY_NODE_FIELD(queryPartsMetadata);
	COPY_NODE_FIELD(numSelectorsPerScanId);
	COPY_NODE_FIELD(rowMarks);
	COPY_NODE_FIELD(relationOids);
	COPY_NODE_FIELD(invalItems);
	COPY_SCALAR_FIELD(nParamExec);
	COPY_SCALAR_FIELD(nMotionNodes);
	COPY_SCALAR_FIELD(nInitPlans);

	COPY_NODE_FIELD(intoPolicy);

	COPY_SCALAR_FIELD(query_mem);

	COPY_NODE_FIELD(intoClause);
	COPY_NODE_FIELD(copyIntoClause);
	COPY_NODE_FIELD(refreshClause);
	COPY_SCALAR_FIELD(metricsQueryType);

	return newnode;
}

static QueryDispatchDesc *
_copyQueryDispatchDesc(const QueryDispatchDesc *from)
{
	QueryDispatchDesc *newnode = makeNode(QueryDispatchDesc);

	COPY_NODE_FIELD(sliceTable);
	COPY_NODE_FIELD(oidAssignments);
	COPY_NODE_FIELD(cursorPositions);
	COPY_SCALAR_FIELD(useChangedAOOpts);

	return newnode;
}

static OidAssignment *
_copyOidAssignment(const OidAssignment *from)
{
	OidAssignment *newnode = makeNode(OidAssignment);

	COPY_SCALAR_FIELD(catalog);
	COPY_STRING_FIELD(objname);
	COPY_SCALAR_FIELD(namespaceOid);
	COPY_SCALAR_FIELD(keyOid1);
	COPY_SCALAR_FIELD(keyOid2);
	COPY_SCALAR_FIELD(oid);

	return newnode;
}

/*
 * CopyPlanFields
 *
 *		This function copies the fields of the Plan node.  It is used by
 *		all the copy functions for classes which inherit from Plan.
 */
static void
CopyPlanFields(const Plan *from, Plan *newnode)
{
	COPY_SCALAR_FIELD(plan_node_id);

	COPY_SCALAR_FIELD(startup_cost);
	COPY_SCALAR_FIELD(total_cost);
	COPY_SCALAR_FIELD(plan_rows);
	COPY_SCALAR_FIELD(plan_width);
	COPY_NODE_FIELD(targetlist);
	COPY_NODE_FIELD(qual);
	COPY_NODE_FIELD(lefttree);
	COPY_NODE_FIELD(righttree);
	COPY_NODE_FIELD(initPlan);
	COPY_BITMAPSET_FIELD(extParam);
	COPY_BITMAPSET_FIELD(allParam);
	COPY_NODE_FIELD(flow);
	COPY_SCALAR_FIELD(dispatch);
	COPY_SCALAR_FIELD(nMotionNodes);
	COPY_SCALAR_FIELD(nInitPlans);
	COPY_NODE_FIELD(sliceTable);

	COPY_SCALAR_FIELD(directDispatch.isDirectDispatch);
	COPY_NODE_FIELD(directDispatch.contentIds);
	COPY_SCALAR_FIELD(operatorMemKB);
	/*
	 * Don't copy memoryAccountId and this is an index to the account array
	 * specific to this process only.
	 */
}

/*
 * CopyLogicalIndexInfo
 *
 *		This function copies the LogicalIndexInfo, which is part of
 *		DynamicIndexScan node.
 */
static LogicalIndexInfo *
CopyLogicalIndexInfo(const LogicalIndexInfo *from)
{
	LogicalIndexInfo *newnode = palloc(sizeof(LogicalIndexInfo));

	COPY_SCALAR_FIELD(logicalIndexOid);
	COPY_SCALAR_FIELD(nColumns);
	COPY_POINTER_FIELD(indexKeys, from->nColumns * sizeof(AttrNumber));
	COPY_NODE_FIELD(indPred);
	COPY_NODE_FIELD(indExprs);
	COPY_SCALAR_FIELD(indIsUnique);
	COPY_SCALAR_FIELD(indType);
	COPY_NODE_FIELD(partCons);
	COPY_NODE_FIELD(defaultLevels);

	return newnode;
}

/*
 * _copyPlan
 */
static Plan *
_copyPlan(const Plan *from)
{
	Plan	   *newnode = makeNode(Plan);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields(from, newnode);

	return newnode;
}


/*
 * _copyResult
 */
static Result *
_copyResult(const Result *from)
{
	Result	   *newnode = makeNode(Result);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(resconstantqual);

	COPY_SCALAR_FIELD(numHashFilterCols);
	if (from->numHashFilterCols > 0)
	{
		COPY_POINTER_FIELD(hashFilterColIdx, from->numHashFilterCols * sizeof(AttrNumber));
		COPY_POINTER_FIELD(hashFilterFuncs, from->numHashFilterCols * sizeof(Oid));
	}

	return newnode;
}

/*
 * _copyRepeat
 */
static Repeat *
_copyRepeat(const Repeat *from)
{
	Repeat *newnode = makeNode(Repeat);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *)from, (Plan *)newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(repeatCountExpr);
	COPY_SCALAR_FIELD(grouping);

	return newnode;
}

/*
 * _copyModifyTable
 */
static ModifyTable *
_copyModifyTable(const ModifyTable *from)
{
	ModifyTable *newnode = makeNode(ModifyTable);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(operation);
	COPY_SCALAR_FIELD(canSetTag);
	COPY_NODE_FIELD(resultRelations);
	COPY_SCALAR_FIELD(resultRelIndex);
	COPY_NODE_FIELD(plans);
	COPY_NODE_FIELD(withCheckOptionLists);
	COPY_NODE_FIELD(returningLists);
	COPY_NODE_FIELD(fdwPrivLists);
	COPY_NODE_FIELD(rowMarks);
	COPY_SCALAR_FIELD(epqParam);
	COPY_NODE_FIELD(action_col_idxes);
	COPY_NODE_FIELD(ctid_col_idxes);
	COPY_NODE_FIELD(oid_col_idxes);

	return newnode;
}

/*
 * _copyAppend
 */
static Append *
_copyAppend(const Append *from)
{
	Append	   *newnode = makeNode(Append);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(appendplans);

	return newnode;
}

static Sequence *
_copySequence(const Sequence *from)
{
	Sequence *newnode = makeNode(Sequence);
	CopyPlanFields((Plan *) from, (Plan *) newnode);
	COPY_NODE_FIELD(subplans);

	return newnode;
}

/*
 * _copyMergeAppend
 */
static MergeAppend *
_copyMergeAppend(const MergeAppend *from)
{
	MergeAppend *newnode = makeNode(MergeAppend);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(mergeplans);
	COPY_SCALAR_FIELD(numCols);
	COPY_POINTER_FIELD(sortColIdx, from->numCols * sizeof(AttrNumber));
	COPY_POINTER_FIELD(sortOperators, from->numCols * sizeof(Oid));
	COPY_POINTER_FIELD(collations, from->numCols * sizeof(Oid));
	COPY_POINTER_FIELD(nullsFirst, from->numCols * sizeof(bool));

	return newnode;
}

/*
 * _copyRecursiveUnion
 */
static RecursiveUnion *
_copyRecursiveUnion(const RecursiveUnion *from)
{
	RecursiveUnion *newnode = makeNode(RecursiveUnion);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(wtParam);
	COPY_SCALAR_FIELD(numCols);
	if (from->numCols > 0)
	{
		COPY_POINTER_FIELD(dupColIdx, from->numCols * sizeof(AttrNumber));
		COPY_POINTER_FIELD(dupOperators, from->numCols * sizeof(Oid));
	}
	COPY_SCALAR_FIELD(numGroups);

	return newnode;
}

/*
 * _copyBitmapAnd
 */
static BitmapAnd *
_copyBitmapAnd(const BitmapAnd *from)
{
	BitmapAnd  *newnode = makeNode(BitmapAnd);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(bitmapplans);

	return newnode;
}

/*
 * _copyBitmapOr
 */
static BitmapOr *
_copyBitmapOr(const BitmapOr *from)
{
	BitmapOr   *newnode = makeNode(BitmapOr);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(bitmapplans);

	return newnode;
}


/*
 * CopyScanFields
 *
 *		This function copies the fields of the Scan node.  It is used by
 *		all the copy functions for classes which inherit from Scan.
 */
static void
CopyScanFields(const Scan *from, Scan *newnode)
{
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(scanrelid);
}

/*
 * _copyScan
 */
static Scan *
_copyScan(const Scan *from)
{
	Scan	   *newnode = makeNode(Scan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	return newnode;
}

/*
 * _copySeqScan
 */
static SeqScan *
_copySeqScan(const SeqScan *from)
{
	SeqScan    *newnode = makeNode(SeqScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	return newnode;
}

static DynamicSeqScan *
_copyDynamicSeqScan(const DynamicSeqScan *from)
{
	DynamicSeqScan *newnode = makeNode(DynamicSeqScan);

	CopyScanFields((Scan *) from, (Scan *) newnode);
	COPY_SCALAR_FIELD(partIndex);
	COPY_SCALAR_FIELD(partIndexPrintable);

	return newnode;
}

/*
 * _copyExternalScan
 */
static ExternalScan *
_copyExternalScan(const ExternalScan *from)
{
	ExternalScan    *newnode = makeNode(ExternalScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(uriList);
	COPY_STRING_FIELD(fmtOptString);
	COPY_SCALAR_FIELD(fmtType);
	COPY_SCALAR_FIELD(isMasterOnly);
	COPY_SCALAR_FIELD(rejLimit);
	COPY_SCALAR_FIELD(rejLimitInRows);
	COPY_SCALAR_FIELD(logErrors);
	COPY_SCALAR_FIELD(encoding);
	COPY_SCALAR_FIELD(scancounter);

	return newnode;
}

static void
CopyIndexScanFields(const IndexScan *from, IndexScan *newnode)
{
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(indexid);
	COPY_NODE_FIELD(indexqual);
	COPY_NODE_FIELD(indexqualorig);
	COPY_NODE_FIELD(indexorderby);
	COPY_NODE_FIELD(indexorderbyorig);
	COPY_SCALAR_FIELD(indexorderdir);
}

/*
 * _copyIndexScan
 */
static IndexScan *
_copyIndexScan(const IndexScan *from)
{
	IndexScan  *newnode = makeNode(IndexScan);

	CopyIndexScanFields(from, newnode);

	return newnode;
}

/*
 * _copyDynamicIndexScan
 */
static DynamicIndexScan *
_copyDynamicIndexScan(const DynamicIndexScan *from)
{
	DynamicIndexScan  *newnode = makeNode(DynamicIndexScan);

	/* DynamicIndexScan has some content from IndexScan */
	CopyIndexScanFields(&from->indexscan, &newnode->indexscan);
	COPY_SCALAR_FIELD(partIndex);
	COPY_SCALAR_FIELD(partIndexPrintable);
	newnode->logicalIndexInfo = CopyLogicalIndexInfo(from->logicalIndexInfo);

	return newnode;
}

/*
 * _copyIndexOnlyScan
 */
static IndexOnlyScan *
_copyIndexOnlyScan(const IndexOnlyScan *from)
{
	IndexOnlyScan *newnode = makeNode(IndexOnlyScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(indexid);
	COPY_NODE_FIELD(indexqual);
	COPY_NODE_FIELD(indexqualorig);
	COPY_NODE_FIELD(indexorderby);
	COPY_NODE_FIELD(indextlist);
	COPY_SCALAR_FIELD(indexorderdir);

	return newnode;
}

/*
 * _copyBitmapIndexScan
 */
static void
CopyBitmapIndexScanFields(const BitmapIndexScan *from, BitmapIndexScan *newnode)
{
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	COPY_SCALAR_FIELD(indexid);
	COPY_NODE_FIELD(indexqual);
	COPY_NODE_FIELD(indexqualorig);
}

static BitmapIndexScan *
_copyBitmapIndexScan(const BitmapIndexScan *from)
{
	BitmapIndexScan *newnode = makeNode(BitmapIndexScan);

	CopyBitmapIndexScanFields(from, newnode);

	return newnode;
}

/*
 * _copyDynamicBitmapIndexScan
 */
static DynamicBitmapIndexScan *
_copyDynamicBitmapIndexScan(const DynamicBitmapIndexScan *from)
{
	DynamicBitmapIndexScan *newnode = makeNode(DynamicBitmapIndexScan);

	CopyBitmapIndexScanFields(&from->biscan, &newnode->biscan);
	COPY_SCALAR_FIELD(partIndex);
	COPY_SCALAR_FIELD(partIndexPrintable);
	newnode->logicalIndexInfo = CopyLogicalIndexInfo(from->logicalIndexInfo);

	return newnode;
}

static void
CopyBitmapHeapScanFields(const BitmapHeapScan *from, BitmapHeapScan *newnode)
{
	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(bitmapqualorig);
}

/*
 * _copyBitmapHeapScan
 */
static BitmapHeapScan *
_copyBitmapHeapScan(const BitmapHeapScan *from)
{
	BitmapHeapScan *newnode = makeNode(BitmapHeapScan);

	CopyBitmapHeapScanFields(from, newnode);

	return newnode;
}

/*
 * _copyDynamicBitmapHeapScan
 */
static DynamicBitmapHeapScan *
_copyDynamicBitmapHeapScan(const DynamicBitmapHeapScan *from)
{
	DynamicBitmapHeapScan *newnode = makeNode(DynamicBitmapHeapScan);

	CopyBitmapHeapScanFields(&from->bitmapheapscan, &newnode->bitmapheapscan);
	COPY_SCALAR_FIELD(partIndex);
	COPY_SCALAR_FIELD(partIndexPrintable);

	return newnode;
}

/*
 * _copyTidScan
 */
static TidScan *
_copyTidScan(const TidScan *from)
{
	TidScan    *newnode = makeNode(TidScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(tidquals);

	return newnode;
}

/*
 * _copySubqueryScan
 */
static SubqueryScan *
_copySubqueryScan(const SubqueryScan *from)
{
	SubqueryScan *newnode = makeNode(SubqueryScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(subplan);

	return newnode;
}

/*
 * _copyFunctionScan
 */
static FunctionScan *
_copyFunctionScan(const FunctionScan *from)
{
	FunctionScan *newnode = makeNode(FunctionScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(functions);
	COPY_SCALAR_FIELD(funcordinality);
	COPY_NODE_FIELD(param);
	COPY_SCALAR_FIELD(resultInTupleStore);
	COPY_SCALAR_FIELD(initplanId);

	return newnode;
}

/*
 * _copyTableFunctionScan
 */
static TableFunctionScan *
_copyTableFunctionScan(const TableFunctionScan *from)
{
	TableFunctionScan	*newnode = makeNode(TableFunctionScan);

	CopyScanFields((const Scan *) from, (Scan *) newnode);
	COPY_NODE_FIELD(function);

	return newnode;
}

/*
 * _copyValuesScan
 */
static ValuesScan *
_copyValuesScan(const ValuesScan *from)
{
	ValuesScan *newnode = makeNode(ValuesScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(values_lists);

	return newnode;
}

/*
 * _copyCteScan
 */
static CteScan *
_copyCteScan(const CteScan *from)
{
	CteScan    *newnode = makeNode(CteScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(ctePlanId);
	COPY_SCALAR_FIELD(cteParam);

	return newnode;
}

/*
 * _copyWorkTableScan
 */
static WorkTableScan *
_copyWorkTableScan(const WorkTableScan *from)
{
	WorkTableScan *newnode = makeNode(WorkTableScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(wtParam);

	return newnode;
}

/*
 * _copyForeignScan
 */
static ForeignScan *
_copyForeignScan(const ForeignScan *from)
{
	ForeignScan *newnode = makeNode(ForeignScan);

	/*
	 * copy node superclass fields
	 */
	CopyScanFields((const Scan *) from, (Scan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(fdw_exprs);
	COPY_NODE_FIELD(fdw_private);
	COPY_SCALAR_FIELD(fsSystemCol);

	return newnode;
}

/*
 * CopyJoinFields
 *
 *		This function copies the fields of the Join node.  It is used by
 *		all the copy functions for classes which inherit from Join.
 */
static void
CopyJoinFields(const Join *from, Join *newnode)
{
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

    COPY_SCALAR_FIELD(prefetch_inner);
	COPY_SCALAR_FIELD(prefetch_joinqual);
	COPY_SCALAR_FIELD(prefetch_qual);

	COPY_SCALAR_FIELD(jointype);
	COPY_NODE_FIELD(joinqual);
}


/*
 * _copyJoin
 */
static Join *
_copyJoin(const Join *from)
{
	Join	   *newnode = makeNode(Join);

	/*
	 * copy node superclass fields
	 */
	CopyJoinFields(from, newnode);

	return newnode;
}


/*
 * _copyNestLoop
 */
static NestLoop *
_copyNestLoop(const NestLoop *from)
{
	NestLoop   *newnode = makeNode(NestLoop);

	/*
	 * copy node superclass fields
	 */
	CopyJoinFields((const Join *) from, (Join *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(nestParams);

    COPY_SCALAR_FIELD(shared_outer);
    COPY_SCALAR_FIELD(singleton_outer); /*CDB-OLAP*/

	return newnode;
}

/*
 * _copyMergeJoin
 */
static MergeJoin *
_copyMergeJoin(const MergeJoin *from)
{
	MergeJoin  *newnode = makeNode(MergeJoin);
	int			numCols;

	/*
	 * copy node superclass fields
	 */
	CopyJoinFields((const Join *) from, (Join *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(mergeclauses);
	numCols = list_length(from->mergeclauses);
	if (numCols > 0)
	{
		COPY_POINTER_FIELD(mergeFamilies, numCols * sizeof(Oid));
		COPY_POINTER_FIELD(mergeCollations, numCols * sizeof(Oid));
		COPY_POINTER_FIELD(mergeStrategies, numCols * sizeof(int));
		COPY_POINTER_FIELD(mergeNullsFirst, numCols * sizeof(bool));
	}
	COPY_SCALAR_FIELD(unique_outer);

	return newnode;
}

/*
 * _copyHashJoin
 */
static HashJoin *
_copyHashJoin(const HashJoin *from)
{
	HashJoin   *newnode = makeNode(HashJoin);

	/*
	 * copy node superclass fields
	 */
	CopyJoinFields((const Join *) from, (Join *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(hashclauses);
	COPY_NODE_FIELD(hashqualclauses);

	return newnode;
}

/*
 * _copyShareInputScan
 */
static ShareInputScan *
_copyShareInputScan(const ShareInputScan *from)
{
	ShareInputScan *newnode = makeNode(ShareInputScan);

	/* copy node superclass fields */
	CopyPlanFields((Plan *) from, (Plan *) newnode);
	COPY_SCALAR_FIELD(share_type);
	COPY_SCALAR_FIELD(share_id);
	COPY_SCALAR_FIELD(driver_slice);

	return newnode;
}


/*
 * _copyMaterial
 */
static Material *
_copyMaterial(const Material *from)
{
	Material   *newnode = makeNode(Material);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);
	COPY_SCALAR_FIELD(cdb_strict);
	COPY_SCALAR_FIELD(cdb_shield_child_from_rescans);
	COPY_SCALAR_FIELD(share_type);
	COPY_SCALAR_FIELD(share_id);
	COPY_SCALAR_FIELD(driver_slice);
	COPY_SCALAR_FIELD(nsharer);
	COPY_SCALAR_FIELD(nsharer_xslice);

    return newnode;
}


/*
 * _copySort
 */
static Sort *
_copySort(const Sort *from)
{
	Sort	   *newnode = makeNode(Sort);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(numCols);
	COPY_POINTER_FIELD(sortColIdx, from->numCols * sizeof(AttrNumber));
	COPY_POINTER_FIELD(sortOperators, from->numCols * sizeof(Oid));
	COPY_POINTER_FIELD(collations, from->numCols * sizeof(Oid));
	COPY_POINTER_FIELD(nullsFirst, from->numCols * sizeof(bool));

    /* CDB */
	COPY_SCALAR_FIELD(noduplicates);

	COPY_SCALAR_FIELD(share_type);
	COPY_SCALAR_FIELD(share_id);
	COPY_SCALAR_FIELD(driver_slice);
	COPY_SCALAR_FIELD(nsharer);
	COPY_SCALAR_FIELD(nsharer_xslice);
	return newnode;
}


/*
 * _copyAgg
 */
static Agg *
_copyAgg(const Agg *from)
{
	Agg		   *newnode = makeNode(Agg);

	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(aggstrategy);
	COPY_SCALAR_FIELD(numCols);
	COPY_SCALAR_FIELD(combineStates);
	COPY_SCALAR_FIELD(finalizeAggs);
	if (from->numCols > 0)
	{
		COPY_POINTER_FIELD(grpColIdx, from->numCols * sizeof(AttrNumber));
		COPY_POINTER_FIELD(grpOperators, from->numCols * sizeof(Oid));
	}
	COPY_SCALAR_FIELD(numGroups);
	COPY_SCALAR_FIELD(transSpace);
	COPY_SCALAR_FIELD(numNullCols);
	COPY_SCALAR_FIELD(inputGrouping);
	COPY_SCALAR_FIELD(grouping);
	COPY_SCALAR_FIELD(inputHasGrouping);
	COPY_SCALAR_FIELD(rollupGSTimes);
	COPY_SCALAR_FIELD(lastAgg);
	COPY_SCALAR_FIELD(streaming);
	COPY_BITMAPSET_FIELD(aggParams);

	return newnode;
}

/*
 * _copyWindowAgg
 */
static WindowAgg *
_copyWindowAgg(const WindowAgg *from)
{
	WindowAgg  *newnode = makeNode(WindowAgg);

	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(winref);
	COPY_SCALAR_FIELD(partNumCols);
	if (from->partNumCols > 0)
	{
		COPY_POINTER_FIELD(partColIdx, from->partNumCols * sizeof(AttrNumber));
		COPY_POINTER_FIELD(partOperators, from->partNumCols * sizeof(Oid));
	}
	COPY_SCALAR_FIELD(ordNumCols);
	if (from->ordNumCols > 0)
	{
		COPY_POINTER_FIELD(ordColIdx, from->ordNumCols * sizeof(AttrNumber));
		COPY_POINTER_FIELD(ordOperators, from->ordNumCols * sizeof(Oid));
	}
	COPY_SCALAR_FIELD(firstOrderCol);
	COPY_SCALAR_FIELD(firstOrderCmpOperator);
	COPY_SCALAR_FIELD(firstOrderNullsFirst);
	COPY_SCALAR_FIELD(frameOptions);
	COPY_NODE_FIELD(startOffset);
	COPY_NODE_FIELD(endOffset);

	return newnode;
}

/*
 * _copyUnique
 */
static Unique *
_copyUnique(const Unique *from)
{
	Unique	   *newnode = makeNode(Unique);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(numCols);
	COPY_POINTER_FIELD(uniqColIdx, from->numCols * sizeof(AttrNumber));
	COPY_POINTER_FIELD(uniqOperators, from->numCols * sizeof(Oid));

	return newnode;
}

/*
 * _copyHash
 */
static Hash *
_copyHash(const Hash *from)
{
	Hash	   *newnode = makeNode(Hash);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(skewTable);
	COPY_SCALAR_FIELD(skewColumn);
	COPY_SCALAR_FIELD(skewInherit);
	COPY_SCALAR_FIELD(skewColType);
	COPY_SCALAR_FIELD(skewColTypmod);

	return newnode;
}

/*
 * _copySetOp
 */
static SetOp *
_copySetOp(const SetOp *from)
{
	SetOp	   *newnode = makeNode(SetOp);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_SCALAR_FIELD(cmd);
	COPY_SCALAR_FIELD(strategy);
	COPY_SCALAR_FIELD(numCols);
	COPY_POINTER_FIELD(dupColIdx, from->numCols * sizeof(AttrNumber));
	COPY_POINTER_FIELD(dupOperators, from->numCols * sizeof(Oid));
	COPY_SCALAR_FIELD(flagColIdx);
	COPY_SCALAR_FIELD(firstFlag);
	COPY_SCALAR_FIELD(numGroups);

	return newnode;
}

/*
 * _copyLockRows
 */
static LockRows *
_copyLockRows(const LockRows *from)
{
	LockRows   *newnode = makeNode(LockRows);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(rowMarks);
	COPY_SCALAR_FIELD(epqParam);

	return newnode;
}

/*
 * _copyLimit
 */
static Limit *
_copyLimit(const Limit *from)
{
	Limit	   *newnode = makeNode(Limit);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((const Plan *) from, (Plan *) newnode);

	/*
	 * copy remainder of node
	 */
	COPY_NODE_FIELD(limitOffset);
	COPY_NODE_FIELD(limitCount);

	return newnode;
}

/*
 * _copyNestLoopParam
 */
static NestLoopParam *
_copyNestLoopParam(const NestLoopParam *from)
{
	NestLoopParam *newnode = makeNode(NestLoopParam);

	COPY_SCALAR_FIELD(paramno);
	COPY_NODE_FIELD(paramval);

	return newnode;
}

/*
 * _copyPlanRowMark
 */
static PlanRowMark *
_copyPlanRowMark(const PlanRowMark *from)
{
	PlanRowMark *newnode = makeNode(PlanRowMark);

	COPY_SCALAR_FIELD(rti);
	COPY_SCALAR_FIELD(prti);
	COPY_SCALAR_FIELD(rowmarkId);
	COPY_SCALAR_FIELD(markType);
	COPY_SCALAR_FIELD(noWait);
	COPY_SCALAR_FIELD(isParent);

	return newnode;
}

/*
 * _copyPlanInvalItem
 */
static PlanInvalItem *
_copyPlanInvalItem(const PlanInvalItem *from)
{
	PlanInvalItem *newnode = makeNode(PlanInvalItem);

	COPY_SCALAR_FIELD(cacheId);
	COPY_SCALAR_FIELD(hashValue);

	return newnode;
}

/*
 * _copyMotion
 */
static Motion *
_copyMotion(const Motion *from)
{
	Motion	   *newnode = makeNode(Motion);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(sendSorted);
	COPY_SCALAR_FIELD(motionID);

	COPY_SCALAR_FIELD(motionType);

	COPY_NODE_FIELD(hashExprs);
	COPY_POINTER_FIELD(hashFuncs, list_length(from->hashExprs) * sizeof(Oid));

	COPY_SCALAR_FIELD(isBroadcast);

	COPY_SCALAR_FIELD(numSortCols);
	COPY_POINTER_FIELD(sortColIdx, from->numSortCols * sizeof(AttrNumber));
	COPY_POINTER_FIELD(sortOperators, from->numSortCols * sizeof(Oid));
	COPY_POINTER_FIELD(collations, from->numSortCols * sizeof(Oid));
	COPY_POINTER_FIELD(nullsFirst, from->numSortCols * sizeof(bool));

	COPY_SCALAR_FIELD(segidColIdx);

	return newnode;
}

/*
 * _copyDML
 */
static DML *
_copyDML(const DML *from)
{
	DML *newnode = makeNode(DML);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(scanrelid);
	COPY_SCALAR_FIELD(actionColIdx);
	COPY_SCALAR_FIELD(ctidColIdx);
	COPY_SCALAR_FIELD(tupleoidColIdx);

	return newnode;
}

/*
 * _copySplitUpdate
 */
static SplitUpdate *
_copySplitUpdate(const SplitUpdate *from)
{
	SplitUpdate *newnode = makeNode(SplitUpdate);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(actionColIdx);
	COPY_SCALAR_FIELD(ctidColIdx);
	COPY_SCALAR_FIELD(tupleoidColIdx);
	COPY_NODE_FIELD(insertColIdx);
	COPY_NODE_FIELD(deleteColIdx);

	return newnode;
}

/*
 * _copyRowTrigger
 */
static RowTrigger *
_copyRowTrigger(const RowTrigger *from)
{
	RowTrigger *newnode = makeNode(RowTrigger);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(relid);
	COPY_SCALAR_FIELD(eventFlags);
	COPY_NODE_FIELD(oldValuesColIdx);
	COPY_NODE_FIELD(newValuesColIdx);

	return newnode;
}

/*
 * _copyAssertOp
 */
static AssertOp *
_copyAssertOp(const AssertOp *from)
{
	AssertOp *newnode = makeNode(AssertOp);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(errcode);
	COPY_NODE_FIELD(errmessage);

	return newnode;
}

/*
 * _copyPartitionSelector
 */
static PartitionSelector *
_copyPartitionSelector(const PartitionSelector *from)
{
	PartitionSelector *newnode = makeNode(PartitionSelector);

	/*
	 * copy node superclass fields
	 */
	CopyPlanFields((Plan *) from, (Plan *) newnode);

	COPY_SCALAR_FIELD(relid);
	COPY_SCALAR_FIELD(nLevels);
	COPY_SCALAR_FIELD(scanId);
	COPY_SCALAR_FIELD(selectorId);
	COPY_NODE_FIELD(levelEqExpressions);
	COPY_NODE_FIELD(levelExpressions);
	COPY_NODE_FIELD(residualPredicate);
	COPY_NODE_FIELD(propagationExpression);
	COPY_NODE_FIELD(printablePredicate);
	COPY_SCALAR_FIELD(staticSelection);
	COPY_NODE_FIELD(staticPartOids);
	COPY_NODE_FIELD(staticScanIds);
	COPY_NODE_FIELD(partTabTargetlist);

	return newnode;
}

/* ****************************************************************
 *					   primnodes.h copy functions
 * ****************************************************************
 */

/*
 * _copyAlias
 */
static Alias *
_copyAlias(const Alias *from)
{
	Alias	   *newnode = makeNode(Alias);

	COPY_STRING_FIELD(aliasname);
	COPY_NODE_FIELD(colnames);

	return newnode;
}

/*
 * _copyRangeVar
 */
static RangeVar *
_copyRangeVar(const RangeVar *from)
{
	RangeVar   *newnode = makeNode(RangeVar);

	Assert(from->schemaname == NULL || strlen(from->schemaname)>0);
	COPY_STRING_FIELD(catalogname);
	COPY_STRING_FIELD(schemaname);
	COPY_STRING_FIELD(relname);
	COPY_SCALAR_FIELD(inhOpt);
	COPY_SCALAR_FIELD(relpersistence);
	COPY_NODE_FIELD(alias);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyIntoClause
 */
static IntoClause *
_copyIntoClause(const IntoClause *from)
{
	IntoClause *newnode = makeNode(IntoClause);

	COPY_NODE_FIELD(rel);
	COPY_NODE_FIELD(colNames);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(onCommit);
	COPY_STRING_FIELD(tableSpaceName);
	COPY_NODE_FIELD(viewQuery);
	COPY_SCALAR_FIELD(skipData);
	COPY_NODE_FIELD(distributedBy);

	return newnode;
}

/*
 * _copyIntoClause
 */
static CopyIntoClause *
_copyCopyIntoClause(const CopyIntoClause *from)
{
	CopyIntoClause *newnode = makeNode(CopyIntoClause);

	COPY_NODE_FIELD(attlist);
	COPY_SCALAR_FIELD(is_program);
	COPY_STRING_FIELD(filename);
	COPY_NODE_FIELD(options);
	COPY_NODE_FIELD(ao_segnos);

	return newnode;
}

/*
 * _copyRefreshClause
 */
static RefreshClause *
_copyRefreshClause(const RefreshClause *from)
{
	RefreshClause *newnode = makeNode(RefreshClause);

	COPY_SCALAR_FIELD(concurrent);
	COPY_NODE_FIELD(relation);

	return newnode;
}

/*
 * We don't need a _copyExpr because Expr is an abstract supertype which
 * should never actually get instantiated.  Also, since it has no common
 * fields except NodeTag, there's no need for a helper routine to factor
 * out copying the common fields...
 */

/*
 * _copyVar
 */
static Var *
_copyVar(const Var *from)
{
	Var		   *newnode = makeNode(Var);

	COPY_SCALAR_FIELD(varno);
	COPY_SCALAR_FIELD(varattno);
	COPY_SCALAR_FIELD(vartype);
	COPY_SCALAR_FIELD(vartypmod);
	COPY_SCALAR_FIELD(varcollid);
	COPY_SCALAR_FIELD(varlevelsup);
	COPY_SCALAR_FIELD(varnoold);
	COPY_SCALAR_FIELD(varoattno);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyConst
 */
static Const *
_copyConst(const Const *from)
{
	Const	   *newnode = makeNode(Const);

	COPY_SCALAR_FIELD(consttype);
	COPY_SCALAR_FIELD(consttypmod);
	COPY_SCALAR_FIELD(constcollid);
	COPY_SCALAR_FIELD(constlen);

	if (from->constbyval || from->constisnull)
	{
		/*
		 * passed by value so just copy the datum. Also, don't try to copy
		 * struct when value is null!
		 */
		newnode->constvalue = from->constvalue;
	}
	else
	{
		/*
		 * passed by reference.  We need a palloc'd copy.
		 */
		newnode->constvalue = datumCopy(from->constvalue,
										from->constbyval,
										from->constlen);
	}

	COPY_SCALAR_FIELD(constisnull);
	COPY_SCALAR_FIELD(constbyval);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyParam
 */
static Param *
_copyParam(const Param *from)
{
	Param	   *newnode = makeNode(Param);

	COPY_SCALAR_FIELD(paramkind);
	COPY_SCALAR_FIELD(paramid);
	COPY_SCALAR_FIELD(paramtype);
	COPY_SCALAR_FIELD(paramtypmod);
	COPY_SCALAR_FIELD(paramcollid);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyAggref
 */
static Aggref *
_copyAggref(const Aggref *from)
{
	Aggref	   *newnode = makeNode(Aggref);

	COPY_SCALAR_FIELD(aggfnoid);
	COPY_SCALAR_FIELD(aggtype);
	COPY_SCALAR_FIELD(aggcollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(aggdirectargs);
	COPY_NODE_FIELD(args);
	COPY_NODE_FIELD(aggorder);
	COPY_NODE_FIELD(aggdistinct);
	COPY_NODE_FIELD(aggfilter);
	COPY_SCALAR_FIELD(aggstar);
	COPY_SCALAR_FIELD(aggvariadic);
	COPY_SCALAR_FIELD(aggkind);
	COPY_SCALAR_FIELD(agglevelsup);
	COPY_SCALAR_FIELD(aggstage);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyWindowFunc
 */
static WindowFunc *
_copyWindowFunc(const WindowFunc *from)
{
	WindowFunc *newnode = makeNode(WindowFunc);

	COPY_SCALAR_FIELD(winfnoid);
	COPY_SCALAR_FIELD(wintype);
	COPY_SCALAR_FIELD(wincollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(args);
	COPY_NODE_FIELD(aggfilter);
	COPY_SCALAR_FIELD(winref);
	COPY_SCALAR_FIELD(winstar);
	COPY_SCALAR_FIELD(winagg);
	COPY_SCALAR_FIELD(windistinct);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyArrayRef
 */
static ArrayRef *
_copyArrayRef(const ArrayRef *from)
{
	ArrayRef   *newnode = makeNode(ArrayRef);

	COPY_SCALAR_FIELD(refarraytype);
	COPY_SCALAR_FIELD(refelemtype);
	COPY_SCALAR_FIELD(reftypmod);
	COPY_SCALAR_FIELD(refcollid);
	COPY_NODE_FIELD(refupperindexpr);
	COPY_NODE_FIELD(reflowerindexpr);
	COPY_NODE_FIELD(refexpr);
	COPY_NODE_FIELD(refassgnexpr);

	return newnode;
}

/*
 * _copyFuncExpr
 */
static FuncExpr *
_copyFuncExpr(const FuncExpr *from)
{
	FuncExpr   *newnode = makeNode(FuncExpr);

	COPY_SCALAR_FIELD(funcid);
	COPY_SCALAR_FIELD(funcresulttype);
	COPY_SCALAR_FIELD(funcretset);
	COPY_SCALAR_FIELD(funcvariadic);
	COPY_SCALAR_FIELD(funcformat);
	COPY_SCALAR_FIELD(funccollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(is_tablefunc);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyNamedArgExpr *
 */
static NamedArgExpr *
_copyNamedArgExpr(const NamedArgExpr *from)
{
	NamedArgExpr *newnode = makeNode(NamedArgExpr);

	COPY_NODE_FIELD(arg);
	COPY_STRING_FIELD(name);
	COPY_SCALAR_FIELD(argnumber);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyOpExpr
 */
static OpExpr *
_copyOpExpr(const OpExpr *from)
{
	OpExpr	   *newnode = makeNode(OpExpr);

	COPY_SCALAR_FIELD(opno);
	COPY_SCALAR_FIELD(opfuncid);
	COPY_SCALAR_FIELD(opresulttype);
	COPY_SCALAR_FIELD(opretset);
	COPY_SCALAR_FIELD(opcollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyDistinctExpr (same as OpExpr)
 */
static DistinctExpr *
_copyDistinctExpr(const DistinctExpr *from)
{
	DistinctExpr *newnode = makeNode(DistinctExpr);

	COPY_SCALAR_FIELD(opno);
	COPY_SCALAR_FIELD(opfuncid);
	COPY_SCALAR_FIELD(opresulttype);
	COPY_SCALAR_FIELD(opretset);
	COPY_SCALAR_FIELD(opcollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyNullIfExpr (same as OpExpr)
 */
static NullIfExpr *
_copyNullIfExpr(const NullIfExpr *from)
{
	NullIfExpr *newnode = makeNode(NullIfExpr);

	COPY_SCALAR_FIELD(opno);
	COPY_SCALAR_FIELD(opfuncid);
	COPY_SCALAR_FIELD(opresulttype);
	COPY_SCALAR_FIELD(opretset);
	COPY_SCALAR_FIELD(opcollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyScalarArrayOpExpr
 */
static ScalarArrayOpExpr *
_copyScalarArrayOpExpr(const ScalarArrayOpExpr *from)
{
	ScalarArrayOpExpr *newnode = makeNode(ScalarArrayOpExpr);

	COPY_SCALAR_FIELD(opno);
	COPY_SCALAR_FIELD(opfuncid);
	COPY_SCALAR_FIELD(useOr);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyBoolExpr
 */
static BoolExpr *
_copyBoolExpr(const BoolExpr *from)
{
	BoolExpr   *newnode = makeNode(BoolExpr);

	COPY_SCALAR_FIELD(boolop);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copySubLink
 */
static SubLink *
_copySubLink(const SubLink *from)
{
	SubLink    *newnode = makeNode(SubLink);

	COPY_SCALAR_FIELD(subLinkType);
	COPY_NODE_FIELD(testexpr);
	COPY_NODE_FIELD(operName);
	COPY_NODE_FIELD(subselect);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copySubPlan
 */
static SubPlan *
_copySubPlan(const SubPlan *from)
{
	SubPlan    *newnode = makeNode(SubPlan);

	COPY_SCALAR_FIELD(subLinkType);
	COPY_SCALAR_FIELD(qDispSliceId);    /*CDB*/
	COPY_NODE_FIELD(testexpr);
	COPY_NODE_FIELD(paramIds);
	COPY_SCALAR_FIELD(plan_id);
	COPY_STRING_FIELD(plan_name);
	COPY_SCALAR_FIELD(firstColType);
	COPY_SCALAR_FIELD(firstColTypmod);
	COPY_SCALAR_FIELD(firstColCollation);
	COPY_SCALAR_FIELD(useHashTable);
	COPY_SCALAR_FIELD(unknownEqFalse);
	COPY_SCALAR_FIELD(is_initplan);	/*CDB*/
	COPY_SCALAR_FIELD(is_multirow);	/*CDB*/
	COPY_NODE_FIELD(setParam);
	COPY_NODE_FIELD(parParam);
	COPY_NODE_FIELD(args);
	COPY_NODE_FIELD(extParam);
	COPY_SCALAR_FIELD(startup_cost);
	COPY_SCALAR_FIELD(per_call_cost);
	COPY_SCALAR_FIELD(initPlanParallel);

	return newnode;
}

/*
 * _copyAlternativeSubPlan
 */
static AlternativeSubPlan *
_copyAlternativeSubPlan(const AlternativeSubPlan *from)
{
	AlternativeSubPlan *newnode = makeNode(AlternativeSubPlan);

	COPY_NODE_FIELD(subplans);

	return newnode;
}

/*
 * _copyFieldSelect
 */
static FieldSelect *
_copyFieldSelect(const FieldSelect *from)
{
	FieldSelect *newnode = makeNode(FieldSelect);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(fieldnum);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(resulttypmod);
	COPY_SCALAR_FIELD(resultcollid);

	return newnode;
}

/*
 * _copyFieldStore
 */
static FieldStore *
_copyFieldStore(const FieldStore *from)
{
	FieldStore *newnode = makeNode(FieldStore);

	COPY_NODE_FIELD(arg);
	COPY_NODE_FIELD(newvals);
	COPY_NODE_FIELD(fieldnums);
	COPY_SCALAR_FIELD(resulttype);

	return newnode;
}

/*
 * _copyRelabelType
 */
static RelabelType *
_copyRelabelType(const RelabelType *from)
{
	RelabelType *newnode = makeNode(RelabelType);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(resulttypmod);
	COPY_SCALAR_FIELD(resultcollid);
	COPY_SCALAR_FIELD(relabelformat);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCoerceViaIO
 */
static CoerceViaIO *
_copyCoerceViaIO(const CoerceViaIO *from)
{
	CoerceViaIO *newnode = makeNode(CoerceViaIO);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(resultcollid);
	COPY_SCALAR_FIELD(coerceformat);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyArrayCoerceExpr
 */
static ArrayCoerceExpr *
_copyArrayCoerceExpr(const ArrayCoerceExpr *from)
{
	ArrayCoerceExpr *newnode = makeNode(ArrayCoerceExpr);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(elemfuncid);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(resulttypmod);
	COPY_SCALAR_FIELD(resultcollid);
	COPY_SCALAR_FIELD(isExplicit);
	COPY_SCALAR_FIELD(coerceformat);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyConvertRowtypeExpr
 */
static ConvertRowtypeExpr *
_copyConvertRowtypeExpr(const ConvertRowtypeExpr *from)
{
	ConvertRowtypeExpr *newnode = makeNode(ConvertRowtypeExpr);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(convertformat);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCollateExpr
 */
static CollateExpr *
_copyCollateExpr(const CollateExpr *from)
{
	CollateExpr *newnode = makeNode(CollateExpr);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(collOid);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCaseExpr
 */
static CaseExpr *
_copyCaseExpr(const CaseExpr *from)
{
	CaseExpr   *newnode = makeNode(CaseExpr);

	COPY_SCALAR_FIELD(casetype);
	COPY_SCALAR_FIELD(casecollid);
	COPY_NODE_FIELD(arg);
	COPY_NODE_FIELD(args);
	COPY_NODE_FIELD(defresult);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCaseWhen
 */
static CaseWhen *
_copyCaseWhen(const CaseWhen *from)
{
	CaseWhen   *newnode = makeNode(CaseWhen);

	COPY_NODE_FIELD(expr);
	COPY_NODE_FIELD(result);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCaseTestExpr
 */
static CaseTestExpr *
_copyCaseTestExpr(const CaseTestExpr *from)
{
	CaseTestExpr *newnode = makeNode(CaseTestExpr);

	COPY_SCALAR_FIELD(typeId);
	COPY_SCALAR_FIELD(typeMod);
	COPY_SCALAR_FIELD(collation);

	return newnode;
}

/*
 * _copyArrayExpr
 */
static ArrayExpr *
_copyArrayExpr(const ArrayExpr *from)
{
	ArrayExpr  *newnode = makeNode(ArrayExpr);

	COPY_SCALAR_FIELD(array_typeid);
	COPY_SCALAR_FIELD(array_collid);
	COPY_SCALAR_FIELD(element_typeid);
	COPY_NODE_FIELD(elements);
	COPY_SCALAR_FIELD(multidims);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyRowExpr
 */
static RowExpr *
_copyRowExpr(const RowExpr *from)
{
	RowExpr    *newnode = makeNode(RowExpr);

	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(row_typeid);
	COPY_SCALAR_FIELD(row_format);
	COPY_NODE_FIELD(colnames);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyRowCompareExpr
 */
static RowCompareExpr *
_copyRowCompareExpr(const RowCompareExpr *from)
{
	RowCompareExpr *newnode = makeNode(RowCompareExpr);

	COPY_SCALAR_FIELD(rctype);
	COPY_NODE_FIELD(opnos);
	COPY_NODE_FIELD(opfamilies);
	COPY_NODE_FIELD(inputcollids);
	COPY_NODE_FIELD(largs);
	COPY_NODE_FIELD(rargs);

	return newnode;
}

/*
 * _copyCoalesceExpr
 */
static CoalesceExpr *
_copyCoalesceExpr(const CoalesceExpr *from)
{
	CoalesceExpr *newnode = makeNode(CoalesceExpr);

	COPY_SCALAR_FIELD(coalescetype);
	COPY_SCALAR_FIELD(coalescecollid);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyMinMaxExpr
 */
static MinMaxExpr *
_copyMinMaxExpr(const MinMaxExpr *from)
{
	MinMaxExpr *newnode = makeNode(MinMaxExpr);

	COPY_SCALAR_FIELD(minmaxtype);
	COPY_SCALAR_FIELD(minmaxcollid);
	COPY_SCALAR_FIELD(inputcollid);
	COPY_SCALAR_FIELD(op);
	COPY_NODE_FIELD(args);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyXmlExpr
 */
static XmlExpr *
_copyXmlExpr(const XmlExpr *from)
{
	XmlExpr    *newnode = makeNode(XmlExpr);

	COPY_SCALAR_FIELD(op);
	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(named_args);
	COPY_NODE_FIELD(arg_names);
	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(xmloption);
	COPY_SCALAR_FIELD(type);
	COPY_SCALAR_FIELD(typmod);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyNullTest
 */
static NullTest *
_copyNullTest(const NullTest *from)
{
	NullTest   *newnode = makeNode(NullTest);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(nulltesttype);
	COPY_SCALAR_FIELD(argisrow);

	return newnode;
}

/*
 * _copyBooleanTest
 */
static BooleanTest *
_copyBooleanTest(const BooleanTest *from)
{
	BooleanTest *newnode = makeNode(BooleanTest);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(booltesttype);

	return newnode;
}

/*
 * _copyCoerceToDomain
 */
static CoerceToDomain *
_copyCoerceToDomain(const CoerceToDomain *from)
{
	CoerceToDomain *newnode = makeNode(CoerceToDomain);

	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(resulttypmod);
	COPY_SCALAR_FIELD(resultcollid);
	COPY_SCALAR_FIELD(coercionformat);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCoerceToDomainValue
 */
static CoerceToDomainValue *
_copyCoerceToDomainValue(const CoerceToDomainValue *from)
{
	CoerceToDomainValue *newnode = makeNode(CoerceToDomainValue);

	COPY_SCALAR_FIELD(typeId);
	COPY_SCALAR_FIELD(typeMod);
	COPY_SCALAR_FIELD(collation);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copySetToDefault
 */
static SetToDefault *
_copySetToDefault(const SetToDefault *from)
{
	SetToDefault *newnode = makeNode(SetToDefault);

	COPY_SCALAR_FIELD(typeId);
	COPY_SCALAR_FIELD(typeMod);
	COPY_SCALAR_FIELD(collation);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

/*
 * _copyCurrentOfExpr
 */
static CurrentOfExpr *
_copyCurrentOfExpr(const CurrentOfExpr *from)
{
	CurrentOfExpr *newnode = makeNode(CurrentOfExpr);

	COPY_SCALAR_FIELD(cvarno);
	COPY_STRING_FIELD(cursor_name);
	COPY_SCALAR_FIELD(cursor_param);
	COPY_SCALAR_FIELD(target_relid);

	return newnode;
}

/*
 * _copyTargetEntry
 */
static TargetEntry *
_copyTargetEntry(const TargetEntry *from)
{
	TargetEntry *newnode = makeNode(TargetEntry);

	COPY_NODE_FIELD(expr);
	COPY_SCALAR_FIELD(resno);
	COPY_STRING_FIELD(resname);
	COPY_SCALAR_FIELD(ressortgroupref);
	COPY_SCALAR_FIELD(resorigtbl);
	COPY_SCALAR_FIELD(resorigcol);
	COPY_SCALAR_FIELD(resjunk);

	return newnode;
}

/*
 * _copyRangeTblRef
 */
static RangeTblRef *
_copyRangeTblRef(const RangeTblRef *from)
{
	RangeTblRef *newnode = makeNode(RangeTblRef);

	COPY_SCALAR_FIELD(rtindex);

	return newnode;
}

/*
 * _copyJoinExpr
 */
static JoinExpr *
_copyJoinExpr(const JoinExpr *from)
{
	JoinExpr   *newnode = makeNode(JoinExpr);

	COPY_SCALAR_FIELD(jointype);
	COPY_SCALAR_FIELD(isNatural);
	COPY_NODE_FIELD(larg);
	COPY_NODE_FIELD(rarg);
	COPY_NODE_FIELD(usingClause);
	COPY_NODE_FIELD(quals);
	COPY_NODE_FIELD(alias);
	COPY_SCALAR_FIELD(rtindex);

	return newnode;
}

/*
 * _copyFromExpr
 */
static FromExpr *
_copyFromExpr(const FromExpr *from)
{
	FromExpr   *newnode = makeNode(FromExpr);

	COPY_NODE_FIELD(fromlist);
	COPY_NODE_FIELD(quals);

	return newnode;
}

/*
 * _copyFlow
 */
static Flow *
_copyFlow(const Flow *from)
{
	Flow   *newnode = makeNode(Flow);

	COPY_SCALAR_FIELD(flotype);
	COPY_SCALAR_FIELD(req_move);
	COPY_SCALAR_FIELD(locustype);
	COPY_SCALAR_FIELD(segindex);
	COPY_SCALAR_FIELD(numsegments);
	COPY_NODE_FIELD(hashExprs);
	COPY_NODE_FIELD(hashOpfamilies);
	COPY_NODE_FIELD(flow_before_req_move);

	return newnode;
}


/* ****************************************************************
 *						relation.h copy functions
 *
 * We don't support copying RelOptInfo, IndexOptInfo, or Path nodes.
 * There are some subsidiary structs that are useful to copy, though.
 * ****************************************************************
 */

/*
 * _copyCdbRelColumnInfo
 */
static CdbRelColumnInfo *
_copyCdbRelColumnInfo(const CdbRelColumnInfo *from)
{
	CdbRelColumnInfo *newnode = makeNode(CdbRelColumnInfo);

	COPY_SCALAR_FIELD(pseudoattno);
    COPY_SCALAR_FIELD(targetresno);
	COPY_NODE_FIELD(defexpr);
	COPY_BITMAPSET_FIELD(where_needed);
	COPY_SCALAR_FIELD(attr_width);
    COPY_BINARY_FIELD(colname, sizeof(from->colname));

	return newnode;
}

/*
 * _copyPathKey
 */
static PathKey *
_copyPathKey(const PathKey *from)
{
	PathKey    *newnode = makeNode(PathKey);

	/* EquivalenceClasses are never moved, so just shallow-copy the pointer */
	COPY_SCALAR_FIELD(pk_eclass);
	COPY_SCALAR_FIELD(pk_opfamily);
	COPY_SCALAR_FIELD(pk_strategy);
	COPY_SCALAR_FIELD(pk_nulls_first);

	return newnode;
}

/*
 * _copyDistributionKey
 */
static DistributionKey *
_copyDistributionKey(const DistributionKey *from)
{
	DistributionKey    *newnode = makeNode(DistributionKey);

	COPY_SCALAR_FIELD(dk_opfamily);
	/* EquivalenceClasses are never moved, so just shallow-copy the pointer */
	newnode->dk_eclasses = list_copy(from->dk_eclasses);

	return newnode;
}

/*
 * _copyRestrictInfo
 */
static RestrictInfo *
_copyRestrictInfo(const RestrictInfo *from)
{
	RestrictInfo *newnode = makeNode(RestrictInfo);

	COPY_NODE_FIELD(clause);
	COPY_SCALAR_FIELD(is_pushed_down);
	COPY_SCALAR_FIELD(outerjoin_delayed);
	COPY_SCALAR_FIELD(can_join);
	COPY_SCALAR_FIELD(pseudoconstant);
	COPY_SCALAR_FIELD(contain_outer_query_references);
	COPY_BITMAPSET_FIELD(clause_relids);
	COPY_BITMAPSET_FIELD(required_relids);
	COPY_BITMAPSET_FIELD(outer_relids);
	COPY_BITMAPSET_FIELD(nullable_relids);
	COPY_BITMAPSET_FIELD(left_relids);
	COPY_BITMAPSET_FIELD(right_relids);
	COPY_NODE_FIELD(orclause);
	/* EquivalenceClasses are never copied, so shallow-copy the pointers */
	COPY_SCALAR_FIELD(parent_ec);
	COPY_SCALAR_FIELD(eval_cost);
	COPY_SCALAR_FIELD(norm_selec);
	COPY_SCALAR_FIELD(outer_selec);
	COPY_NODE_FIELD(mergeopfamilies);
	/* EquivalenceClasses are never copied, so shallow-copy the pointers */
	COPY_SCALAR_FIELD(left_ec);
	COPY_SCALAR_FIELD(right_ec);
	COPY_SCALAR_FIELD(left_em);
	COPY_SCALAR_FIELD(right_em);
	/* MergeScanSelCache isn't a Node, so hard to copy; just reset cache */
	newnode->scansel_cache = NIL;
	COPY_SCALAR_FIELD(outer_is_left);
	COPY_SCALAR_FIELD(hashjoinoperator);
	COPY_SCALAR_FIELD(left_bucketsize);
	COPY_SCALAR_FIELD(right_bucketsize);

	return newnode;
}

/*
 * _copyPlaceHolderVar
 */
static PlaceHolderVar *
_copyPlaceHolderVar(const PlaceHolderVar *from)
{
	PlaceHolderVar *newnode = makeNode(PlaceHolderVar);

	COPY_NODE_FIELD(phexpr);
	COPY_BITMAPSET_FIELD(phrels);
	COPY_SCALAR_FIELD(phid);
	COPY_SCALAR_FIELD(phlevelsup);

	return newnode;
}

/*
 * _copySpecialJoinInfo
 */
static SpecialJoinInfo *
_copySpecialJoinInfo(const SpecialJoinInfo *from)
{
	SpecialJoinInfo *newnode = makeNode(SpecialJoinInfo);

	COPY_BITMAPSET_FIELD(min_lefthand);
	COPY_BITMAPSET_FIELD(min_righthand);
	COPY_BITMAPSET_FIELD(syn_lefthand);
	COPY_BITMAPSET_FIELD(syn_righthand);
	COPY_SCALAR_FIELD(jointype);
	COPY_SCALAR_FIELD(lhs_strict);
	COPY_SCALAR_FIELD(delay_upper_joins);
	COPY_NODE_FIELD(join_quals);

	return newnode;
}

/*
 * _copyLateralJoinInfo
 */
static LateralJoinInfo *
_copyLateralJoinInfo(const LateralJoinInfo *from)
{
	LateralJoinInfo *newnode = makeNode(LateralJoinInfo);

	COPY_BITMAPSET_FIELD(lateral_lhs);
	COPY_BITMAPSET_FIELD(lateral_rhs);

	return newnode;
}

/*
 * _copyAppendRelInfo
 */
static AppendRelInfo *
_copyAppendRelInfo(const AppendRelInfo *from)
{
	AppendRelInfo *newnode = makeNode(AppendRelInfo);

	COPY_SCALAR_FIELD(parent_relid);
	COPY_SCALAR_FIELD(child_relid);
	COPY_SCALAR_FIELD(parent_reltype);
	COPY_SCALAR_FIELD(child_reltype);
	COPY_NODE_FIELD(translated_vars);
	COPY_SCALAR_FIELD(parent_reloid);

	return newnode;
}

/*
 * _copyPlaceHolderInfo
 */
static PlaceHolderInfo *
_copyPlaceHolderInfo(const PlaceHolderInfo *from)
{
	PlaceHolderInfo *newnode = makeNode(PlaceHolderInfo);

	COPY_SCALAR_FIELD(phid);
	COPY_NODE_FIELD(ph_var);
	COPY_BITMAPSET_FIELD(ph_eval_at);
	COPY_BITMAPSET_FIELD(ph_lateral);
	COPY_BITMAPSET_FIELD(ph_needed);
	COPY_SCALAR_FIELD(ph_width);

	return newnode;
}

/* ****************************************************************
 *					parsenodes.h copy functions
 * ****************************************************************
 */

static RangeTblEntry *
_copyRangeTblEntry(const RangeTblEntry *from)
{
	RangeTblEntry *newnode = makeNode(RangeTblEntry);

	COPY_SCALAR_FIELD(rtekind);
	COPY_SCALAR_FIELD(relid);
	COPY_SCALAR_FIELD(relkind);
	COPY_NODE_FIELD(subquery);
	COPY_SCALAR_FIELD(security_barrier);
	COPY_SCALAR_FIELD(jointype);
	COPY_NODE_FIELD(joinaliasvars);
	COPY_NODE_FIELD(functions);
	COPY_SCALAR_FIELD(funcordinality);
	COPY_NODE_FIELD(values_lists);
	COPY_NODE_FIELD(values_collations);
	COPY_STRING_FIELD(ctename);
	COPY_SCALAR_FIELD(ctelevelsup);
	COPY_SCALAR_FIELD(self_reference);
	COPY_NODE_FIELD(ctecoltypes);
	COPY_NODE_FIELD(ctecoltypmods);
	COPY_NODE_FIELD(ctecolcollations);
	COPY_NODE_FIELD(alias);
	COPY_NODE_FIELD(eref);
	COPY_SCALAR_FIELD(lateral);
	COPY_SCALAR_FIELD(inh);
	COPY_SCALAR_FIELD(inFromCl);
	COPY_SCALAR_FIELD(requiredPerms);
	COPY_SCALAR_FIELD(checkAsUser);
	COPY_BITMAPSET_FIELD(selectedCols);
	COPY_BITMAPSET_FIELD(modifiedCols);
	COPY_NODE_FIELD(securityQuals);

	COPY_STRING_FIELD(ctename);
	COPY_SCALAR_FIELD(ctelevelsup);
	COPY_SCALAR_FIELD(self_reference);
	COPY_NODE_FIELD(ctecoltypes);
	COPY_NODE_FIELD(ctecoltypmods);

	COPY_SCALAR_FIELD(forceDistRandom);
	COPY_NODE_FIELD(pseudocols);                /*CDB*/

	return newnode;
}

static RangeTblFunction *
_copyRangeTblFunction(const RangeTblFunction *from)
{
	RangeTblFunction *newnode = makeNode(RangeTblFunction);

	COPY_NODE_FIELD(funcexpr);
	COPY_SCALAR_FIELD(funccolcount);
	COPY_NODE_FIELD(funccolnames);
	COPY_NODE_FIELD(funccoltypes);
	COPY_NODE_FIELD(funccoltypmods);
	COPY_NODE_FIELD(funccolcollations);
	COPY_VARLENA_FIELD(funcuserdata, -1);
	COPY_BITMAPSET_FIELD(funcparams);

	return newnode;
}

static WithCheckOption *
_copyWithCheckOption(const WithCheckOption *from)
{
	WithCheckOption *newnode = makeNode(WithCheckOption);

	COPY_STRING_FIELD(viewname);
	COPY_NODE_FIELD(qual);
	COPY_SCALAR_FIELD(cascaded);

	return newnode;
}

static SortGroupClause *
_copySortGroupClause(const SortGroupClause *from)
{
	SortGroupClause *newnode = makeNode(SortGroupClause);

	COPY_SCALAR_FIELD(tleSortGroupRef);
	COPY_SCALAR_FIELD(eqop);
	COPY_SCALAR_FIELD(sortop);
	COPY_SCALAR_FIELD(nulls_first);
	COPY_SCALAR_FIELD(hashable);

	return newnode;
}

static GroupingClause *
_copyGroupingClause(const GroupingClause *from)
{
	GroupingClause *newnode = makeNode(GroupingClause);
	COPY_SCALAR_FIELD(groupType);
	COPY_NODE_FIELD(groupsets);

	return newnode;
}

static GroupingFunc *
_copyGroupingFunc(const GroupingFunc *from)
{
	GroupingFunc *newnode = makeNode(GroupingFunc);

	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(ngrpcols);

	return newnode;
}

static Grouping *
_copyGrouping(const Grouping *from)
{
	Grouping *newnode = makeNode(Grouping);

	return newnode;
}

static GroupId *
_copyGroupId(const GroupId *from)
{
	GroupId *newnode = makeNode(GroupId);

	return newnode;
}

static WindowClause *
_copyWindowClause(const WindowClause *from)
{
	WindowClause *newnode = makeNode(WindowClause);

	COPY_STRING_FIELD(name);
	COPY_STRING_FIELD(refname);
	COPY_NODE_FIELD(partitionClause);
	COPY_NODE_FIELD(orderClause);
	COPY_SCALAR_FIELD(frameOptions);
	COPY_NODE_FIELD(startOffset);
	COPY_NODE_FIELD(endOffset);
	COPY_SCALAR_FIELD(winref);
	COPY_SCALAR_FIELD(copiedOrder);

	return newnode;
}

static RowMarkClause *
_copyRowMarkClause(const RowMarkClause *from)
{
	RowMarkClause *newnode = makeNode(RowMarkClause);

	COPY_SCALAR_FIELD(rti);
	COPY_SCALAR_FIELD(strength);
	COPY_SCALAR_FIELD(noWait);
	COPY_SCALAR_FIELD(pushedDown);

	return newnode;
}

static WithClause *
_copyWithClause(const WithClause *from)
{
	WithClause *newnode = makeNode(WithClause);

	COPY_NODE_FIELD(ctes);
	COPY_SCALAR_FIELD(recursive);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static CommonTableExpr *
_copyCommonTableExpr(const CommonTableExpr *from)
{
	CommonTableExpr *newnode = makeNode(CommonTableExpr);

	COPY_STRING_FIELD(ctename);
	COPY_NODE_FIELD(aliascolnames);
	COPY_NODE_FIELD(ctequery);
	COPY_LOCATION_FIELD(location);
	COPY_SCALAR_FIELD(cterecursive);
	COPY_SCALAR_FIELD(cterefcount);
	COPY_NODE_FIELD(ctecolnames);
	COPY_NODE_FIELD(ctecoltypes);
	COPY_NODE_FIELD(ctecoltypmods);
	COPY_NODE_FIELD(ctecolcollations);

	return newnode;
}

static A_Expr *
_copyAExpr(const A_Expr *from)
{
	A_Expr	   *newnode = makeNode(A_Expr);

	COPY_SCALAR_FIELD(kind);
	COPY_NODE_FIELD(name);
	COPY_NODE_FIELD(lexpr);
	COPY_NODE_FIELD(rexpr);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static ColumnRef *
_copyColumnRef(const ColumnRef *from)
{
	ColumnRef  *newnode = makeNode(ColumnRef);

	COPY_NODE_FIELD(fields);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static ParamRef *
_copyParamRef(const ParamRef *from)
{
	ParamRef   *newnode = makeNode(ParamRef);

	COPY_SCALAR_FIELD(number);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static A_Const *
_copyAConst(const A_Const *from)
{
	A_Const    *newnode = makeNode(A_Const);

	/* This part must duplicate _copyValue */
	COPY_SCALAR_FIELD(val.type);
	switch (from->val.type)
	{
		case T_Integer:
			COPY_SCALAR_FIELD(val.val.ival);
			break;
		case T_Float:
		case T_String:
		case T_BitString:
			COPY_STRING_FIELD(val.val.str);
			break;
		case T_Null:
			/* nothing to do */
			break;
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) from->val.type);
			break;
	}

	COPY_LOCATION_FIELD(location);

	return newnode;
}

static FuncCall *
_copyFuncCall(const FuncCall *from)
{
	FuncCall   *newnode = makeNode(FuncCall);

	COPY_NODE_FIELD(funcname);
	COPY_NODE_FIELD(args);
	COPY_NODE_FIELD(agg_order);
	COPY_NODE_FIELD(agg_filter);
	COPY_SCALAR_FIELD(agg_within_group);
	COPY_SCALAR_FIELD(agg_star);
	COPY_SCALAR_FIELD(agg_distinct);
	COPY_SCALAR_FIELD(func_variadic);
	COPY_NODE_FIELD(over);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static A_Star *
_copyAStar(const A_Star *from)
{
	A_Star	   *newnode = makeNode(A_Star);

	return newnode;
}

static A_Indices *
_copyAIndices(const A_Indices *from)
{
	A_Indices  *newnode = makeNode(A_Indices);

	COPY_NODE_FIELD(lidx);
	COPY_NODE_FIELD(uidx);

	return newnode;
}

static A_Indirection *
_copyA_Indirection(const A_Indirection *from)
{
	A_Indirection *newnode = makeNode(A_Indirection);

	COPY_NODE_FIELD(arg);
	COPY_NODE_FIELD(indirection);

	return newnode;
}

static A_ArrayExpr *
_copyA_ArrayExpr(const A_ArrayExpr *from)
{
	A_ArrayExpr *newnode = makeNode(A_ArrayExpr);

	COPY_NODE_FIELD(elements);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static ResTarget *
_copyResTarget(const ResTarget *from)
{
	ResTarget  *newnode = makeNode(ResTarget);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(indirection);
	COPY_NODE_FIELD(val);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static TypeName *
_copyTypeName(const TypeName *from)
{
	TypeName   *newnode = makeNode(TypeName);

	COPY_NODE_FIELD(names);
	COPY_SCALAR_FIELD(typeOid);
	COPY_SCALAR_FIELD(setof);
	COPY_SCALAR_FIELD(pct_type);
	COPY_NODE_FIELD(typmods);
	COPY_SCALAR_FIELD(typemod);
	COPY_NODE_FIELD(arrayBounds);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static SortBy *
_copySortBy(const SortBy *from)
{
	SortBy	   *newnode = makeNode(SortBy);

	COPY_NODE_FIELD(node);
	COPY_SCALAR_FIELD(sortby_dir);
	COPY_SCALAR_FIELD(sortby_nulls);
	COPY_NODE_FIELD(useOp);
	COPY_NODE_FIELD(node);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static WindowDef *
_copyWindowDef(const WindowDef *from)
{
	WindowDef  *newnode = makeNode(WindowDef);

	COPY_STRING_FIELD(name);
	COPY_STRING_FIELD(refname);
	COPY_NODE_FIELD(partitionClause);
	COPY_NODE_FIELD(orderClause);
	COPY_SCALAR_FIELD(frameOptions);
	COPY_NODE_FIELD(startOffset);
	COPY_NODE_FIELD(endOffset);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static RangeSubselect *
_copyRangeSubselect(const RangeSubselect *from)
{
	RangeSubselect *newnode = makeNode(RangeSubselect);

	COPY_SCALAR_FIELD(lateral);
	COPY_NODE_FIELD(subquery);
	COPY_NODE_FIELD(alias);

	return newnode;
}

static RangeFunction *
_copyRangeFunction(const RangeFunction *from)
{
	RangeFunction *newnode = makeNode(RangeFunction);

	COPY_SCALAR_FIELD(lateral);
	COPY_SCALAR_FIELD(ordinality);
	COPY_SCALAR_FIELD(is_rowsfrom);
	COPY_NODE_FIELD(functions);
	COPY_NODE_FIELD(alias);
	COPY_NODE_FIELD(coldeflist);

	return newnode;
}

static TypeCast *
_copyTypeCast(const TypeCast *from)
{
	TypeCast   *newnode = makeNode(TypeCast);

	COPY_NODE_FIELD(arg);
	COPY_NODE_FIELD(typeName);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static CollateClause *
_copyCollateClause(const CollateClause *from)
{
	CollateClause *newnode = makeNode(CollateClause);

	COPY_NODE_FIELD(arg);
	COPY_NODE_FIELD(collname);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static IndexElem *
_copyIndexElem(const IndexElem *from)
{
	IndexElem  *newnode = makeNode(IndexElem);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(expr);
	COPY_STRING_FIELD(indexcolname);
	COPY_NODE_FIELD(collation);
	COPY_NODE_FIELD(opclass);
	COPY_SCALAR_FIELD(ordering);
	COPY_SCALAR_FIELD(nulls_ordering);

	return newnode;
}

static ColumnDef *
_copyColumnDef(const ColumnDef *from)
{
	ColumnDef  *newnode = makeNode(ColumnDef);

	COPY_STRING_FIELD(colname);
	COPY_NODE_FIELD(typeName);
	COPY_SCALAR_FIELD(inhcount);
	COPY_SCALAR_FIELD(is_local);
	COPY_SCALAR_FIELD(is_not_null);
	COPY_SCALAR_FIELD(is_from_type);
	COPY_SCALAR_FIELD(attnum);
	COPY_SCALAR_FIELD(storage);
	COPY_NODE_FIELD(raw_default);
	COPY_NODE_FIELD(cooked_default);
	COPY_NODE_FIELD(collClause);
	COPY_SCALAR_FIELD(collOid);
	COPY_NODE_FIELD(constraints);
	COPY_NODE_FIELD(fdwoptions);
	COPY_NODE_FIELD(encoding);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static ColumnReferenceStorageDirective *
_copyColumnReferenceStorageDirective(const ColumnReferenceStorageDirective *from)
{
	ColumnReferenceStorageDirective *newnode =
		makeNode(ColumnReferenceStorageDirective);

	COPY_STRING_FIELD(column);
	COPY_SCALAR_FIELD(deflt);
	COPY_NODE_FIELD(encoding);

	return newnode;
}

static Constraint *
_copyConstraint(const Constraint *from)
{
	Constraint *newnode = makeNode(Constraint);

	COPY_SCALAR_FIELD(contype);
	COPY_STRING_FIELD(conname);
	COPY_SCALAR_FIELD(deferrable);
	COPY_SCALAR_FIELD(initdeferred);
	COPY_LOCATION_FIELD(location);
	COPY_SCALAR_FIELD(is_no_inherit);
	COPY_NODE_FIELD(raw_expr);
	COPY_STRING_FIELD(cooked_expr);
	COPY_NODE_FIELD(keys);
	COPY_NODE_FIELD(exclusions);
	COPY_NODE_FIELD(options);
	COPY_STRING_FIELD(indexname);
	COPY_STRING_FIELD(indexspace);
	COPY_STRING_FIELD(access_method);
	COPY_NODE_FIELD(where_clause);
	COPY_NODE_FIELD(pktable);
	COPY_NODE_FIELD(fk_attrs);
	COPY_NODE_FIELD(pk_attrs);
	COPY_SCALAR_FIELD(fk_matchtype);
	COPY_SCALAR_FIELD(fk_upd_action);
	COPY_SCALAR_FIELD(fk_del_action);
	COPY_NODE_FIELD(old_conpfeqop);
	COPY_SCALAR_FIELD(old_pktable_oid);
	COPY_SCALAR_FIELD(skip_validation);
	COPY_SCALAR_FIELD(initially_valid);

	COPY_SCALAR_FIELD(trig1Oid);
	COPY_SCALAR_FIELD(trig2Oid);
	COPY_SCALAR_FIELD(trig3Oid);
	COPY_SCALAR_FIELD(trig4Oid);

	return newnode;
}

static DefElem *
_copyDefElem(const DefElem *from)
{
	DefElem    *newnode = makeNode(DefElem);

	COPY_STRING_FIELD(defnamespace);
	COPY_STRING_FIELD(defname);
	COPY_NODE_FIELD(arg);
	COPY_SCALAR_FIELD(defaction);

	return newnode;
}

static LockingClause *
_copyLockingClause(const LockingClause *from)
{
	LockingClause *newnode = makeNode(LockingClause);

	COPY_NODE_FIELD(lockedRels);
	COPY_SCALAR_FIELD(strength);
	COPY_SCALAR_FIELD(noWait);

	return newnode;
}

static XmlSerialize *
_copyXmlSerialize(const XmlSerialize *from)
{
	XmlSerialize *newnode = makeNode(XmlSerialize);

	COPY_SCALAR_FIELD(xmloption);
	COPY_NODE_FIELD(expr);
	COPY_NODE_FIELD(typeName);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static DMLActionExpr *
_copyDMLActionExpr(const DMLActionExpr *from)
{
	DMLActionExpr *newnode = makeNode(DMLActionExpr);

	return newnode;
}

static PartSelectedExpr *
_copyPartSelectedExpr(const PartSelectedExpr *from)
{
	PartSelectedExpr *newnode = makeNode(PartSelectedExpr);
	COPY_SCALAR_FIELD(dynamicScanId);
	COPY_SCALAR_FIELD(partOid);

	return newnode;
}

static PartDefaultExpr *
_copyPartDefaultExpr(const PartDefaultExpr *from)
{
	PartDefaultExpr *newnode = makeNode(PartDefaultExpr);
	COPY_SCALAR_FIELD(level);

	return newnode;
}

static PartBoundExpr *
_copyPartBoundExpr(const PartBoundExpr *from)
{
	PartBoundExpr *newnode = makeNode(PartBoundExpr);
	COPY_SCALAR_FIELD(level);
	COPY_SCALAR_FIELD(boundType);
	COPY_SCALAR_FIELD(isLowerBound);

	return newnode;
}

static PartBoundInclusionExpr *
_copyPartBoundInclusionExpr(const PartBoundInclusionExpr *from)
{
	PartBoundInclusionExpr *newnode = makeNode(PartBoundInclusionExpr);
	COPY_SCALAR_FIELD(level);
	COPY_SCALAR_FIELD(isLowerBound);

	return newnode;
}

static PartBoundOpenExpr *
_copyPartBoundOpenExpr(const PartBoundOpenExpr *from)
{
	PartBoundOpenExpr *newnode = makeNode(PartBoundOpenExpr);
	COPY_SCALAR_FIELD(level);
	COPY_SCALAR_FIELD(isLowerBound);

	return newnode;
}

static PartListRuleExpr *
_copyPartListRuleExpr(const PartListRuleExpr *from)
{
	PartListRuleExpr *newnode = makeNode(PartListRuleExpr);
	COPY_SCALAR_FIELD(level);
	COPY_SCALAR_FIELD(resulttype);
	COPY_SCALAR_FIELD(elementtype);

	return newnode;
}

static PartListNullTestExpr *
_copyPartListNullTestExpr(const PartListNullTestExpr *from)
{
	PartListNullTestExpr *newnode = makeNode(PartListNullTestExpr);
	COPY_SCALAR_FIELD(level);
	COPY_SCALAR_FIELD(nulltesttype);

	return newnode;
}

static Query *
_copyQuery(const Query *from)
{
	Query	   *newnode = makeNode(Query);

	COPY_SCALAR_FIELD(commandType);
	COPY_SCALAR_FIELD(querySource);
	COPY_SCALAR_FIELD(queryId);
	COPY_SCALAR_FIELD(canSetTag);
	COPY_NODE_FIELD(utilityStmt);
	COPY_SCALAR_FIELD(resultRelation);
	COPY_SCALAR_FIELD(hasAggs);
	COPY_SCALAR_FIELD(hasWindowFuncs);
	COPY_SCALAR_FIELD(hasSubLinks);
	COPY_SCALAR_FIELD(hasDynamicFunctions);
	COPY_SCALAR_FIELD(hasFuncsWithExecRestrictions);
	COPY_SCALAR_FIELD(hasDistinctOn);
	COPY_SCALAR_FIELD(hasRecursive);
	COPY_SCALAR_FIELD(hasModifyingCTE);
	COPY_SCALAR_FIELD(hasForUpdate);
	COPY_NODE_FIELD(cteList);
	COPY_NODE_FIELD(rtable);
	COPY_NODE_FIELD(jointree);
	COPY_NODE_FIELD(targetList);
	COPY_NODE_FIELD(withCheckOptions);
	COPY_NODE_FIELD(returningList);
	COPY_NODE_FIELD(groupClause);
	COPY_NODE_FIELD(havingQual);
	COPY_NODE_FIELD(windowClause);
	COPY_NODE_FIELD(distinctClause);
	COPY_NODE_FIELD(sortClause);
	COPY_NODE_FIELD(scatterClause);
	COPY_SCALAR_FIELD(isTableValueSelect);
	COPY_NODE_FIELD(limitOffset);
	COPY_NODE_FIELD(limitCount);
	COPY_NODE_FIELD(rowMarks);
	COPY_NODE_FIELD(setOperations);
	COPY_NODE_FIELD(constraintDeps);
	COPY_NODE_FIELD(intoPolicy);
	COPY_SCALAR_FIELD(parentStmtType);

	return newnode;
}

static InsertStmt *
_copyInsertStmt(const InsertStmt *from)
{
	InsertStmt *newnode = makeNode(InsertStmt);

	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(cols);
	COPY_NODE_FIELD(selectStmt);
	COPY_NODE_FIELD(returningList);
	COPY_NODE_FIELD(withClause);

	return newnode;
}

static DeleteStmt *
_copyDeleteStmt(const DeleteStmt *from)
{
	DeleteStmt *newnode = makeNode(DeleteStmt);

	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(usingClause);
	COPY_NODE_FIELD(whereClause);
	COPY_NODE_FIELD(returningList);
	COPY_NODE_FIELD(withClause);

	return newnode;
}

static UpdateStmt *
_copyUpdateStmt(const UpdateStmt *from)
{
	UpdateStmt *newnode = makeNode(UpdateStmt);

	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(targetList);
	COPY_NODE_FIELD(whereClause);
	COPY_NODE_FIELD(fromClause);
	COPY_NODE_FIELD(returningList);
	COPY_NODE_FIELD(withClause);

	return newnode;
}

static SelectStmt *
_copySelectStmt(const SelectStmt *from)
{
	SelectStmt *newnode = makeNode(SelectStmt);

	COPY_NODE_FIELD(distinctClause);
	COPY_NODE_FIELD(intoClause);
	COPY_NODE_FIELD(targetList);
	COPY_NODE_FIELD(fromClause);
	COPY_NODE_FIELD(whereClause);
	COPY_NODE_FIELD(groupClause);
	COPY_NODE_FIELD(havingClause);
	COPY_NODE_FIELD(windowClause);
	COPY_NODE_FIELD(valuesLists);
	COPY_NODE_FIELD(sortClause);
	COPY_NODE_FIELD(scatterClause);
	COPY_NODE_FIELD(limitOffset);
	COPY_NODE_FIELD(limitCount);
	COPY_NODE_FIELD(lockingClause);
	COPY_NODE_FIELD(withClause);
	COPY_SCALAR_FIELD(op);
	COPY_SCALAR_FIELD(all);
	COPY_NODE_FIELD(larg);
	COPY_NODE_FIELD(rarg);

	return newnode;
}

static SetOperationStmt *
_copySetOperationStmt(const SetOperationStmt *from)
{
	SetOperationStmt *newnode = makeNode(SetOperationStmt);

	COPY_SCALAR_FIELD(op);
	COPY_SCALAR_FIELD(all);
	COPY_NODE_FIELD(larg);
	COPY_NODE_FIELD(rarg);
	COPY_NODE_FIELD(colTypes);
	COPY_NODE_FIELD(colTypmods);
	COPY_NODE_FIELD(colCollations);
	COPY_NODE_FIELD(groupClauses);

	return newnode;
}

static AlterTableStmt *
_copyAlterTableStmt(const AlterTableStmt *from)
{
	AlterTableStmt *newnode = makeNode(AlterTableStmt);

	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(cmds);
	COPY_SCALAR_FIELD(relkind);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static AlterTableCmd *
_copyAlterTableCmd(const AlterTableCmd *from)
{
	AlterTableCmd *newnode = makeNode(AlterTableCmd);

	COPY_SCALAR_FIELD(subtype);
	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(def);
	COPY_SCALAR_FIELD(behavior);
	COPY_SCALAR_FIELD(part_expanded);

	/* Need to copy AT workspace since process uses copy internally. */
	COPY_NODE_FIELD(partoids);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static SetDistributionCmd*
_copySetDistributionCmd(const SetDistributionCmd *from)
{
	SetDistributionCmd *newnode = makeNode(SetDistributionCmd);

	COPY_SCALAR_FIELD(backendId);
	COPY_NODE_FIELD(relids);

	return newnode;
}

static InheritPartitionCmd *
_copyInheritPartitionCmd(const InheritPartitionCmd *from)
{
	InheritPartitionCmd *newnode = makeNode(InheritPartitionCmd);

	COPY_NODE_FIELD(parent);

	return newnode;
}

static AlterPartitionCmd *
_copyAlterPartitionCmd(const AlterPartitionCmd *from)
{
	AlterPartitionCmd *newnode = makeNode(AlterPartitionCmd);

	COPY_NODE_FIELD(partid);
	COPY_NODE_FIELD(arg1);
	COPY_NODE_FIELD(arg2);

	return newnode;
}

static AlterPartitionId *
_copyAlterPartitionId(const AlterPartitionId *from)
{
	AlterPartitionId *newnode = makeNode(AlterPartitionId);

	COPY_SCALAR_FIELD(idtype);
	COPY_NODE_FIELD(partiddef);

	return newnode;
}


static AlterDomainStmt *
_copyAlterDomainStmt(const AlterDomainStmt *from)
{
	AlterDomainStmt *newnode = makeNode(AlterDomainStmt);

	COPY_SCALAR_FIELD(subtype);
	COPY_NODE_FIELD(typeName);
	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(def);
	COPY_SCALAR_FIELD(behavior);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static GrantStmt *
_copyGrantStmt(const GrantStmt *from)
{
	GrantStmt  *newnode = makeNode(GrantStmt);

	COPY_SCALAR_FIELD(is_grant);
	COPY_SCALAR_FIELD(targtype);
	COPY_SCALAR_FIELD(objtype);
	COPY_NODE_FIELD(objects);
	COPY_NODE_FIELD(privileges);
	COPY_NODE_FIELD(grantees);
	COPY_SCALAR_FIELD(grant_option);
	COPY_SCALAR_FIELD(behavior);

	return newnode;
}

static PrivGrantee *
_copyPrivGrantee(const PrivGrantee *from)
{
	PrivGrantee *newnode = makeNode(PrivGrantee);

	COPY_STRING_FIELD(rolname);

	return newnode;
}

static FuncWithArgs *
_copyFuncWithArgs(const FuncWithArgs *from)
{
	FuncWithArgs *newnode = makeNode(FuncWithArgs);

	COPY_NODE_FIELD(funcname);
	COPY_NODE_FIELD(funcargs);

	return newnode;
}

static AccessPriv *
_copyAccessPriv(const AccessPriv *from)
{
	AccessPriv *newnode = makeNode(AccessPriv);

	COPY_STRING_FIELD(priv_name);
	COPY_NODE_FIELD(cols);

	return newnode;
}

static GrantRoleStmt *
_copyGrantRoleStmt(const GrantRoleStmt *from)
{
	GrantRoleStmt *newnode = makeNode(GrantRoleStmt);

	COPY_NODE_FIELD(granted_roles);
	COPY_NODE_FIELD(grantee_roles);
	COPY_SCALAR_FIELD(is_grant);
	COPY_SCALAR_FIELD(admin_opt);
	COPY_STRING_FIELD(grantor);
	COPY_SCALAR_FIELD(behavior);

	return newnode;
}

static AlterDefaultPrivilegesStmt *
_copyAlterDefaultPrivilegesStmt(const AlterDefaultPrivilegesStmt *from)
{
	AlterDefaultPrivilegesStmt *newnode = makeNode(AlterDefaultPrivilegesStmt);

	COPY_NODE_FIELD(options);
	COPY_NODE_FIELD(action);

	return newnode;
}

static DeclareCursorStmt *
_copyDeclareCursorStmt(const DeclareCursorStmt *from)
{
	DeclareCursorStmt *newnode = makeNode(DeclareCursorStmt);

	COPY_STRING_FIELD(portalname);
	COPY_SCALAR_FIELD(options);
	COPY_NODE_FIELD(query);

	return newnode;
}

static ClosePortalStmt *
_copyClosePortalStmt(const ClosePortalStmt *from)
{
	ClosePortalStmt *newnode = makeNode(ClosePortalStmt);

	COPY_STRING_FIELD(portalname);

	return newnode;
}

static ClusterStmt *
_copyClusterStmt(const ClusterStmt *from)
{
	ClusterStmt *newnode = makeNode(ClusterStmt);

	COPY_NODE_FIELD(relation);
	COPY_STRING_FIELD(indexname);
	COPY_SCALAR_FIELD(verbose);

	return newnode;
}

static SingleRowErrorDesc *
_copySingleRowErrorDesc(const SingleRowErrorDesc *from)
{
	SingleRowErrorDesc *newnode = makeNode(SingleRowErrorDesc);

	COPY_SCALAR_FIELD(rejectlimit);
	COPY_SCALAR_FIELD(is_limit_in_rows);
	COPY_SCALAR_FIELD(into_file);
	COPY_SCALAR_FIELD(log_errors_type);

	return newnode;
}

static CopyStmt *
_copyCopyStmt(const CopyStmt *from)
{
	CopyStmt   *newnode = makeNode(CopyStmt);

	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(query);
	COPY_NODE_FIELD(attlist);
	COPY_SCALAR_FIELD(is_from);
	COPY_SCALAR_FIELD(is_program);
	COPY_SCALAR_FIELD(skip_ext_partition);
	COPY_STRING_FIELD(filename);
	COPY_NODE_FIELD(options);
	COPY_NODE_FIELD(sreh);
	return newnode;
}

/*
 * CopyCreateStmtFields
 *
 *		This function copies the fields of the CreateStmt node.  It is used by
 *		copy functions for classes which inherit from CreateStmt.
 */
static void
CopyCreateStmtFields(const CreateStmt *from, CreateStmt *newnode)
{
	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(tableElts);
	COPY_NODE_FIELD(inhRelations);
	COPY_NODE_FIELD(inhOids);
	COPY_SCALAR_FIELD(parentOidCount);
	COPY_NODE_FIELD(ofTypename);
	COPY_NODE_FIELD(constraints);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(oncommit);
	COPY_STRING_FIELD(tablespacename);
	COPY_SCALAR_FIELD(if_not_exists);

	COPY_NODE_FIELD(distributedBy);
	COPY_NODE_FIELD(partitionBy);
	COPY_SCALAR_FIELD(relKind);
	COPY_SCALAR_FIELD(relStorage);
	COPY_NODE_FIELD(deferredStmts);
	COPY_SCALAR_FIELD(is_part_child);
	COPY_SCALAR_FIELD(is_part_parent);
	COPY_SCALAR_FIELD(is_add_part);
	COPY_SCALAR_FIELD(is_split_part);
	COPY_SCALAR_FIELD(ownerid);
	COPY_SCALAR_FIELD(buildAoBlkdir);
	COPY_NODE_FIELD(attr_encodings);
	COPY_SCALAR_FIELD(isCtas);
}

static CreateStmt *
_copyCreateStmt(const CreateStmt *from)
{
	CreateStmt *newnode = makeNode(CreateStmt);

	CopyCreateStmtFields(from, newnode);

	return newnode;
}

static TableLikeClause *
_copyTableLikeClause(const TableLikeClause *from)
{
	TableLikeClause *newnode = makeNode(TableLikeClause);

	COPY_NODE_FIELD(relation);
	COPY_SCALAR_FIELD(options);

	return newnode;
}

static PartitionBy *
_copyPartitionBy(const PartitionBy *from)
{
	PartitionBy *newnode = makeNode(PartitionBy);

	COPY_SCALAR_FIELD(partType);
	COPY_NODE_FIELD(keys);
	COPY_NODE_FIELD(keyopclass);
	COPY_NODE_FIELD(subPart);
	COPY_NODE_FIELD(partSpec);
	COPY_SCALAR_FIELD(partDepth);
	COPY_NODE_FIELD(parentRel);
	COPY_SCALAR_FIELD(partQuiet);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static PartitionSpec *
_copyPartitionSpec(const PartitionSpec *from)
{
	PartitionSpec *newnode = makeNode(PartitionSpec);

	COPY_NODE_FIELD(partElem);
	COPY_NODE_FIELD(subSpec);
	COPY_SCALAR_FIELD(istemplate);
	COPY_NODE_FIELD(enc_clauses);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static PartitionValuesSpec *
_copyPartitionValuesSpec(const PartitionValuesSpec *from)
{
	PartitionValuesSpec *newnode = makeNode(PartitionValuesSpec);

	COPY_NODE_FIELD(partValues);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static ExpandStmtSpec *
_copyExpandStmtSpec(const ExpandStmtSpec *from)
{
	ExpandStmtSpec *newnode = makeNode(ExpandStmtSpec);

	COPY_SCALAR_FIELD(backendId);

	return newnode;
}

static PartitionElem *
_copyPartitionElem(const PartitionElem *from)
{
	PartitionElem *newnode = makeNode(PartitionElem);

	COPY_STRING_FIELD(partName);
	COPY_NODE_FIELD(boundSpec);
	COPY_NODE_FIELD(subSpec);
	COPY_SCALAR_FIELD(isDefault);
	COPY_NODE_FIELD(storeAttr);
	COPY_SCALAR_FIELD(partno);
	COPY_SCALAR_FIELD(rrand);
	COPY_NODE_FIELD(colencs);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static PartitionRangeItem *
_copyPartitionRangeItem(const PartitionRangeItem *from)
{
	PartitionRangeItem *newnode = makeNode(PartitionRangeItem);

	COPY_NODE_FIELD(partRangeVal);
	COPY_SCALAR_FIELD(partedge);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static PartitionBoundSpec *
_copyPartitionBoundSpec(const PartitionBoundSpec *from)
{
	PartitionBoundSpec *newnode = makeNode(PartitionBoundSpec);

	COPY_NODE_FIELD(partStart);
	COPY_NODE_FIELD(partEnd);
	COPY_NODE_FIELD(partEvery);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static PgPartRule *
_copyPgPartRule(const PgPartRule *from)
{
	PgPartRule *newnode = makeNode(PgPartRule);

	COPY_NODE_FIELD(pNode);
	COPY_NODE_FIELD(topRule);
	COPY_STRING_FIELD(partIdStr);
	COPY_SCALAR_FIELD(isName);
	COPY_SCALAR_FIELD(topRuleRank);
	COPY_STRING_FIELD(relname);

	return newnode;
}

static Partition *
_copyPartition(const Partition *from)
{
	Partition *newnode = makeNode(Partition);

	COPY_SCALAR_FIELD(partid);
	COPY_SCALAR_FIELD(parrelid);
	COPY_SCALAR_FIELD(parkind);
	COPY_SCALAR_FIELD(parlevel);
	COPY_SCALAR_FIELD(paristemplate);
	COPY_SCALAR_FIELD(parnatts);
	COPY_POINTER_FIELD(paratts, from->parnatts * sizeof(AttrNumber));
	COPY_POINTER_FIELD(parclass, from->parnatts * sizeof(Oid));

	return newnode;
}

static PartitionRule *
_copyPartitionRule(const PartitionRule *from)
{
	PartitionRule *newnode = makeNode(PartitionRule);

	COPY_SCALAR_FIELD(parruleid);
	COPY_SCALAR_FIELD(paroid);
	COPY_SCALAR_FIELD(parchildrelid);
	COPY_SCALAR_FIELD(parparentoid);
	COPY_SCALAR_FIELD(parisdefault);
	COPY_STRING_FIELD(parname);
	COPY_NODE_FIELD(parrangestart);
	COPY_SCALAR_FIELD(parrangestartincl);
	COPY_NODE_FIELD(parrangeend);
	COPY_SCALAR_FIELD(parrangeendincl);
	COPY_NODE_FIELD(parrangeevery);
	COPY_NODE_FIELD(parlistvalues);
	COPY_SCALAR_FIELD(parruleord);
	COPY_NODE_FIELD(parreloptions);
	COPY_SCALAR_FIELD(partemplatespaceId);
	COPY_NODE_FIELD(children); /* sub partition */

	return newnode;
}

static PartitionNode *
_copyPartitionNode(const PartitionNode *from)
{
	PartitionNode *newnode = makeNode(PartitionNode);

	COPY_NODE_FIELD(part);
	COPY_NODE_FIELD(default_part);
	COPY_NODE_FIELD(rules);

	return newnode;
}

static ExtTableTypeDesc *
_copyExtTableTypeDesc(const ExtTableTypeDesc *from)
{
	ExtTableTypeDesc *newnode = makeNode(ExtTableTypeDesc);

	COPY_SCALAR_FIELD(exttabletype);
	COPY_NODE_FIELD(location_list);
	COPY_NODE_FIELD(on_clause);
	COPY_STRING_FIELD(command_string);

	return newnode;
}

static CreateExternalStmt *
_copyCreateExternalStmt(const CreateExternalStmt *from)
{
	CreateExternalStmt *newnode = makeNode(CreateExternalStmt);

	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(tableElts);
	COPY_NODE_FIELD(exttypedesc);
	COPY_STRING_FIELD(format);
	COPY_NODE_FIELD(formatOpts);
	COPY_SCALAR_FIELD(isweb);
	COPY_SCALAR_FIELD(iswritable);
	COPY_NODE_FIELD(sreh);
	COPY_NODE_FIELD(extOptions);
	COPY_NODE_FIELD(encoding);
	COPY_NODE_FIELD(distributedBy);

	return newnode;
}

static DefineStmt *
_copyDefineStmt(const DefineStmt *from)
{
	DefineStmt *newnode = makeNode(DefineStmt);

	COPY_SCALAR_FIELD(kind);
	COPY_SCALAR_FIELD(oldstyle);
	COPY_NODE_FIELD(defnames);
	COPY_NODE_FIELD(args);
	COPY_NODE_FIELD(definition);
	COPY_SCALAR_FIELD(trusted);  /* CDB */

	return newnode;
}

static DropStmt *
_copyDropStmt(const DropStmt *from)
{
	DropStmt   *newnode = makeNode(DropStmt);

	COPY_NODE_FIELD(objects);
	COPY_NODE_FIELD(arguments);
	COPY_SCALAR_FIELD(removeType);
	COPY_SCALAR_FIELD(behavior);
	COPY_SCALAR_FIELD(missing_ok);
	COPY_SCALAR_FIELD(concurrent);
	COPY_SCALAR_FIELD(bAllowPartn);

	return newnode;
}

static TruncateStmt *
_copyTruncateStmt(const TruncateStmt *from)
{
	TruncateStmt *newnode = makeNode(TruncateStmt);

	COPY_NODE_FIELD(relations);
	COPY_SCALAR_FIELD(restart_seqs);
	COPY_SCALAR_FIELD(behavior);

	return newnode;
}

static CommentStmt *
_copyCommentStmt(const CommentStmt *from)
{
	CommentStmt *newnode = makeNode(CommentStmt);

	COPY_SCALAR_FIELD(objtype);
	COPY_NODE_FIELD(objname);
	COPY_NODE_FIELD(objargs);
	COPY_STRING_FIELD(comment);

	return newnode;
}

static SecLabelStmt *
_copySecLabelStmt(const SecLabelStmt *from)
{
	SecLabelStmt *newnode = makeNode(SecLabelStmt);

	COPY_SCALAR_FIELD(objtype);
	COPY_NODE_FIELD(objname);
	COPY_NODE_FIELD(objargs);
	COPY_STRING_FIELD(provider);
	COPY_STRING_FIELD(label);

	return newnode;
}

static FetchStmt *
_copyFetchStmt(const FetchStmt *from)
{
	FetchStmt  *newnode = makeNode(FetchStmt);

	COPY_SCALAR_FIELD(direction);
	COPY_SCALAR_FIELD(howMany);
	COPY_STRING_FIELD(portalname);
	COPY_SCALAR_FIELD(ismove);

	return newnode;
}

static IndexStmt *
_copyIndexStmt(const IndexStmt *from)
{
	IndexStmt  *newnode = makeNode(IndexStmt);

	COPY_STRING_FIELD(idxname);
	COPY_NODE_FIELD(relation);
	COPY_STRING_FIELD(accessMethod);
	COPY_STRING_FIELD(tableSpace);
	COPY_NODE_FIELD(indexParams);
	COPY_NODE_FIELD(options);
	COPY_NODE_FIELD(whereClause);
	COPY_NODE_FIELD(excludeOpNames);
	COPY_STRING_FIELD(idxcomment);
	COPY_SCALAR_FIELD(indexOid);
	COPY_SCALAR_FIELD(oldNode);
	COPY_SCALAR_FIELD(is_part_child);
	COPY_SCALAR_FIELD(unique);
	COPY_SCALAR_FIELD(primary);
	COPY_SCALAR_FIELD(isconstraint);
	COPY_SCALAR_FIELD(deferrable);
	COPY_SCALAR_FIELD(initdeferred);
	COPY_SCALAR_FIELD(concurrent);
	COPY_SCALAR_FIELD(is_split_part);
	COPY_SCALAR_FIELD(parentIndexId);
	COPY_SCALAR_FIELD(parentConstraintId);

	return newnode;
}

static CreateFunctionStmt *
_copyCreateFunctionStmt(const CreateFunctionStmt *from)
{
	CreateFunctionStmt *newnode = makeNode(CreateFunctionStmt);

	COPY_SCALAR_FIELD(replace);
	COPY_NODE_FIELD(funcname);
	COPY_NODE_FIELD(parameters);
	COPY_NODE_FIELD(returnType);
	COPY_NODE_FIELD(options);
	COPY_NODE_FIELD(withClause);

	return newnode;
}

static FunctionParameter *
_copyFunctionParameter(const FunctionParameter *from)
{
	FunctionParameter *newnode = makeNode(FunctionParameter);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(argType);
	COPY_SCALAR_FIELD(mode);
	COPY_NODE_FIELD(defexpr);

	return newnode;
}

static AlterFunctionStmt *
_copyAlterFunctionStmt(const AlterFunctionStmt *from)
{
	AlterFunctionStmt *newnode = makeNode(AlterFunctionStmt);

	COPY_NODE_FIELD(func);
	COPY_NODE_FIELD(actions);

	return newnode;
}

static DoStmt *
_copyDoStmt(const DoStmt *from)
{
	DoStmt	   *newnode = makeNode(DoStmt);

	COPY_NODE_FIELD(args);

	return newnode;
}

static RenameStmt *
_copyRenameStmt(const RenameStmt *from)
{
	RenameStmt *newnode = makeNode(RenameStmt);

	COPY_SCALAR_FIELD(renameType);
	COPY_SCALAR_FIELD(relationType);
	COPY_NODE_FIELD(relation);
	COPY_SCALAR_FIELD(objid);
	COPY_NODE_FIELD(object);
	COPY_NODE_FIELD(objarg);
	COPY_STRING_FIELD(subname);
	COPY_STRING_FIELD(newname);
	COPY_SCALAR_FIELD(behavior);
	COPY_SCALAR_FIELD(bAllowPartn);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static AlterObjectSchemaStmt *
_copyAlterObjectSchemaStmt(const AlterObjectSchemaStmt *from)
{
	AlterObjectSchemaStmt *newnode = makeNode(AlterObjectSchemaStmt);

	COPY_SCALAR_FIELD(objectType);
	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(object);
	COPY_NODE_FIELD(objarg);
	COPY_STRING_FIELD(newschema);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static AlterOwnerStmt *
_copyAlterOwnerStmt(const AlterOwnerStmt *from)
{
	AlterOwnerStmt *newnode = makeNode(AlterOwnerStmt);

	COPY_SCALAR_FIELD(objectType);
	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(object);
	COPY_NODE_FIELD(objarg);
	COPY_STRING_FIELD(newowner);

	return newnode;
}

static RuleStmt *
_copyRuleStmt(const RuleStmt *from)
{
	RuleStmt   *newnode = makeNode(RuleStmt);

	COPY_NODE_FIELD(relation);
	COPY_STRING_FIELD(rulename);
	COPY_NODE_FIELD(whereClause);
	COPY_SCALAR_FIELD(event);
	COPY_SCALAR_FIELD(instead);
	COPY_NODE_FIELD(actions);
	COPY_SCALAR_FIELD(replace);

	return newnode;
}

static NotifyStmt *
_copyNotifyStmt(const NotifyStmt *from)
{
	NotifyStmt *newnode = makeNode(NotifyStmt);

	COPY_STRING_FIELD(conditionname);
	COPY_STRING_FIELD(payload);

	return newnode;
}

static ListenStmt *
_copyListenStmt(const ListenStmt *from)
{
	ListenStmt *newnode = makeNode(ListenStmt);

	COPY_STRING_FIELD(conditionname);

	return newnode;
}

static UnlistenStmt *
_copyUnlistenStmt(const UnlistenStmt *from)
{
	UnlistenStmt *newnode = makeNode(UnlistenStmt);

	COPY_STRING_FIELD(conditionname);

	return newnode;
}

static TransactionStmt *
_copyTransactionStmt(const TransactionStmt *from)
{
	TransactionStmt *newnode = makeNode(TransactionStmt);

	COPY_SCALAR_FIELD(kind);
	COPY_NODE_FIELD(options);
	COPY_STRING_FIELD(gid);

	return newnode;
}

static CompositeTypeStmt *
_copyCompositeTypeStmt(const CompositeTypeStmt *from)
{
	CompositeTypeStmt *newnode = makeNode(CompositeTypeStmt);

	COPY_NODE_FIELD(typevar);
	COPY_NODE_FIELD(coldeflist);

	return newnode;
}

static CreateEnumStmt *
_copyCreateEnumStmt(const CreateEnumStmt *from)
{
	CreateEnumStmt *newnode = makeNode(CreateEnumStmt);

	COPY_NODE_FIELD(typeName);
	COPY_NODE_FIELD(vals);

	return newnode;
}

static CreateRangeStmt *
_copyCreateRangeStmt(const CreateRangeStmt *from)
{
	CreateRangeStmt *newnode = makeNode(CreateRangeStmt);

	COPY_NODE_FIELD(typeName);
	COPY_NODE_FIELD(params);

	return newnode;
}

static AlterEnumStmt *
_copyAlterEnumStmt(const AlterEnumStmt *from)
{
	AlterEnumStmt *newnode = makeNode(AlterEnumStmt);

	COPY_NODE_FIELD(typeName);
	COPY_STRING_FIELD(newVal);
	COPY_STRING_FIELD(newValNeighbor);
	COPY_SCALAR_FIELD(newValIsAfter);
	COPY_SCALAR_FIELD(skipIfExists);

	return newnode;
}

static ViewStmt *
_copyViewStmt(const ViewStmt *from)
{
	ViewStmt   *newnode = makeNode(ViewStmt);

	COPY_NODE_FIELD(view);
	COPY_NODE_FIELD(aliases);
	COPY_NODE_FIELD(query);
	COPY_SCALAR_FIELD(replace);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(withCheckOption);

	return newnode;
}

static LoadStmt *
_copyLoadStmt(const LoadStmt *from)
{
	LoadStmt   *newnode = makeNode(LoadStmt);

	COPY_STRING_FIELD(filename);

	return newnode;
}

static CreateDomainStmt *
_copyCreateDomainStmt(const CreateDomainStmt *from)
{
	CreateDomainStmt *newnode = makeNode(CreateDomainStmt);

	COPY_NODE_FIELD(domainname);
	COPY_NODE_FIELD(typeName);
	COPY_NODE_FIELD(collClause);
	COPY_NODE_FIELD(constraints);

	return newnode;
}

static CreateOpClassStmt *
_copyCreateOpClassStmt(const CreateOpClassStmt *from)
{
	CreateOpClassStmt *newnode = makeNode(CreateOpClassStmt);

	COPY_NODE_FIELD(opclassname);
	COPY_NODE_FIELD(opfamilyname);
	COPY_STRING_FIELD(amname);
	COPY_NODE_FIELD(datatype);
	COPY_NODE_FIELD(items);
	COPY_SCALAR_FIELD(isDefault);

	return newnode;
}

static CreateOpClassItem *
_copyCreateOpClassItem(const CreateOpClassItem *from)
{
	CreateOpClassItem *newnode = makeNode(CreateOpClassItem);

	COPY_SCALAR_FIELD(itemtype);
	COPY_NODE_FIELD(name);
	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(number);
	COPY_NODE_FIELD(order_family);
	COPY_NODE_FIELD(class_args);
	COPY_NODE_FIELD(storedtype);

	return newnode;
}

static CreateOpFamilyStmt *
_copyCreateOpFamilyStmt(const CreateOpFamilyStmt *from)
{
	CreateOpFamilyStmt *newnode = makeNode(CreateOpFamilyStmt);

	COPY_NODE_FIELD(opfamilyname);
	COPY_STRING_FIELD(amname);

	return newnode;
}

static AlterOpFamilyStmt *
_copyAlterOpFamilyStmt(const AlterOpFamilyStmt *from)
{
	AlterOpFamilyStmt *newnode = makeNode(AlterOpFamilyStmt);

	COPY_NODE_FIELD(opfamilyname);
	COPY_STRING_FIELD(amname);
	COPY_SCALAR_FIELD(isDrop);
	COPY_NODE_FIELD(items);

	return newnode;
}

static CreatedbStmt *
_copyCreatedbStmt(const CreatedbStmt *from)
{
	CreatedbStmt *newnode = makeNode(CreatedbStmt);

	COPY_STRING_FIELD(dbname);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterDatabaseStmt *
_copyAlterDatabaseStmt(const AlterDatabaseStmt *from)
{
	AlterDatabaseStmt *newnode = makeNode(AlterDatabaseStmt);

	COPY_STRING_FIELD(dbname);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterDatabaseSetStmt *
_copyAlterDatabaseSetStmt(const AlterDatabaseSetStmt *from)
{
	AlterDatabaseSetStmt *newnode = makeNode(AlterDatabaseSetStmt);

	COPY_STRING_FIELD(dbname);
	COPY_NODE_FIELD(setstmt);

	return newnode;
}

static DropdbStmt *
_copyDropdbStmt(const DropdbStmt *from)
{
	DropdbStmt *newnode = makeNode(DropdbStmt);

	COPY_STRING_FIELD(dbname);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static VacuumStmt *
_copyVacuumStmt(const VacuumStmt *from)
{
	VacuumStmt *newnode = makeNode(VacuumStmt);

	COPY_SCALAR_FIELD(options);
	COPY_SCALAR_FIELD(freeze_min_age);
	COPY_SCALAR_FIELD(freeze_table_age);
	COPY_SCALAR_FIELD(multixact_freeze_min_age);
	COPY_SCALAR_FIELD(multixact_freeze_table_age);
	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(va_cols);

	COPY_SCALAR_FIELD(skip_twophase);
	COPY_NODE_FIELD(expanded_relids);
	COPY_NODE_FIELD(appendonly_compaction_segno);
	COPY_NODE_FIELD(appendonly_compaction_insert_segno);
	COPY_SCALAR_FIELD(appendonly_phase);
	COPY_SCALAR_FIELD(appendonly_relation_empty);

	return newnode;
}

static ExplainStmt *
_copyExplainStmt(const ExplainStmt *from)
{
	ExplainStmt *newnode = makeNode(ExplainStmt);

	COPY_NODE_FIELD(query);
	COPY_NODE_FIELD(options);

	return newnode;
}

static CreateTableAsStmt *
_copyCreateTableAsStmt(const CreateTableAsStmt *from)
{
	CreateTableAsStmt *newnode = makeNode(CreateTableAsStmt);

	COPY_NODE_FIELD(query);
	COPY_NODE_FIELD(into);
	COPY_SCALAR_FIELD(relkind);
	COPY_SCALAR_FIELD(is_select_into);

	return newnode;
}

static RefreshMatViewStmt *
_copyRefreshMatViewStmt(const RefreshMatViewStmt *from)
{
	RefreshMatViewStmt *newnode = makeNode(RefreshMatViewStmt);

	COPY_SCALAR_FIELD(concurrent);
	COPY_SCALAR_FIELD(skipData);
	COPY_NODE_FIELD(relation);

	return newnode;
}

static ReplicaIdentityStmt *
_copyReplicaIdentityStmt(const ReplicaIdentityStmt *from)
{
	ReplicaIdentityStmt *newnode = makeNode(ReplicaIdentityStmt);

	COPY_SCALAR_FIELD(identity_type);
	COPY_STRING_FIELD(name);

	return newnode;
}

static AlterSystemStmt *
_copyAlterSystemStmt(const AlterSystemStmt *from)
{
	AlterSystemStmt *newnode = makeNode(AlterSystemStmt);

	COPY_NODE_FIELD(setstmt);

	return newnode;
}

static CreateSeqStmt *
_copyCreateSeqStmt(const CreateSeqStmt *from)
{
	CreateSeqStmt *newnode = makeNode(CreateSeqStmt);

	COPY_NODE_FIELD(sequence);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(ownerId);

	return newnode;
}

static AlterSeqStmt *
_copyAlterSeqStmt(const AlterSeqStmt *from)
{
	AlterSeqStmt *newnode = makeNode(AlterSeqStmt);

	COPY_NODE_FIELD(sequence);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static VariableSetStmt *
_copyVariableSetStmt(const VariableSetStmt *from)
{
	VariableSetStmt *newnode = makeNode(VariableSetStmt);

	COPY_SCALAR_FIELD(kind);
	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(is_local);

	return newnode;
}

static VariableShowStmt *
_copyVariableShowStmt(const VariableShowStmt *from)
{
	VariableShowStmt *newnode = makeNode(VariableShowStmt);

	COPY_STRING_FIELD(name);

	return newnode;
}

static DiscardStmt *
_copyDiscardStmt(const DiscardStmt *from)
{
	DiscardStmt *newnode = makeNode(DiscardStmt);

	COPY_SCALAR_FIELD(target);

	return newnode;
}

static CreateTableSpaceStmt *
_copyCreateTableSpaceStmt(const CreateTableSpaceStmt *from)
{
	CreateTableSpaceStmt *newnode = makeNode(CreateTableSpaceStmt);

	COPY_STRING_FIELD(tablespacename);
	COPY_STRING_FIELD(owner);
	COPY_STRING_FIELD(location);
	COPY_NODE_FIELD(options);

	return newnode;
}

static DropTableSpaceStmt *
_copyDropTableSpaceStmt(const DropTableSpaceStmt *from)
{
	DropTableSpaceStmt *newnode = makeNode(DropTableSpaceStmt);

	COPY_STRING_FIELD(tablespacename);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static AlterTableSpaceOptionsStmt *
_copyAlterTableSpaceOptionsStmt(const AlterTableSpaceOptionsStmt *from)
{
	AlterTableSpaceOptionsStmt *newnode = makeNode(AlterTableSpaceOptionsStmt);

	COPY_STRING_FIELD(tablespacename);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(isReset);

	return newnode;
}

static AlterTableMoveAllStmt *
_copyAlterTableMoveAllStmt(const AlterTableMoveAllStmt *from)
{
	AlterTableMoveAllStmt *newnode = makeNode(AlterTableMoveAllStmt);

	COPY_STRING_FIELD(orig_tablespacename);
	COPY_SCALAR_FIELD(objtype);
	COPY_NODE_FIELD(roles);
	COPY_STRING_FIELD(new_tablespacename);
	COPY_SCALAR_FIELD(nowait);

	return newnode;
}

static CreateExtensionStmt *
_copyCreateExtensionStmt(const CreateExtensionStmt *from)
{
	CreateExtensionStmt *newnode = makeNode(CreateExtensionStmt);

	COPY_STRING_FIELD(extname);
	COPY_SCALAR_FIELD(if_not_exists);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(create_ext_state);

	return newnode;
}

static AlterExtensionStmt *
_copyAlterExtensionStmt(const AlterExtensionStmt *from)
{
	AlterExtensionStmt *newnode = makeNode(AlterExtensionStmt);

	COPY_STRING_FIELD(extname);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(update_ext_state);

	return newnode;
}

static AlterExtensionContentsStmt *
_copyAlterExtensionContentsStmt(const AlterExtensionContentsStmt *from)
{
	AlterExtensionContentsStmt *newnode = makeNode(AlterExtensionContentsStmt);

	COPY_STRING_FIELD(extname);
	COPY_SCALAR_FIELD(action);
	COPY_SCALAR_FIELD(objtype);
	COPY_NODE_FIELD(objname);
	COPY_NODE_FIELD(objargs);

	return newnode;
}

static CreateFdwStmt *
_copyCreateFdwStmt(const CreateFdwStmt *from)
{
	CreateFdwStmt *newnode = makeNode(CreateFdwStmt);

	COPY_STRING_FIELD(fdwname);
	COPY_NODE_FIELD(func_options);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterFdwStmt *
_copyAlterFdwStmt(const AlterFdwStmt *from)
{
	AlterFdwStmt *newnode = makeNode(AlterFdwStmt);

	COPY_STRING_FIELD(fdwname);
	COPY_NODE_FIELD(func_options);
	COPY_NODE_FIELD(options);

	return newnode;
}

static CreateForeignServerStmt *
_copyCreateForeignServerStmt(const CreateForeignServerStmt *from)
{
	CreateForeignServerStmt *newnode = makeNode(CreateForeignServerStmt);

	COPY_STRING_FIELD(servername);
	COPY_STRING_FIELD(servertype);
	COPY_STRING_FIELD(version);
	COPY_STRING_FIELD(fdwname);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterForeignServerStmt *
_copyAlterForeignServerStmt(const AlterForeignServerStmt *from)
{
	AlterForeignServerStmt *newnode = makeNode(AlterForeignServerStmt);

	COPY_STRING_FIELD(servername);
	COPY_STRING_FIELD(version);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(has_version);

	return newnode;
}

static CreateUserMappingStmt *
_copyCreateUserMappingStmt(const CreateUserMappingStmt *from)
{
	CreateUserMappingStmt *newnode = makeNode(CreateUserMappingStmt);

	COPY_STRING_FIELD(username);
	COPY_STRING_FIELD(servername);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterUserMappingStmt *
_copyAlterUserMappingStmt(const AlterUserMappingStmt *from)
{
	AlterUserMappingStmt *newnode = makeNode(AlterUserMappingStmt);

	COPY_STRING_FIELD(username);
	COPY_STRING_FIELD(servername);
	COPY_NODE_FIELD(options);

	return newnode;
}

static DropUserMappingStmt *
_copyDropUserMappingStmt(const DropUserMappingStmt *from)
{
	DropUserMappingStmt *newnode = makeNode(DropUserMappingStmt);

	COPY_STRING_FIELD(username);
	COPY_STRING_FIELD(servername);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static CreateForeignTableStmt *
_copyCreateForeignTableStmt(const CreateForeignTableStmt *from)
{
	CreateForeignTableStmt *newnode = makeNode(CreateForeignTableStmt);

	CopyCreateStmtFields((const CreateStmt *) from, (CreateStmt *) newnode);

	COPY_STRING_FIELD(servername);
	COPY_NODE_FIELD(options);

	return newnode;
}

static CreateTrigStmt *
_copyCreateTrigStmt(const CreateTrigStmt *from)
{
	CreateTrigStmt *newnode = makeNode(CreateTrigStmt);

	COPY_STRING_FIELD(trigname);
	COPY_NODE_FIELD(relation);
	COPY_NODE_FIELD(funcname);
	COPY_NODE_FIELD(args);
	COPY_SCALAR_FIELD(row);
	COPY_SCALAR_FIELD(timing);
	COPY_SCALAR_FIELD(events);
	COPY_NODE_FIELD(columns);
	COPY_NODE_FIELD(whenClause);
	COPY_SCALAR_FIELD(isconstraint);
	COPY_SCALAR_FIELD(deferrable);
	COPY_SCALAR_FIELD(initdeferred);
	COPY_NODE_FIELD(constrrel);

	return newnode;
}

static CreateEventTrigStmt *
_copyCreateEventTrigStmt(const CreateEventTrigStmt *from)
{
	CreateEventTrigStmt *newnode = makeNode(CreateEventTrigStmt);

	COPY_STRING_FIELD(trigname);
	COPY_STRING_FIELD(eventname);
	COPY_NODE_FIELD(whenclause);
	COPY_NODE_FIELD(funcname);

	return newnode;
}

static AlterEventTrigStmt *
_copyAlterEventTrigStmt(const AlterEventTrigStmt *from)
{
	AlterEventTrigStmt *newnode = makeNode(AlterEventTrigStmt);

	COPY_STRING_FIELD(trigname);
	COPY_SCALAR_FIELD(tgenabled);

	return newnode;
}

static CreatePLangStmt *
_copyCreatePLangStmt(const CreatePLangStmt *from)
{
	CreatePLangStmt *newnode = makeNode(CreatePLangStmt);

	COPY_SCALAR_FIELD(replace);
	COPY_STRING_FIELD(plname);
	COPY_NODE_FIELD(plhandler);
	COPY_NODE_FIELD(plinline);
	COPY_NODE_FIELD(plvalidator);
	COPY_SCALAR_FIELD(pltrusted);

	return newnode;
}

static CreateRoleStmt *
_copyCreateRoleStmt(const CreateRoleStmt *from)
{
	CreateRoleStmt *newnode = makeNode(CreateRoleStmt);

	COPY_SCALAR_FIELD(stmt_type);
	COPY_STRING_FIELD(role);
	COPY_NODE_FIELD(options);

	return newnode;
}

static DenyLoginInterval *
_copyDenyLoginInterval(const DenyLoginInterval *from)
{
	DenyLoginInterval *newnode = makeNode(DenyLoginInterval);

	COPY_NODE_FIELD(start);
	COPY_NODE_FIELD(end);

	return newnode;
}

static DenyLoginPoint *
_copyDenyLoginPoint(const DenyLoginPoint *from)
{
	DenyLoginPoint *newnode = makeNode(DenyLoginPoint);

	COPY_NODE_FIELD(day);
	COPY_NODE_FIELD(time);

	return newnode;
}

static AlterRoleStmt *
_copyAlterRoleStmt(const AlterRoleStmt *from)
{
	AlterRoleStmt *newnode = makeNode(AlterRoleStmt);

	COPY_STRING_FIELD(role);
	COPY_NODE_FIELD(options);
	COPY_SCALAR_FIELD(action);

	return newnode;
}

static AlterRoleSetStmt *
_copyAlterRoleSetStmt(const AlterRoleSetStmt *from)
{
	AlterRoleSetStmt *newnode = makeNode(AlterRoleSetStmt);

	COPY_STRING_FIELD(role);
	COPY_STRING_FIELD(database);
	COPY_NODE_FIELD(setstmt);

	return newnode;
}

static DropRoleStmt *
_copyDropRoleStmt(const DropRoleStmt *from)
{
	DropRoleStmt *newnode = makeNode(DropRoleStmt);

	COPY_NODE_FIELD(roles);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}

static LockStmt *
_copyLockStmt(const LockStmt *from)
{
	LockStmt   *newnode = makeNode(LockStmt);

	COPY_NODE_FIELD(relations);
	COPY_SCALAR_FIELD(mode);
	COPY_SCALAR_FIELD(nowait);

	return newnode;
}

static ConstraintsSetStmt *
_copyConstraintsSetStmt(const ConstraintsSetStmt *from)
{
	ConstraintsSetStmt *newnode = makeNode(ConstraintsSetStmt);

	COPY_NODE_FIELD(constraints);
	COPY_SCALAR_FIELD(deferred);

	return newnode;
}

static ReindexStmt *
_copyReindexStmt(const ReindexStmt *from)
{
	ReindexStmt *newnode = makeNode(ReindexStmt);

	COPY_SCALAR_FIELD(kind);
	COPY_NODE_FIELD(relation);
	COPY_STRING_FIELD(name);
	COPY_SCALAR_FIELD(do_system);
	COPY_SCALAR_FIELD(do_user);
	COPY_SCALAR_FIELD(relid);

	return newnode;
}

static CreateSchemaStmt *
_copyCreateSchemaStmt(const CreateSchemaStmt *from)
{
	CreateSchemaStmt *newnode = makeNode(CreateSchemaStmt);

	COPY_STRING_FIELD(schemaname);
	COPY_STRING_FIELD(authid);
	COPY_NODE_FIELD(schemaElts);
	COPY_SCALAR_FIELD(if_not_exists);
	COPY_SCALAR_FIELD(istemp);

	return newnode;
}

static CreateConversionStmt *
_copyCreateConversionStmt(const CreateConversionStmt *from)
{
	CreateConversionStmt *newnode = makeNode(CreateConversionStmt);

	COPY_NODE_FIELD(conversion_name);
	COPY_STRING_FIELD(for_encoding_name);
	COPY_STRING_FIELD(to_encoding_name);
	COPY_NODE_FIELD(func_name);
	COPY_SCALAR_FIELD(def);

	return newnode;
}

static CreateCastStmt *
_copyCreateCastStmt(const CreateCastStmt *from)
{
	CreateCastStmt *newnode = makeNode(CreateCastStmt);

	COPY_NODE_FIELD(sourcetype);
	COPY_NODE_FIELD(targettype);
	COPY_NODE_FIELD(func);
	COPY_SCALAR_FIELD(context);
	COPY_SCALAR_FIELD(inout);

	return newnode;
}

static PrepareStmt *
_copyPrepareStmt(const PrepareStmt *from)
{
	PrepareStmt *newnode = makeNode(PrepareStmt);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(argtypes);
	COPY_NODE_FIELD(query);

	return newnode;
}

static ExecuteStmt *
_copyExecuteStmt(const ExecuteStmt *from)
{
	ExecuteStmt *newnode = makeNode(ExecuteStmt);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(params);

	return newnode;
}

static DeallocateStmt *
_copyDeallocateStmt(const DeallocateStmt *from)
{
	DeallocateStmt *newnode = makeNode(DeallocateStmt);

	COPY_STRING_FIELD(name);

	return newnode;
}

static DropOwnedStmt *
_copyDropOwnedStmt(const DropOwnedStmt *from)
{
	DropOwnedStmt *newnode = makeNode(DropOwnedStmt);

	COPY_NODE_FIELD(roles);
	COPY_SCALAR_FIELD(behavior);

	return newnode;
}

static ReassignOwnedStmt *
_copyReassignOwnedStmt(const ReassignOwnedStmt *from)
{
	ReassignOwnedStmt *newnode = makeNode(ReassignOwnedStmt);

	COPY_NODE_FIELD(roles);
	COPY_STRING_FIELD(newrole);

	return newnode;
}

static AlterTSDictionaryStmt *
_copyAlterTSDictionaryStmt(const AlterTSDictionaryStmt *from)
{
	AlterTSDictionaryStmt *newnode = makeNode(AlterTSDictionaryStmt);

	COPY_NODE_FIELD(dictname);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterTSConfigurationStmt *
_copyAlterTSConfigurationStmt(const AlterTSConfigurationStmt *from)
{
	AlterTSConfigurationStmt *newnode = makeNode(AlterTSConfigurationStmt);

	COPY_NODE_FIELD(cfgname);
	COPY_NODE_FIELD(tokentype);
	COPY_NODE_FIELD(dicts);
	COPY_SCALAR_FIELD(override);
	COPY_SCALAR_FIELD(replace);
	COPY_SCALAR_FIELD(missing_ok);

	return newnode;
}



static CdbProcess *
_copyCdbProcess(const CdbProcess *from)
{
	CdbProcess *newnode = makeNode(CdbProcess);

	COPY_STRING_FIELD(listenerAddr);
	COPY_SCALAR_FIELD(listenerPort);
	COPY_SCALAR_FIELD(pid);
	COPY_SCALAR_FIELD(contentid);
	COPY_SCALAR_FIELD(dbid);

	return newnode;
}

static Slice *
_copySlice(const Slice *from)
{
	Slice *newnode = makeNode(Slice);

	COPY_SCALAR_FIELD(sliceIndex);
	COPY_SCALAR_FIELD(rootIndex);
	COPY_SCALAR_FIELD(gangType);
	COPY_SCALAR_FIELD(gangSize);
	COPY_SCALAR_FIELD(directDispatch.isDirectDispatch);
	COPY_NODE_FIELD(directDispatch.contentIds);

	newnode->primaryGang = from->primaryGang;
	COPY_SCALAR_FIELD(parentIndex);
	COPY_NODE_FIELD(children);
	COPY_NODE_FIELD(primaryProcesses);
	COPY_BITMAPSET_FIELD(processesMap);

	return newnode;
}

static SliceTable *
_copySliceTable(const SliceTable *from)
{
	SliceTable *newnode = makeNode(SliceTable);

	COPY_SCALAR_FIELD(nMotions);
	COPY_SCALAR_FIELD(nInitPlans);
	COPY_SCALAR_FIELD(localSlice);
	COPY_NODE_FIELD(slices);
	COPY_SCALAR_FIELD(instrument_options);
	COPY_SCALAR_FIELD(ic_instance_id);

	return newnode;
}

static CursorPosInfo *
_copyCursorPosInfo(const CursorPosInfo *from)
{
	CursorPosInfo *newnode = makeNode(CursorPosInfo);

	COPY_STRING_FIELD(cursor_name);
	COPY_SCALAR_FIELD(gp_segment_id);
	COPY_BINARY_FIELD(ctid, sizeof(ItemPointerData));
	COPY_SCALAR_FIELD(table_oid);

	return newnode;
}


static CreateQueueStmt *
_copyCreateQueueStmt(const CreateQueueStmt *from)
{
	CreateQueueStmt *newnode = makeNode(CreateQueueStmt);

	COPY_STRING_FIELD(queue);
	COPY_NODE_FIELD(options);

	return newnode;
}

static AlterQueueStmt *
_copyAlterQueueStmt(const AlterQueueStmt *from)
{
	AlterQueueStmt *newnode = makeNode(AlterQueueStmt);

	COPY_STRING_FIELD(queue);
	COPY_NODE_FIELD(options);

	return newnode;
}

static DropQueueStmt *
_copyDropQueueStmt(const DropQueueStmt *from)
{
	DropQueueStmt *newnode = makeNode(DropQueueStmt);

	COPY_STRING_FIELD(queue);

	return newnode;
}

static CreateResourceGroupStmt *
_copyCreateResourceGroupStmt(const CreateResourceGroupStmt *from)
{
	CreateResourceGroupStmt *newnode = makeNode(CreateResourceGroupStmt);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(options);

	return newnode;
}

static DropResourceGroupStmt *
_copyDropResourceGroupStmt(const DropResourceGroupStmt *from)
{
	DropResourceGroupStmt *newnode = makeNode(DropResourceGroupStmt);

	COPY_STRING_FIELD(name);

	return newnode;
}

static AlterResourceGroupStmt *
_copyAlterResourceGroupStmt(const AlterResourceGroupStmt *from)
{
	AlterResourceGroupStmt *newnode = makeNode(AlterResourceGroupStmt);

	COPY_STRING_FIELD(name);
	COPY_NODE_FIELD(options);

	return newnode;
}

static TableValueExpr *
_copyTableValueExpr(const TableValueExpr *from)
{
	TableValueExpr *newnode = makeNode(TableValueExpr);

	COPY_NODE_FIELD(subquery);
	COPY_LOCATION_FIELD(location);

	return newnode;
}

static AlterTypeStmt *
_copyAlterTypeStmt(const AlterTypeStmt *from)
{
	AlterTypeStmt *newnode = makeNode(AlterTypeStmt);

	COPY_NODE_FIELD(typeName);
	COPY_NODE_FIELD(encoding);

	return newnode;
}

static CookedConstraint *
_copyCookedConstraint(const CookedConstraint *from)
{
	CookedConstraint *newnode = makeNode(CookedConstraint);

	COPY_SCALAR_FIELD(contype);
	COPY_STRING_FIELD(name);
	COPY_SCALAR_FIELD(attnum);
	COPY_NODE_FIELD(expr);
	COPY_SCALAR_FIELD(is_local);
	COPY_SCALAR_FIELD(inhcount);

	return newnode;
}

static GpPolicy *
_copyGpPolicy(const GpPolicy *from)
{
	GpPolicy *newnode = makeNode(GpPolicy);

	COPY_SCALAR_FIELD(ptype);
	COPY_SCALAR_FIELD(numsegments);
	COPY_SCALAR_FIELD(nattrs);
	COPY_POINTER_FIELD(attrs, from->nattrs * sizeof(AttrNumber));
	COPY_POINTER_FIELD(opclasses, from->nattrs * sizeof(Oid));

	return newnode;
}

static DistributedBy *
_copyDistributedBy(const DistributedBy *from)
{
	DistributedBy *newnode = makeNode(DistributedBy);

	COPY_SCALAR_FIELD(ptype);
	COPY_SCALAR_FIELD(numsegments);
	COPY_NODE_FIELD(keyCols);

	return newnode;
}

/* ****************************************************************
 *					pg_list.h copy functions
 * ****************************************************************
 */

/*
 * Perform a deep copy of the specified list, using copyObject(). The
 * list MUST be of type T_List; T_IntList and T_OidList nodes don't
 * need deep copies, so they should be copied via list_copy()
 */
#define COPY_NODE_CELL(new, old)					\
	(new) = (ListCell *) palloc(sizeof(ListCell));	\
	lfirst(new) = copyObject(lfirst(old));

static List *
_copyList(const List *from)
{
	List	   *new;
	ListCell   *curr_old;
	ListCell   *prev_new;

	Assert(list_length(from) >= 1);

	new = makeNode(List);
	new->length = from->length;

	COPY_NODE_CELL(new->head, from->head);
	prev_new = new->head;
	curr_old = lnext(from->head);

	while (curr_old)
	{
		COPY_NODE_CELL(prev_new->next, curr_old);
		prev_new = prev_new->next;
		curr_old = curr_old->next;
	}
	prev_new->next = NULL;
	new->tail = prev_new;

	return new;
}

/* ****************************************************************
 *					value.h copy functions
 * ****************************************************************
 */
static Value *
_copyValue(const Value *from)
{
	Value	   *newnode = makeNode(Value);

	/* See also _copyAConst when changing this code! */

	COPY_SCALAR_FIELD(type);
	switch (from->type)
	{
		case T_Integer:
			COPY_SCALAR_FIELD(val.ival);
			break;
		case T_Float:
		case T_String:
		case T_BitString:
			COPY_STRING_FIELD(val.str);
			break;
		case T_Null:
			/* nothing to do */
			break;
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) from->type);
			break;
	}
	return newnode;
}

/*
 * copyObject
 *
 * Create a copy of a Node tree or list.  This is a "deep" copy: all
 * substructure is copied too, recursively.
 */
void *
copyObject(const void *from)
{
	void	   *retval;

	if (from == NULL)
		return NULL;

	/* Guard against stack overflow due to overly complex expressions */
	check_stack_depth();

	switch (nodeTag(from))
	{
			/*
			 * PLAN NODES
			 */
		case T_PlannedStmt:
			retval = _copyPlannedStmt(from);
			break;
		case T_QueryDispatchDesc:
			retval = _copyQueryDispatchDesc(from);
			break;
		case T_OidAssignment:
			retval = _copyOidAssignment(from);
			break;
		case T_Plan:
			retval = _copyPlan(from);
			break;
		case T_Result:
			retval = _copyResult(from);
			break;
		case T_Repeat:
			retval = _copyRepeat(from);
			break;
		case T_ModifyTable:
			retval = _copyModifyTable(from);
			break;
		case T_Append:
			retval = _copyAppend(from);
			break;
		case T_MergeAppend:
			retval = _copyMergeAppend(from);
			break;
		case T_RecursiveUnion:
			retval = _copyRecursiveUnion(from);
			break;
		case T_Sequence:
			retval = _copySequence(from);
			break;
		case T_BitmapAnd:
			retval = _copyBitmapAnd(from);
			break;
		case T_BitmapOr:
			retval = _copyBitmapOr(from);
			break;
		case T_Scan:
			retval = _copyScan(from);
			break;
		case T_SeqScan:
			retval = _copySeqScan(from);
			break;
		case T_DynamicSeqScan:
			retval = _copyDynamicSeqScan(from);
			break;
		case T_ExternalScan:
			retval = _copyExternalScan(from);
			break;
		case T_IndexScan:
			retval = _copyIndexScan(from);
			break;
		case T_DynamicIndexScan:
			retval = _copyDynamicIndexScan(from);
			break;
		case T_IndexOnlyScan:
			retval = _copyIndexOnlyScan(from);
			break;
		case T_BitmapIndexScan:
			retval = _copyBitmapIndexScan(from);
			break;
		case T_DynamicBitmapIndexScan:
			retval = _copyDynamicBitmapIndexScan(from);
			break;
		case T_BitmapHeapScan:
			retval = _copyBitmapHeapScan(from);
			break;
		case T_DynamicBitmapHeapScan:
			retval = _copyDynamicBitmapHeapScan(from);
			break;
		case T_TidScan:
			retval = _copyTidScan(from);
			break;
		case T_SubqueryScan:
			retval = _copySubqueryScan(from);
			break;
		case T_FunctionScan:
			retval = _copyFunctionScan(from);
			break;
		case T_ValuesScan:
			retval = _copyValuesScan(from);
			break;
		case T_CteScan:
			retval = _copyCteScan(from);
			break;
		case T_WorkTableScan:
			retval = _copyWorkTableScan(from);
			break;
		case T_ForeignScan:
			retval = _copyForeignScan(from);
			break;
		case T_Join:
			retval = _copyJoin(from);
			break;
		case T_NestLoop:
			retval = _copyNestLoop(from);
			break;
		case T_MergeJoin:
			retval = _copyMergeJoin(from);
			break;
		case T_HashJoin:
			retval = _copyHashJoin(from);
			break;
		case T_ShareInputScan:
			retval = _copyShareInputScan(from);
			break;
		case T_Material:
			retval = _copyMaterial(from);
			break;
		case T_Sort:
			retval = _copySort(from);
			break;
		case T_Agg:
			retval = _copyAgg(from);
			break;
		case T_WindowAgg:
			retval = _copyWindowAgg(from);
			break;
		case T_TableFunctionScan:
			retval = _copyTableFunctionScan(from);
			break;
		case T_Unique:
			retval = _copyUnique(from);
			break;
		case T_Hash:
			retval = _copyHash(from);
			break;
		case T_SetOp:
			retval = _copySetOp(from);
			break;
		case T_LockRows:
			retval = _copyLockRows(from);
			break;
		case T_Limit:
			retval = _copyLimit(from);
			break;
		case T_NestLoopParam:
			retval = _copyNestLoopParam(from);
			break;
		case T_PlanRowMark:
			retval = _copyPlanRowMark(from);
			break;
		case T_PlanInvalItem:
			retval = _copyPlanInvalItem(from);
			break;
		case T_Motion:
			retval = _copyMotion(from);
			break;
		case T_DML:
			retval = _copyDML(from);
			break;
		case T_SplitUpdate:
			retval = _copySplitUpdate(from);
			break;
		case T_RowTrigger:
			retval = _copyRowTrigger(from);
			break;
		case T_AssertOp:
			retval = _copyAssertOp(from);
			break;
		case T_PartitionSelector:
			retval = _copyPartitionSelector(from);
			break;

			/*
			 * PRIMITIVE NODES
			 */
		case T_Alias:
			retval = _copyAlias(from);
			break;
		case T_RangeVar:
			retval = _copyRangeVar(from);
			break;
		case T_IntoClause:
			retval = _copyIntoClause(from);
			break;
		case T_CopyIntoClause:
			retval = _copyCopyIntoClause(from);
			break;
		case T_RefreshClause:
			retval = _copyRefreshClause(from);
			break;
		case T_Var:
			retval = _copyVar(from);
			break;
		case T_Const:
			retval = _copyConst(from);
			break;
		case T_Param:
			retval = _copyParam(from);
			break;
		case T_Aggref:
			retval = _copyAggref(from);
			break;
		case T_WindowFunc:
			retval = _copyWindowFunc(from);
			break;
		case T_ArrayRef:
			retval = _copyArrayRef(from);
			break;
		case T_FuncExpr:
			retval = _copyFuncExpr(from);
			break;
		case T_NamedArgExpr:
			retval = _copyNamedArgExpr(from);
			break;
		case T_OpExpr:
			retval = _copyOpExpr(from);
			break;
		case T_DistinctExpr:
			retval = _copyDistinctExpr(from);
			break;
		case T_NullIfExpr:
			retval = _copyNullIfExpr(from);
			break;
		case T_ScalarArrayOpExpr:
			retval = _copyScalarArrayOpExpr(from);
			break;
		case T_BoolExpr:
			retval = _copyBoolExpr(from);
			break;
		case T_SubLink:
			retval = _copySubLink(from);
			break;
		case T_SubPlan:
			retval = _copySubPlan(from);
			break;
		case T_AlternativeSubPlan:
			retval = _copyAlternativeSubPlan(from);
			break;
		case T_FieldSelect:
			retval = _copyFieldSelect(from);
			break;
		case T_FieldStore:
			retval = _copyFieldStore(from);
			break;
		case T_RelabelType:
			retval = _copyRelabelType(from);
			break;
		case T_CoerceViaIO:
			retval = _copyCoerceViaIO(from);
			break;
		case T_ArrayCoerceExpr:
			retval = _copyArrayCoerceExpr(from);
			break;
		case T_ConvertRowtypeExpr:
			retval = _copyConvertRowtypeExpr(from);
			break;
		case T_CollateExpr:
			retval = _copyCollateExpr(from);
			break;
		case T_CaseExpr:
			retval = _copyCaseExpr(from);
			break;
		case T_CaseWhen:
			retval = _copyCaseWhen(from);
			break;
		case T_CaseTestExpr:
			retval = _copyCaseTestExpr(from);
			break;
		case T_ArrayExpr:
			retval = _copyArrayExpr(from);
			break;
		case T_RowExpr:
			retval = _copyRowExpr(from);
			break;
		case T_RowCompareExpr:
			retval = _copyRowCompareExpr(from);
			break;
		case T_CoalesceExpr:
			retval = _copyCoalesceExpr(from);
			break;
		case T_MinMaxExpr:
			retval = _copyMinMaxExpr(from);
			break;
		case T_XmlExpr:
			retval = _copyXmlExpr(from);
			break;
		case T_NullTest:
			retval = _copyNullTest(from);
			break;
		case T_BooleanTest:
			retval = _copyBooleanTest(from);
			break;
		case T_CoerceToDomain:
			retval = _copyCoerceToDomain(from);
			break;
		case T_CoerceToDomainValue:
			retval = _copyCoerceToDomainValue(from);
			break;
		case T_SetToDefault:
			retval = _copySetToDefault(from);
			break;
		case T_CurrentOfExpr:
			retval = _copyCurrentOfExpr(from);
			break;
		case T_TargetEntry:
			retval = _copyTargetEntry(from);
			break;
		case T_RangeTblRef:
			retval = _copyRangeTblRef(from);
			break;
		case T_JoinExpr:
			retval = _copyJoinExpr(from);
			break;
		case T_FromExpr:
			retval = _copyFromExpr(from);
			break;
		case T_Flow:
			retval = _copyFlow(from);
			break;

			/*
			 * RELATION NODES
			 */
		case T_CdbRelColumnInfo:
			retval = _copyCdbRelColumnInfo(from);
			break;
		case T_PathKey:
			retval = _copyPathKey(from);
			break;
		case T_DistributionKey:
			retval = _copyDistributionKey(from);
			break;
		case T_RestrictInfo:
			retval = _copyRestrictInfo(from);
			break;
		case T_PlaceHolderVar:
			retval = _copyPlaceHolderVar(from);
			break;
		case T_SpecialJoinInfo:
			retval = _copySpecialJoinInfo(from);
			break;
		case T_LateralJoinInfo:
			retval = _copyLateralJoinInfo(from);
			break;
		case T_AppendRelInfo:
			retval = _copyAppendRelInfo(from);
			break;
		case T_PlaceHolderInfo:
			retval = _copyPlaceHolderInfo(from);
			break;

			/*
			 * VALUE NODES
			 */
		case T_Integer:
		case T_Float:
		case T_String:
		case T_BitString:
		case T_Null:
			retval = _copyValue(from);
			break;

			/*
			 * LIST NODES
			 */
		case T_List:
			retval = _copyList(from);
			break;

			/*
			 * Lists of integers and OIDs don't need to be deep-copied, so we
			 * perform a shallow copy via list_copy()
			 */
		case T_IntList:
		case T_OidList:
			retval = list_copy(from);
			break;

			/*
			 * PARSE NODES
			 */
		case T_Query:
			retval = _copyQuery(from);
			break;
		case T_InsertStmt:
			retval = _copyInsertStmt(from);
			break;
		case T_DeleteStmt:
			retval = _copyDeleteStmt(from);
			break;
		case T_UpdateStmt:
			retval = _copyUpdateStmt(from);
			break;
		case T_SelectStmt:
			retval = _copySelectStmt(from);
			break;
		case T_SetOperationStmt:
			retval = _copySetOperationStmt(from);
			break;
		case T_AlterTableStmt:
			retval = _copyAlterTableStmt(from);
			break;
		case T_AlterTableCmd:
			retval = _copyAlterTableCmd(from);
			break;
		case T_SetDistributionCmd:
			retval = _copySetDistributionCmd(from);
			break;
		case T_InheritPartitionCmd:
			retval = _copyInheritPartitionCmd(from);
			break;
		case T_AlterPartitionCmd:
			retval = _copyAlterPartitionCmd(from);
			break;
		case T_AlterPartitionId:
			retval = _copyAlterPartitionId(from);
			break;
		case T_AlterDomainStmt:
			retval = _copyAlterDomainStmt(from);
			break;
		case T_GrantStmt:
			retval = _copyGrantStmt(from);
			break;
		case T_GrantRoleStmt:
			retval = _copyGrantRoleStmt(from);
			break;
		case T_AlterDefaultPrivilegesStmt:
			retval = _copyAlterDefaultPrivilegesStmt(from);
			break;
		case T_DeclareCursorStmt:
			retval = _copyDeclareCursorStmt(from);
			break;
		case T_ClosePortalStmt:
			retval = _copyClosePortalStmt(from);
			break;
		case T_ClusterStmt:
			retval = _copyClusterStmt(from);
			break;
		case T_SingleRowErrorDesc:
			retval = _copySingleRowErrorDesc(from);
			break;
		case T_CopyStmt:
			retval = _copyCopyStmt(from);
			break;
		case T_CreateStmt:
			retval = _copyCreateStmt(from);
			break;
		case T_PartitionBy:
			retval = _copyPartitionBy(from);
			break;
		case T_PartitionSpec:
			retval = _copyPartitionSpec(from);
			break;
		case T_PartitionValuesSpec:
			retval = _copyPartitionValuesSpec(from);
			break;
		case T_ExpandStmtSpec:
			retval = _copyExpandStmtSpec(from);
			break;
		case T_PartitionElem:
			retval = _copyPartitionElem(from);
			break;
		case T_PartitionRangeItem:
			retval = _copyPartitionRangeItem(from);
			break;
		case T_PartitionBoundSpec:
			retval = _copyPartitionBoundSpec(from);
			break;
		case T_PgPartRule:
			retval = _copyPgPartRule(from);
			break;
		case T_PartitionNode:
			retval = _copyPartitionNode(from);
			break;
		case T_Partition:
			retval = _copyPartition(from);
			break;
		case T_PartitionRule:
			retval = _copyPartitionRule(from);
			break;
		case T_ExtTableTypeDesc:
			retval = _copyExtTableTypeDesc(from);
			break;
		case T_CreateExternalStmt:
			retval = _copyCreateExternalStmt(from);
			break;
		case T_TableLikeClause:
			retval = _copyTableLikeClause(from);
			break;
		case T_DefineStmt:
			retval = _copyDefineStmt(from);
			break;
		case T_DropStmt:
			retval = _copyDropStmt(from);
			break;
		case T_TruncateStmt:
			retval = _copyTruncateStmt(from);
			break;
		case T_CommentStmt:
			retval = _copyCommentStmt(from);
			break;
		case T_SecLabelStmt:
			retval = _copySecLabelStmt(from);
			break;
		case T_FetchStmt:
			retval = _copyFetchStmt(from);
			break;
		case T_IndexStmt:
			retval = _copyIndexStmt(from);
			break;
		case T_CreateFunctionStmt:
			retval = _copyCreateFunctionStmt(from);
			break;
		case T_FunctionParameter:
			retval = _copyFunctionParameter(from);
			break;
		case T_AlterFunctionStmt:
			retval = _copyAlterFunctionStmt(from);
			break;
		case T_DoStmt:
			retval = _copyDoStmt(from);
			break;
		case T_RenameStmt:
			retval = _copyRenameStmt(from);
			break;
		case T_AlterObjectSchemaStmt:
			retval = _copyAlterObjectSchemaStmt(from);
			break;
		case T_AlterOwnerStmt:
			retval = _copyAlterOwnerStmt(from);
			break;
		case T_RuleStmt:
			retval = _copyRuleStmt(from);
			break;
		case T_NotifyStmt:
			retval = _copyNotifyStmt(from);
			break;
		case T_ListenStmt:
			retval = _copyListenStmt(from);
			break;
		case T_UnlistenStmt:
			retval = _copyUnlistenStmt(from);
			break;
		case T_TransactionStmt:
			retval = _copyTransactionStmt(from);
			break;
		case T_CompositeTypeStmt:
			retval = _copyCompositeTypeStmt(from);
			break;
		case T_CreateEnumStmt:
			retval = _copyCreateEnumStmt(from);
			break;
		case T_CreateRangeStmt:
			retval = _copyCreateRangeStmt(from);
			break;
		case T_AlterEnumStmt:
			retval = _copyAlterEnumStmt(from);
			break;
		case T_ViewStmt:
			retval = _copyViewStmt(from);
			break;
		case T_LoadStmt:
			retval = _copyLoadStmt(from);
			break;
		case T_CreateDomainStmt:
			retval = _copyCreateDomainStmt(from);
			break;
		case T_CreateOpClassStmt:
			retval = _copyCreateOpClassStmt(from);
			break;
		case T_CreateOpClassItem:
			retval = _copyCreateOpClassItem(from);
			break;
		case T_CreateOpFamilyStmt:
			retval = _copyCreateOpFamilyStmt(from);
			break;
		case T_AlterOpFamilyStmt:
			retval = _copyAlterOpFamilyStmt(from);
			break;
		case T_CreatedbStmt:
			retval = _copyCreatedbStmt(from);
			break;
		case T_AlterDatabaseStmt:
			retval = _copyAlterDatabaseStmt(from);
			break;
		case T_AlterDatabaseSetStmt:
			retval = _copyAlterDatabaseSetStmt(from);
			break;
		case T_DropdbStmt:
			retval = _copyDropdbStmt(from);
			break;
		case T_VacuumStmt:
			retval = _copyVacuumStmt(from);
			break;
		case T_ExplainStmt:
			retval = _copyExplainStmt(from);
			break;
		case T_CreateTableAsStmt:
			retval = _copyCreateTableAsStmt(from);
			break;
		case T_RefreshMatViewStmt:
			retval = _copyRefreshMatViewStmt(from);
			break;
		case T_ReplicaIdentityStmt:
			retval = _copyReplicaIdentityStmt(from);
			break;
		case T_AlterSystemStmt:
			retval = _copyAlterSystemStmt(from);
			break;
		case T_CreateSeqStmt:
			retval = _copyCreateSeqStmt(from);
			break;
		case T_AlterSeqStmt:
			retval = _copyAlterSeqStmt(from);
			break;
		case T_VariableSetStmt:
			retval = _copyVariableSetStmt(from);
			break;
		case T_VariableShowStmt:
			retval = _copyVariableShowStmt(from);
			break;
		case T_DiscardStmt:
			retval = _copyDiscardStmt(from);
			break;
		case T_CreateTableSpaceStmt:
			retval = _copyCreateTableSpaceStmt(from);
			break;
		case T_DropTableSpaceStmt:
			retval = _copyDropTableSpaceStmt(from);
			break;
		case T_AlterTableSpaceOptionsStmt:
			retval = _copyAlterTableSpaceOptionsStmt(from);
			break;
		case T_AlterTableMoveAllStmt:
			retval = _copyAlterTableMoveAllStmt(from);
			break;
		case T_CreateExtensionStmt:
			retval = _copyCreateExtensionStmt(from);
			break;
		case T_AlterExtensionStmt:
			retval = _copyAlterExtensionStmt(from);
			break;
		case T_AlterExtensionContentsStmt:
			retval = _copyAlterExtensionContentsStmt(from);
			break;
		case T_CreateFdwStmt:
			retval = _copyCreateFdwStmt(from);
			break;
		case T_AlterFdwStmt:
			retval = _copyAlterFdwStmt(from);
			break;
		case T_CreateForeignServerStmt:
			retval = _copyCreateForeignServerStmt(from);
			break;
		case T_AlterForeignServerStmt:
			retval = _copyAlterForeignServerStmt(from);
			break;
		case T_CreateUserMappingStmt:
			retval = _copyCreateUserMappingStmt(from);
			break;
		case T_AlterUserMappingStmt:
			retval = _copyAlterUserMappingStmt(from);
			break;
		case T_DropUserMappingStmt:
			retval = _copyDropUserMappingStmt(from);
			break;
		case T_CreateForeignTableStmt:
			retval = _copyCreateForeignTableStmt(from);
			break;
		case T_CreateTrigStmt:
			retval = _copyCreateTrigStmt(from);
			break;
		case T_CreateEventTrigStmt:
			retval = _copyCreateEventTrigStmt(from);
			break;
		case T_AlterEventTrigStmt:
			retval = _copyAlterEventTrigStmt(from);
			break;
		case T_CreatePLangStmt:
			retval = _copyCreatePLangStmt(from);
			break;
		case T_CreateRoleStmt:
			retval = _copyCreateRoleStmt(from);
			break;
		case T_AlterRoleStmt:
			retval = _copyAlterRoleStmt(from);
			break;
		case T_AlterRoleSetStmt:
			retval = _copyAlterRoleSetStmt(from);
			break;
		case T_DropRoleStmt:
			retval = _copyDropRoleStmt(from);
			break;
		case T_LockStmt:
			retval = _copyLockStmt(from);
			break;
		case T_ConstraintsSetStmt:
			retval = _copyConstraintsSetStmt(from);
			break;
		case T_ReindexStmt:
			retval = _copyReindexStmt(from);
			break;
		case T_CheckPointStmt:
			retval = (void *) makeNode(CheckPointStmt);
			break;
		case T_CreateSchemaStmt:
			retval = _copyCreateSchemaStmt(from);
			break;
		case T_CreateConversionStmt:
			retval = _copyCreateConversionStmt(from);
			break;
		case T_CreateCastStmt:
			retval = _copyCreateCastStmt(from);
			break;
		case T_PrepareStmt:
			retval = _copyPrepareStmt(from);
			break;
		case T_ExecuteStmt:
			retval = _copyExecuteStmt(from);
			break;
		case T_DeallocateStmt:
			retval = _copyDeallocateStmt(from);
			break;
		case T_DropOwnedStmt:
			retval = _copyDropOwnedStmt(from);
			break;
		case T_ReassignOwnedStmt:
			retval = _copyReassignOwnedStmt(from);
			break;
		case T_AlterTSDictionaryStmt:
			retval = _copyAlterTSDictionaryStmt(from);
			break;
		case T_AlterTSConfigurationStmt:
			retval = _copyAlterTSConfigurationStmt(from);
			break;

		case T_CreateQueueStmt:
			retval = _copyCreateQueueStmt(from);
			break;
		case T_AlterQueueStmt:
			retval = _copyAlterQueueStmt(from);
			break;
		case T_DropQueueStmt:
			retval = _copyDropQueueStmt(from);
			break;

		case T_CreateResourceGroupStmt:
			retval = _copyCreateResourceGroupStmt(from);
			break;
		case T_DropResourceGroupStmt:
			retval = _copyDropResourceGroupStmt(from);
			break;
		case T_AlterResourceGroupStmt:
			retval = _copyAlterResourceGroupStmt(from);
			break;

		case T_A_Expr:
			retval = _copyAExpr(from);
			break;
		case T_ColumnRef:
			retval = _copyColumnRef(from);
			break;
		case T_ParamRef:
			retval = _copyParamRef(from);
			break;
		case T_A_Const:
			retval = _copyAConst(from);
			break;
		case T_FuncCall:
			retval = _copyFuncCall(from);
			break;
		case T_A_Star:
			retval = _copyAStar(from);
			break;
		case T_A_Indices:
			retval = _copyAIndices(from);
			break;
		case T_A_Indirection:
			retval = _copyA_Indirection(from);
			break;
		case T_A_ArrayExpr:
			retval = _copyA_ArrayExpr(from);
			break;
		case T_ResTarget:
			retval = _copyResTarget(from);
			break;
		case T_TypeCast:
			retval = _copyTypeCast(from);
			break;
		case T_CollateClause:
			retval = _copyCollateClause(from);
			break;
		case T_SortBy:
			retval = _copySortBy(from);
			break;
		case T_WindowDef:
			retval = _copyWindowDef(from);
			break;
		case T_RangeSubselect:
			retval = _copyRangeSubselect(from);
			break;
		case T_RangeFunction:
			retval = _copyRangeFunction(from);
			break;
		case T_TypeName:
			retval = _copyTypeName(from);
			break;
		case T_IndexElem:
			retval = _copyIndexElem(from);
			break;
		case T_ColumnDef:
			retval = _copyColumnDef(from);
			break;
		case T_ColumnReferenceStorageDirective:
			retval = _copyColumnReferenceStorageDirective(from);
			break;
		case T_Constraint:
			retval = _copyConstraint(from);
			break;
		case T_DefElem:
			retval = _copyDefElem(from);
			break;
		case T_LockingClause:
			retval = _copyLockingClause(from);
			break;
		case T_DMLActionExpr:
			retval = _copyDMLActionExpr(from);
			break;
		case T_PartSelectedExpr:
			retval = _copyPartSelectedExpr(from);
			break;
		case T_PartDefaultExpr:
			retval = _copyPartDefaultExpr(from);
			break;
		case T_PartBoundExpr:
			retval = _copyPartBoundExpr(from);
			break;
		case T_PartBoundInclusionExpr:
			retval = _copyPartBoundInclusionExpr(from);
			break;
		case T_PartBoundOpenExpr:
			retval = _copyPartBoundOpenExpr(from);
			break;
		case T_PartListRuleExpr:
			retval = _copyPartListRuleExpr(from);
			break;
		case T_PartListNullTestExpr:
			retval = _copyPartListNullTestExpr(from);
			break;
		case T_RangeTblEntry:
			retval = _copyRangeTblEntry(from);
			break;
		case T_RangeTblFunction:
			retval = _copyRangeTblFunction(from);
			break;
		case T_WithCheckOption:
			retval = _copyWithCheckOption(from);
			break;
		case T_SortGroupClause:
			retval = _copySortGroupClause(from);
			break;
		case T_GroupingClause:
			retval = _copyGroupingClause(from);
			break;
		case T_GroupingFunc:
			retval = _copyGroupingFunc(from);
			break;
		case T_Grouping:
			retval = _copyGrouping(from);
			break;
		case T_GroupId:
			retval = _copyGroupId(from);
			break;
		case T_WindowClause:
			retval = _copyWindowClause(from);
			break;
		case T_RowMarkClause:
			retval = _copyRowMarkClause(from);
			break;
		case T_WithClause:
			retval = _copyWithClause(from);
			break;
		case T_CommonTableExpr:
			retval = _copyCommonTableExpr(from);
			break;
		case T_PrivGrantee:
			retval = _copyPrivGrantee(from);
			break;
		case T_FuncWithArgs:
			retval = _copyFuncWithArgs(from);
			break;
		case T_AccessPriv:
			retval = _copyAccessPriv(from);
			break;
		case T_XmlSerialize:
			retval = _copyXmlSerialize(from);
			break;

		case T_CdbProcess:
			retval = _copyCdbProcess(from);
			break;
		case T_Slice:
			retval = _copySlice(from);
			break;
		case T_SliceTable:
			retval = _copySliceTable(from);
			break;
		case T_CursorPosInfo:
			retval = _copyCursorPosInfo(from);
			break;
		case T_TableValueExpr:
			retval = _copyTableValueExpr(from);
			break;
		case T_AlterTypeStmt:
			retval = _copyAlterTypeStmt(from);
			break;

		case T_DenyLoginInterval:
			retval = _copyDenyLoginInterval(from);
			break;
		case T_DenyLoginPoint:
			retval = _copyDenyLoginPoint(from);
			break;

		case T_CookedConstraint:
			retval = _copyCookedConstraint(from);
			break;
		case T_GpPolicy:
			retval = _copyGpPolicy(from);
			break;

		case T_DistributedBy:
			retval = _copyDistributedBy(from);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(from));
			retval = 0;			/* keep compiler quiet */
			break;
	}

	return retval;
}
