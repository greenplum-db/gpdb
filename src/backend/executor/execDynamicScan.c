/*-------------------------------------------------------------------------
 *
 * execDynamicScan.c
 *	  Support routines for iterating through dynamically chosen partitions of a relation
 *
 * Portions Copyright (c) 2014-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execDynamicScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/partitionselection.h"
#include "executor/executor.h"
#include "executor/execDynamicScan.h"
#include "parser/parsetree.h"

/*
 * isDynamicScan
 * 		Returns true if the scan node is dynamic (i.e., determining
 * 		relations to scan at runtime).
 */
bool
isDynamicScan(const Plan *plan)
{
	switch (nodeTag(plan))
	{
		case T_DynamicSeqScan:
		case T_DynamicIndexScan:
		case T_DynamicBitmapHeapScan:
		case T_DynamicBitmapIndexScan:
			return true;

		default:
			return false;
	}
}

/*
 * DynamicScan_GetDynamicScanId
 *		Returns the index into EState->dynamicTableScanInfo arrays for this
 *      dynamic scan node.
 */
static int
DynamicScan_GetDynamicScanId(Plan *plan)
{
	switch (nodeTag(plan))
	{
		case T_DynamicSeqScan:
			return ((DynamicSeqScan *) plan)->partIndex;

		case T_DynamicIndexScan:
			return ((DynamicIndexScan *) plan)->partIndex;

		case T_DynamicBitmapHeapScan:
			return ((DynamicBitmapHeapScan *) plan)->partIndex;

		case T_DynamicBitmapIndexScan:
			return ((DynamicBitmapIndexScan *) plan)->partIndex;

		default:
			elog(ERROR, "unknown dynamic scan node type %u", nodeTag(plan));
	}
}

/*
 * DynamicScan_GetDynamicScanIdPrintable
 *		Return "printable" scan id for a node, for EXPLAIN
 */
int
DynamicScan_GetDynamicScanIdPrintable(Plan *plan)
{
	switch (nodeTag(plan))
	{
		case T_DynamicSeqScan:
			return ((DynamicSeqScan *) plan)->partIndexPrintable;

		case T_DynamicIndexScan:
			return ((DynamicIndexScan *) plan)->partIndexPrintable;

		case T_DynamicBitmapHeapScan:
			return ((DynamicBitmapHeapScan *) plan)->partIndexPrintable;

		case T_DynamicBitmapIndexScan:
			return ((DynamicBitmapIndexScan *) plan)->partIndexPrintable;

		default:
			elog(ERROR, "unknown dynamic scan node type %u", nodeTag(plan));
	}
}

/*
 * DynamicScan_GetTableOid
 * 		Returns the Oid of the table/partition to scan.
 *
 *		A partition must have been selected already.
 */
Oid
DynamicScan_GetTableOid(ScanState *scanState)
{
	int			partIndex;
	Oid			curRelOid;

	partIndex = DynamicScan_GetDynamicScanId(scanState->ps.plan);

	/* Get the oid of the current relation */
	Assert(NULL != scanState->ps.state->dynamicTableScanInfo);
	Assert(partIndex <= scanState->ps.state->dynamicTableScanInfo->numScans);

	curRelOid = scanState->ps.state->dynamicTableScanInfo->curRelOids[partIndex - 1];
	if (!OidIsValid(curRelOid))
		elog(ERROR, "no partition selected for dynamic scan id %d", partIndex);

	return curRelOid;
}

/*
 * DynamicScan_SetTableOid
 * 		Select a partition to scan in a dynamic scan.
 *
 *		This is used to advertise the selected partition in EState->dynamicTableScanInfo.
 */
void
DynamicScan_SetTableOid(ScanState *scanState, Oid curRelOid)
{
	int			partIndex;

	partIndex = DynamicScan_GetDynamicScanId(scanState->ps.plan);

	Assert(NULL != scanState->ps.state->dynamicTableScanInfo);
	Assert(partIndex <= scanState->ps.state->dynamicTableScanInfo->numScans);

	scanState->ps.state->dynamicTableScanInfo->curRelOids[partIndex - 1] = curRelOid;
}

/*
 * DynamicScan_RemapExpression
 * 		Re-maps the expression using the provided attMap.
 */
bool
DynamicScan_RemapExpression(ScanState *scanState, AttrNumber *attMap, Node *expr)
{
	if (!isDynamicScan(scanState->ps.plan))
	{
		return false;
	}

	if (NULL != attMap)
	{
		change_varattnos_of_a_varno((Node*)expr, attMap, ((Scan *)scanState->ps.plan)->scanrelid);
		return true;
	}

	return false;
}
