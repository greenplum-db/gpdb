/*-------------------------------------------------------------------------
 *
 * cdbtargeteddispatch.c
 *
 * Portions Copyright (c) 2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbtargeteddispatch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/relation.h"
#include "cdb/cdbtargeteddispatch.h"
#include "optimizer/clauses.h"
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "nodes/makefuncs.h"	/* for makeVar() */
#include "utils/relcache.h"		/* RelationGetPartitioningKey() */
#include "optimizer/optimizer.h"
#include "optimizer/predtest_valueset.h"

#include "catalog/gp_distribution_policy.h"
#include "catalog/pg_type.h"

#include "catalog/pg_proc.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbutil.h"

#include "executor/executor.h"

#define PRINT_DISPATCH_DECISIONS_STRING ("print_dispatch_decisions")

static char *gp_test_options = "";

/* PRINT_DISPATCH_DECISIONS_STRING; */

/**
 * Used when building up all of the composite distribution keys.
 */
typedef struct PartitionKeyInfo
{
	Form_pg_attribute attr;
	Node	  **values;
	int			counter;
	int			numValues;
} PartitionKeyInfo;

/**
 * Initialize a DirectDispatchCalculationInfo.
 */
static void
InitDirectDispatchCalculationInfo(DirectDispatchInfo *data)
{
	data->isDirectDispatch = false;
	data->contentIds = NULL;
	data->haveProcessedAnyCalculations = false;
}

/**
 * Mark a DirectDispatchCalculationInfo as having targeted dispatch disabled
 */
static void
DisableTargetedDispatch(DirectDispatchInfo *data)
{
	data->isDirectDispatch = false;
	data->contentIds = NULL;
	data->haveProcessedAnyCalculations = true;
}

/**
 * helper function for AssignContentIdsFromUpdateDeleteQualification
 */
