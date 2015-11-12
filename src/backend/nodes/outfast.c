/*-------------------------------------------------------------------------
 *
 * outfast.c
 *	  Fast serialization functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * NOTES
 *	  Every node type that can appear in an Greenplum Database serialized query or plan
 *    tree must have an output function defined here.
 *
 * 	  There *MUST* be a one-to-one correspondence between this routine
 *    and readfast.c.  If not, you will likely crash the system.
 *
 *     By design, the only user of these routines is the function
 *     serializeNode in cdbsrlz.c.  Other callers beware.
 *
 *    This file is based on outfuncs.c, however, it is optimized for speed
 *    at the expense legibility.
 *
 * 	  Rather than serialize to a (somewhat human-readable) string, these
 *    routines create a binary serialization via a simple depth-first walk
 *    of the tree.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "utils/datum.h"
#include "cdb/cdbgang.h"
#include "utils/workfile_mgr.h"
#include "parser/parsetree.h"


/*
 * Macros to simplify output of different kinds of fields.	Use these
 * wherever possible to reduce the chance for silly typos.	Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/*
 * Write the label for the node type.  nodelabel is accepted for
 * compatibility with outfuncs.c, but is ignored
 */
#define WRITE_NODE_TYPE(nodelabel) \
	{ int16 nt =nodeTag(node); appendBinaryStringInfo(str, (const char *)&nt, sizeof(int16)); }

/* Write an integer field  */
#define WRITE_INT_FIELD(fldname) \
	{ appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int)); }

/* Write an integer field  */
#define WRITE_INT16_FIELD(fldname) \
	{ appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int16)); }

/* Write an unsigned integer field */
#define WRITE_UINT_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(int))

/* Write an uint64 field */
#define WRITE_UINT64_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(uint64))

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(Oid))

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(long))

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) \
	appendBinaryStringInfo(str, &node->fldname, 1)

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) \
	{ int16 en=node->fldname; appendBinaryStringInfo(str, (const char *)&en, sizeof(int16)); }

/* Write a float field --- the format is accepted but ignored (for compat with outfuncs.c)  */
#define WRITE_FLOAT_FIELD(fldname,format) \
	appendBinaryStringInfo(str, (const char *)&node->fldname, sizeof(double))

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) \
	{ \
		char b = node->fldname ? 1 : 0; \
		appendBinaryStringInfo(str, (const char *)&b, 1); }

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname) \
	{ int slen = node->fldname != NULL ? strlen(node->fldname) : 0; \
		appendBinaryStringInfo(str, (const char *)&slen, sizeof(int)); \
		if (slen>0) appendBinaryStringInfo(str, node->fldname, strlen(node->fldname));}

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(_outNode(str, node->fldname))

/* Write a List field: this is a shortcut (you can call
 * WRITE_NODE_FIELD for list fields) to avoid an extra call to
 * _outNode() */
#define WRITE_LIST_FIELD(fldname) \
	(_outList(str, node->fldname))

/* Write a bitmapset field */
#define WRITE_BITMAPSET_FIELD(fldname) \
	 _outBitmapset(str, node->fldname)

/* Write a binary field */
#define WRITE_BINARY_FIELD(fldname, sz) \
{ appendBinaryStringInfo(str, (const char *) &node->fldname, (sz)); }

/* Write a bytea field */
#define WRITE_BYTEA_FIELD(fldname) \
	(_outDatum(str, PointerGetDatum(node->fldname), -1, false))

/* Write a dummy field -- value not displayable or copyable */
#define WRITE_DUMMY_FIELD(fldname) \
	{ /*int * dummy = 0; appendBinaryStringInfo(str,(const char *)&dummy, sizeof(int *)) ;*/ }

	/* Read an integer array */
#define WRITE_INT_ARRAY(fldname, count, Type) \
	if ( node->count > 0 ) \
	{ \
		int i; \
		for(i=0; i<node->count; i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(Type)); \
		} \
	}

/* Write an Trasnaction ID array  */
#define WRITE_XID_ARRAY(fldname, count) \
	if ( node->count > 0 ) \
	{ \
		int i; \
		for(i=0; i<node->count; i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(TransactionId)); \
		} \
	}



/* Write an Oid array  */
#define WRITE_OID_ARRAY(fldname, count) \
	if ( node->count > 0 ) \
	{ \
		int i; \
		for(i=0; i<node->count; i++) \
		{ \
			appendBinaryStringInfo(str, (const char *)&node->fldname[i], sizeof(Oid)); \
		} \
	}

static void _outNode(StringInfo str, void *obj);

/* When serializing a plan for workfile caching, we want to leave out
 * all variable fields by setting this to false */
static bool print_variable_fields = true;
/* rtable needed when serializing for workfile caching */
static List *range_table = NULL;

static void
_outList(StringInfo str, List *node)
{
	ListCell   *lc;

	if (node == NULL)
	{
		int16 tg = 0;
		appendBinaryStringInfo(str, (const char *)&tg, sizeof(int16));
		return;
	}

	WRITE_NODE_TYPE("");
    WRITE_INT_FIELD(length);

	foreach(lc, node)
	{

		if (IsA(node, List))
		{
			_outNode(str, lfirst(lc));
		}
		else if (IsA(node, IntList))
		{
			int n = lfirst_int(lc);
			appendBinaryStringInfo(str, (const char *)&n, sizeof(int));
		}
		else if (IsA(node, OidList))
		{
			Oid n = lfirst_oid(lc);
			appendBinaryStringInfo(str, (const char *)&n, sizeof(Oid));
		}
	}
}

/*
 * _outBitmapset -
 *	   converts a bitmap set of integers
 *
 * Currently bitmapsets do not appear in any node type that is stored in
 * rules, so there is no support in readfast.c for reading this format.
 */
static void
_outBitmapset(StringInfo str, Bitmapset *bms)
{
	int i;
	int nwords = 0;
	if (bms) nwords = bms->nwords;
	appendBinaryStringInfo(str, (char *)&nwords, sizeof(int));
	for (i = 0; i < nwords; i++)
	{
		appendBinaryStringInfo(str, (char *)&bms->words[i], sizeof(bitmapword));
	}

}

/*
 * Print the value of a Datum given its type.
 */
static void
_outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
	Size		length;
	char	   *s;

	if (typbyval)
	{
		s = (char *) (&value);
		appendBinaryStringInfo(str, s, sizeof(Datum));
	}
	else
	{
		s = (char *) DatumGetPointer(value);
		if (!PointerIsValid(s))
		{
			length = 0;
			appendBinaryStringInfo(str, (char *)&length, sizeof(Size));
		}
		else
		{
			length = datumGetSize(value, typbyval, typlen);
			appendBinaryStringInfo(str, (char *)&length, sizeof(Size));
			appendBinaryStringInfo(str, s, length);
		}
	}
}


/*
 *	Stuff from plannodes.h
 */

/*
 * print the basic stuff of all nodes that inherit from Plan
 */
static void
_outPlanInfo(StringInfo str, Plan *node)
{

	if (print_variable_fields)
	{
		WRITE_INT_FIELD(plan_node_id);
		WRITE_INT_FIELD(plan_parent_node_id);

		WRITE_FLOAT_FIELD(startup_cost, "%.2f");
		WRITE_FLOAT_FIELD(total_cost, "%.2f");
		WRITE_FLOAT_FIELD(plan_rows, "%.0f");
		WRITE_INT_FIELD(plan_width);
	}

	WRITE_NODE_FIELD(targetlist);
	WRITE_NODE_FIELD(qual);

	WRITE_BITMAPSET_FIELD(extParam);
	WRITE_BITMAPSET_FIELD(allParam);

	WRITE_INT_FIELD(nParamExec);
	
	if (print_variable_fields)
	{
		WRITE_NODE_FIELD(flow);
		WRITE_INT_FIELD(dispatch);
		WRITE_BOOL_FIELD(directDispatch.isDirectDispatch);
		WRITE_NODE_FIELD(directDispatch.contentIds);

		WRITE_INT_FIELD(nMotionNodes);
		WRITE_INT_FIELD(nInitPlans);

		WRITE_NODE_FIELD(sliceTable);
	}

    WRITE_NODE_FIELD(lefttree);
    WRITE_NODE_FIELD(righttree);
    WRITE_NODE_FIELD(initPlan);

	if (print_variable_fields)
	{
		WRITE_UINT64_FIELD(operatorMemKB);
	}
}

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_outScanInfo(StringInfo str, Scan *node)
{
	_outPlanInfo(str, (Plan *) node);

	if (print_variable_fields)
	{
		WRITE_UINT_FIELD(scanrelid);
	}
	else
	{
		/*
		 * Serializing for workfile caching.
		 * Instead of outputing rtable indices, serialize the actual rtable entry
		 */
		Assert(range_table != NULL);

		RangeTblEntry *rte = rt_fetch(node->scanrelid, range_table);
		/*
		 * Serialize all rtable entries except for subquery type.
		 * For subquery scan, the rtable entry contains the entire plan of the
		 * subquery, but this is serialized elsewhere in outSubqueryScan, no
		 * need to duplicate it here
		 */
		if (rte->type != RTE_SUBQUERY)
		{
			_outNode(str,rte);
		}
	}

	WRITE_INT_FIELD(partIndex);
	WRITE_INT_FIELD(partIndexPrintable);
}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_outJoinPlanInfo(StringInfo str, Join *node)
{
	_outPlanInfo(str, (Plan *) node);

	WRITE_BOOL_FIELD(prefetch_inner);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(joinqual);
}

static void
_outPlannedStmt(StringInfo str, PlannedStmt *node)
{
	WRITE_NODE_TYPE("PLANNEDSTMT");
	
	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(planGen, PlanGenerator);
	WRITE_BOOL_FIELD(canSetTag);
	WRITE_BOOL_FIELD(transientPlan);
	
	WRITE_NODE_FIELD(planTree);
	
	WRITE_NODE_FIELD(rtable);
	
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(utilityStmt);
	WRITE_NODE_FIELD(intoClause);
	WRITE_NODE_FIELD(subplans);
	WRITE_NODE_FIELD(rewindPlanIDs);
	WRITE_NODE_FIELD(returningLists);
	
	WRITE_NODE_FIELD(result_partitions);
	WRITE_NODE_FIELD(result_aosegnos);
	WRITE_NODE_FIELD(queryPartOids);
	WRITE_NODE_FIELD(queryPartsMetadata);
	WRITE_NODE_FIELD(numSelectorsPerScanId);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(relationOids);
	WRITE_NODE_FIELD(invalItems);
	WRITE_INT_FIELD(nCrossLevelParams);
	WRITE_INT_FIELD(nMotionNodes);
	WRITE_INT_FIELD(nInitPlans);
	
	/* Don't serialize policy */
	WRITE_NODE_FIELD(sliceTable);
	
	WRITE_UINT64_FIELD(query_mem);
	WRITE_NODE_FIELD(transientTypeRecords);
}

static void
_outPlan(StringInfo str, Plan *node)
{
	WRITE_NODE_TYPE("PLAN");

	_outPlanInfo(str, (Plan *) node);
}

