/*-------------------------------------------------------------------------
 *
 * nodeSeqscan.c
 *	  Support routines for sequential scans of relations.
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeSeqscan.c,v 1.64 2008/01/01 19:45:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecSeqScan				sequentially scans a relation.
 *		ExecSeqNext				retrieve next tuple in sequential order.
 *		ExecInitSeqScan			creates and initializes a seqscan node.
 *		ExecEndSeqScan			releases any storage allocated.
 *		ExecSeqReScan			rescans the relation
 *		ExecSeqMarkPos			marks scan position
 *		ExecSeqRestrPos			restores scan position
 */
#include "postgres.h"

#include "access/heapam.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"

#include "parser/parsetree.h"
#include "utils/lsyscache.h"

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "cdb/cdbgang.h"

static void InitScanRelation(SeqScanState *node, EState *estate);

/* Open and close scanned relation Open/Close means resource acquisition.
 */
static void OpenScanRelation(SeqScanState *node);
static void CloseScanRelation(SeqScanState *node);

static TupleTableSlot *SeqNext(SeqScanState *node);

/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *		SeqNext
 *
 *		This is a workhorse for ExecSeqScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
SeqNext(SeqScanState *node)
{
	HeapTuple	tuple;
	HeapScanDesc scandesc;
	Index		scanrelid;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	Assert((node->scan_state & SCAN_SCAN) != 0);

	/*
	 * get information from the estate and scan state
	 */
	estate = node->ps.state;
	scandesc = node->ss_currentScanDesc;
	scanrelid = ((SeqScan *) node->ps.plan)->scanrelid;
	direction = estate->es_direction;
	slot = node->ss_ScanTupleSlot;

	/*
	 * Check if we are evaluating PlanQual for tuple of this relation.
	 * Additional checking is not good, but no other way for now. We could
	 * introduce new nodes for this case and handle SeqScan --> NewNode
	 * switching in Init/ReScan plan...
	 */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		if (estate->es_evTupleNull[scanrelid - 1])
			return ExecClearTuple(slot);

		ExecStoreGenericTuple(estate->es_evTuple[scanrelid - 1], slot, false);

		/*
		 * Note that unlike IndexScan, SeqScan never use keys in
		 * heap_beginscan (and this is very bad) - so, here we do not check
		 * are keys ok or not.
		 */

		/* Flag for the next call that no more tuples */
		estate->es_evTupleNull[scanrelid - 1] = true;
		return slot;
	}

	/*
	 * get the next tuple from the access methods
	 */
	/* CKTAN- */
	if (node->ss_heapTupleData.bot == node->ss_heapTupleData.top
		&& !node->ss_heapTupleData.seen_EOS)
	{
		node->ss_heapTupleData.last = NULL;
		node->ss_heapTupleData.bot = 0;
		node->ss_heapTupleData.top = lengthof(node->ss_heapTupleData.item);
		heap_getnextx(scandesc, direction, node->ss_heapTupleData.item,
					  &node->ss_heapTupleData.top,
					  &node->ss_heapTupleData.seen_EOS);

		/* Gpmon */
		if(scandesc->rs_pageatatime)
                {
			Gpmon_M_Incr(GpmonPktFromSeqScanState(node), GPMON_SEQSCAN_PAGE);
                        CheckSendPlanStateGpmonPkt(&node->ps);
                }
	}

	node->ss_heapTupleData.last =
		((node->ss_heapTupleData.bot < node->ss_heapTupleData.top)
		 ? &node->ss_heapTupleData.item[node->ss_heapTupleData.bot++]
		 : 0);
	tuple = node->ss_heapTupleData.last;

	/*
	 * save the tuple and the buffer returned to us by the access methods in
	 * our scan tuple slot and return the slot.  Note: we pass 'false' because
	 * tuples returned by heap_getnext() are pointers onto disk pages and were
	 * not created with palloc() and so should not be pfree()'d.  Note also
	 * that ExecStoreTuple will increment the refcount of the buffer; the
	 * refcount will not be dropped until the tuple table slot is cleared.
	 */
	if (tuple)
		ExecStoreHeapTuple(tuple,	/* tuple to store */
					   slot,	/* slot to store in */
					   scandesc->rs_cbuf,		/* buffer associated with this
												 * tuple */
					   false);	/* don't pfree this pointer */
	else
		ExecClearTuple(slot);

	return slot;
}

