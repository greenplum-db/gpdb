/*
 * execAOCSScan.c
 *   Support routines for scanning AppendOnly Columnar tables.
 *
 * Copyright (c) 2012 - present, EMC/Greenplum
 */
#include "postgres.h"

#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "cdb/cdbaocsam.h"

static void
InitAOCSScanOpaque(TableScanState *state)
{
	Assert(state->opaque == NULL);
	state->opaque.aocs = palloc(sizeof(AOCSScanOpaqueData));

	/* Initialize AOCS projection info */
	AOCSScanOpaqueData *opaque = state->opaque.aocs;
	Relation currentRelation = state->ss.ss_currentRelation;
	Assert(currentRelation != NULL);

	opaque->ncol = currentRelation->rd_att->natts;
	opaque->proj = palloc0(sizeof(bool) * opaque->ncol);
	GetNeededColumnsForScan((Node *) state->ss.ps.plan->targetlist, opaque->proj, opaque->ncol);
	GetNeededColumnsForScan((Node *) state->ss.ps.plan->qual, opaque->proj, opaque->ncol);
	
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
FreeAOCSScanOpaque(TableScanState *state)
{
	Assert(state->opaque.aocs != NULL);

	Assert(state->opaque.aocs->proj != NULL);
	pfree(state->opaque.aocs->proj);
	pfree(state->opaque.aocs);
	state->opaque.aocs = NULL;
}

TupleTableSlot *
AOCSScanNext(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *)scanState;
	Assert(node->opaque.aocs != NULL &&
		   node->opaque.aocs->scandesc != NULL);

	aocs_getnext(node->opaque.aocs->scandesc, node->ss.ps.state->es_direction, node->ss.ss_ScanTupleSlot);
	return node->ss.ss_ScanTupleSlot;
}

void
BeginScanAOCSRelation(ScanState *scanState)
{
	Snapshot appendOnlyMetaDataSnapshot;

	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *)scanState;

	Assert(node->ss.scan_state == SCAN_INIT || node->ss.scan_state == SCAN_DONE);

	InitAOCSScanOpaque(node);

	appendOnlyMetaDataSnapshot = node->ss.ps.state->es_snapshot;
	if (appendOnlyMetaDataSnapshot == SnapshotAny)
	{
		/* 
		 * the append-only meta data should never be fetched with
		 * SnapshotAny as bogus results are returned.
		 */
		appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
	}

	node->opaque.aocs->scandesc =
		aocs_beginscan(node->ss.ss_currentRelation, 
					   node->ss.ps.state->es_snapshot,
					   appendOnlyMetaDataSnapshot,
					   NULL /* relationTupleDesc */,
					   node->opaque.aocs->proj);

	node->ss.scan_state = SCAN_SCAN;
}
 
void
EndScanAOCSRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *)scanState;

	Assert((node->ss.scan_state & SCAN_SCAN) != 0);
	Assert(node->opaque.aocs != NULL &&
		   node->opaque.aocs->scandesc != NULL);

	aocs_endscan(node->opaque.aocs->scandesc);
        
	FreeAOCSScanOpaque(node);
	
	node->ss.scan_state = SCAN_INIT;
}

void
ReScanAOCSRelation(ScanState *scanState)
{
	Assert(IsA(scanState, TableScanState) ||
		   IsA(scanState, DynamicTableScanState));
	TableScanState *node = (TableScanState *)scanState;

	Assert(node->opaque.aocs != NULL &&
		   node->opaque.aocs->scandesc != NULL);

	aocs_rescan(node->opaque.aocs->scandesc);
}