static void
_outResult(StringInfo str, Result *node)
{
	WRITE_NODE_TYPE("RESULT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(resconstantqual);

	WRITE_BOOL_FIELD(hashFilter);
	WRITE_NODE_FIELD(hashList);
}

static void
_outRepeat(StringInfo str, Repeat *node)
{
	WRITE_NODE_TYPE("REPEAT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(repeatCountExpr);
	WRITE_UINT64_FIELD(grouping);
}

static void
_outAppend(StringInfo str, Append *node)
{
	WRITE_NODE_TYPE("APPEND");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(appendplans);
	WRITE_BOOL_FIELD(isTarget);
	WRITE_BOOL_FIELD(isZapped);
	WRITE_BOOL_FIELD(hasXslice);
}

static void
_outSequence(StringInfo str, Sequence *node)
{
	WRITE_NODE_TYPE("SEQUENCE");
	_outPlanInfo(str, (Plan *)node);
	WRITE_NODE_FIELD(subplans);
}

static void
_outBitmapAnd(StringInfo str, BitmapAnd *node)
{
	WRITE_NODE_TYPE("BITMAPAND");

	_outPlanInfo(str, (Plan *) node);

	WRITE_LIST_FIELD(bitmapplans);
}

static void
_outBitmapOr(StringInfo str, BitmapOr *node)
{
	WRITE_NODE_TYPE("BITMAPOR");

	_outPlanInfo(str, (Plan *) node);

	WRITE_LIST_FIELD(bitmapplans);
}

static void
_outScan(StringInfo str, Scan *node)
{
	WRITE_NODE_TYPE("SCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outSeqScan(StringInfo str, SeqScan *node)
{
	WRITE_NODE_TYPE("SEQSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outAppendOnlyScan(StringInfo str, AppendOnlyScan *node)
{
	WRITE_NODE_TYPE("APPENDONLYSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outAOCSScan(StringInfo str, AOCSScan *node)
{
	WRITE_NODE_TYPE("AOCSSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outTableScan(StringInfo str, TableScan *node)
{
	WRITE_NODE_TYPE("TABLESCAN");
	_outScanInfo(str, (Scan *)node);
}
static void
_outDynamicTableScan(StringInfo str, DynamicTableScan *node)
{
	WRITE_NODE_TYPE("DYNAMICTABLESCAN");
	_outScanInfo(str, (Scan *)node);
	WRITE_INT_FIELD(partIndex);
	WRITE_INT_FIELD(partIndexPrintable);
}
static void
_outExternalScan(StringInfo str, ExternalScan *node)
{
	WRITE_NODE_TYPE("EXTERNALSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(uriList);
	WRITE_NODE_FIELD(fmtOpts);
	WRITE_CHAR_FIELD(fmtType);
	WRITE_BOOL_FIELD(isMasterOnly);
	WRITE_INT_FIELD(rejLimit);
	WRITE_BOOL_FIELD(rejLimitInRows);
	WRITE_OID_FIELD(fmterrtbl);
	WRITE_INT_FIELD(encoding);
	WRITE_INT_FIELD(scancounter);
}

static void
outLogicalIndexInfo(StringInfo str, LogicalIndexInfo *node)
{
	WRITE_OID_FIELD(logicalIndexOid);
	WRITE_INT_FIELD(nColumns);
	WRITE_INT_ARRAY(indexKeys, nColumns, AttrNumber);
	WRITE_LIST_FIELD(indPred);
	WRITE_LIST_FIELD(indExprs);
	WRITE_BOOL_FIELD(indIsUnique);
	WRITE_ENUM_FIELD(indType, LogicalIndexType);
	WRITE_NODE_FIELD(partCons);
	WRITE_LIST_FIELD(defaultLevels);
}

static void
outIndexScanFields(StringInfo str, IndexScan *node)
{
	_outScanInfo(str, (Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_LIST_FIELD(indexqual);
	WRITE_LIST_FIELD(indexqualorig);
	WRITE_LIST_FIELD(indexstrategy);
	WRITE_LIST_FIELD(indexsubtype);
	WRITE_ENUM_FIELD(indexorderdir, ScanDirection);

	if (isDynamicScan(&node->scan))
	{
		Assert(node->logicalIndexInfo);
		outLogicalIndexInfo(str, node->logicalIndexInfo);
	}
	else
	{
		Assert(node->logicalIndexInfo == NULL);
	}
}

static void
_outIndexScan(StringInfo str, IndexScan *node)
{
	WRITE_NODE_TYPE("INDEXSCAN");

	outIndexScanFields(str, node);
}

static void
_outDynamicIndexScan(StringInfo str, DynamicIndexScan *node)
{
	WRITE_NODE_TYPE("DYNAMICINDEXSCAN");
	/* DynamicIndexScan has the same content as IndexScan. */
	outIndexScanFields(str, (IndexScan *) node);
}

static void
_outBitmapIndexScan(StringInfo str, BitmapIndexScan *node)
{
	WRITE_NODE_TYPE("BITMAPINDEXSCAN");
	/* BitmapIndexScan has the same content as IndexScan. */
	outIndexScanFields(str, (IndexScan *) node);
}

static void
_outBitmapHeapScan(StringInfo str, BitmapHeapScan *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_LIST_FIELD(bitmapqualorig);
}

static void
_outBitmapAppendOnlyScan(StringInfo str, BitmapAppendOnlyScan *node)
{
	WRITE_NODE_TYPE("BITMAPAPPENDONLYSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_LIST_FIELD(bitmapqualorig);
	WRITE_BOOL_FIELD(isAORow);
}

static void
_outBitmapTableScan(StringInfo str, BitmapTableScan *node)
{
	WRITE_NODE_TYPE("BITMAPTABLESCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_LIST_FIELD(bitmapqualorig);
}

static void
_outTidScan(StringInfo str, TidScan *node)
{
	WRITE_NODE_TYPE("TIDSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(tidquals);
}

static void
_outSubqueryScan(StringInfo str, SubqueryScan *node)
{
	WRITE_NODE_TYPE("SUBQUERYSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(subplan);
	/* Planner-only: subrtable -- don't serialize. */
}

static void
_outFunctionScan(StringInfo str, FunctionScan *node)
{
	WRITE_NODE_TYPE("FUNCTIONSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outValuesScan(StringInfo str, ValuesScan *node)
{
	WRITE_NODE_TYPE("VALUESSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outJoin(StringInfo str, Join *node)
{
	WRITE_NODE_TYPE("JOIN");

	_outJoinPlanInfo(str, (Join *) node);
}

static void
_outNestLoop(StringInfo str, NestLoop *node)
{
	WRITE_NODE_TYPE("NESTLOOP");

	_outJoinPlanInfo(str, (Join *) node);

    WRITE_BOOL_FIELD(outernotreferencedbyinner);    /*CDB*/
	WRITE_BOOL_FIELD(shared_outer);
	WRITE_BOOL_FIELD(singleton_outer); /*CDB-OLAP*/
}


static void
_outMergeJoin(StringInfo str, MergeJoin *node)
{
	WRITE_NODE_TYPE("MERGEJOIN");

	_outJoinPlanInfo(str, (Join *) node);

	WRITE_LIST_FIELD(mergeclauses);
	WRITE_BOOL_FIELD(unique_outer);
}

static void
_outHashJoin(StringInfo str, HashJoin *node)
{
	WRITE_NODE_TYPE("HASHJOIN");

	_outJoinPlanInfo(str, (Join *) node);

	WRITE_LIST_FIELD(hashclauses);
	WRITE_LIST_FIELD(hashqualclauses);
}

static void
_outAgg(StringInfo str, Agg *node)
{

	WRITE_NODE_TYPE("AGG");

	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(aggstrategy, AggStrategy);
	WRITE_INT_FIELD(numCols);

	WRITE_INT_ARRAY(grpColIdx, numCols, AttrNumber);

	if (print_variable_fields)
	{
		WRITE_LONG_FIELD(numGroups);
		WRITE_INT_FIELD(transSpace);
	}
	WRITE_INT_FIELD(numNullCols);
	WRITE_UINT64_FIELD(inputGrouping);
	WRITE_UINT64_FIELD(grouping);
	WRITE_BOOL_FIELD(inputHasGrouping);
	WRITE_INT_FIELD(rollupGSTimes);
	WRITE_BOOL_FIELD(lastAgg);
	WRITE_BOOL_FIELD(streaming);
}

static void
_outWindowKey(StringInfo str, WindowKey *node)
{
	WRITE_NODE_TYPE("WINDOWKEY");
	WRITE_INT_FIELD(numSortCols);

	WRITE_INT_ARRAY(sortColIdx, numSortCols, AttrNumber);
	WRITE_OID_ARRAY(sortOperators, numSortCols);
	WRITE_NODE_FIELD(frame);
}


static void
_outWindow(StringInfo str, Window *node)
{
	WRITE_NODE_TYPE("WINDOW");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numPartCols);

	WRITE_INT_ARRAY(partColIdx, numPartCols, AttrNumber);

	WRITE_NODE_FIELD(windowKeys);
}

static void
_outTableFunctionScan(StringInfo str, TableFunctionScan *node)
{
	WRITE_NODE_TYPE("TABLEFUNCTIONSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outMaterial(StringInfo str, Material *node)
{
	WRITE_NODE_TYPE("MATERIAL");

    WRITE_BOOL_FIELD(cdb_strict);

	WRITE_ENUM_FIELD(share_type, ShareType);
	WRITE_INT_FIELD(share_id);
	WRITE_INT_FIELD(driver_slice);
	WRITE_INT_FIELD(nsharer);
	WRITE_INT_FIELD(nsharer_xslice);

	_outPlanInfo(str, (Plan *) node);
}

static void
_outShareInputScan(StringInfo str, ShareInputScan *node)
{
	WRITE_NODE_TYPE("SHAREINPUTSCAN");

	WRITE_ENUM_FIELD(share_type, ShareType);
	WRITE_INT_FIELD(share_id);
	WRITE_INT_FIELD(driver_slice);

	_outPlanInfo(str, (Plan *) node);
}

static void
_outSort(StringInfo str, Sort *node)
{

	WRITE_NODE_TYPE("SORT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	WRITE_INT_ARRAY(sortColIdx, numCols, AttrNumber);

	WRITE_OID_ARRAY(sortOperators, numCols);

    /* CDB */
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
    WRITE_BOOL_FIELD(noduplicates);

	WRITE_ENUM_FIELD(share_type, ShareType);
	WRITE_INT_FIELD(share_id);
	WRITE_INT_FIELD(driver_slice);
	WRITE_INT_FIELD(nsharer);
	WRITE_INT_FIELD(nsharer_xslice);
}

static void
_outUnique(StringInfo str, Unique *node)
{

	WRITE_NODE_TYPE("UNIQUE");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	WRITE_INT_ARRAY(uniqColIdx, numCols, AttrNumber);

}

static void
_outSetOp(StringInfo str, SetOp *node)
{

	WRITE_NODE_TYPE("SETOP");

	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(cmd, SetOpCmd);
	WRITE_INT_FIELD(numCols);

	WRITE_INT_ARRAY(dupColIdx, numCols, AttrNumber);

	WRITE_INT_FIELD(flagColIdx);
}

static void
_outLimit(StringInfo str, Limit *node)
{
	WRITE_NODE_TYPE("LIMIT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
}

static void
_outHash(StringInfo str, Hash *node)
{
	WRITE_NODE_TYPE("HASH");

	_outPlanInfo(str, (Plan *) node);
    WRITE_BOOL_FIELD(rescannable);          /*CDB*/
}

static void
_outMotion(StringInfo str, Motion *node)
{

	WRITE_NODE_TYPE("MOTION");

	WRITE_INT_FIELD(motionID);
	WRITE_ENUM_FIELD(motionType, MotionType);
	WRITE_BOOL_FIELD(sendSorted);

	WRITE_LIST_FIELD(hashExpr);
	WRITE_LIST_FIELD(hashDataTypes);

	WRITE_INT_FIELD(numOutputSegs);
	WRITE_INT_ARRAY(outputSegIdx, numOutputSegs, int);

	WRITE_INT_FIELD(numSortCols);
	WRITE_INT_ARRAY(sortColIdx, numSortCols, AttrNumber);
	WRITE_OID_ARRAY(sortOperators, numSortCols);

	WRITE_INT_FIELD(segidColIdx);

	_outPlanInfo(str, (Plan *) node);
}

/*
 * _outDML
 */
static void
_outDML(StringInfo str, DML *node)
{
	WRITE_NODE_TYPE("DML");

	WRITE_UINT_FIELD(scanrelid);
	WRITE_INT_FIELD(oidColIdx);
	WRITE_INT_FIELD(actionColIdx);
	WRITE_INT_FIELD(ctidColIdx);
	WRITE_INT_FIELD(tupleoidColIdx);

	_outPlanInfo(str, (Plan *) node);
}


/*
 * _outSplitUpdate
 */
static void
_outSplitUpdate(StringInfo str, SplitUpdate *node)
{
	WRITE_NODE_TYPE("SPLITUPDATE");

	WRITE_INT_FIELD(actionColIdx);
	WRITE_INT_FIELD(ctidColIdx);
	WRITE_INT_FIELD(tupleoidColIdx);
	WRITE_NODE_FIELD(insertColIdx);
	WRITE_NODE_FIELD(deleteColIdx);

	_outPlanInfo(str, (Plan *) node);
}

/*
 * _outRowTrigger
 */
static void
_outRowTrigger(StringInfo str, RowTrigger *node)
{
	WRITE_NODE_TYPE("ROWTRIGGER");

	WRITE_INT_FIELD(relid);
	WRITE_INT_FIELD(eventFlags);
	WRITE_NODE_FIELD(oldValuesColIdx);
	WRITE_NODE_FIELD(newValuesColIdx);

	_outPlanInfo(str, (Plan *) node);
}

/*
 * _outAssertOp
 */
static void
_outAssertOp(StringInfo str, AssertOp *node)
{
	WRITE_NODE_TYPE("ASSERTOP");

	WRITE_NODE_FIELD(errmessage);
	WRITE_INT_FIELD(errcode);
	
	_outPlanInfo(str, (Plan *) node);
}

/*
 * _outPartitionSelector
 */
static void
_outPartitionSelector(StringInfo str, PartitionSelector *node)
{
	WRITE_NODE_TYPE("PARTITIONSELECTOR");

	WRITE_INT_FIELD(relid);
	WRITE_INT_FIELD(nLevels);
	WRITE_INT_FIELD(scanId);
	WRITE_INT_FIELD(selectorId);
	WRITE_NODE_FIELD(levelEqExpressions);
	WRITE_NODE_FIELD(levelExpressions);
	WRITE_NODE_FIELD(residualPredicate);
	WRITE_NODE_FIELD(propagationExpression);
	WRITE_NODE_FIELD(printablePredicate);
	WRITE_BOOL_FIELD(staticSelection);
	WRITE_NODE_FIELD(staticPartOids);
	WRITE_NODE_FIELD(staticScanIds);

	_outPlanInfo(str, (Plan *) node);
}

/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

static void
_outAlias(StringInfo str, Alias *node)
{
	WRITE_NODE_TYPE("ALIAS");

	WRITE_STRING_FIELD(aliasname);
	WRITE_NODE_FIELD(colnames);
}

static void
_outRangeVar(StringInfo str, RangeVar *node)
{
	WRITE_NODE_TYPE("RANGEVAR");

	/*
	 * we deliberately ignore catalogname here, since it is presently not
	 * semantically meaningful
	 */
	WRITE_STRING_FIELD(schemaname);
	WRITE_STRING_FIELD(relname);
	WRITE_ENUM_FIELD(inhOpt, InhOption);
	WRITE_BOOL_FIELD(istemp);
	WRITE_NODE_FIELD(alias);
    WRITE_INT_FIELD(location);  /*CDB*/
}

static void
_outIntoClause(StringInfo str, IntoClause *node)
{
	WRITE_NODE_TYPE("INTOCLAUSE");
	
	WRITE_NODE_FIELD(rel);
	WRITE_NODE_FIELD(colNames);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(onCommit, OnCommitAction);
	WRITE_STRING_FIELD(tableSpaceName);
	WRITE_OID_FIELD(oidInfo.relOid);
	WRITE_OID_FIELD(oidInfo.comptypeOid);
	WRITE_OID_FIELD(oidInfo.toastOid);
	WRITE_OID_FIELD(oidInfo.toastIndexOid);
	WRITE_OID_FIELD(oidInfo.toastComptypeOid);
	WRITE_OID_FIELD(oidInfo.aosegOid);
	WRITE_OID_FIELD(oidInfo.aosegIndexOid);
	WRITE_OID_FIELD(oidInfo.aosegComptypeOid);
	WRITE_OID_FIELD(oidInfo.aovisimapOid);
	WRITE_OID_FIELD(oidInfo.aovisimapIndexOid);
	WRITE_OID_FIELD(oidInfo.aovisimapComptypeOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirIndexOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirComptypeOid);	

}

static void
_outVar(StringInfo str, Var *node)
{
	WRITE_NODE_TYPE("VAR");

	if (print_variable_fields)
	{
		WRITE_UINT_FIELD(varno);
	}
	WRITE_INT_FIELD(varattno);
	WRITE_OID_FIELD(vartype);
	WRITE_INT_FIELD(vartypmod);
	WRITE_UINT_FIELD(varlevelsup);
	if (print_variable_fields)
	{
		WRITE_UINT_FIELD(varnoold);
	}
	WRITE_INT_FIELD(varoattno);
}

static void
_outConst(StringInfo str, Const *node)
{
	WRITE_NODE_TYPE("CONST");

	WRITE_OID_FIELD(consttype);
	WRITE_INT_FIELD(constlen);
	WRITE_BOOL_FIELD(constbyval);
	WRITE_BOOL_FIELD(constisnull);

	if (!node->constisnull)
		_outDatum(str, node->constvalue, node->constlen, node->constbyval);
}

static void
_outParam(StringInfo str, Param *node)
{
	WRITE_NODE_TYPE("PARAM");

	WRITE_ENUM_FIELD(paramkind, ParamKind);
	WRITE_INT_FIELD(paramid);
	WRITE_OID_FIELD(paramtype);
}

static void
_outAggref(StringInfo str, Aggref *node)
{
	WRITE_NODE_TYPE("AGGREF");

	WRITE_OID_FIELD(aggfnoid);
	WRITE_OID_FIELD(aggtype);
	WRITE_NODE_FIELD(args);
	WRITE_UINT_FIELD(agglevelsup);
	WRITE_BOOL_FIELD(aggstar);
	WRITE_BOOL_FIELD(aggdistinct);

	WRITE_ENUM_FIELD(aggstage, AggStage);
    WRITE_NODE_FIELD(aggorder);

}

static void
_outAggOrder(StringInfo str, AggOrder *node)
{
	WRITE_NODE_TYPE("AGGORDER");

    WRITE_BOOL_FIELD(sortImplicit);
    WRITE_NODE_FIELD(sortTargets);
    WRITE_NODE_FIELD(sortClause);
}

static void
_outWindowRef(StringInfo str, WindowRef *node)
{
	WRITE_NODE_TYPE("WINDOWREF");

	WRITE_OID_FIELD(winfnoid);
	WRITE_OID_FIELD(restype);
	WRITE_NODE_FIELD(args);
	WRITE_UINT_FIELD(winlevelsup);
	WRITE_BOOL_FIELD(windistinct);
	WRITE_UINT_FIELD(winspec);
	WRITE_UINT_FIELD(winindex);
	WRITE_ENUM_FIELD(winstage, WinStage);
	WRITE_UINT_FIELD(winlevel);
}

	static void
_outArrayRef(StringInfo str, ArrayRef *node)
{
	WRITE_NODE_TYPE("ARRAYREF");

	WRITE_OID_FIELD(refrestype);
	WRITE_OID_FIELD(refarraytype);
	WRITE_OID_FIELD(refelemtype);
	WRITE_NODE_FIELD(refupperindexpr);
	WRITE_NODE_FIELD(reflowerindexpr);
	WRITE_NODE_FIELD(refexpr);
	WRITE_NODE_FIELD(refassgnexpr);
}

static void
_outFuncExpr(StringInfo str, FuncExpr *node)
{
	WRITE_NODE_TYPE("FUNCEXPR");

	WRITE_OID_FIELD(funcid);
	WRITE_OID_FIELD(funcresulttype);
	WRITE_BOOL_FIELD(funcretset);
	WRITE_ENUM_FIELD(funcformat, CoercionForm);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(is_tablefunc);
}

static void
_outOpExpr(StringInfo str, OpExpr *node)
{
	WRITE_NODE_TYPE("OPEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_NODE_FIELD(args);
}

static void
_outDistinctExpr(StringInfo str, DistinctExpr *node)
{
	WRITE_NODE_TYPE("DISTINCTEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_NODE_FIELD(args);
}

static void
_outScalarArrayOpExpr(StringInfo str, ScalarArrayOpExpr *node)
{
	WRITE_NODE_TYPE("SCALARARRAYOPEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_BOOL_FIELD(useOr);
	WRITE_NODE_FIELD(args);
}

static void
_outBoolExpr(StringInfo str, BoolExpr *node)
{


	WRITE_NODE_TYPE("BOOLEXPR");
	WRITE_ENUM_FIELD(boolop, BoolExprType);

	WRITE_NODE_FIELD(args);
}

static void
_outSubLink(StringInfo str, SubLink *node)
{
	WRITE_NODE_TYPE("SUBLINK");

	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(operName);
	WRITE_INT_FIELD(location);      /*CDB*/
	WRITE_NODE_FIELD(subselect);
}

static void
_outSubPlan(StringInfo str, SubPlan *node)
{
	WRITE_NODE_TYPE("SUBPLAN");

    WRITE_INT_FIELD(qDispSliceId);  /*CDB*/
	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(paramIds);
	WRITE_INT_FIELD(plan_id);
	WRITE_OID_FIELD(firstColType);
	WRITE_INT_FIELD(firstColTypmod);
	WRITE_BOOL_FIELD(useHashTable);
	WRITE_BOOL_FIELD(unknownEqFalse);
	WRITE_BOOL_FIELD(is_initplan); /*CDB*/
	WRITE_BOOL_FIELD(is_multirow); /*CDB*/
	WRITE_NODE_FIELD(setParam);
	WRITE_NODE_FIELD(parParam);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(extParam);
}

static void
_outFieldSelect(StringInfo str, FieldSelect *node)
{
	WRITE_NODE_TYPE("FIELDSELECT");

	WRITE_NODE_FIELD(arg);
	WRITE_INT_FIELD(fieldnum);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
}

static void
_outFieldStore(StringInfo str, FieldStore *node)
{
	WRITE_NODE_TYPE("FIELDSTORE");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(newvals);
	WRITE_NODE_FIELD(fieldnums);
	WRITE_OID_FIELD(resulttype);
}

static void
_outRelabelType(StringInfo str, RelabelType *node)
{
	WRITE_NODE_TYPE("RELABELTYPE");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_ENUM_FIELD(relabelformat, CoercionForm);
}

static void
_outConvertRowtypeExpr(StringInfo str, ConvertRowtypeExpr *node)
{
	WRITE_NODE_TYPE("CONVERTROWTYPEEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_ENUM_FIELD(convertformat, CoercionForm);
}

static void
_outCaseExpr(StringInfo str, CaseExpr *node)
{
	WRITE_NODE_TYPE("CASE");

	WRITE_OID_FIELD(casetype);
	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(defresult);
}

static void
_outCaseWhen(StringInfo str, CaseWhen *node)
{
	WRITE_NODE_TYPE("WHEN");

	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(result);
}

static void
_outCaseTestExpr(StringInfo str, CaseTestExpr *node)
{
	WRITE_NODE_TYPE("CASETESTEXPR");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}

static void
_outArrayExpr(StringInfo str, ArrayExpr *node)
{
	WRITE_NODE_TYPE("ARRAY");

	WRITE_OID_FIELD(array_typeid);
	WRITE_OID_FIELD(element_typeid);
	WRITE_NODE_FIELD(elements);
	WRITE_BOOL_FIELD(multidims);
}

static void
_outRowExpr(StringInfo str, RowExpr *node)
{
	WRITE_NODE_TYPE("ROW");

	WRITE_NODE_FIELD(args);
	WRITE_OID_FIELD(row_typeid);
	WRITE_ENUM_FIELD(row_format, CoercionForm);
}

static void
_outRowCompareExpr(StringInfo str, RowCompareExpr *node)
{
	WRITE_NODE_TYPE("ROWCOMPARE");

	WRITE_ENUM_FIELD(rctype, RowCompareType);
	WRITE_NODE_FIELD(opnos);
	WRITE_NODE_FIELD(opclasses);
	WRITE_NODE_FIELD(largs);
	WRITE_NODE_FIELD(rargs);
}

static void
_outCoalesceExpr(StringInfo str, CoalesceExpr *node)
{
	WRITE_NODE_TYPE("COALESCE");

	WRITE_OID_FIELD(coalescetype);
	WRITE_NODE_FIELD(args);
}

static void
_outMinMaxExpr(StringInfo str, MinMaxExpr *node)
{
	WRITE_NODE_TYPE("MINMAX");

	WRITE_OID_FIELD(minmaxtype);
	WRITE_ENUM_FIELD(op, MinMaxOp);
	WRITE_NODE_FIELD(args);
}

static void
_outNullIfExpr(StringInfo str, NullIfExpr *node)
{
	WRITE_NODE_TYPE("NULLIFEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_NODE_FIELD(args);
}

static void
_outNullTest(StringInfo str, NullTest *node)
{
	WRITE_NODE_TYPE("NULLTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(nulltesttype, NullTestType);
}

static void
_outBooleanTest(StringInfo str, BooleanTest *node)
{
	WRITE_NODE_TYPE("BOOLEANTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(booltesttype, BoolTestType);
}

static void
_outCoerceToDomain(StringInfo str, CoerceToDomain *node)
{
	WRITE_NODE_TYPE("COERCETODOMAIN");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_ENUM_FIELD(coercionformat, CoercionForm);
}

static void
_outCoerceToDomainValue(StringInfo str, CoerceToDomainValue *node)
{
	WRITE_NODE_TYPE("COERCETODOMAINVALUE");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}

static void
_outSetToDefault(StringInfo str, SetToDefault *node)
{
	WRITE_NODE_TYPE("SETTODEFAULT");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}

static void
_outCurrentOfExpr(StringInfo str, CurrentOfExpr *node)
{
	WRITE_NODE_TYPE("CURRENTOFEXPR");

	WRITE_STRING_FIELD(cursor_name);
	WRITE_UINT_FIELD(cvarno);
	WRITE_OID_FIELD(target_relid);
	WRITE_INT_FIELD(gp_segment_id);
	WRITE_BINARY_FIELD(ctid, sizeof(ItemPointerData));	
	WRITE_OID_FIELD(tableoid);
}

static void
_outTargetEntry(StringInfo str, TargetEntry *node)
{
	WRITE_NODE_TYPE("TARGETENTRY");

	WRITE_NODE_FIELD(expr);
	WRITE_INT_FIELD(resno);
	WRITE_STRING_FIELD(resname);
	WRITE_UINT_FIELD(ressortgroupref);
	WRITE_OID_FIELD(resorigtbl);
	WRITE_INT_FIELD(resorigcol);
	WRITE_BOOL_FIELD(resjunk);
}

static void
_outRangeTblRef(StringInfo str, RangeTblRef *node)
{
	WRITE_NODE_TYPE("RANGETBLREF");

	WRITE_INT_FIELD(rtindex);
}

static void
_outJoinExpr(StringInfo str, JoinExpr *node)
{
	WRITE_NODE_TYPE("JOINEXPR");

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_BOOL_FIELD(isNatural);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(usingClause);
	WRITE_NODE_FIELD(quals);
	WRITE_NODE_FIELD(alias);
	WRITE_INT_FIELD(rtindex);
}

static void
_outFromExpr(StringInfo str, FromExpr *node)
{
	WRITE_NODE_TYPE("FROMEXPR");

	WRITE_NODE_FIELD(fromlist);
	WRITE_NODE_FIELD(quals);
}

static void
_outFlow(StringInfo str, Flow *node)
{

	WRITE_NODE_TYPE("FLOW");

	WRITE_ENUM_FIELD(flotype, FlowType);
	WRITE_ENUM_FIELD(req_move, Movement);
	WRITE_ENUM_FIELD(locustype, CdbLocusType);
	WRITE_INT_FIELD(segindex);

	/* This array format as in Group and Sort nodes. */
	WRITE_INT_FIELD(numSortCols);

	WRITE_INT_ARRAY(sortColIdx, numSortCols, AttrNumber);
	WRITE_OID_ARRAY(sortOperators, numSortCols);


	WRITE_NODE_FIELD(hashExpr);

	WRITE_NODE_FIELD(flow_before_req_move);
}

/*****************************************************************************
 *
 *	Stuff from cdbpathlocus.h.
 *
 *****************************************************************************/

/*
 * _outCdbPathLocus
 */
static void
_outCdbPathLocus(StringInfo str, CdbPathLocus *node)
{
    WRITE_ENUM_FIELD(locustype, CdbLocusType);
    WRITE_NODE_FIELD(partkey);
}                               /* _outCdbPathLocus */


/*****************************************************************************
 *
 *	Stuff from relation.h.
 *
 *****************************************************************************/

/*
 * print the basic stuff of all nodes that inherit from Path
 *
 * Note we do NOT print the parent, else we'd be in infinite recursion
 */
static void
_outPathInfo(StringInfo str, Path *node)
{
	WRITE_ENUM_FIELD(pathtype, NodeTag);
	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(total_cost, "%.2f");
    WRITE_NODE_FIELD(parent);
    _outCdbPathLocus(str, &node->locus);
	WRITE_NODE_FIELD(pathkeys);
}

/*
 * print the basic stuff of all nodes that inherit from JoinPath
 */
static void
_outJoinPathInfo(StringInfo str, JoinPath *node)
{
	_outPathInfo(str, (Path *) node);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(outerjoinpath);
	WRITE_NODE_FIELD(innerjoinpath);
	WRITE_NODE_FIELD(joinrestrictinfo);
}

static void
_outPath(StringInfo str, Path *node)
{
	WRITE_NODE_TYPE("PATH");

	_outPathInfo(str, (Path *) node);
}

static void
_outIndexPath(StringInfo str, IndexPath *node)
{
	WRITE_NODE_TYPE("INDEXPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(indexinfo);
	WRITE_NODE_FIELD(indexclauses);
	WRITE_NODE_FIELD(indexquals);
	WRITE_BOOL_FIELD(isjoininner);
	WRITE_ENUM_FIELD(indexscandir, ScanDirection);
	WRITE_FLOAT_FIELD(indextotalcost, "%.2f");
	WRITE_FLOAT_FIELD(indexselectivity, "%.4f");
	WRITE_FLOAT_FIELD(rows, "%.0f");
    WRITE_INT_FIELD(num_leading_eq);
}

static void
_outBitmapHeapPath(StringInfo str, BitmapHeapPath *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapqual);
	WRITE_BOOL_FIELD(isjoininner);
	WRITE_FLOAT_FIELD(rows, "%.0f");
}

static void
_outBitmapAppendOnlyPath(StringInfo str, BitmapAppendOnlyPath *node)
{
	WRITE_NODE_TYPE("BITMAPAPPENDONLYPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapqual);
	WRITE_BOOL_FIELD(isjoininner);
	WRITE_FLOAT_FIELD(rows, "%.0f");
	WRITE_BOOL_FIELD(isAORow);
}

static void
_outBitmapAndPath(StringInfo str, BitmapAndPath *node)
{
	WRITE_NODE_TYPE("BITMAPANDPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapquals);
	WRITE_FLOAT_FIELD(bitmapselectivity, "%.4f");
}

static void
_outBitmapOrPath(StringInfo str, BitmapOrPath *node)
{
	WRITE_NODE_TYPE("BITMAPORPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapquals);
	WRITE_FLOAT_FIELD(bitmapselectivity, "%.4f");
}

static void
_outTidPath(StringInfo str, TidPath *node)
{
	WRITE_NODE_TYPE("TIDPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(tidquals);
}

static void
_outAppendPath(StringInfo str, AppendPath *node)
{
	WRITE_NODE_TYPE("APPENDPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(subpaths);
}

static void
_outAppendOnlyPath(StringInfo str, AppendOnlyPath *node)
{
	WRITE_NODE_TYPE("APPENDONLYPATH");

	_outPathInfo(str, (Path *) node);
}

static void
_outAOCSPath(StringInfo str, AOCSPath *node)
{
	WRITE_NODE_TYPE("AOCSPATH");

	_outPathInfo(str, (Path *) node);
}
static void
_outResultPath(StringInfo str, ResultPath *node)
{
	WRITE_NODE_TYPE("RESULTPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(quals);
}

static void
_outMaterialPath(StringInfo str, MaterialPath *node)
{
	WRITE_NODE_TYPE("MATERIALPATH");

	_outPathInfo(str, (Path *) node);
    WRITE_BOOL_FIELD(cdb_strict);

	WRITE_NODE_FIELD(subpath);
}

static void
_outUniquePath(StringInfo str, UniquePath *node)
{
	WRITE_NODE_TYPE("UNIQUEPATH");

	_outPathInfo(str, (Path *) node);
	WRITE_ENUM_FIELD(umethod, UniquePathMethod);
	WRITE_FLOAT_FIELD(rows, "%.0f");
    WRITE_BOOL_FIELD(must_repartition);                 /*CDB*/
    WRITE_BITMAPSET_FIELD(distinct_on_rowid_relids);    /*CDB*/
	WRITE_NODE_FIELD(distinct_on_exprs);                /*CDB*/

	WRITE_NODE_FIELD(subpath);
}

static void
_outNestPath(StringInfo str, NestPath *node)
{
	WRITE_NODE_TYPE("NESTPATH");

	_outJoinPathInfo(str, (JoinPath *) node);
}

static void
_outMergePath(StringInfo str, MergePath *node)
{
	WRITE_NODE_TYPE("MERGEPATH");

	_outJoinPathInfo(str, (JoinPath *) node);

	WRITE_NODE_FIELD(path_mergeclauses);
	WRITE_NODE_FIELD(outersortkeys);
	WRITE_NODE_FIELD(innersortkeys);
}

static void
_outHashPath(StringInfo str, HashPath *node)
{
	WRITE_NODE_TYPE("HASHPATH");

	_outJoinPathInfo(str, (JoinPath *) node);

	WRITE_NODE_FIELD(path_hashclauses);
}

static void
_outCdbMotionPath(StringInfo str, CdbMotionPath *node)
{
    WRITE_NODE_TYPE("MOTIONPATH");

    _outPathInfo(str, &node->path);

    WRITE_NODE_FIELD(subpath);
}

static void
_outPlannerInfo(StringInfo str, PlannerInfo *node)
{
	WRITE_NODE_TYPE("PLANNERINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(parse);
	WRITE_NODE_FIELD(join_rel_list);
	WRITE_NODE_FIELD(equi_key_list);
	WRITE_NODE_FIELD(left_join_clauses);
	WRITE_NODE_FIELD(right_join_clauses);
	WRITE_NODE_FIELD(full_join_clauses);
	WRITE_NODE_FIELD(oj_info_list);
	WRITE_NODE_FIELD(in_info_list);
	WRITE_NODE_FIELD(append_rel_list);
	WRITE_NODE_FIELD(query_pathkeys);
	WRITE_NODE_FIELD(group_pathkeys);
	WRITE_NODE_FIELD(sort_pathkeys);
	WRITE_FLOAT_FIELD(total_table_pages, "%.0f");
	WRITE_FLOAT_FIELD(tuple_fraction, "%.4f");
	WRITE_BOOL_FIELD(hasJoinRTEs);
	WRITE_BOOL_FIELD(hasOuterJoins);
	WRITE_BOOL_FIELD(hasHavingQual);
	WRITE_BOOL_FIELD(hasPseudoConstantQuals);
}

static void
_outRelOptInfo(StringInfo str, RelOptInfo *node)
{
	WRITE_NODE_TYPE("RELOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_ENUM_FIELD(reloptkind, RelOptKind);
	WRITE_BITMAPSET_FIELD(relids);
	WRITE_FLOAT_FIELD(rows, "%.0f");
	WRITE_INT_FIELD(width);
	WRITE_NODE_FIELD(reltargetlist);
	/* Skip writing Path ptrs to avoid endless recursion */
	/* WRITE_NODE_FIELD(pathlist); */
	/* WRITE_NODE_FIELD(cheapest_startup_path); */
	/* WRITE_NODE_FIELD(cheapest_total_path); */

	WRITE_NODE_FIELD(dedup_info);
	WRITE_UINT_FIELD(relid);
	WRITE_ENUM_FIELD(rtekind, RTEKind);
	WRITE_INT_FIELD(min_attr);
	WRITE_INT_FIELD(max_attr);
	WRITE_NODE_FIELD(indexlist);
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_NODE_FIELD(subplan);
	WRITE_NODE_FIELD(locationlist);
	WRITE_STRING_FIELD(execcommand);
	WRITE_CHAR_FIELD(fmttype);
	WRITE_STRING_FIELD(fmtopts);
	WRITE_INT_FIELD(rejectlimit);
	WRITE_CHAR_FIELD(rejectlimittype);
	WRITE_OID_FIELD(fmterrtbl);
	WRITE_INT_FIELD(ext_encoding);
	WRITE_BOOL_FIELD(isrescannable);
	WRITE_BOOL_FIELD(writable);
	WRITE_NODE_FIELD(baserestrictinfo);
	WRITE_NODE_FIELD(joininfo);
	WRITE_BITMAPSET_FIELD(index_outer_relids);
	/* Skip writing Path ptrs to avoid endless recursion */
	/* WRITE_NODE_FIELD(index_inner_paths); */
}

static void
_outIndexOptInfo(StringInfo str, IndexOptInfo *node)
{

	WRITE_NODE_TYPE("INDEXOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_OID_FIELD(indexoid);
	/* Do NOT print rel field, else infinite recursion */
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_INT_FIELD(ncolumns);

	WRITE_INT_ARRAY(classlist, ncolumns, int);
	WRITE_INT_ARRAY(indexkeys, ncolumns, int);
	WRITE_INT_ARRAY(ordering, ncolumns, int);


    WRITE_OID_FIELD(relam);
	WRITE_OID_FIELD(amcostestimate);
	WRITE_NODE_FIELD(indexprs);
	WRITE_NODE_FIELD(indpred);
	WRITE_BOOL_FIELD(predOK);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(amoptionalkey);
	WRITE_BOOL_FIELD(cdb_default_stats_used);
}

static void
_outCdbRelDedupInfo(StringInfo str, CdbRelDedupInfo *node)
{
	WRITE_NODE_TYPE("CdbRelDedupInfo");

	WRITE_BITMAPSET_FIELD(prejoin_dedup_subqrelids);
	WRITE_BITMAPSET_FIELD(spent_subqrelids);
	WRITE_BOOL_FIELD(try_postjoin_dedup);
	WRITE_BOOL_FIELD(no_more_subqueries);
	WRITE_NODE_FIELD(join_unique_ininfo);
	/* Skip writing Path ptrs to avoid endless recursion */
	/* WRITE_NODE_FIELD(later_dedup_pathlist);  */
	/* WRITE_NODE_FIELD(cheapest_startup_path); */
	/* WRITE_NODE_FIELD(cheapest_total_path);   */
}

static void
_outPathKeyItem(StringInfo str, PathKeyItem *node)
{
	WRITE_NODE_TYPE("PATHKEYITEM");

	WRITE_NODE_FIELD(key);
	WRITE_OID_FIELD(sortop);
}

static void
_outRestrictInfo(StringInfo str, RestrictInfo *node)
{
	WRITE_NODE_TYPE("RESTRICTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(clause);
	WRITE_BOOL_FIELD(is_pushed_down);
	WRITE_BOOL_FIELD(outerjoin_delayed);
	WRITE_BOOL_FIELD(can_join);
	WRITE_BOOL_FIELD(pseudoconstant);
	WRITE_BITMAPSET_FIELD(clause_relids);
	WRITE_BITMAPSET_FIELD(required_relids);
	WRITE_BITMAPSET_FIELD(left_relids);
	WRITE_BITMAPSET_FIELD(right_relids);
	WRITE_NODE_FIELD(orclause);
	WRITE_OID_FIELD(mergejoinoperator);
	WRITE_OID_FIELD(left_sortop);
	WRITE_OID_FIELD(right_sortop);
	WRITE_NODE_FIELD(left_pathkey);
	WRITE_NODE_FIELD(right_pathkey);
	WRITE_OID_FIELD(hashjoinoperator);
}

static void
_outInnerIndexscanInfo(StringInfo str, InnerIndexscanInfo *node)
{
	WRITE_NODE_TYPE("INNERINDEXSCANINFO");
	WRITE_BITMAPSET_FIELD(other_relids);
	WRITE_BOOL_FIELD(isouterjoin);
	WRITE_NODE_FIELD(cheapest_startup_innerpath);
	WRITE_NODE_FIELD(cheapest_total_innerpath);
}

static void
_outOuterJoinInfo(StringInfo str, OuterJoinInfo *node)
{
	WRITE_NODE_TYPE("OUTERJOININFO");

	WRITE_BITMAPSET_FIELD(min_lefthand);
	WRITE_BITMAPSET_FIELD(min_righthand);
	WRITE_ENUM_FIELD(join_type, JoinType);
	WRITE_BOOL_FIELD(lhs_strict);
}

static void
_outInClauseInfo(StringInfo str, InClauseInfo *node)
{
	WRITE_NODE_TYPE("INCLAUSEINFO");

	WRITE_BITMAPSET_FIELD(righthand);
    WRITE_BOOL_FIELD(try_join_unique);                  /*CDB*/
	WRITE_NODE_FIELD(sub_targetlist);
}

static void
_outAppendRelInfo(StringInfo str, AppendRelInfo *node)
{
	WRITE_NODE_TYPE("APPENDRELINFO");

	WRITE_UINT_FIELD(parent_relid);
	WRITE_UINT_FIELD(child_relid);
	WRITE_OID_FIELD(parent_reltype);
	WRITE_OID_FIELD(child_reltype);
	WRITE_NODE_FIELD(col_mappings);
	WRITE_NODE_FIELD(translated_vars);
	WRITE_OID_FIELD(parent_reloid);
}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

static void
_outCreateStmt(StringInfo str, CreateStmt *node)
{
	WRITE_NODE_TYPE("CREATESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(tableElts);
	WRITE_NODE_FIELD(inhRelations);
	WRITE_NODE_FIELD(inhOids);
	WRITE_INT_FIELD(parentOidCount);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(oncommit, OnCommitAction);
	WRITE_STRING_FIELD(tablespacename);
	WRITE_NODE_FIELD(distributedBy);
	WRITE_OID_FIELD(oidInfo.relOid);
	WRITE_OID_FIELD(oidInfo.comptypeOid);
	WRITE_OID_FIELD(oidInfo.toastOid);
	WRITE_OID_FIELD(oidInfo.toastIndexOid);
	WRITE_OID_FIELD(oidInfo.toastComptypeOid);
	WRITE_OID_FIELD(oidInfo.aosegOid);
	WRITE_OID_FIELD(oidInfo.aosegIndexOid);
	WRITE_OID_FIELD(oidInfo.aosegComptypeOid);
	WRITE_OID_FIELD(oidInfo.aovisimapOid);
	WRITE_OID_FIELD(oidInfo.aovisimapIndexOid);
	WRITE_OID_FIELD(oidInfo.aovisimapComptypeOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirIndexOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirComptypeOid);
	WRITE_CHAR_FIELD(relKind);
	WRITE_CHAR_FIELD(relStorage);
	/* policy omitted */
	/* postCreate - for analysis, QD only */
	/* deferredStmts - for analysis, QD only */
	WRITE_BOOL_FIELD(is_part_child);
	WRITE_BOOL_FIELD(is_add_part);
	WRITE_BOOL_FIELD(is_split_part);
	WRITE_OID_FIELD(ownerid);
	WRITE_BOOL_FIELD(buildAoBlkdir);
	WRITE_BOOL_FIELD(is_error_table);
	WRITE_NODE_FIELD(attr_encodings);
}

static void
_outColumnReferenceStorageDirective(StringInfo str, ColumnReferenceStorageDirective *node)
{
	WRITE_NODE_TYPE("COLUMNREFERENCESTORAGEDIRECTIVE");
	
	WRITE_NODE_FIELD(column);
	WRITE_BOOL_FIELD(deflt);
	WRITE_NODE_FIELD(encoding);
}

static void
_outPartitionBy(StringInfo str, PartitionBy *node)
{
	WRITE_NODE_TYPE("PARTITIONBY");
	WRITE_ENUM_FIELD(partType, PartitionByType);
	WRITE_NODE_FIELD(keys);
	WRITE_NODE_FIELD(keyopclass);
	WRITE_NODE_FIELD(partNum);
	WRITE_NODE_FIELD(subPart);
	WRITE_NODE_FIELD(partSpec);
	WRITE_INT_FIELD(partDepth);
	WRITE_INT_FIELD(partQuiet);
	WRITE_INT_FIELD(location);
}

static void
_outPartitionSpec(StringInfo str, PartitionSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONSPEC");
	WRITE_NODE_FIELD(partElem);
	WRITE_NODE_FIELD(subSpec);
	WRITE_BOOL_FIELD(istemplate);
	WRITE_INT_FIELD(location);
	WRITE_NODE_FIELD(enc_clauses);
}

static void
_outPartitionElem(StringInfo str, PartitionElem *node)
{
	WRITE_NODE_TYPE("PARTITIONELEM");
	WRITE_NODE_FIELD(partName);
	WRITE_NODE_FIELD(boundSpec);
	WRITE_NODE_FIELD(subSpec);
	WRITE_BOOL_FIELD(isDefault);
	WRITE_NODE_FIELD(storeAttr);
	WRITE_INT_FIELD(partno);
	WRITE_LONG_FIELD(rrand);
	WRITE_NODE_FIELD(colencs);
	WRITE_INT_FIELD(location);
}

static void
_outPartitionRangeItem(StringInfo str, PartitionRangeItem *node)
{
	WRITE_NODE_TYPE("PARTITIONRANGEITEM");
	WRITE_NODE_FIELD(partRangeVal);
	WRITE_ENUM_FIELD(partedge, PartitionEdgeBounding);
	WRITE_INT_FIELD(location);
}

static void
_outPartitionBoundSpec(StringInfo str, PartitionBoundSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONBOUNDSPEC");
	WRITE_NODE_FIELD(partStart);
	WRITE_NODE_FIELD(partEnd);
	WRITE_NODE_FIELD(partEvery);
	WRITE_INT_FIELD(location);
}

static void
_outPartitionValuesSpec(StringInfo str, PartitionValuesSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONVALUESSPEC");
	WRITE_NODE_FIELD(partValues);
	WRITE_INT_FIELD(location);
}

static void
_outPartition(StringInfo str, Partition *node)
{
	WRITE_NODE_TYPE("PARTITION");

	WRITE_OID_FIELD(partid);
	WRITE_OID_FIELD(parrelid);
	WRITE_CHAR_FIELD(parkind);
	WRITE_INT_FIELD(parlevel);
	WRITE_BOOL_FIELD(paristemplate);
	WRITE_BINARY_FIELD(parnatts, sizeof(int2));
	WRITE_INT_ARRAY(paratts, parnatts, int2);
	WRITE_OID_ARRAY(parclass, parnatts);
}

static void
_outPartitionRule(StringInfo str, PartitionRule *node)
{
	WRITE_NODE_TYPE("PARTITIONRULE");

	WRITE_OID_FIELD(parruleid);
	WRITE_OID_FIELD(paroid);
	WRITE_OID_FIELD(parchildrelid);
	WRITE_OID_FIELD(parparentoid);
	WRITE_BOOL_FIELD(parisdefault);
	WRITE_STRING_FIELD(parname);
	WRITE_NODE_FIELD(parrangestart);
	WRITE_BOOL_FIELD(parrangestartincl);
	WRITE_NODE_FIELD(parrangeend);
	WRITE_BOOL_FIELD(parrangeendincl);
	WRITE_NODE_FIELD(parrangeevery);
	WRITE_NODE_FIELD(parlistvalues);
	WRITE_BINARY_FIELD(parruleord, sizeof(int2));
	WRITE_NODE_FIELD(parreloptions);
	WRITE_OID_FIELD(partemplatespaceId);
	WRITE_NODE_FIELD(children);
}

static void
_outPartitionNode(StringInfo str, PartitionNode *node)
{
	WRITE_NODE_TYPE("PARTITIONNODE");

	WRITE_NODE_FIELD(part);
	WRITE_NODE_FIELD(default_part);
	WRITE_NODE_FIELD(rules);
}

static void
_outPgPartRule(StringInfo str, PgPartRule *node)
{
	WRITE_NODE_TYPE("PGPARTRULE");

	WRITE_NODE_FIELD(pNode);
	WRITE_NODE_FIELD(topRule);
	WRITE_STRING_FIELD(partIdStr);
	WRITE_BOOL_FIELD(isName);
	WRITE_INT_FIELD(topRuleRank);
	WRITE_STRING_FIELD(relname);
}

static void
_outSegfileMapNode(StringInfo str, SegfileMapNode *node)
{
	WRITE_NODE_TYPE("SEGFILEMAPNODE");

	WRITE_OID_FIELD(relid);
	WRITE_INT_FIELD(segno);
}

static void
_outExtTableTypeDesc(StringInfo str, ExtTableTypeDesc *node)
{
	WRITE_NODE_TYPE("EXTTABLETYPEDESC");

	WRITE_ENUM_FIELD(exttabletype, ExtTableType);
	WRITE_NODE_FIELD(location_list);
	WRITE_NODE_FIELD(on_clause);
	WRITE_STRING_FIELD(command_string);
}

static void
_outCreateExternalStmt(StringInfo str, CreateExternalStmt *node)
{
	WRITE_NODE_TYPE("CREATEEXTERNALSTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(tableElts);
	WRITE_NODE_FIELD(exttypedesc);
	WRITE_STRING_FIELD(format);
	WRITE_NODE_FIELD(formatOpts);
	WRITE_BOOL_FIELD(isweb);
	WRITE_BOOL_FIELD(iswritable);
	WRITE_NODE_FIELD(sreh);
	WRITE_NODE_FIELD(encoding);
	WRITE_NODE_FIELD(distributedBy);

}

static void
_outCreateForeignStmt(StringInfo str, CreateForeignStmt *node)
{
	WRITE_NODE_TYPE("CREATEFOREIGNSTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(tableElts);
	WRITE_STRING_FIELD(srvname);
	WRITE_NODE_FIELD(options);
}

static void
_outIndexStmt(StringInfo str, IndexStmt *node)
{
	WRITE_NODE_TYPE("INDEXSTMT");

	WRITE_STRING_FIELD(idxname);
	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(accessMethod);
	WRITE_STRING_FIELD(tableSpace);
	WRITE_NODE_FIELD(indexParams);
	WRITE_NODE_FIELD(options);

	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(rangetable);
	WRITE_BOOL_FIELD(is_part_child);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(primary);
	WRITE_BOOL_FIELD(isconstraint);
	WRITE_STRING_FIELD(altconname);
	WRITE_OID_FIELD(constrOid);
	WRITE_BOOL_FIELD(concurrent);
	WRITE_NODE_FIELD(idxOids);
}

static void
_outReindexStmt(StringInfo str, ReindexStmt *node)
{
	WRITE_NODE_TYPE("REINDEXSTMT");

	WRITE_ENUM_FIELD(kind,ObjectType);
	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(name);
	WRITE_BOOL_FIELD(do_system);
	WRITE_BOOL_FIELD(do_user);
	WRITE_NODE_FIELD(new_ind_oids);
	WRITE_OID_FIELD(relid);
}


static void
_outViewStmt(StringInfo str, ViewStmt *node)
{
	WRITE_NODE_TYPE("VIEWSTMT");

	WRITE_NODE_FIELD(view);
	WRITE_NODE_FIELD(aliases);
	WRITE_NODE_FIELD(query);
	WRITE_BOOL_FIELD(replace);
	WRITE_OID_FIELD(relOid);
	WRITE_OID_FIELD(comptypeOid);
	WRITE_OID_FIELD(rewriteOid);
}

static void
_outRuleStmt(StringInfo str, RuleStmt *node)
{
	WRITE_NODE_TYPE("RULESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(rulename);
	WRITE_NODE_FIELD(whereClause);
	WRITE_ENUM_FIELD(event,CmdType);
	WRITE_BOOL_FIELD(instead);
	WRITE_NODE_FIELD(actions);
	WRITE_BOOL_FIELD(replace);
	WRITE_OID_FIELD(ruleOid);
}

static void
_outDropStmt(StringInfo str, DropStmt *node)
{
	WRITE_NODE_TYPE("DROPSTMT");

	WRITE_NODE_FIELD(objects);
	WRITE_ENUM_FIELD(removeType, ObjectType);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
	WRITE_BOOL_FIELD(bAllowPartn);


}

static void
_outDropPropertyStmt(StringInfo str, DropPropertyStmt *node)
{
	WRITE_NODE_TYPE("DROPPROPSTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(property);
	WRITE_ENUM_FIELD(removeType, ObjectType);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);

}

static void
_outDropOwnedStmt(StringInfo str, DropOwnedStmt *node)
{
	WRITE_NODE_TYPE("DROPOWNEDSTMT");

	WRITE_NODE_FIELD(roles);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outReassignOwnedStmt(StringInfo str, ReassignOwnedStmt *node)
{
	WRITE_NODE_TYPE("REASSIGNOWNEDSTMT");

	WRITE_NODE_FIELD(roles);
	WRITE_STRING_FIELD(newrole)

}

static void
_outTruncateStmt(StringInfo str, TruncateStmt *node)
{
	WRITE_NODE_TYPE("TRUNCATESTMT");

	WRITE_NODE_FIELD(relations);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_NODE_FIELD(relids);
	WRITE_NODE_FIELD(new_heap_oids);
	WRITE_NODE_FIELD(new_toast_oids);
	WRITE_NODE_FIELD(new_aoseg_oids);
	WRITE_NODE_FIELD(new_aoblkdir_oids);
	WRITE_NODE_FIELD(new_aovisimap_oids);
	WRITE_NODE_FIELD(new_ind_oids);
}

static void
_outAlterTableStmt(StringInfo str, AlterTableStmt *node)
{
	int m;
	WRITE_NODE_TYPE("ALTERTABLESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(cmds);
	WRITE_ENUM_FIELD(relkind, ObjectType);
	WRITE_NODE_FIELD(oidmap);
	WRITE_INT_FIELD(oidInfoCount);

	for (m = 0; m < node->oidInfoCount; m++)
	{
		WRITE_OID_FIELD(oidInfo[m].relOid);
		WRITE_OID_FIELD(oidInfo[m].comptypeOid);
		WRITE_OID_FIELD(oidInfo[m].toastOid);
		WRITE_OID_FIELD(oidInfo[m].toastIndexOid);
		WRITE_OID_FIELD(oidInfo[m].toastComptypeOid);
		WRITE_OID_FIELD(oidInfo[m].aosegOid);
		WRITE_OID_FIELD(oidInfo[m].aosegIndexOid);
		WRITE_OID_FIELD(oidInfo[m].aosegComptypeOid);
		WRITE_OID_FIELD(oidInfo[m].aovisimapOid);
		WRITE_OID_FIELD(oidInfo[m].aovisimapIndexOid);
		WRITE_OID_FIELD(oidInfo[m].aovisimapComptypeOid);
		WRITE_OID_FIELD(oidInfo[m].aoblkdirOid);
		WRITE_OID_FIELD(oidInfo[m].aoblkdirIndexOid);
		WRITE_OID_FIELD(oidInfo[m].aoblkdirComptypeOid);
	}
}

static void
_outAlterTableCmd(StringInfo str, AlterTableCmd *node)
{
	WRITE_NODE_TYPE("ALTERTABLECMD");

	WRITE_ENUM_FIELD(subtype, AlterTableType);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(def);
	WRITE_NODE_FIELD(transform);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(part_expanded);
	WRITE_NODE_FIELD(partoids);
}

static void
_outInheritPartitionCmd(StringInfo str, InheritPartitionCmd *node)
{
	WRITE_NODE_TYPE("INHERITPARTITION");

	WRITE_NODE_FIELD(parent);
}

static void

_outAlterPartitionCmd(StringInfo str, AlterPartitionCmd *node)
{
	WRITE_NODE_TYPE("ALTERPARTITIONCMD");

	WRITE_NODE_FIELD(partid);
	WRITE_NODE_FIELD(arg1);
	WRITE_NODE_FIELD(arg2);
	WRITE_LIST_FIELD(newOids);
}

static void
_outAlterPartitionId(StringInfo str, AlterPartitionId *node)
{
	WRITE_NODE_TYPE("ALTERPARTITIONID");

	WRITE_ENUM_FIELD(idtype, AlterPartitionIdType);
	WRITE_NODE_FIELD(partiddef);
}

static void
_outCreateRoleStmt(StringInfo str, CreateRoleStmt *node)
{
	WRITE_NODE_TYPE("CREATEROLESTMT");

	WRITE_ENUM_FIELD(stmt_type, RoleStmtType);
	WRITE_STRING_FIELD(role);
	WRITE_NODE_FIELD(options);
	WRITE_OID_FIELD(roleOid);
}

static void
_outDenyLoginInterval(StringInfo str, DenyLoginInterval *node)
{
	WRITE_NODE_TYPE("DENYLOGININTERVAL");

	WRITE_NODE_FIELD(start);
	WRITE_NODE_FIELD(end);
}

static void
_outDenyLoginPoint(StringInfo str, DenyLoginPoint *node)
{
	WRITE_NODE_TYPE("DENYLOGINPOINT");

	WRITE_NODE_FIELD(day);
	WRITE_NODE_FIELD(time);
}

static  void
_outDropRoleStmt(StringInfo str, DropRoleStmt *node)
{
	WRITE_NODE_TYPE("DROPROLESTMT");

	WRITE_NODE_FIELD(roles);
	WRITE_BOOL_FIELD(missing_ok);
}

static  void
_outAlterRoleStmt(StringInfo str, AlterRoleStmt *node)
{
	WRITE_NODE_TYPE("ALTERROLESTMT");

	WRITE_STRING_FIELD(role);
	WRITE_NODE_FIELD(options);
	WRITE_INT_FIELD(action);
}

static  void
_outAlterRoleSetStmt(StringInfo str, AlterRoleSetStmt *node)
{
	WRITE_NODE_TYPE("ALTERROLESETSTMT");

	WRITE_STRING_FIELD(role);
	WRITE_STRING_FIELD(variable);
	WRITE_NODE_FIELD(value);
}


static  void
_outAlterOwnerStmt(StringInfo str, AlterOwnerStmt *node)
{
	WRITE_NODE_TYPE("ALTEROWNERSTMT");

	WRITE_ENUM_FIELD(objectType,ObjectType);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(objarg);
	WRITE_STRING_FIELD(addname);
	WRITE_STRING_FIELD(newowner);

}


static void
_outRenameStmt(StringInfo str, RenameStmt *node)
{
	WRITE_NODE_TYPE("RENAMESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_OID_FIELD(objid);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(objarg);
	WRITE_STRING_FIELD(subname);
	WRITE_STRING_FIELD(newname);
	WRITE_ENUM_FIELD(renameType,ObjectType);
	WRITE_BOOL_FIELD(bAllowPartn);

}

static void
_outAlterObjectSchemaStmt(StringInfo str, AlterObjectSchemaStmt *node)
{
	WRITE_NODE_TYPE("ALTEROBJECTSCHEMASTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(objarg);
	WRITE_STRING_FIELD(addname);
	WRITE_STRING_FIELD(newschema);
	WRITE_ENUM_FIELD(objectType,ObjectType);
}

static void
_outCreateSeqStmt(StringInfo str, CreateSeqStmt *node)
{
	WRITE_NODE_TYPE("CREATESEQSTMT");
	WRITE_NODE_FIELD(sequence);
	WRITE_NODE_FIELD(options);
	WRITE_OID_FIELD(relOid);
	WRITE_OID_FIELD(comptypeOid);
}

static void
_outAlterSeqStmt(StringInfo str, AlterSeqStmt *node)
{
	WRITE_NODE_TYPE("ALTERSEQSTMT");
	WRITE_NODE_FIELD(sequence);
	WRITE_NODE_FIELD(options);
}

static void
_outClusterStmt(StringInfo str, ClusterStmt *node)
{
	WRITE_NODE_TYPE("CLUSTERSTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(indexname);
	WRITE_OID_FIELD(oidInfo.relOid);
	WRITE_OID_FIELD(oidInfo.comptypeOid);
	WRITE_OID_FIELD(oidInfo.toastOid);
	WRITE_OID_FIELD(oidInfo.toastIndexOid);
	WRITE_OID_FIELD(oidInfo.toastComptypeOid);
	WRITE_OID_FIELD(oidInfo.aosegOid);
	WRITE_OID_FIELD(oidInfo.aosegIndexOid);
	WRITE_OID_FIELD(oidInfo.aosegComptypeOid);
	WRITE_OID_FIELD(oidInfo.aovisimapOid);
	WRITE_OID_FIELD(oidInfo.aovisimapIndexOid);
	WRITE_OID_FIELD(oidInfo.aovisimapComptypeOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirIndexOid);
	WRITE_OID_FIELD(oidInfo.aoblkdirComptypeOid);
	WRITE_NODE_FIELD(new_ind_oids);
}

static void
_outCreatedbStmt(StringInfo str, CreatedbStmt *node)
{
	WRITE_NODE_TYPE("CREATEDBSTMT");
	WRITE_STRING_FIELD(dbname);
	WRITE_NODE_FIELD(options);
	WRITE_OID_FIELD(dbOid);
}

static void
_outDropdbStmt(StringInfo str, DropdbStmt *node)
{
	WRITE_NODE_TYPE("DROPDBSTMT");
	WRITE_STRING_FIELD(dbname);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outCreateDomainStmt(StringInfo str, CreateDomainStmt *node)
{
	WRITE_NODE_TYPE("CREATEDOMAINSTMT");
	WRITE_NODE_FIELD(domainname);
	WRITE_NODE_FIELD(typname);
	WRITE_NODE_FIELD(constraints);
	WRITE_OID_FIELD(domainOid);
}

static void
_outAlterDomainStmt(StringInfo str, AlterDomainStmt *node)
{
	WRITE_NODE_TYPE("ALTERDOMAINSTMT");
	WRITE_CHAR_FIELD(subtype);
	WRITE_NODE_FIELD(typname);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(def);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outCreateFdwStmt(StringInfo str, CreateFdwStmt *node)
{
	WRITE_NODE_TYPE("CREATEFDWSTMT");
	WRITE_STRING_FIELD(fdwname);
	WRITE_NODE_FIELD(validator);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterFdwStmt(StringInfo str, AlterFdwStmt *node)
{
	WRITE_NODE_TYPE("ALTERFDWSTMT");
	WRITE_STRING_FIELD(fdwname);
	WRITE_NODE_FIELD(validator);
	WRITE_BOOL_FIELD(change_validator);
	WRITE_NODE_FIELD(options);
}

static void
_outDropFdwStmt(StringInfo str, DropFdwStmt *node)
{
	WRITE_NODE_TYPE("DROPFDWSTMT");
	WRITE_STRING_FIELD(fdwname);
	WRITE_BOOL_FIELD(missing_ok);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outCreateForeignServerStmt(StringInfo str, CreateForeignServerStmt *node)
{
	WRITE_NODE_TYPE("CREATEFOREIGNSERVERSTMT");
	WRITE_STRING_FIELD(servername);
	WRITE_STRING_FIELD(servertype);
	WRITE_STRING_FIELD(version);
	WRITE_STRING_FIELD(fdwname);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterForeignServerStmt(StringInfo str, AlterForeignServerStmt *node)
{
	WRITE_NODE_TYPE("ALTERFOREIGNSERVERSTMT");
	WRITE_STRING_FIELD(servername);
	WRITE_STRING_FIELD(version);
	WRITE_NODE_FIELD(options);
	WRITE_BOOL_FIELD(has_version);
}

static void
_outDropForeignServerStmt(StringInfo str, DropForeignServerStmt *node)
{
	WRITE_NODE_TYPE("DROPFOREIGNSERVERSTMT");
	WRITE_STRING_FIELD(servername);
	WRITE_BOOL_FIELD(missing_ok);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outCreateUserMappingStmt(StringInfo str, CreateUserMappingStmt *node)
{
	WRITE_NODE_TYPE("CREATEUSERMAPPINGSTMT");
	WRITE_STRING_FIELD(username);
	WRITE_STRING_FIELD(servername);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterUserMappingStmt(StringInfo str, AlterUserMappingStmt *node)
{
	WRITE_NODE_TYPE("ALTERUSERMAPPINGSTMT");
	WRITE_STRING_FIELD(username);
	WRITE_STRING_FIELD(servername);
	WRITE_NODE_FIELD(options);
}

static void
_outDropUserMappingStmt(StringInfo str, DropUserMappingStmt *node)
{
	WRITE_NODE_TYPE("DROPUSERMAPPINGSTMT");
	WRITE_STRING_FIELD(username);
	WRITE_STRING_FIELD(servername);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outCreateFunctionStmt(StringInfo str, CreateFunctionStmt *node)
{
	WRITE_NODE_TYPE("CREATEFUNCSTMT");
	WRITE_BOOL_FIELD(replace);
	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(parameters);
	WRITE_NODE_FIELD(returnType);
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(withClause);
	WRITE_OID_FIELD(funcOid);
	WRITE_OID_FIELD(shelltypeOid);
}

static void
_outFunctionParameter(StringInfo str, FunctionParameter *node)
{
	WRITE_NODE_TYPE("FUNCTIONPARAMETER");
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(argType);
	WRITE_ENUM_FIELD(mode, FunctionParameterMode);

}

static void
_outRemoveFuncStmt(StringInfo str, RemoveFuncStmt *node)
{
	WRITE_NODE_TYPE("REMOVEFUNCSTMT");
	WRITE_ENUM_FIELD(kind,ObjectType);
	WRITE_NODE_FIELD(name);
	WRITE_NODE_FIELD(args);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outAlterFunctionStmt(StringInfo str, AlterFunctionStmt *node)
{
	WRITE_NODE_TYPE("ALTERFUNCTIONSTMT");
	WRITE_NODE_FIELD(func);
	WRITE_NODE_FIELD(actions);
}





static void
_outDefineStmt(StringInfo str, DefineStmt *node)
{
	WRITE_NODE_TYPE("DEFINESTMT");
	WRITE_ENUM_FIELD(kind, ObjectType);
	WRITE_BOOL_FIELD(oldstyle);
	WRITE_NODE_FIELD(defnames);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(definition);
	WRITE_OID_FIELD(newOid);
	WRITE_OID_FIELD(shadowOid);
	WRITE_BOOL_FIELD(ordered);  /* CDB */
	WRITE_BOOL_FIELD(trusted);  /* CDB */
}

static void
_outCompositeTypeStmt(StringInfo str, CompositeTypeStmt *node)
{
	WRITE_NODE_TYPE("COMPTYPESTMT");

	WRITE_NODE_FIELD(typevar);
	WRITE_NODE_FIELD(coldeflist);
	WRITE_OID_FIELD(relOid);
	WRITE_OID_FIELD(comptypeOid);

}

static void
_outCreateCastStmt(StringInfo str, CreateCastStmt *node)
{
	WRITE_NODE_TYPE("CREATECAST");
	WRITE_NODE_FIELD(sourcetype);
	WRITE_NODE_FIELD(targettype);
	WRITE_NODE_FIELD(func);
	WRITE_ENUM_FIELD(context, CoercionContext);
	WRITE_OID_FIELD(castOid);
}

static void
_outDropCastStmt(StringInfo str, DropCastStmt *node)
{
	WRITE_NODE_TYPE("DROPCAST");
	WRITE_NODE_FIELD(sourcetype);
	WRITE_NODE_FIELD(targettype);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outCreateOpClassStmt(StringInfo str, CreateOpClassStmt *node)
{
	WRITE_NODE_TYPE("CREATEOPCLASS");
	WRITE_NODE_FIELD(opclassname);
	WRITE_STRING_FIELD(amname);
	WRITE_NODE_FIELD(datatype);
	WRITE_NODE_FIELD(items);
	WRITE_BOOL_FIELD(isDefault);
	WRITE_OID_FIELD(opclassOid);
}

static void
_outCreateOpClassItem(StringInfo str, CreateOpClassItem *node)
{
	WRITE_NODE_TYPE("CREATEOPCLASSITEM");
	WRITE_INT_FIELD(itemtype);
	WRITE_NODE_FIELD(name);
	WRITE_NODE_FIELD(args);
	WRITE_INT_FIELD(number);
	WRITE_BOOL_FIELD(recheck);
	WRITE_NODE_FIELD(storedtype);
}

static void
_outRemoveOpClassStmt(StringInfo str, RemoveOpClassStmt *node)
{
	WRITE_NODE_TYPE("REMOVEOPCLASS");
	WRITE_NODE_FIELD(opclassname);
	WRITE_STRING_FIELD(amname);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outCreateConversionStmt(StringInfo str, CreateConversionStmt *node)
{
	WRITE_NODE_TYPE("CREATECONVERSION");
	WRITE_NODE_FIELD(conversion_name);
	WRITE_STRING_FIELD(for_encoding_name);
	WRITE_STRING_FIELD(to_encoding_name);
	WRITE_NODE_FIELD(func_name);
	WRITE_BOOL_FIELD(def);
	WRITE_OID_FIELD(convOid);
}

static void
_outTransactionStmt(StringInfo str, TransactionStmt *node)
{
	WRITE_NODE_TYPE("TRANSACTIONSTMT");

	WRITE_ENUM_FIELD(kind, TransactionStmtKind);
	WRITE_NODE_FIELD(options);

}

static void
_outNotifyStmt(StringInfo str, NotifyStmt *node)
{
	WRITE_NODE_TYPE("NOTIFY");

	WRITE_NODE_FIELD(relation);
}

static void
_outDeclareCursorStmt(StringInfo str, DeclareCursorStmt *node)
{
	WRITE_NODE_TYPE("DECLARECURSOR");

	WRITE_STRING_FIELD(portalname);
	WRITE_INT_FIELD(options);
	WRITE_NODE_FIELD(query);
	WRITE_BOOL_FIELD(is_simply_updatable);
}

static void
_outSingleRowErrorDesc(StringInfo str, SingleRowErrorDesc *node)
{
	WRITE_NODE_TYPE("SINGLEROWERRORDESC");
	WRITE_NODE_FIELD(errtable);
	WRITE_INT_FIELD(rejectlimit);
	WRITE_BOOL_FIELD(is_keep);
	WRITE_BOOL_FIELD(is_limit_in_rows);
	WRITE_BOOL_FIELD(reusing_existing_errtable);
	WRITE_BOOL_FIELD(into_file);
}

static void
_outCopyStmt(StringInfo str, CopyStmt *node)
{
	WRITE_NODE_TYPE("COPYSTMT");
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(attlist);
	WRITE_BOOL_FIELD(is_from);
	WRITE_BOOL_FIELD(skip_ext_partition);
	WRITE_STRING_FIELD(filename);
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(sreh);
	WRITE_NODE_FIELD(partitions);
	WRITE_NODE_FIELD(ao_segnos);
}


static void
_outGrantStmt(StringInfo str, GrantStmt *node)
{
	WRITE_NODE_TYPE("GRANTSTMT");
	WRITE_BOOL_FIELD(is_grant);
	WRITE_ENUM_FIELD(objtype,GrantObjectType);
	WRITE_NODE_FIELD(objects);
	WRITE_NODE_FIELD(privileges);
	WRITE_NODE_FIELD(grantees);
	WRITE_BOOL_FIELD(grant_option);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_NODE_FIELD(cooked_privs);
}

static void
_outPrivGrantee(StringInfo str, PrivGrantee *node)
{
	WRITE_NODE_TYPE("PRIVGRANTEE");
	WRITE_STRING_FIELD(rolname);
}

static void
_outFuncWithArgs(StringInfo str, FuncWithArgs *node)
{
	WRITE_NODE_TYPE("FUNCWITHARGS");
	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(funcargs);
}

static void
_outGrantRoleStmt(StringInfo str, GrantRoleStmt *node)
{
	WRITE_NODE_TYPE("GRANTROLESTMT");
	WRITE_NODE_FIELD(granted_roles);
	WRITE_NODE_FIELD(grantee_roles);
	WRITE_BOOL_FIELD(is_grant);
	WRITE_BOOL_FIELD(admin_opt);
	WRITE_STRING_FIELD(grantor);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outLockStmt(StringInfo str, LockStmt *node)
{
	WRITE_NODE_TYPE("LOCKSTMT");
	WRITE_NODE_FIELD(relations);
	WRITE_INT_FIELD(mode);
	WRITE_BOOL_FIELD(nowait);
}

static void
_outConstraintsSetStmt(StringInfo str, ConstraintsSetStmt *node)
{
	WRITE_NODE_TYPE("CONSTRAINTSSETSTMT");
	WRITE_NODE_FIELD(constraints);
	WRITE_BOOL_FIELD(deferred);
}

static void
_outFuncCall(StringInfo str, FuncCall *node)
{
	WRITE_NODE_TYPE("FUNCCALL");

	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
    WRITE_NODE_FIELD(agg_order);
	WRITE_BOOL_FIELD(agg_star);
	WRITE_BOOL_FIELD(agg_distinct);
	WRITE_BOOL_FIELD(func_variadic);
	WRITE_NODE_FIELD(over);
	WRITE_INT_FIELD(location);
	WRITE_NODE_FIELD(agg_filter);
}

static void
_outDefElem(StringInfo str, DefElem *node)
{
	WRITE_NODE_TYPE("DEFELEM");

	WRITE_STRING_FIELD(defname);
	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(defaction, DefElemAction);
}

static void
_outLockingClause(StringInfo str, LockingClause *node)
{
	WRITE_NODE_TYPE("LOCKINGCLAUSE");

	WRITE_NODE_FIELD(lockedRels);
	WRITE_BOOL_FIELD(forUpdate);
	WRITE_BOOL_FIELD(noWait);
}

static void
_outDMLActionExpr(StringInfo str, DMLActionExpr *node)
{
	WRITE_NODE_TYPE("DMLACTIONEXPR");
}

static void
_outPartOidExpr(StringInfo str, PartOidExpr *node)
{
	WRITE_NODE_TYPE("PARTOIDEXPR");

	WRITE_INT_FIELD(level);
}

static void
_outPartDefaultExpr(StringInfo str, PartDefaultExpr *node)
{
	WRITE_NODE_TYPE("PARTDEFAULTEXPR");

	WRITE_INT_FIELD(level);
}

static void
_outPartBoundExpr(StringInfo str, PartBoundExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDEXPR");

	WRITE_INT_FIELD(level);
	WRITE_OID_FIELD(boundType);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartBoundInclusionExpr(StringInfo str, PartBoundInclusionExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDOPENEXPR");

	WRITE_INT_FIELD(level);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartBoundOpenExpr(StringInfo str, PartBoundOpenExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDINCLUSIONEXPR");

	WRITE_INT_FIELD(level);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outColumnDef(StringInfo str, ColumnDef *node)
{
	WRITE_NODE_TYPE("COLUMNDEF");

	WRITE_STRING_FIELD(colname);
	WRITE_NODE_FIELD(typname);
	WRITE_INT_FIELD(inhcount);
	WRITE_BOOL_FIELD(is_local);
	WRITE_BOOL_FIELD(is_not_null);
	WRITE_INT_FIELD(attnum);
	WRITE_OID_FIELD(default_oid);
	WRITE_NODE_FIELD(raw_default);
	WRITE_BOOL_FIELD(default_is_null);
	WRITE_STRING_FIELD(cooked_default);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(encoding);
}

static void
_outTypeName(StringInfo str, TypeName *node)
{
	WRITE_NODE_TYPE("TYPENAME");

	WRITE_NODE_FIELD(names);
	WRITE_OID_FIELD(typid);
	WRITE_BOOL_FIELD(timezone);
	WRITE_BOOL_FIELD(setof);
	WRITE_BOOL_FIELD(pct_type);
	WRITE_INT_FIELD(typmod);
	WRITE_NODE_FIELD(arrayBounds);
	WRITE_INT_FIELD(location);
}

static void
_outTypeCast(StringInfo str, TypeCast *node)
{
	WRITE_NODE_TYPE("TYPECAST");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(typname);
}

static void
_outIndexElem(StringInfo str, IndexElem *node)
{
	WRITE_NODE_TYPE("INDEXELEM");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(opclass);
}

static void
_outVariableResetStmt(StringInfo str, VariableResetStmt *node)
{
	WRITE_NODE_TYPE("VARIABLERESETSTMT");
	WRITE_STRING_FIELD(name);
}

static void
_outQuery(StringInfo str, Query *node)
{
	WRITE_NODE_TYPE("QUERY");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(querySource, QuerySource);
	WRITE_BOOL_FIELD(canSetTag);

	WRITE_NODE_FIELD(utilityStmt);
	WRITE_INT_FIELD(resultRelation);
	WRITE_NODE_FIELD(intoClause);
	WRITE_BOOL_FIELD(hasAggs);
	WRITE_BOOL_FIELD(hasWindFuncs);
	WRITE_BOOL_FIELD(hasSubLinks);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(jointree);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(havingQual);
	WRITE_NODE_FIELD(windowClause);
	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(scatterClause);
	WRITE_NODE_FIELD(cteList);
	WRITE_BOOL_FIELD(hasRecursive);
	WRITE_BOOL_FIELD(hasModifyingCTE);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(setOperations);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(result_partitions);
	WRITE_NODE_FIELD(result_aosegnos);
	WRITE_NODE_FIELD(returningLists);
	/* Don't serialize policy */
}

static void
_outSortClause(StringInfo str, SortClause *node)
{
	WRITE_NODE_TYPE("SORTCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
}

static void
_outGroupClause(StringInfo str, GroupClause *node)
{
	WRITE_NODE_TYPE("GROUPCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
}

static void
_outGroupingClause(StringInfo str, GroupingClause *node)
{
	WRITE_NODE_TYPE("GROUPINGCLAUSE");

	WRITE_ENUM_FIELD(groupType, GroupingType);
	WRITE_NODE_FIELD(groupsets);
}

static void
_outGroupingFunc(StringInfo str, GroupingFunc *node)
{
	WRITE_NODE_TYPE("GROUPINGFUNC");

	WRITE_NODE_FIELD(args);
	WRITE_INT_FIELD(ngrpcols);
}

static void
_outGrouping(StringInfo str, Grouping *node)
{
	WRITE_NODE_TYPE("GROUPING");
}

static void
_outGroupId(StringInfo str, GroupId *node)
{
	WRITE_NODE_TYPE("GROUPID");
}

static void
_outWindowSpecParse(StringInfo str, WindowSpecParse *node)
{
	WRITE_NODE_TYPE("WINDOWSPECPARSE");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(elems);
}

static void
_outWindowSpec(StringInfo str, WindowSpec *node)
{
	WRITE_NODE_TYPE("WINDOWSPEC");

	WRITE_STRING_FIELD(name);
	WRITE_STRING_FIELD(parent);
	WRITE_NODE_FIELD(partition);
	WRITE_NODE_FIELD(order);
	WRITE_NODE_FIELD(frame);
    WRITE_INT_FIELD(location);
}

static void
_outWindowFrame(StringInfo str, WindowFrame *node)
{
	WRITE_NODE_TYPE("WINDOWFRAME");

	WRITE_BOOL_FIELD(is_rows);
	WRITE_BOOL_FIELD(is_between);
	WRITE_NODE_FIELD(trail);
	WRITE_NODE_FIELD(lead);
	WRITE_ENUM_FIELD(exclude, WindowExclusion);
}

static void
_outWindowFrameEdge(StringInfo str, WindowFrameEdge *node)
{
	WRITE_NODE_TYPE("WINDOWFRAMEEDGE");

	WRITE_ENUM_FIELD(kind, WindowBoundingKind);
	WRITE_NODE_FIELD(val);
}

static void
_outPercentileExpr(StringInfo str, PercentileExpr *node)
{
	WRITE_NODE_TYPE("PERCENTILEEXPR");

	WRITE_OID_FIELD(perctype);
	WRITE_NODE_FIELD(args);
	WRITE_ENUM_FIELD(perckind, PercKind);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(sortTargets);
	WRITE_NODE_FIELD(pcExpr);
	WRITE_NODE_FIELD(tcExpr);
	WRITE_INT_FIELD(location);
}

static void
_outRowMarkClause(StringInfo str, RowMarkClause *node)
{
	WRITE_NODE_TYPE("ROWMARKCLAUSE");

	WRITE_UINT_FIELD(rti);
	WRITE_BOOL_FIELD(forUpdate);
	WRITE_BOOL_FIELD(noWait);
}

static void
_outWithClause(StringInfo str, WithClause *node)
{
	WRITE_NODE_TYPE("WITHCLAUSE");
	
	WRITE_NODE_FIELD(ctes);
	WRITE_BOOL_FIELD(recursive);
	WRITE_INT_FIELD(location);
}

static void
_outCommonTableExpr(StringInfo str, CommonTableExpr *node)
{
	WRITE_NODE_TYPE("COMMONTABLEEXPR");
	
    WRITE_STRING_FIELD(ctename);
	WRITE_NODE_FIELD(aliascolnames);
	WRITE_NODE_FIELD(ctequery);
	WRITE_INT_FIELD(location);
	WRITE_BOOL_FIELD(cterecursive);
	WRITE_INT_FIELD(cterefcount);
	WRITE_NODE_FIELD(ctecolnames);
	WRITE_NODE_FIELD(ctecoltypes);
	WRITE_NODE_FIELD(ctecoltypmods);
}

static void
_outSetOperationStmt(StringInfo str, SetOperationStmt *node)
{
	WRITE_NODE_TYPE("SETOPERATIONSTMT");

	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(colTypes);
	WRITE_NODE_FIELD(colTypmods);
}

static void
_outRangeTblEntry(StringInfo str, RangeTblEntry *node)
{
	WRITE_NODE_TYPE("RTE");

	/* put alias + eref first to make dump more legible */
	WRITE_NODE_FIELD(alias);
	WRITE_NODE_FIELD(eref);
	WRITE_ENUM_FIELD(rtekind, RTEKind);

	switch (node->rtekind)
	{
		case RTE_RELATION:
		case RTE_SPECIAL:
			WRITE_OID_FIELD(relid);
			break;
		case RTE_SUBQUERY:
			WRITE_NODE_FIELD(subquery);
			break;
		case RTE_CTE:
			WRITE_STRING_FIELD(ctename);
			WRITE_INT_FIELD(ctelevelsup);
			WRITE_BOOL_FIELD(self_reference);
			WRITE_NODE_FIELD(ctecoltypes);
			WRITE_NODE_FIELD(ctecoltypmods);
			break;
		case RTE_FUNCTION:
			WRITE_NODE_FIELD(funcexpr);
			WRITE_NODE_FIELD(funccoltypes);
			WRITE_NODE_FIELD(funccoltypmods);
			break;
		case RTE_TABLEFUNCTION:
			WRITE_NODE_FIELD(subquery);
			WRITE_NODE_FIELD(funcexpr);
			WRITE_NODE_FIELD(funccoltypes);
			WRITE_NODE_FIELD(funccoltypmods);
			WRITE_BYTEA_FIELD(funcuserdata);
			break;
		case RTE_VALUES:
			WRITE_NODE_FIELD(values_lists);
			break;
		case RTE_JOIN:
			WRITE_ENUM_FIELD(jointype, JoinType);
			WRITE_NODE_FIELD(joinaliasvars);
			break;
        case RTE_VOID:                                                  /*CDB*/
            break;
		default:
			elog(ERROR, "unrecognized RTE kind: %d", (int) node->rtekind);
			break;
	}

	WRITE_BOOL_FIELD(inh);
	WRITE_BOOL_FIELD(inFromCl);
	WRITE_UINT_FIELD(requiredPerms);
	WRITE_OID_FIELD(checkAsUser);

	WRITE_BOOL_FIELD(forceDistRandom);
}

static void
_outAExpr(StringInfo str, A_Expr *node)
{
	WRITE_NODE_TYPE("AEXPR");
	WRITE_ENUM_FIELD(kind, A_Expr_Kind);

	switch (node->kind)
	{
		case AEXPR_OP:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_AND:

			break;
		case AEXPR_OR:

			break;
		case AEXPR_NOT:

			break;
		case AEXPR_OP_ANY:

			WRITE_NODE_FIELD(name);

			break;
		case AEXPR_OP_ALL:

			WRITE_NODE_FIELD(name);

			break;
		case AEXPR_DISTINCT:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NULLIF:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_OF:

			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_IN:

			WRITE_NODE_FIELD(name);
			break;
		default:

			break;
	}

	WRITE_NODE_FIELD(lexpr);
	WRITE_NODE_FIELD(rexpr);
	WRITE_INT_FIELD(location);
}

static void
_outValue(StringInfo str, Value *value)
{

	int16 vt = value->type;
	appendBinaryStringInfo(str, (const char *)&vt, sizeof(int16));
	switch (value->type)
	{
		case T_Integer:
			appendBinaryStringInfo(str, (const char *)&value->val.ival, sizeof(long));
			break;
		case T_Float:
		case T_String:
		case T_BitString:
			{
				int slen = (value->val.str != NULL ? strlen(value->val.str) : 0);
				appendBinaryStringInfo(str, (const char *)&slen, sizeof(int));
				if (slen > 0)
					appendBinaryStringInfo(str, value->val.str, slen);
			}
			break;
		case T_Null:
			/* nothing to do */
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) value->type);
			break;
	}
}

static void
_outColumnRef(StringInfo str, ColumnRef *node)
{
	WRITE_NODE_TYPE("COLUMNREF");

	WRITE_NODE_FIELD(fields);
	WRITE_INT_FIELD(location);
}

static void
_outParamRef(StringInfo str, ParamRef *node)
{
	WRITE_NODE_TYPE("PARAMREF");

	WRITE_INT_FIELD(number);
	WRITE_INT_FIELD(location);  /*CDB*/
}

static void
_outAConst(StringInfo str, A_Const *node)
{
	WRITE_NODE_TYPE("A_CONST");

	_outValue(str, &(node->val));
	WRITE_NODE_FIELD(typname);
	WRITE_INT_FIELD(location);  /*CDB*/

}

static void
_outA_Indices(StringInfo str, A_Indices *node)
{
	WRITE_NODE_TYPE("A_INDICES");

	WRITE_NODE_FIELD(lidx);
	WRITE_NODE_FIELD(uidx);
}

static void
_outA_Indirection(StringInfo str, A_Indirection *node)
{
	WRITE_NODE_TYPE("A_INDIRECTION");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(indirection);
}

static void
_outResTarget(StringInfo str, ResTarget *node)
{
	WRITE_NODE_TYPE("RESTARGET");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(indirection);
	WRITE_NODE_FIELD(val);
	WRITE_INT_FIELD(location);
}

static void
_outConstraint(StringInfo str, Constraint *node)
{
	WRITE_NODE_TYPE("CONSTRAINT");

	WRITE_STRING_FIELD(name);
	WRITE_OID_FIELD(conoid);

	WRITE_ENUM_FIELD(contype,ConstrType);

	switch (node->contype)
	{
		case CONSTR_PRIMARY:
		case CONSTR_UNIQUE:
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexspace);
			break;

		case CONSTR_CHECK:
		case CONSTR_DEFAULT:
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_NOTNULL:
		case CONSTR_NULL:
		case CONSTR_ATTR_DEFERRABLE:
		case CONSTR_ATTR_NOT_DEFERRABLE:
		case CONSTR_ATTR_DEFERRED:
		case CONSTR_ATTR_IMMEDIATE:
			break;

		default:
			elog(WARNING,"serialization doesn't know what to do with this constraint");
			break;
	}
}

static void
_outFkConstraint(StringInfo str, FkConstraint *node)
{
	WRITE_NODE_TYPE("FKCONSTRAINT");

	WRITE_STRING_FIELD(constr_name);
	WRITE_OID_FIELD(constrOid);
	WRITE_NODE_FIELD(pktable);
	WRITE_NODE_FIELD(fk_attrs);
	WRITE_NODE_FIELD(pk_attrs);
	WRITE_CHAR_FIELD(fk_matchtype);
	WRITE_CHAR_FIELD(fk_upd_action);
	WRITE_CHAR_FIELD(fk_del_action);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_BOOL_FIELD(skip_validation);
	WRITE_OID_FIELD(trig1Oid);
	WRITE_OID_FIELD(trig2Oid);
	WRITE_OID_FIELD(trig3Oid);
	WRITE_OID_FIELD(trig4Oid);
}

static void
_outCreateSchemaStmt(StringInfo str, CreateSchemaStmt *node)
{
	WRITE_NODE_TYPE("CREATESCHEMASTMT");

	WRITE_STRING_FIELD(schemaname);
	WRITE_STRING_FIELD(authid);
	WRITE_BOOL_FIELD(istemp);
	WRITE_OID_FIELD(schemaOid);

}

static void
_outCreatePLangStmt(StringInfo str, CreatePLangStmt *node)
{
	WRITE_NODE_TYPE("CREATEPLANGSTMT");

	WRITE_STRING_FIELD(plname);
	WRITE_NODE_FIELD(plhandler);
	WRITE_NODE_FIELD(plvalidator);
	WRITE_BOOL_FIELD(pltrusted);
	WRITE_OID_FIELD(plangOid);
	WRITE_OID_FIELD(plhandlerOid);
	WRITE_OID_FIELD(plvalidatorOid);

}

static void
_outDropPLangStmt(StringInfo str, DropPLangStmt *node)
{
	WRITE_NODE_TYPE("DROPPLANGSTMT");

	WRITE_STRING_FIELD(plname);
	WRITE_ENUM_FIELD(behavior,DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);

}

static void
_outVacuumStmt(StringInfo str, VacuumStmt *node)
{
	WRITE_NODE_TYPE("VACUUMSTMT");

	WRITE_BOOL_FIELD(vacuum);
	WRITE_BOOL_FIELD(full);
	WRITE_BOOL_FIELD(analyze);
	WRITE_BOOL_FIELD(verbose);
	WRITE_BOOL_FIELD(rootonly);
	WRITE_INT_FIELD(freeze_min_age);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(va_cols);
	WRITE_NODE_FIELD(expanded_relids);
	WRITE_NODE_FIELD(extra_oids);
	WRITE_NODE_FIELD(appendonly_compaction_segno);
	WRITE_NODE_FIELD(appendonly_compaction_insert_segno);
	WRITE_BOOL_FIELD(appendonly_compaction_vacuum_cleanup);
	WRITE_BOOL_FIELD(appendonly_compaction_vacuum_prepare);
	WRITE_BOOL_FIELD(heap_truncate);
}


static void
_outCdbProcess(StringInfo str, CdbProcess *node)
{
	WRITE_NODE_TYPE("CDBPROCESS");
	WRITE_STRING_FIELD(listenerAddr);
	WRITE_INT_FIELD(listenerPort);
	WRITE_INT_FIELD(pid);
	WRITE_INT_FIELD(contentid);
}

static void
_outSlice(StringInfo str, Slice *node)
{
	WRITE_NODE_TYPE("SLICE");
	WRITE_INT_FIELD(sliceIndex);
	WRITE_INT_FIELD(rootIndex);
	WRITE_ENUM_FIELD(gangType,GangType);
	WRITE_INT_FIELD(gangSize);
	WRITE_INT_FIELD(numGangMembersToBeActive);
	WRITE_BOOL_FIELD(directDispatch.isDirectDispatch);
	WRITE_NODE_FIELD(directDispatch.contentIds); /* List of int */
	WRITE_DUMMY_FIELD(primaryGang);
	WRITE_INT_FIELD(primary_gang_id);
	WRITE_INT_FIELD(parentIndex); /* List of int index */
	WRITE_NODE_FIELD(children); /* List of int index */
	WRITE_NODE_FIELD(primaryProcesses); /* List of (CDBProcess *) */
}

static void
_outSliceTable(StringInfo str, SliceTable *node)
{
	WRITE_NODE_TYPE("SLICETABLE");
	WRITE_INT_FIELD(nMotions);
	WRITE_INT_FIELD(nInitPlans);
	WRITE_INT_FIELD(localSlice);
	WRITE_NODE_FIELD(slices); /* List of int */
    WRITE_BOOL_FIELD(doInstrument);
	WRITE_INT_FIELD(ic_instance_id);
}


static void
_outCreateTrigStmt(StringInfo str, CreateTrigStmt *node)
{
	WRITE_NODE_TYPE("CREATETRIGSTMT");

	WRITE_STRING_FIELD(trigname);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(before);
	WRITE_BOOL_FIELD(row);
	WRITE_STRING_FIELD(actions);
	WRITE_BOOL_FIELD(isconstraint);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_NODE_FIELD(constrrel);
	WRITE_OID_FIELD(trigOid);

}

static void
_outCreateFileSpaceStmt(StringInfo str, CreateFileSpaceStmt *node)
{
	WRITE_NODE_TYPE("CREATEFILESPACESTMT");

	WRITE_STRING_FIELD(filespacename);
	WRITE_STRING_FIELD(owner);
	WRITE_NODE_FIELD(locations);
	WRITE_OID_FIELD(fsoid);
}

static void
_outFileSpaceEntry(StringInfo str, FileSpaceEntry *node)
{
	WRITE_NODE_TYPE("FILESPACEENTRY");

	WRITE_INT_FIELD(dbid);
	WRITE_INT_FIELD(contentid);
	WRITE_STRING_FIELD(location);
	WRITE_STRING_FIELD(hostname);
}

static void
_outCreateTableSpaceStmt(StringInfo str, CreateTableSpaceStmt *node)
{
	WRITE_NODE_TYPE("CREATETABLESPACESTMT");

	WRITE_STRING_FIELD(tablespacename);
	WRITE_STRING_FIELD(owner);
	WRITE_STRING_FIELD(filespacename);
	WRITE_OID_FIELD(tsoid);
}

static void
_outCreateQueueStmt(StringInfo str, CreateQueueStmt *node)
{
	WRITE_NODE_TYPE("CREATEQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
	WRITE_OID_FIELD(queueOid); 
	WRITE_NODE_FIELD(optids); /* List of oids for nodes */
}

static void
_outAlterQueueStmt(StringInfo str, AlterQueueStmt *node)
{
	WRITE_NODE_TYPE("ALTERQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
	WRITE_NODE_FIELD(optids); /* List of oids for nodes */
}

static void
_outDropQueueStmt(StringInfo str, DropQueueStmt *node)
{
	WRITE_NODE_TYPE("DROPQUEUESTMT");

	WRITE_STRING_FIELD(queue);
}

static void
_outCommentStmt(StringInfo str, CommentStmt *node)
{
        WRITE_NODE_TYPE("COMMENTSTMT");

        WRITE_ENUM_FIELD(objtype, ObjectType);
        WRITE_NODE_FIELD(objname);
        WRITE_NODE_FIELD(objargs);
        WRITE_STRING_FIELD(comment);
}


static void
_outTableValueExpr(StringInfo str, TableValueExpr *node)
{
	WRITE_NODE_TYPE("TABLEVALUEEXPR");

	WRITE_NODE_FIELD(subquery);
}

static void
_outAlterTypeStmt(StringInfo str, AlterTypeStmt *node)
{
	WRITE_NODE_TYPE("ALTERTYPESTMT");

	WRITE_NODE_FIELD(typname);
	WRITE_NODE_FIELD(encoding);
}

static void
_outTupleDescNode(StringInfo str, TupleDescNode *node)
{
	Assert(node->tuple->tdtypeid == RECORDOID);

	WRITE_NODE_TYPE("TUPLEDESCNODE");
	WRITE_INT_FIELD(natts);
	WRITE_INT_FIELD(tuple->natts);

	int i = 0;
	for (; i < node->tuple->natts; i++)
		appendBinaryStringInfo(str, node->tuple->attrs[i], ATTRIBUTE_FIXED_PART_SIZE);

	Assert(node->tuple->constr == NULL);

	WRITE_OID_FIELD(tuple->tdtypeid);
	WRITE_INT_FIELD(tuple->tdtypmod);
	WRITE_INT_FIELD(tuple->tdqdtypmod);
	WRITE_BOOL_FIELD(tuple->tdhasoid);
	WRITE_INT_FIELD(tuple->tdrefcount);
}

/*
 * _outNode -
 *	  converts a Node into binary string and append it to 'str'
 */
static void
_outNode(StringInfo str, void *obj)
{
	if (obj == NULL)
	{
		int16 tg = 0;
		appendBinaryStringInfo(str, (const char *)&tg, sizeof(int16));
		return;
	}
	else if (IsA(obj, List) ||IsA(obj, IntList) || IsA(obj, OidList))
		_outList(str, obj);
	else if (IsA(obj, Integer) ||
			 IsA(obj, Float) ||
			 IsA(obj, String) ||
			 IsA(obj, Null) ||
			 IsA(obj, BitString))
	{
		_outValue(str, obj);
	}
	else
	{

		switch (nodeTag(obj))
		{
			case T_PlannedStmt:
				_outPlannedStmt(str,obj);
				break;
			case T_Plan:
				_outPlan(str, obj);
				break;
			case T_Result:
				_outResult(str, obj);
				break;
			case T_Repeat:
				_outRepeat(str, obj);
				break;
			case T_Append:
				_outAppend(str, obj);
				break;
			case T_Sequence:
				_outSequence(str, obj);
				break;
			case T_BitmapAnd:
				_outBitmapAnd(str, obj);
				break;
			case T_BitmapOr:
				_outBitmapOr(str, obj);
				break;
			case T_Scan:
				_outScan(str, obj);
				break;
			case T_SeqScan:
				_outSeqScan(str, obj);
				break;
			case T_AppendOnlyScan:
				_outAppendOnlyScan(str, obj);
				break;
			case T_AOCSScan:
				_outAOCSScan(str, obj);
				break;
			case T_TableScan:
				_outTableScan(str, obj);
				break;
			case T_DynamicTableScan:
				_outDynamicTableScan(str, obj);
				break;
			case T_ExternalScan:
				_outExternalScan(str, obj);
				break;
			case T_IndexScan:
				_outIndexScan(str, obj);
				break;
			case T_DynamicIndexScan:
				_outDynamicIndexScan(str, obj);
				break;
			case T_BitmapIndexScan:
				_outBitmapIndexScan(str, obj);
				break;
			case T_BitmapHeapScan:
				_outBitmapHeapScan(str, obj);
				break;
			case T_BitmapAppendOnlyScan:
				_outBitmapAppendOnlyScan(str, obj);
				break;
			case T_BitmapTableScan:
				_outBitmapTableScan(str, obj);
				break;
			case T_TidScan:
				_outTidScan(str, obj);
				break;
			case T_SubqueryScan:
				_outSubqueryScan(str, obj);
				break;
			case T_FunctionScan:
				_outFunctionScan(str, obj);
				break;
			case T_ValuesScan:
				_outValuesScan(str, obj);
				break;
			case T_Join:
				_outJoin(str, obj);
				break;
			case T_NestLoop:
				_outNestLoop(str, obj);
				break;
			case T_MergeJoin:
				_outMergeJoin(str, obj);
				break;
			case T_HashJoin:
				_outHashJoin(str, obj);
				break;
			case T_Agg:
				_outAgg(str, obj);
				break;
			case T_WindowKey:
				_outWindowKey(str, obj);
				break;
			case T_Window:
				_outWindow(str, obj);
				break;
			case T_TableFunctionScan:
				_outTableFunctionScan(str, obj);
				break;
			case T_Material:
				_outMaterial(str, obj);
				break;
			case T_ShareInputScan:
				_outShareInputScan(str, obj);
				break;
			case T_Sort:
				_outSort(str, obj);
				break;
			case T_Unique:
				_outUnique(str, obj);
				break;
			case T_SetOp:
				_outSetOp(str, obj);
				break;
			case T_Limit:
				_outLimit(str, obj);
				break;
			case T_Hash:
				_outHash(str, obj);
				break;
			case T_Motion:
				_outMotion(str, obj);
				break;
			case T_DML:
				_outDML(str, obj);
				break;
			case T_SplitUpdate:
				_outSplitUpdate(str, obj);
				break;
			case T_RowTrigger:
				_outRowTrigger(str, obj);
				break;
			case T_AssertOp:
				_outAssertOp(str, obj);
				break;
			case T_PartitionSelector:
				_outPartitionSelector(str, obj);
				break;
			case T_Alias:
				_outAlias(str, obj);
				break;
			case T_RangeVar:
				_outRangeVar(str, obj);
				break;
			case T_IntoClause:
				_outIntoClause(str, obj);
				break;
			case T_Var:
				_outVar(str, obj);
				break;
			case T_Const:
				_outConst(str, obj);
				break;
			case T_Param:
				_outParam(str, obj);
				break;
			case T_Aggref:
				_outAggref(str, obj);
				break;
			case T_AggOrder:
				_outAggOrder(str, obj);
				break;
			case T_WindowRef:
				_outWindowRef(str, obj);
				break;
			case T_ArrayRef:
				_outArrayRef(str, obj);
				break;
			case T_FuncExpr:
				_outFuncExpr(str, obj);
				break;
			case T_OpExpr:
				_outOpExpr(str, obj);
				break;
			case T_DistinctExpr:
				_outDistinctExpr(str, obj);
				break;
			case T_ScalarArrayOpExpr:
				_outScalarArrayOpExpr(str, obj);
				break;
			case T_BoolExpr:
				_outBoolExpr(str, obj);
				break;
			case T_SubLink:
				_outSubLink(str, obj);
				break;
			case T_SubPlan:
				_outSubPlan(str, obj);
				break;
			case T_FieldSelect:
				_outFieldSelect(str, obj);
				break;
			case T_FieldStore:
				_outFieldStore(str, obj);
				break;
			case T_RelabelType:
				_outRelabelType(str, obj);
				break;
			case T_ConvertRowtypeExpr:
				_outConvertRowtypeExpr(str, obj);
				break;
			case T_CaseExpr:
				_outCaseExpr(str, obj);
				break;
			case T_CaseWhen:
				_outCaseWhen(str, obj);
				break;
			case T_CaseTestExpr:
				_outCaseTestExpr(str, obj);
				break;
			case T_ArrayExpr:
				_outArrayExpr(str, obj);
				break;
			case T_RowExpr:
				_outRowExpr(str, obj);
				break;
			case T_RowCompareExpr:
				_outRowCompareExpr(str, obj);
				break;
			case T_CoalesceExpr:
				_outCoalesceExpr(str, obj);
				break;
			case T_MinMaxExpr:
				_outMinMaxExpr(str, obj);
				break;
			case T_NullIfExpr:
				_outNullIfExpr(str, obj);
				break;
			case T_NullTest:
				_outNullTest(str, obj);
				break;
			case T_BooleanTest:
				_outBooleanTest(str, obj);
				break;
			case T_CoerceToDomain:
				_outCoerceToDomain(str, obj);
				break;
			case T_CoerceToDomainValue:
				_outCoerceToDomainValue(str, obj);
				break;
			case T_SetToDefault:
				_outSetToDefault(str, obj);
				break;
			case T_CurrentOfExpr:
				_outCurrentOfExpr(str, obj);
				break;
			case T_TargetEntry:
				_outTargetEntry(str, obj);
				break;
			case T_RangeTblRef:
				_outRangeTblRef(str, obj);
				break;
			case T_JoinExpr:
				_outJoinExpr(str, obj);
				break;
			case T_FromExpr:
				_outFromExpr(str, obj);
				break;
			case T_Flow:
				_outFlow(str, obj);
				break;

			case T_Path:
				_outPath(str, obj);
				break;
			case T_IndexPath:
				_outIndexPath(str, obj);
				break;
			case T_BitmapHeapPath:
				_outBitmapHeapPath(str, obj);
				break;
			case T_BitmapAppendOnlyPath:
				_outBitmapAppendOnlyPath(str, obj);
				break;
			case T_BitmapAndPath:
				_outBitmapAndPath(str, obj);
				break;
			case T_BitmapOrPath:
				_outBitmapOrPath(str, obj);
				break;
			case T_TidPath:
				_outTidPath(str, obj);
				break;
			case T_AppendPath:
				_outAppendPath(str, obj);
				break;
			case T_AppendOnlyPath:
				_outAppendOnlyPath(str, obj);
				break;
			case T_AOCSPath:
				_outAOCSPath(str, obj);
				break;
			case T_ResultPath:
				_outResultPath(str, obj);
				break;
			case T_MaterialPath:
				_outMaterialPath(str, obj);
				break;
			case T_UniquePath:
				_outUniquePath(str, obj);
				break;
			case T_NestPath:
				_outNestPath(str, obj);
				break;
			case T_MergePath:
				_outMergePath(str, obj);
				break;
			case T_HashPath:
				_outHashPath(str, obj);
				break;
            case T_CdbMotionPath:
                _outCdbMotionPath(str, obj);
                break;
			case T_PlannerInfo:
				_outPlannerInfo(str, obj);
				break;
			case T_RelOptInfo:
				_outRelOptInfo(str, obj);
				break;
			case T_IndexOptInfo:
				_outIndexOptInfo(str, obj);
				break;
			case T_CdbRelDedupInfo:
				_outCdbRelDedupInfo(str, obj);
				break;
			case T_PathKeyItem:
				_outPathKeyItem(str, obj);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj);
				break;
			case T_InnerIndexscanInfo:
				_outInnerIndexscanInfo(str, obj);
				break;
			case T_OuterJoinInfo:
				_outOuterJoinInfo(str, obj);
				break;
			case T_InClauseInfo:
				_outInClauseInfo(str, obj);
				break;
			case T_AppendRelInfo:
				_outAppendRelInfo(str, obj);
				break;


			case T_GrantStmt:
				_outGrantStmt(str, obj);
				break;
			case T_PrivGrantee:
				_outPrivGrantee(str, obj);
				break;
			case T_FuncWithArgs:
				_outFuncWithArgs(str, obj);
				break;
			case T_GrantRoleStmt:
				_outGrantRoleStmt(str, obj);
				break;
			case T_LockStmt:
				_outLockStmt(str, obj);
				break;

			case T_CreateStmt:
				_outCreateStmt(str, obj);
				break;
			case T_ColumnReferenceStorageDirective:
				_outColumnReferenceStorageDirective(str, obj);
				break;
			case T_PartitionBy:
				_outPartitionBy(str, obj);
				break;
			case T_PartitionElem:
				_outPartitionElem(str, obj);
				break;
			case T_PartitionRangeItem:
				_outPartitionRangeItem(str, obj);
				break;
			case T_PartitionBoundSpec:
				_outPartitionBoundSpec(str, obj);
				break;
			case T_PartitionSpec:
				_outPartitionSpec(str, obj);
				break;
			case T_PartitionValuesSpec:
				_outPartitionValuesSpec(str, obj);
				break;
			case T_Partition:
				_outPartition(str, obj);
				break;
			case T_PartitionRule:
				_outPartitionRule(str, obj);
				break;
			case T_PartitionNode:
				_outPartitionNode(str, obj);
				break;
			case T_PgPartRule:
				_outPgPartRule(str, obj);
				break;

			case T_SegfileMapNode:
				_outSegfileMapNode(str, obj);
				break;

			case T_ExtTableTypeDesc:
				_outExtTableTypeDesc(str, obj);
				break;
            case T_CreateExternalStmt:
				_outCreateExternalStmt(str, obj);
				break;

            case T_CreateForeignStmt:
				_outCreateForeignStmt(str, obj);
				break;

			case T_IndexStmt:
				_outIndexStmt(str, obj);
				break;
			case T_ReindexStmt:
				_outReindexStmt(str, obj);
				break;

			case T_ConstraintsSetStmt:
				_outConstraintsSetStmt(str, obj);
				break;

			case T_CreateFunctionStmt:
				_outCreateFunctionStmt(str, obj);
				break;
			case T_FunctionParameter:
				_outFunctionParameter(str, obj);
				break;
			case T_RemoveFuncStmt:
				_outRemoveFuncStmt(str, obj);
				break;
			case T_AlterFunctionStmt:
				_outAlterFunctionStmt(str, obj);
				break;

			case T_DefineStmt:
				_outDefineStmt(str,obj);
				break;

			case T_CompositeTypeStmt:
				_outCompositeTypeStmt(str,obj);
				break;
			case T_CreateCastStmt:
				_outCreateCastStmt(str,obj);
				break;
			case T_DropCastStmt:
				_outDropCastStmt(str,obj);
				break;
			case T_CreateOpClassStmt:
				_outCreateOpClassStmt(str,obj);
				break;
			case T_CreateOpClassItem:
				_outCreateOpClassItem(str,obj);
				break;
			case T_RemoveOpClassStmt:
				_outRemoveOpClassStmt(str,obj);
				break;
			case T_CreateConversionStmt:
				_outCreateConversionStmt(str,obj);
				break;


			case T_ViewStmt:
				_outViewStmt(str, obj);
				break;
			case T_RuleStmt:
				_outRuleStmt(str, obj);
				break;
			case T_DropStmt:
				_outDropStmt(str, obj);
				break;
			case T_DropPropertyStmt:
				_outDropPropertyStmt(str, obj);
				break;
			case T_DropOwnedStmt:
				_outDropOwnedStmt(str, obj);
				break;
			case T_ReassignOwnedStmt:
				_outReassignOwnedStmt(str, obj);
				break;
			case T_TruncateStmt:
				_outTruncateStmt(str, obj);
				break;
			case T_AlterTableStmt:
				_outAlterTableStmt(str, obj);
				break;
			case T_AlterTableCmd:
				_outAlterTableCmd(str, obj);
				break;
			case T_InheritPartitionCmd:
				_outInheritPartitionCmd(str, obj);
				break;

			case T_AlterPartitionCmd:
				_outAlterPartitionCmd(str, obj);
				break;
			case T_AlterPartitionId:
				_outAlterPartitionId(str, obj);
				break;

			case T_CreateRoleStmt:
				_outCreateRoleStmt(str, obj);
				break;
			case T_DropRoleStmt:
				_outDropRoleStmt(str, obj);
				break;
			case T_AlterRoleStmt:
				_outAlterRoleStmt(str, obj);
				break;
			case T_AlterRoleSetStmt:
				_outAlterRoleSetStmt(str, obj);
				break;

			case T_AlterObjectSchemaStmt:
				_outAlterObjectSchemaStmt(str, obj);
				break;

			case T_AlterOwnerStmt:
				_outAlterOwnerStmt(str, obj);
				break;

			case T_RenameStmt:
				_outRenameStmt(str, obj);
				break;

			case T_CreateSeqStmt:
				_outCreateSeqStmt(str, obj);
				break;
			case T_AlterSeqStmt:
				_outAlterSeqStmt(str, obj);
				break;
			case T_ClusterStmt:
				_outClusterStmt(str, obj);
				break;
			case T_CreatedbStmt:
				_outCreatedbStmt(str, obj);
				break;
			case T_DropdbStmt:
				_outDropdbStmt(str, obj);
				break;
			case T_CreateDomainStmt:
				_outCreateDomainStmt(str, obj);
				break;
			case T_AlterDomainStmt:
				_outAlterDomainStmt(str, obj);
				break;

			case T_CreateFdwStmt:
				_outCreateFdwStmt(str, obj);
				break;
			case T_AlterFdwStmt:
				_outAlterFdwStmt(str, obj);
				break;
			case T_DropFdwStmt:
				_outDropFdwStmt(str, obj);
				break;
			case T_CreateForeignServerStmt:
				_outCreateForeignServerStmt(str, obj);
				break;
			case T_AlterForeignServerStmt:
				_outAlterForeignServerStmt(str, obj);
				break;
			case T_DropForeignServerStmt:
				_outDropForeignServerStmt(str, obj);
				break;
			case T_CreateUserMappingStmt:
				_outCreateUserMappingStmt(str, obj);
				break;
			case T_AlterUserMappingStmt:
				_outAlterUserMappingStmt(str, obj);
				break;
			case T_DropUserMappingStmt:
				_outDropUserMappingStmt(str, obj);
				break;
				
			case T_TransactionStmt:
				_outTransactionStmt(str, obj);
				break;

			case T_NotifyStmt:
				_outNotifyStmt(str, obj);
				break;
			case T_DeclareCursorStmt:
				_outDeclareCursorStmt(str, obj);
				break;
			case T_SingleRowErrorDesc:
				_outSingleRowErrorDesc(str, obj);
				break;
			case T_CopyStmt:
				_outCopyStmt(str, obj);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj);
				break;
			case T_TypeName:
				_outTypeName(str, obj);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj);
				break;
			case T_Query:
				_outQuery(str, obj);
				break;
			case T_SortClause:
				_outSortClause(str, obj);
				break;
			case T_GroupClause:
				_outGroupClause(str, obj);
				break;
			case T_GroupingClause:
				_outGroupingClause(str, obj);
				break;
			case T_GroupingFunc:
				_outGroupingFunc(str, obj);
				break;
			case T_Grouping:
				_outGrouping(str, obj);
				break;
			case T_GroupId:
				_outGroupId(str, obj);
				break;
			case T_WindowSpecParse:
				_outWindowSpecParse(str, obj);
				break;
			case T_WindowSpec:
				_outWindowSpec(str, obj);
				break;
			case T_WindowFrame:
				_outWindowFrame(str, obj);
				break;
			case T_WindowFrameEdge:
				_outWindowFrameEdge(str, obj);
				break;
			case T_PercentileExpr:
				_outPercentileExpr(str, obj);
				break;
			case T_RowMarkClause:
				_outRowMarkClause(str, obj);
				break;
			case T_WithClause:
				_outWithClause(str, obj);
				break;
			case T_CommonTableExpr:
				_outCommonTableExpr(str, obj);
				break;
			case T_SetOperationStmt:
				_outSetOperationStmt(str, obj);
				break;
			case T_RangeTblEntry:
				_outRangeTblEntry(str, obj);
				break;
			case T_A_Expr:
				_outAExpr(str, obj);
				break;
			case T_ColumnRef:
				_outColumnRef(str, obj);
				break;
			case T_ParamRef:
				_outParamRef(str, obj);
				break;
			case T_A_Const:
				_outAConst(str, obj);
				break;
			case T_A_Indices:
				_outA_Indices(str, obj);
				break;
			case T_A_Indirection:
				_outA_Indirection(str, obj);
				break;
			case T_ResTarget:
				_outResTarget(str, obj);
				break;
			case T_Constraint:
				_outConstraint(str, obj);
				break;
			case T_FkConstraint:
				_outFkConstraint(str, obj);
				break;
			case T_FuncCall:
				_outFuncCall(str, obj);
				break;
			case T_DefElem:
				_outDefElem(str, obj);
				break;
			case T_CreateSchemaStmt:
				_outCreateSchemaStmt(str, obj);
				break;
			case T_CreatePLangStmt:
				_outCreatePLangStmt(str, obj);
				break;
			case T_DropPLangStmt:
				_outDropPLangStmt(str, obj);
				break;
			case T_VacuumStmt:
				_outVacuumStmt(str, obj);
				break;
			case T_CdbProcess:
				_outCdbProcess(str, obj);
				break;
			case T_Slice:
				_outSlice(str, obj);
				break;
			case T_SliceTable:
				_outSliceTable(str, obj);
				break;
			case T_VariableResetStmt:
				_outVariableResetStmt(str, obj);
				break;

			case T_LockingClause:
				_outLockingClause(str, obj);
				break;

			case T_DMLActionExpr:
				_outDMLActionExpr(str, obj);
				break;

			case T_PartOidExpr:
				_outPartOidExpr(str, obj);
				break;

			case T_PartDefaultExpr:
				_outPartDefaultExpr(str, obj);
				break;

			case T_PartBoundExpr:
				_outPartBoundExpr(str, obj);
				break;

			case T_PartBoundInclusionExpr:
				_outPartBoundInclusionExpr(str, obj);
				break;

			case T_PartBoundOpenExpr:
				_outPartBoundOpenExpr(str, obj);
				break;

			case T_CreateTrigStmt:
				_outCreateTrigStmt(str, obj);
				break;

			case T_CreateFileSpaceStmt:
				_outCreateFileSpaceStmt(str, obj);
				break;

			case T_FileSpaceEntry:
				_outFileSpaceEntry(str, obj);
				break;

			case T_CreateTableSpaceStmt:
				_outCreateTableSpaceStmt(str, obj);
				break;

			case T_CreateQueueStmt:
				_outCreateQueueStmt(str, obj);
				break;
			case T_AlterQueueStmt:
				_outAlterQueueStmt(str, obj);
				break;
			case T_DropQueueStmt:
				_outDropQueueStmt(str, obj);
				break;

            case T_CommentStmt:
                _outCommentStmt(str, obj);
                break;
			case T_TableValueExpr:
				_outTableValueExpr(str, obj);
                break;
			case T_DenyLoginInterval:
				_outDenyLoginInterval(str, obj);
				break;
			case T_DenyLoginPoint:
				_outDenyLoginPoint(str, obj);
				break;

			case T_AlterTypeStmt:
				_outAlterTypeStmt(str, obj);
				break;
			case T_TupleDescNode:
				_outTupleDescNode(str, obj);
				break;

			default:
				elog(ERROR, "could not serialize unrecognized node type: %d",
						 (int) nodeTag(obj));
				break;
		}

	}
}

/*
 * Initialize global variables for serializing a plan for the workfile manager.
 * The serialized form of a plan for workfile manager does not include some
 * variable fields such as costs and node ids.
 * In addition, range table pointers are replaced with Oids where applicable.
 */
void
outfast_workfile_mgr_init(List *rtable)
{
	Assert(NULL == range_table);
	Assert(print_variable_fields);
	range_table = rtable;
	print_variable_fields = false;
}

/*
 * Reset global variables to their default values at the end of serializing
 * a plan for the workfile manager.
 */
void
outfast_workfile_mgr_end()
{
	Assert(!print_variable_fields);

	print_variable_fields = true;
	range_table = NULL;
}


/*
 * nodeToBinaryStringFast -
 *	   returns a binary representation of the Node as a palloc'd string
 */
char *
nodeToBinaryStringFast(void *obj, int * length)
{
	StringInfoData str;
	int16 tg = (int16) 0xDEAD;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfoOfSize(&str, 4096);

	_outNode(&str, obj);

	/* Add something special at the end that we can check in readfast.c */
	appendBinaryStringInfo(&str, (const char *)&tg, sizeof(int16));

	*length = str.len;
	return str.data;
}

