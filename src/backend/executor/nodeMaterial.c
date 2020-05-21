/*-------------------------------------------------------------------------
 *
 * nodeMaterial.c
 *	  Routines to handle materialization nodes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeMaterial.c
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
#include "miscadmin.h"

#include "cdb/cdbvars.h"
#include "executor/instrument.h"        /* Instrumentation */

static void ExecMaterialExplainEnd(PlanState *planstate, struct StringInfoData *buf);
static void ExecChildRescan(MaterialState *node);

static void ExecEagerFreeMaterial(MaterialState *node);

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
	Tuplestorestate *tuplestorestate;
	bool		eof_tuplestore;
	TupleTableSlot *slot;

	/*
	 * get state info from node
	 */
	estate = node->ss.ps.state;
	dir = estate->es_direction;
	forward = ScanDirectionIsForward(dir);
	tuplestorestate = node->tuplestorestate;

	/*
	 * If first time through, and we need a tuplestore, initialize it.
	 */
	if (tuplestorestate == NULL && node->eflags != 0)
	{
		tuplestorestate = tuplestore_begin_heap(true, false, PlanStateOperatorMemKB(&node->ss.ps));
		tuplestore_set_eflags(tuplestorestate, node->eflags);
		if (node->eflags & EXEC_FLAG_MARK)
		{
			/*
			 * Allocate a second read pointer to serve as the mark. We know it
			 * must have index 1, so needn't store that.
			 */
			int ptrno	PG_USED_FOR_ASSERTS_ONLY;

			ptrno = tuplestore_alloc_read_pointer(tuplestorestate,
												  node->eflags);
			Assert(ptrno == 1);
		}
		node->tuplestorestate = tuplestorestate;

        /* CDB: Offer extra info for EXPLAIN ANALYZE. */
        if (node->ss.ps.instrument && node->ss.ps.instrument->need_cdb)
        {
            /* Let the tuplestore share our Instrumentation object. */
			tuplestore_set_instrument(tuplestorestate, node->ss.ps.instrument);

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
		 */
		if (((Material *) node->ss.ps.plan)->cdb_strict)
		{
			for (;;)
			{
				TupleTableSlot *outerslot = ExecProcNode(outerPlanState(node));

				if (TupIsNull(outerslot))
					break;

				tuplestore_puttupleslot(tuplestorestate, outerslot);
			}
			node->eof_underlying = true;
			tuplestore_rescan(tuplestorestate);
		}
	}

	/*
	 * If we are not at the end of the tuplestore, or are going backwards, try
	 * to fetch a tuple from tuplestore.
	 */
	eof_tuplestore = (tuplestorestate == NULL) ||
		tuplestore_ateof(tuplestorestate);

	if (!forward && eof_tuplestore)
	{
		if (!node->eof_underlying)
		{
			/*
			 * When reversing direction at tuplestore EOF, the first
			 * gettupleslot call will fetch the last-added tuple; but we want
			 * to return the one before that, if possible. So do an extra
			 * fetch.
			 */
			if (!tuplestore_advance(tuplestorestate, forward))
				return NULL;	/* the tuplestore must be empty */
		}
		eof_tuplestore = false;
	}

	/*
	 * If we can fetch another tuple from the tuplestore, return it.
	 */
	slot = node->ss.ps.ps_ResultTupleSlot;
	if (!eof_tuplestore)
	{
		if (tuplestore_gettupleslot(tuplestorestate, forward, false, slot))
			return slot;
		if (forward)
			eof_tuplestore = true;
	}

	/*
	 * If necessary, try to fetch another row from the subplan.
	 *
	 * Note: the eof_underlying state variable exists to short-circuit further
	 * subplan calls.  It's not optional, unfortunately, because some plan
	 * node types are not robust about being called again when they've already
	 * returned NULL.
	 *
	 * GPDB: If reusing cached workfiles, there is no need to execute subplan
	 * at all.
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
			if (!node->delayEagerFree)
			{
				ExecEagerFreeMaterial(node);
			}

			return NULL;
		}

		/*
		 * Append a copy of the returned tuple to tuplestore.  NOTE: because
		 * the tuplestore is certainly in EOF state, its read position will
		 * move forward over the added tuple.  This is what we want.
		 */
		if (tuplestorestate)
			tuplestore_puttupleslot(tuplestorestate, outerslot);

		/*
		 * We can just return the subplan's returned tuple, without copying.
		 */
		return outerslot;
	}


	if (!node->delayEagerFree)
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
	 * If the Material node was inserted to protect the child node from rescanning, don't
	 * eager free.
	 *
	 * XXX: The planner doesn't always set the flag for Material nodes that are put
	 * directly on top of Motion nodes, so check for that, too. (Or is this for ORCA?)
	 */
	if (node->cdb_shield_child_from_rescans ||
		IsA(outerPlan((Plan *) node), Motion))
	{
		eflags |= EXEC_FLAG_REWIND;
	}

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
	 * means "backwards all the way to start".  If told to support BACKWARD we
	 * must include REWIND in the tuplestore eflags, else tuplestore_trim
	 * might throw away too much.
	 */
	if (eflags & EXEC_FLAG_BACKWARD)
		matstate->eflags |= EXEC_FLAG_REWIND;

	matstate->eof_underlying = false;
	matstate->tuplestorestate = NULL;
	matstate->ts_destroyed = false;

	/*
	 * Miscellaneous initialization
	 *
	 * Materialization nodes don't need ExprContexts because they never call
	 * ExecQual or ExecProject.
	 */

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
	matstate->delayEagerFree =
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
	if (list_length(node->plan.targetlist) != list_length(outerPlan->targetlist))
		elog(ERROR, "Material operator does not support projection");
	outerPlanState(matstate) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * initialize tuple type.  no need to initialize projection info because
	 * this node doesn't do projections.
	 */
	ExecAssignResultTypeFromTL(&matstate->ss.ps);
	ExecAssignScanTypeFromOuterPlan(&matstate->ss);
	matstate->ss.ps.ps_ProjInfo = NULL;

	return matstate;
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
	if (node->tuplestorestate != NULL)
	{
		tuplestore_end(node->tuplestorestate);
		node->ts_destroyed = true;
	}
	node->tuplestorestate = NULL;

	/*
	 * shut down the subplan
	 */
	ExecEndNode(outerPlanState(node));
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

	/*
	 * if we haven't materialized yet, just return.
	 */
	if (!node->tuplestorestate)
		return;

	/*
	 * copy the active read pointer to the mark.
	 */
	tuplestore_copy_read_pointer(node->tuplestorestate, 0, 1);

	/*
	 * since we may have advanced the mark, try to truncate the tuplestore.
	 */
	tuplestore_trim(node->tuplestorestate);
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

	/*
	 * if we haven't materialized yet, just return.
	 */
	if (!node->tuplestorestate)
		return;

	/*
	 * copy the mark to the active read pointer.
	 */
	tuplestore_copy_read_pointer(node->tuplestorestate, 1, 0);
}