/* ----------------------------------------------------------------
 *		ExecSeqScan(node)
 *
 *		Scans the relation sequentially and returns the next qualifying
 *		tuple.
 *		It calls the ExecScan() routine and passes it the access method
 *		which retrieve tuples sequentially.
 *
 */

TupleTableSlot *
ExecSeqScan(SeqScanState *node)
{
	/*
	 * use SeqNext as access method
	 */
	TupleTableSlot *slot;

	if((node->scan_state & SCAN_SCAN) == 0)
		OpenScanRelation(node);

	slot = ExecScan((ScanState *) node, (ExecScanAccessMtd) SeqNext);
     	if (!TupIsNull(slot))
        {
     		Gpmon_M_Incr_Rows_Out(GpmonPktFromSeqScanState(node));
                CheckSendPlanStateGpmonPkt(&node->ps);
        }

	if(TupIsNull(slot) && !node->ps.delayEagerFree)
	{
		CloseScanRelation(node);
	}

	return slot;
}

/* ----------------------------------------------------------------
 *		InitScanRelation
 *
 *		This does the initialization for scan relations and
 *		subplans of scans.
 * ----------------------------------------------------------------
 */
static void
InitScanRelation(SeqScanState *node, EState *estate)
{
	Relation	currentRelation;

	/*
	 * get the relation object id from the relid'th entry in the range table,
	 * open that relation and acquire appropriate lock on it.
	 */
	currentRelation = ExecOpenScanRelation(estate, ((SeqScan *) node->ps.plan)->scanrelid);

	node->ss_currentRelation = currentRelation;
	ExecAssignScanType(node, RelationGetDescr(currentRelation));
}

/* ---------------------------------------------------------------
 * Open underlying relation for scan.
 * Open means resource acquisition, in seq scan, file descriptor.
 *
 * We delay open as much as possible, and later, close as early as
 * possible to prevent from hogging lots of resources.  This is
 * very important because now some user partition a talbe to thousands
 * of smaller ones.  If we are not careful, we cannot even do a
 * scan on the table.
 *
 * We need to do this for all the scanners.  See nodeAppendOnlyscan.c
 * as well.
 * --------------------------------------------------------------
 */
static void
OpenScanRelation(SeqScanState *node)
{
	Assert(node->scan_state == SCAN_INIT || node->scan_state == SCAN_DONE);
	Assert(!node->ss_currentScanDesc);

	node->ss_currentScanDesc = heap_beginscan(
			node->ss_currentRelation,
			node->ps.state->es_snapshot,
			0,
			NULL);

	/* CKTAN- */
	node->ss_heapTupleData.bot = node->ss_heapTupleData.top
		= node->ss_heapTupleData.seen_EOS = 0;
	node->ss_heapTupleData.last = NULL;

	node->scan_state = SCAN_SCAN;
}

/* ----------------------------------------------------------------
 * Close the scanned relation.
 * Close means release resource.
 * ----------------------------------------------------------------
 */
static void
CloseScanRelation(SeqScanState *node)
{
	Assert((node->scan_state & SCAN_SCAN) != 0);

	heap_endscan(node->ss_currentScanDesc);

	node->ss_heapTupleData.top = 0;
	node->ss_heapTupleData.bot = 0;
	node->ss_heapTupleData.seen_EOS = 1;
	node->ss_heapTupleData.last = NULL;
	node->ss_currentScanDesc = NULL;
	node->scan_state = SCAN_INIT;
}


/* ----------------------------------------------------------------
 *		ExecInitSeqScan
 * ----------------------------------------------------------------
 */
