/*-------------------------------------------------------------------------
 *
 * execAppendOnlyScan.c
 *   Support routines for scanning AppendOnly tables.
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execAOScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "cdb/cdbappendonlyam.h"
#include "utils/snapmgr.h"

TupleTableSlot *
AppendOnlyScanNext(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AppendOnlyScanState *node = (AppendOnlyScanState *)scanState;
	
	AppendOnlyScanDesc scandesc;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	Assert((node->ss.scan_state & SCAN_SCAN) != 0);
	/*
	 * get information from the estate and scan state
	 */
	estate = node->ss.ps.state;
	scandesc = node->aos_ScanDesc;
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
	AppendOnlyScanState *node = (AppendOnlyScanState *)scanState;
	
	Assert(node->ss.scan_state == SCAN_INIT ||
		   node->ss.scan_state == SCAN_DONE);
	Assert(node->aos_ScanDesc == NULL);

	appendOnlyMetaDataSnapshot = node->ss.ps.state->es_snapshot;
	if (appendOnlyMetaDataSnapshot == SnapshotAny)
	{
		/* 
		 * the append-only meta data should never be fetched with
		 * SnapshotAny as bogus results are returned.
		 */
		appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
	}

	node->aos_ScanDesc = appendonly_beginscan(
			node->ss.ss_currentRelation, 
			node->ss.ps.state->es_snapshot, 
			appendOnlyMetaDataSnapshot,
			0, NULL);
	node->ss.scan_state = SCAN_SCAN;
}

void
EndScanAppendOnlyRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AppendOnlyScanState *node = (AppendOnlyScanState *)scanState;
	Assert(node->aos_ScanDesc != NULL);

	Assert((node->ss.scan_state & SCAN_SCAN) != 0);
	appendonly_endscan(node->aos_ScanDesc);

	node->aos_ScanDesc = NULL;
	
	node->ss.scan_state = SCAN_INIT;
}

void
ReScanAppendOnlyRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AppendOnlyScanState *node = (AppendOnlyScanState *)scanState;
	Assert(node->aos_ScanDesc != NULL);

	appendonly_rescan(node->aos_ScanDesc, NULL /* new scan keys */);
}
