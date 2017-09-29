/*-------------------------------------------------------------------------
 *
 * cdbtargeteddispatch.c
 *
 * Portions Copyright (c) 2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbtargeteddispatch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbtargeteddispatch.h"
#include "optimizer/clauses.h"
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "nodes/makefuncs.h"	/* for makeVar() */
#include "utils/relcache.h"		/* RelationGetPartitioningKey() */
#include "optimizer/predtest.h"

#include "catalog/gp_policy.h"
#include "catalog/pg_type.h"

#include "catalog/pg_proc.h"
#include "utils/syscache.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbhash.h"		/* isGreenplumDbHashable() */
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
 * Targeted dispatch info: used when building the dispatch information for a given qual or slice
 */
typedef struct DirectDispatchCalculationInfo
{
	/*
	 * In this use of the  DirectDispatchInfo structure, a NULL contentIds
	 * means that ANY contentID is valid (when dd->isDirectDispatch is true)
	 *
	 * Note also that we don't free dd->contentIds.  We let memory context
	 * cleanup handle that.
	 */
	DirectDispatchInfo dd;

	/**
	 * Have any calculations been processed and used to update dd ?  If false then
	 *    no, and dd should be considered uninitialized.
	 */
	bool		haveProcessedAnyCalculations;
}
DirectDispatchCalculationInfo;

/**
 * ContentIdAssignmentData: used as the context parameter to the plan walking function that assigns
 *   targeted dispatch values to slices
 */
typedef struct ContentIdAssignmentData
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */

	/**
	 * the range table!
	 */
	List	   *rtable;

	/**
	 * of DirectDispatchCalculationInfo*
	 */
	List	   *sliceStack;

	/*
	 * For use when constructing the output data structure -- objects in the
	 * output structure will be allocated in this memory context
	 */
	MemoryContext memoryContextForOutput;

	/**
	 * of Plan*
	 */
	List	   *allSlices;
} ContentIdAssignmentData;

static bool AssignContentIdsToPlanData_Walker(Node *node, void *context);

/**
 * Add the given constant to the given hash, properly checking for null and
 *   using the constant's type and value otherwise
 */
static
void
CdbHashConstValue(CdbHash *h, Const *c)
{
	if (c->constisnull)
	{
		cdbhashnull(h);
	}
	else
	{
		cdbhash(h, c->constvalue, typeIsArrayType(c->consttype) ? ANYARRAYOID : c->consttype);
	}
}

/**
 * Initialize a DirectDispatchCalculationInfo.
 */
static void
InitDirectDispatchCalculationInfo(DirectDispatchCalculationInfo *data)
{
	data->dd.isDirectDispatch = false;
	data->dd.contentIds = NULL;
	data->haveProcessedAnyCalculations = false;
}

/**
 * Mark a DirectDispatchCalculationInfo as having targeted dispatch disabled
 */
static void
DisableTargetedDispatch(DirectDispatchCalculationInfo *data)
{
	data->dd.isDirectDispatch = false;
	data->dd.contentIds = NULL; /* leaks but it's okay, we made a new memory
								 * context for the entire calculation */
	data->haveProcessedAnyCalculations = true;
}

/**
 * helper function for AssignContentIdsFromUpdateDeleteQualification
 */