SeqScanState *
ExecInitSeqScan(SeqScan *node, EState *estate, int eflags)
{
	SeqScanState *scanstate;

	/*
	 * Once upon a time it was possible to have an outerPlan of a SeqScan, but
	 * not any more.
	 */
	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);

	/*
	 * create state structure
	 */
	scanstate = makeNode(SeqScanState);
	scanstate->ps.plan = (Plan *) node;
	scanstate->ps.state = estate;
	scanstate->ss->scan_state = SCAN_INIT;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->ps);

	/*
	 * initialize child expressions
	 */
	scanstate->ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) scanstate);
	scanstate->ps.qual = (List *)
		ExecInitExpr((Expr *) node->plan.qual,
					 (PlanState *) scanstate);

#define SEQSCAN_NSLOTS 2

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &scanstate->ps);
	ExecInitScanTupleSlot(estate, scanstate);

	/*
	 * initialize scan relation
	 */
	InitScanRelation(scanstate, estate);

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&scanstate->ps);
	ExecAssignScanProjectionInfo(scanstate);

	initGpmonPktForSeqScan((Plan *)node, &scanstate->ps.gpmon_pkt, estate);

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	scanstate->ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);

	return scanstate;
}

int
ExecCountSlotsSeqScan(SeqScan *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		SEQSCAN_NSLOTS;
}

/* ----------------------------------------------------------------
 *		ExecEndSeqScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndSeqScan(SeqScanState *node)
{
	Relation	relation;

	/*
	 * get information from node
	 */
	relation = node->ss_currentRelation;

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss_ScanTupleSlot);

	ExecEagerFreeSeqScan(node);

	/*
	 * close the heap relation.
	 */
	ExecCloseScanRelation(relation);

	/* gpmon */
	EndPlanStateGpmonPkt(&node->ps);
}

/* ----------------------------------------------------------------
 *						Join Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecSeqReScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecSeqReScan(SeqScanState *node, ExprContext *exprCtxt)
{
	EState	   *estate;
	Index		scanrelid;
	HeapScanDesc scan;

	estate = node->ps.state;
	scanrelid = ((SeqScan *) node->ps.plan)->scanrelid;

	/*node->ps.ps_TupFromTlist = false;*/

	/* If this is re-scanning of PlanQual ... */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		estate->es_evTupleNull[scanrelid - 1] = false;
		return;
	}

	if((node->scan_state & SCAN_SCAN) == 0)
		OpenScanRelation(node);

	scan = node->ss_currentScanDesc;

	heap_rescan(scan,			/* scan desc */
				NULL);			/* new scan keys */
	/* CKTAN */
	node->ss_heapTupleData.bot = node->ss_heapTupleData.top
		= node->ss_heapTupleData.seen_EOS = 0;
	node->ss_heapTupleData.last = NULL;

	Gpmon_M_Incr(GpmonPktFromSeqScanState(node), GPMON_SEQSCAN_RESCAN);
	CheckSendPlanStateGpmonPkt(&node->ps);
}

/* ----------------------------------------------------------------
 *		ExecSeqMarkPos(node)
 *
 *		Marks scan position.
 * ----------------------------------------------------------------
 */
void
ExecSeqMarkPos(SeqScanState *node)
{
	HeapScanDesc scan = node->ss_currentScanDesc;

	Assert((node->scan_state & SCAN_SCAN) != 0);

	/* CKTAN
	heap_markpos(scan);
	*/
	heap_markposx(scan, node->ss_heapTupleData.last);

	node->scan_state |= SCAN_MARKPOS;
}

/* ----------------------------------------------------------------
 *		ExecSeqRestrPos
 *
 *		Restores scan position.
 * ----------------------------------------------------------------
 */
void
ExecSeqRestrPos(SeqScanState *node)
{
	HeapScanDesc scan = node->ss_currentScanDesc;

	Assert((node->scan_state & SCAN_SCAN) != 0);
	Assert((node->scan_state & SCAN_MARKPOS) != 0);

	/*
	 * Clear any reference to the previously returned tuple.  This is needed
	 * because the slot is simply pointing at scan->rs_cbuf, which
	 * heap_restrpos will change; we'd have an internally inconsistent slot if
	 * we didn't do this.
	 */
	ExecClearTuple(node->ss_ScanTupleSlot);

	heap_restrpos(scan);
	Gpmon_M_Incr(GpmonPktFromSeqScanState(node), GPMON_SEQSCAN_RESTOREPOS);
	CheckSendPlanStateGpmonPkt(&node->ps);

	node->scan_state &= (~ ((int) SCAN_MARKPOS));
}

