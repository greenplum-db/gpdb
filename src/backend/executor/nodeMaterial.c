/*-------------------------------------------------------------------------
 *
 * nodeMaterial.c
 *	  Routines to handle materialization nodes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeMaterial.c,v 1.69 2009/06/11 14:48:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecMaterial			- materialize the result of a subplan
 *		ExecInitMaterial		- initialize node and subnodes
 *		ExecEndMaterial			- shutdown node and subnodes
 *
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeMaterial.h"
#include "executor/instrument.h"        /* Instrumentation */
#include "utils/tuplestorenew.h"

#include "miscadmin.h"

#include "cdb/cdbvars.h"

static void ExecMaterialExplainEnd(PlanState *planstate, struct StringInfoData *buf);
static void ExecChildRescan(MaterialState *node, ExprContext *exprCtxt);
static void DestroyTupleStore(MaterialState *node);


/* ----------------------------------------------------------------
 *		ExecMaterial
 *
 *		As long as we are at the end of the data collected in the tuplestore,
 *		we collect one new row from the subplan on each call, and stash it
 *		aside in the tuplestore before returning it.  The tuplestore is
 *		only read if we are asked to scan backwards, rescan, or mark/restore.
 *
 * ----------------------------------------------------------------
 */
TupleTableSlot *				/* result tuple from subplan */
ExecMaterial(MaterialState *node)
{
	EState	   *estate;
	ScanDirection dir;
	bool		forward;

	NTupleStore *ts;
	NTupleStoreAccessor *tsa;

	bool		eof_tuplestore;
	TupleTableSlot *slot;
	Material *ma;

	/*
	 * get state info from node
	 */
	estate = node->ss.ps.state;
	dir = estate->es_direction;
	forward = ScanDirectionIsForward(dir);

	ts = node->ts_state->matstore;
	tsa = (NTupleStoreAccessor *) node->ts_pos;

	ma = (Material *) node->ss.ps.plan;
	Assert(IsA(ma, Material));

	/*
	 * If first time through, and we need a tuplestore, initialize it.
	 */
	if (ts == NULL && (ma->share_type != SHARE_NOTSHARED || node->eflags != 0))
	{
		/*
		 * For cross slice material, we only run ExecMaterial on DriverSlice
		 */
		if(ma->share_type == SHARE_MATERIAL_XSLICE)
		{
			char rwfile_prefix[100];

			if(ma->driver_slice != currentSliceId)
			{
				elog(LOG, "Material Exec on CrossSlice, current slice %d", currentSliceId);
				return NULL;
			}

			shareinput_create_bufname_prefix(rwfile_prefix, sizeof(rwfile_prefix), ma->share_id);
			elog(LOG, "Material node creates shareinput rwfile %s", rwfile_prefix);

			ts = ntuplestore_create_readerwriter(rwfile_prefix, PlanStateOperatorMemKB((PlanState *)node) * 1024, true);
			tsa = ntuplestore_create_accessor(ts, true);
		}
		else
		{
			/* Non-shared Materialize node */
			workfile_set *work_set =  workfile_mgr_create_set(BUFFILE, false /* can_reuse */, &node->ss.ps);

			ts = ntuplestore_create_workset(work_set, PlanStateOperatorMemKB((PlanState *) node) * 1024);
			tsa = ntuplestore_create_accessor(ts, true /* isWriter */);
		}

		Assert(ts && tsa);
		node->ts_state->matstore = ts;
		node->ts_pos = (void *) tsa;

        /* CDB: Offer extra info for EXPLAIN ANALYZE. */
        if (node->ss.ps.instrument)
        {
            /* Let the tuplestore share our Instrumentation object. */
			ntuplestore_setinstrument(ts, node->ss.ps.instrument);

            /* Request a callback at end of query. */
            node->ss.ps.cdbexplainfun = ExecMaterialExplainEnd;
        }

		/*
		 * MPP: If requested, fetch all rows from subplan and put them
		 * in the tuplestore.  This decouples a middle slice's receiving
		 * and sending Motion operators to neutralize a deadlock hazard.
		 * MPP TODO: Remove when a better solution is implemented.
		 *
		 * See motion_sanity_walker() for details on how a deadlock may occur.
		 *
		 * ShareInput: if the material node
		 * is used to share input, we will need to fetch all rows and put
		 * them in tuple store
		 */
		while (((Material *) node->ss.ps.plan)->cdb_strict
				|| ma->share_type != SHARE_NOTSHARED)
		{
			TupleTableSlot *outerslot = ExecProcNode(outerPlanState(node));

			if (TupIsNull(outerslot))
			{
				node->eof_underlying = true;
				ntuplestore_acc_seek_bof(tsa);

				break;
			}

			ntuplestore_acc_put_tupleslot(tsa, outerslot);
		}

		CheckSendPlanStateGpmonPkt(&node->ss.ps);

		if(forward)
			ntuplestore_acc_seek_bof(tsa);
		else
			ntuplestore_acc_seek_eof(tsa);

		/* for share input, material do not need to return any tuple */
		if(ma->share_type != SHARE_NOTSHARED)
		{
			Assert(ma->share_type == SHARE_MATERIAL || ma->share_type == SHARE_MATERIAL_XSLICE);
			/*
			 * if the material is shared across slice, notify consumers that
			 * it is ready.
			 */
			if (ma->share_type == SHARE_MATERIAL_XSLICE)
			{
				if (ma->driver_slice == currentSliceId)
				{
					ntuplestore_flush(ts);

					node->share_lk_ctxt = shareinput_writer_notifyready(ma->share_id, ma->nsharer_xslice,
							estate->es_plannedstmt->planGen);
				}
			}
			return NULL;
		}
	}

	if(ma->share_type != SHARE_NOTSHARED)
		return NULL;

	/*
	 * If we can fetch another tuple from the tuplestore, return it.
	 */
	slot = node->ss.ps.ps_ResultTupleSlot;

	if(forward)
		eof_tuplestore = (tsa == NULL) || !ntuplestore_acc_advance(tsa, 1);
	else
		eof_tuplestore = (tsa == NULL) || !ntuplestore_acc_advance(tsa, -1);

	if(tsa != NULL && ntuplestore_acc_tell(tsa, NULL))
	{
		ntuplestore_acc_current_tupleslot(tsa, slot);
		return slot;
	}

	/*
	 * If necessary, try to fetch another row from the subplan.
	 *
	 * Note: the eof_underlying state variable exists to short-circuit further
	 * subplan calls.  It's not optional, unfortunately, because some plan
	 * node types are not robust about being called again when they've already
	 * returned NULL.
	 * If reusing cached workfiles, there is no need to execute subplan at all.
	 */
	if (eof_tuplestore && !node->eof_underlying)
	{
		PlanState  *outerNode;
		TupleTableSlot *outerslot;

		/*
		 * We can only get here with forward==true, so no need to worry about
		 * which direction the subplan will go.
		 */
		outerNode = outerPlanState(node);
		outerslot = ExecProcNode(outerNode);
		if (TupIsNull(outerslot))
		{
			node->eof_underlying = true;
			if (!node->ss.ps.delayEagerFree)
			{
				ExecEagerFreeMaterial(node);
			}

			return NULL;
		}

		if (tsa)
			ntuplestore_acc_put_tupleslot(tsa, outerslot);

		/*
		 * We can just return the subplan's returned tuple, without copying.
		 */
		return outerslot;
	}


	if (!node->ss.ps.delayEagerFree)
	{
		ExecEagerFreeMaterial(node);
	}

	/*
	 * Nothing left ...
	 */
	return NULL;
}

