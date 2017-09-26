/*-------------------------------------------------------------------------
 *
 * execAOCSScan.c
 *   Support routines for scanning AppendOnly Columnar tables.
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execAOCSScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/snapmgr.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "cdb/cdbaocsam.h"

static void
InitAOCSScanOpaque(ScanState *scanState)
{
	AOCSScanState *state = (AOCSScanState *)scanState;
	Assert(state->opaque == NULL);
	state->opaque = palloc(sizeof(AOCSScanOpaqueData));

	/* Initialize AOCS projection info */
	AOCSScanOpaqueData *opaque = (AOCSScanOpaqueData *)state->opaque;
	Relation currentRelation = scanState->ss_currentRelation;
	Assert(currentRelation != NULL);

	opaque->ncol = currentRelation->rd_att->natts;
	opaque->proj = palloc0(sizeof(bool) * opaque->ncol);
	GetNeededColumnsForScan((Node *)scanState->ps.plan->targetlist, opaque->proj, opaque->ncol);
	GetNeededColumnsForScan((Node *)scanState->ps.plan->qual, opaque->proj, opaque->ncol);
	
	int i = 0;
	for (i = 0; i < opaque->ncol; i++)
	{
		if (opaque->proj[i])
		{
			break;
		}
	}
	
	/*
	 * In some cases (for example, count(*)), no columns are specified.
	 * We always scan the first column.
	 */
	if (i == opaque->ncol)
	{
		opaque->proj[0] = true;
	}
}

static void
FreeAOCSScanOpaque(ScanState *scanState)
{
	AOCSScanState *state = (AOCSScanState *)scanState;
	Assert(state->opaque != NULL);

	AOCSScanOpaqueData *opaque = (AOCSScanOpaqueData *)state->opaque;
	Assert(opaque->proj != NULL);
	pfree(opaque->proj);
	pfree(state->opaque);
	state->opaque = NULL;
}

TupleTableSlot *
AOCSScanNext(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AOCSScanState *node = (AOCSScanState *)scanState;
	Assert(node->opaque != NULL &&
		   node->opaque->scandesc != NULL);

	aocs_getnext(node->opaque->scandesc, node->ss.ps.state->es_direction, node->ss.ss_ScanTupleSlot);
	return node->ss.ss_ScanTupleSlot;
}

void
BeginScanAOCSRelation(ScanState *scanState)
{
	Snapshot appendOnlyMetaDataSnapshot;

	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AOCSScanState *node = (AOCSScanState *)scanState;

	Assert(node->ss.scan_state == SCAN_INIT || node->ss.scan_state == SCAN_DONE);

	InitAOCSScanOpaque(scanState);
	   
	appendOnlyMetaDataSnapshot = node->ss.ps.state->es_snapshot;
	if (appendOnlyMetaDataSnapshot == SnapshotAny)
	{
		/* 
		 * the append-only meta data should never be fetched with
		 * SnapshotAny as bogus results are returned.
		 */
		appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
	}

	node->opaque->scandesc =
		aocs_beginscan(node->ss.ss_currentRelation, 
					   node->ss.ps.state->es_snapshot,
					   appendOnlyMetaDataSnapshot,
					   NULL /* relationTupleDesc */,
					   node->opaque->proj);

	node->ss.scan_state = SCAN_SCAN;
}
 
void
EndScanAOCSRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AOCSScanState *node = (AOCSScanState *)scanState;

	Assert((node->ss.scan_state & SCAN_SCAN) != 0);
	Assert(node->opaque != NULL &&
		   node->opaque->scandesc != NULL);

	aocs_endscan(node->opaque->scandesc);
        
	FreeAOCSScanOpaque(scanState);
	
	node->ss.scan_state = SCAN_INIT;
}

void
ReScanAOCSRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	AOCSScanState *node = (AOCSScanState *)scanState;
	Assert(node->opaque != NULL &&
		   node->opaque->scandesc != NULL);

	aocs_rescan(node->opaque->scandesc); 
}