/* Gpmon helpers. */
char * GetScanRelNameGpmon(Oid relid, char schema_rel_name[SCAN_REL_NAME_BUF_SIZE])
{
	if (relid > 0)
	{
		char *relname = get_rel_name(relid);
		char *schemaname = get_namespace_name(get_rel_namespace(relid));
		snprintf(schema_rel_name, SCAN_REL_NAME_BUF_SIZE, "%s.%s", schemaname, relname);
		if (relname)
			pfree(relname);
		if (schemaname)
			pfree(schemaname);
	}
	return schema_rel_name;
}

/*
 * toSendGpmonExecutingPkt
 *   Return true if a perfmon executing packet needs to be sent for the given plan node.
 *
 * In most cases, this function will return true. The exception is for a Sort/Material
 * that is called by the SharedNode and is not in a driver slice.
 */
static bool
toSendGpmonExecutingPkt(Plan *plan, EState *estate)
{
	bool doSend = true;
	Assert(plan != NULL);
	if (IsA(plan, Material))
	{
		Material *ma = (Material *)plan;
		doSend = (ma->share_type == SHARE_NOTSHARED ||
				  ma->share_type == SHARE_MATERIAL ||
				  ma->driver_slice == LocallyExecutingSliceIndex(estate));
	}
	
	else if (IsA(plan, Sort))
	{
		Sort *sort = (Sort *)plan;
		doSend = (sort->share_type == SHARE_NOTSHARED ||
				  sort->share_type == SHARE_MATERIAL ||
				  sort->driver_slice == LocallyExecutingSliceIndex(estate));
		
	}

	return doSend;
}


void CheckSendPlanStateGpmonPkt(PlanState *ps)
{
	if(ps == NULL ||
	   ps->state == NULL)
	{
		return;
	}

	if(gp_enable_gpperfmon)
	{
		/*
		 * When Sort/Material is called by the SharedNode that is not
		 * a driver slice, do not send gpmon packet.
		 */
		Assert(ps->plan != NULL);
		bool doSend = toSendGpmonExecutingPkt(ps->plan, ps->state);
		if (!doSend)
		{
			return;
		}

		if(!ps->fHadSentGpmon || ps->gpmon_plan_tick != gpmon_tick)
		{
			gpmon_send(&ps->gpmon_pkt);
			ps->fHadSentGpmon = true;
		}

		ps->gpmon_plan_tick = gpmon_tick;
	}
}

/*
 * NodeInExecutingSlice
 *   Return true if the given node is running in the same slice of the
 * executing QE.
 *
 * Each motion node has a sender and a receiver slice, so for the same
 * motion node, this function returns true in two slices.
 *
 * For all other nodes, the node's slice id is stored in
 * estate->currentExecutingSliceId.
 */
static bool
NodeInExecutingSlice(Plan *plan, EState *estate)
{
	Assert(plan != NULL && estate != NULL);
	
	/*
	 * For Motion node, check either the sender slice or the receiver
	 * slice is running on the same slice as this QE. For the Motion
	 * nodes in the top slice of an InitPlan, the executing slice
	 * id might be different from the receiver slice id. We need to
	 * check that as well.
	 */
	if (IsA(plan, Motion))
	{
		Motion *motion = (Motion *)plan;

		SliceTable *sliceTable = estate->es_sliceTable;

		Slice *sendSlice = (Slice *)list_nth(sliceTable->slices, motion->motionID);
		Assert(sendSlice->parentIndex >= 0);
		Slice *recvSlice = (Slice *)list_nth(sliceTable->slices, sendSlice->parentIndex);
		int localSliceId = LocallyExecutingSliceIndex(estate);
		
		return (localSliceId == recvSlice->sliceIndex ||
				localSliceId == sendSlice->sliceIndex ||
				localSliceId == estate->currentExecutingSliceId);
	}

	/* For other nodes except for Motion, simple check estate->currentExecutingSliceId. */
	return (LocallyExecutingSliceIndex(estate) == estate->currentExecutingSliceId);
}

