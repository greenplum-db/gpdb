/*
 * execAppendOnlyScan.c
 *   Support routines for scanning AppendOnly tables.
 *
 * Copyright (c) 2012 - present, EMC/Greenplum
 */
#include "postgres.h"

#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "cdb/cdbappendonlyam.h"

TupleTableSlot *
AppendOnlyScanNext(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *) scanState;
	AppendOnlyScanDesc scandesc;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	Assert((node->ss.scan_state & SCAN_SCAN) != 0);
	/*
	 * get information from the estate and scan state
	 */
	estate = node->ss.ps.state;
	scandesc = node->opaque.appendonly;
	direction = estate->es_direction;
	slot = node->ss.ss_ScanTupleSlot;

	/*
	 * put the next tuple from the access methods in our tuple slot
	 */
	appendonly_getnext(scandesc, direction, slot);

	return slot;
}

void
BeginScanAppendOnlyRelation(ScanState *scanState)
{
	Snapshot appendOnlyMetaDataSnapshot;

	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *) scanState;
	Assert(scanState->scan_state == SCAN_INIT ||
		   scanState->scan_state == SCAN_DONE);
	Assert(((TableScanState *) node)->opaque == NULL);

	appendOnlyMetaDataSnapshot = scanState->ps.state->es_snapshot;
	if (appendOnlyMetaDataSnapshot == SnapshotAny)
	{
		/* 
		 * the append-only meta data should never be fetched with
		 * SnapshotAny as bogus results are returned.
		 */
		appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
	}

	node->opaque.appendonly = appendonly_beginscan(
			scanState->ss_currentRelation, 
			scanState->ps.state->es_snapshot, 
			appendOnlyMetaDataSnapshot,
			0, NULL);
	node->ss.scan_state = SCAN_SCAN;
}

void
EndScanAppendOnlyRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *) scanState;
	Assert(node->aos_ScanDesc != NULL);

	Assert((node->ss.scan_state & SCAN_SCAN) != 0);
	appendonly_endscan(node->opaque.appendonly);

	node->opaque.appendonly = NULL;
	
	node->ss.scan_state = SCAN_INIT;
}

void
ReScanAppendOnlyRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *) scanState;
	Assert(node->opaque != NULL);

	appendonly_rescan(node->opaque.appendonly, NULL /* new scan keys */);
}
