/*-------------------------------------------------------------------------
 *
 * execBitmapAOScan.c
 *	  Support routines for scanning AO and AOCO tables using bitmaps.
 *
 * Portions Copyright (c) 2014-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execBitmapAOScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h" /* gp_select_invisible */
#include "executor/execdebug.h"
#include "executor/nodeBitmapAppendOnlyscan.h"
#include "miscadmin.h"
#include "nodes/tidbitmap.h"
#include "parser/parsetree.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

typedef struct
{
	int			tupleIndex;
	int			nTuples;
} AOIteratorState;

/*
 * Prepares for a new AO scan.
 */
void
BitmapAOScanBegin(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)(scanState);
	Relation currentRelation = node->ss.ss_currentRelation;
	EState *estate = node->ss.ps.state;

	Snapshot appendOnlyMetaDataSnapshot = estate->es_snapshot;
	if (appendOnlyMetaDataSnapshot == SnapshotAny)
	{
		/*
		 * the append-only meta data should never be fetched with
		 * SnapshotAny as bogus results are returned.
		 */
		appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
	}

	if (scanState->tableType == TableTypeAppendOnly)
	{
		node->scanDesc =
			appendonly_fetch_init(currentRelation,
								  estate->es_snapshot,
								  appendOnlyMetaDataSnapshot);
	}
	else if (scanState->tableType == TableTypeAOCS)
	{
		/*
		 * Obtain the projection.
		 */
		Assert(currentRelation->rd_att != NULL);

		bool *proj = (bool *)palloc0(sizeof(bool) * currentRelation->rd_att->natts);

		GetNeededColumnsForScan((Node *) node->ss.ps.plan->targetlist, proj, currentRelation->rd_att->natts);
		GetNeededColumnsForScan((Node *) node->ss.ps.plan->qual, proj, currentRelation->rd_att->natts);

		int colno = 0;

		/* Check if any column is projected */
		for(colno = 0; colno < currentRelation->rd_att->natts; colno++)
		{
			if(proj[colno])
			{
				break;
			}
		}

		/*
		 * At least project one column. Since the tids stored in the index may not have
		 * a corresponding tuple any more (because of previous crashes, for example), we
		 * need to read the tuple to make sure.
		 */
		if(colno == currentRelation->rd_att->natts)
		{
			proj[0] = true;
		}

		node->scanDesc =
			aocs_fetch_init(currentRelation, estate->es_snapshot, appendOnlyMetaDataSnapshot, proj);
	}
	else
	{
		Assert(!"Invalid table type");
	}


	/*
	 * AO doesn't need rechecking every tuple once it resolves
	 * from the bitmap page, except when it deals with lossy
	 * bitmap, which is handled via scanState->isLossyBitmapPage.
	 */
	node->recheckTuples = false;
}

/*
 * Cleans up after the scanning is done.
 */
void
BitmapAOScanEnd(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)scanState;

	/*
	 * We might call "End" method before even calling init method,
	 * in case we had an ERROR. Ignore scanDesc cleanup in such cases
	 */
	if (NULL != node->scanDesc)
	{
		if (scanState->tableType == TableTypeAppendOnly)
		{
			appendonly_fetch_finish((AppendOnlyFetchDesc)node->scanDesc);
		}
		else if (scanState->tableType == TableTypeAOCS)
		{
			aocs_fetch_finish(node->scanDesc);
		}
		else
		{
			Assert(!"Invalid table type");
		}
		pfree(node->scanDesc);
		node->scanDesc = NULL;
	}

	if (NULL != node->iterator)
	{
		pfree(node->iterator);
		node->iterator = NULL;
	}
}

/*
 * Returns the next matching tuple.
 */
TupleTableSlot *
BitmapAOScanNext(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)scanState;

	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;

	TBMIterateResult *tbmres = (TBMIterateResult *)node->tbmres;

	/* Make sure we never cross 15-bit offset number [MPP-24326] */
	Assert(tbmres->ntuples <= INT16_MAX + 1);

	OffsetNumber psuedoHeapOffset;
	ItemPointerData psudeoHeapTid;
	AOTupleId aoTid;

	Assert(tbmres != NULL && tbmres->ntuples != 0);
	Assert(node->needNewBitmapPage == false);

	AOIteratorState *iterator = (AOIteratorState *)node->iterator;
	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (iterator == NULL)
		{
			iterator = palloc0(sizeof(AOIteratorState));

			if (node->isLossyBitmapPage)
			{
				/* Iterate over the first 2^15 tuples [MPP-24326] */
				iterator->nTuples = INT16_MAX + 1;
			}
			else
			{
				iterator->nTuples = tbmres->ntuples;
			}
			/* Start from the beginning of the page */
			iterator->tupleIndex = 0;

			node->iterator = iterator;
		}
		else
		{
			/*
			 * Continuing in previously obtained page; advance tupleIndex
			 */
			iterator->tupleIndex++;
		}

		/*
		 * Out of range?  If so, nothing more to look at on this page
		 */
		if (iterator->tupleIndex < 0 || iterator->tupleIndex >= iterator->nTuples)
		{
			pfree(iterator);

			node->iterator = NULL;

			node->needNewBitmapPage = true;

			return ExecClearTuple(slot);
		}

		/*
		 * Must account for lossy page info...
		 */
		if (node->isLossyBitmapPage)
		{
			/* We are iterating through all items. */
			psuedoHeapOffset = iterator->tupleIndex;
		}
		else
		{
			Assert(iterator->tupleIndex <= tbmres->ntuples);
			psuedoHeapOffset = tbmres->offsets[iterator->tupleIndex];

			/*
			 * Ensure that the reserved 16-th bit is always ON for offsets from
			 * lossless bitmap pages [MPP-24326].
			 */
			Assert(((uint16)(psuedoHeapOffset & 0x8000)) > 0);
		}

		/*
		 * Okay to fetch the tuple
		 */
		ItemPointerSet(
				&psudeoHeapTid,
				tbmres->blockno,
				psuedoHeapOffset);

		tbm_convert_appendonly_tid_out(&psudeoHeapTid, &aoTid);

		if (scanState->tableType == TableTypeAppendOnly)
		{
			appendonly_fetch((AppendOnlyFetchDesc)node->scanDesc, &aoTid, slot);
		}
		else
		{
			Assert(scanState->tableType == TableTypeAOCS);
			aocs_fetch((AOCSFetchDesc)node->scanDesc, &aoTid, slot);
		}

      	if (TupIsNull(slot))
      	{
			continue;
      	}

		Assert(ItemPointerIsValid(slot_get_ctid(slot)));

		pgstat_count_heap_fetch(node->ss.ss_currentRelation);

		if (!BitmapTableScanRecheckTuple(node, slot))
		{
			ExecClearTuple(slot);
			continue;
		}

		return slot;
	}

	/*
	 * We should never reach here as the termination is handled
	 * from nodeBitmapTableScan.
	 */
	Assert(false);
	return NULL;
}

/*
 * Prepares for a re-scan.
 */
void
BitmapAOScanReScan(ScanState *scanState)
{
	/*
	 * AO doesn't need rechecking every tuple once it resolves
	 * from the bitmap page, except when it deals with lossy
	 * bitmap, which is handled via scanState->isLossyBitmapPage.
	 */
	BitmapTableScanState *node = (BitmapTableScanState *)(scanState);
	node->recheckTuples = false;

	/*
	 * As per the existing implementation from nodeBitmapAppendOnlyScan.c
	 * for rescanning of AO, we don't have anything else
	 * to do here (the refactored BitmapTableScan takes care of everything).
	 */
}

