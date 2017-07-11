/*-------------------------------------------------------------------------
 *
 * outfuncs.c
 *	  Output functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/nodes/outfuncs.c,v 1.340 2008/10/04 21:56:53 tgl Exp $
 *
 * NOTES
 *	  Every node type that can appear in stored rules' parsetrees *must*
 *	  have an output function defined here (as well as an input function
 *	  in readfuncs.c).	For use in debugging, we also provide output
 *	  functions for nodes that appear in raw parsetrees, path, and plan trees.
 *	  These nodes however need not have input functions.
 *
 *    N.B. Faster variants of these functions (producing illegible output)
 *         are supplied in outfast.c for use in Greenplum Database serialization.  The
 *         function in this file are intended to produce legible output.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "cdb/cdbgang.h"
#include "utils/workfile_mgr.h"
#include "parser/parsetree.h"


/*
 * outfuncs.c is compiled normally into outfuncs.o, but it's also
 * #included from outfast.c. When #included, outfast.c defines
 * COMPILING_BINARY_FUNCS, and provides replacements WRITE_* macros. See
 * comments at top of readfast.c.
 */
#ifndef COMPILING_BINARY_FUNCS

/*
 * Macros to simplify output of different kinds of fields.	Use these
 * wherever possible to reduce the chance for silly typos.	Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) \
	appendStringInfoLiteral(str, nodelabel)

/* Write an integer field (anything written as ":fldname %d") */
#define WRITE_INT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write an unsigned integer field (anything written as ":fldname %u") */
#define WRITE_UINT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write an uint64 field (anything written as ":fldname %u") */
#define WRITE_UINT64_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " " UINT64_FORMAT, node->fldname)

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* CDB: Write an OID field, renamed */
#define WRITE_OID_FIELD_AS(fldname, asname) \
	appendStringInfo(str, " :" CppAsString(asname) " %u", node->fldname)

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %ld", node->fldname)

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) \
	if ( node->fldname == '\\' ) \
		appendStringInfo(str, " :" CppAsString(fldname) " \\\\"); \
	else if ( isprint(node->fldname) ) \
		appendStringInfo(str, " :" CppAsString(fldname) " %c", node->fldname); \
	else \
		appendStringInfo(str, " :" CppAsString(fldname) " %03u", (unsigned)node->fldname)


/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", \
					 (int) node->fldname)

/* Write a float field --- caller must give format to define precision */
#define WRITE_FLOAT_FIELD(fldname,format) \
	appendStringInfo(str, " :" CppAsString(fldname) " " format, node->fldname)

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %s", \
					 booltostr(node->fldname))

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outToken(str, node->fldname))

/* Write a parse location field (actually same as INT case) */
#define WRITE_LOCATION_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outNode(str, node->fldname))

/* CDB: Write a Node field, renamed */
#define WRITE_NODE_FIELD_AS(fldname, asname) \
	(appendStringInfo(str, " :" CppAsString(asname) " "), \
	 _outNode(str, node->fldname))

/* Write a bitmapset field */
#define WRITE_BITMAPSET_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outBitmapset(str, node->fldname))

/* Write a bytea field */
#define WRITE_BYTEA_FIELD(fldname) \
	(_outDatum(str, PointerGetDatum(node->fldname), -1, false))

/* Write a dummy field -- value not displayable or copyable */
#define WRITE_DUMMY_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outToken(str, NULL))

#define booltostr(x)  ((x) ? "true" : "false")

static void _outNode(StringInfo str, void *obj);

/*
 * _outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void
_outToken(StringInfo str, const char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfoLiteral(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by read.c
	 * (either in pg_strtok() or in nodeRead()), and therefore need a
	 * protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '\"' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

static void
_outList(StringInfo str, List *node)
{
	ListCell   *lc;

	appendStringInfoChar(str, '(');

	if (IsA(node, IntList))
		appendStringInfoChar(str, 'i');
	else if (IsA(node, OidList))
		appendStringInfoChar(str, 'o');

	foreach(lc, node)
	{
		/*
		 * For the sake of backward compatibility, we emit a slightly
		 * different whitespace format for lists of nodes vs. other types of
		 * lists. XXX: is this necessary?
		 */
		if (IsA(node, List))
		{
			_outNode(str, lfirst(lc));
			if (lnext(lc))
				appendStringInfoChar(str, ' ');
		}
		else if (IsA(node, IntList))
			appendStringInfo(str, " %d", lfirst_int(lc));
		else if (IsA(node, OidList))
			appendStringInfo(str, " %u", lfirst_oid(lc));
		else
			elog(ERROR, "unrecognized list node type: %d",
				 (int) node->type);
	}

	appendStringInfoChar(str, ')');
}

/*
 * _outBitmapset -
 *	   converts a bitmap set of integers
 *
 * Note: the output format is "(b int int ...)", similar to an integer List.
 * Currently bitmapsets do not appear in any node type that is stored in
 * rules, so there is no support in readfuncs.c for reading this format.
 */
static void
_outBitmapset(StringInfo str, Bitmapset *bms)
{
	Bitmapset  *tmpset;
	int			x;

	appendStringInfoChar(str, '(');
	appendStringInfoChar(str, 'b');
	tmpset = bms_copy(bms);
	while ((x = bms_first_member(tmpset)) >= 0)
		appendStringInfo(str, " %d", x);
	bms_free(tmpset);
	appendStringInfoChar(str, ')');
}

/*
 * Print the value of a Datum given its type.
 */
static void
_outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
	Size		length,
				i;
	char	   *s;

	length = datumGetSize(value, typbyval, typlen);

	if (typbyval)
	{
		s = (char *) (&value);
		appendStringInfo(str, "%u [ ", (unsigned int) length);
		for (i = 0; i < (Size) sizeof(Datum); i++)
			appendStringInfo(str, "%d ", (int) (s[i]));
		appendStringInfoChar(str, ']');
	}
	else
	{
		s = (char *) DatumGetPointer(value);
		if (!PointerIsValid(s))
			appendStringInfoLiteral(str, "0 [ ]");
		else
		{
			appendStringInfo(str, "%u [ ", (unsigned int) length);
			for (i = 0; i < length; i++)
				appendStringInfo(str, "%d ", (int) (s[i]));
			appendStringInfoChar(str, ']');
		}
	}
}

#endif /* COMPILING_BINARY_FUNCS */

static void _outPlanInfo(StringInfo str, Plan *node);
static void outLogicalIndexInfo(StringInfo str, LogicalIndexInfo *node);

/*
 *	Stuff from plannodes.h
 */

#ifndef COMPILING_BINARY_FUNCS
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
	WRITE_BITMAPSET_FIELD(rewindPlanIDs);
	WRITE_NODE_FIELD(returningLists);

	WRITE_NODE_FIELD(result_partitions);
	WRITE_NODE_FIELD(result_aosegnos);
	WRITE_NODE_FIELD(queryPartOids);
	WRITE_NODE_FIELD(queryPartsMetadata);
	WRITE_NODE_FIELD(numSelectorsPerScanId);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(relationOids);
	WRITE_NODE_FIELD(invalItems);
	WRITE_INT_FIELD(nParamExec);
	WRITE_INT_FIELD(nMotionNodes);
	WRITE_INT_FIELD(nInitPlans);

	/* Don't serialize policy */

	WRITE_UINT64_FIELD(query_mem);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outQueryDispatchDesc(StringInfo str, QueryDispatchDesc *node)
{
	WRITE_NODE_TYPE("QUERYDISPATCHDESC");

	WRITE_STRING_FIELD(intoTableSpaceName);
	WRITE_NODE_FIELD(oidAssignments);
	WRITE_NODE_FIELD(sliceTable);
	WRITE_NODE_FIELD(cursorPositions);
}

static void
_outOidAssignment(StringInfo str, OidAssignment *node)
{
	WRITE_NODE_TYPE("OIDASSIGNMENT");

	WRITE_OID_FIELD(catalog);
	WRITE_STRING_FIELD(objname);
	WRITE_OID_FIELD(namespaceOid);
	WRITE_OID_FIELD(keyOid1);
	WRITE_OID_FIELD(keyOid2);
	WRITE_OID_FIELD(oid);
}

#ifndef COMPILING_BINARY_FUNCS
/*
 * print the basic stuff of all nodes that inherit from Plan
 */
static void
_outPlanInfo(StringInfo str, Plan *node)
{
	WRITE_INT_FIELD(plan_node_id);
	WRITE_INT_FIELD(plan_parent_node_id);

	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(total_cost, "%.2f");
	WRITE_FLOAT_FIELD(plan_rows, "%.0f");
	WRITE_INT_FIELD(plan_width);

	WRITE_NODE_FIELD(targetlist);
	WRITE_NODE_FIELD(qual);

	WRITE_BITMAPSET_FIELD(extParam);
	WRITE_BITMAPSET_FIELD(allParam);

	WRITE_NODE_FIELD(flow);
	WRITE_ENUM_FIELD(dispatch, DispatchMethod);
	WRITE_INT_FIELD(nMotionNodes);
	WRITE_INT_FIELD(nInitPlans);
	WRITE_NODE_FIELD(sliceTable);

	WRITE_NODE_FIELD(lefttree);
	WRITE_NODE_FIELD(righttree);
	WRITE_NODE_FIELD(initPlan);

	WRITE_UINT64_FIELD(operatorMemKB);
}
#endif /* COMPILING_BINARY_FUNCS */

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_outScanInfo(StringInfo str, Scan *node)
{
	_outPlanInfo(str, (Plan *) node);

	WRITE_UINT_FIELD(scanrelid);

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
}

