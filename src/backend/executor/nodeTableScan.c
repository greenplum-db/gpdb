/*-------------------------------------------------------------------------
 *
 * nodeTableScan.c
 *    Support routines for scanning a relation. This relation can be Heap,
 *    AppendOnly Row, or AppendOnly Columnar.
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeTableScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "executor/nodeTableScan.h"
#include "utils/elog.h"
#include "parser/parsetree.h"

#define TABLE_SCAN_NSLOTS 2

TableScanState *
ExecInitTableScan(TableScan *node, EState *estate, int eflags)
{
	TableScanState *state = makeNode(TableScanState);
	state->ss.scan_state = SCAN_INIT;

	InitScanStateInternal((ScanState *)state, (Plan *)node, estate, eflags, true /* initCurrentRelation */);
	
	initGpmonPktForTableScan((Plan *)node, &state->ss.ps.gpmon_pkt, estate);

	return state;
}

TupleTableSlot *
ExecTableScan(TableScanState *node)
{
	ScanState *scanState = (ScanState *)node;

	if (scanState->scan_state == SCAN_INIT ||
		scanState->scan_state == SCAN_DONE)
	{
		BeginTableScanRelation(scanState);
	}

	TupleTableSlot *slot = ExecTableScanRelation(scanState);
	
	if (!TupIsNull(slot))
	{
		Gpmon_Incr_Rows_Out(GpmonPktFromTableScanState(node));
		CheckSendPlanStateGpmonPkt(&scanState->ps);
	}
	
	else if (!scanState->ps.delayEagerFree)
	{
		EndTableScanRelation(scanState);
	}

	return slot;
}

void
ExecEndTableScan(TableScanState *node)
{
	if ((node->ss.scan_state & SCAN_SCAN) != 0)
	{
		EndTableScanRelation(&(node->ss));
	}

	FreeScanRelationInternal((ScanState *)node, true /* closeCurrentRelation */);
	EndPlanStateGpmonPkt(&node->ss.ps);
}

void
ExecTableReScan(TableScanState *node, ExprContext *exprCtxt)
{
	ReScanRelation((ScanState *)node);

	CheckSendPlanStateGpmonPkt(&node->ss.ps);
}

void
ExecTableMarkPos(TableScanState *node)
{
	MarkPosScanRelation((ScanState *)node);
}

void
ExecTableRestrPos(TableScanState *node)
{
	RestrPosScanRelation((ScanState *)node);

	CheckSendPlanStateGpmonPkt(&node->ss.ps);
}

int
ExecCountSlotsTableScan(TableScan *node)
{
	return TABLE_SCAN_NSLOTS;
}

void
initGpmonPktForTableScan(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL);
	Assert(IsA(planNode, TableScan) ||
		   IsA(planNode, SeqScan) ||
		   IsA(planNode, AppendOnlyScan) ||
		   IsA(planNode, AOCSScan));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate);
}

void
ExecEagerFreeTableScan(TableScanState *node)
{
	if (node->ss.scan_state != SCAN_INIT &&
		node->ss.scan_state != SCAN_DONE)
	{
		EndTableScanRelation((ScanState *)node);
	}
}