/* ----------------------------------------------------------------
 *		ExecInitMaterial
 * ----------------------------------------------------------------
 */
MaterialState *
ExecInitMaterial(Material *node, EState *estate, int eflags)
{
	MaterialState *matstate;
	Plan	   *outerPlan;

	/*
	 * create state structure
	 */
	matstate = makeNode(MaterialState);
	matstate->ss.ps.plan = (Plan *) node;
	matstate->ss.ps.state = estate;

	if (node->cdb_strict)
		eflags |= EXEC_FLAG_REWIND;

	/*
	 * We must have a tuplestore buffering the subplan output to do backward
	 * scan or mark/restore.  We also prefer to materialize the subplan output
	 * if we might be called on to rewind and replay it many times. However,
	 * if none of these cases apply, we can skip storing the data.
	 */
	matstate->eflags = (eflags & (EXEC_FLAG_REWIND |
								  EXEC_FLAG_BACKWARD |
								  EXEC_FLAG_MARK));

	/*
	 * Tuplestore's interpretation of the flag bits is subtly different from
	 * the general executor meaning: it doesn't think BACKWARD necessarily
	 * means "backwards all the way to start".	If told to support BACKWARD we
	 * must include REWIND in the tuplestore eflags, else tuplestore_trim
	 * might throw away too much.
	 */
	if (eflags & EXEC_FLAG_BACKWARD)
		matstate->eflags |= EXEC_FLAG_REWIND;

	matstate->eof_underlying = false;
	matstate->ts_state = palloc0(sizeof(GenericTupStore));
	matstate->ts_pos = NULL;
	matstate->ts_markpos = NULL;
	matstate->share_lk_ctxt = NULL;
	matstate->ts_destroyed = false;

	/*
	 * Miscellaneous initialization
	 *
	 * Materialization nodes don't need ExprContexts because they never call
	 * ExecQual or ExecProject.
	 */

#define MATERIAL_NSLOTS 2

	/*
	 * tuple table initialization
	 *
	 * material nodes only return tuples from their materialized relation.
	 */
	ExecInitResultTupleSlot(estate, &matstate->ss.ps);
	matstate->ss.ss_ScanTupleSlot = ExecInitExtraTupleSlot(estate);

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	matstate->ss.ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);

	/*
	 * initialize child nodes
	 *
	 * We shield the child node from the need to support BACKWARD, or
	 * MARK/RESTORE.
	 */
	eflags &= ~(EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK);

	/*
	 * If Materialize does not have any external parameters, then it
	 * can shield the child node from being rescanned as well, hence
	 * we can clear the EXEC_FLAG_REWIND as well. If there are parameters,
	 * don't clear the REWIND flag, as the child will be rewound.
	 */
	if (node->plan.allParam == NULL || node->plan.extParam == NULL)
	{
		eflags &= ~EXEC_FLAG_REWIND;
	}

	outerPlan = outerPlan(node);
	/*
	 * A very basic check to see if the optimizer requires the material to do a projection.
	 * Ideally, this check would recursively compare all the target list expressions. However,
	 * such a check is tricky because of the varno mismatch (outer plan may have a varno that
	 * index into range table, while the material may refer to the same relation as "outer" varno)
	 * [JIRA: MPP-25365]
	 */
	insist_log(list_length(node->plan.targetlist) == list_length(outerPlan->targetlist),
			"Material operator does not support projection");
	outerPlanState(matstate) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * If the child node of a Material is a Motion, then this Material node is
	 * not eager free safe.
	 */
	if (IsA(outerPlan((Plan *)node), Motion))
	{
		matstate->ss.ps.delayEagerFree = true;
	}

	/*
	 * initialize tuple type.  no need to initialize projection info because
	 * this node doesn't do projections.
	 */
	ExecAssignResultTypeFromTL(&matstate->ss.ps);
	ExecAssignScanTypeFromOuterPlan(&matstate->ss);
	matstate->ss.ps.ps_ProjInfo = NULL;

	/*
	 * If share input, need to register with range table entry
	 */
	if (node->share_type != SHARE_NOTSHARED)
	{
		ShareNodeEntry *snEntry = ExecGetShareNodeEntry(estate, node->share_id, true);
		snEntry->sharePlan = (Node *) node;
		snEntry->shareState = (Node *) matstate;
	}

	return matstate;
}