void EndPlanStateGpmonPkt(PlanState *ps)
{
	if(ps == NULL ||
	   ps->state == NULL)
	{
		return;
	}

	/*
	 * If this operator is not running in this slice, do not
	 * send the finished packet.
	 */
	if (!NodeInExecutingSlice(ps->plan, ps->state))
	{
		return;
	}

	ps->gpmon_pkt.u.qexec.status = (uint8)PMNS_Finished;

	if(gp_enable_gpperfmon)
	{
		gpmon_send(&ps->gpmon_pkt);
	}
}

/*
 * InitPlanNodeGpmonPkt -- initialize the init gpmon package, and send it off.
 */
void InitPlanNodeGpmonPkt(Plan *plan, gpmon_packet_t *gpmon_pkt, EState *estate,
						  PerfmonNodeType type,
						  int64 rowsout_est,
						  char* relname)
{
	int rowsout_adjustment_factor = 0;

	if (plan == NULL ||
		estate == NULL)
		return;

	/*
	 * If this operator is not running in this slice, do not
	 * send the init packet.
	 */
	if (!NodeInExecutingSlice(plan, estate))
	{
		return;
	}

	/* The estimates are now global so we need to adjust by
	 * the number of segments in the array.
	 */
	rowsout_adjustment_factor = getgpsegmentCount();

	/* Make sure we don't div by zero below */
	if (rowsout_adjustment_factor < 1)
		rowsout_adjustment_factor = 1;

	Assert(rowsout_adjustment_factor >= 1);

	memset(gpmon_pkt, 0, sizeof(gpmon_packet_t));

	gpmon_pkt->magic = GPMON_MAGIC;
	gpmon_pkt->version = GPMON_PACKET_VERSION;
	gpmon_pkt->pkttype = GPMON_PKTTYPE_QEXEC;

	gpmon_gettmid(&gpmon_pkt->u.qexec.key.tmid);
	gpmon_pkt->u.qexec.key.ssid = gp_session_id;
	gpmon_pkt->u.qexec.key.ccnt = gp_command_count;
	gpmon_pkt->u.qexec.key.hash_key.segid = Gp_segment;
	gpmon_pkt->u.qexec.key.hash_key.pid = MyProcPid;
	gpmon_pkt->u.qexec.key.hash_key.nid = plan->plan_node_id;

	gpmon_pkt->u.qexec.pnid = plan->plan_parent_node_id;


	gpmon_pkt->u.qexec.nodeType = (apr_uint16_t)type;

	gpmon_pkt->u.qexec.rowsout = 0;
	gpmon_pkt->u.qexec.rowsout_est = rowsout_est / rowsout_adjustment_factor;

	if (relname)
	{
		snprintf(gpmon_pkt->u.qexec.relation_name, sizeof(gpmon_pkt->u.qexec.relation_name), "%s", relname);
	}

	gpmon_pkt->u.qexec.status = (uint8)PMNS_Initialize;

	if(gp_enable_gpperfmon)
	{
		gpmon_send(gpmon_pkt);
	}

	gpmon_pkt->u.qexec.status = (uint8)PMNS_Executing;
}

void
initGpmonPktForSeqScan(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, SeqScan));

	{
		RangeTblEntry *rte = rt_fetch(((Scan *)planNode)->scanrelid, estate->es_range_table);
		char schema_rel_name[SCAN_REL_NAME_BUF_SIZE] = {0};
		
		Assert(GPMON_SEQSCAN_TOTAL <= (int)GPMON_QEXEC_M_COUNT);
		InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_SeqScan,
							 (int64) planNode->plan_rows, GetScanRelNameGpmon(rte->relid, schema_rel_name));
	}
}

void
ExecEagerFreeSeqScan(SeqScanState *node)
{
	if (((node->scan_state & SCAN_SCAN) != 0))
	{
		CloseScanRelation(node);
	}
}