static DirectDispatchInfo
GetContentIdsFromPlanForSingleRelation(PlannerInfo *root, Plan *plan, int rangeTableIndex, List *qualification)
{
	GpPolicy   *policy = NULL;
	PartitionKeyInfo *parts = NULL;
	int			i;

	DirectDispatchInfo result;
	RangeTblEntry *rte;
	Relation	relation = NULL;

	InitDirectDispatchCalculationInfo(&result);

	if (nodeTag((Node *) plan) == T_BitmapHeapScan)
	{
		/*
		 * do not assert for bitmap heap scan --> it can have a child which is
		 * an index scan
		 */
		/*
		 * in fact, checking the quals for the bitmap heap scan are redundant
		 * with checking them on the child scan.  But it won't cause any harm
		 * since we will recurse to the child scan.
		 */
	}
	else
	{
		Assert(plan->lefttree == NULL);
	}
	Assert(plan->righttree == NULL);

	/* open and get relation info */
	rte = planner_rt_fetch(rangeTableIndex, root);
	if (rte->rtekind == RTE_RELATION)
	{
		/* Get a copy of the rel's GpPolicy from the relcache. */
		relation = relation_open(rte->relid, NoLock);
		policy = relation->rd_cdbpolicy;

		if (policy != NULL)
		{
			parts = (PartitionKeyInfo *) palloc(policy->nattrs * sizeof(PartitionKeyInfo));
			for (i = 0; i < policy->nattrs; i++)
			{
				parts[i].attr = TupleDescAttr(relation->rd_att, policy->attrs[i] - 1);
				parts[i].values = NULL;
				parts[i].numValues = 0;
				parts[i].counter = 0;
			}
		}
	}
	else
	{
		/* fall through, policy will be NULL so we won't direct dispatch */
	}

	if (rte->forceDistRandom ||	policy == NULL)
	{
		/*  we won't direct dispatch  */
		if (rte->rtekind == RTE_RELATION)
			relation_close(relation, NoLock);
		result.haveProcessedAnyCalculations = true;
		return result;
	}

	/*
	 * We first test if the predict contains a qual like
	 * gp_segment_id = some_const. This is very suitable
	 * for a direct dispatch. If this leads to direct dispatch
	 * then we just return because it does not need to
	 * eval distkey.
	 */
	if (GpPolicyIsPartitioned(policy))
	{
		Var                *seg_id_var;
		Oid                 vartypeid;
		int32               type_mod;
		Oid                 type_coll;
		PossibleValueSet    pvs_segids;
		Node              **seg_ids;
		int                 len;
		int                 i;
		List               *contentIds = NULL;

		get_atttypetypmodcoll(rte->relid, GpSegmentIdAttributeNumber,
							  &vartypeid, &type_mod, &type_coll);
		seg_id_var = makeVar(rangeTableIndex,
							 GpSegmentIdAttributeNumber,
							 vartypeid, type_mod, type_coll, 0);
		pvs_segids = DeterminePossibleValueSet((Node *) qualification,
											   (Node *) seg_id_var);
		if (!pvs_segids.isAnyValuePossible)
		{
			seg_ids = GetPossibleValuesAsArray(&pvs_segids, &len);
			if (len > 0 && len < policy->numsegments)
			{
				result.isDirectDispatch = true;
				for (i = 0; i < len; i++)
				{
					Node *val = seg_ids[i];
					if (IsA(val, Const))
					{
						int32 segid = DatumGetInt32(((Const *) val)->constvalue);
						contentIds = list_append_unique_int(contentIds, segid);
					}
					else
					{
						result.isDirectDispatch = false;
						break;
					}
				}
			}
		}

		DeletePossibleValueSetData(&pvs_segids);
		if (result.isDirectDispatch)
		{
			result.contentIds = contentIds;
			result.haveProcessedAnyCalculations = true;
			if (rte->rtekind == RTE_RELATION)
				relation_close(relation, NoLock);
			return result;
		}
	}

	if (GpPolicyIsHashPartitioned(policy))
	{
		long		totalCombinations = 1;

		Assert(parts != NULL);

		/* calculate possible value set for each partitioning attribute */
		for (i = 0; i < policy->nattrs; i++)
		{
			Var		   *var;
			PossibleValueSet pvs;

			var = makeVar(rangeTableIndex,
						  policy->attrs[i],
						  parts[i].attr->atttypid,
						  parts[i].attr->atttypmod,
						  parts[i].attr->attcollation,
						  0);

			/**
			 * Note that right now we only examine the given qual.  This is okay because if there are other
			 *   quals on the plan then those would be ANDed with the qual, which can only narrow our choice
			 *   of segment and not expand it.
			 */
			pvs = DeterminePossibleValueSet((Node *) qualification, (Node *) var);

			if (pvs.isAnyValuePossible)
			{
				/*
				 * can't isolate to single statement -- totalCombinations = -1
				 * will signal this
				 */
				DeletePossibleValueSetData(&pvs);
				totalCombinations = -1;
				break;
			}
			else
			{
				parts[i].values = GetPossibleValuesAsArray(&pvs, &parts[i].numValues);
				totalCombinations *= parts[i].numValues;
				DeletePossibleValueSetData(&pvs);
			}
		}

		/*
		 * calculate possible target content ids from the combinations of
		 * partitioning attributes
		 */
		if (totalCombinations == 0)
		{
			/*
			 * one of the possible sets was empty and so we don't care where
			 * we run this
			 */
			result.isDirectDispatch = true;
			Assert(result.contentIds == NULL);		/* direct dispatch but no
													 * specific content at
													 * all! */
		}
		else if (totalCombinations > 0 &&
			/* don't bother for ones which will likely hash to many segments */
				 totalCombinations < policy->numsegments * 3)
		{
			CdbHash    *h;
			long		index;

			h = makeCdbHashForRelation(relation);

			result.isDirectDispatch = true;
			result.contentIds = NULL;

			/* for each combination of keys calculate target segment */
			for (index = 0; index < totalCombinations; index++)
			{
				long		curIndex = index;
				int			hashCode;

				/* hash the attribute values */
				cdbhashinit(h);

				for (i = 0; i < policy->nattrs; i++)
				{
					int			numValues = parts[i].numValues;
					int			which = curIndex % numValues;
					Node	   *val = parts[i].values[which];

					if (IsA(val, Const))
					{
						Const		*c = (Const *) val;

						cdbhash(h, i + 1, c->constvalue, c->constisnull);
					}
					else
					{
						result.isDirectDispatch = false;
						break;
					}
					curIndex /= numValues;
				}

				if (!result.isDirectDispatch)
					break;

				hashCode = cdbhashreduce(h);

				result.contentIds = list_append_unique_int(result.contentIds, hashCode);
			}
		}
		else
		{
			/* know nothing, can't do directed dispatch */
			result.isDirectDispatch = false;
		}
	}

	if (rte->rtekind == RTE_RELATION)
	{
		relation_close(relation, NoLock);
	}

	result.haveProcessedAnyCalculations = true;
	return result;
}

