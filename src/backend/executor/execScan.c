/*-------------------------------------------------------------------------
 *
 * execScan.c
 *    Support routines for scans on various table type.
 *
 * Portions Copyright (c) 2006 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/execScan.c,v 1.44 2009/01/01 17:23:41 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "codegen/codegen_wrapper.h"

#include "executor/executor.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/debugbreak.h"

/*
 * getScanMethod
 *   Return ScanMethod for a given table type.
 */
static const ScanMethod *
getScanMethod(int tableType)
{
	Assert(tableType >= TableTypeHeap && tableType < TableTypeInvalid);

	/*
	 * scanMethods
	 *    Array that specifies different scan methods for various table types.
	 *
	 * The index in this array for a specific table type should match the enum value
	 * defined in TableType.
	 */
	static const ScanMethod scanMethods[] =
	{
		{
			&HeapScanNext, &BeginScanHeapRelation, &EndScanHeapRelation,
			&ReScanHeapRelation, &MarkPosHeapRelation, &RestrPosHeapRelation
		},
		{
			&AppendOnlyScanNext, &BeginScanAppendOnlyRelation, &EndScanAppendOnlyRelation,
			&ReScanAppendOnlyRelation, &MarkRestrNotAllowed, &MarkRestrNotAllowed
		},
		{
			&AOCSScanNext, &BeginScanAOCSRelation, &EndScanAOCSRelation,
			&ReScanAOCSRelation, &MarkRestrNotAllowed, &MarkRestrNotAllowed
		}
	};
	
	COMPILE_ASSERT(ARRAY_SIZE(scanMethods) == TableTypeInvalid);

	return &scanMethods[tableType];
}


static bool tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc);


/* ----------------------------------------------------------------
 *		ExecScan
 *
 *		Scans the relation using the 'access method' indicated and
 *		returns the next qualifying tuple in the direction specified
 *		in the global variable ExecDirection.
 *		The access method returns the next tuple and execScan() is
 *		responsible for checking the tuple returned against the qual-clause.
 *
 *		Conditions:
 *		  -- the "cursor" maintained by the AMI is positioned at the tuple
 *			 returned previously.
 *
 *		Initial States:
 *		  -- the relation indicated is opened for scanning so that the
 *			 "cursor" is positioned before the first qualifying tuple.
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecScan(ScanState *node,
		 ExecScanAccessMtd accessMtd)	/* function returning a tuple */
{
	ExprContext *econtext;
	List	   *qual;
	ProjectionInfo *projInfo;

	/*
	 * Fetch data from node
	 */
	qual = node->ps.qual;
	projInfo = node->ps.ps_ProjInfo;

	/*
	 * If we have neither a qual to check nor a projection to do, just skip
	 * all the overhead and return the raw scan tuple.
	 */
	if (!qual && !projInfo)
		return (*accessMtd) (node);

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.  
	 */
	econtext = node->ps.ps_ExprContext;
	ResetExprContext(econtext);

	/*
	 * get a tuple from the access method loop until we obtain a tuple which
	 * passes the qualification.
	 */
	for (;;)
	{
		TupleTableSlot *slot;

		CHECK_FOR_INTERRUPTS();

		if (QueryFinishPending)
			return NULL;

		slot = (*accessMtd) (node);

		/*
		 * if the slot returned by the accessMtd contains NULL, then it means
		 * there is nothing more to scan so we just return an empty slot,
		 * being careful to use the projection result slot so it has correct
		 * tupleDesc.
		 */
		if (TupIsNull(slot))
		{
			if (projInfo)
				return ExecClearTuple(projInfo->pi_slot);
			else
				return slot;
		}

		/*
		 * place the current tuple into the expr context
		 */
		econtext->ecxt_scantuple = slot;

		/*
		 * check that the current tuple satisfies the qual-clause
		 *
		 * check for non-nil qual here to avoid a function call to ExecQual()
		 * when the qual is nil ... saves only a few cycles, but they add up
		 * ...
		 */
		if (!qual || ExecQual(qual, econtext, false))
		{
			/*
			 * Found a satisfactory scan tuple.
			 */
			if (projInfo)
			{
				/*
				 * Form a projection tuple, store it in the result tuple slot
				 * and return it.
				 */
				return ExecProject(projInfo, NULL);
			}
			else
			{
				/*
				 * Here, we aren't projecting, so just return scan tuple.
				 */
				return slot;
			}
		}

		/*
		 * Tuple fails qual, so free per-tuple memory and try again.
		 */
		ResetExprContext(econtext);
	}
}

/*
 * ExecAssignScanProjectionInfo
 *		Set up projection info for a scan node, if necessary.
 *
 * We can avoid a projection step if the requested tlist exactly matches
 * the underlying tuple type.  If so, we just set ps_ProjInfo to NULL.
 * Note that this case occurs not only for simple "SELECT * FROM ...", but
 * also in most cases where there are joins or other processing nodes above
 * the scan node, because the planner will preferentially generate a matching
 * tlist.
 *
 * ExecAssignScanType must have been called already.
 */