int
ExecCountSlotsMaterial(Material *node)
{
	return ExecCountSlotsNode(outerPlan((Plan *) node)) +
		ExecCountSlotsNode(innerPlan((Plan *) node)) +
		MATERIAL_NSLOTS;
}


/*
 * ExecMaterialExplainEnd
 *      Called before ExecutorEnd to finish EXPLAIN ANALYZE reporting.
 *
 * Some of the cleanup that ordinarily would occur during ExecEndMaterial()
 * needs to be done earlier in order to report statistics to EXPLAIN ANALYZE.
 * Note that ExecEndMaterial() will be called again during ExecutorEnd().
 */
void
ExecMaterialExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	ExecEagerFreeMaterial((MaterialState*)planstate);
}                               /* ExecMaterialExplainEnd */


/* ----------------------------------------------------------------
 *		ExecEndMaterial
 * ----------------------------------------------------------------
 */
void
ExecEndMaterial(MaterialState *node)
{
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	ExecEagerFreeMaterial(node);

	/*
	 * Release tuplestore resources for cases where EagerFree doesn't do it
	 */
	if (node->ts_state->matstore != NULL)
	{
		Material   *ma = (Material *) node->ss.ps.plan;
		if (ma->share_type == SHARE_MATERIAL_XSLICE && node->share_lk_ctxt)
		{
			shareinput_writer_waitdone(node->share_lk_ctxt, ma->share_id, ma->nsharer_xslice);
		}
		Assert(node->ts_pos);

		DestroyTupleStore(node);
	}

	/*
	 * shut down the subplan
	 */
	ExecEndNode(outerPlanState(node));
	EndPlanStateGpmonPkt(&node->ss.ps);
}