void
MergeDirectDispatchCalculationInfo(DirectDispatchInfo *to, DirectDispatchInfo *from)
{
	if (!from->isDirectDispatch)
	{
		/* from eliminates all options so take it */
		to->isDirectDispatch = false;
	}
	else if (!to->haveProcessedAnyCalculations)
	{
		/* to has no data, so just take from */
		*to = *from;
	}
	else if (!to->isDirectDispatch)
	{
		/* to cannot get better -- leave it alone */
	}
	else if (from->contentIds == NULL)
	{
		/* from says that it doesn't need to run anywhere -- so we accept to */
	}
	else if (to->contentIds == NULL)
	{
		/* to didn't even think it needed to run so accept from */
		to->contentIds = from->contentIds;
	}
	else
	{
		/* union to with from */
		to->contentIds = list_union_int(to->contentIds, from->contentIds);
	}

	to->haveProcessedAnyCalculations = true;
}

/**
 * returns true if we should print test messages.  Note for clients: for multi-slice queries then messages will print in
 *   the order of processing which may not always be deterministic (single joins can be rearranged by the planner,
 *   for example).
 */
static bool
ShouldPrintTestMessages()
{
	return gp_test_options && strstr(gp_test_options, PRINT_DISPATCH_DECISIONS_STRING) != NULL;
}

void
FinalizeDirectDispatchDataForSlice(PlanSlice *slice)
{
	DirectDispatchInfo *dd = &slice->directDispatch;

	if (dd->haveProcessedAnyCalculations)
	{
		if (dd->isDirectDispatch)
		{
			if (dd->contentIds == NULL)
			{
				int			random_segno;

				random_segno = cdbhashrandomseg(getgpsegmentCount());
				dd->contentIds = list_make1_int(random_segno);
				if (ShouldPrintTestMessages())
					elog(INFO, "DDCR learned no content dispatch is required");
			}
			else
			{
				if (ShouldPrintTestMessages())
					elog(INFO, "DDCR learned dispatch to content %d", linitial_int(dd->contentIds));
			}
		}
		else
		{
			if (ShouldPrintTestMessages())
				elog(INFO, "DDCR learned full dispatch is required");
		}
	}
	else
	{
		if (ShouldPrintTestMessages())
			elog(INFO, "DDCR learned no information: default to full dispatch");
		dd->isDirectDispatch = false;
	}
}

void
DirectDispatchUpdateContentIdsFromPlan(PlannerInfo *root, Plan *plan)
{
	DirectDispatchInfo dispatchInfo;

	InitDirectDispatchCalculationInfo(&dispatchInfo);

	switch (nodeTag(plan))
	{
		case T_Result:
		case T_Append:
		case T_MergeAppend:
		case T_LockRows:
		case T_ModifyTable:
		case T_BitmapAnd:
		case T_BitmapOr:
		case T_BitmapHeapScan:
			/* no change to dispatchInfo */
			break;
		case T_SampleScan:
			DisableTargetedDispatch(&dispatchInfo);
			break;
		case T_SeqScan:
			/*
			 * we can determine the dispatch data to merge by looking at
			 * the relation begin scanned
			 */
			dispatchInfo = GetContentIdsFromPlanForSingleRelation(root,
																  plan,
																  ((Scan *) plan)->scanrelid,
																  plan->qual);
			break;

		case T_IndexScan:
			{
				IndexScan  *indexScan = (IndexScan *) plan;

				/*
				 * we can determine the dispatch data to merge by looking
				 * at the relation begin scanned
				 */
				dispatchInfo = GetContentIdsFromPlanForSingleRelation(root,
																	  plan,
																	  ((Scan *) plan)->scanrelid,
																	  indexScan->indexqualorig);
				/* must use _orig_ qual ! */
			}
			break;

		case T_IndexOnlyScan:
			{
				IndexOnlyScan  *indexOnlyScan = (IndexOnlyScan *) plan;

				/*
				 * we can determine the dispatch data to merge by looking
				 * at the relation begin scanned
				 */
				dispatchInfo = GetContentIdsFromPlanForSingleRelation(root,
																	  plan,
																	  ((Scan *) plan)->scanrelid,
																	  indexOnlyScan->indexqualorig);
				/* must use _orig_ qual ! */
			}
			break;

		case T_BitmapIndexScan:
			{
				BitmapIndexScan *bitmapScan = (BitmapIndexScan *) plan;

				/*
				 * we can determine the dispatch data to merge by looking
				 * at the relation begin scanned
				 */
				dispatchInfo = GetContentIdsFromPlanForSingleRelation(root,
																	  plan,
																	  ((Scan *) plan)->scanrelid,
																	  bitmapScan->indexqualorig);
				/* must use original qual ! */
			}
			break;
		case T_SubqueryScan:
		case T_NamedTuplestoreScan:
			/* no change to dispatchInfo */
			break;
		case T_TidScan:
		case T_FunctionScan:
		case T_TableFuncScan:
		case T_WorkTableScan:
			DisableTargetedDispatch(&dispatchInfo);
			break;
		case T_ValuesScan:
			/* no change to dispatchInfo */
			break;
		case T_NestLoop:
		case T_MergeJoin:
		case T_HashJoin:
			/* join: no change to dispatchInfo */

			/*
			 * note that we could want to look at the join qual but
			 * constant checks should have been pushed down to the
			 * underlying scans so we shouldn't learn anything
			 */
			break;
		case T_Material:
		case T_Sort:
		case T_Agg:
		case T_TupleSplit:
		case T_Unique:
		case T_Gather:
		case T_Hash:
		case T_SetOp:
		case T_Limit:
		case T_PartitionSelector:
			break;
		case T_Motion:
			/*
			 * It's currently not allowed to direct-dispatch a slice that
			 * has a Motion that sends tuples to it. It would be possible
			 * in principle, but the interconnect initialization code gets
			 * confused.
			 */
			DisableTargetedDispatch(&dispatchInfo);
			break;

		case T_ShareInputScan:

			/*
			 * note: could try to peek into the building slice to get its
			 * direct dispatch values but we don't
			 */
			DisableTargetedDispatch(&dispatchInfo);
			break;
		case T_WindowAgg:
		case T_TableFunctionScan:
		case T_RecursiveUnion:
			/* no change to dispatchInfo */
			break;
		case T_ForeignScan:
			DisableTargetedDispatch(&dispatchInfo); /* not sure about
													 * foreign tables ...
													 * so disable */
			break;
		case T_SplitUpdate:
			break;
		default:
			elog(ERROR, "unknown plan node %d", nodeTag(plan));
			break;
	}

	/*
	 * analyzed node type, now do the work (for all except subquery scan,
	 * which do work in the switch above and return
	 */
	if (dispatchInfo.haveProcessedAnyCalculations)
	{
		/* learned new info: merge it in */
		MergeDirectDispatchCalculationInfo(&root->curSlice->directDispatch, &dispatchInfo);
	}
}