void
ExecAssignScanProjectionInfo(ScanState *node)
{
	Scan	   *scan = (Scan *) node->ps.plan;

	if (tlist_matches_tupdesc(&node->ps,
							  scan->plan.targetlist,
							  scan->scanrelid,
							  node->ss_ScanTupleSlot->tts_tupleDescriptor))
		node->ps.ps_ProjInfo = NULL;
	else
		ExecAssignProjectionInfo(&node->ps,
								 node->ss_ScanTupleSlot->tts_tupleDescriptor);
}

static bool
tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc)
{
	int			numattrs = tupdesc->natts;
	int			attrno;
	bool		hasoid;
	ListCell   *tlist_item = list_head(tlist);

	/* Check the tlist attributes */
	for (attrno = 1; attrno <= numattrs; attrno++)
	{
		Form_pg_attribute att_tup = tupdesc->attrs[attrno - 1];
		Var		   *var;

		if (tlist_item == NULL)
			return false;		/* tlist too short */
		var = (Var *) ((TargetEntry *) lfirst(tlist_item))->expr;
		if (!var || !IsA(var, Var))
			return false;		/* tlist item not a Var */

		/* if these Asserts fail, planner messed up */
		Assert(var->varlevelsup == 0);
		if (var->varattno != attrno)
			return false;		/* out of order */
		if (att_tup->attisdropped)
			return false;		/* table contains dropped columns */

		/*
		 * Note: usually the Var's type should match the tupdesc exactly, but
		 * in situations involving unions of columns that have different
		 * typmods, the Var may have come from above the union and hence have
		 * typmod -1.  This is a legitimate situation since the Var still
		 * describes the column, just not as exactly as the tupdesc does. We
		 * could change the planner to prevent it, but it'd then insert
		 * projection steps just to convert from specific typmod to typmod -1,
		 * which is pretty silly.
		 */
		if (var->vartype != att_tup->atttypid ||
			(var->vartypmod != att_tup->atttypmod &&
			 var->vartypmod != -1))
			return false;		/* type mismatch */

		tlist_item = lnext(tlist_item);
	}

	if (tlist_item)
		return false;			/* tlist too long */

	/*
	 * If the plan context requires a particular hasoid setting, then that has
	 * to match, too.
	 */
 	{
		bool forceOids = ExecContextForcesOids(ps, &hasoid);

		/* If Oid matters, and there are different requirement, then does not match */
		if(forceOids && hasoid != tupdesc->tdhasoid)
			return false;

		/* If Oid does not matter, but old tupdesc has oids, does not match either. 
		 * XXX: Memtuple: tupleformat is different depends on if has oid, so we cannot
		 * mismatch.
		 */
		if(!forceOids && tupdesc->tdhasoid)
			return false;
	}

	return true;
}

/*
 * InitScanStateRelationDetails
 *   Opens a relation and sets various relation specific ScanState fields.
 */
void
InitScanStateRelationDetails(ScanState *scanState, Plan *plan, EState *estate)
{
	Assert(NULL != scanState);
	PlanState *planState = &scanState->ps;

	/* Initialize child expressions */
	planState->targetlist = (List *)ExecInitExpr((Expr *)plan->targetlist, planState);
	planState->qual = (List *)ExecInitExpr((Expr *)plan->qual, planState);

	Relation currentRelation = ExecOpenScanRelation(estate, ((Scan *)plan)->scanrelid);
	scanState->ss_currentRelation = currentRelation;
	ExecAssignScanType(scanState, RelationGetDescr(currentRelation));
	ExecAssignScanProjectionInfo(scanState);

	scanState->tableType = getTableType(scanState->ss_currentRelation);
}

/*
 * InitScanStateInternal
 *   Initialize ScanState common variables for various Scan node.
 */
void
InitScanStateInternal(ScanState *scanState, Plan *plan, EState *estate,
		int eflags, bool initCurrentRelation)
{
	Assert(IsA(plan, SeqScan) ||
		   IsA(plan, AppendOnlyScan) ||
		   IsA(plan, AOCSScan) ||
		   IsA(plan, TableScan) ||
		   IsA(plan, DynamicTableScan) ||
		   IsA(plan, BitmapTableScan));

	PlanState *planState = &scanState->ps;

	planState->plan = plan;
	planState->state = estate;

	/* Create expression evaluation context */
	ExecAssignExprContext(estate, planState);
	
	/* Initialize tuple table slot */
	ExecInitResultTupleSlot(estate, planState);
	ExecInitScanTupleSlot(estate, scanState);
	
	/*
	 * For dynamic table scan, We do not initialize expression states; instead
	 * we wait until the first partition, and initialize the expression state
	 * at that time. Also, for dynamic table scan, we do not need to open the
	 * parent partition relation.
	 */
	if (initCurrentRelation)
	{
		InitScanStateRelationDetails(scanState, plan, estate);
	}

	/* Initialize result tuple type. */
	ExecAssignResultTypeFromTL(planState);

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	scanState->ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);

	/* Currently, only SeqScan supports Mark/Restore. */
	AssertImply((eflags & EXEC_FLAG_MARK) != 0, IsA(plan, SeqScan));
}