static DirectDispatchCalculationInfo
GetContentIdsFromPlanForSingleRelation(List *rtable, Plan *plan, int rangeTableIndex, Node *qualification)
{
	GpPolicy   *policy = NULL;
	PartitionKeyInfo *parts = NULL;
	int			i;

	DirectDispatchCalculationInfo result;
	RangeTblEntry *rte;

	InitDirectDispatchCalculationInfo(&result);

	if (nodeTag((Node *) plan) == T_BitmapHeapScan ||
		nodeTag((Node *) plan) == T_BitmapAppendOnlyScan ||
		nodeTag((Node *) plan) == T_BitmapTableScan)
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
	{
		Relation	relation;

		rte = rt_fetch(rangeTableIndex, rtable);
		if (rte->rtekind == RTE_RELATION)
		{
			/* Get a copy of the rel's GpPolicy from the relcache. */
			relation = relation_open(rte->relid, NoLock);
			policy = RelationGetPartitioningKey(relation);

			if (policy != NULL)
			{
				parts = (PartitionKeyInfo *) palloc(policy->nattrs * sizeof(PartitionKeyInfo));
				for (i = 0; i < policy->nattrs; i++)
				{
					parts[i].attr = relation->rd_att->attrs[policy->attrs[i] - 1];
					parts[i].values = NULL;
					parts[i].numValues = 0;
					parts[i].counter = 0;
				}
			}
			relation_close(relation, NoLock);
		}
		else
		{
			/* fall through, policy will be NULL so we won't direct dispatch */
		}
	}

	if (rte->forceDistRandom ||
		policy == NULL ||
		policy->nattrs == 0 ||
		policy->ptype != POLICYTYPE_PARTITIONED)
	{
		result.dd.isDirectDispatch = false;
	}
	else
	{
		long		totalCombinations = 1;

		/* calculate possible value set for each partitioning attribute */
		for (i = 0; i < policy->nattrs; i++)
		{
			Var		   *var;
			PossibleValueSet pvs;

			var = makeVar(rangeTableIndex, policy->attrs[i], parts[i].attr->atttypid, parts[i].attr->atttypmod, 0);

			/**
			 * Note that right now we only examine the given qual.  This is okay because if there are other
			 *   quals on the plan then those would be ANDed with the qual, which can only narrow our choice
			 *   of segment and not expand it.
			 */
			pvs = DeterminePossibleValueSet(qualification, (Node *) var);

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
			result.dd.isDirectDispatch = true;
			Assert(result.dd.contentIds == NULL);	/* direct dispatch but no
													 * specific content at
													 * all! */
		}
		else if (totalCombinations > 0 &&
			/* don't bother for ones which will likely hash to many segments */
				 totalCombinations < GpIdentity.numsegments * 3)
		{
			CdbHash    *h = makeCdbHash(GpIdentity.numsegments);
			long		index = 0;

			result.dd.isDirectDispatch = true;
			result.dd.contentIds = NULL;

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
						CdbHashConstValue(h, (Const *) val);
					}
					else
					{
						result.dd.isDirectDispatch = false;
						break;
					}
					curIndex /= numValues;
				}

				if (!result.dd.isDirectDispatch)
					break;

				hashCode = cdbhashreduce(h);

				/*
				 * right now we only allow ONE contentid
				 */
				if (result.dd.contentIds == NULL)
					result.dd.contentIds = list_make1_int(hashCode);
				else if (linitial_int(result.dd.contentIds) != hashCode)
				{
					result.dd.isDirectDispatch = false;
					break;
				}
			}
		}
		else
		{
			/* know nothing, can't do directed dispatch */
			result.dd.isDirectDispatch = false;
		}
	}

	result.haveProcessedAnyCalculations = true;
	return result;
}