void
DirectDispatchUpdateContentIdsForInsert(PlannerInfo *root, Plan *plan,
										GpPolicy *targetPolicy, Oid *hashfuncs)
{
	DirectDispatchInfo dispatchInfo;
	int			i;
	ListCell   *cell = NULL;
	bool		isDirectDispatch;
	Datum	   *values;
	bool	   *nulls;

	InitDirectDispatchCalculationInfo(&dispatchInfo);

	values = (Datum *) palloc(targetPolicy->nattrs * sizeof(Datum));
	nulls = (bool *) palloc(targetPolicy->nattrs * sizeof(bool));

	/*
	 * the nested loops here seem scary -- especially since we've already
	 * walked them before -- but I think this is still required since they may
	 * not be in the same order. (also typically we don't distribute by more
	 * than a handful of attributes).
	 */
	isDirectDispatch = true;
	for (i = 0; i < targetPolicy->nattrs; i++)
	{
		foreach(cell, plan->targetlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(cell);
			Const	   *c;

			Assert(tle->expr);

			if (tle->resno != targetPolicy->attrs[i])
				continue;

			if (!IsA(tle->expr, Const))
			{
				/* the planner could not simplify this */
				isDirectDispatch = false;
				break;
			}

			c = (Const *) tle->expr;

			values[i] = c->constvalue;
			nulls[i] = c->constisnull;
			break;
		}

		if (!isDirectDispatch)
			break;
	}

	if (isDirectDispatch)
	{
		uint32		hashcode;
		CdbHash    *h;

		h = makeCdbHash(targetPolicy->numsegments, targetPolicy->nattrs, hashfuncs);

		cdbhashinit(h);
		for (i = 0; i < targetPolicy->nattrs; i++)
		{
			cdbhash(h, i + 1, values[i], nulls[i]);
		}

		/* We now have the hash-partition that this row belong to */
		hashcode = cdbhashreduce(h);
		dispatchInfo.isDirectDispatch = true;
		dispatchInfo.contentIds = list_make1_int(hashcode);
		dispatchInfo.haveProcessedAnyCalculations = true;

		/* learned new info: merge it in */
		MergeDirectDispatchCalculationInfo(&root->curSlice->directDispatch, &dispatchInfo);

		elog(DEBUG1, "sending single row constant insert to content %d", hashcode);
	}
	pfree(values);
	pfree(nulls);
}