static void
_outSequence(StringInfo str, Sequence *node)
{
	WRITE_NODE_TYPE("SEQUENCE");
	_outPlanInfo(str, (Plan *)node);
	WRITE_NODE_FIELD(subplans);
}

static void
_outRecursiveUnion(StringInfo str, RecursiveUnion *node)
{
	WRITE_NODE_TYPE("RECURSIVEUNION");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(wtParam);
}

static void
_outBitmapAnd(StringInfo str, BitmapAnd *node)
{
	WRITE_NODE_TYPE("BITMAPAND");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(bitmapplans);
}

static void
_outBitmapOr(StringInfo str, BitmapOr *node)
{
	WRITE_NODE_TYPE("BITMAPOR");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(bitmapplans);
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

#ifndef COMPILING_BINARY_FUNCS
static void
outLogicalIndexInfo(StringInfo str, LogicalIndexInfo *node)
{
	WRITE_OID_FIELD(logicalIndexOid);
	WRITE_INT_FIELD(nColumns);
	appendStringInfoLiteral(str, " :indexKeys");
	for (int i = 0; i < node->nColumns; i++)
	{
		appendStringInfo(str, " %d", node->indexKeys[i]);
	}
	WRITE_NODE_FIELD(indPred);
	WRITE_NODE_FIELD(indExprs);
	WRITE_BOOL_FIELD(indIsUnique);
	WRITE_ENUM_FIELD(indType, LogicalIndexType);
	WRITE_NODE_FIELD(partCons);
	WRITE_NODE_FIELD(defaultLevels);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
outIndexScanFields(StringInfo str, IndexScan *node)
{
	_outScanInfo(str, (Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_NODE_FIELD(indexqual);
	WRITE_NODE_FIELD(indexqualorig);
	WRITE_NODE_FIELD(indexstrategy);
	WRITE_NODE_FIELD(indexsubtype);
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

	outIndexScanFields(str, (IndexScan *)node);
}

static void
_outBitmapIndexScan(StringInfo str, BitmapIndexScan *node)
{
	WRITE_NODE_TYPE("BITMAPINDEXSCAN");

	outIndexScanFields(str, (IndexScan *)node);
}

static void
_outBitmapHeapScan(StringInfo str, BitmapHeapScan *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(bitmapqualorig);
}

static void
_outBitmapAppendOnlyScan(StringInfo str, BitmapAppendOnlyScan *node)
{
	WRITE_NODE_TYPE("BITMAPAPPENDONLYSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(bitmapqualorig);
	WRITE_BOOL_FIELD(isAORow);
}

static void
_outBitmapTableScan(StringInfo str, BitmapTableScan *node)
{
	WRITE_NODE_TYPE("BITMAPTABLESCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(bitmapqualorig);
}

static void
_outTidScan(StringInfo str, TidScan *node)
{
	WRITE_NODE_TYPE("TIDSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(tidquals);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outSubqueryScan(StringInfo str, SubqueryScan *node)
{
	WRITE_NODE_TYPE("SUBQUERYSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(subplan);
	WRITE_NODE_FIELD(subrtable); /* debugging convenience */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outFunctionScan(StringInfo str, FunctionScan *node)
{
	WRITE_NODE_TYPE("FUNCTIONSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(funcexpr);
	WRITE_NODE_FIELD(funccolnames);
	WRITE_NODE_FIELD(funccoltypes);
	WRITE_NODE_FIELD(funccoltypmods);
}

static void
_outValuesScan(StringInfo str, ValuesScan *node)
{
	WRITE_NODE_TYPE("VALUESSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(values_lists);
}

static void
_outCteScan(StringInfo str, CteScan *node)
{
	WRITE_NODE_TYPE("CTESCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_INT_FIELD(ctePlanId);
	WRITE_INT_FIELD(cteParam);
}

static void
_outWorkTableScan(StringInfo str, WorkTableScan *node)
{
	WRITE_NODE_TYPE("WORKTABLESCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_INT_FIELD(wtParam);
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

	WRITE_BOOL_FIELD(shared_outer);
	WRITE_BOOL_FIELD(singleton_outer); /*CDB-OLAP*/
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outMergeJoin(StringInfo str, MergeJoin *node)
{
	int			numCols;
	int			i;

	WRITE_NODE_TYPE("MERGEJOIN");

	_outJoinPlanInfo(str, (Join *) node);

	WRITE_NODE_FIELD(mergeclauses);

	numCols = list_length(node->mergeclauses);

	appendStringInfo(str, " :mergeFamilies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %u", node->mergeFamilies[i]);

	appendStringInfo(str, " :mergeStrategies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", node->mergeStrategies[i]);

	appendStringInfo(str, " :mergeNullsFirst");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", (int) node->mergeNullsFirst[i]);

	WRITE_BOOL_FIELD(unique_outer);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outHashJoin(StringInfo str, HashJoin *node)
{
	WRITE_NODE_TYPE("HASHJOIN");

	_outJoinPlanInfo(str, (Join *) node);

	WRITE_NODE_FIELD(hashclauses);
	WRITE_NODE_FIELD(hashqualclauses);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAgg(StringInfo str, Agg *node)
{
	int i;

	WRITE_NODE_TYPE("AGG");

	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(aggstrategy, AggStrategy);
	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :grpColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->grpColIdx[i]);

	appendStringInfo(str, " :grpOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->grpOperators[i]);

	WRITE_LONG_FIELD(numGroups);
	WRITE_INT_FIELD(transSpace);
	WRITE_INT_FIELD(numNullCols);
	WRITE_UINT64_FIELD(inputGrouping);
	WRITE_UINT64_FIELD(grouping);
	WRITE_BOOL_FIELD(inputHasGrouping);
	WRITE_INT_FIELD(rollupGSTimes);
	WRITE_BOOL_FIELD(lastAgg);
	WRITE_BOOL_FIELD(streaming);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outWindowKey(StringInfo str, WindowKey *node)
{
	int			i;

	WRITE_NODE_TYPE("WINDOWKEY");
	WRITE_INT_FIELD(numSortCols);

	appendStringInfoLiteral(str, " :sortColIdx");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfoLiteral(str, " :sortOperators");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	WRITE_NODE_FIELD(frame);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outWindow(StringInfo str, Window *node)
{
	int			i;

	WRITE_NODE_TYPE("WINDOW");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numPartCols);

	appendStringInfoLiteral(str, " :partColIdx");
	for (i = 0; i < node->numPartCols; i++)
		appendStringInfo(str, " %d", node->partColIdx[i]);

	appendStringInfoLiteral(str, " :partOperators");
	for (i = 0; i < node->numPartCols; i++)
		appendStringInfo(str, " %u", node->partOperators[i]);

	WRITE_NODE_FIELD(windowKeys);
}
#endif /* COMPILING_BINARY_FUNCS */

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

#ifndef COMPILING_BINARY_FUNCS
static void
_outSort(StringInfo str, Sort *node)
{
	int			i;

	WRITE_NODE_TYPE("SORT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfoLiteral(str, " :sortColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfoLiteral(str, " :sortOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	appendStringInfo(str, " :nullsFirst");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %s", booltostr(node->nullsFirst[i]));

	/* CDB */
    WRITE_BOOL_FIELD(noduplicates);

	WRITE_ENUM_FIELD(share_type, ShareType);
	WRITE_INT_FIELD(share_id);
	WRITE_INT_FIELD(driver_slice);
	WRITE_INT_FIELD(nsharer);
	WRITE_INT_FIELD(nsharer_xslice);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outUnique(StringInfo str, Unique *node)
{
	int			i;

	WRITE_NODE_TYPE("UNIQUE");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfoLiteral(str, " :uniqColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->uniqColIdx[i]);

	appendStringInfo(str, " :uniqOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->uniqOperators[i]);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outHash(StringInfo str, Hash *node)
{
	WRITE_NODE_TYPE("HASH");

	_outPlanInfo(str, (Plan *) node);
	WRITE_BOOL_FIELD(rescannable);          /*CDB*/
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outSetOp(StringInfo str, SetOp *node)
{
	int			i;

	WRITE_NODE_TYPE("SETOP");

	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(cmd, SetOpCmd);
	WRITE_INT_FIELD(numCols);

	appendStringInfoLiteral(str, " :dupColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->dupColIdx[i]);

	appendStringInfo(str, " :dupOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->dupOperators[i]);

	WRITE_INT_FIELD(flagColIdx);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outLimit(StringInfo str, Limit *node)
{
	WRITE_NODE_TYPE("LIMIT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outPlanInvalItem(StringInfo str, PlanInvalItem *node)
{
	WRITE_NODE_TYPE("PLANINVALITEM");

	WRITE_INT_FIELD(cacheId);
	appendStringInfo(str, " :tupleId (%u,%u)",
					 ItemPointerGetBlockNumber(&node->tupleId),
					 ItemPointerGetOffsetNumber(&node->tupleId));
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outMotion(StringInfo str, Motion *node)
{
	int i;

	WRITE_NODE_TYPE("MOTION");

	WRITE_INT_FIELD(motionID);
	WRITE_ENUM_FIELD(motionType, MotionType);

	WRITE_BOOL_FIELD(sendSorted);

	WRITE_NODE_FIELD(hashExpr);
	WRITE_NODE_FIELD(hashDataTypes);

	WRITE_INT_FIELD(numOutputSegs);
	appendStringInfoLiteral(str, " :outputSegIdx");
	for (i = 0; i < node->numOutputSegs; i++)
		appendStringInfo(str, " %d", node->outputSegIdx[i]);

	WRITE_INT_FIELD(numSortCols);
	appendStringInfoLiteral(str, " :sortColIdx");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfoLiteral(str, " :sortOperators");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	WRITE_INT_FIELD(segidColIdx);

	_outPlanInfo(str, (Plan *) node);
}
#endif /* COMPILING_BINARY_FUNCS */

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
	WRITE_NODE_TYPE("SplitUpdate");

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
	WRITE_NODE_TYPE("RowTrigger");

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
	WRITE_NODE_TYPE("AssertOp");

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
	WRITE_NODE_TYPE("PartitionSelector");

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
	WRITE_NODE_FIELD(partTabTargetlist);

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
    WRITE_LOCATION_FIELD(location);
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
}

static void
_outVar(StringInfo str, Var *node)
{
	WRITE_NODE_TYPE("VAR");

	WRITE_UINT_FIELD(varno);
	WRITE_INT_FIELD(varattno);
	WRITE_OID_FIELD(vartype);
	WRITE_INT_FIELD(vartypmod);
	WRITE_UINT_FIELD(varlevelsup);
	WRITE_UINT_FIELD(varnoold);
	WRITE_INT_FIELD(varoattno);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outConst(StringInfo str, Const *node)
{
	WRITE_NODE_TYPE("CONST");

	WRITE_OID_FIELD(consttype);
	WRITE_INT_FIELD(consttypmod);
	WRITE_INT_FIELD(constlen);
	WRITE_BOOL_FIELD(constbyval);
	WRITE_BOOL_FIELD(constisnull);

	appendStringInfoLiteral(str, " :constvalue ");
	if (node->constisnull)
		appendStringInfoLiteral(str, "<>");
	else
		_outDatum(str, node->constvalue, node->constlen, node->constbyval);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outParam(StringInfo str, Param *node)
{
	WRITE_NODE_TYPE("PARAM");

	WRITE_ENUM_FIELD(paramkind, ParamKind);
	WRITE_INT_FIELD(paramid);
	WRITE_OID_FIELD(paramtype);
	WRITE_INT_FIELD(paramtypmod);
}

#ifndef COMPILING_BINARY_FUNCS
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
#endif /* COMPILING_BINARY_FUNCS */

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

	WRITE_OID_FIELD(refarraytype);
	WRITE_OID_FIELD(refelemtype);
	WRITE_INT_FIELD(reftypmod);
	WRITE_NODE_FIELD(refupperindexpr);
	WRITE_NODE_FIELD(reflowerindexpr);
	WRITE_NODE_FIELD(refexpr);
	WRITE_NODE_FIELD(refassgnexpr);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outFuncExpr(StringInfo str, FuncExpr *node)
{
	WRITE_NODE_TYPE("FUNCEXPR");

	WRITE_OID_FIELD(funcid);
	WRITE_OID_FIELD(funcresulttype);
	WRITE_BOOL_FIELD(funcretset);
	WRITE_ENUM_FIELD(funcformat, CoercionForm);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(is_tablefunc);  /* GPDB */
}
#endif /* COMPILING_BINARY_FUNCS */

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

#ifndef COMPILING_BINARY_FUNCS
static void
_outBoolExpr(StringInfo str, BoolExpr *node)
{
	char	   *opstr = NULL;

	WRITE_NODE_TYPE("BOOLEXPR");

	/* do-it-yourself enum representation */
	switch (node->boolop)
	{
		case AND_EXPR:
			opstr = "and";
			break;
		case OR_EXPR:
			opstr = "or";
			break;
		case NOT_EXPR:
			opstr = "not";
			break;
	}
	appendStringInfoLiteral(str, " :boolop ");
	_outToken(str, opstr);

	WRITE_NODE_FIELD(args);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outSubLink(StringInfo str, SubLink *node)
{
	WRITE_NODE_TYPE("SUBLINK");

	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(operName);
    /*
     * CDB: For now we don't serialize the 'location' field, for compatibility
     * so stored sublinks can be read by pre-3.2 releases.  Anyway it's only
     * meaningful with the original source string, which isn't kept when a
     * view or rule definition is stored in the catalog.
     */
	WRITE_NODE_FIELD(subselect);
}
#endif /* COMPILING_BINARY_FUNCS */

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
_outCoerceViaIO(StringInfo str, CoerceViaIO *node)
{
	WRITE_NODE_TYPE("COERCEVIAIO");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_ENUM_FIELD(coerceformat, CoercionForm);
}

static void
_outArrayCoerceExpr(StringInfo str, ArrayCoerceExpr *node)
{
	WRITE_NODE_TYPE("ARRAYCOERCEEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(elemfuncid);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_BOOL_FIELD(isExplicit);
	WRITE_ENUM_FIELD(coerceformat, CoercionForm);
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
/*	WRITE_LOCATION_FIELD(location); */
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
	WRITE_NODE_FIELD(opfamilies);
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
_outXmlExpr(StringInfo str, XmlExpr *node)
{
	WRITE_NODE_TYPE("XMLEXPR");

	WRITE_ENUM_FIELD(op, XmlExprOp);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(named_args);
	WRITE_NODE_FIELD(arg_names);
	WRITE_NODE_FIELD(args);
	WRITE_ENUM_FIELD(xmloption, XmlOptionType);
	WRITE_OID_FIELD(type);
	WRITE_INT_FIELD(typmod);
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

#ifndef COMPILING_BINARY_FUNCS
static void
_outCurrentOfExpr(StringInfo str, CurrentOfExpr *node)
{
	WRITE_NODE_TYPE("CURRENTOFEXPR");

	WRITE_STRING_FIELD(cursor_name);
	WRITE_INT_FIELD(cvarno);
	WRITE_OID_FIELD(target_relid);

	/* some attributes omitted as they're bound only just before executor dispatch */
}
#endif /* COMPILING_BINARY_FUNCS */

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

#ifndef COMPILING_BINARY_FUNCS
static void
_outJoinExpr(StringInfo str, JoinExpr *node)
{
	WRITE_NODE_TYPE("JOINEXPR");

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_BOOL_FIELD(isNatural);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
    if (node->subqfromlist)                     /*CDB*/
        WRITE_NODE_FIELD(subqfromlist);         /*CDB*/
	WRITE_NODE_FIELD_AS(usingClause, using);    /*CDB*/
	WRITE_NODE_FIELD(quals);
	WRITE_NODE_FIELD(alias);
	WRITE_INT_FIELD(rtindex);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outFromExpr(StringInfo str, FromExpr *node)
{
	WRITE_NODE_TYPE("FROMEXPR");

	WRITE_NODE_FIELD(fromlist);
	WRITE_NODE_FIELD(quals);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outFlow(StringInfo str, Flow *node)
{
	int i;

	WRITE_NODE_TYPE("FLOW");

	WRITE_ENUM_FIELD(flotype, FlowType);
	WRITE_ENUM_FIELD(req_move, Movement);
	WRITE_ENUM_FIELD(locustype, CdbLocusType);
	WRITE_INT_FIELD(segindex);

	/* This array format as in Group and Sort nodes. */
	WRITE_INT_FIELD(numSortCols);
	if(node->numSortCols > 0)
	{
		appendStringInfoLiteral(str, " :sortColIdx");
		if(node->sortColIdx == NULL)
			appendStringInfoString(str, " <>");
		else {
			for ( i = 0; i < node->numSortCols; i++ )
				appendStringInfo(str, " %d", node->sortColIdx[i]);
		}

		appendStringInfoLiteral(str, " :sortOperators");
		if(node->sortOperators == NULL)
			appendStringInfoString(str, " <>");
		else {
			for ( i = 0; i < node->numSortCols; i++ )
				appendStringInfo(str, " %u", node->sortOperators[i]);
		}
	}
	WRITE_INT_FIELD(numOrderbyCols);

	WRITE_NODE_FIELD(hashExpr);

	WRITE_NODE_FIELD(flow_before_req_move);
}
#endif /* COMPILING_BINARY_FUNCS */

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
    WRITE_NODE_FIELD(partkey_h);
    WRITE_NODE_FIELD(partkey_oj);
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
	WRITE_NODE_TYPE("APPENDONLYPATH");

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

#ifndef COMPILING_BINARY_FUNCS
static void
_outPlannerGlobal(StringInfo str, PlannerGlobal *node)
{
	WRITE_NODE_TYPE("PLANNERGLOBAL");
	
	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(paramlist);
	WRITE_NODE_FIELD(subplans);
	WRITE_NODE_FIELD(subrtables);
	WRITE_BITMAPSET_FIELD(rewindPlanIDs);
	WRITE_NODE_FIELD(finalrtable);
	WRITE_NODE_FIELD(relationOids);
	WRITE_NODE_FIELD(invalItems);
	WRITE_BOOL_FIELD(transientPlan);
	WRITE_NODE_FIELD(share.motStack);
	WRITE_NODE_FIELD(share.qdShares);
	WRITE_NODE_FIELD(share.qdSlices);
	WRITE_INT_FIELD(share.nextPlanId);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPlannerInfo(StringInfo str, PlannerInfo *node)
{
	WRITE_NODE_TYPE("PLANNERINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(parse);
	WRITE_NODE_FIELD(glob);
	WRITE_UINT_FIELD(query_level);
	WRITE_NODE_FIELD(join_rel_list);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(returningLists);
	WRITE_NODE_FIELD(init_plans);
	WRITE_NODE_FIELD(cte_plan_ids);
	WRITE_NODE_FIELD(eq_classes);
	WRITE_NODE_FIELD(canon_pathkeys);
	WRITE_NODE_FIELD(left_join_clauses);
	WRITE_NODE_FIELD(right_join_clauses);
	WRITE_NODE_FIELD(full_join_clauses);
	WRITE_NODE_FIELD(join_info_list);
	WRITE_NODE_FIELD(append_rel_list);
	WRITE_NODE_FIELD(query_pathkeys);
	WRITE_NODE_FIELD(group_pathkeys);
	WRITE_NODE_FIELD(sort_pathkeys);
	WRITE_FLOAT_FIELD(total_table_pages, "%.0f");
	WRITE_FLOAT_FIELD(tuple_fraction, "%.4f");
	WRITE_BOOL_FIELD(hasJoinRTEs);
	WRITE_BOOL_FIELD(hasHavingQual);
	WRITE_BOOL_FIELD(hasPseudoConstantQuals);
	WRITE_BOOL_FIELD(hasRecursion);
	WRITE_INT_FIELD(wt_param_id);
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
	/* WRITE_NODE_FIELD(pathlist);              */
	/* WRITE_NODE_FIELD(cheapest_startup_path); */
	/* WRITE_NODE_FIELD(cheapest_total_path);   */
	WRITE_NODE_FIELD(dedup_info);
	WRITE_UINT_FIELD(relid);
	WRITE_ENUM_FIELD(rtekind, RTEKind);
	WRITE_INT_FIELD(min_attr);
	WRITE_INT_FIELD(max_attr);
	WRITE_NODE_FIELD(indexlist);
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_NODE_FIELD(subplan);
	WRITE_NODE_FIELD(urilocationlist);
	WRITE_NODE_FIELD(execlocationlist);
	WRITE_STRING_FIELD(execcommand);
	WRITE_CHAR_FIELD(fmttype);
	WRITE_STRING_FIELD(fmtopts);
	WRITE_INT_FIELD(rejectlimit);
	WRITE_CHAR_FIELD(rejectlimittype);
	WRITE_OID_FIELD(fmterrtbl);
	WRITE_INT_FIELD(ext_encoding);
	WRITE_BOOL_FIELD(writable);
	WRITE_NODE_FIELD(subrtable);
	WRITE_NODE_FIELD(baserestrictinfo);
	WRITE_NODE_FIELD(joininfo);
	WRITE_BOOL_FIELD(has_eclass_joins);
	WRITE_BITMAPSET_FIELD(index_outer_relids);
	/* Skip writing Path ptrs to avoid endless recursion */
	/* WRITE_NODE_FIELD(index_inner_paths);     */
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outIndexOptInfo(StringInfo str, IndexOptInfo *node)
{
    int i;

	WRITE_NODE_TYPE("INDEXOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_OID_FIELD(indexoid);
	/* Do NOT print rel field, else infinite recursion */
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_INT_FIELD(ncolumns);

	appendStringInfoLiteral(str, " :opfamily");
	for (i = 0; i < node->ncolumns; i++)
		appendStringInfo(str, " %u", node->opfamily[i]);

	appendStringInfoLiteral(str, " :indexkeys");
	for (i = 0; i < node->ncolumns; i++)
		appendStringInfo(str, " %d", node->indexkeys[i]);

	appendStringInfoLiteral(str, " :fwdsortop");
	for (i = 0; i < node->ncolumns; i++)
		appendStringInfo(str, " %u", node->fwdsortop[i]);

	appendStringInfoLiteral(str, " :revsortop");
	for (i = 0; i < node->ncolumns; i++)
		appendStringInfo(str, " %u", node->revsortop[i]);

	WRITE_BOOL_FIELD(nulls_first);

    WRITE_OID_FIELD(relam);
	WRITE_OID_FIELD(amcostestimate);
	WRITE_NODE_FIELD(indexprs);
	WRITE_NODE_FIELD(indpred);
	WRITE_BOOL_FIELD(predOK);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(amoptionalkey);
	WRITE_BOOL_FIELD(cdb_default_stats_used);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outCdbRelColumnInfo(StringInfo str, CdbRelColumnInfo *node)
{
	WRITE_NODE_TYPE("CdbRelColumnInfo");

    WRITE_INT_FIELD(pseudoattno);
    WRITE_INT_FIELD(targetresno);
	WRITE_INT_FIELD(attr_width);
	WRITE_BITMAPSET_FIELD(where_needed);
    WRITE_STRING_FIELD(colname);
	WRITE_NODE_FIELD(defexpr);
}
#endif /* COMPILING_BINARY_FUNCS */

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

#ifndef COMPILING_BINARY_FUNCS
static void
_outEquivalenceClass(StringInfo str, EquivalenceClass *node)
{
	/*
	 * To simplify reading, we just chase up to the topmost merged EC and
	 * print that, without bothering to show the merge-ees separately.
	 */
	while (node->ec_merged)
		node = node->ec_merged;

	WRITE_NODE_TYPE("EQUIVALENCECLASS");

	WRITE_NODE_FIELD(ec_opfamilies);
	WRITE_NODE_FIELD(ec_members);
	WRITE_NODE_FIELD(ec_sources);
	WRITE_NODE_FIELD(ec_derives);
	WRITE_BITMAPSET_FIELD(ec_relids);
	WRITE_BOOL_FIELD(ec_has_const);
	WRITE_BOOL_FIELD(ec_has_volatile);
	WRITE_BOOL_FIELD(ec_below_outer_join);
	WRITE_BOOL_FIELD(ec_broken);
	WRITE_UINT_FIELD(ec_sortref);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outEquivalenceMember(StringInfo str, EquivalenceMember *node)
{
	WRITE_NODE_TYPE("EQUIVALENCEMEMBER");

	WRITE_NODE_FIELD(em_expr);
	WRITE_BITMAPSET_FIELD(em_relids);
	WRITE_BITMAPSET_FIELD(em_nullable_relids);
	WRITE_BOOL_FIELD(em_is_const);
	WRITE_BOOL_FIELD(em_is_child);
	WRITE_OID_FIELD(em_datatype);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPathKey(StringInfo str, PathKey *node)
{
	WRITE_NODE_TYPE("PATHKEY");

	WRITE_NODE_FIELD(pk_eclass);
	WRITE_OID_FIELD(pk_opfamily);
	WRITE_INT_FIELD(pk_strategy);
	WRITE_BOOL_FIELD(pk_nulls_first);
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
	WRITE_BITMAPSET_FIELD(nullable_relids);
	WRITE_BITMAPSET_FIELD(left_relids);
	WRITE_BITMAPSET_FIELD(right_relids);
	WRITE_NODE_FIELD(orclause);
	/* don't write parent_ec, leads to infinite recursion in plan tree dump */
	WRITE_FLOAT_FIELD(this_selec, "%.4f");
	WRITE_NODE_FIELD(mergeopfamilies);
	/* don't write left_ec, leads to infinite recursion in plan tree dump */
	/* don't write right_ec, leads to infinite recursion in plan tree dump */
	WRITE_NODE_FIELD(left_em);
	WRITE_NODE_FIELD(right_em);
	WRITE_BOOL_FIELD(outer_is_left);
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
_outSpecialJoinInfo(StringInfo str, SpecialJoinInfo *node)
{
	WRITE_NODE_TYPE("SPECIALJOININFO");
	
	WRITE_BITMAPSET_FIELD(min_lefthand);
	WRITE_BITMAPSET_FIELD(min_righthand);
	WRITE_BITMAPSET_FIELD(syn_lefthand);
	WRITE_BITMAPSET_FIELD(syn_righthand);
	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_BOOL_FIELD(lhs_strict);
	WRITE_BOOL_FIELD(delay_upper_joins);
	WRITE_NODE_FIELD(join_quals);
	WRITE_BOOL_FIELD(try_join_unique);	/*CDB*/
	WRITE_BOOL_FIELD(consider_dedup);	/*CDB*/
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

static void
_outPlannerParamItem(StringInfo str, PlannerParamItem *node)
{
	WRITE_NODE_TYPE("PLANNERPARAMITEM");

	WRITE_NODE_FIELD(item);
	WRITE_UINT_FIELD(abslevel);
}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

#ifndef COMPILING_BINARY_FUNCS
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
	WRITE_NODE_FIELD(partitionBy);
	WRITE_CHAR_FIELD(relKind);
	WRITE_CHAR_FIELD(relStorage);
	/* policy omitted */
	/* postCreate omitted */
	WRITE_NODE_FIELD(deferredStmts);
	WRITE_BOOL_FIELD(is_part_child);
	WRITE_BOOL_FIELD(is_add_part);
	WRITE_BOOL_FIELD(is_split_part);
	WRITE_OID_FIELD(ownerid);
	WRITE_BOOL_FIELD(buildAoBlkdir);
	WRITE_NODE_FIELD(attr_encodings);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outColumnReferenceStorageDirective(StringInfo str, ColumnReferenceStorageDirective *node)
{
	WRITE_NODE_TYPE("COLUMNREFERENCESTORAGEDIRECTIVE");
	
	WRITE_STRING_FIELD(column);
	WRITE_BOOL_FIELD(deflt);
	WRITE_NODE_FIELD(encoding);
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
	WRITE_NODE_FIELD(extOptions);
	WRITE_NODE_FIELD(encoding);
	WRITE_NODE_FIELD(distributedBy);
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
	WRITE_BOOL_FIELD(is_part_child);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(primary);
	WRITE_BOOL_FIELD(isconstraint);
	WRITE_STRING_FIELD(altconname);
	WRITE_BOOL_FIELD(concurrent);
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
}

static void
_outRuleStmt(StringInfo str, RuleStmt *node)
{
	WRITE_NODE_TYPE("RULESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(rulename);
	WRITE_NODE_FIELD(whereClause);
	WRITE_ENUM_FIELD(event, CmdType);
	WRITE_BOOL_FIELD(instead);
	WRITE_NODE_FIELD(actions);
	WRITE_BOOL_FIELD(replace);
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
	WRITE_STRING_FIELD(newrole);
}

static void
_outTruncateStmt(StringInfo str, TruncateStmt *node)
{
	WRITE_NODE_TYPE("TRUNCATESTMT");

	WRITE_NODE_FIELD(relations);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outAlterTableStmt(StringInfo str, AlterTableStmt *node)
{
	WRITE_NODE_TYPE("ALTERTABLESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(cmds);
	WRITE_ENUM_FIELD(relkind, ObjectType);
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
_outSetDistributionCmd(StringInfo str, SetDistributionCmd*node)
{
	WRITE_NODE_TYPE("SETDISTRIBUTIONCMD");

	WRITE_INT_FIELD(backendId);
	WRITE_NODE_FIELD(relids);
	WRITE_NODE_FIELD(indexOidMap);
	WRITE_NODE_FIELD(hiddenTypes);
}

static void
_outInheritPartitionCmd(StringInfo str, InheritPartitionCmd *node)
{
	WRITE_NODE_TYPE("INHERITPARTITION");

	WRITE_NODE_FIELD(parent);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterPartitionCmd(StringInfo str, AlterPartitionCmd *node)
{
	WRITE_NODE_TYPE("ALTERPARTITIONCMD");

	WRITE_NODE_FIELD(partid);
	WRITE_NODE_FIELD(arg1);
	WRITE_NODE_FIELD(arg2);
}
#endif /* COMPILING_BINARY_FUNCS */

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
	WRITE_NODE_FIELD(setstmt);
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
}

static void
_outCreatedbStmt(StringInfo str, CreatedbStmt *node)
{
	WRITE_NODE_TYPE("CREATEDBSTMT");
	WRITE_STRING_FIELD(dbname);
	WRITE_NODE_FIELD(options);
}

static void
_outDropdbStmt(StringInfo str, DropdbStmt *node)
{
	WRITE_NODE_TYPE("DROPDBSTMT");
	WRITE_STRING_FIELD(dbname);
	WRITE_BOOL_FIELD(missing_ok);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCreateDomainStmt(StringInfo str, CreateDomainStmt *node)
{
	WRITE_NODE_TYPE("CREATEDOMAINSTMT");
	WRITE_NODE_FIELD(domainname);
	WRITE_NODE_FIELD_AS(typeName, typename);
	WRITE_NODE_FIELD(constraints);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterDomainStmt(StringInfo str, AlterDomainStmt *node)
{
	WRITE_NODE_TYPE("ALTERDOMAINSTMT");
	WRITE_CHAR_FIELD(subtype);
	WRITE_NODE_FIELD_AS(typeName, typename);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(def);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}
#endif /* COMPILING_BINARY_FUNCS */

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
}

static void
_outFunctionParameter(StringInfo str, FunctionParameter *node)
{
	WRITE_NODE_TYPE("FUNCTIONPARAMETER");
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(argType);
	WRITE_ENUM_FIELD(mode, FunctionParameterMode);
	WRITE_NODE_FIELD(defexpr);
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

#ifndef COMPILING_BINARY_FUNCS
static void
_outPartitionSpec(StringInfo str, PartitionSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONSPEC");
	WRITE_NODE_FIELD(partElem);
	WRITE_NODE_FIELD(subSpec);
	WRITE_BOOL_FIELD(istemplate);
	WRITE_INT_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPartitionElem(StringInfo str, PartitionElem *node)
{
	WRITE_NODE_TYPE("PARTITIONELEM");
	WRITE_STRING_FIELD(partName);
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

#ifndef COMPILING_BINARY_FUNCS
static void
_outPartitionBoundSpec(StringInfo str, PartitionBoundSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONBOUNDSPEC");
	WRITE_NODE_FIELD(partStart);
	WRITE_NODE_FIELD(partEnd);
	WRITE_NODE_FIELD(partEvery);
	WRITE_NODE_FIELD(everyGenList);
	WRITE_STRING_FIELD(pWithTnameStr);
	WRITE_INT_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPartitionValuesSpec(StringInfo str, PartitionValuesSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONVALUESSPEC");
	WRITE_NODE_FIELD(partValues);
	WRITE_INT_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outInhRelation(StringInfo str, InhRelation *node)
{
	WRITE_NODE_TYPE("INHRELATION");
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(options);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outPartition(StringInfo str, Partition *node)
{
	int i;

	WRITE_NODE_TYPE("PARTITION");

	WRITE_OID_FIELD(partid);
	WRITE_OID_FIELD(parrelid);
	WRITE_CHAR_FIELD(parkind);
	WRITE_INT_FIELD(parlevel);
	WRITE_BOOL_FIELD(paristemplate);
	WRITE_INT_FIELD(parnatts);
	appendStringInfoLiteral(str, " :paratts");
	for (i = 0; i < node->parnatts; i++)
		appendStringInfo(str, " %i", node->paratts[i]);

	appendStringInfoLiteral(str, " :parclass");
	for (i = 0; i < node->parnatts; i++)
		appendStringInfo(str, " %d", node->parclass[i]);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outPartitionRule(StringInfo str, PartitionRule *node)
{
	WRITE_NODE_TYPE("PARTITIONRULE");

	WRITE_OID_FIELD(parruleid);
	WRITE_OID_FIELD(paroid);
	WRITE_OID_FIELD(parchildrelid);
	WRITE_OID_FIELD(parparentoid);
	WRITE_STRING_FIELD(parname);
	WRITE_NODE_FIELD(parrangestart);
	WRITE_BOOL_FIELD(parrangestartincl);
	WRITE_NODE_FIELD(parrangeend);
	WRITE_BOOL_FIELD(parrangeendincl);
	WRITE_NODE_FIELD(parrangeevery);
	WRITE_NODE_FIELD(parlistvalues);
	WRITE_INT_FIELD(parruleord);
	WRITE_NODE_FIELD(parreloptions);
	WRITE_OID_FIELD(partemplatespaceId);
	WRITE_NODE_FIELD(children);
}
#endif /* COMPILING_BINARY_FUNCS */

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
_outDefineStmt(StringInfo str, DefineStmt *node)
{
	WRITE_NODE_TYPE("DEFINESTMT");
	WRITE_ENUM_FIELD(kind, ObjectType);
	WRITE_BOOL_FIELD(oldstyle);
	WRITE_NODE_FIELD(defnames);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(definition);
	WRITE_BOOL_FIELD(ordered);  /* CDB */
	WRITE_BOOL_FIELD(trusted);  /* CDB */
}

static void
_outCompositeTypeStmt(StringInfo str, CompositeTypeStmt *node)
{
	WRITE_NODE_TYPE("COMPTYPESTMT");

	WRITE_NODE_FIELD(typevar);
	WRITE_NODE_FIELD(coldeflist);
}

static void
_outCreateEnumStmt(StringInfo str, CreateEnumStmt *node)
{
	WRITE_NODE_TYPE("CREATEENUMSTMT");

	WRITE_NODE_FIELD(typeName);
	WRITE_NODE_FIELD(vals);
}

static void
_outCreateCastStmt(StringInfo str, CreateCastStmt *node)
{
	WRITE_NODE_TYPE("CREATECAST");
	WRITE_NODE_FIELD(sourcetype);
	WRITE_NODE_FIELD(targettype);
	WRITE_NODE_FIELD(func);
	WRITE_ENUM_FIELD(context, CoercionContext);
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
	WRITE_NODE_FIELD(opfamilyname);
	WRITE_STRING_FIELD(amname);
	WRITE_NODE_FIELD(datatype);
	WRITE_NODE_FIELD(items);
	WRITE_BOOL_FIELD(isDefault);
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
_outCreateOpFamilyStmt(StringInfo str, CreateOpFamilyStmt *node)
{
	WRITE_NODE_TYPE("CREATEOPFAMILY");
	WRITE_NODE_FIELD(opfamilyname);
	WRITE_STRING_FIELD(amname);
}

static void
_outAlterOpFamilyStmt(StringInfo str, AlterOpFamilyStmt *node)
{
	WRITE_NODE_TYPE("ALTEROPFAMILY");
	WRITE_NODE_FIELD(opfamilyname);
	WRITE_STRING_FIELD(amname);
	WRITE_BOOL_FIELD(isDrop);
	WRITE_NODE_FIELD(items);
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
_outRemoveOpFamilyStmt(StringInfo str, RemoveOpFamilyStmt *node)
{
	WRITE_NODE_TYPE("REMOVEOPFAMILY");
	WRITE_NODE_FIELD(opfamilyname);
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
	WRITE_INT_FIELD(rejectlimit);
	WRITE_BOOL_FIELD(is_limit_in_rows);
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

/*
 * SelectStmt's are never written to the catalog, they only exist
 * between parse and parseTransform.  The only use of this function
 * is for debugging purposes.
 *
 * In GPDB, these are also dispatched from QD to QEs, so we need full
 * out/read support.
 */
static void
_outSelectStmt(StringInfo str, SelectStmt *node)
{
	WRITE_NODE_TYPE("SELECT");

	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(intoClause);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(fromClause);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(havingClause);
	WRITE_NODE_FIELD(windowClause);
	WRITE_NODE_FIELD(valuesLists);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(scatterClause);
	WRITE_NODE_FIELD(withClause);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(lockingClause);
	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(distributedBy);
}

static void
_outInsertStmt(StringInfo str, InsertStmt *node)
{
	WRITE_NODE_TYPE("INSERT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(cols);
	WRITE_NODE_FIELD(selectStmt);
	WRITE_NODE_FIELD(returningList);
}

static void
_outDeleteStmt(StringInfo str, DeleteStmt *node)
{
	WRITE_NODE_TYPE("DELETE");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(usingClause);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(returningList);
}

static void
_outUpdateStmt(StringInfo str, UpdateStmt *node)
{
	WRITE_NODE_TYPE("UPDATE");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(returningList);
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
_outXmlSerialize(StringInfo str, XmlSerialize *node)
{
	WRITE_NODE_TYPE("XMLSERIALIZE");

	WRITE_ENUM_FIELD(xmloption, XmlOptionType);
	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(typeName);
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
_outPartSelectedExpr(StringInfo str, PartSelectedExpr *node)
{
	WRITE_NODE_TYPE("PARTSELECTEDEXPR");

	WRITE_INT_FIELD(dynamicScanId);
	WRITE_OID_FIELD(partOid);
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
	WRITE_NODE_TYPE("PARTBOUNDINCLUSIONEXPR");

	WRITE_INT_FIELD(level);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartBoundOpenExpr(StringInfo str, PartBoundOpenExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDOPENEXPR");

	WRITE_INT_FIELD(level);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartListRuleExpr(StringInfo str, PartListRuleExpr *node)
{
	WRITE_NODE_TYPE("PARTLISTRULEEXPR");

	WRITE_INT_FIELD(level);
	WRITE_OID_FIELD(resulttype);
	WRITE_OID_FIELD(elementtype);
}

static void
_outPartListNullTestExpr(StringInfo str, PartListNullTestExpr *node)
{
	WRITE_NODE_TYPE("PARTLISTNULLTESTEXPR");

	WRITE_INT_FIELD(level);
	WRITE_ENUM_FIELD(nulltesttype, NullTestType);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outColumnDef(StringInfo str, ColumnDef *node)
{
	WRITE_NODE_TYPE("COLUMNDEF");

	WRITE_STRING_FIELD(colname);
	WRITE_NODE_FIELD_AS(typeName, typename);
	WRITE_INT_FIELD(inhcount);
	WRITE_BOOL_FIELD(is_local);
	WRITE_BOOL_FIELD(is_not_null);
	WRITE_INT_FIELD(attnum);
	WRITE_NODE_FIELD(raw_default);
	WRITE_STRING_FIELD(cooked_default);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(encoding);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outTypeName(StringInfo str, TypeName *node)
{
	WRITE_NODE_TYPE("TYPENAME");

	WRITE_NODE_FIELD(names);
	WRITE_OID_FIELD_AS(typid, typeid);
	WRITE_BOOL_FIELD(timezone);
	WRITE_BOOL_FIELD(setof);
	WRITE_BOOL_FIELD(pct_type);
	WRITE_NODE_FIELD(typmods);
	WRITE_INT_FIELD(typemod);
	WRITE_NODE_FIELD(arrayBounds);
	WRITE_INT_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outTypeCast(StringInfo str, TypeCast *node)
{
	WRITE_NODE_TYPE("TYPECAST");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD_AS(typeName, typename);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outIndexElem(StringInfo str, IndexElem *node)
{
	WRITE_NODE_TYPE("INDEXELEM");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(opclass);
	WRITE_ENUM_FIELD(ordering, SortByDir);
	WRITE_ENUM_FIELD(nulls_ordering, SortByNulls);
}

static void
_outVariableSetStmt(StringInfo str, VariableSetStmt *node)
{
	WRITE_NODE_TYPE("VARIABLESETSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_ENUM_FIELD(kind, VariableSetKind);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(is_local);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outQuery(StringInfo str, Query *node)
{
	WRITE_NODE_TYPE("QUERY");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(querySource, QuerySource);
	WRITE_BOOL_FIELD(canSetTag);

	/*
	 * Hack to work around missing outfuncs routines for a lot of the
	 * utility-statement node types.  (The only one we actually *need* for
	 * rules support is NotifyStmt.)  Someday we ought to support 'em all, but
	 * for the meantime do this to avoid getting lots of warnings when running
	 * with debug_print_parse on.
	 */
	if (node->utilityStmt)
	{
		switch (nodeTag(node->utilityStmt))
		{
			case T_CreateStmt:
			case T_CreateExternalStmt:
			case T_DropStmt:
			case T_DropPropertyStmt:
			case T_TruncateStmt:
			case T_AlterTableStmt:
			case T_AlterTableCmd:
			case T_SetDistributionCmd:
			case T_ViewStmt:
			case T_RuleStmt:

			case T_CreateRoleStmt:
			case T_AlterRoleStmt:
			case T_AlterRoleSetStmt:
			case T_DropRoleStmt:

			case T_CreateSchemaStmt:
			case T_CreatePLangStmt:
			case T_DropPLangStmt:
			case T_AlterOwnerStmt:
			case T_AlterObjectSchemaStmt:

			case T_CreateFileSpaceStmt:
			case T_CreateTableSpaceStmt:

			case T_RenameStmt:
			case T_IndexStmt:
			case T_NotifyStmt:
			case T_DeclareCursorStmt:
			case T_VacuumStmt:
			case T_CreateSeqStmt:
			case T_AlterSeqStmt:
			case T_CreatedbStmt:
			case T_AlterDatabaseSetStmt:
			case T_DropdbStmt:
			case T_CreateDomainStmt:
			case T_AlterDomainStmt:
			case T_ClusterStmt:

			case T_CreateFunctionStmt:
			case T_RemoveFuncStmt:
			case T_AlterFunctionStmt:

			case T_TransactionStmt:
			case T_GrantStmt:
			case T_GrantRoleStmt:
			case T_LockStmt:
			case T_CopyStmt:
			case T_ReindexStmt:
			case T_ConstraintsSetStmt:
			case T_VariableSetStmt:
			case T_CreateTrigStmt:
			case T_DefineStmt:
			case T_CompositeTypeStmt:
			case T_CreateCastStmt:
			case T_DropCastStmt:
			case T_CreateOpClassStmt:
			case T_CreateOpClassItem:
			case T_RemoveOpClassStmt:
			case T_CreateConversionStmt:
				WRITE_NODE_FIELD(utilityStmt);
				break;
			default:
				appendStringInfoLiteral(str, " :utilityStmt ?");
				appendStringInfo(str, "%u", nodeTag(node->utilityStmt));
				break;
		}
	}
	else
		appendStringInfoLiteral(str, " :utilityStmt <>");

	WRITE_INT_FIELD(resultRelation);
	WRITE_NODE_FIELD(intoClause);
	WRITE_BOOL_FIELD(hasAggs);
	WRITE_BOOL_FIELD(hasWindFuncs);
	WRITE_BOOL_FIELD(hasSubLinks);
	WRITE_BOOL_FIELD(hasDynamicFunctions);
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
	/* Don't serialize policy */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outSortClause(StringInfo str, SortClause *node)
{
	WRITE_NODE_TYPE("SORTCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
	WRITE_BOOL_FIELD(nulls_first);
}

static void
_outGroupClause(StringInfo str, GroupClause *node)
{
	WRITE_NODE_TYPE("GROUPCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
	WRITE_BOOL_FIELD(nulls_first);
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
_outGrouping(StringInfo str, Grouping *node __attribute__((unused)))
{
	WRITE_NODE_TYPE("GROUPING");
}

static void
_outGroupId(StringInfo str, GroupId *node __attribute__((unused)))
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

#ifndef COMPILING_BINARY_FUNCS
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
		case RTE_JOIN:
			WRITE_ENUM_FIELD(jointype, JoinType);
			WRITE_NODE_FIELD(joinaliasvars);
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
			if (node->funcuserdata)
			{
				appendStringInfoLiteral(str, " :funcuserdata ");
				WRITE_BYTEA_FIELD(funcuserdata);
			}
			break;
		case RTE_VALUES:
			WRITE_NODE_FIELD(values_lists);
			break;
		case RTE_CTE:
			WRITE_STRING_FIELD(ctename);
			WRITE_UINT_FIELD(ctelevelsup);
			WRITE_BOOL_FIELD(self_reference);
			WRITE_NODE_FIELD(ctecoltypes);
			WRITE_NODE_FIELD(ctecoltypmods);
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
	/*
	 * pseudocols is intentionally not serialized. It's only used in the planning
	 * stage, so no need to transfer it to the QEs.
	 */
    WRITE_NODE_FIELD(pseudocols);                                       /*CDB*/
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outAExpr(StringInfo str, A_Expr *node)
{
	WRITE_NODE_TYPE("AEXPR");

	switch (node->kind)
	{
		case AEXPR_OP:
			appendStringInfoLiteral(str, " OPER ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_AND:
			appendStringInfoLiteral(str, " AND ");
			break;
		case AEXPR_OR:
			appendStringInfoLiteral(str, " OR ");
			break;
		case AEXPR_NOT:
			appendStringInfoLiteral(str, " NOT ");
			break;
		case AEXPR_OP_ANY:
			appendStringInfoLiteral(str, " ANY ");
			WRITE_NODE_FIELD(name);

			break;
		case AEXPR_OP_ALL:
			appendStringInfoLiteral(str, " ALL ");
			WRITE_NODE_FIELD(name);

			break;
		case AEXPR_DISTINCT:
			appendStringInfoLiteral(str, " DISTINCT ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NULLIF:
			appendStringInfoLiteral(str, " NULLIF ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_OF:
			appendStringInfoLiteral(str, " OF ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_IN:
			appendStringInfo(str, " IN ");
			WRITE_NODE_FIELD(name);
			break;
		default:
			appendStringInfoLiteral(str, " ??");
			break;
	}

	WRITE_NODE_FIELD(lexpr);
	WRITE_NODE_FIELD(rexpr);
	WRITE_INT_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outValue(StringInfo str, Value *value)
{
	switch (value->type)
	{
		case T_Integer:
			appendStringInfo(str, "%ld", value->val.ival);
			break;
		case T_Float:

			/*
			 * We assume the value is a valid numeric literal and so does not
			 * need quoting.
			 */
			appendStringInfoString(str, value->val.str);
			break;
		case T_String:
			appendStringInfoChar(str, '"');
			_outToken(str, value->val.str);
			appendStringInfoChar(str, '"');
			break;
		case T_BitString:
			/* internal representation already has leading 'b' */
			appendStringInfoString(str, value->val.str);
			break;
		case T_Null:
			/* this is seen only within A_Const, not in transformed trees */
			appendStringInfoString(str, "NULL");
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) value->type);
			break;
	}
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outNull(StringInfo str, Node *n __attribute__((unused)))
{
	WRITE_NODE_TYPE("NULL");
}
#endif /* COMPILING_BINARY_FUNCS */

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

#ifndef COMPILING_BINARY_FUNCS
static void
_outAConst(StringInfo str, A_Const *node)
{
	WRITE_NODE_TYPE("A_CONST");

	appendStringInfoChar(str, ' ');

	_outValue(str, &(node->val));
	WRITE_NODE_FIELD_AS(typeName, typename);
    /*
     * CDB: For now we don't serialize the 'location' field, for compatibility
     * so stored constants can be read by pre-3.2 releases.  Anyway it's only
     * meaningful with the original source string, which isn't kept when a
     * view or rule definition is stored in the catalog.
     */
}
#endif /* COMPILING_BINARY_FUNCS */

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
_outA_ArrayExpr(StringInfo str, A_ArrayExpr *node)
{
	WRITE_NODE_TYPE("A_ARRAYEXPR");

	WRITE_NODE_FIELD(elements);
/*	WRITE_LOCATION_FIELD(location); */
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
_outSortBy(StringInfo str, SortBy *node)
{
	WRITE_NODE_TYPE("SORTBY");

	WRITE_INT_FIELD(sortby_dir);
	WRITE_INT_FIELD(sortby_nulls);
	WRITE_NODE_FIELD(useOp);
	WRITE_NODE_FIELD(node);
	WRITE_INT_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outRangeSubselect(StringInfo str, RangeSubselect *node)
{
	WRITE_NODE_TYPE("RANGESUBSELECT");

	WRITE_NODE_FIELD(subquery);
	WRITE_NODE_FIELD(alias);
}

static void
_outRangeFunction(StringInfo str, RangeFunction *node)
{
	WRITE_NODE_TYPE("RANGEFUNCTION");

	WRITE_NODE_FIELD(funccallnode);
	WRITE_NODE_FIELD(alias);
	WRITE_NODE_FIELD(coldeflist);
}
#endif

#ifndef COMPILING_BINARY_FUNCS
static void
_outConstraint(StringInfo str, Constraint *node)
{
	WRITE_NODE_TYPE("CONSTRAINT");

	WRITE_STRING_FIELD(name);

	appendStringInfoLiteral(str, " :contype ");
	switch (node->contype)
	{
		case CONSTR_PRIMARY:
			appendStringInfoLiteral(str, "PRIMARY_KEY");
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexspace);
			break;

		case CONSTR_UNIQUE:
			appendStringInfoLiteral(str, "UNIQUE");
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexspace);
			break;

		case CONSTR_CHECK:
			appendStringInfoLiteral(str, "CHECK");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_DEFAULT:
			appendStringInfoLiteral(str, "DEFAULT");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_NOTNULL:
			appendStringInfoLiteral(str, "NOT_NULL");
			break;

		default:
			appendStringInfoLiteral(str, "<unrecognized_constraint>");
			break;
	}
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outFkConstraint(StringInfo str, FkConstraint *node)
{
	WRITE_NODE_TYPE("FKCONSTRAINT");

	WRITE_STRING_FIELD(constr_name);
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
}

static void
_outCreatePLangStmt(StringInfo str, CreatePLangStmt *node)
{
	WRITE_NODE_TYPE("CREATEPLANGSTMT");

	WRITE_STRING_FIELD(plname);
	WRITE_NODE_FIELD(plhandler);
	WRITE_NODE_FIELD(plinline);
	WRITE_NODE_FIELD(plvalidator);
	WRITE_BOOL_FIELD(pltrusted);
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
	WRITE_INT_FIELD(parentIndex);
	WRITE_NODE_FIELD(children); /* List of int index */
	WRITE_ENUM_FIELD(gangType,GangType);
	WRITE_INT_FIELD(gangSize);
	WRITE_INT_FIELD(numGangMembersToBeActive);
	WRITE_BOOL_FIELD(directDispatch.isDirectDispatch);
	WRITE_NODE_FIELD(directDispatch.contentIds); /* List of int */
	WRITE_DUMMY_FIELD(primaryGang);
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
_outCursorPosInfo(StringInfo str, CursorPosInfo *node)
{
	WRITE_NODE_TYPE("CURSORPOSINFO");

	WRITE_STRING_FIELD(cursor_name);
	WRITE_INT_FIELD(gp_segment_id);
	WRITE_UINT_FIELD(ctid.ip_blkid.bi_hi);
	WRITE_UINT_FIELD(ctid.ip_blkid.bi_lo);
	WRITE_UINT_FIELD(ctid.ip_posid);
	WRITE_OID_FIELD(table_oid);
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
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCreateQueueStmt(StringInfo str, CreateQueueStmt *node)
{
	WRITE_NODE_TYPE("CREATEQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterQueueStmt(StringInfo str, AlterQueueStmt *node)
{
	WRITE_NODE_TYPE("ALTERQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outDropQueueStmt(StringInfo str, DropQueueStmt *node)
{
	WRITE_NODE_TYPE("DROPQUEUESTMT");

	WRITE_STRING_FIELD(queue);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCreateResourceGroupStmt(StringInfo str, CreateResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("CREATERESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outDropResourceGroupStmt(StringInfo str, DropResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("DROPRESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterResourceGroupStmt(StringInfo str, AlterResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("ALTERRESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */


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

	WRITE_NODE_FIELD(typeName);
	WRITE_NODE_FIELD(encoding);
}

static void
_outAlterExtensionStmt(StringInfo str, AlterExtensionStmt *node)
{
	WRITE_NODE_TYPE("ALTEREXTENSIONSTMT");

	WRITE_STRING_FIELD(extname);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterExtensionContentsStmt(StringInfo str, AlterExtensionContentsStmt *node)
{
	WRITE_NODE_TYPE("ALTEREXTENSIONCONTENTSSTMT");

	WRITE_STRING_FIELD(extname);
	WRITE_INT_FIELD(action);
	WRITE_ENUM_FIELD(objtype, ObjectType);
	WRITE_NODE_FIELD(objname);
	WRITE_NODE_FIELD(objargs);
}

static void
_outAlterTSConfigurationStmt(StringInfo str, AlterTSConfigurationStmt *node)
{
	WRITE_NODE_TYPE("ALTERTSCONFIGURATIONSTMT");

	WRITE_NODE_FIELD(cfgname);
	WRITE_NODE_FIELD(tokentype);
	WRITE_NODE_FIELD(dicts);
	WRITE_BOOL_FIELD(override);
	WRITE_BOOL_FIELD(replace);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outAlterTSDictionaryStmt(StringInfo str, AlterTSDictionaryStmt *node)
{
	WRITE_NODE_TYPE("ALTERTSDICTIONARYSTMT");

	WRITE_NODE_FIELD(dictname);
	WRITE_NODE_FIELD(options);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outTupleDescNode(StringInfo str, TupleDescNode *node)
{
	int			i;

	Assert(node->tuple->tdtypeid == RECORDOID);

	WRITE_NODE_TYPE("TUPLEDESCNODE");
	WRITE_INT_FIELD(natts);
	WRITE_INT_FIELD(tuple->natts);

	for (i = 0; i < node->tuple->natts; i++)
		appendBinaryStringInfo(str, node->tuple->attrs[i], ATTRIBUTE_FIXED_PART_SIZE);

	Assert(node->tuple->constr == NULL);

	WRITE_OID_FIELD(tuple->tdtypeid);
	WRITE_INT_FIELD(tuple->tdtypmod);
	WRITE_BOOL_FIELD(tuple->tdhasoid);
	WRITE_INT_FIELD(tuple->tdrefcount);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
/*
 * _outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
static void
_outNode(StringInfo str, void *obj)
{
	if (obj == NULL)
		appendStringInfoLiteral(str, "<>");
	else if (IsA(obj, List) ||IsA(obj, IntList) || IsA(obj, OidList))
		_outList(str, obj);
	else if (IsA(obj, Integer) ||
			 IsA(obj, Float) ||
			 IsA(obj, String) ||
			 IsA(obj, BitString))
	{
		/* nodeRead does not want to see { } around these! */
		_outValue(str, obj);
	}
	else
	{
		appendStringInfoChar(str, '{');
		switch (nodeTag(obj))
		{
			case T_PlannedStmt:
				_outPlannedStmt(str, obj);
				break;
			case T_QueryDispatchDesc:
				_outQueryDispatchDesc(str, obj);
				break;
			case T_OidAssignment:
				_outOidAssignment(str, obj);
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
			case T_RecursiveUnion:
				_outRecursiveUnion(str, obj);
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
				_outDynamicIndexScan(str,obj);
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
			case T_CteScan:
				_outCteScan(str, obj);
				break;
			case T_WorkTableScan:
				_outWorkTableScan(str, obj);
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
			case T_Hash:
				_outHash(str, obj);
				break;
			case T_SetOp:
				_outSetOp(str, obj);
				break;
			case T_Limit:
				_outLimit(str, obj);
				break;
			case T_PlanInvalItem:
				_outPlanInvalItem(str, obj);
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
			case T_CoerceViaIO:
				_outCoerceViaIO(str, obj);
				break;
			case T_ArrayCoerceExpr:
				_outArrayCoerceExpr(str, obj);
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
			case T_XmlExpr:
				_outXmlExpr(str, obj);
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
			case T_PlannerGlobal:
				_outPlannerGlobal(str, obj);
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
			case T_CdbRelColumnInfo:
				_outCdbRelColumnInfo(str, obj);
				break;
			case T_CdbRelDedupInfo:
				_outCdbRelDedupInfo(str, obj);
				break;
			case T_EquivalenceClass:
				_outEquivalenceClass(str, obj);
				break;
			case T_EquivalenceMember:
				_outEquivalenceMember(str, obj);
				break;
			case T_PathKey:
				_outPathKey(str, obj);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj);
				break;
			case T_InnerIndexscanInfo:
				_outInnerIndexscanInfo(str, obj);
				break;
			case T_SpecialJoinInfo:
				_outSpecialJoinInfo(str, obj);
				break;
			case T_AppendRelInfo:
				_outAppendRelInfo(str, obj);
				break;
			case T_PlannerParamItem:
				_outPlannerParamItem(str, obj);
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
			case T_PartitionValuesSpec:
				_outPartitionValuesSpec(str, obj);
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
			case T_PartitionBy:
				_outPartitionBy(str, obj);
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
			case T_CreateEnumStmt:
				_outCreateEnumStmt(str,obj);
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
			case T_CreateOpFamilyStmt:
				_outCreateOpFamilyStmt(str,obj);
				break;
			case T_AlterOpFamilyStmt:
				_outAlterOpFamilyStmt(str,obj);
				break;
			case T_RemoveOpClassStmt:
				_outRemoveOpClassStmt(str,obj);
				break;
			case T_RemoveOpFamilyStmt:
				_outRemoveOpFamilyStmt(str,obj);
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
			case T_SetDistributionCmd:
				_outSetDistributionCmd(str, obj);
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
			case T_SelectStmt:
				_outSelectStmt(str, obj);
				break;
			case T_InsertStmt:
				_outInsertStmt(str, obj);
				break;
			case T_DeleteStmt:
				_outDeleteStmt(str, obj);
				break;
			case T_UpdateStmt:
				_outUpdateStmt(str, obj);
				break;
			case T_Null:
				_outNull(str, obj);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj);
				break;
			case T_TypeName:
				_outTypeName(str, obj);
				break;
			case T_SortBy:
				_outSortBy(str, obj);
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
			case T_A_ArrayExpr:
				_outA_ArrayExpr(str, obj);
				break;
			case T_ResTarget:
				_outResTarget(str, obj);
				break;
			case T_RangeSubselect:
				_outRangeSubselect(str, obj);
				break;
			case T_RangeFunction:
				_outRangeFunction(str, obj);
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
			case T_InhRelation:
				_outInhRelation(str, obj);
				break;
			case T_LockingClause:
				_outLockingClause(str, obj);
				break;
			case T_XmlSerialize:
				_outXmlSerialize(str, obj);
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
			case T_CursorPosInfo:
				_outCursorPosInfo(str, obj);
				break;
			case T_VariableSetStmt:
				_outVariableSetStmt(str, obj);
				break;

			case T_DMLActionExpr:
				_outDMLActionExpr(str, obj);
				break;

			case T_PartOidExpr:
				_outPartOidExpr(str, obj);
				break;

			case T_PartSelectedExpr:
				_outPartSelectedExpr(str, obj);
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

			case T_PartListRuleExpr:
				_outPartListRuleExpr(str, obj);
				break;

			case T_PartListNullTestExpr:
				_outPartListNullTestExpr(str, obj);
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

			case T_CreateResourceGroupStmt:
				_outCreateResourceGroupStmt(str, obj);
				break;
			case T_DropResourceGroupStmt:
				_outDropResourceGroupStmt(str, obj);
				break;
			case T_AlterResourceGroupStmt:
				_outAlterResourceGroupStmt(str, obj);
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
			case T_AlterExtensionStmt:
				_outAlterExtensionStmt(str, obj);
				break;
			case T_AlterExtensionContentsStmt:
				_outAlterExtensionContentsStmt(str, obj);
				break;
			case T_TupleDescNode:
				_outTupleDescNode(str, obj);
				break;

			case T_AlterTSConfigurationStmt:
				_outAlterTSConfigurationStmt(str, obj);
				break;
			case T_AlterTSDictionaryStmt:
				_outAlterTSDictionaryStmt(str, obj);
				break;

			default:

				/*
				 * This should be an ERROR, but it's too useful to be able to
				 * dump structures that _outNode only understands part of.
				 */
				elog(WARNING, "could not dump unrecognized node type: %d",
					 (int) nodeTag(obj));
				break;
		}
		appendStringInfoChar(str, '}');
	}
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToString(void *obj)
{
	StringInfoData str;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfo(&str);
	_outNode(&str, obj);
	return str.data;
}

#endif /* COMPILING_BINARY_FUNCS */
