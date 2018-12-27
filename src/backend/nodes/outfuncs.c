/*-------------------------------------------------------------------------
 *
 * outfuncs.c
 *	  Output functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/outfuncs.c
 *
 * NOTES
 *	  Every node type that can appear in stored rules' parsetrees *must*
 *	  have an output function defined here (as well as an input function
 *	  in readfuncs.c).  For use in debugging, we also provide output
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
#include "utils/datum.h"
#include "cdb/cdbgang.h"


/*
 * outfuncs.c is compiled normally into outfuncs.o, but it's also
 * #included from outfast.c. When #included, outfast.c defines
 * COMPILING_BINARY_FUNCS, and provides replacements WRITE_* macros. See
 * comments at top of readfast.c.
 */
#ifndef COMPILING_BINARY_FUNCS

/*
 * Macros to simplify output of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.  Note that these
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

#endif /* COMPILING_BINARY_FUNCS */

#define booltostr(x)  ((x) ? "true" : "false")

#ifndef COMPILING_BINARY_FUNCS
static void _outNode(StringInfo str, const void *obj);

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
		appendStringInfoString(str, "<>");
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
_outList(StringInfo str, const List *node)
{
	const ListCell *lc;

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
 */
static void
_outBitmapset(StringInfo str, const Bitmapset *bms)
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
			appendStringInfoString(str, "0 [ ]");
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

static void _outPlanInfo(StringInfo str, const Plan *node);
static void outLogicalIndexInfo(StringInfo str, const LogicalIndexInfo *node);

/*
 *	Stuff from plannodes.h
 */

#ifndef COMPILING_BINARY_FUNCS
static void
_outPlannedStmt(StringInfo str, const PlannedStmt *node)
{
	WRITE_NODE_TYPE("PLANNEDSTMT");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(planGen, PlanGenerator);
	WRITE_UINT_FIELD(queryId);
	WRITE_BOOL_FIELD(hasReturning);
	WRITE_BOOL_FIELD(hasModifyingCTE);
	WRITE_BOOL_FIELD(canSetTag);
	WRITE_BOOL_FIELD(transientPlan);
	WRITE_BOOL_FIELD(oneoffPlan);
	WRITE_BOOL_FIELD(simplyUpdatable);
	WRITE_NODE_FIELD(planTree);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(utilityStmt);
	WRITE_NODE_FIELD(subplans);
	WRITE_BITMAPSET_FIELD(rewindPlanIDs);

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

	WRITE_NODE_FIELD(intoPolicy);

	WRITE_UINT64_FIELD(query_mem);
	WRITE_NODE_FIELD(intoClause);
	WRITE_NODE_FIELD(copyIntoClause);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outQueryDispatchDesc(StringInfo str, const QueryDispatchDesc *node)
{
	WRITE_NODE_TYPE("QUERYDISPATCHDESC");

	WRITE_STRING_FIELD(intoTableSpaceName);
	WRITE_NODE_FIELD(oidAssignments);
	WRITE_NODE_FIELD(sliceTable);
	WRITE_NODE_FIELD(cursorPositions);
	WRITE_BOOL_FIELD(useChangedAOOpts);
}

static void
_outOidAssignment(StringInfo str, const OidAssignment *node)
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
_outPlanInfo(StringInfo str, const Plan *node)
{
	WRITE_INT_FIELD(plan_node_id);

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
_outScanInfo(StringInfo str, const Scan *node)
{
	_outPlanInfo(str, (const Plan *) node);

	WRITE_UINT_FIELD(scanrelid);
}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_outJoinPlanInfo(StringInfo str, const Join *node)
{
	_outPlanInfo(str, (const Plan *) node);

	WRITE_BOOL_FIELD(prefetch_inner);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(joinqual);
}

static void
_outPlan(StringInfo str, const Plan *node)
{
	WRITE_NODE_TYPE("PLAN");

	_outPlanInfo(str, (const Plan *) node);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outResult(StringInfo str, const Result *node)
{
	int			i;

	WRITE_NODE_TYPE("RESULT");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(resconstantqual);

	WRITE_INT_FIELD(numHashFilterCols);
	appendStringInfoString(str, " :hashFilterColIdx");
	for (i = 0; i < node->numHashFilterCols; i++)
		appendStringInfo(str, " %d", node->hashFilterColIdx[i]);
}
#endif

static void
_outRepeat(StringInfo str, const Repeat *node)
{
	WRITE_NODE_TYPE("REPEAT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(repeatCountExpr);
	WRITE_UINT64_FIELD(grouping);
}

static void
_outModifyTable(StringInfo str, const ModifyTable *node)
{
	WRITE_NODE_TYPE("MODIFYTABLE");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_ENUM_FIELD(operation, CmdType);
	WRITE_BOOL_FIELD(canSetTag);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_INT_FIELD(resultRelIndex);
	WRITE_NODE_FIELD(plans);
	WRITE_NODE_FIELD(withCheckOptionLists);
	WRITE_NODE_FIELD(returningLists);
	WRITE_NODE_FIELD(fdwPrivLists);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_INT_FIELD(epqParam);
	WRITE_NODE_FIELD(action_col_idxes);
	WRITE_NODE_FIELD(ctid_col_idxes);
	WRITE_NODE_FIELD(oid_col_idxes);
}

static void
_outAppend(StringInfo str, const Append *node)
{
	WRITE_NODE_TYPE("APPEND");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(appendplans);
}

static void
_outSequence(StringInfo str, const Sequence *node)
{
	WRITE_NODE_TYPE("SEQUENCE");
	_outPlanInfo(str, (Plan *)node);
	WRITE_NODE_FIELD(subplans);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outMergeAppend(StringInfo str, const MergeAppend *node)
{
	int			i;

	WRITE_NODE_TYPE("MERGEAPPEND");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(mergeplans);

	WRITE_INT_FIELD(numCols);

	appendStringInfoString(str, " :sortColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfoString(str, " :sortOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	appendStringInfoString(str, " :collations");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->collations[i]);

	appendStringInfoString(str, " :nullsFirst");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %s", booltostr(node->nullsFirst[i]));
}
#endif

#ifndef COMPILING_BINARY_FUNCS
static void
_outRecursiveUnion(StringInfo str, const RecursiveUnion *node)
{
	int			i;

	WRITE_NODE_TYPE("RECURSIVEUNION");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_INT_FIELD(wtParam);
	WRITE_INT_FIELD(numCols);

	appendStringInfoString(str, " :dupColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->dupColIdx[i]);

	appendStringInfoString(str, " :dupOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->dupOperators[i]);

	WRITE_LONG_FIELD(numGroups);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outBitmapAnd(StringInfo str, const BitmapAnd *node)
{
	WRITE_NODE_TYPE("BITMAPAND");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(bitmapplans);
}

static void
_outBitmapOr(StringInfo str, const BitmapOr *node)
{
	WRITE_NODE_TYPE("BITMAPOR");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(bitmapplans);
}

static void
_outScan(StringInfo str, const Scan *node)
{
	WRITE_NODE_TYPE("SCAN");

	_outScanInfo(str, node);
}

static void
_outSeqScan(StringInfo str, const SeqScan *node)
{
	WRITE_NODE_TYPE("SEQSCAN");

	_outScanInfo(str, (const Scan *) node);
}

static void
_outDynamicSeqScan(StringInfo str, const DynamicSeqScan *node)
{
	WRITE_NODE_TYPE("DYNAMICSEQSCAN");

	_outScanInfo(str, (Scan *)node);
	WRITE_INT_FIELD(partIndex);
	WRITE_INT_FIELD(partIndexPrintable);
}

static void
_outExternalScan(StringInfo str, const ExternalScan *node)
{
	WRITE_NODE_TYPE("EXTERNALSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(uriList);
	WRITE_STRING_FIELD(fmtOptString);
	WRITE_CHAR_FIELD(fmtType);
	WRITE_BOOL_FIELD(isMasterOnly);
	WRITE_INT_FIELD(rejLimit);
	WRITE_BOOL_FIELD(rejLimitInRows);
	WRITE_BOOL_FIELD(logErrors);
	WRITE_INT_FIELD(encoding);
	WRITE_INT_FIELD(scancounter);
}

#ifndef COMPILING_BINARY_FUNCS
static void
outLogicalIndexInfo(StringInfo str, const LogicalIndexInfo *node)
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
outIndexScanFields(StringInfo str, const IndexScan *node)
{
	_outScanInfo(str, (const Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_NODE_FIELD(indexqual);
	WRITE_NODE_FIELD(indexqualorig);
	WRITE_NODE_FIELD(indexorderby);
	WRITE_NODE_FIELD(indexorderbyorig);
	WRITE_ENUM_FIELD(indexorderdir, ScanDirection);
}

static void
_outIndexScan(StringInfo str, const IndexScan *node)
{
	WRITE_NODE_TYPE("INDEXSCAN");

	outIndexScanFields(str, node);
}

static void
_outIndexOnlyScan(StringInfo str, const IndexOnlyScan *node)
{
	WRITE_NODE_TYPE("INDEXONLYSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_NODE_FIELD(indexqual);
	WRITE_NODE_FIELD(indexorderby);
	WRITE_NODE_FIELD(indextlist);
	WRITE_ENUM_FIELD(indexorderdir, ScanDirection);
}

static void
_outDynamicIndexScan(StringInfo str, const DynamicIndexScan *node)
{
	WRITE_NODE_TYPE("DYNAMICINDEXSCAN");

	outIndexScanFields(str, &node->indexscan);
	WRITE_INT_FIELD(partIndex);
	WRITE_INT_FIELD(partIndexPrintable);
	outLogicalIndexInfo(str, node->logicalIndexInfo);
}

static void
_outBitmapIndexScanFields(StringInfo str, const BitmapIndexScan *node)
{
	_outScanInfo(str, (Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_NODE_FIELD(indexqual);
	WRITE_NODE_FIELD(indexqualorig);
}

static void
_outBitmapIndexScan(StringInfo str, const BitmapIndexScan *node)
{
	WRITE_NODE_TYPE("BITMAPINDEXSCAN");

	_outBitmapIndexScanFields(str, node);
}

static void
_outDynamicBitmapIndexScan(StringInfo str, const DynamicBitmapIndexScan *node)
{
	WRITE_NODE_TYPE("DYNAMICBITMAPINDEXSCAN");

	_outBitmapIndexScanFields(str, &node->biscan);
	WRITE_INT_FIELD(partIndex);
	WRITE_INT_FIELD(partIndexPrintable);
	outLogicalIndexInfo(str, node->logicalIndexInfo);
}

static void
outBitmapHeapScanFields(StringInfo str, const BitmapHeapScan *node)
{
	_outScanInfo(str, (const Scan *) node);

	WRITE_NODE_FIELD(bitmapqualorig);
}

static void
_outBitmapHeapScan(StringInfo str, const BitmapHeapScan *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPSCAN");

	outBitmapHeapScanFields(str, node);
}

static void
_outDynamicBitmapHeapScan(StringInfo str, const DynamicBitmapHeapScan *node)
{
	WRITE_NODE_TYPE("DYNAMICBITMAPHEAPSCAN");

	outBitmapHeapScanFields(str, &node->bitmapheapscan);
	WRITE_INT_FIELD(partIndex);
	WRITE_INT_FIELD(partIndexPrintable);
}

static void
_outTidScan(StringInfo str, const TidScan *node)
{
	WRITE_NODE_TYPE("TIDSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_NODE_FIELD(tidquals);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outSubqueryScan(StringInfo str, const SubqueryScan *node)
{
	WRITE_NODE_TYPE("SUBQUERYSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_NODE_FIELD(subplan);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outFunctionScan(StringInfo str, const FunctionScan *node)
{
	WRITE_NODE_TYPE("FUNCTIONSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_NODE_FIELD(functions);
	WRITE_BOOL_FIELD(funcordinality);
}

static void
_outValuesScan(StringInfo str, const ValuesScan *node)
{
	WRITE_NODE_TYPE("VALUESSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_NODE_FIELD(values_lists);
}

static void
_outCteScan(StringInfo str, const CteScan *node)
{
	WRITE_NODE_TYPE("CTESCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_INT_FIELD(ctePlanId);
	WRITE_INT_FIELD(cteParam);
}

static void
_outWorkTableScan(StringInfo str, const WorkTableScan *node)
{
	WRITE_NODE_TYPE("WORKTABLESCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_INT_FIELD(wtParam);
}

static void
_outForeignScan(StringInfo str, const ForeignScan *node)
{
	WRITE_NODE_TYPE("FOREIGNSCAN");

	_outScanInfo(str, (const Scan *) node);

	WRITE_NODE_FIELD(fdw_exprs);
	WRITE_NODE_FIELD(fdw_private);
	WRITE_BOOL_FIELD(fsSystemCol);
}

static void
_outJoin(StringInfo str, const Join *node)
{
	WRITE_NODE_TYPE("JOIN");

	_outJoinPlanInfo(str, (const Join *) node);
}

static void
_outNestLoop(StringInfo str, const NestLoop *node)
{
	WRITE_NODE_TYPE("NESTLOOP");

	_outJoinPlanInfo(str, (const Join *) node);

	WRITE_NODE_FIELD(nestParams);

	WRITE_BOOL_FIELD(shared_outer);
	WRITE_BOOL_FIELD(singleton_outer); /*CDB-OLAP*/
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outMergeJoin(StringInfo str, const MergeJoin *node)
{
	int			numCols;
	int			i;

	WRITE_NODE_TYPE("MERGEJOIN");

	_outJoinPlanInfo(str, (const Join *) node);

	WRITE_NODE_FIELD(mergeclauses);

	numCols = list_length(node->mergeclauses);

	appendStringInfoString(str, " :mergeFamilies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %u", node->mergeFamilies[i]);

	appendStringInfoString(str, " :mergeCollations");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %u", node->mergeCollations[i]);

	appendStringInfoString(str, " :mergeStrategies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", node->mergeStrategies[i]);

	appendStringInfoString(str, " :mergeNullsFirst");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", (int) node->mergeNullsFirst[i]);

	WRITE_BOOL_FIELD(unique_outer);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outHashJoin(StringInfo str, const HashJoin *node)
{
	WRITE_NODE_TYPE("HASHJOIN");

	_outJoinPlanInfo(str, (const Join *) node);

	WRITE_NODE_FIELD(hashclauses);
	WRITE_NODE_FIELD(hashqualclauses);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAgg(StringInfo str, const Agg *node)
{
	int			i;

	WRITE_NODE_TYPE("AGG");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_ENUM_FIELD(aggstrategy, AggStrategy);
	WRITE_INT_FIELD(numCols);

	appendStringInfoString(str, " :grpColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->grpColIdx[i]);

	appendStringInfoString(str, " :grpOperators");
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
	WRITE_BITMAPSET_FIELD(aggParams);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outWindowAgg(StringInfo str, const WindowAgg *node)
{
	int			i;

	WRITE_NODE_TYPE("WINDOWAGG");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_UINT_FIELD(winref);
	WRITE_INT_FIELD(partNumCols);

	appendStringInfoString(str, " :partColIdx");
	for (i = 0; i < node->partNumCols; i++)
		appendStringInfo(str, " %d", node->partColIdx[i]);

	appendStringInfoString(str, " :partOperations");
	for (i = 0; i < node->partNumCols; i++)
		appendStringInfo(str, " %u", node->partOperators[i]);

	WRITE_INT_FIELD(ordNumCols);

	appendStringInfoString(str, " :ordColIdx");
	for (i = 0; i < node->ordNumCols; i++)
		appendStringInfo(str, " %d", node->ordColIdx[i]);

	appendStringInfoString(str, " :ordOperations");
	for (i = 0; i < node->ordNumCols; i++)
		appendStringInfo(str, " %u", node->ordOperators[i]);

	WRITE_INT_FIELD(firstOrderCol);
	WRITE_OID_FIELD(firstOrderCmpOperator);
	WRITE_BOOL_FIELD(firstOrderNullsFirst);

	WRITE_INT_FIELD(frameOptions);
	WRITE_NODE_FIELD(startOffset);
	WRITE_NODE_FIELD(endOffset);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outTableFunctionScan(StringInfo str, const TableFunctionScan *node)
{
	WRITE_NODE_TYPE("TABLEFUNCTIONSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(function);
}

static void
_outMaterial(StringInfo str, const Material *node)
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
_outShareInputScan(StringInfo str, const ShareInputScan *node)
{
	WRITE_NODE_TYPE("SHAREINPUTSCAN");

	WRITE_ENUM_FIELD(share_type, ShareType);
	WRITE_INT_FIELD(share_id);
	WRITE_INT_FIELD(driver_slice);

	_outPlanInfo(str, (Plan *) node);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outSort(StringInfo str, const Sort *node)
{
	int			i;

	WRITE_NODE_TYPE("SORT");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfoString(str, " :sortColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfoString(str, " :sortOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	appendStringInfoString(str, " :collations");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->collations[i]);

	appendStringInfoString(str, " :nullsFirst");
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
_outUnique(StringInfo str, const Unique *node)
{
	int			i;

	WRITE_NODE_TYPE("UNIQUE");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfoString(str, " :uniqColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->uniqColIdx[i]);

	appendStringInfoString(str, " :uniqOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->uniqOperators[i]);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outHash(StringInfo str, const Hash *node)
{
	WRITE_NODE_TYPE("HASH");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_OID_FIELD(skewTable);
	WRITE_INT_FIELD(skewColumn);
	WRITE_BOOL_FIELD(skewInherit);
	WRITE_OID_FIELD(skewColType);
	WRITE_INT_FIELD(skewColTypmod);

	WRITE_BOOL_FIELD(rescannable);          /*CDB*/
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outSetOp(StringInfo str, const SetOp *node)
{
	int			i;

	WRITE_NODE_TYPE("SETOP");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_ENUM_FIELD(cmd, SetOpCmd);
	WRITE_ENUM_FIELD(strategy, SetOpStrategy);
	WRITE_INT_FIELD(numCols);

	appendStringInfoString(str, " :dupColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->dupColIdx[i]);

	appendStringInfoString(str, " :dupOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->dupOperators[i]);

	WRITE_INT_FIELD(flagColIdx);
	WRITE_INT_FIELD(firstFlag);
	WRITE_LONG_FIELD(numGroups);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outLockRows(StringInfo str, const LockRows *node)
{
	WRITE_NODE_TYPE("LOCKROWS");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(rowMarks);
	WRITE_INT_FIELD(epqParam);
}

static void
_outLimit(StringInfo str, const Limit *node)
{
	WRITE_NODE_TYPE("LIMIT");

	_outPlanInfo(str, (const Plan *) node);

	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
}

static void
_outNestLoopParam(StringInfo str, const NestLoopParam *node)
{
	WRITE_NODE_TYPE("NESTLOOPPARAM");

	WRITE_INT_FIELD(paramno);
	WRITE_NODE_FIELD(paramval);
}

static void
_outPlanRowMark(StringInfo str, const PlanRowMark *node)
{
	WRITE_NODE_TYPE("PLANROWMARK");

	WRITE_UINT_FIELD(rti);
	WRITE_UINT_FIELD(prti);
	WRITE_UINT_FIELD(rowmarkId);
	WRITE_ENUM_FIELD(markType, RowMarkType);
	WRITE_BOOL_FIELD(noWait);
	WRITE_BOOL_FIELD(isParent);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outPlanInvalItem(StringInfo str, const PlanInvalItem *node)
{
	WRITE_NODE_TYPE("PLANINVALITEM");

	WRITE_INT_FIELD(cacheId);
	WRITE_UINT_FIELD(hashValue);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outMotion(StringInfo str, const Motion *node)
{
	int i;

	WRITE_NODE_TYPE("MOTION");

	WRITE_INT_FIELD(motionID);
	WRITE_ENUM_FIELD(motionType, MotionType);

	WRITE_BOOL_FIELD(sendSorted);

	WRITE_NODE_FIELD(hashExpr);
	WRITE_NODE_FIELD(hashDataTypes);

	WRITE_INT_FIELD(isBroadcast);

	WRITE_INT_FIELD(numSortCols);
	appendStringInfoLiteral(str, " :sortColIdx");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfoLiteral(str, " :sortOperators");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	appendStringInfoLiteral(str, " :collations");
	for (i = 0; i < node->numSortCols; i++)
		appendStringInfo(str, " %u", node->collations[i]);

	WRITE_INT_FIELD(segidColIdx);

	_outPlanInfo(str, (Plan *) node);
}
#endif /* COMPILING_BINARY_FUNCS */

/*
 * _outDML
 */
static void
_outDML(StringInfo str, const DML *node)
{
	WRITE_NODE_TYPE("DML");

	WRITE_UINT_FIELD(scanrelid);
	WRITE_INT_FIELD(actionColIdx);
	WRITE_INT_FIELD(ctidColIdx);
	WRITE_INT_FIELD(tupleoidColIdx);

	_outPlanInfo(str, (Plan *) node);
}

/*
 * _outSplitUpdate
 */
static void
_outSplitUpdate(StringInfo str, const SplitUpdate *node)
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
 * _outReshuffle
 */
static void
_outReshuffle(StringInfo str, const Reshuffle *node)
{
	WRITE_NODE_TYPE("Reshuffle");

	WRITE_INT_FIELD(tupleSegIdx);
	WRITE_NODE_FIELD(policyAttrs);
	WRITE_INT_FIELD(oldSegs);
	WRITE_INT_FIELD(ptype);
	_outPlanInfo(str, (Plan *) node);
}

/*
 * _outRowTrigger
 */
static void
_outRowTrigger(StringInfo str, const RowTrigger *node)
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
_outAssertOp(StringInfo str, const AssertOp *node)
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
_outPartitionSelector(StringInfo str, const PartitionSelector *node)
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
_outAlias(StringInfo str, const Alias *node)
{
	WRITE_NODE_TYPE("ALIAS");

	WRITE_STRING_FIELD(aliasname);
	WRITE_NODE_FIELD(colnames);
}

static void
_outRangeVar(StringInfo str, const RangeVar *node)
{
	WRITE_NODE_TYPE("RANGEVAR");

	/*
	 * we deliberately ignore catalogname here, since it is presently not
	 * semantically meaningful
	 */
	WRITE_STRING_FIELD(catalogname);
	WRITE_STRING_FIELD(schemaname);
	WRITE_STRING_FIELD(relname);
	WRITE_ENUM_FIELD(inhOpt, InhOption);
	WRITE_CHAR_FIELD(relpersistence);
	WRITE_NODE_FIELD(alias);
	WRITE_LOCATION_FIELD(location);
}

static void
_outIntoClause(StringInfo str, const IntoClause *node)
{
	WRITE_NODE_TYPE("INTOCLAUSE");

	WRITE_NODE_FIELD(rel);
	WRITE_NODE_FIELD(colNames);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(onCommit, OnCommitAction);
	WRITE_STRING_FIELD(tableSpaceName);
	WRITE_NODE_FIELD(viewQuery);
	WRITE_BOOL_FIELD(skipData);
	WRITE_NODE_FIELD(distributedBy);
}

static void
_outCopyIntoClause(StringInfo str, const CopyIntoClause *node)
{
	WRITE_NODE_TYPE("COPYINTOCLAUSE");

	WRITE_NODE_FIELD(attlist);
	WRITE_BOOL_FIELD(is_program);
	WRITE_STRING_FIELD(filename);
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(ao_segnos);

}

static void
_outVar(StringInfo str, const Var *node)
{
	WRITE_NODE_TYPE("VAR");

	WRITE_UINT_FIELD(varno);
	WRITE_INT_FIELD(varattno);
	WRITE_OID_FIELD(vartype);
	WRITE_INT_FIELD(vartypmod);
	WRITE_OID_FIELD(varcollid);
	WRITE_UINT_FIELD(varlevelsup);
	WRITE_UINT_FIELD(varnoold);
	WRITE_INT_FIELD(varoattno);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outConst(StringInfo str, const Const *node)
{
	WRITE_NODE_TYPE("CONST");

	WRITE_OID_FIELD(consttype);
	WRITE_INT_FIELD(consttypmod);
	WRITE_OID_FIELD(constcollid);
	WRITE_INT_FIELD(constlen);
	WRITE_BOOL_FIELD(constbyval);
	WRITE_BOOL_FIELD(constisnull);
	WRITE_LOCATION_FIELD(location);

	appendStringInfoString(str, " :constvalue ");
	if (node->constisnull)
		appendStringInfoString(str, "<>");
	else
		_outDatum(str, node->constvalue, node->constlen, node->constbyval);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outParam(StringInfo str, const Param *node)
{
	WRITE_NODE_TYPE("PARAM");

	WRITE_ENUM_FIELD(paramkind, ParamKind);
	WRITE_INT_FIELD(paramid);
	WRITE_OID_FIELD(paramtype);
	WRITE_INT_FIELD(paramtypmod);
	WRITE_OID_FIELD(paramcollid);
	WRITE_LOCATION_FIELD(location);
}

static void
_outAggref(StringInfo str, const Aggref *node)
{
	WRITE_NODE_TYPE("AGGREF");

	WRITE_OID_FIELD(aggfnoid);
	WRITE_OID_FIELD(aggtype);
	WRITE_OID_FIELD(aggcollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(aggdirectargs);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(aggorder);
	WRITE_NODE_FIELD(aggdistinct);
	WRITE_NODE_FIELD(aggfilter);
	WRITE_BOOL_FIELD(aggstar);
	WRITE_BOOL_FIELD(aggvariadic);
	WRITE_CHAR_FIELD(aggkind);
	WRITE_UINT_FIELD(agglevelsup);
	WRITE_ENUM_FIELD(aggstage, AggStage);
	WRITE_LOCATION_FIELD(location);
}

static void
_outWindowFunc(StringInfo str, const WindowFunc *node)
{
	WRITE_NODE_TYPE("WINDOWFUNC");

	WRITE_OID_FIELD(winfnoid);
	WRITE_OID_FIELD(wintype);
	WRITE_OID_FIELD(wincollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(aggfilter);
	WRITE_UINT_FIELD(winref);
	WRITE_BOOL_FIELD(winstar);
	WRITE_BOOL_FIELD(winagg);
	WRITE_BOOL_FIELD(windistinct);
	WRITE_LOCATION_FIELD(location);
}

static void
_outArrayRef(StringInfo str, const ArrayRef *node)
{
	WRITE_NODE_TYPE("ARRAYREF");

	WRITE_OID_FIELD(refarraytype);
	WRITE_OID_FIELD(refelemtype);
	WRITE_INT_FIELD(reftypmod);
	WRITE_OID_FIELD(refcollid);
	WRITE_NODE_FIELD(refupperindexpr);
	WRITE_NODE_FIELD(reflowerindexpr);
	WRITE_NODE_FIELD(refexpr);
	WRITE_NODE_FIELD(refassgnexpr);
}

static void
_outFuncExpr(StringInfo str, const FuncExpr *node)
{
	WRITE_NODE_TYPE("FUNCEXPR");

	WRITE_OID_FIELD(funcid);
	WRITE_OID_FIELD(funcresulttype);
	WRITE_BOOL_FIELD(funcretset);
	WRITE_BOOL_FIELD(funcvariadic);
	WRITE_ENUM_FIELD(funcformat, CoercionForm);
	WRITE_OID_FIELD(funccollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(is_tablefunc);  /* GPDB */
	WRITE_LOCATION_FIELD(location);
}

static void
_outNamedArgExpr(StringInfo str, const NamedArgExpr *node)
{
	WRITE_NODE_TYPE("NAMEDARGEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_STRING_FIELD(name);
	WRITE_INT_FIELD(argnumber);
	WRITE_LOCATION_FIELD(location);
}

static void
_outOpExpr(StringInfo str, const OpExpr *node)
{
	WRITE_NODE_TYPE("OPEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_OID_FIELD(opcollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

static void
_outDistinctExpr(StringInfo str, const DistinctExpr *node)
{
	WRITE_NODE_TYPE("DISTINCTEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_OID_FIELD(opcollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

static void
_outNullIfExpr(StringInfo str, const NullIfExpr *node)
{
	WRITE_NODE_TYPE("NULLIFEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_OID_FIELD(opcollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

static void
_outScalarArrayOpExpr(StringInfo str, const ScalarArrayOpExpr *node)
{
	WRITE_NODE_TYPE("SCALARARRAYOPEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_BOOL_FIELD(useOr);
	WRITE_OID_FIELD(inputcollid);
	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outBoolExpr(StringInfo str, const BoolExpr *node)
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
	appendStringInfoString(str, " :boolop ");
	_outToken(str, opstr);

	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outSubLink(StringInfo str, const SubLink *node)
{
	WRITE_NODE_TYPE("SUBLINK");

	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(operName);
	WRITE_NODE_FIELD(subselect);
	WRITE_LOCATION_FIELD(location);
}

static void
_outSubPlan(StringInfo str, const SubPlan *node)
{
	WRITE_NODE_TYPE("SUBPLAN");

    WRITE_INT_FIELD(qDispSliceId);  /*CDB*/
	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(paramIds);
	WRITE_INT_FIELD(plan_id);
	WRITE_STRING_FIELD(plan_name);
	WRITE_OID_FIELD(firstColType);
	WRITE_INT_FIELD(firstColTypmod);
	WRITE_OID_FIELD(firstColCollation);
	WRITE_BOOL_FIELD(useHashTable);
	WRITE_BOOL_FIELD(unknownEqFalse);
	WRITE_BOOL_FIELD(is_initplan); /*CDB*/
	WRITE_BOOL_FIELD(is_multirow); /*CDB*/
	WRITE_NODE_FIELD(setParam);
	WRITE_NODE_FIELD(parParam);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(extParam);
	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(per_call_cost, "%.2f");
	WRITE_BOOL_FIELD(initPlanParallel); /*CDB*/
}

static void
_outAlternativeSubPlan(StringInfo str, const AlternativeSubPlan *node)
{
	WRITE_NODE_TYPE("ALTERNATIVESUBPLAN");

	WRITE_NODE_FIELD(subplans);
}

static void
_outFieldSelect(StringInfo str, const FieldSelect *node)
{
	WRITE_NODE_TYPE("FIELDSELECT");

	WRITE_NODE_FIELD(arg);
	WRITE_INT_FIELD(fieldnum);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_OID_FIELD(resultcollid);
}

static void
_outFieldStore(StringInfo str, const FieldStore *node)
{
	WRITE_NODE_TYPE("FIELDSTORE");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(newvals);
	WRITE_NODE_FIELD(fieldnums);
	WRITE_OID_FIELD(resulttype);
}

static void
_outRelabelType(StringInfo str, const RelabelType *node)
{
	WRITE_NODE_TYPE("RELABELTYPE");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_OID_FIELD(resultcollid);
	WRITE_ENUM_FIELD(relabelformat, CoercionForm);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCoerceViaIO(StringInfo str, const CoerceViaIO *node)
{
	WRITE_NODE_TYPE("COERCEVIAIO");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_OID_FIELD(resultcollid);
	WRITE_ENUM_FIELD(coerceformat, CoercionForm);
	WRITE_LOCATION_FIELD(location);
}

static void
_outArrayCoerceExpr(StringInfo str, const ArrayCoerceExpr *node)
{
	WRITE_NODE_TYPE("ARRAYCOERCEEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(elemfuncid);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_OID_FIELD(resultcollid);
	WRITE_BOOL_FIELD(isExplicit);
	WRITE_ENUM_FIELD(coerceformat, CoercionForm);
	WRITE_LOCATION_FIELD(location);
}

static void
_outConvertRowtypeExpr(StringInfo str, const ConvertRowtypeExpr *node)
{
	WRITE_NODE_TYPE("CONVERTROWTYPEEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_ENUM_FIELD(convertformat, CoercionForm);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCollateExpr(StringInfo str, const CollateExpr *node)
{
	WRITE_NODE_TYPE("COLLATE");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(collOid);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCaseExpr(StringInfo str, const CaseExpr *node)
{
	WRITE_NODE_TYPE("CASE");

	WRITE_OID_FIELD(casetype);
	WRITE_OID_FIELD(casecollid);
	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(defresult);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCaseWhen(StringInfo str, const CaseWhen *node)
{
	WRITE_NODE_TYPE("WHEN");

	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(result);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCaseTestExpr(StringInfo str, const CaseTestExpr *node)
{
	WRITE_NODE_TYPE("CASETESTEXPR");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
	WRITE_OID_FIELD(collation);
}

static void
_outArrayExpr(StringInfo str, const ArrayExpr *node)
{
	WRITE_NODE_TYPE("ARRAY");

	WRITE_OID_FIELD(array_typeid);
	WRITE_OID_FIELD(array_collid);
	WRITE_OID_FIELD(element_typeid);
	WRITE_NODE_FIELD(elements);
	WRITE_BOOL_FIELD(multidims);
	WRITE_LOCATION_FIELD(location);
}

static void
_outRowExpr(StringInfo str, const RowExpr *node)
{
	WRITE_NODE_TYPE("ROW");

	WRITE_NODE_FIELD(args);
	WRITE_OID_FIELD(row_typeid);
	WRITE_ENUM_FIELD(row_format, CoercionForm);
	WRITE_NODE_FIELD(colnames);
	WRITE_LOCATION_FIELD(location);
}

static void
_outRowCompareExpr(StringInfo str, const RowCompareExpr *node)
{
	WRITE_NODE_TYPE("ROWCOMPARE");

	WRITE_ENUM_FIELD(rctype, RowCompareType);
	WRITE_NODE_FIELD(opnos);
	WRITE_NODE_FIELD(opfamilies);
	WRITE_NODE_FIELD(inputcollids);
	WRITE_NODE_FIELD(largs);
	WRITE_NODE_FIELD(rargs);
}

static void
_outCoalesceExpr(StringInfo str, const CoalesceExpr *node)
{
	WRITE_NODE_TYPE("COALESCE");

	WRITE_OID_FIELD(coalescetype);
	WRITE_OID_FIELD(coalescecollid);
	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

static void
_outMinMaxExpr(StringInfo str, const MinMaxExpr *node)
{
	WRITE_NODE_TYPE("MINMAX");

	WRITE_OID_FIELD(minmaxtype);
	WRITE_OID_FIELD(minmaxcollid);
	WRITE_OID_FIELD(inputcollid);
	WRITE_ENUM_FIELD(op, MinMaxOp);
	WRITE_NODE_FIELD(args);
	WRITE_LOCATION_FIELD(location);
}

static void
_outXmlExpr(StringInfo str, const XmlExpr *node)
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
	WRITE_LOCATION_FIELD(location);
}

static void
_outNullTest(StringInfo str, const NullTest *node)
{
	WRITE_NODE_TYPE("NULLTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(nulltesttype, NullTestType);
	WRITE_BOOL_FIELD(argisrow);
}

static void
_outBooleanTest(StringInfo str, const BooleanTest *node)
{
	WRITE_NODE_TYPE("BOOLEANTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(booltesttype, BoolTestType);
}

static void
_outCoerceToDomain(StringInfo str, const CoerceToDomain *node)
{
	WRITE_NODE_TYPE("COERCETODOMAIN");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_OID_FIELD(resultcollid);
	WRITE_ENUM_FIELD(coercionformat, CoercionForm);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCoerceToDomainValue(StringInfo str, const CoerceToDomainValue *node)
{
	WRITE_NODE_TYPE("COERCETODOMAINVALUE");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
	WRITE_OID_FIELD(collation);
	WRITE_LOCATION_FIELD(location);
}

static void
_outSetToDefault(StringInfo str, const SetToDefault *node)
{
	WRITE_NODE_TYPE("SETTODEFAULT");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
	WRITE_OID_FIELD(collation);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCurrentOfExpr(StringInfo str, const CurrentOfExpr *node)
{
	WRITE_NODE_TYPE("CURRENTOFEXPR");

	WRITE_INT_FIELD(cvarno);
	WRITE_STRING_FIELD(cursor_name);
	WRITE_INT_FIELD(cursor_param);
	WRITE_OID_FIELD(target_relid);

	/* some attributes omitted as they're bound only just before executor dispatch */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outTargetEntry(StringInfo str, const TargetEntry *node)
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
_outRangeTblRef(StringInfo str, const RangeTblRef *node)
{
	WRITE_NODE_TYPE("RANGETBLREF");

	WRITE_INT_FIELD(rtindex);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outJoinExpr(StringInfo str, const JoinExpr *node)
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
#endif /* COMPILING_BINARY_FUNCS */

static void
_outFromExpr(StringInfo str, const FromExpr *node)
{
	WRITE_NODE_TYPE("FROMEXPR");

	WRITE_NODE_FIELD(fromlist);
	WRITE_NODE_FIELD(quals);
}

static void
_outFlow(StringInfo str, const Flow *node)
{
	WRITE_NODE_TYPE("FLOW");

	WRITE_ENUM_FIELD(flotype, FlowType);
	WRITE_ENUM_FIELD(req_move, Movement);
	WRITE_ENUM_FIELD(locustype, CdbLocusType);
	WRITE_INT_FIELD(segindex);
	WRITE_INT_FIELD(numsegments);

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
_outCdbPathLocus(StringInfo str, const CdbPathLocus *node)
{
	WRITE_ENUM_FIELD(locustype, CdbLocusType);
	WRITE_NODE_FIELD(distkey);
	WRITE_INT_FIELD(numsegments);
}                               /* _outCdbPathLocus */


/*****************************************************************************
 *
 *	Stuff from relation.h.
 *
 *****************************************************************************/

/*
 * print the basic stuff of all nodes that inherit from Path
 *
 * Note we do NOT print the parent, else we'd be in infinite recursion.
 * We can print the parent's relids for identification purposes, though.
 * We also do not print the whole of param_info, since it's printed by
 * _outRelOptInfo; it's sufficient and less cluttering to print just the
 * required outer relids.
 */
static void
_outPathInfo(StringInfo str, const Path *node)
{
	WRITE_ENUM_FIELD(pathtype, NodeTag);
	appendStringInfoString(str, " :parent_relids ");
	if (node->parent)
		_outBitmapset(str, node->parent->relids);
	else
		_outBitmapset(str, NULL);
	appendStringInfoString(str, " :required_outer ");
	if (node->param_info)
		_outBitmapset(str, node->param_info->ppi_req_outer);
	else
		_outBitmapset(str, NULL);
	WRITE_FLOAT_FIELD(rows, "%.0f");
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
_outJoinPathInfo(StringInfo str, const JoinPath *node)
{
	_outPathInfo(str, (const Path *) node);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(outerjoinpath);
	WRITE_NODE_FIELD(innerjoinpath);
	WRITE_NODE_FIELD(joinrestrictinfo);
}

static void
_outPath(StringInfo str, const Path *node)
{
	WRITE_NODE_TYPE("PATH");

	_outPathInfo(str, (const Path *) node);
}

static void
_outIndexPath(StringInfo str, const IndexPath *node)
{
	WRITE_NODE_TYPE("INDEXPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(indexinfo);
	WRITE_NODE_FIELD(indexclauses);
	WRITE_NODE_FIELD(indexquals);
	WRITE_NODE_FIELD(indexqualcols);
	WRITE_NODE_FIELD(indexorderbys);
	WRITE_NODE_FIELD(indexorderbycols);
	WRITE_ENUM_FIELD(indexscandir, ScanDirection);
	WRITE_FLOAT_FIELD(indextotalcost, "%.2f");
	WRITE_FLOAT_FIELD(indexselectivity, "%.4f");
    WRITE_INT_FIELD(num_leading_eq);
}

static void
_outBitmapHeapPath(StringInfo str, const BitmapHeapPath *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(bitmapqual);
}

static void
_outBitmapAndPath(StringInfo str, const BitmapAndPath *node)
{
	WRITE_NODE_TYPE("BITMAPANDPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(bitmapquals);
	WRITE_FLOAT_FIELD(bitmapselectivity, "%.4f");
}

static void
_outBitmapOrPath(StringInfo str, const BitmapOrPath *node)
{
	WRITE_NODE_TYPE("BITMAPORPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(bitmapquals);
	WRITE_FLOAT_FIELD(bitmapselectivity, "%.4f");
}

static void
_outTidPath(StringInfo str, const TidPath *node)
{
	WRITE_NODE_TYPE("TIDPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(tidquals);
}

static void
_outForeignPath(StringInfo str, const ForeignPath *node)
{
	WRITE_NODE_TYPE("FOREIGNPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(fdw_private);
}

static void
_outAppendPath(StringInfo str, const AppendPath *node)
{
	WRITE_NODE_TYPE("APPENDPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(subpaths);
}

static void
_outMergeAppendPath(StringInfo str, const MergeAppendPath *node)
{
	WRITE_NODE_TYPE("MERGEAPPENDPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(subpaths);
	WRITE_FLOAT_FIELD(limit_tuples, "%.0f");
}

static void
_outAppendOnlyPath(StringInfo str, const AppendOnlyPath *node)
{
	WRITE_NODE_TYPE("APPENDONLYPATH");

	_outPathInfo(str, (Path *) node);
}

static void
_outAOCSPath(StringInfo str, const AOCSPath *node)
{
	WRITE_NODE_TYPE("APPENDONLYPATH");

	_outPathInfo(str, (Path *) node);
}

static void
_outResultPath(StringInfo str, const ResultPath *node)
{
	WRITE_NODE_TYPE("RESULTPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(quals);
}

static void
_outMaterialPath(StringInfo str, const MaterialPath *node)
{
	WRITE_NODE_TYPE("MATERIALPATH");

	_outPathInfo(str, (const Path *) node);
    WRITE_BOOL_FIELD(cdb_strict);

	WRITE_NODE_FIELD(subpath);
}

static void
_outUniquePath(StringInfo str, const UniquePath *node)
{
	WRITE_NODE_TYPE("UNIQUEPATH");

	_outPathInfo(str, (const Path *) node);

	WRITE_NODE_FIELD(subpath);
	WRITE_ENUM_FIELD(umethod, UniquePathMethod);
	WRITE_NODE_FIELD(in_operators);
	WRITE_NODE_FIELD(uniq_exprs);

    WRITE_BOOL_FIELD(must_repartition);                 /*CDB*/
    WRITE_BITMAPSET_FIELD(distinct_on_rowid_relids);    /*CDB*/
}

static void
_outNestPath(StringInfo str, const NestPath *node)
{
	WRITE_NODE_TYPE("NESTPATH");

	_outJoinPathInfo(str, (const JoinPath *) node);
}

static void
_outMergePath(StringInfo str, const MergePath *node)
{
	WRITE_NODE_TYPE("MERGEPATH");

	_outJoinPathInfo(str, (const JoinPath *) node);

	WRITE_NODE_FIELD(path_mergeclauses);
	WRITE_NODE_FIELD(outersortkeys);
	WRITE_NODE_FIELD(innersortkeys);
	WRITE_BOOL_FIELD(materialize_inner);
}

static void
_outHashPath(StringInfo str, const HashPath *node)
{
	WRITE_NODE_TYPE("HASHPATH");

	_outJoinPathInfo(str, (const JoinPath *) node);

	WRITE_NODE_FIELD(path_hashclauses);
	WRITE_INT_FIELD(num_batches);
}

static void
_outCdbMotionPath(StringInfo str, const CdbMotionPath *node)
{
    WRITE_NODE_TYPE("MOTIONPATH");

    _outPathInfo(str, &node->path);

    WRITE_NODE_FIELD(subpath);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outPlannerGlobal(StringInfo str, const PlannerGlobal *node)
{
	WRITE_NODE_TYPE("PLANNERGLOBAL");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(subplans);
	WRITE_BITMAPSET_FIELD(rewindPlanIDs);
	WRITE_NODE_FIELD(finalrtable);
	WRITE_NODE_FIELD(finalrowmarks);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(relationOids);
	WRITE_NODE_FIELD(invalItems);
	WRITE_INT_FIELD(nParamExec);
	WRITE_UINT_FIELD(lastPHId);
	WRITE_UINT_FIELD(lastRowMarkId);
	WRITE_BOOL_FIELD(transientPlan);
	WRITE_BOOL_FIELD(oneoffPlan);
	WRITE_NODE_FIELD(share.motStack);
	WRITE_NODE_FIELD(share.qdShares);
	WRITE_NODE_FIELD(share.qdSlices);
	WRITE_INT_FIELD(share.nextPlanId);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPlannerInfo(StringInfo str, const PlannerInfo *node)
{
	WRITE_NODE_TYPE("PLANNERINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(parse);
	WRITE_NODE_FIELD(glob);
	WRITE_UINT_FIELD(query_level);
	WRITE_NODE_FIELD(plan_params);
	WRITE_BITMAPSET_FIELD(all_baserels);
	WRITE_BITMAPSET_FIELD(nullable_baserels);
	WRITE_NODE_FIELD(join_rel_list);
	WRITE_INT_FIELD(join_cur_level);
	WRITE_NODE_FIELD(init_plans);
	WRITE_NODE_FIELD(cte_plan_ids);
	WRITE_NODE_FIELD(eq_classes);
	WRITE_NODE_FIELD(canon_pathkeys);
	WRITE_NODE_FIELD(left_join_clauses);
	WRITE_NODE_FIELD(right_join_clauses);
	WRITE_NODE_FIELD(full_join_clauses);
	WRITE_NODE_FIELD(join_info_list);
	WRITE_NODE_FIELD(lateral_info_list);
	WRITE_NODE_FIELD(append_rel_list);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(placeholder_list);
	WRITE_NODE_FIELD(query_pathkeys);
	WRITE_NODE_FIELD(group_pathkeys);
	WRITE_NODE_FIELD(distinct_pathkeys);
	WRITE_NODE_FIELD(sort_pathkeys);
	WRITE_NODE_FIELD(minmax_aggs);
	WRITE_FLOAT_FIELD(total_table_pages, "%.0f");
	WRITE_FLOAT_FIELD(tuple_fraction, "%.4f");
	WRITE_FLOAT_FIELD(limit_tuples, "%.0f");
	WRITE_BOOL_FIELD(hasInheritedTarget);
	WRITE_BOOL_FIELD(hasJoinRTEs);
	WRITE_BOOL_FIELD(hasLateralRTEs);
	WRITE_BOOL_FIELD(hasHavingQual);
	WRITE_BOOL_FIELD(hasPseudoConstantQuals);
	WRITE_BOOL_FIELD(hasRecursion);
	WRITE_INT_FIELD(wt_param_id);
	WRITE_BITMAPSET_FIELD(curOuterRels);
	WRITE_NODE_FIELD(curOuterParams);
}

static void
_outRelOptInfo(StringInfo str, const RelOptInfo *node)
{
	WRITE_NODE_TYPE("RELOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_ENUM_FIELD(reloptkind, RelOptKind);
	WRITE_BITMAPSET_FIELD(relids);
	WRITE_FLOAT_FIELD(rows, "%.0f");
	WRITE_INT_FIELD(width);
	WRITE_BOOL_FIELD(consider_startup);
	WRITE_BOOL_FIELD(consider_param_startup);
	WRITE_NODE_FIELD(reltargetlist);
	/* Skip writing Path ptrs to avoid endless recursion */
	/* WRITE_NODE_FIELD(pathlist);              */
	/* WRITE_NODE_FIELD(cheapest_startup_path); */
	/* WRITE_NODE_FIELD(cheapest_total_path);   */
	WRITE_UINT_FIELD(relid);
	WRITE_UINT_FIELD(reltablespace);
	WRITE_ENUM_FIELD(rtekind, RTEKind);
	WRITE_INT_FIELD(min_attr);
	WRITE_INT_FIELD(max_attr);
	WRITE_NODE_FIELD(lateral_vars);
	WRITE_BITMAPSET_FIELD(lateral_relids);
	WRITE_BITMAPSET_FIELD(lateral_referencers);
	WRITE_NODE_FIELD(indexlist);
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_FLOAT_FIELD(allvisfrac, "%.6f");
	WRITE_NODE_FIELD(subplan);
	WRITE_NODE_FIELD(subroot);
	WRITE_NODE_FIELD(subplan_params);
	/* we don't try to print fdwroutine or fdw_private */
	WRITE_NODE_FIELD(baserestrictinfo);
	WRITE_NODE_FIELD(joininfo);
	WRITE_BOOL_FIELD(has_eclass_joins);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outIndexOptInfo(StringInfo str, const IndexOptInfo *node)
{
	WRITE_NODE_TYPE("INDEXOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_OID_FIELD(indexoid);
	/* Do NOT print rel field, else infinite recursion */
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_INT_FIELD(tree_height);
	WRITE_INT_FIELD(ncolumns);
	/* array fields aren't really worth the trouble to print */
	WRITE_OID_FIELD(relam);
	WRITE_OID_FIELD(amcostestimate);
	/* indexprs is redundant since we print indextlist */
	WRITE_NODE_FIELD(indpred);
	WRITE_NODE_FIELD(indextlist);
	WRITE_BOOL_FIELD(predOK);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(immediate);
	WRITE_BOOL_FIELD(hypothetical);
	/* we don't bother with fields copied from the pg_am entry */
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outEquivalenceClass(StringInfo str, const EquivalenceClass *node)
{
	/*
	 * To simplify reading, we just chase up to the topmost merged EC and
	 * print that, without bothering to show the merge-ees separately.
	 */
	while (node->ec_merged)
		node = node->ec_merged;

	WRITE_NODE_TYPE("EQUIVALENCECLASS");

	WRITE_NODE_FIELD(ec_opfamilies);
	WRITE_OID_FIELD(ec_collation);
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
_outEquivalenceMember(StringInfo str, const EquivalenceMember *node)
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
_outPathKey(StringInfo str, const PathKey *node)
{
	WRITE_NODE_TYPE("PATHKEY");

	WRITE_NODE_FIELD(pk_eclass);
	WRITE_OID_FIELD(pk_opfamily);
	WRITE_INT_FIELD(pk_strategy);
	WRITE_BOOL_FIELD(pk_nulls_first);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outDistributionKey(StringInfo str, const DistributionKey *node)
{
	WRITE_NODE_TYPE("DISTRIBUTIONKEY");

	WRITE_NODE_FIELD(dk_eclasses);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outParamPathInfo(StringInfo str, const ParamPathInfo *node)
{
	WRITE_NODE_TYPE("PARAMPATHINFO");

	WRITE_BITMAPSET_FIELD(ppi_req_outer);
	WRITE_FLOAT_FIELD(ppi_rows, "%.0f");
	WRITE_NODE_FIELD(ppi_clauses);
}

static void
_outRestrictInfo(StringInfo str, const RestrictInfo *node)
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
	WRITE_BITMAPSET_FIELD(outer_relids);
	WRITE_BITMAPSET_FIELD(nullable_relids);
	WRITE_BITMAPSET_FIELD(left_relids);
	WRITE_BITMAPSET_FIELD(right_relids);
	WRITE_NODE_FIELD(orclause);
	/* don't write parent_ec, leads to infinite recursion in plan tree dump */
	WRITE_FLOAT_FIELD(norm_selec, "%.4f");
	WRITE_FLOAT_FIELD(outer_selec, "%.4f");
	WRITE_NODE_FIELD(mergeopfamilies);
	/* don't write left_ec, leads to infinite recursion in plan tree dump */
	/* don't write right_ec, leads to infinite recursion in plan tree dump */
	WRITE_NODE_FIELD(left_em);
	WRITE_NODE_FIELD(right_em);
	WRITE_BOOL_FIELD(outer_is_left);
	WRITE_OID_FIELD(hashjoinoperator);
}

static void
_outPlaceHolderVar(StringInfo str, const PlaceHolderVar *node)
{
	WRITE_NODE_TYPE("PLACEHOLDERVAR");

	WRITE_NODE_FIELD(phexpr);
	WRITE_BITMAPSET_FIELD(phrels);
	WRITE_UINT_FIELD(phid);
	WRITE_UINT_FIELD(phlevelsup);
}

static void
_outSpecialJoinInfo(StringInfo str, const SpecialJoinInfo *node)
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
}

static void
_outLateralJoinInfo(StringInfo str, const LateralJoinInfo *node)
{
	WRITE_NODE_TYPE("LATERALJOININFO");

	WRITE_BITMAPSET_FIELD(lateral_lhs);
	WRITE_BITMAPSET_FIELD(lateral_rhs);
}

static void
_outAppendRelInfo(StringInfo str, const AppendRelInfo *node)
{
	WRITE_NODE_TYPE("APPENDRELINFO");

	WRITE_UINT_FIELD(parent_relid);
	WRITE_UINT_FIELD(child_relid);
	WRITE_OID_FIELD(parent_reltype);
	WRITE_OID_FIELD(child_reltype);
	WRITE_NODE_FIELD(translated_vars);
	WRITE_OID_FIELD(parent_reloid);
}

static void
_outPlaceHolderInfo(StringInfo str, const PlaceHolderInfo *node)
{
	WRITE_NODE_TYPE("PLACEHOLDERINFO");

	WRITE_UINT_FIELD(phid);
	WRITE_NODE_FIELD(ph_var);
	WRITE_BITMAPSET_FIELD(ph_eval_at);
	WRITE_BITMAPSET_FIELD(ph_lateral);
	WRITE_BITMAPSET_FIELD(ph_needed);
	WRITE_INT_FIELD(ph_width);
}

static void
_outMinMaxAggInfo(StringInfo str, const MinMaxAggInfo *node)
{
	WRITE_NODE_TYPE("MINMAXAGGINFO");

	WRITE_OID_FIELD(aggfnoid);
	WRITE_OID_FIELD(aggsortop);
	WRITE_NODE_FIELD(target);
	/* We intentionally omit subroot --- too large, not interesting enough */
	WRITE_NODE_FIELD(path);
	WRITE_FLOAT_FIELD(pathcost, "%.2f");
	WRITE_NODE_FIELD(param);
}

static void
_outPlannerParamItem(StringInfo str, const PlannerParamItem *node)
{
	WRITE_NODE_TYPE("PLANNERPARAMITEM");

	WRITE_NODE_FIELD(item);
	WRITE_INT_FIELD(paramId);
}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

#ifndef COMPILING_BINARY_FUNCS
/*
 * print the basic stuff of all nodes that inherit from CreateStmt
 */
static void
_outCreateStmtInfo(StringInfo str, const CreateStmt *node)
{
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(tableElts);
	WRITE_NODE_FIELD(inhRelations);
	WRITE_NODE_FIELD(inhOids);
	WRITE_INT_FIELD(parentOidCount);
	WRITE_NODE_FIELD(ofTypename);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(oncommit, OnCommitAction);
	WRITE_STRING_FIELD(tablespacename);
	WRITE_BOOL_FIELD(if_not_exists);

	WRITE_NODE_FIELD(distributedBy);
	WRITE_NODE_FIELD(partitionBy);
	WRITE_CHAR_FIELD(relKind);
	WRITE_CHAR_FIELD(relStorage);
	/* policy omitted */
	/* postCreate omitted */
	WRITE_NODE_FIELD(deferredStmts);
	WRITE_BOOL_FIELD(is_part_child);
	WRITE_BOOL_FIELD(is_part_parent);
	WRITE_BOOL_FIELD(is_add_part);
	WRITE_BOOL_FIELD(is_split_part);
	WRITE_OID_FIELD(ownerid);
	WRITE_BOOL_FIELD(buildAoBlkdir);
	WRITE_NODE_FIELD(attr_encodings);
}

static void
_outCreateStmt(StringInfo str, const CreateStmt *node)
{
	WRITE_NODE_TYPE("CREATESTMT");

	_outCreateStmtInfo(str, (const CreateStmt *) node);
}

static void
_outCreateForeignTableStmt(StringInfo str, const CreateForeignTableStmt *node)
{
	WRITE_NODE_TYPE("CREATEFOREIGNTABLESTMT");

	_outCreateStmtInfo(str, (const CreateStmt *) node);

	WRITE_STRING_FIELD(servername);
	WRITE_NODE_FIELD(options);
}

#endif /* COMPILING_BINARY_FUNCS */

static void
_outColumnReferenceStorageDirective(StringInfo str, const ColumnReferenceStorageDirective *node)
{
	WRITE_NODE_TYPE("COLUMNREFERENCESTORAGEDIRECTIVE");

	WRITE_STRING_FIELD(column);
	WRITE_BOOL_FIELD(deflt);
	WRITE_NODE_FIELD(encoding);
}

static void
_outExtTableTypeDesc(StringInfo str, const ExtTableTypeDesc *node)
{
	WRITE_NODE_TYPE("EXTTABLETYPEDESC");

	WRITE_ENUM_FIELD(exttabletype, ExtTableType);
	WRITE_NODE_FIELD(location_list);
	WRITE_NODE_FIELD(on_clause);
	WRITE_STRING_FIELD(command_string);
}

static void
_outCreateExternalStmt(StringInfo str, const CreateExternalStmt *node)
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
_outDistributedBy(StringInfo str, const DistributedBy *node)
{
	WRITE_NODE_TYPE("DISTRIBUTEDBY");

	WRITE_ENUM_FIELD(ptype, GpPolicyType);
	WRITE_INT_FIELD(numsegments);
	WRITE_NODE_FIELD(keys);
}


static void
_outIndexStmt(StringInfo str, const IndexStmt *node)
{
	WRITE_NODE_TYPE("INDEXSTMT");

	WRITE_STRING_FIELD(idxname);
	WRITE_NODE_FIELD(relation);
	WRITE_OID_FIELD(relationOid);
	WRITE_STRING_FIELD(accessMethod);
	WRITE_STRING_FIELD(tableSpace);
	WRITE_NODE_FIELD(indexParams);
	WRITE_NODE_FIELD(options);

	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(excludeOpNames);
	WRITE_STRING_FIELD(idxcomment);
	WRITE_OID_FIELD(indexOid);
	WRITE_OID_FIELD(oldNode);
	WRITE_BOOL_FIELD(is_part_child);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(primary);
	WRITE_BOOL_FIELD(isconstraint);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_BOOL_FIELD(concurrent);
	WRITE_STRING_FIELD(altconname);
	WRITE_BOOL_FIELD(is_split_part);
}

static void
_outReindexStmt(StringInfo str, const ReindexStmt *node)
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
_outViewStmt(StringInfo str, const ViewStmt *node)
{
	WRITE_NODE_TYPE("VIEWSTMT");

	WRITE_NODE_FIELD(view);
	WRITE_NODE_FIELD(aliases);
	WRITE_NODE_FIELD(query);
	WRITE_BOOL_FIELD(replace);
}

static void
_outRuleStmt(StringInfo str, const RuleStmt *node)
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
_outDropStmt(StringInfo str, const DropStmt *node)
{
	WRITE_NODE_TYPE("DROPSTMT");

	WRITE_NODE_FIELD(objects);
	WRITE_NODE_FIELD(arguments);
	WRITE_ENUM_FIELD(removeType, ObjectType);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
	WRITE_BOOL_FIELD(bAllowPartn);
	WRITE_BOOL_FIELD(concurrent);
}

static void
_outDropOwnedStmt(StringInfo str, const DropOwnedStmt *node)
{
	WRITE_NODE_TYPE("DROPOWNEDSTMT");

	WRITE_NODE_FIELD(roles);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outReassignOwnedStmt(StringInfo str, const ReassignOwnedStmt *node)
{
	WRITE_NODE_TYPE("REASSIGNOWNEDSTMT");

	WRITE_NODE_FIELD(roles);
	WRITE_STRING_FIELD(newrole);
}

static void
_outTruncateStmt(StringInfo str, const TruncateStmt *node)
{
	WRITE_NODE_TYPE("TRUNCATESTMT");

	WRITE_NODE_FIELD(relations);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outReplicaIdentityStmt(StringInfo str, const ReplicaIdentityStmt *node)
{
	WRITE_NODE_TYPE("REPLICAIDENTITYSTMT");

	WRITE_CHAR_FIELD(identity_type);
	WRITE_STRING_FIELD(name);
}

static void
_outAlterTableStmt(StringInfo str, const AlterTableStmt *node)
{
	WRITE_NODE_TYPE("ALTERTABLESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(cmds);
	WRITE_ENUM_FIELD(relkind, ObjectType);
}

static void
_outAlterTableCmd(StringInfo str, const AlterTableCmd *node)
{
	WRITE_NODE_TYPE("ALTERTABLECMD");

	WRITE_ENUM_FIELD(subtype, AlterTableType);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(def);
	WRITE_NODE_FIELD(transform);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(part_expanded);
	WRITE_NODE_FIELD(partoids);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outSetDistributionCmd(StringInfo str, const SetDistributionCmd *node)
{
	WRITE_NODE_TYPE("SETDISTRIBUTIONCMD");

	WRITE_INT_FIELD(backendId);
	WRITE_NODE_FIELD(relids);
}

static void
_outInheritPartitionCmd(StringInfo str, const InheritPartitionCmd *node)
{
	WRITE_NODE_TYPE("INHERITPARTITION");

	WRITE_NODE_FIELD(parent);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterPartitionCmd(StringInfo str, const AlterPartitionCmd *node)
{
	WRITE_NODE_TYPE("ALTERPARTITIONCMD");

	WRITE_NODE_FIELD(partid);
	WRITE_NODE_FIELD(arg1);
	WRITE_NODE_FIELD(arg2);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outAlterPartitionId(StringInfo str, const AlterPartitionId *node)
{
	WRITE_NODE_TYPE("ALTERPARTITIONID");

	WRITE_ENUM_FIELD(idtype, AlterPartitionIdType);
	WRITE_NODE_FIELD(partiddef);
}

static void
_outCreateRoleStmt(StringInfo str, const CreateRoleStmt *node)
{
	WRITE_NODE_TYPE("CREATEROLESTMT");

	WRITE_ENUM_FIELD(stmt_type, RoleStmtType);
	WRITE_STRING_FIELD(role);
	WRITE_NODE_FIELD(options);
}

static void
_outDenyLoginInterval(StringInfo str, const DenyLoginInterval *node)
{
	WRITE_NODE_TYPE("DENYLOGININTERVAL");

	WRITE_NODE_FIELD(start);
	WRITE_NODE_FIELD(end);
}

static void
_outDenyLoginPoint(StringInfo str, const DenyLoginPoint *node)
{
	WRITE_NODE_TYPE("DENYLOGINPOINT");

	WRITE_NODE_FIELD(day);
	WRITE_NODE_FIELD(time);
}

static  void
_outDropRoleStmt(StringInfo str, const DropRoleStmt *node)
{
	WRITE_NODE_TYPE("DROPROLESTMT");

	WRITE_NODE_FIELD(roles);
	WRITE_BOOL_FIELD(missing_ok);
}

static  void
_outAlterRoleStmt(StringInfo str, const AlterRoleStmt *node)
{
	WRITE_NODE_TYPE("ALTERROLESTMT");

	WRITE_STRING_FIELD(role);
	WRITE_NODE_FIELD(options);
	WRITE_INT_FIELD(action);
}

static  void
_outAlterRoleSetStmt(StringInfo str, const AlterRoleSetStmt *node)
{
	WRITE_NODE_TYPE("ALTERROLESETSTMT");

	WRITE_STRING_FIELD(role);
	WRITE_NODE_FIELD(setstmt);
}


static  void
_outAlterOwnerStmt(StringInfo str, const AlterOwnerStmt *node)
{
	WRITE_NODE_TYPE("ALTEROWNERSTMT");

	WRITE_ENUM_FIELD(objectType,ObjectType);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(objarg);
	WRITE_STRING_FIELD(newowner);
}


static void
_outRenameStmt(StringInfo str, const RenameStmt *node)
{
	WRITE_NODE_TYPE("RENAMESTMT");

	WRITE_ENUM_FIELD(renameType, ObjectType);
	WRITE_ENUM_FIELD(relationType, ObjectType);
	WRITE_NODE_FIELD(relation);
	WRITE_OID_FIELD(objid);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(objarg);
	WRITE_STRING_FIELD(subname);
	WRITE_STRING_FIELD(newname);
	WRITE_ENUM_FIELD(behavior,DropBehavior);

	WRITE_BOOL_FIELD(bAllowPartn);
	WRITE_BOOL_FIELD(missing_ok);
}

static void
_outAlterObjectSchemaStmt(StringInfo str, const AlterObjectSchemaStmt *node)
{
	WRITE_NODE_TYPE("ALTEROBJECTSCHEMASTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(object);
	WRITE_NODE_FIELD(objarg);
	WRITE_STRING_FIELD(newschema);
	WRITE_BOOL_FIELD(missing_ok);
	WRITE_ENUM_FIELD(objectType,ObjectType);
}

static void
_outCreateSeqStmt(StringInfo str, const CreateSeqStmt *node)
{
	WRITE_NODE_TYPE("CREATESEQSTMT");
	WRITE_NODE_FIELD(sequence);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterSeqStmt(StringInfo str, const AlterSeqStmt *node)
{
	WRITE_NODE_TYPE("ALTERSEQSTMT");
	WRITE_NODE_FIELD(sequence);
	WRITE_NODE_FIELD(options);
}

static void
_outClusterStmt(StringInfo str, const ClusterStmt *node)
{
	WRITE_NODE_TYPE("CLUSTERSTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(indexname);
}

static void
_outCreatedbStmt(StringInfo str, const CreatedbStmt *node)
{
	WRITE_NODE_TYPE("CREATEDBSTMT");
	WRITE_STRING_FIELD(dbname);
	WRITE_NODE_FIELD(options);
}

static void
_outDropdbStmt(StringInfo str, const DropdbStmt *node)
{
	WRITE_NODE_TYPE("DROPDBSTMT");
	WRITE_STRING_FIELD(dbname);
	WRITE_BOOL_FIELD(missing_ok);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCreateDomainStmt(StringInfo str, const CreateDomainStmt *node)
{
	WRITE_NODE_TYPE("CREATEDOMAINSTMT");
	WRITE_NODE_FIELD(domainname);
	WRITE_NODE_FIELD_AS(typeName, typename);
	WRITE_NODE_FIELD(constraints);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterDomainStmt(StringInfo str, const AlterDomainStmt *node)
{
	WRITE_NODE_TYPE("ALTERDOMAINSTMT");
	WRITE_CHAR_FIELD(subtype);
	WRITE_NODE_FIELD_AS(typeName, typename);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(def);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
	WRITE_BOOL_FIELD(missing_ok);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outCreateFunctionStmt(StringInfo str, const CreateFunctionStmt *node)
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
_outFunctionParameter(StringInfo str, const FunctionParameter *node)
{
	WRITE_NODE_TYPE("FUNCTIONPARAMETER");
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(argType);
	WRITE_ENUM_FIELD(mode, FunctionParameterMode);
	WRITE_NODE_FIELD(defexpr);
}

static void
_outAlterFunctionStmt(StringInfo str, const AlterFunctionStmt *node)
{
	WRITE_NODE_TYPE("ALTERFUNCTIONSTMT");
	WRITE_NODE_FIELD(func);
	WRITE_NODE_FIELD(actions);
}

static void
_outPartitionBy(StringInfo str, const PartitionBy *node)
{
	WRITE_NODE_TYPE("PARTITIONBY");
	WRITE_ENUM_FIELD(partType, PartitionByType);
	WRITE_NODE_FIELD(keys);
	WRITE_NODE_FIELD(keyopclass);
	WRITE_NODE_FIELD(subPart);
	WRITE_NODE_FIELD(partSpec);
	WRITE_INT_FIELD(partDepth);
	WRITE_INT_FIELD(partQuiet);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outPartitionSpec(StringInfo str, const PartitionSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONSPEC");
	WRITE_NODE_FIELD(partElem);
	WRITE_NODE_FIELD(subSpec);
	WRITE_BOOL_FIELD(istemplate);
	WRITE_LOCATION_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPartitionElem(StringInfo str, const PartitionElem *node)
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
	WRITE_LOCATION_FIELD(location);
}

static void
_outPartitionRangeItem(StringInfo str, const PartitionRangeItem *node)
{
	WRITE_NODE_TYPE("PARTITIONRANGEITEM");
	WRITE_NODE_FIELD(partRangeVal);
	WRITE_ENUM_FIELD(partedge, PartitionEdgeBounding);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outPartitionBoundSpec(StringInfo str, const PartitionBoundSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONBOUNDSPEC");
	WRITE_NODE_FIELD(partStart);
	WRITE_NODE_FIELD(partEnd);
	WRITE_NODE_FIELD(partEvery);
	WRITE_NODE_FIELD(everyGenList);
	WRITE_STRING_FIELD(pWithTnameStr);
	WRITE_LOCATION_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outPartitionValuesSpec(StringInfo str, const PartitionValuesSpec *node)
{
	WRITE_NODE_TYPE("PARTITIONVALUESSPEC");
	WRITE_NODE_FIELD(partValues);
	WRITE_LOCATION_FIELD(location);
}

static void
_outExpandStmtSpec(StringInfo str, const ExpandStmtSpec *node)
{
	WRITE_NODE_TYPE("EXPANDSTMTSPEC");
	WRITE_ENUM_FIELD(method, ExpandMethod);
	WRITE_BITMAPSET_FIELD(ps_none);
	WRITE_BITMAPSET_FIELD(ps_root);
	WRITE_BITMAPSET_FIELD(ps_interior);
	WRITE_BITMAPSET_FIELD(ps_leaf);
	WRITE_OID_FIELD(backendId);
}


#ifndef COMPILING_BINARY_FUNCS
static void
_outPartition(StringInfo str, const Partition *node)
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
_outPartitionRule(StringInfo str, const PartitionRule *node)
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
_outPartitionNode(StringInfo str, const PartitionNode *node)
{
	WRITE_NODE_TYPE("PARTITIONNODE");

	WRITE_NODE_FIELD(part);
	WRITE_NODE_FIELD(default_part);
	WRITE_NODE_FIELD(rules);
}

static void
_outPgPartRule(StringInfo str, const PgPartRule *node)
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
_outSegfileMapNode(StringInfo str, const SegfileMapNode *node)
{
	WRITE_NODE_TYPE("SEGFILEMAPNODE");

	WRITE_OID_FIELD(relid);
	WRITE_INT_FIELD(segno);
}


static void
_outDefineStmt(StringInfo str, const DefineStmt *node)
{
	WRITE_NODE_TYPE("DEFINESTMT");
	WRITE_ENUM_FIELD(kind, ObjectType);
	WRITE_BOOL_FIELD(oldstyle);
	WRITE_NODE_FIELD(defnames);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(definition);
	WRITE_BOOL_FIELD(trusted);  /* CDB */
}

static void
_outCompositeTypeStmt(StringInfo str, const CompositeTypeStmt *node)
{
	WRITE_NODE_TYPE("COMPTYPESTMT");

	WRITE_NODE_FIELD(typevar);
	WRITE_NODE_FIELD(coldeflist);
}

static void
_outCreateEnumStmt(StringInfo str, const CreateEnumStmt *node)
{
	WRITE_NODE_TYPE("CREATEENUMSTMT");

	WRITE_NODE_FIELD(typeName);
	WRITE_NODE_FIELD(vals);
}

static void
_outCreateRangeStmt(StringInfo str, const CreateRangeStmt *node)
{
	WRITE_NODE_TYPE("CREATERANGESTMT");

	WRITE_NODE_FIELD(typeName);
	WRITE_NODE_FIELD(params);
}

static void
_outCreateCastStmt(StringInfo str, const CreateCastStmt *node)
{
	WRITE_NODE_TYPE("CREATECAST");
	WRITE_NODE_FIELD(sourcetype);
	WRITE_NODE_FIELD(targettype);
	WRITE_NODE_FIELD(func);
	WRITE_ENUM_FIELD(context, CoercionContext);
	WRITE_BOOL_FIELD(inout);
}

static void
_outCreateOpClassStmt(StringInfo str, const CreateOpClassStmt *node)
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
_outCreateOpClassItem(StringInfo str, const CreateOpClassItem *node)
{
	WRITE_NODE_TYPE("CREATEOPCLASSITEM");
	WRITE_INT_FIELD(itemtype);
	WRITE_NODE_FIELD(name);
	WRITE_NODE_FIELD(args);
	WRITE_INT_FIELD(number);
	WRITE_NODE_FIELD(order_family);
	WRITE_NODE_FIELD(class_args);
	WRITE_NODE_FIELD(storedtype);
}

static void
_outCreateOpFamilyStmt(StringInfo str, const CreateOpFamilyStmt *node)
{
	WRITE_NODE_TYPE("CREATEOPFAMILY");
	WRITE_NODE_FIELD(opfamilyname);
	WRITE_STRING_FIELD(amname);
}

static void
_outAlterOpFamilyStmt(StringInfo str, const AlterOpFamilyStmt *node)
{
	WRITE_NODE_TYPE("ALTEROPFAMILY");
	WRITE_NODE_FIELD(opfamilyname);
	WRITE_STRING_FIELD(amname);
	WRITE_BOOL_FIELD(isDrop);
	WRITE_NODE_FIELD(items);
}

static void
_outCreateConversionStmt(StringInfo str, const CreateConversionStmt *node)
{
	WRITE_NODE_TYPE("CREATECONVERSION");
	WRITE_NODE_FIELD(conversion_name);
	WRITE_STRING_FIELD(for_encoding_name);
	WRITE_STRING_FIELD(to_encoding_name);
	WRITE_NODE_FIELD(func_name);
	WRITE_BOOL_FIELD(def);
}

static void
_outTransactionStmt(StringInfo str, const TransactionStmt *node)
{
	WRITE_NODE_TYPE("TRANSACTIONSTMT");

	WRITE_ENUM_FIELD(kind, TransactionStmtKind);
	WRITE_NODE_FIELD(options);
}

static void
_outNotifyStmt(StringInfo str, const NotifyStmt *node)
{
	WRITE_NODE_TYPE("NOTIFY");

	WRITE_STRING_FIELD(conditionname);
	WRITE_STRING_FIELD(payload);
}

static void
_outDeclareCursorStmt(StringInfo str, const DeclareCursorStmt *node)
{
	WRITE_NODE_TYPE("DECLARECURSOR");

	WRITE_STRING_FIELD(portalname);
	WRITE_INT_FIELD(options);
	WRITE_NODE_FIELD(query);
}

static void
_outSingleRowErrorDesc(StringInfo str, const SingleRowErrorDesc *node)
{
	WRITE_NODE_TYPE("SINGLEROWERRORDESC");
	WRITE_INT_FIELD(rejectlimit);
	WRITE_BOOL_FIELD(is_limit_in_rows);
	WRITE_BOOL_FIELD(into_file);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCopyStmt(StringInfo str, const CopyStmt *node)
{
	WRITE_NODE_TYPE("COPYSTMT");
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(attlist);
	WRITE_BOOL_FIELD(is_from);
	WRITE_BOOL_FIELD(is_program);
	WRITE_BOOL_FIELD(skip_ext_partition);
	WRITE_STRING_FIELD(filename);
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(sreh);
	WRITE_NODE_FIELD(partitions);
	WRITE_NODE_FIELD(ao_segnos);
}
#endif/* COMPILING_BINARY_FUNCS */


static void
_outGrantStmt(StringInfo str, const GrantStmt *node)
{
	WRITE_NODE_TYPE("GRANTSTMT");
	WRITE_BOOL_FIELD(is_grant);
	WRITE_ENUM_FIELD(targtype,GrantTargetType);
	WRITE_ENUM_FIELD(objtype,GrantObjectType);
	WRITE_NODE_FIELD(objects);
	WRITE_NODE_FIELD(privileges);
	WRITE_NODE_FIELD(grantees);
	WRITE_BOOL_FIELD(grant_option);
	WRITE_ENUM_FIELD(behavior, DropBehavior);
}

static void
_outPrivGrantee(StringInfo str, const PrivGrantee *node)
{
	WRITE_NODE_TYPE("PRIVGRANTEE");
	WRITE_STRING_FIELD(rolname);
}

static void
_outFuncWithArgs(StringInfo str, const FuncWithArgs *node)
{
	WRITE_NODE_TYPE("FUNCWITHARGS");
	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(funcargs);
}

static void
_outGrantRoleStmt(StringInfo str, const GrantRoleStmt *node)
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
_outLockStmt(StringInfo str, const LockStmt *node)
{
	WRITE_NODE_TYPE("LOCKSTMT");
	WRITE_NODE_FIELD(relations);
	WRITE_INT_FIELD(mode);
	WRITE_BOOL_FIELD(nowait);
}

static void
_outConstraintsSetStmt(StringInfo str, const ConstraintsSetStmt *node)
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
 *
 * If the Nodes Struct changed, we need to maintain these funtions.
 */
static void
_outSelectStmt(StringInfo str, const SelectStmt *node)
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
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(lockingClause);
	WRITE_NODE_FIELD(withClause);
	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
}

static void
_outInsertStmt(StringInfo str, const InsertStmt *node)
{
	WRITE_NODE_TYPE("INSERT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(cols);
	WRITE_NODE_FIELD(selectStmt);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(withClause);
}

static void
_outDeleteStmt(StringInfo str, const DeleteStmt *node)
{
	WRITE_NODE_TYPE("DELETE");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(usingClause);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(withClause);
}

static void
_outUpdateStmt(StringInfo str, const UpdateStmt *node)
{
	WRITE_NODE_TYPE("UPDATE");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(fromClause);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(withClause);
	WRITE_BOOL_FIELD(needReshuffle);
}

static void
_outFuncCall(StringInfo str, const FuncCall *node)
{
	WRITE_NODE_TYPE("FUNCCALL");

	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(agg_order);
	WRITE_NODE_FIELD(agg_filter);
	WRITE_BOOL_FIELD(agg_within_group);
	WRITE_BOOL_FIELD(agg_star);
	WRITE_BOOL_FIELD(agg_distinct);
	WRITE_BOOL_FIELD(func_variadic);
	WRITE_NODE_FIELD(over);
	WRITE_LOCATION_FIELD(location);
}

static void
_outDefElem(StringInfo str, const DefElem *node)
{
	WRITE_NODE_TYPE("DEFELEM");

	WRITE_STRING_FIELD(defnamespace);
	WRITE_STRING_FIELD(defname);
	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(defaction, DefElemAction);
}

static void
_outTableLikeClause(StringInfo str, const TableLikeClause *node)
{
	WRITE_NODE_TYPE("TABLELIKECLAUSE");

	WRITE_NODE_FIELD(relation);
	WRITE_UINT_FIELD(options);
}

static void
_outLockingClause(StringInfo str, const LockingClause *node)
{
	WRITE_NODE_TYPE("LOCKINGCLAUSE");

	WRITE_NODE_FIELD(lockedRels);
	WRITE_ENUM_FIELD(strength, LockClauseStrength);
	WRITE_BOOL_FIELD(noWait);
}

static void
_outXmlSerialize(StringInfo str, const XmlSerialize *node)
{
	WRITE_NODE_TYPE("XMLSERIALIZE");

	WRITE_ENUM_FIELD(xmloption, XmlOptionType);
	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(typeName);
	WRITE_LOCATION_FIELD(location);
}

static void
_outDMLActionExpr(StringInfo str, const DMLActionExpr *node)
{
	WRITE_NODE_TYPE("DMLACTIONEXPR");
}

static void
_outPartSelectedExpr(StringInfo str, const PartSelectedExpr *node)
{
	WRITE_NODE_TYPE("PARTSELECTEDEXPR");

	WRITE_INT_FIELD(dynamicScanId);
	WRITE_OID_FIELD(partOid);
}

static void
_outPartDefaultExpr(StringInfo str, const PartDefaultExpr *node)
{
	WRITE_NODE_TYPE("PARTDEFAULTEXPR");

	WRITE_INT_FIELD(level);
}

static void
_outPartBoundExpr(StringInfo str, const PartBoundExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDEXPR");

	WRITE_INT_FIELD(level);
	WRITE_OID_FIELD(boundType);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartBoundInclusionExpr(StringInfo str, const PartBoundInclusionExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDINCLUSIONEXPR");

	WRITE_INT_FIELD(level);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartBoundOpenExpr(StringInfo str, const PartBoundOpenExpr *node)
{
	WRITE_NODE_TYPE("PARTBOUNDOPENEXPR");

	WRITE_INT_FIELD(level);
	WRITE_BOOL_FIELD(isLowerBound);
}

static void
_outPartListRuleExpr(StringInfo str, const PartListRuleExpr *node)
{
	WRITE_NODE_TYPE("PARTLISTRULEEXPR");

	WRITE_INT_FIELD(level);
	WRITE_OID_FIELD(resulttype);
	WRITE_OID_FIELD(elementtype);
}

static void
_outPartListNullTestExpr(StringInfo str, const PartListNullTestExpr *node)
{
	WRITE_NODE_TYPE("PARTLISTNULLTESTEXPR");

	WRITE_INT_FIELD(level);
	WRITE_ENUM_FIELD(nulltesttype, NullTestType);
}

static void
_outColumnDef(StringInfo str, const ColumnDef *node)
{
	WRITE_NODE_TYPE("COLUMNDEF");

	WRITE_STRING_FIELD(colname);
	WRITE_NODE_FIELD(typeName);
	WRITE_INT_FIELD(inhcount);
	WRITE_BOOL_FIELD(is_local);
	WRITE_BOOL_FIELD(is_not_null);
	WRITE_BOOL_FIELD(is_from_type);
	WRITE_INT_FIELD(attnum);
	WRITE_INT_FIELD(storage);
	WRITE_NODE_FIELD(raw_default);
	WRITE_NODE_FIELD(cooked_default);
	WRITE_NODE_FIELD(collClause);
	WRITE_OID_FIELD(collOid);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(encoding);
	WRITE_NODE_FIELD(fdwoptions);
	WRITE_LOCATION_FIELD(location);
}

static void
_outTypeName(StringInfo str, const TypeName *node)
{
	WRITE_NODE_TYPE("TYPENAME");

	WRITE_NODE_FIELD(names);
	WRITE_OID_FIELD(typeOid);
	WRITE_BOOL_FIELD(setof);
	WRITE_BOOL_FIELD(pct_type);
	WRITE_NODE_FIELD(typmods);
	WRITE_INT_FIELD(typemod);
	WRITE_NODE_FIELD(arrayBounds);
	WRITE_LOCATION_FIELD(location);
}

static void
_outTypeCast(StringInfo str, const TypeCast *node)
{
	WRITE_NODE_TYPE("TYPECAST");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(typeName);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCollateClause(StringInfo str, const CollateClause *node)
{
	WRITE_NODE_TYPE("COLLATECLAUSE");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(collname);
	WRITE_LOCATION_FIELD(location);
}

static void
_outIndexElem(StringInfo str, const IndexElem *node)
{
	WRITE_NODE_TYPE("INDEXELEM");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(expr);
	WRITE_STRING_FIELD(indexcolname);
	WRITE_NODE_FIELD(collation);
	WRITE_NODE_FIELD(opclass);
	WRITE_ENUM_FIELD(ordering, SortByDir);
	WRITE_ENUM_FIELD(nulls_ordering, SortByNulls);
}

static void
_outVariableSetStmt(StringInfo str, const VariableSetStmt *node)
{
	WRITE_NODE_TYPE("VARIABLESETSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_ENUM_FIELD(kind, VariableSetKind);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(is_local);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outQuery(StringInfo str, const Query *node)
{
	WRITE_NODE_TYPE("QUERY");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(querySource, QuerySource);
	/* we intentionally do not print the queryId field */
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
			case T_AlterOwnerStmt:
			case T_AlterObjectSchemaStmt:

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
			case T_CreateOpClassStmt:
			case T_CreateOpClassItem:
			case T_CreateConversionStmt:
				WRITE_NODE_FIELD(utilityStmt);
				break;
			default:
				appendStringInfoString(str, " :utilityStmt ?");
				appendStringInfo(str, "%u", nodeTag(node->utilityStmt));
				break;
		}
	}
	else
		appendStringInfoString(str, " :utilityStmt <>");

	WRITE_INT_FIELD(resultRelation);
	WRITE_BOOL_FIELD(hasAggs);
	WRITE_BOOL_FIELD(hasWindowFuncs);
	WRITE_BOOL_FIELD(hasSubLinks);
	WRITE_BOOL_FIELD(hasDynamicFunctions);
	WRITE_BOOL_FIELD(hasFuncsWithExecRestrictions);
	WRITE_BOOL_FIELD(hasDistinctOn);
	WRITE_BOOL_FIELD(hasRecursive);
	WRITE_BOOL_FIELD(hasModifyingCTE);
	WRITE_BOOL_FIELD(hasForUpdate);
	WRITE_NODE_FIELD(cteList);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(jointree);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(withCheckOptions);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(havingQual);
	WRITE_NODE_FIELD(windowClause);
	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(scatterClause);
	WRITE_BOOL_FIELD(isTableValueSelect);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(setOperations);
	WRITE_NODE_FIELD(constraintDeps);
	WRITE_BOOL_FIELD(parentStmtType);
	WRITE_BOOL_FIELD(needReshuffle);

	/* Don't serialize policy */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outWithCheckOption(StringInfo str, const WithCheckOption *node)
{
	WRITE_NODE_TYPE("WITHCHECKOPTION");

	WRITE_STRING_FIELD(viewname);
	WRITE_NODE_FIELD(qual);
	WRITE_BOOL_FIELD(cascaded);
}

static void
_outSortGroupClause(StringInfo str, const SortGroupClause *node)
{
	WRITE_NODE_TYPE("SORTGROUPCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(eqop);
	WRITE_OID_FIELD(sortop);
	WRITE_BOOL_FIELD(nulls_first);
	WRITE_BOOL_FIELD(hashable);
}

static void
_outGroupingClause(StringInfo str, const GroupingClause *node)
{
	WRITE_NODE_TYPE("GROUPINGCLAUSE");

	WRITE_ENUM_FIELD(groupType, GroupingType);
	WRITE_NODE_FIELD(groupsets);
}

static void
_outGroupingFunc(StringInfo str, const GroupingFunc *node)
{
	WRITE_NODE_TYPE("GROUPINGFUNC");

	WRITE_NODE_FIELD(args);
	WRITE_INT_FIELD(ngrpcols);
}

static void
_outGrouping(StringInfo str, const Grouping *node __attribute__((unused)))
{
	WRITE_NODE_TYPE("GROUPING");
}

static void
_outGroupId(StringInfo str, const GroupId *node __attribute__((unused)))
{
	WRITE_NODE_TYPE("GROUPID");
}

static void
_outWindowClause(StringInfo str, const WindowClause *node)
{
	WRITE_NODE_TYPE("WINDOWCLAUSE");

	WRITE_STRING_FIELD(name);
	WRITE_STRING_FIELD(refname);
	WRITE_NODE_FIELD(partitionClause);
	WRITE_NODE_FIELD(orderClause);
	WRITE_INT_FIELD(frameOptions);
	WRITE_NODE_FIELD(startOffset);
	WRITE_NODE_FIELD(endOffset);
	WRITE_UINT_FIELD(winref);
	WRITE_BOOL_FIELD(copiedOrder);
}

static void
_outRowMarkClause(StringInfo str, const RowMarkClause *node)
{
	WRITE_NODE_TYPE("ROWMARKCLAUSE");

	WRITE_UINT_FIELD(rti);
	WRITE_ENUM_FIELD(strength, LockClauseStrength);
	WRITE_BOOL_FIELD(noWait);
	WRITE_BOOL_FIELD(pushedDown);
}

static void
_outWithClause(StringInfo str, const WithClause *node)
{
	WRITE_NODE_TYPE("WITHCLAUSE");

	WRITE_NODE_FIELD(ctes);
	WRITE_BOOL_FIELD(recursive);
	WRITE_LOCATION_FIELD(location);
}

static void
_outCommonTableExpr(StringInfo str, const CommonTableExpr *node)
{
	WRITE_NODE_TYPE("COMMONTABLEEXPR");

	WRITE_STRING_FIELD(ctename);
	WRITE_NODE_FIELD(aliascolnames);
	WRITE_NODE_FIELD(ctequery);
	WRITE_LOCATION_FIELD(location);
	WRITE_BOOL_FIELD(cterecursive);
	WRITE_INT_FIELD(cterefcount);
	WRITE_NODE_FIELD(ctecolnames);
	WRITE_NODE_FIELD(ctecoltypes);
	WRITE_NODE_FIELD(ctecoltypmods);
	WRITE_NODE_FIELD(ctecolcollations);
}

static void
_outSetOperationStmt(StringInfo str, const SetOperationStmt *node)
{
	WRITE_NODE_TYPE("SETOPERATIONSTMT");

	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(colTypes);
	WRITE_NODE_FIELD(colTypmods);
	WRITE_NODE_FIELD(colCollations);
	WRITE_NODE_FIELD(groupClauses);
}

static void
_outRangeTblEntry(StringInfo str, const RangeTblEntry *node)
{
	WRITE_NODE_TYPE("RTE");

	/* put alias + eref first to make dump more legible */
	WRITE_NODE_FIELD(alias);
	WRITE_NODE_FIELD(eref);
	WRITE_ENUM_FIELD(rtekind, RTEKind);

	switch (node->rtekind)
	{
		case RTE_RELATION:
			WRITE_OID_FIELD(relid);
			WRITE_CHAR_FIELD(relkind);
			break;
		case RTE_SUBQUERY:
			WRITE_NODE_FIELD(subquery);
			WRITE_BOOL_FIELD(security_barrier);
			break;
		case RTE_JOIN:
			WRITE_ENUM_FIELD(jointype, JoinType);
			WRITE_NODE_FIELD(joinaliasvars);
			break;
		case RTE_FUNCTION:
			WRITE_NODE_FIELD(functions);
			WRITE_BOOL_FIELD(funcordinality);
			break;
		case RTE_TABLEFUNCTION:
			WRITE_NODE_FIELD(subquery);
			WRITE_NODE_FIELD(functions);
			WRITE_BOOL_FIELD(funcordinality);
			break;
		case RTE_VALUES:
			WRITE_NODE_FIELD(values_lists);
			WRITE_NODE_FIELD(values_collations);
			break;
		case RTE_CTE:
			WRITE_STRING_FIELD(ctename);
			WRITE_UINT_FIELD(ctelevelsup);
			WRITE_BOOL_FIELD(self_reference);
			WRITE_NODE_FIELD(ctecoltypes);
			WRITE_NODE_FIELD(ctecoltypmods);
			WRITE_NODE_FIELD(ctecolcollations);
			break;
        case RTE_VOID:                                                  /*CDB*/
            break;
		default:
			elog(ERROR, "unrecognized RTE kind: %d", (int) node->rtekind);
			break;
	}

	WRITE_BOOL_FIELD(lateral);
	WRITE_BOOL_FIELD(inh);
	WRITE_BOOL_FIELD(inFromCl);
	WRITE_UINT_FIELD(requiredPerms);
	WRITE_OID_FIELD(checkAsUser);
	WRITE_BITMAPSET_FIELD(selectedCols);
	WRITE_BITMAPSET_FIELD(modifiedCols);

	WRITE_BOOL_FIELD(forceDistRandom);
	/*
	 * pseudocols is intentionally not serialized. It's only used in the planning
	 * stage, so no need to transfer it to the QEs.
	 */
	WRITE_NODE_FIELD(securityQuals);
}

static void
_outRangeTblFunction(StringInfo str, const RangeTblFunction *node)
{
	WRITE_NODE_TYPE("RANGETBLFUNCTION");

	WRITE_NODE_FIELD(funcexpr);
	WRITE_INT_FIELD(funccolcount);
	WRITE_NODE_FIELD(funccolnames);
	WRITE_NODE_FIELD(funccoltypes);
	WRITE_NODE_FIELD(funccoltypmods);
	WRITE_NODE_FIELD(funccolcollations);
	/* funcuserdata is only serialized in binary out/read functions */
#ifdef COMPILING_BINARY_FUNCS
	WRITE_BYTEA_FIELD(funcuserdata);
#endif
	WRITE_BITMAPSET_FIELD(funcparams);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAExpr(StringInfo str, const A_Expr *node)
{
	WRITE_NODE_TYPE("AEXPR");

	switch (node->kind)
	{
		case AEXPR_OP:
			appendStringInfoChar(str, ' ');
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_AND:
			appendStringInfoString(str, " AND");
			break;
		case AEXPR_OR:
			appendStringInfoString(str, " OR");
			break;
		case AEXPR_NOT:
			appendStringInfoString(str, " NOT");
			break;
		case AEXPR_OP_ANY:
			appendStringInfoChar(str, ' ');
			WRITE_NODE_FIELD(name);
			appendStringInfoString(str, " ANY ");
			break;
		case AEXPR_OP_ALL:
			appendStringInfoChar(str, ' ');
			WRITE_NODE_FIELD(name);
			appendStringInfoString(str, " ALL ");
			break;
		case AEXPR_DISTINCT:
			appendStringInfoString(str, " DISTINCT ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NULLIF:
			appendStringInfoString(str, " NULLIF ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_OF:
			appendStringInfoString(str, " OF ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_IN:
			appendStringInfoString(str, " IN ");
			WRITE_NODE_FIELD(name);
			break;
		default:
			appendStringInfoString(str, " ??");
			break;
	}

	WRITE_NODE_FIELD(lexpr);
	WRITE_NODE_FIELD(rexpr);
	WRITE_LOCATION_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outValue(StringInfo str, const Value *value)
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

			/*
			 * We use _outToken to provide escaping of the string's content,
			 * but we don't want it to do anything with an empty string.
			 */
			appendStringInfoChar(str, '"');
			if (value->val.str[0] != '\0')
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
_outNull(StringInfo str, const Node *n __attribute__((unused)))
{
	WRITE_NODE_TYPE("NULL");
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outColumnRef(StringInfo str, const ColumnRef *node)
{
	WRITE_NODE_TYPE("COLUMNREF");

	WRITE_NODE_FIELD(fields);
	WRITE_LOCATION_FIELD(location);
}

static void
_outParamRef(StringInfo str, const ParamRef *node)
{
	WRITE_NODE_TYPE("PARAMREF");

	WRITE_INT_FIELD(number);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAConst(StringInfo str, const A_Const *node)
{
	WRITE_NODE_TYPE("A_CONST");

	appendStringInfoString(str, " :val ");
	_outValue(str, &(node->val));
	WRITE_LOCATION_FIELD(location);
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outA_Star(StringInfo str, const A_Star *node)
{
	WRITE_NODE_TYPE("A_STAR");
}

static void
_outA_Indices(StringInfo str, const A_Indices *node)
{
	WRITE_NODE_TYPE("A_INDICES");

	WRITE_NODE_FIELD(lidx);
	WRITE_NODE_FIELD(uidx);
}

static void
_outA_Indirection(StringInfo str, const A_Indirection *node)
{
	WRITE_NODE_TYPE("A_INDIRECTION");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(indirection);
}

static void
_outA_ArrayExpr(StringInfo str, const A_ArrayExpr *node)
{
	WRITE_NODE_TYPE("A_ARRAYEXPR");

	WRITE_NODE_FIELD(elements);
	WRITE_LOCATION_FIELD(location);
}

static void
_outResTarget(StringInfo str, const ResTarget *node)
{
	WRITE_NODE_TYPE("RESTARGET");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(indirection);
	WRITE_NODE_FIELD(val);
	WRITE_LOCATION_FIELD(location);
}

static void
_outSortBy(StringInfo str, const SortBy *node)
{
	WRITE_NODE_TYPE("SORTBY");

	WRITE_NODE_FIELD(node);
	WRITE_ENUM_FIELD(sortby_dir, SortByDir);
	WRITE_ENUM_FIELD(sortby_nulls, SortByNulls);
	WRITE_NODE_FIELD(useOp);
	WRITE_NODE_FIELD(node);
	WRITE_LOCATION_FIELD(location);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outWindowDef(StringInfo str, const WindowDef *node)
{
	WRITE_NODE_TYPE("WINDOWDEF");

	WRITE_STRING_FIELD(name);
	WRITE_STRING_FIELD(refname);
	WRITE_NODE_FIELD(partitionClause);
	WRITE_NODE_FIELD(orderClause);
	WRITE_INT_FIELD(frameOptions);
	WRITE_NODE_FIELD(startOffset);
	WRITE_NODE_FIELD(endOffset);
	WRITE_LOCATION_FIELD(location);
}

static void
_outRangeSubselect(StringInfo str, const RangeSubselect *node)
{
	WRITE_NODE_TYPE("RANGESUBSELECT");

	WRITE_BOOL_FIELD(lateral);
	WRITE_NODE_FIELD(subquery);
	WRITE_NODE_FIELD(alias);
}

static void
_outRangeFunction(StringInfo str, const RangeFunction *node)
{
	WRITE_NODE_TYPE("RANGEFUNCTION");

	WRITE_BOOL_FIELD(lateral);
	WRITE_BOOL_FIELD(ordinality);
	WRITE_BOOL_FIELD(is_rowsfrom);
	WRITE_NODE_FIELD(functions);
	WRITE_NODE_FIELD(alias);
	WRITE_NODE_FIELD(coldeflist);
}
#endif

#ifndef COMPILING_BINARY_FUNCS
static void
_outConstraint(StringInfo str, const Constraint *node)
{
	WRITE_NODE_TYPE("CONSTRAINT");

	WRITE_STRING_FIELD(conname);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_LOCATION_FIELD(location);

	appendStringInfoString(str, " :contype ");
	switch (node->contype)
	{
		case CONSTR_NULL:
			appendStringInfoString(str, "NULL");
			break;

		case CONSTR_NOTNULL:
			appendStringInfoString(str, "NOT_NULL");
			break;

		case CONSTR_DEFAULT:
			appendStringInfoString(str, "DEFAULT");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_CHECK:
			appendStringInfoString(str, "CHECK");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			/*
			 * GPDB: need dispatch skip_validation and is_no_inherit for statement like:
			 * ALTER DOMAIN things ADD CONSTRAINT meow CHECK (VALUE < 11) NOT VALID;
			 * ALTER TABLE constraint_rename_test ADD CONSTRAINT con2 CHECK NO INHERIT (b > 0);
			 */
			WRITE_BOOL_FIELD(skip_validation);
			WRITE_BOOL_FIELD(is_no_inherit);
			break;

		case CONSTR_PRIMARY:
			appendStringInfoString(str, "PRIMARY_KEY");
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexname);
			WRITE_STRING_FIELD(indexspace);
			/* access_method and where_clause not currently used */
			break;

		case CONSTR_UNIQUE:
			appendStringInfoString(str, "UNIQUE");
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexname);
			WRITE_STRING_FIELD(indexspace);
			/* access_method and where_clause not currently used */
			break;

		case CONSTR_EXCLUSION:
			appendStringInfoString(str, "EXCLUSION");
			WRITE_NODE_FIELD(exclusions);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexname);
			WRITE_STRING_FIELD(indexspace);
			WRITE_STRING_FIELD(access_method);
			WRITE_NODE_FIELD(where_clause);
			break;

		case CONSTR_FOREIGN:
			appendStringInfoString(str, "FOREIGN_KEY");
			WRITE_NODE_FIELD(pktable);
			WRITE_NODE_FIELD(fk_attrs);
			WRITE_NODE_FIELD(pk_attrs);
			WRITE_CHAR_FIELD(fk_matchtype);
			WRITE_CHAR_FIELD(fk_upd_action);
			WRITE_CHAR_FIELD(fk_del_action);
			WRITE_NODE_FIELD(old_conpfeqop);
			WRITE_OID_FIELD(old_pktable_oid);
			WRITE_BOOL_FIELD(skip_validation);
			WRITE_BOOL_FIELD(initially_valid);
			WRITE_OID_FIELD(trig1Oid);
			WRITE_OID_FIELD(trig2Oid);
			WRITE_OID_FIELD(trig3Oid);
			WRITE_OID_FIELD(trig4Oid);
			break;

		case CONSTR_ATTR_DEFERRABLE:
			appendStringInfoString(str, "ATTR_DEFERRABLE");
			break;

		case CONSTR_ATTR_NOT_DEFERRABLE:
			appendStringInfoString(str, "ATTR_NOT_DEFERRABLE");
			break;

		case CONSTR_ATTR_DEFERRED:
			appendStringInfoString(str, "ATTR_DEFERRED");
			break;

		case CONSTR_ATTR_IMMEDIATE:
			appendStringInfoString(str, "ATTR_IMMEDIATE");
			break;

		default:
			appendStringInfo(str, "<unrecognized_constraint %d>",
							(int) node->contype);
			break;
	}
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outCreateSchemaStmt(StringInfo str, const CreateSchemaStmt *node)
{
	WRITE_NODE_TYPE("CREATESCHEMASTMT");

	WRITE_STRING_FIELD(schemaname);
	WRITE_STRING_FIELD(authid);
	WRITE_BOOL_FIELD(istemp);
}

static void
_outCreatePLangStmt(StringInfo str, const CreatePLangStmt *node)
{
	WRITE_NODE_TYPE("CREATEPLANGSTMT");

	WRITE_BOOL_FIELD(replace);
	WRITE_STRING_FIELD(plname);
	WRITE_NODE_FIELD(plhandler);
	WRITE_NODE_FIELD(plinline);
	WRITE_NODE_FIELD(plvalidator);
	WRITE_BOOL_FIELD(pltrusted);
}

static void
_outVacuumStmt(StringInfo str, const VacuumStmt *node)
{
	WRITE_NODE_TYPE("VACUUMSTMT");

	WRITE_INT_FIELD(options);
	WRITE_INT_FIELD(freeze_min_age);
	WRITE_INT_FIELD(freeze_table_age);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(va_cols);

	WRITE_BOOL_FIELD(skip_twophase);
	WRITE_NODE_FIELD(expanded_relids);
	WRITE_NODE_FIELD(appendonly_compaction_segno);
	WRITE_NODE_FIELD(appendonly_compaction_insert_segno);
	WRITE_ENUM_FIELD(appendonly_phase, AOVacuumPhase);
}

static void
_outCdbProcess(StringInfo str, const CdbProcess *node)
{
	WRITE_NODE_TYPE("CDBPROCESS");
	WRITE_STRING_FIELD(listenerAddr);
	WRITE_INT_FIELD(listenerPort);
	WRITE_INT_FIELD(pid);
	WRITE_INT_FIELD(contentid);
}

static void
_outSlice(StringInfo str, const Slice *node)
{
	WRITE_NODE_TYPE("SLICE");
	WRITE_INT_FIELD(sliceIndex);
	WRITE_INT_FIELD(rootIndex);
	WRITE_INT_FIELD(parentIndex);
	WRITE_NODE_FIELD(children); /* List of int index */
	WRITE_ENUM_FIELD(gangType,GangType);
	WRITE_INT_FIELD(gangSize);
	WRITE_BOOL_FIELD(directDispatch.isDirectDispatch);
	WRITE_NODE_FIELD(directDispatch.contentIds); /* List of int */
	WRITE_DUMMY_FIELD(primaryGang);
	WRITE_NODE_FIELD(primaryProcesses); /* List of (CDBProcess *) */
	WRITE_BITMAPSET_FIELD(processesMap);
}

static void
_outSliceTable(StringInfo str, const SliceTable *node)
{
	WRITE_NODE_TYPE("SLICETABLE");
	WRITE_INT_FIELD(nMotions);
	WRITE_INT_FIELD(nInitPlans);
	WRITE_INT_FIELD(localSlice);
	WRITE_NODE_FIELD(slices); /* List of int */
	WRITE_INT_FIELD(instrument_options);
	WRITE_INT_FIELD(ic_instance_id);
}

static void
_outCursorPosInfo(StringInfo str, const CursorPosInfo *node)
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
_outCreateTrigStmt(StringInfo str, const CreateTrigStmt *node)
{
	WRITE_NODE_TYPE("CREATETRIGSTMT");

	WRITE_STRING_FIELD(trigname);
	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(row);
	WRITE_INT_FIELD(timing);
	WRITE_INT_FIELD(events);
	WRITE_NODE_FIELD(columns);
	WRITE_NODE_FIELD(whenClause);
	WRITE_BOOL_FIELD(isconstraint);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_NODE_FIELD(constrrel);
	WRITE_OID_FIELD(trigOid);
}

static void
_outCreateTableSpaceStmt(StringInfo str, const CreateTableSpaceStmt *node)
{
	WRITE_NODE_TYPE("CREATETABLESPACESTMT");

	WRITE_STRING_FIELD(tablespacename);
	WRITE_STRING_FIELD(owner);
	WRITE_STRING_FIELD(location);
	WRITE_NODE_FIELD(options);
}

static void
_outDropTableSpaceStmt(StringInfo str, const DropTableSpaceStmt *node)
{
	WRITE_NODE_TYPE("DROPTABLESPACESTMT");

	WRITE_STRING_FIELD(tablespacename);
	WRITE_BOOL_FIELD(missing_ok);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCreateQueueStmt(StringInfo str, const CreateQueueStmt *node)
{
	WRITE_NODE_TYPE("CREATEQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterQueueStmt(StringInfo str, const AlterQueueStmt *node)
{
	WRITE_NODE_TYPE("ALTERQUEUESTMT");

	WRITE_STRING_FIELD(queue);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outDropQueueStmt(StringInfo str, const DropQueueStmt *node)
{
	WRITE_NODE_TYPE("DROPQUEUESTMT");

	WRITE_STRING_FIELD(queue);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outCreateResourceGroupStmt(StringInfo str, const CreateResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("CREATERESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */

static void
_outDropResourceGroupStmt(StringInfo str, const DropResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("DROPRESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outAlterResourceGroupStmt(StringInfo str, const AlterResourceGroupStmt *node)
{
	WRITE_NODE_TYPE("ALTERRESOURCEGROUPSTMT");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(options); /* List of DefElem nodes */
}
#endif /* COMPILING_BINARY_FUNCS */


static void
_outCommentStmt(StringInfo str, const CommentStmt *node)
{
	WRITE_NODE_TYPE("COMMENTSTMT");

	WRITE_ENUM_FIELD(objtype, ObjectType);
	WRITE_NODE_FIELD(objname);
	WRITE_NODE_FIELD(objargs);
	WRITE_STRING_FIELD(comment);
}

static void
_outTableValueExpr(StringInfo str, const TableValueExpr *node)
{
	WRITE_NODE_TYPE("TABLEVALUEEXPR");

	WRITE_NODE_FIELD(subquery);
}

static void
_outAlterTypeStmt(StringInfo str, const AlterTypeStmt *node)
{
	WRITE_NODE_TYPE("ALTERTYPESTMT");

	WRITE_NODE_FIELD(typeName);
	WRITE_NODE_FIELD(encoding);
}

static void
_outAlterExtensionStmt(StringInfo str, const AlterExtensionStmt *node)
{
	WRITE_NODE_TYPE("ALTEREXTENSIONSTMT");

	WRITE_STRING_FIELD(extname);
	WRITE_NODE_FIELD(options);
}

static void
_outAlterExtensionContentsStmt(StringInfo str, const AlterExtensionContentsStmt *node)
{
	WRITE_NODE_TYPE("ALTEREXTENSIONCONTENTSSTMT");

	WRITE_STRING_FIELD(extname);
	WRITE_INT_FIELD(action);
	WRITE_ENUM_FIELD(objtype, ObjectType);
	WRITE_NODE_FIELD(objname);
	WRITE_NODE_FIELD(objargs);
}

static void
_outAlterTSConfigurationStmt(StringInfo str, const AlterTSConfigurationStmt *node)
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
_outAlterTSDictionaryStmt(StringInfo str, const AlterTSDictionaryStmt *node)
{
	WRITE_NODE_TYPE("ALTERTSDICTIONARYSTMT");

	WRITE_NODE_FIELD(dictname);
	WRITE_NODE_FIELD(options);
}

#ifndef COMPILING_BINARY_FUNCS
static void
_outReshuffleExpr(StringInfo str, const ReshuffleExpr *node)
{
	WRITE_NODE_TYPE("RESHUFFLEEXPR");

	WRITE_INT_FIELD(newSegs);
	WRITE_INT_FIELD(oldSegs);
	WRITE_NODE_FIELD(hashKeys);
	WRITE_NODE_FIELD(hashTypes);
	WRITE_INT_FIELD(ptype);
}


static void
_outTupleDescNode(StringInfo str, const TupleDescNode *node)
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
_outNode(StringInfo str, const void *obj)
{
	if (obj == NULL)
		appendStringInfoString(str, "<>");
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
			case T_ModifyTable:
				_outModifyTable(str, obj);
				break;
			case T_Append:
				_outAppend(str, obj);
				break;
			case T_MergeAppend:
				_outMergeAppend(str, obj);
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
			case T_DynamicSeqScan:
				_outDynamicSeqScan(str, obj);
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
			case T_IndexOnlyScan:
				_outIndexOnlyScan(str, obj);
				break;
			case T_BitmapIndexScan:
				_outBitmapIndexScan(str, obj);
				break;
			case T_DynamicBitmapIndexScan:
				_outDynamicBitmapIndexScan(str, obj);
				break;
			case T_BitmapHeapScan:
				_outBitmapHeapScan(str, obj);
				break;
			case T_DynamicBitmapHeapScan:
				_outDynamicBitmapHeapScan(str, obj);
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
			case T_ForeignScan:
				_outForeignScan(str, obj);
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
			case T_WindowAgg:
				_outWindowAgg(str, obj);
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
			case T_LockRows:
				_outLockRows(str, obj);
				break;
			case T_Limit:
				_outLimit(str, obj);
				break;
			case T_NestLoopParam:
				_outNestLoopParam(str, obj);
				break;
			case T_PlanRowMark:
				_outPlanRowMark(str, obj);
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
			case T_Reshuffle:
				_outReshuffle(str, obj);
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
			case T_CopyIntoClause:
				_outCopyIntoClause(str, obj);
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
			case T_WindowFunc:
				_outWindowFunc(str, obj);
				break;
			case T_ArrayRef:
				_outArrayRef(str, obj);
				break;
			case T_FuncExpr:
				_outFuncExpr(str, obj);
				break;
			case T_NamedArgExpr:
				_outNamedArgExpr(str, obj);
				break;
			case T_OpExpr:
				_outOpExpr(str, obj);
				break;
			case T_DistinctExpr:
				_outDistinctExpr(str, obj);
				break;
			case T_NullIfExpr:
				_outNullIfExpr(str, obj);
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
			case T_AlternativeSubPlan:
				_outAlternativeSubPlan(str, obj);
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
			case T_CollateExpr:
				_outCollateExpr(str, obj);
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
			case T_RangeTblFunction:
				_outRangeTblFunction(str, obj);
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
			case T_BitmapAndPath:
				_outBitmapAndPath(str, obj);
				break;
			case T_BitmapOrPath:
				_outBitmapOrPath(str, obj);
				break;
			case T_TidPath:
				_outTidPath(str, obj);
				break;
			case T_ForeignPath:
				_outForeignPath(str, obj);
				break;
			case T_AppendPath:
				_outAppendPath(str, obj);
				break;
			case T_MergeAppendPath:
				_outMergeAppendPath(str, obj);
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
			case T_EquivalenceClass:
				_outEquivalenceClass(str, obj);
				break;
			case T_EquivalenceMember:
				_outEquivalenceMember(str, obj);
				break;
			case T_PathKey:
				_outPathKey(str, obj);
				break;
			case T_DistributionKey:
				_outDistributionKey(str, obj);
				break;
			case T_ParamPathInfo:
				_outParamPathInfo(str, obj);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj);
				break;
			case T_PlaceHolderVar:
				_outPlaceHolderVar(str, obj);
				break;
			case T_SpecialJoinInfo:
				_outSpecialJoinInfo(str, obj);
				break;
			case T_LateralJoinInfo:
				_outLateralJoinInfo(str, obj);
				break;
			case T_AppendRelInfo:
				_outAppendRelInfo(str, obj);
				break;
			case T_PlaceHolderInfo:
				_outPlaceHolderInfo(str, obj);
				break;
			case T_MinMaxAggInfo:
				_outMinMaxAggInfo(str, obj);
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
			case T_CreateForeignTableStmt:
				_outCreateForeignTableStmt(str, obj);
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
			case T_ExpandStmtSpec:
				_outExpandStmtSpec(str, obj);
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
			case T_DistributedBy:
				_outDistributedBy(str, obj);
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
			case T_CreateRangeStmt:
				_outCreateRangeStmt(str,obj);
				break;
			case T_CreateCastStmt:
				_outCreateCastStmt(str,obj);
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
			case T_DropOwnedStmt:
				_outDropOwnedStmt(str, obj);
				break;
			case T_ReassignOwnedStmt:
				_outReassignOwnedStmt(str, obj);
				break;
			case T_TruncateStmt:
				_outTruncateStmt(str, obj);
				break;

			case T_ReplicaIdentityStmt:
				_outReplicaIdentityStmt(str, obj);
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
			case T_WindowDef:
				_outWindowDef(str, obj);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj);
				break;
			case T_CollateClause:
				_outCollateClause(str, obj);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj);
				break;
			case T_Query:
				_outQuery(str, obj);
				break;
			case T_WithCheckOption:
				_outWithCheckOption(str, obj);
				break;
			case T_SortGroupClause:
				_outSortGroupClause(str, obj);
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
			case T_WindowClause:
				_outWindowClause(str, obj);
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
			case T_A_Star:
				_outA_Star(str, obj);
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
			case T_FuncCall:
				_outFuncCall(str, obj);
				break;
			case T_DefElem:
				_outDefElem(str, obj);
				break;
			case T_TableLikeClause:
				_outTableLikeClause(str, obj);
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

			case T_CreateTableSpaceStmt:
				_outCreateTableSpaceStmt(str, obj);
				break;

			case T_DropTableSpaceStmt:
				_outDropTableSpaceStmt(str, obj);
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
			case T_ReshuffleExpr:
                _outReshuffleExpr(str, obj);
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
nodeToString(const void *obj)
{
	StringInfoData str;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfo(&str);
	_outNode(&str, obj);
	return str.data;
}

#endif /* COMPILING_BINARY_FUNCS */
