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

TableScanState *
ExecInitTableScan(TableScan *node, EState *estate, int eflags)
{
	TableScanState *state = makeNode(TableScanState);
	state->ss.scan_state = SCAN_INIT;

	InitScanStateInternal((ScanState *)state, (Plan *)node, estate, eflags, true /* initCurrentRelation */);

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

	if (TupIsNull(slot) && !scanState->ps.delayEagerFree)
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
ExecReScanTable(TableScanState *node)
{
	ReScanRelation((ScanState *)node);
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