static void
MergeDirectDispatchCalculationInfo(DirectDispatchCalculationInfo *to, DirectDispatchCalculationInfo *from)
{
	if (!from->dd.isDirectDispatch)
	{
		/* from eliminates all options so take it */
		to->dd.isDirectDispatch = false;
	}
	else if (!to->haveProcessedAnyCalculations)
	{
		/* to has no data, so just take from */
		*to = *from;
	}
	else if (!to->dd.isDirectDispatch)
	{
		/* to cannot get better -- leave it alone */
	}
	else if (from->dd.contentIds == NULL)
	{
		/* from says that it doesn't need to run anywhere -- so we accept to */
	}
	else if (to->dd.contentIds == NULL)
	{
		/* to didn't even think it needed to run so accept from */
		to->dd.contentIds = from->dd.contentIds;
	}
	else if (linitial_int(to->dd.contentIds) != linitial_int(from->dd.contentIds))
	{
		/*
		 * we only support dispatch to single segment or all segments, so
		 * can't support this right now
		 */
		/* but this needs to be a union of the segments */
		to->dd.isDirectDispatch = false;
	}
	else
	{
		/* they matched -- no merge required! */

		Assert(list_length(to->dd.contentIds) == list_length(from->dd.contentIds));
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

/**
 * If node is not a plan then no direct dispatch data is copied in.
 *
 * Otherwise, overrides any previous direct dispatch data on the given node and updates it
 *   with the targeted dispatch info from the ContentIdAssignmentData
 *
 * @param isFromTopRoot is this being finalized on the topmost node of the tree, or on a lower motion node?
 */
static void
FinalizeDirectDispatchDataForSlice(Node *node, ContentIdAssignmentData *data, bool isFromTopRoot)
{
	/* pop it from the top and set it on the root of the slice */
	DirectDispatchCalculationInfo *ddcr = (DirectDispatchCalculationInfo *) lfirst(list_tail(data->sliceStack));

	data->sliceStack = list_truncate(data->sliceStack, list_length(data->sliceStack) - 1);

	if (is_plan_node(node))
	{
		Plan	   *plan = (Plan *) node;

		data->allSlices = lappend(data->allSlices, plan);

		list_free(plan->directDispatch.contentIds);
		plan->directDispatch.contentIds = NIL;

		if (ddcr->haveProcessedAnyCalculations)
		{
			plan->directDispatch.isDirectDispatch = ddcr->dd.isDirectDispatch;
			if (ddcr->dd.isDirectDispatch)
			{
				MemoryContext oldContext;

				if (ddcr->dd.contentIds == NULL)
				{
					ddcr->dd.contentIds = list_make1_int(cdb_randint(GpIdentity.numsegments - 1, 0));
					if (ShouldPrintTestMessages())
						elog(INFO, "DDCR learned no content dispatch is required");
				}
				else
				{
					if (ShouldPrintTestMessages())
						elog(INFO, "DDCR learned dispatch to content %d", linitial_int(ddcr->dd.contentIds));
				}

				oldContext = MemoryContextSwitchTo(data->memoryContextForOutput);
				plan->directDispatch.contentIds = list_copy(ddcr->dd.contentIds);
				MemoryContextSwitchTo(oldContext);
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
			plan->directDispatch.isDirectDispatch = false;
		}
	}

	pfree(ddcr);
}

/**
 * Recursively assign content ids to the motion nodes of each slice.
 */
static bool
AssignContentIdsToPlanData_Walker(Node *node, void *context)
{
	ContentIdAssignmentData *data = (ContentIdAssignmentData *) context;
	DirectDispatchCalculationInfo *ddcr = NULL;
	DirectDispatchCalculationInfo dispatchInfo;
	bool		pushNewDirectDispatchInfo = false;
	bool		result;

	if (node == NULL)
		return false;

	if (is_plan_node(node) || IsA(node, SubPlan))
	{
		InitDirectDispatchCalculationInfo(&dispatchInfo);

		switch (nodeTag(node))
		{
			case T_Result:
				/* no change to dispatchInfo --> just iterate children */
				break;
			case T_Append:
				/* no change to dispatchInfo --> just iterate children */
				break;
			case T_BitmapAnd:
			case T_BitmapOr:
				/* no change to dispatchInfo --> just iterate children */
				break;
			case T_BitmapHeapScan:
			case T_BitmapAppendOnlyScan:
			case T_BitmapTableScan:
				/* no change to dispatchInfo --> just iterate children */
				break;
			case T_SeqScan:
			case T_AppendOnlyScan:
			case T_AOCSScan:

				/*
				 * we can determine the dispatch data to merge by looking at
				 * the relation begin scanned
				 */
				dispatchInfo = GetContentIdsFromPlanForSingleRelation(data->rtable, (Plan *) node, ((Scan *) node)->scanrelid,
																	  (Node *) ((Plan *) node)->qual);
				break;

			case T_ExternalScan:
				DisableTargetedDispatch(&dispatchInfo); /* not sure about
														 * external tables ...
														 * so disable */
				break;

			case T_IndexScan:
				{
					IndexScan  *indexScan = (IndexScan *) node;

					/*
					 * we can determine the dispatch data to merge by looking
					 * at the relation begin scanned
					 */
					dispatchInfo = GetContentIdsFromPlanForSingleRelation(data->rtable, (Plan *) node, ((Scan *) node)->scanrelid,
																		  (Node *) indexScan->indexqualorig);
					/* must use _orig_ qual ! */
				}
				break;
			case T_BitmapIndexScan:
				{
					BitmapIndexScan *bitmapScan = (BitmapIndexScan *) node;

					/*
					 * we can determine the dispatch data to merge by looking
					 * at the relation begin scanned
					 */
					dispatchInfo = GetContentIdsFromPlanForSingleRelation(data->rtable, (Plan *) node, ((Scan *) node)->scanrelid,
																		  (Node *) bitmapScan->indexqualorig);
					/* must use original qual ! */
				}
				break;
			case T_SubqueryScan:
				/* Regular walker goes into subplans */
				break;
			case T_TidScan:
			case T_FunctionScan:
			case T_WorkTableScan:
				DisableTargetedDispatch(&dispatchInfo);
				break;
			case T_ValuesScan:
				/* no change to dispatchInfo */
				break;
			case T_NestLoop:
			case T_MergeJoin:
			case T_HashJoin:
				/* join: no change to dispatchInfo --> just iterate children */

				/*
				 * note that we could want to look at the join qual but
				 * constant checks should have been pushed down to the
				 * underlying scans so we shouldn't learn anything
				 */
				break;
			case T_Material:
			case T_Sort:
			case T_Agg:
			case T_Unique:
			case T_Hash:
			case T_SetOp:
			case T_Limit:
			case T_PartitionSelector:
				break;
			case T_Motion:

				/*
				 * receiving end knows nothing (since we don't analyze the
				 * distribution key right now)
				 */

				/*
				 * ideally we would take subsegments from analyzing the
				 * distribution key's distribution but that is not done right
				 * now
				 */
				DisableTargetedDispatch(&dispatchInfo);

				/* and we must push a new slice */
				pushNewDirectDispatchInfo = true;
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
			case T_Repeat:
			case T_RecursiveUnion:
				/* no change to dispatchInfo --> just iterate children */
				break;
			case T_SubPlan:
				{
					SubPlan    *subplan = (SubPlan *) node;

					if (!subplan->is_initplan)
					{
						/*
						 * init subplans are dispatched independently of the
						 * main plan, but for regular subplans we need to
						 * process the subplan plan as it may require
						 * dispatching to all segments even if the main plan
						 * does not need to (MPP-22019)
						 */
						Plan	   *subplan_plan = plan_tree_base_subplan_get_plan(context, subplan);

						plan_tree_walker((Node *) subplan_plan, AssignContentIdsToPlanData_Walker, context);
					}
					pushNewDirectDispatchInfo = true;
					break;
				}
			default:
				elog(ERROR, "Invalid plan node %d", nodeTag(node));
				break;
		}

		/*
		 * analyzed node type, now do the work (for all except subquery scan,
		 * which do work in the switch above and return
		 */
		if (dispatchInfo.haveProcessedAnyCalculations)
		{
			/* learned new info: merge it in */
			ddcr = llast(data->sliceStack);
			MergeDirectDispatchCalculationInfo(ddcr, &dispatchInfo);
		}

		if (pushNewDirectDispatchInfo)
		{
			/* recursing to a child slice */
			ddcr = palloc(sizeof(DirectDispatchCalculationInfo));
			InitDirectDispatchCalculationInfo(ddcr);
			data->sliceStack = lappend(data->sliceStack, ddcr);
		}

	}

	/*
	 * recurse -- must recurse even for non-plan nodes because of
	 * qualifications?
	 */

	/*
	 * note that the SubqueryScan nodes do NOT reach here -- its children are
	 * managed in the switch above
	 */
	result = plan_tree_walker(node, AssignContentIdsToPlanData_Walker, context);
	Assert(!result);

	if (pushNewDirectDispatchInfo)
	{
		FinalizeDirectDispatchDataForSlice(node, data, false);
	}

	return result;
}

/**
 * Update the plan and its descendants with markings telling which subsets of content the node can run on.
 *
 */
void
AssignContentIdsToPlanData(Query *query, Plan *plan, PlannerInfo *root)
{
	ContentIdAssignmentData data;

	/* setup */
	SwitchedMemoryContext mem = AllocSetCreateDefaultContextInCurrentAndSwitchTo("AssignContentIdsToPlanData");
	DirectDispatchCalculationInfo *ddcr = palloc(sizeof(DirectDispatchCalculationInfo));

	InitDirectDispatchCalculationInfo(ddcr);

	planner_init_plan_tree_base(&data.base, root);
	data.memoryContextForOutput = mem.oldContext;
	data.sliceStack = list_make1(ddcr);
	data.rtable = root->glob->finalrtable;
	data.allSlices = NULL;

	/* Do it! */
	AssignContentIdsToPlanData_Walker((Node *) plan, &data);

	if (!IsA(plan, SubPlan) &&!IsA(plan, Motion))
	{
		/* subplan and motion will already have been finalized */
		FinalizeDirectDispatchDataForSlice((Node *) plan, &data, true);
	}

	/*
	 * now check to see if we are multi-slice.  If so then we will disable the
	 * direct dispatch (because it's not working right now -- see MPP-7630)
	 */
	if (list_length(data.allSlices) > 2)
	{
		/*
		 * NOTE!!!  When this is fixed, make a test for subquery initPlan and
		 * qual case (hitting code in AssignContentIdsToPlanData_SubqueryScan)
		 */
		ListCell   *cell;

		foreach(cell, data.allSlices)
		{
			Plan	   *plan = (Plan *) lfirst(cell);

			plan->directDispatch.isDirectDispatch = false;
			list_free(plan->directDispatch.contentIds);
			plan->directDispatch.contentIds = NIL;
		}
	}

	DeleteAndRestoreSwitchedMemoryContext(mem);
}