/*
 * ExecChildRescan
 *      Helper function for rescanning child of materialize node
 */
static void
ExecChildRescan(MaterialState *node)
{
	Assert(node);
	/*
	 * if parameters of subplan have changed, then subplan will be rescanned by
	 * first ExecProcNode. Otherwise, we need to rescan subplan here
	 */
	if (node->ss.ps.lefttree->chgParam == NULL)
		ExecReScan(node->ss.ps.lefttree);

	node->eof_underlying = false;
}

/* ----------------------------------------------------------------
 *		ExecReScanMaterial
 *
 *		Rescans the materialized relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanMaterial(MaterialState *node)
{
	PlanState  *outerPlan = outerPlanState(node);

	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	if (node->eflags != 0)
	{
		/*
		 * If tuple store is empty, then either we have not materialized yet
		 * or tuple store was destroyed after a previous execution of materialize.
		 */
		if (!node->tuplestorestate)
		{
			/*
			 *  If tuple store was destroyed before, then materialize is part of subquery
			 *  execution, and we need to rescan child (MPP-15087).
			 */
			if (node->ts_destroyed)
			{
				ExecChildRescan(node);
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
		if (outerPlan->chgParam != NULL ||
			(node->eflags & EXEC_FLAG_REWIND) == 0)
		{
			tuplestore_end(node->tuplestorestate);
			node->tuplestorestate = NULL;
			node->ts_destroyed = true;
			if (outerPlan->chgParam == NULL)
				ExecReScan(outerPlan);
			node->eof_underlying = false;
		}
		else
			tuplestore_rescan(node->tuplestorestate);
	}
	else
	{
		/* In this case we are just passing on the subquery's output */
		ExecChildRescan(node);
	}
}

static void
ExecEagerFreeMaterial(MaterialState *node)
{
	/*
	 * Release tuplestore resources
	 */
	if (node->tuplestorestate)
	{
		tuplestore_end(node->tuplestorestate);
		node->tuplestorestate = NULL;
		node->ts_destroyed = true;
	}
}

void
ExecSquelchMaterial(MaterialState *node)
{
	/*
	 * If this Material is shielding the underlying nodes from rescanning (for
	 * example, if there is a Motion node below), then keep the tuplestore.
	 * Also, don't recurse to the subtree in that case, because we might need
	 * to read more tuples from it after a ReScan. Most likely we have already
	 * read all the tuples from the underlying node in that case, but it's
	 * possible that ExecMaterial hasn't been called even once yet, and we
	 * haven't created the tuplestore yet.
	 */
	if (!node->delayEagerFree)
	{
		ExecEagerFreeMaterial(node);
		ExecSquelchNode(outerPlanState(node));
	}
}