/* ----------------------------------------------------------------
 *		ExecMaterialMarkPos
 *
 *		Calls tuplestore to save the current position in the stored file.
 * ----------------------------------------------------------------
 */
void
ExecMaterialMarkPos(MaterialState *node)
{
	Assert(node->eflags & EXEC_FLAG_MARK);

#ifdef DEBUG
	{
		/* share input should never call this */
		Material *ma = (Material *) node->ss.ps.plan;
		Assert(ma->share_type == SHARE_NOTSHARED);
	}
#endif

	/*
	 * if we haven't materialized yet, just return.
	 */
	if (NULL == node->ts_state->matstore)
	{
		return;
	}

	Assert(node->ts_pos);

	if(node->ts_markpos == NULL)
	{
		node->ts_markpos = palloc(sizeof(NTupleStorePos));
	}

	ntuplestore_acc_tell((NTupleStoreAccessor *) node->ts_pos, (NTupleStorePos *) node->ts_markpos);
}

/* ----------------------------------------------------------------
 *		ExecMaterialRestrPos
 *
 *		Calls tuplestore to restore the last saved file position.
 * ----------------------------------------------------------------
 */
void
ExecMaterialRestrPos(MaterialState *node)
{
	Assert(node->eflags & EXEC_FLAG_MARK);

#ifdef DEBUG
	{
		/* share input should never call this */
		Material *ma = (Material *) node->ss.ps.plan;
		Assert(ma->share_type == SHARE_NOTSHARED);
	}
#endif

	/*
	 * if we haven't materialized yet, just return.
	 */
	if (NULL == node->ts_state->matstore)
	{
		return;
	}

	Assert(node->ts_pos && node->ts_markpos);
	ntuplestore_acc_seek((NTupleStoreAccessor *) node->ts_pos, (NTupleStorePos *) node->ts_markpos);
}

/*
 * DestroyTupleStore
 * 		Helper function for destroying tuple store
 */