/*
 * FreeScanRelationInternal
 *   Free ScanState common variables initialized in InitScanStateInternal.
 */
void
FreeScanRelationInternal(ScanState *scanState, bool closeCurrentRelation)
{
	ExecFreeExprContext(&scanState->ps);
	ExecClearTuple(scanState->ps.ps_ResultTupleSlot);
	ExecClearTuple(scanState->ss_ScanTupleSlot);

	if (closeCurrentRelation)
	{
		ExecCloseScanRelation(scanState->ss_currentRelation);
	}
}

/*
 * OpenScanRelationByOid
 *   Open the relation by the given Oid with AccessShareLock.
 */
Relation
OpenScanRelationByOid(Oid relid)
{
	return heap_open(relid, AccessShareLock);
}

/*
 * CloseScanRelation
 *  Close the relation that is opened through OpenScanRelationByOid.
 */
void
CloseScanRelation(Relation rel)
{
	heap_close(rel, AccessShareLock);
}

/*
 * getTableType
 *   Return the table type for a given relation.
 */
int
getTableType(Relation rel)
{
	Assert(rel != NULL && rel->rd_rel != NULL);
	
	if (RelationIsHeap(rel))
	{
		return TableTypeHeap;
	}

	if (RelationIsAoRows(rel))
	{
		return TableTypeAppendOnly;
	}
	
	if (RelationIsAoCols(rel))
	{
		return TableTypeAOCS;
	}
	
	elog(ERROR, "undefined table type for storage format: %c", rel->rd_rel->relstorage);
	return TableTypeInvalid;
}

/*
 * ExecTableScanRelation
 *    Scan the relation and return the next qualifying tuple.
 *
 * This is a wrapper function for ExecScan. The access method is determined
 * based on the type of the table being scanned.
 */
TupleTableSlot *
ExecTableScanRelation(ScanState *scanState)
{
	return ExecScan(scanState, getScanMethod(scanState->tableType)->accessMethod);
}

/*
 * BeginScanRelation
 *   Begin the relation scan.
 */
void
BeginTableScanRelation(ScanState *scanState)
{
	getScanMethod(scanState->tableType)->beginScanMethod(scanState);
}

/*
 * EndTableScanRelation
 *   Terminate the relation scan.
 */
void
EndTableScanRelation(ScanState *scanState)
{
	getScanMethod(scanState->tableType)->endScanMethod(scanState);
}

/*
 * ReScanRelation
 *   Rescan the relation.
 */
void
ReScanRelation(ScanState *scanState)
{
	const ScanMethod *scanMethod;
	EState *estate = scanState->ps.state;
	Index scanrelid = ((Scan *)scanState->ps.plan)->scanrelid;
	
	/* If this is re-scanning of PlanQual ... */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		estate->es_evTupleNull[scanrelid - 1] = false;
		return;
	}

	scanMethod = getScanMethod(scanState->tableType);
	if ((scanState->scan_state & SCAN_SCAN) == 0)
	{
		scanMethod->beginScanMethod(scanState);
	}
	
	scanMethod->reScanMethod(scanState);
}

/*
 * MarkPosScanRelation
 *   Set a Mark position in the scan.
 */
void
MarkPosScanRelation(ScanState *scanState)
{
	getScanMethod(scanState->tableType)->markPosMethod(scanState);	
}

/*
 * RestrPosScanRelation
 *   Restore a marked position in the scan.
 */
void
RestrPosScanRelation(ScanState *scanState)
{
	getScanMethod(scanState->tableType)->restrPosMethod(scanState);	
}

/*
 * MarkRestrNotAllowed
 *   Errors when Mark/Restr is not implemented in the given scan.
 *
 * This function only supports AppendOnlyScan, AOCSScan, DynamicTableScan.
 */
void
MarkRestrNotAllowed(ScanState *scanState)
{
	Assert(scanState->tableType == TableTypeAppendOnly ||
		   scanState->tableType == TableTypeAOCS ||
		   IsA(scanState, DynamicTableScanState));
	
	const char *scan = NULL;
	if (scanState->tableType == TableTypeAppendOnly)
	{
		scan = "AppendOnlyRowScan";
	}
	
	else if (scanState->tableType == TableTypeAOCS)
	{
		scan = "AppendOnlyColumnarScan";
	}
	
	else
	{
		Assert(IsA(scanState, DynamicTableScanState));
		scan = "DynamicTableScan";
	}

	ereport(ERROR,
			(errcode(ERRCODE_GP_INTERNAL_ERROR),
			 errmsg("Mark/Restore is not allowed in %s", scan)));
	
}