void
DestroyTupleStore(MaterialState *node)
{
	Assert(NULL != node);
	Assert(NULL != node->ts_state);
	Assert(NULL != node->ts_state->matstore);

	ntuplestore_destroy_accessor((NTupleStoreAccessor *) node->ts_pos);
	ntuplestore_destroy(node->ts_state->matstore);
	if(node->ts_markpos)
	{
		pfree(node->ts_markpos);
	}

	node->ts_state->matstore = NULL;
	node->ts_pos = NULL;
	node->ts_markpos = NULL;
	node->eof_underlying = false;
	node->ts_destroyed = true;
}

/*
 * ExecChildRescan
 *      Helper function for rescanning child of materialize node
 */
void
ExecChildRescan(MaterialState *node, ExprContext *exprCtxt)
{
	Assert(node);
	/*
	 * if parameters of subplan have changed, then subplan will be rescanned by
	 * first ExecProcNode. Otherwise, we need to rescan subplan here
	 */
	if (((PlanState *) node)->lefttree->chgParam == NULL)
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);

	node->eof_underlying = false;
}

/* ----------------------------------------------------------------
 *		ExecMaterialReScan
 *
 *		Rescans the materialized relation.
 * ----------------------------------------------------------------
 */
void
ExecMaterialReScan(MaterialState *node, ExprContext *exprCtxt)
{
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	if (node->eflags != 0)
	{
		/*
		 * If tuple store is empty, then either we have not materialized yet
		 * or tuple store was destroyed after a previous execution of materialize.
		 */
		if (NULL == node->ts_state->matstore)
		{
			/*
			 *  If tuple store was destroyed before, then materialize is part of subquery
			 *  execution, and we need to rescan child (MPP-15087).
			 */
			if (node->ts_destroyed)
			{
				ExecChildRescan(node, exprCtxt);
			}
			return;
		}

		/*
		 * If subnode is to be rescanned then we forget previous stored
		 * results; we have to re-read the subplan and re-store.  Also, if we
		 * told tuplestore it needn't support rescan, we lose and must
		 * re-read.  (This last should not happen in common cases; else our
		 * caller lied by not passing EXEC_FLAG_REWIND to us.)
		 *
		 * Otherwise we can just rewind and rescan the stored output. The
		 * state of the subnode does not change.
		 */
		if (((PlanState *) node)->lefttree->chgParam != NULL ||
			(node->eflags & EXEC_FLAG_REWIND) == 0)
		{
			DestroyTupleStore(node);
			if (((PlanState *) node)->lefttree->chgParam == NULL)
				ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
		}
		else
		{
			ntuplestore_acc_seek_bof((NTupleStoreAccessor *) node->ts_pos);
		}
	}
	else
	{
		/* In this case we are just passing on the subquery's output */
		ExecChildRescan(node, exprCtxt);
	}
}

void
ExecEagerFreeMaterial(MaterialState *node)
{
	Material   *ma = (Material *) node->ss.ps.plan;
	EState	   *estate = node->ss.ps.state;

	/*
	 * If we still have potential readers assocated with this node,
	 * we shouldn't free the tuplestore too early.  The eager-free message
	 * doesn't know about upper ShareInputScan nodes, but those nodes
	 * bumps up the reference count in their initializations and decrement
	 * it in either EagerFree or ExecEnd.
	 */
	if (ma->share_type == SHARE_MATERIAL)
	{
		ShareNodeEntry	   *snEntry;

		snEntry = ExecGetShareNodeEntry(estate, ma->share_id, false);

		if (snEntry->refcount > 0)
			return;
	}

	/*
	 * Release tuplestore resources
	 */
	if (NULL != node->ts_state->matstore)
	{
		if (ma->share_type == SHARE_MATERIAL_XSLICE && node->share_lk_ctxt)
		{
			/*
			 * MPP-22682: If this is a producer shared XSLICE, don't free up
			 * the tuple store here. For XSLICE producers, that will wait for
			 * consumers that haven't completed yet, which can cause deadlocks.
			 * Wait until ExecEndMaterial to free it, which is safer.
			 */
			return;
		}
		Assert(node->ts_pos);

		DestroyTupleStore(node);
	}
}
