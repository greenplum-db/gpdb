/*-------------------------------------------------------------------------
 *
 * cdbmutate.c
 *	  Parallelize a PostgreSQL sequential plan tree.
 *
 * Portions Copyright (c) 2004-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbmutate.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "access/xact.h"
#include "nodes/bitmapset.h"
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "parser/parse_oper.h"	/* for compatible_oper_opid() */
#include "utils/relcache.h"		/* RelationGetPartitioningKey() */
#include "optimizer/cost.h"
#include "optimizer/tlist.h"	/* get_sortgroupclause_tle() */
#include "optimizer/planmain.h"
#include "optimizer/predtest.h"
#include "optimizer/var.h"
#include "parser/parse_relation.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/datum.h"
#include "utils/syscache.h"
#include "utils/portal.h"
#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "nodes/makefuncs.h"

#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "catalog/gp_policy.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"

#include "catalog/pg_proc.h"
#include "catalog/pg_trigger.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbsetop.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbtargeteddispatch.h"

#include "executor/executor.h"

/*
 * An ApplyMotionState holds state for the recursive apply_motion_mutator().
 * It is externalized here to make it shareable by helper code in other
 * modules, e.g., cdbaggmutate.c.
 */
typedef struct ApplyMotionState
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	int			nextMotionID;
	int			sliceDepth;
	bool		containMotionNodes;
	HTAB	   *planid_subplans; /* hash table for InitPlanItem */
} ApplyMotionState;

typedef struct InitPlanItem
{
	int plan_id; /* plan id of the init plan */
	List *subplans; /* list of subplans that refer to the same init plan */
} InitPlanItem;

typedef struct
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	EState	   *estate;
	bool		single_row_insert;
	List	   *cursorPositions;
} pre_dispatch_function_evaluation_context;

static Node *planner_make_plan_constant(struct PlannerInfo *root, Node *n, bool is_SRI);

static Node *pre_dispatch_function_evaluation_mutator(Node *node,
										 pre_dispatch_function_evaluation_context *context);

/*
 * Forward Declarations
 */
static void assignMotionID(Node *newnode, ApplyMotionState *context, Node *oldnode);

static void add_slice_to_motion(Motion *motion,
					MotionType motionType,
					List *hashExprs, List *hashOpfamilies, int numsegments,
					bool isBroadcast, CdbLocusType targetLocus);

static Node *apply_motion_mutator(Node *node, ApplyMotionState *context);

static bool replace_shareinput_targetlists_walker(Node *node, PlannerInfo *root, bool fPop);

static bool fixup_subplan_walker(Node *node, SubPlanWalkerContext *context);

/*
 * Is target list of a Result node all-constant?
 *
 * That means that there is no subplan, and the elements of the
 * targetlist are Const nodes.
 */
static bool
allConstantValuesClause(Plan *node)
{
	ListCell   *cell;
	TargetEntry *tle;

	Assert(IsA(node, Result));

	if (node->lefttree != NULL)
		return false;

	foreach(cell, node->targetlist)
	{
		tle = (TargetEntry *) lfirst(cell);

		Assert(tle->expr);

		if (!IsA(tle->expr, Const))
			return false;
	}

	return true;
}

static void
directDispatchCalculateHash(Plan *plan, GpPolicy *targetPolicy, Oid *hashfuncs)
{
	int			i;
	ListCell   *cell = NULL;
	bool		directDispatch;
	Datum	   *values;
	bool	   *nulls;

	values = (Datum *) palloc(targetPolicy->nattrs * sizeof(Datum));
	nulls = (bool *) palloc(targetPolicy->nattrs * sizeof(bool));

	/*
	 * the nested loops here seem scary -- especially since we've already
	 * walked them before -- but I think this is still required since they may
	 * not be in the same order. (also typically we don't distribute by more
	 * than a handful of attributes).
	 */
	directDispatch = true;
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
				directDispatch = false;
				break;
			}

			c = (Const *) tle->expr;

			values[i] = c->constvalue;
			nulls[i] = c->constisnull;
			break;
		}

		if (!directDispatch)
			break;
	}

	plan->directDispatch.isDirectDispatch = directDispatch;
	if (directDispatch)
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
		plan->directDispatch.contentIds = list_make1_int(hashcode);

		elog(DEBUG1, "sending single row constant insert to content %d", hashcode);
	}
	pfree(values);
	pfree(nulls);
}

/*
 * Create a GpPolicy that matches the Flow of the given plan.
 *
 * This is used with CREATE TABLE AS, to derive the distribution
 * key for the table from the query plan.
 */
static GpPolicy *
get_partitioned_policy_from_flow(Plan *plan)
{
	/* Find out what the flow is partitioned on */
	List	   *policykeys;
	List	   *policyopclasses;
	ListCell   *exp_cell;
	ListCell   *opf_cell;

	/*
	 * Is it a Hashed distribution?
	 *
	 * NOTE: HashedOJ is not OK, because we cannot let the NULLs be stored
	 * multiple segments.
	 */
	if (plan->flow->locustype != CdbLocusType_Hashed)
	{
		return NULL;
	}

	/*
	 * Sometimes the planner produces a Flow with CdbLocusType_Hashed,
	 * but hashExpr are not set because we have lost track of the
	 * expressions it's hashed on.
	 */
	if (!plan->flow->hashExprs)
		return NULL;

	policykeys = NIL;
	policyopclasses = NIL;
	forboth(exp_cell, plan->flow->hashExprs,
			opf_cell, plan->flow->hashOpfamilies)
	{
		Expr	   *var1 = (Expr *) lfirst(exp_cell);
		Oid			opf1 = lfirst_oid(opf_cell);
		AttrNumber	n;
		bool		found_expr = false;

		/*
		 * Right side variable may be encapsulated by a relabel node.
		 * Motion, however, does not care about relabel nodes.
		 */
		if (IsA(var1, RelabelType))
			var1 = ((RelabelType *) var1)->arg;

		/* See if this Expr is a column of the result table */

		for (n = 1; n <= list_length(plan->targetlist); n++)
		{
			TargetEntry *target = get_tle_by_resno(plan->targetlist, n);
			Var		   *new_var;

			if (target->resjunk)
				continue;

			/*
			 * If subplan expr is a Var, copy to preserve its EXPLAIN info.
			 */
			if (IsA(target->expr, Var))
			{
				new_var = copyObject(target->expr);
				new_var->varno = OUTER_VAR;
				new_var->varattno = n;
			}

			/*
			 * Make a Var that references the target list entry at this
			 * offset, using OUTER_VAR as the varno
			 */
			else
				new_var = makeVar(OUTER_VAR,
								  n,
								  exprType((Node *) target->expr),
								  exprTypmod((Node *) target->expr),
								  exprCollation((Node *) target->expr),
								  0);

			if (equal(var1, new_var))
			{
				/*
				 * If it is, use it to partition the result table, to avoid
				 * unnecessary redistribution of data
				 */
				Oid			opclass;

				Assert(list_length(policykeys) < MaxPolicyAttributeNumber);

				if (list_member_int(policykeys, n))
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_COLUMN),
							 errmsg("duplicate DISTRIBUTED BY column '%s'",
									target->resname ? target->resname : "???")));


				/*
				 * We know the operator family to use, but the problem is, we
				 * don't know the exact operator class within the family. In
				 * the common case, the datatype has exactly one default
				 * operator, and you usually use only that. So look up the
				 * default operator class for the datatype, and if it's in
				 * the same operator family, use that.
				 *
				 * If that fails, we could do some further checks. We could
				 * check if there is exactly one operator class for the
				 * datatype in the operator family, and if so, use that.
				 * But it doesn't seem worth adding much extra code to deal
				 * with more obscure cases. Deriving the distribution key
				 * from the query plan is a heuristic, anyway.
				 */
				opclass = GetDefaultOpClass(new_var->vartype, HASH_AM_OID);
				if (get_opclass_family(opclass) != opf1)
					continue;	/* not compatible */

				policykeys = lappend_int(policykeys, n);
				policyopclasses = lappend_oid(policyopclasses, opclass);
				found_expr = true;
				break;
			}
		}

		if (!found_expr)
		{
			/*
			 * This distribution key is not present in the target list. Give
			 * up.
			 */
			return NULL;
		}
	}

	/*
	 * We use the default number of segments, even if the flow was partially
	 * distributed. That defeats the performance benefit of using the same
	 * distribution key columns, because we'll need a Restribute Motion
	 * anyway. But presumably if the user had expanded the cluster, they want
	 * to use all the segments for new tables.
	 */
	return createHashPartitionedPolicy(policykeys,
									   policyopclasses,
									   GP_POLICY_DEFAULT_NUMSEGMENTS());
}


/* -------------------------------------------------------------------------
 * Function apply_motion() and apply_motion_mutator() add motion nodes to a
 * top-level Plan tree as directed by the Flow nodes in the plan.
 *
 * In addition, if the argument Plan produces a partitioned flow, a Motion
 * appropriate to the command is added to the top. For example, to consolidate
 * the tuple streams from a SELECT command into a single tuple stream on the
 * dispatcher.
 *
 * The result is a deep copy of the argument Plan tree with added Motion
 * nodes.
 *
 * TODO Maybe add more context to argument list!
 * -------------------------------------------------------------------------
 */
Plan *
apply_motion(PlannerInfo *root, Plan *plan, Query *query)
{
	Plan	   *result;
	ListCell   *cell;
	GpPolicy   *targetPolicy = NULL;
	InitPlanItem *item;
	GpPolicyType targetPolicyType = POLICYTYPE_ENTRY;
	ApplyMotionState state;
	HASHCTL ctl;
	HASH_SEQ_STATUS status;
	bool		needToAssignDirectDispatchContentIds = false;
	bool		bringResultToDispatcher = false;
	int			numsegments = getgpsegmentCount();

	/* Initialize mutator context. */

	planner_init_plan_tree_base(&state.base, root); /* error on attempt to
													 * descend into subplan
													 * plan */
	state.nextMotionID = 1;		/* Start at 1 so zero will mean "unassigned". */
	state.sliceDepth = 0;
	state.containMotionNodes = false;
	memset(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(int);
	ctl.entrysize = sizeof(InitPlanItem);
	ctl.hash = tag_hash;
	ctl.hcxt = CurrentMemoryContext;
	state.planid_subplans = hash_create("plan_id to subplans", 8, &ctl,
										 HASH_ELEM | HASH_CONTEXT | HASH_FUNCTION);

	Assert(is_plan_node((Node *) plan) && IsA(query, Query));

	/* Does query have a target relation?  (INSERT/DELETE/UPDATE) */

	/*
	 * NOTE: This code makes the assumption that if we are working on a
	 * hierarchy of tables, all the tables are distributed, or all are on the
	 * entry DB.  Any mixture will fail
	 */
	if (query->resultRelation > 0)
	{
		RangeTblEntry *rte = rt_fetch(query->resultRelation, query->rtable);

		Assert(rte->rtekind == RTE_RELATION);

		targetPolicy = GpPolicyFetch(rte->relid);
		targetPolicyType = targetPolicy->ptype;
	}

	switch (query->commandType)
	{
		case CMD_SELECT:
			/* If the query comes from 'CREATE TABLE AS' or 'SELECT INTO' */
			if (query->parentStmtType != PARENTSTMTTYPE_NONE)
			{
				if (query->intoPolicy != NULL)
				{
					targetPolicy = query->intoPolicy;

					Assert(query->intoPolicy->ptype != POLICYTYPE_ENTRY);
					Assert(query->intoPolicy->nattrs >= 0);
					Assert(query->intoPolicy->nattrs <= MaxPolicyAttributeNumber);
				}
				else if (gp_create_table_random_default_distribution)
				{
					targetPolicy = createRandomPartitionedPolicy(GP_POLICY_DEFAULT_NUMSEGMENTS());
					ereport(NOTICE,
							(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
							 errmsg("using default RANDOM distribution since no distribution was specified"),
							 errhint("Consider including the 'DISTRIBUTED BY' clause to determine the distribution of rows.")));
				}
				else
				{
					/* First try to deduce the distribution from the query */
					targetPolicy = get_partitioned_policy_from_flow(plan);

					/*
					 * If that fails, hash on the first hashable column we can
					 * find.
					 */
					if (!targetPolicy)
					{
						int			i;
						List	   *policykeys = NIL;
						List	   *policyopclasses = NIL;

						for (i = 0; i < list_length(plan->targetlist); i++)
						{
							TargetEntry *target =
							get_tle_by_resno(plan->targetlist, i + 1);

							if (target)
							{
								Oid			typeOid = exprType((Node *) target->expr);
								Oid			opclass = InvalidOid;

								/*
								 * Check for a legacy hash operator class if
								 * gp_use_legacy_hashops GUC is set. If
								 * InvalidOid is returned or the GUC is not
								 * set, we'll get the default operator class.
								 */
								if (gp_use_legacy_hashops)
									opclass = get_legacy_cdbhash_opclass_for_base_type(typeOid);

								if (!OidIsValid(opclass))
									opclass = cdb_default_distribution_opclass_for_type(typeOid);


								if (OidIsValid(opclass))
								{
									policykeys = lappend_int(policykeys, i + 1);
									policyopclasses = lappend_oid(policyopclasses, opclass);
									break;
								}
							}
						}
						targetPolicy = createHashPartitionedPolicy(policykeys,
																   policyopclasses,
																   GP_POLICY_DEFAULT_NUMSEGMENTS());
					}

					/* If we deduced the policy from the query, give a NOTICE */
					if (query->parentStmtType == PARENTSTMTTYPE_CTAS)
					{
						StringInfoData columnsbuf;
						int			i;

						initStringInfo(&columnsbuf);
						for (i = 0; i < targetPolicy->nattrs; i++)
						{
							TargetEntry *target = get_tle_by_resno(plan->targetlist, targetPolicy->attrs[i]);

							if (i > 0)
								appendStringInfoString(&columnsbuf, ", ");
							if (target->resname)
								appendStringInfoString(&columnsbuf, target->resname);
							else
								appendStringInfoString(&columnsbuf, "???");

						}
						ereport(NOTICE,
								(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
								 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column(s) "
										"named '%s' as the Greenplum Database data distribution key for this "
										"table. ", columnsbuf.data),
								 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
										 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
					}
				}

				query->intoPolicy = targetPolicy;

				Assert(query->intoPolicy->ptype != POLICYTYPE_ENTRY);

				/*
				 * To copy data from old table to new table we must set
				 * motion's dest numsegments the same as new table.
				 */
				numsegments = query->intoPolicy->numsegments;

				if (GpPolicyIsReplicated(query->intoPolicy))
				{
					bool needBroadcast = true;
					/*
					 * CdbLocusType_SegmentGeneral is only used by replicated
					 * table right now, so if both input and target are replicated
					 * table, no need to add a motion.
					 * And if plan's data are available on all segment as
					 * CdbLocusType_General, no motion needed.
					 * The above two cases have an exception if the plan contains volatile
					 * target list or havingQual, we can not run it on every segments.
					 */

					if (plan->flow->flotype == FLOW_SINGLETON &&
						(plan->flow->locustype == CdbLocusType_General ||
						 plan->flow->locustype == CdbLocusType_SegmentGeneral))
					{
						if (contain_volatile_functions((Node *) query->targetList) ||
							contain_volatile_functions(query->havingQual))
						{
							plan->flow->locustype = CdbLocusType_SingleQE;
						}
						else
							needBroadcast = plan->flow->numsegments == numsegments ? false : true;
					}

					if (needBroadcast)
					{
						if (!broadcastPlan(plan, false, false, numsegments))
							ereport(ERROR,
									(errcode(ERRCODE_GP_FEATURE_NOT_YET),
									 errmsg("cannot parallelize that SELECT INTO yet")));
					}

				}
				else
				{
					/*
					 * Make sure the top level flow is partitioned on the
					 * partitioning key of the target relation.	Since this is
					 * a SELECT INTO (basically same as an INSERT) command,
					 * the target list will correspond to the attributes of
					 * the target relation in order.
					 */
					List	   *hashExprs;
					List	   *hashOpfamilies;

					hashExprs = getExprListFromTargetList(plan->targetlist,
														  targetPolicy->nattrs,
														  targetPolicy->attrs,
														  true);
					hashOpfamilies = NIL;
					for (int i = 0; i < targetPolicy->nattrs; i++)
					{
						Oid			opfamily = get_opclass_family(targetPolicy->opclasses[i]);

						hashOpfamilies = lappend_oid(hashOpfamilies, opfamily);
					}

					if (!repartitionPlan(plan, false, false,
										 hashExprs, hashOpfamilies,
										 numsegments))
						ereport(ERROR,
								(errcode(ERRCODE_GP_FEATURE_NOT_YET),
								 errmsg("cannot parallelize that SELECT INTO yet")));
				}

				Assert(query->intoPolicy->ptype != POLICYTYPE_ENTRY);
			}
			else
			{
				if ((plan->flow->flotype == FLOW_PARTITIONED ||
					(plan->flow->flotype == FLOW_SINGLETON &&
					 plan->flow->locustype == CdbLocusType_SegmentGeneral)) &&
					 !root->glob->is_parallel_cursor)
					bringResultToDispatcher = true;

				needToAssignDirectDispatchContentIds = root->config->gp_enable_direct_dispatch;
			}

			break;

		case CMD_INSERT:
			if (query->returningList)
			{
				bringResultToDispatcher = true;
			}
			break;

		case CMD_UPDATE:
		case CMD_DELETE:
			needToAssignDirectDispatchContentIds = root->config->gp_enable_direct_dispatch;
			if (query->returningList)
			{
				bringResultToDispatcher = true;
			}
			break;

		case CMD_UTILITY:
			break;

		default:
			Insist(0);			/* Never see non-DML in here! */
	}

	if (bringResultToDispatcher)
	{
		/*
		 * Query result needs to be brought back to the QD. Ask for
		 * motion to a single QE.  Later, apply_motion will override
		 * that to bring it to the QD instead.
		 *
		 * If the query has an ORDER BY clause, use Merge Receive to
		 * preserve the ordering.
		 *
		 * The plan has already been set up to ensure each qExec's
		 * result is properly ordered according to the ORDER BY
		 * specification.  The existing ordering could be stronger
		 * than required; if so, omit the extra trailing columns from
		 * the Merge Receive key.
		 *
		 * An unordered result is ok if the ORDER BY ordering is
		 * degenerate (on constant exprs) or the result is known to
		 * have at most one row.
		 */
		if (query->sortClause)
		{
			Insist(focusPlan(plan, true, false));
		}

		/* Use UNION RECEIVE.  Does not preserve ordering. */
		else
			Insist(focusPlan(plan, false, false));
	}

	result = (Plan *) apply_motion_mutator((Node *) plan, &state);

	if (needToAssignDirectDispatchContentIds)
	{
		/* figure out if we can run on a reduced set of nodes */
		AssignContentIdsToPlanData(query, result, root);
	}

	Assert(result->nMotionNodes == state.nextMotionID - 1);
	Assert(result->nInitPlans == hash_get_num_entries(state.planid_subplans));

	hash_seq_init(&status, state.planid_subplans);
	while ((item = (InitPlanItem *) hash_seq_search(&status)) != NULL)
	{
		int plan_id = item->plan_id;
		int sliceId = state.nextMotionID++;
		List *subplans = item->subplans;
		Assert(list_length(subplans) > 0);
		foreach(cell, subplans)
		{
			SubPlan *subplan = (SubPlan *) lfirst(cell);
			Assert(IsA(subplan, SubPlan) && subplan->qDispSliceId == 0 &&
					subplan->plan_id == plan_id);
			subplan->qDispSliceId = sliceId;
		}
	}
	hash_destroy(state.planid_subplans);

	/*
	 * Discard subtrees of Query node that aren't needed for execution. Note
	 * the targetlist (query->targetList) is used in execution of prepared
	 * statements, so we leave that alone.
	 */
	query->jointree = NULL;
	query->groupClause = NIL;
	query->havingQual = NULL;
	query->distinctClause = NIL;
	query->sortClause = NIL;
	query->limitCount = NULL;
	query->limitOffset = NULL;
	query->setOperations = NULL;

	return result;
}


/*
 * Function apply_motion_mutator() is the workhorse for apply_motion().
 */
static Node *
apply_motion_mutator(Node *node, ApplyMotionState *context)
{
	Node	   *newnode;
	Plan	   *plan;
	Flow	   *flow;
	int			saveNextMotionID;
	int			saveNumInitPlans;
	int			saveSliceDepth;

#ifdef USE_ASSERT_CHECKING
	PlannerInfo *root = (PlannerInfo *) context->base.node;
	Assert(root && IsA(root, PlannerInfo));
#endif

	if (node == NULL)
		return NULL;

	/* An expression node might have subtrees containing plans to be mutated. */
	if (!is_plan_node(node))
	{
		/*
		 * The containMotionNodes flag keeps track of whether there are any
		 * Motion nodes, ignoring any in InitPlans. So if we recurse into an
		 * InitPlan, save and restore the flag.
		 */
		if (IsA(node, SubPlan) &&((SubPlan *) node)->is_initplan)
		{
			bool		found;
			bool		saveContainMotionNodes = context->containMotionNodes;
			int			saveSliceDepth = context->sliceDepth;
			SubPlan		*subplan = (SubPlan *) node;
			/*
			 * If the init-plan refered by `subplan` has been visited, we should
			 * not re-visit the subplan, or the motions under the init-plan are
			 * re-counted.
			 */
			hash_search(context->planid_subplans, &subplan->plan_id,
						HASH_FIND, &found);
			if (found)
				return node;

			/* reset sliceDepth for each init plan */
			context->sliceDepth = 0;
			node = plan_tree_mutator(node, apply_motion_mutator, context);

			context->containMotionNodes = saveContainMotionNodes;
			context->sliceDepth = saveSliceDepth;

			return node;
		}
		else
			return plan_tree_mutator(node, apply_motion_mutator, context);
	}

	plan = (Plan *) node;
	flow = plan->flow;

	saveNextMotionID = context->nextMotionID;
	saveNumInitPlans = hash_get_num_entries(context->planid_subplans);

	/* Descending into a subquery or a new slice? */
	saveSliceDepth = context->sliceDepth;
	Assert(!IsA(node, Query));
	if (IsA(node, Motion))
		context->sliceDepth++;
	else if (flow && flow->req_move != MOVEMENT_NONE)
		context->sliceDepth++;

	/*
	 * Copy this node and mutate its children. Afterward, this node should be
	 * an exact image of the input node, except that contained nodes requiring
	 * parallelization will have had it applied.
	 */
	newnode = plan_tree_mutator(node, apply_motion_mutator, context);

	context->sliceDepth = saveSliceDepth;

	plan = (Plan *) newnode;
	flow = plan->flow;

	/* Make one big list of all of the initplans. */
	if (plan->initPlan)
	{
		ListCell   *cell;
		SubPlan    *subplan;

		/*
		 * Collect all subplans that refer to an init-plan for later usage.
		 * The subplans that refers to an init-plan update their `qDispSliceId`
		 * in apply_motion().
		 * Two or more subplans may refer to the same init-plan, so we group them
		 * by the plan_id of the init-plan.
		 */
		foreach(cell, plan->initPlan)
		{
			InitPlanItem *item;
			bool found;
			subplan = (SubPlan *) lfirst(cell);
			Assert(IsA(subplan, SubPlan));
			Assert(root);
			Assert(planner_subplan_get_plan(root, subplan));

			item = hash_search(context->planid_subplans,
										&subplan->plan_id,
										HASH_ENTER,
										&found);
			if (!found)
				item->subplans = NIL;
			item->subplans = lappend(item->subplans, subplan);
		}
	}

	/* Pre-existing Motion nodes must be renumbered. */
	if (IsA(newnode, Motion) &&flow->req_move != MOVEMENT_NONE)
	{
		plan = ((Motion *) newnode)->plan.lefttree;
		flow = plan->flow;
		newnode = (Node *) plan;
	}

	if (IsA(newnode, Motion))
	{
		Motion	   *motion = (Motion *) newnode;

		/* Sanity check */
		/* Sub plan must have flow */
		Assert(flow && motion->plan.lefttree->flow);
		Assert(flow->req_move == MOVEMENT_NONE && !flow->flow_before_req_move);

		/* If top slice marked as singleton, make it a dispatcher singleton. */
		if (motion->motionType == MOTIONTYPE_FIXED
			&& !motion->isBroadcast
			&& context->sliceDepth == 0)
		{
			flow->segindex = -1;
		}

		/*
		 * For non-top slice, if this motion is QE singleton and subplan's locus
		 * is CdbLocusType_SegmentGeneral, omit this motion.
		 */
		if (context->sliceDepth > 0 &&
			flow->flotype == FLOW_SINGLETON &&
			flow->segindex == 0 &&
			motion->plan.lefttree->flow->locustype == CdbLocusType_SegmentGeneral)
		{
			/* omit this motion */
			newnode = (Node *)motion->plan.lefttree;

			/* Don't need assign a motion node Id */
			goto done;
		}

		/* Assign unique node number to the new node. */
		assignMotionID(newnode, context, node);

		goto done;
	}

	/*
	 * If no Flow node, we're in a portion of the tree that was finished by
	 * the planner and already contains any needed Motion nodes.
	 */
	if (!flow)
		goto done;

	Assert(IsA(flow, Flow));

	/* Add motion atop the copied node, if necessary. */
	switch (flow->req_move)
	{
		case MOVEMENT_FOCUS:
			/* If top slice is a singleton, let it execute on qDisp. */
			if (flow->segindex >= 0 &&
				context->sliceDepth == 0)
				flow->segindex = -1;

			newnode = (Node *) make_union_motion(plan, true, flow->numsegments,
												 flow->segindex == -1 ?
													CdbLocusType_Entry : CdbLocusType_SingleQE);
			break;

		case MOVEMENT_BROADCAST:
			newnode = (Node *) make_broadcast_motion(plan, true /* useExecutorVarFormat */,
													 flow->numsegments);
			break;

		case MOVEMENT_REPARTITION:
			newnode = (Node *) make_hashed_motion(plan,
												  flow->hashExprs,
												  flow->hashOpfamilies,
												  true	/* useExecutorVarFormat */,
												  flow->numsegments);
			break;

		case MOVEMENT_EXPLICIT:

			/*
			 * add an ExplicitRedistribute motion node only if child plan
			 * nodes have a motion node
			 */
			if (context->containMotionNodes)
			{
				/*
				 * motion node in child nodes: add a ExplicitRedistribute
				 * motion
				 */
				newnode = (Node *) make_explicit_motion(plan,
														flow->segidColIdx,
														true	/* useExecutorVarFormat */
					);
			}
			else
			{
				/*
				 * no motion nodes in child plan nodes - no need for
				 * ExplicitRedistribute: restore flow
				 */
				flow->req_move = MOVEMENT_NONE;
				flow->flow_before_req_move = NULL;
			}

			break;

		case MOVEMENT_NONE:
			/* Update flow if reassigning singleton top slice to qDisp. */
			if (flow->flotype == FLOW_SINGLETON &&
				flow->segindex >= 0 &&
				context->sliceDepth == 0)
			{
				flow->segindex = -1;
			}
			break;

		default:
			Insist(0);
			break;
	}

	/*
	 * After adding Motion node, assign slice id and restore subplan's Flow.
	 */
	if (flow->flow_before_req_move)
	{
		Assert(flow->req_move != MOVEMENT_NONE &&
			   IsA(newnode, Motion) &&
			   plan == ((Motion *) newnode)->plan.lefttree);

		/* Assign unique node number to the new node. */
		assignMotionID(newnode, context, NULL);

		/* Replace the subplan's modified Flow with the original. */
		plan->flow = flow->flow_before_req_move;
	}
	else
		Assert(flow->req_move == MOVEMENT_NONE);

done:

	/*
	 * Label the Plan node with the number of Motion nodes and Init Plans in
	 * this subtree, inclusive of the node itself.  This info is used only in
	 * the top node of a query or subquery.  Someday, find a better place to
	 * keep it.
	 */
	plan = (Plan *) newnode;
	plan->nMotionNodes = context->nextMotionID - saveNextMotionID;
	plan->nInitPlans = hash_get_num_entries(context->planid_subplans) - saveNumInitPlans;

	/*
	 * Remember if this was a Motion node. This is used at the top of the
	 * tree, with MOVEMENT_EXPLICIT, to avoid adding an explicit motion, if
	 * there were no Motion in the subtree. Note that this does not take
	 * InitPlans containing Motion nodes into account. InitPlans are executed
	 * as a separate step before the main plan, and hence any Motion nodes in
	 * them don't need to affect the way the main plan is executed.
	 */
	if (IsA(newnode, Motion))
		context->containMotionNodes = true;

	return newnode;
}								/* apply_motion_mutator */


/*
 * Helper code for ApplyMotionState -- Assign motion id to new Motion node.
 */
static void
assignMotionID(Node *newnode, ApplyMotionState *context, Node *oldnode)
{
	Assert(IsA(newnode, Motion) &&context != NULL);
	Assert(oldnode == NULL || (IsA(oldnode, Motion) &&oldnode != newnode));

	/* Assign unique node number to the new node. */
	((Motion *) newnode)->motionID = context->nextMotionID++;

	/* Debugging hint that we have reassigned a previously assigned ID. */
	if (oldnode != NULL && ((Motion *) oldnode)->motionID != 0)
	{
		ereport(DEBUG2, (
						 errmsg("Motion node renumbered: old=%d new=%d.",
								((Motion *) oldnode)->motionID,
								((Motion *) newnode)->motionID)
						 ));
	}
}

static void
add_slice_to_motion(Motion *motion,
					MotionType motionType,
					List *hashExprs, List *hashOpfamilies, int numsegments,
					bool isBroadcast, CdbLocusType targetLocus)
{
	Oid		   *hashFuncs;
	ListCell   *expr_cell;
	ListCell   *opf_cell;
	int			i;

	Assert(numsegments > 0);
	if (numsegments == GP_POLICY_INVALID_NUMSEGMENTS())
	{
		Assert(!"what's the proper value of numsegments?");
	}

	AssertImply(isBroadcast, motionType == MOTIONTYPE_FIXED);
	Assert(list_length(hashExprs) == list_length(hashOpfamilies));

	hashFuncs = palloc(list_length(hashExprs) * sizeof(Oid));
	i = 0;
	forboth(expr_cell, hashExprs, opf_cell, hashOpfamilies)
	{
		Node	   *expr = lfirst(expr_cell);
		Oid			opfamily = lfirst_oid(opf_cell);
		Oid			typeoid = exprType(expr);

		hashFuncs[i++] = cdb_hashproc_in_opfamily(opfamily, typeoid);
	}

	motion->motionType = motionType;
	motion->hashExprs = hashExprs;
	motion->hashFuncs = hashFuncs;
	motion->isBroadcast = isBroadcast;

	Assert(motion->plan.lefttree);

	/* Attach a descriptive Flow. */
	switch (motion->motionType)
	{
		case MOTIONTYPE_HASH:
			motion->plan.flow = makeFlow(FLOW_PARTITIONED, numsegments);
			motion->plan.flow->locustype = CdbLocusType_Hashed;
			motion->plan.flow->hashExprs = copyObject(motion->hashExprs);
			motion->plan.flow->hashOpfamilies = copyObject(hashOpfamilies);

			break;
		case MOTIONTYPE_FIXED:
			Assert(targetLocus == CdbLocusType_Null || targetLocus == CdbLocusType_Entry
				   || targetLocus == CdbLocusType_SingleQE);
			if (motion->isBroadcast)
			{
				/* broadcast */
				motion->plan.flow = makeFlow(FLOW_REPLICATED, numsegments);
				motion->plan.flow->locustype = CdbLocusType_Replicated;

			}
			else
			{
				/* Focus motion */
				motion->plan.flow = makeFlow(FLOW_SINGLETON, numsegments);
				if (targetLocus == CdbLocusType_Entry)
				{
					motion->plan.flow->locustype = CdbLocusType_Entry;
					motion->plan.flow->segindex = -1;
				}
				else
					motion->plan.flow->locustype = CdbLocusType_SingleQE;
			}

			break;
		case MOTIONTYPE_EXPLICIT:
			motion->plan.flow = makeFlow(FLOW_PARTITIONED, numsegments);

			/*
			 * TODO: antova - Nov 18, 2010; add a special locus type for
			 * ExplicitRedistribute flows
			 */
			motion->plan.flow->locustype = CdbLocusType_Strewn;
			break;
		default:
			Assert(!"Invalid motion type");
			motion->plan.flow = NULL;
			break;
	}

	Assert(motion->plan.flow);
}

Motion *
make_union_motion(Plan *lefttree, bool useExecutorVarFormat, int numsegments,
				  CdbLocusType targetLocus)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL, /* no ordering */
						 useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_FIXED, NIL, NIL, numsegments, false, targetLocus);
	return motion;
}

Motion *
make_sorted_union_motion(PlannerInfo *root, Plan *lefttree, int numSortCols,
						 AttrNumber *sortColIdx, Oid *sortOperators,
						 Oid *collations, bool *nullsFirst,
						 bool useExecutorVarFormat, int numsegments,
						 CdbLocusType targetLocus)
{
	Motion	   *motion;

	motion = make_motion(root, lefttree,
						 numSortCols, sortColIdx, sortOperators, collations, nullsFirst,
						 useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_FIXED, NIL, NIL, numsegments, false, targetLocus);
	return motion;
}

Motion *
make_hashed_motion(Plan *lefttree,
				   List *hashExprs,
				   List *hashOpfamilies,
				   bool useExecutorVarFormat,
				   int numsegments)
{
	Motion	   *motion;

	Assert(list_length(hashExprs) == list_length(hashOpfamilies));

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL, /* no ordering */
						 useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_HASH,
						hashExprs, hashOpfamilies, numsegments,
						false, CdbLocusType_Null);
	return motion;
}

Motion *
make_broadcast_motion(Plan *lefttree, bool useExecutorVarFormat,
					  int numsegments)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL, /* no ordering */
						 useExecutorVarFormat);

	add_slice_to_motion(motion, MOTIONTYPE_FIXED,
						NIL, NIL, numsegments,
						true, CdbLocusType_Null);
	return motion;
}

Motion *
make_explicit_motion(Plan *lefttree, AttrNumber segidColIdx, bool useExecutorVarFormat)
{
	Motion	   *motion;
	int			numsegments;

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL, /* no ordering */
						 useExecutorVarFormat);

	Assert(segidColIdx > 0 && segidColIdx <= list_length(lefttree->targetlist));

	motion->segidColIdx = segidColIdx;

	/*
	 * For explicit motion data come back to the source segments,
	 * so numsegments is also the same with source.
	 */
	numsegments = lefttree->flow->numsegments;

	add_slice_to_motion(motion, MOTIONTYPE_EXPLICIT,
						NIL, NIL, numsegments,
						false, CdbLocusType_Null);
	return motion;
}

/* --------------------------------------------------------------------
 *
 *	Static Helper routines
 * --------------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 * getExprListFromTargetList
 *
 * Creates a VAR that references the indicated entries from the target list,
 * sets the restype and restypmod fields from the target list info,
 * and puts them into a list.
 *
 * The AttrNumber indexes actually refer to the 1 based index into the
 * target list.
 *
 * The entries have the varno field replaced by references in OUTER_VAR.
 * ----------------------------------------------------------------
 */
List *
getExprListFromTargetList(List *tlist,
						  int numCols,
						  AttrNumber *colIdx,
						  bool useExecutorVarFormat)
{
	int			i;
	List	   *elist = NIL;

	for (i = 0; i < numCols; i++)
	{
		/* Find expr in TargetList */
		AttrNumber	n = colIdx[i];

		TargetEntry *target = get_tle_by_resno(tlist, n);

		if (target == NULL)
			elog(ERROR, "no tlist entry for key %d", n);

		/* After set_plan_references(), make a Var referencing subplan tlist. */
		if (useExecutorVarFormat)
			elist = lappend(elist, cdbpullup_make_expr(OUTER_VAR, n, target->expr, false));

		/* Before set_plan_references(), copy the subplan's result expr. */
		else
			elist = lappend(elist, copyObject(target->expr));
	}

	return elist;
}

/*
 * Request an ExplicitRedistribute motion node for a plan node
 */
void
request_explicit_motion(Plan *plan, Index resultRelationsIdx, List *rtable)
{
	/* request a segid redistribute motion */
	/* create a shallow copy of the plan flow */
	Flow	   *flow = plan->flow;

	plan->flow = (Flow *) palloc(sizeof(*(plan->flow)));
	*(plan->flow) = *flow;

	/* save original flow information */
	plan->flow->flow_before_req_move = flow;

	/* request a SegIdRedistribute motion node */
	plan->flow->req_move = MOVEMENT_EXPLICIT;

	/* find segid column in target list */
	AttrNumber	segidColIdx = ExecFindJunkAttributeInTlist(plan->targetlist, "gp_segment_id");
	if (segidColIdx == InvalidAttrNumber)
		elog(ERROR, "could not find gp_segment_id column in target list");

	plan->flow->segidColIdx = segidColIdx;
}

static void
failIfUpdateTriggers(Relation relation)
{
	bool	found = false;

	if (relation->rd_rel->relhastriggers && NULL == relation->trigdesc)
		RelationBuildTriggers(relation);

	if (!relation->trigdesc)
		return;

	if (relation->rd_rel->relhastriggers)
	{
		for (int i = 0; i < relation->trigdesc->numtriggers && !found; i++)
		{
			Trigger trigger = relation->trigdesc->triggers[i];
			found = trigger_enabled(trigger.tgoid) &&
					(get_trigger_type(trigger.tgoid) & TRIGGER_TYPE_UPDATE) == TRIGGER_TYPE_UPDATE;
			if (found)
				break;
		}
	}

	if (found || child_triggers(relation->rd_id, TRIGGER_TYPE_UPDATE))
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_YET),
				 errmsg("UPDATE on distributed key column not allowed on relation with update triggers")));
}

static TargetEntry *
find_junk_tle(List *targetList, const char *junkAttrName)
{
	ListCell	*lct;

	foreach(lct, targetList)
	{
		TargetEntry	*tle = (TargetEntry*) lfirst(lct);

		if (!tle->resjunk)
			continue;

		if (strcmp(tle->resname, junkAttrName) == 0)
			return tle;
	}
	elog(ERROR, "could not find junk tle \"%s\"", junkAttrName);
}

static AttrNumber
find_ctid_attribute_check(List *targetList)
{
	TargetEntry	*ctid;
	Var			*var;

	ctid = find_junk_tle(targetList, "ctid");
	if (!ctid)
		elog(ERROR, "could not find \"ctid\" column in input to UPDATE");
	Assert(IsA(ctid->expr, Var));

	var = (Var *) (ctid->expr);

	/* Ctid should follow after normal attributes */
	Assert(var->vartype == TIDOID);

	return ctid->resno;
}

static AttrNumber
find_oid_attribute_check(List *targetList)
{
	TargetEntry	*tle;
	Var			*var;

	tle = find_junk_tle(targetList, "oid");
	if (!tle)
		elog(ERROR, "could not find \"oid\" column in input to UPDATE");
	Assert(IsA(tle->expr, Var));

	var = (Var *) (tle->expr);

	/* OID should follow after normal attributes */
	Assert(var->vartype == OIDOID);

	return tle->resno;
}

/*
 * Copy all junk attributes into dest
 */
static void
copy_junk_attributes(List *src, List **dest, AttrNumber startAttrIdx)
{
	ListCell	*currAppendCell;
	ListCell	*lct;
	Var			*var;
	TargetEntry	*newTargetEntry;

	/* There should be at least ctid exist */
	Assert(startAttrIdx < list_length(src));

	currAppendCell = list_nth_cell(src, startAttrIdx);

	for_each_cell(lct, currAppendCell)
	{
		Assert(IsA(lfirst(lct), TargetEntry) && IsA(((TargetEntry *) lfirst(lct))->expr, Var));

		var = copyObject(((TargetEntry *) lfirst(lct))->expr);
		var->varno = OUTER_VAR;
		var->varattno = ((TargetEntry *) lfirst(lct))->resno;

		newTargetEntry = makeTargetEntry((Expr *) var, startAttrIdx + 1, ((TargetEntry *) lfirst(lct))->resname,
										 true);
		*dest = lappend(*dest, newTargetEntry);
		++startAttrIdx;
	}
}

/*
 * Find attributes in the targetlist of top plan.
 *
 * We generates informations as following:
 *
 * splitUpdateTargetList which should be a simple var list used by SplitUpdate
 * insertColIdx which point to resno of corresponding attributes in targetlist
 * deleteColIdx which contains placeholder, and value will be corrected later
 */
static void
process_targetlist_for_splitupdate(Relation resultRel, List *targetlist,
								   List **splitUpdateTargetList, List **insertColIdx, List **deleteColIdx)
{
	TupleDesc	resultDesc = RelationGetDescr(resultRel);
	GpPolicy   *cdbpolicy = resultRel->rd_cdbpolicy;
	int			attrIdx;
	Var		   *splitVar;
	TargetEntry	*splitTargetEntry;
	ListCell   *lc;

	lc = list_head(targetlist);
	for (attrIdx = 1; attrIdx <= resultDesc->natts; ++attrIdx)
	{
		TargetEntry			*tle;
		Form_pg_attribute	attr;
		int			oldAttrIdx = -1;

		tle = (TargetEntry *) lfirst(lc);
		lc = lnext(lc);
		Assert(tle);

		attr = resultDesc->attrs[attrIdx - 1];
		if (attr->attisdropped)
		{
			Assert(IsA(tle->expr, Const) && ((Const *) tle->expr)->constisnull);

			splitTargetEntry = makeTargetEntry((Expr *) copyObject(tle->expr), tle->resno, tle->resname, tle->resjunk);
			*splitUpdateTargetList = lappend(*splitUpdateTargetList, splitTargetEntry);
		}
		else
		{
			int			i;

			Assert(exprType((Node *) tle->expr) == attr->atttypid);

			splitVar = (Var *) makeVar(OUTER_VAR,
									   attrIdx,
									   attr->atttypid,
									   attr->atttypmod,
									   attr->attcollation,
									   0);
			splitVar->varnoold = attrIdx;

			splitTargetEntry = makeTargetEntry((Expr *) splitVar, tle->resno, tle->resname, tle->resjunk);
			*splitUpdateTargetList = lappend(*splitUpdateTargetList, splitTargetEntry);

			/*
			 * Is this a distribution key column? If so, we will need its old value.
			 * expand_targetlist() put the old values for distribution key columns in
			 * the target list after the new column values.
			 */
			for (i = 0; i < cdbpolicy->nattrs; i++)
			{
				AttrNumber	keyattno = cdbpolicy->attrs[i];

				if (keyattno == attrIdx)
				{
					TargetEntry *oldtle;

					oldAttrIdx = resultDesc->natts + i + 1;

					/* sanity checks. */
					if (oldAttrIdx > list_length(targetlist))
						elog(ERROR, "old value for attribute \"%s\" missing from split update input target list",
							 NameStr(attr->attname));
					oldtle = list_nth(targetlist, oldAttrIdx - 1);
					if (exprType((Node *) oldtle->expr) != attr->atttypid)
						elog(ERROR, "datatype mismatch for old value for attribute \"%s\" in split update input target list",
							 NameStr(attr->attname));

					break;
				}
			}
			/*
			 * If oldAttrIdx is still -1, this is not a distribution key column.
			 */
		}

		*insertColIdx = lappend_int(*insertColIdx, attrIdx);
		*deleteColIdx = lappend_int(*deleteColIdx, oldAttrIdx);
	}
}

/*
 * In Postgres planner, we add a SplitUpdate node at top so that updating on distribution
 * columns could be handled. The SplitUpdate will split each update into delete + insert.
 *
 * There are several important points should be highlighted:
 *
 * First, in order to split each update operation into two operations: delete + insert,
 * we add several columns into targetlist:
 *
 * ctid: the tuple id used for deletion
 * action: which is generated by SplitUpdate node, and can be value of delete or insert
 * oid: if result relation has oids, we need to add oid to targetlist
 *
 * Second, current GPDB executor don't support statement-level update triggers and will
 * skip row-level update triggers because a split-update is actually consist of a delete
 * and insert. So, if the result relation has update triggers, we should reject and error
 * out because it's not functional.
 *
 * Third, to support deletion, and hash delete operation to correct segment,
 * we need to get attributes of OLD tuple. The old attributes must therefore
 * be present in the subplan's target list. That is handled earlier in the
 * planner, in expand_targetlist().
 *
 * For example, a typical plan would be as following for statement:
 * update foo set id = l.v + 1 from dep l where foo.v = l.id:
 *
 * |-- join ( targetlist: [ l.v + 1, foo.v, foo.id, foo.ctid, foo.gp_segment_id ] )
 *       |
 *       |-- motion ( targetlist: [l.id, l.v] )
 *       |    |
 *       |    |-- seqscan on dep ....
 *       |
 *       |-- hash (targetlist [ v, foo.ctid, foo.gp_segment_id ] )
 *            |
 *            |-- seqscan on foo (targetlist: [ v, foo.id, foo.ctid, foo.gp_segment_id ] )
 *
 * From the plan above, the target foo.id is assigned as l.v + 1, and expand_targetlist()
 * ensured that the old value of id, is also available, even though it would not otherwise
 * be needed.
 *
 * 'result_relation' is the RTI of the UPDATE target relation.
 */
SplitUpdate *
make_splitupdate(PlannerInfo *root, ModifyTable *mt, Plan *subplan, RangeTblEntry *rte)
{
	AttrNumber		ctidColIdx = 0;
	List			*deleteColIdx = NIL;
	List			*insertColIdx = NIL;
	int				actionColIdx;
	AttrNumber		oidColIdx = 0;
	List			*splitUpdateTargetList = NIL;
	TargetEntry		*newTargetEntry;
	SplitUpdate		*splitupdate;
	DMLActionExpr	*actionExpr;
	Relation		resultRelation;
	TupleDesc		resultDesc;

	Assert(IsA(mt, ModifyTable));

	/* Suppose we already hold locks before caller */
	resultRelation = relation_open(rte->relid, NoLock);

	failIfUpdateTriggers(resultRelation);

	resultDesc = RelationGetDescr(resultRelation);

	/*
	 * insertColIdx/deleteColIdx: In split update mode, we have to form two tuples,
	 * one is for deleting and the other is for inserting. Find the old and new
	 * values in the subplan's target list, and store their column indexes in
	 * insertColIdx and deleteColIdx.
	 */
	process_targetlist_for_splitupdate(resultRelation,
									   subplan->targetlist,
									   &splitUpdateTargetList, &insertColIdx, &deleteColIdx);
	ctidColIdx = find_ctid_attribute_check(subplan->targetlist);

	if (resultRelation->rd_rel->relhasoids)
		oidColIdx = find_oid_attribute_check(subplan->targetlist);

	copy_junk_attributes(subplan->targetlist, &splitUpdateTargetList, resultDesc->natts);

	relation_close(resultRelation, NoLock);

	/* finally, we should add action column at the end of targetlist */
	actionExpr = makeNode(DMLActionExpr);
	actionColIdx = list_length(splitUpdateTargetList) + 1;
	newTargetEntry = makeTargetEntry((Expr *) actionExpr, actionColIdx, "ColRef", true);
	splitUpdateTargetList = lappend(splitUpdateTargetList, newTargetEntry);

	splitupdate = makeNode(SplitUpdate);
	splitupdate->actionColIdx = actionColIdx;

	Assert(ctidColIdx > 0);

	/* populate information generated above into splitupdate node */
	splitupdate->ctidColIdx = ctidColIdx;
	splitupdate->tupleoidColIdx = oidColIdx;
	splitupdate->insertColIdx = insertColIdx;
	splitupdate->deleteColIdx = deleteColIdx;
	splitupdate->plan.targetlist = splitUpdateTargetList;
	splitupdate->plan.lefttree = subplan;

	/*
	 * Now the plan tree has been determined, we have no choice, so use the
	 * cost of lower plan node directly, plus the cpu_tuple_cost of each row.
	 * GPDB_96_MERGE_FIXME: width here is incorrect, until we merge
	 * upstream commit 3fc6e2d
	 */
	splitupdate->plan.startup_cost = subplan->startup_cost;
	splitupdate->plan.total_cost = subplan->total_cost;
	splitupdate->plan.plan_rows = 2 * subplan->plan_rows;
	splitupdate->plan.total_cost += (splitupdate->plan.plan_rows * cpu_tuple_cost);
	splitupdate->plan.plan_width = subplan->plan_width;

	/*
	 * A redistributed-motion has to be added above	the split node in
	 * the plan and this can be achieved by marking the split node strewn.
	 * However, if the subplan is an entry, we should not mark it strewn.
	 */
	if (subplan->flow->locustype != CdbLocusType_Entry)
		mark_plan_strewn((Plan *) splitupdate, subplan->flow->numsegments);
	else
		mark_plan_entry((Plan *) splitupdate);

	mt->action_col_idxes = lappend_int(mt->action_col_idxes, actionColIdx);
	mt->ctid_col_idxes = lappend_int(mt->ctid_col_idxes, ctidColIdx);
	mt->oid_col_idxes = lappend_int(mt->oid_col_idxes, oidColIdx);

	return splitupdate;
}

/* ----------------------------------------------------------------------- *
 * cdbmutate_warn_ctid_without_segid() warns the user if the plan refers to a
 * partitioned table's ctid column without also referencing its
 * gp_segment_id column.
 * ----------------------------------------------------------------------- *
 */
typedef struct ctid_inventory_context
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	bool		uses_ctid;
	bool		uses_segid;
	Index		relid;
} ctid_inventory_context;

static bool
ctid_inventory_walker(Node *node, ctid_inventory_context *inv)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varattno < 0 &&
			var->varno == inv->relid &&
			var->varlevelsup == 0)
		{
			if (var->varattno == SelfItemPointerAttributeNumber)
				inv->uses_ctid = true;
			else if (var->varattno == GpSegmentIdAttributeNumber)
				inv->uses_segid = true;
		}
		return false;
	}
	return plan_tree_walker(node, ctid_inventory_walker, inv);
}

void
cdbmutate_warn_ctid_without_segid(struct PlannerInfo *root, struct RelOptInfo *rel)
{
	ctid_inventory_context inv;
	Relids		relids_to_ignore;
	ListCell   *cell;
	AttrNumber	attno;
	int			ndx;

	planner_init_plan_tree_base(&inv.base, root);
	inv.uses_ctid = false;
	inv.uses_segid = false;
	inv.relid = rel->relid;

	/* Rel not distributed?  Then segment id doesn't matter. */
	if (!rel->cdbpolicy ||
		rel->cdbpolicy->ptype == POLICYTYPE_ENTRY)
		return;

	/* Ignore references occurring in the Query's final output targetlist. */
	relids_to_ignore = bms_make_singleton(0);

	/* Is 'ctid' referenced in join quals? */
	attno = SelfItemPointerAttributeNumber;
	Assert(attno >= rel->min_attr && attno <= rel->max_attr);
	ndx = attno - rel->min_attr;
	if (bms_nonempty_difference(rel->attr_needed[ndx], relids_to_ignore))
		inv.uses_ctid = true;

	/* Is 'gp_segment_id' referenced in join quals? */
	attno = GpSegmentIdAttributeNumber;
	Assert(attno >= rel->min_attr && attno <= rel->max_attr);
	ndx = attno - rel->min_attr;
	if (bms_nonempty_difference(rel->attr_needed[ndx], relids_to_ignore))
		inv.uses_segid = true;

	/* Examine the single-table quals on this rel. */
	foreach(cell, rel->baserestrictinfo)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(cell);

		Assert(IsA(rinfo, RestrictInfo));

		ctid_inventory_walker((Node *) rinfo->clause, &inv);
	}

	/* Notify client if found a reference to ctid only, without gp_segment_id */
	if (inv.uses_ctid &&
		!inv.uses_segid)
	{
		RangeTblEntry *rte = rt_fetch(rel->relid, root->parse->rtable);
		const char *cmd;
		int			elevel;

		/* Reject if UPDATE or DELETE.  Otherwise just give info msg. */
		switch (root->parse->commandType)
		{
			case CMD_UPDATE:
				cmd = "UPDATE";
				elevel = ERROR;
				break;

			case CMD_DELETE:
				cmd = "DELETE";
				elevel = ERROR;
				break;

			default:
				cmd = "SELECT";
				elevel = NOTICE;
		}

		ereport(elevel,
				(errmsg("%s uses system-defined column \"%s.ctid\" without the necessary companion column \"%s.gp_segment_id\"",
						cmd, rte->eref->aliasname, rte->eref->aliasname),
				 errhint("To uniquely identify a row within a distributed table, use the \"gp_segment_id\" column together with the \"ctid\" column.")));
	}
	bms_free(relids_to_ignore);
}								/* cdbmutate_warn_ctid_without_segid */


/*
 * Code that mutate the tree for share input
 *
 * After the planner, the plan is really a DAG.  SISCs will have valid
 * pointer to the underlying share.  However, other code (setrefs etc)
 * depends on the fact that the plan is a tree.  We first mutate the DAG
 * to a tree.
 *
 * Next, we will need to decide if the share is cross slices.  If the share
 * is not cross slice, we do not need the syncrhonization, and it is possible to
 * keep the Material/Sort in memory to save a sort.
 *
 * It is essential that we walk the tree in the same order as the ExecProcNode start
 * execution, otherwise, deadlock may rise.
 */

/* Walk the tree for shareinput.
 * Shareinput fix shared_as_id and underlying_share_id of nodes in place.  We do not want to use
 * the ordinary tree walker as it is unnecessary to make copies etc.
 */
typedef bool (*SHAREINPUT_MUTATOR) (Node *node, PlannerInfo *root, bool fPop);
static void
shareinput_walker(SHAREINPUT_MUTATOR f, Node *node, PlannerInfo *root)
{
	Plan	   *plan = NULL;
	bool		recursive_down;

	if (node == NULL)
		return;

	if (IsA(node, List))
	{
		List	   *l = (List *) node;
		ListCell   *lc;

		foreach(lc, l)
		{
			Node	   *n = lfirst(lc);

			shareinput_walker(f, n, root);
		}
		return;
	}

	if (!is_plan_node(node))
		return;

	plan = (Plan *) node;
	recursive_down = (*f) (node, root, false);

	if (recursive_down)
	{
		if (IsA(node, Append))
		{
			ListCell   *cell;
			Append	   *app = (Append *) node;

			foreach(cell, app->appendplans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, ModifyTable))
		{
			ListCell   *cell;
			ModifyTable *mt = (ModifyTable *) node;

			foreach(cell, mt->plans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, SubqueryScan))
		{
			SubqueryScan  *subqscan = (SubqueryScan *) node;
			PlannerGlobal *glob = root->glob;
			PlannerInfo   *subroot;
			List	      *save_rtable;
			RelOptInfo    *rel;

			/*
			 * If glob->finalrtable is not NULL, rtables have been flatten,
			 * thus we should use glob->finalrtable instead.
			 */
			save_rtable = glob->share.curr_rtable;
			if (root->glob->finalrtable == NULL)
			{
				rel = find_base_rel(root, subqscan->scan.scanrelid);
				/*
				 * The Assert() on RelOptInfo's subplan being
				 * same as the subqueryscan's subplan, is valid
				 * in Upstream but for not for GPDB, since we
				 * create a new copy of the subplan if two
				 * SubPlans refer to the same initplan.
				 */
				subroot = rel->subroot;
				glob->share.curr_rtable = subroot->parse->rtable;
			}
			else
			{
				subroot = root;
				glob->share.curr_rtable = glob->finalrtable;
			}
			shareinput_walker(f, (Node *) subqscan->subplan, subroot);
			glob->share.curr_rtable = save_rtable;
		}
		else if (IsA(node, TableFunctionScan))
		{
			TableFunctionScan  *tfscan = (TableFunctionScan *) node;
			PlannerGlobal *glob = root->glob;
			PlannerInfo   *subroot;
			List	      *save_rtable;
			RelOptInfo    *rel;

			/*
			 * If glob->finalrtable is not NULL, rtables have been flatten,
			 * thus we should use glob->finalrtable instead.
			 */
			save_rtable = glob->share.curr_rtable;
			if (root->glob->finalrtable == NULL)
			{
				rel = find_base_rel(root, tfscan->scan.scanrelid);
				Assert(rel->subplan == tfscan->scan.plan.lefttree);
				subroot = rel->subroot;
				glob->share.curr_rtable = subroot->parse->rtable;
			}
			else
			{
				subroot = root;
				glob->share.curr_rtable = glob->finalrtable;
			}
			shareinput_walker(f, (Node *)  tfscan->scan.plan.lefttree, subroot);
			glob->share.curr_rtable = save_rtable;
		}
		else if (IsA(node, BitmapAnd))
		{
			ListCell   *cell;
			BitmapAnd  *ba = (BitmapAnd *) node;

			foreach(cell, ba->bitmapplans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, BitmapOr))
		{
			ListCell   *cell;
			BitmapOr   *bo = (BitmapOr *) node;

			foreach(cell, bo->bitmapplans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, NestLoop))
		{
			/*
			 * Nest loop join is strange.  Exec order depends on
			 * prefetch_inner
			 */
			NestLoop   *nl = (NestLoop *) node;

			if (nl->join.prefetch_inner)
			{
				shareinput_walker(f, (Node *) plan->righttree, root);
				shareinput_walker(f, (Node *) plan->lefttree, root);
			}
			else
			{
				shareinput_walker(f, (Node *) plan->lefttree, root);
				shareinput_walker(f, (Node *) plan->righttree, root);
			}
		}
		else if (IsA(node, HashJoin))
		{
			/* Hash join the hash table is at inner */
			shareinput_walker(f, (Node *) plan->righttree, root);
			shareinput_walker(f, (Node *) plan->lefttree, root);
		}
		else if (IsA(node, MergeJoin))
		{
			MergeJoin  *mj = (MergeJoin *) node;

			if (mj->unique_outer)
			{
				shareinput_walker(f, (Node *) plan->lefttree, root);
				shareinput_walker(f, (Node *) plan->righttree, root);
			}
			else
			{
				shareinput_walker(f, (Node *) plan->righttree, root);
				shareinput_walker(f, (Node *) plan->lefttree, root);
			}
		}
		else if (IsA(node, Sequence))
		{
			ListCell   *cell = NULL;
			Sequence   *sequence = (Sequence *) node;

			foreach(cell, sequence->subplans)
			{
				shareinput_walker(f, (Node *) lfirst(cell), root);
			}
		}
		else
		{
			shareinput_walker(f, (Node *) plan->lefttree, root);
			shareinput_walker(f, (Node *) plan->righttree, root);
			shareinput_walker(f, (Node *) plan->initPlan, root);
		}
	}

	(*f) (node, root, true);
}

typedef struct
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	int			nextPlanId;
} assign_plannode_id_context;

static bool
assign_plannode_id_walker(Node *node, assign_plannode_id_context *ctxt)
{
	if (node == NULL)
		return false;

	if (is_plan_node(node))
		((Plan *) node)->plan_node_id = ++ctxt->nextPlanId;

	if (IsA(node, SubPlan))
		return false;

	return plan_tree_walker(node, assign_plannode_id_walker, ctxt);
}

/*
 * Create a fake CTE range table entry that reflects the target list of a
 * shared input.
 */
static RangeTblEntry *
create_shareinput_producer_rte(ApplyShareInputContext *ctxt, int share_id,
							   int refno)
{
	int			attno = 1;
	ListCell   *lc;
	Plan	   *subplan;
	char		buf[100];
	RangeTblEntry *rte;
	List	   *colnames = NIL;
	List	   *coltypes = NIL;
	List	   *coltypmods = NIL;
	List	   *colcollations = NIL;
	ShareInputScan *producer;

	Assert(ctxt->producer_count > share_id);
	producer = ctxt->producers[share_id];
	subplan = producer->scan.plan.lefttree;

	foreach(lc, subplan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		Oid			vartype;
		int32		vartypmod;
		Oid			varcollid;
		char	   *resname;

		vartype = exprType((Node *) tle->expr);
		vartypmod = exprTypmod((Node *) tle->expr);
		varcollid = exprCollation((Node *) tle->expr);

		/*
		 * We should've filled in tle->resname in shareinput_save_producer().
		 * Note that it's too late to call get_tle_name() here, because this
		 * runs after all the varnos in Vars have already been changed to
		 * INNER_VAR/OUTER_VAR.
		 */
		resname = tle->resname;
		if (!resname)
			resname = pstrdup("unnamed_attr");

		colnames = lappend(colnames, makeString(resname));
		coltypes = lappend_oid(coltypes, vartype);
		coltypmods = lappend_int(coltypmods, vartypmod);
		colcollations = lappend_oid(colcollations, varcollid);
		attno++;
	}

	/*
	 * Create a new RTE. Note that we use a different RTE for each reference,
	 * because we want to give each reference a different name.
	 */
	snprintf(buf, sizeof(buf), "share%d_ref%d", share_id, refno);

	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_CTE;
	rte->ctename = pstrdup(buf);
	rte->ctelevelsup = 0;
	rte->self_reference = false;
	rte->alias = NULL;

	rte->eref = makeAlias(rte->ctename, colnames);
	rte->ctecoltypes = coltypes;
	rte->ctecoltypmods = coltypmods;
	rte->ctecolcollations = colcollations;

	rte->inh = false;
	rte->inFromCl = false;

	rte->requiredPerms = 0;
	rte->checkAsUser = InvalidOid;

	return rte;
}

/*
 * Memorize the producer of a shared input in an array of producers, one
 * producer per share_id.
 */
static void
shareinput_save_producer(ShareInputScan *plan, ApplyShareInputContext *ctxt)
{
	int			share_id = plan->share_id;
	int			new_producer_count = (share_id + 1);

	Assert(plan->share_id >= 0);

	if (ctxt->producers == NULL)
	{
		ctxt->producers = palloc0(sizeof(ShareInputScan *) * new_producer_count);
		ctxt->producer_count = new_producer_count;
	}
	else if (ctxt->producer_count < new_producer_count)
	{
		ctxt->producers = repalloc(ctxt->producers, new_producer_count * sizeof(ShareInputScan *));
		memset(&ctxt->producers[ctxt->producer_count], 0, (new_producer_count - ctxt->producer_count) * sizeof(ShareInputScan *));
		ctxt->producer_count = new_producer_count;
	}

	Assert(ctxt->producers[share_id] == NULL);
	ctxt->producers[share_id] = plan;
}

/*
 * When a plan comes out of the planner, all the ShareInputScan nodes belonging
 * to the same "share" have the same child node. apply_shareinput_dag_to_tree()
 * turns the DAG into a proper tree. The first occurrence of a ShareInput scan,
 * with a particular child tree, becomes the "producer" of the share, and the
 * others becomes consumers. The subtree is removed from all the consumer nodes.
 *
 * Also, a share_id is assigned to each ShareInputScan node, as well as the
 * Material/Sort nodes below the producers. The producers and its consumers
 * are linked together by the same share_id.
 */
static bool
shareinput_mutator_dag_to_tree(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
		return true;

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *siscan = (ShareInputScan *) plan;
		Plan	   *subplan = plan->lefttree;
		int			i;
		int			attno;
		ListCell   *lc;

		/* Is there a producer for this sub-tree already? */
		for (i = 0; i < ctxt->producer_count; i++)
		{
			if (ctxt->producers[i] && ctxt->producers[i]->scan.plan.lefttree == subplan)
			{
				/*
				 * Yes. This is a consumer. Remove the subtree, and assign the
				 * same share_id as the producer.
				 */
				Assert(get_plan_share_id((Plan *) ctxt->producers[i]) == i);
				set_plan_share_id((Plan *) plan, ctxt->producers[i]->share_id);
				siscan->scan.plan.lefttree = NULL;
				return false;
			}
		}

		/*
		 * Couldn't find a match in existing list of producers, so this is a
		 * producer. Add this to the list of producers, and assign a new
		 * share_id.
		 */
		Assert(get_plan_share_id(subplan) == SHARE_ID_NOT_ASSIGNED);
		set_plan_share_id((Plan *) plan, ctxt->producer_count);
		set_plan_share_id(subplan, ctxt->producer_count);

		shareinput_save_producer(siscan, ctxt);

		/*
		 * Also make sure that all the entries in the subplan's target list
		 * have human-readable column names. They are used for EXPLAIN.
		 */
		attno = 1;
		foreach(lc, subplan->targetlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);

			if (tle->resname == NULL)
			{
				char		default_name[100];
				char	   *resname;

				snprintf(default_name, sizeof(default_name), "col_%d", attno);

				resname = strVal(get_tle_name(tle, ctxt->curr_rtable, default_name));
				tle->resname = pstrdup(resname);
			}
			attno++;
		}
	}

	return true;
}

Plan *
apply_shareinput_dag_to_tree(PlannerInfo *root, Plan *plan)
{
	PlannerGlobal *glob = root->glob;

	glob->share.curr_rtable = root->parse->rtable;
	shareinput_walker(shareinput_mutator_dag_to_tree, (Node *) plan, root);
	return plan;
}

/*
 * Collect all the producer ShareInput nodes into an array, for later use by
 * replace_shareinput_targetlists().
 *
 * This is a stripped-down version of apply_shareinput_dag_to_tree(), for use
 * on ORCA-produced plans. ORCA assigns share_ids to all ShareInputScan nodes,
 * and only producer nodes have a subtree, so we don't need to do the DAG to
 * tree conversion or assign share_ids here.
 */
static bool
collect_shareinput_producers_walker(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;

	if (fPop)
		return true;

	if (IsA(node, ShareInputScan))
	{
		ShareInputScan *siscan = (ShareInputScan *) node;
		Plan	   *subplan = siscan->scan.plan.lefttree;

		Assert(get_plan_share_id((Plan *) siscan) >= 0);

		if (subplan)
		{
			shareinput_save_producer(siscan, ctxt);
		}
	}
	return true;
}

void
collect_shareinput_producers(PlannerInfo *root, Plan *plan)
{
	PlannerGlobal *glob = root->glob;

	glob->share.curr_rtable = glob->finalrtable;
	shareinput_walker(collect_shareinput_producers_walker, (Node *) plan, root);
}

/* Some helper: implements a stack using List. */
static void
shareinput_pushmot(ApplyShareInputContext *ctxt, int motid)
{
	ctxt->motStack = lcons_int(motid, ctxt->motStack);
}
static void
shareinput_popmot(ApplyShareInputContext *ctxt)
{
	list_delete_first(ctxt->motStack);
}
static int
shareinput_peekmot(ApplyShareInputContext *ctxt)
{
	return linitial_int(ctxt->motStack);
}


/*
 * Replace the target list of ShareInputScan nodes, with references
 * to CTEs that we build on the fly.
 *
 * Only one of the ShareInputScan nodes in a plan tree contains the real
 * child plan, while others contain just a "share id" that binds all the
 * ShareInputScan nodes sharing the same input together. The missing
 * child plan is a problem for EXPLAIN, as any OUTER Vars in the
 * ShareInputScan's target list cannot be resolved without the child
 * plan.
 *
 * To work around that issue, create a CTE for each shared input node, with
 * columns that match the target list of the SharedInputScan's subplan,
 * and replace the target list entries of the SharedInputScan with
 * Vars that point to the CTE instead of the child plan.
 */
Plan *
replace_shareinput_targetlists(PlannerInfo *root, Plan *plan)
{
	shareinput_walker(replace_shareinput_targetlists_walker, (Node *) plan, root);
	return plan;
}

static bool
replace_shareinput_targetlists_walker(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;

	if (fPop)
		return true;

	if (IsA(node, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) node;
		int			share_id = sisc->share_id;
		ListCell   *lc;
		int			attno;
		List	   *newtargetlist;
		RangeTblEntry *rte;

		/*
		 * Note that even though the planner assigns sequential share_ids for
		 * each shared node, so that share_id is always below
		 * list_length(ctxt->sharedNodes), ORCA has a different assignment
		 * scheme. So we have to be prepared for any share_id, at least when
		 * ORCA is in use.
		 */
		if (ctxt->share_refcounts == NULL)
		{
			int			new_sz = share_id + 1;

			ctxt->share_refcounts = palloc0(new_sz * sizeof(int));
			ctxt->share_refcounts_sz = new_sz;
		}
		else if (share_id >= ctxt->share_refcounts_sz)
		{
			int			old_sz = ctxt->share_refcounts_sz;
			int			new_sz = share_id + 1;

			ctxt->share_refcounts = repalloc(ctxt->share_refcounts, new_sz * sizeof(int));
			memset(&ctxt->share_refcounts[old_sz], 0, (new_sz - old_sz) * sizeof(int));
			ctxt->share_refcounts_sz = new_sz;
		}

		ctxt->share_refcounts[share_id]++;

		/*
		 * Create a new RTE. Note that we use a different RTE for each
		 * reference, because we want to give each reference a different name.
		 */
		rte = create_shareinput_producer_rte(ctxt, share_id,
											 ctxt->share_refcounts[share_id]);

		glob->finalrtable = lappend(glob->finalrtable, rte);
		sisc->scan.scanrelid = list_length(glob->finalrtable);

		/*
		 * Replace all the target list entries.
		 *
		 * SharedInputScan nodes are not projection-capable, so the target
		 * list of the SharedInputScan matches the subplan's target list.
		 */
		newtargetlist = NIL;
		attno = 1;
		foreach(lc, sisc->scan.plan.targetlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);
			TargetEntry *newtle = flatCopyTargetEntry(tle);

			newtle->expr = (Expr *) makeVar(sisc->scan.scanrelid, attno,
											exprType((Node *) tle->expr),
											exprTypmod((Node *) tle->expr),
											exprCollation((Node *) tle->expr),
											0);
			newtargetlist = lappend(newtargetlist, newtle);
			attno++;
		}
		sisc->scan.plan.targetlist = newtargetlist;
	}

	return true;
}


typedef struct ShareNodeWithSliceMark
{
	Plan	   *plan;
	int			slice_mark;
} ShareNodeWithSliceMark;

static bool
shareinput_find_sharenode(ApplyShareInputContext *ctxt, int share_id, ShareNodeWithSliceMark *result)
{
	ShareInputScan *siscan;
	Plan	   *plan;

	Assert(share_id < ctxt->producer_count);
	if (share_id >= ctxt->producer_count)
		return false;

	siscan = ctxt->producers[share_id];
	if (!siscan)
		return false;

	plan = siscan->scan.plan.lefttree;

	Assert(get_plan_share_id(plan) == share_id);
	Assert(IsA(plan, Material) ||IsA(plan, Sort));

	if (result)
	{
		result->plan = plan;
		result->slice_mark = ctxt->sliceMarks[share_id];
	}

	return true;
}

/*
 * First walk on shareinput xslice.  It does the following:
 *
 * 1. Build the sliceMarks in context.
 * 2. Build a list a share on QD
 */
static bool
shareinput_mutator_xslice_1(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
	{
		if (IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int			motId = shareinput_peekmot(ctxt);
		Plan	   *shared = plan->lefttree;

		Assert(sisc->scan.plan.flow);
		if (sisc->scan.plan.flow->flotype == FLOW_SINGLETON)
		{
			if (sisc->scan.plan.flow->segindex < 0)
				ctxt->qdShares = list_append_unique_int(ctxt->qdShares, sisc->share_id);
		}

		if (shared)
		{
			Assert(get_plan_share_id(plan) == get_plan_share_id(shared));
			set_plan_driver_slice(shared, motId);

			/*
			 * We need to repopulate the producers array. cdbparallelize() was
			 * run on the plan tree between shareinput_mutator_dag_to_tree()
			 * and here, which copies all the nodes, and the destroys the
			 * producers array in the process.
			 */
			ctxt->producers[sisc->share_id] = sisc;
			ctxt->sliceMarks[sisc->share_id] = motId;
		}
	}

	return true;
}

/*
 * Second pass on shareinput xslice.  It marks the shared node xslice,
 * if a 'shared' is cross-slice.
 */
static bool
shareinput_mutator_xslice_2(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
	{
		if (IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int			motId = shareinput_peekmot(ctxt);
		Plan	   *shared = plan->lefttree;

		if (!shared)
		{
			ShareNodeWithSliceMark plan_slicemark = {NULL /* plan */ , 0 /* slice_mark */ };
			int			shareSliceId = 0;

			shareinput_find_sharenode(ctxt, sisc->share_id, &plan_slicemark);
			if (plan_slicemark.plan == NULL)
				elog(ERROR, "could not find shared input node with id %d", sisc->share_id);

			shareSliceId = get_plan_driver_slice(plan_slicemark.plan);

			if (shareSliceId != motId)
			{
				ShareType	stype = get_plan_share_type(plan_slicemark.plan);

				if (stype == SHARE_MATERIAL || stype == SHARE_SORT)
					set_plan_share_type_xslice(plan_slicemark.plan);

				incr_plan_nsharer_xslice(plan_slicemark.plan);
				sisc->driver_slice = motId;
			}
		}
	}

	return true;
}

/*
 * Third pass:
 * 	1. Mark shareinput scan xslice,
 * 	2. Bulid a list of QD slices
 */
static bool
shareinput_mutator_xslice_3(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
	{
		if (IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int			motId = shareinput_peekmot(ctxt);

		ShareNodeWithSliceMark plan_slicemark = {NULL, 0};
		ShareType	stype = SHARE_NOTSHARED;

		shareinput_find_sharenode(ctxt, sisc->share_id, &plan_slicemark);

		if (!plan_slicemark.plan)
			elog(ERROR, "sub-plan for share_id %d cannot be NULL", sisc->share_id);

		stype = get_plan_share_type(plan_slicemark.plan);

		switch (stype)
		{
			case SHARE_MATERIAL_XSLICE:
				Assert(sisc->share_type == SHARE_MATERIAL);
				set_plan_share_type_xslice(plan);
				break;
			case SHARE_SORT_XSLICE:
				Assert(sisc->share_type == SHARE_SORT);
				set_plan_share_type_xslice(plan);
				break;
			default:
				Assert(sisc->share_type == stype);
				break;
		}

		if (list_member_int(ctxt->qdShares, sisc->share_id))
		{
			Assert(sisc->scan.plan.flow);
			Assert(sisc->scan.plan.flow->flotype == FLOW_SINGLETON);
			ctxt->qdSlices = list_append_unique_int(ctxt->qdSlices, motId);
		}
	}
	return true;
}

/*
 * The fourth pass.  If a shareinput is running on QD, then all slices in
 * this share must be on QD.  Move them to QD.
 */
static bool
shareinput_mutator_xslice_4(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;
	int			motId = shareinput_peekmot(ctxt);

	if (fPop)
	{
		if (IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		shareinput_pushmot(ctxt, motion->motionID);
		/* Do not return.  Motion need to be adjusted as well */
	}

	/*
	 * Well, the following test can be optimized if we record the test result
	 * so we test just once for all node in one slice.  But this code is not
	 * perf critical so be lazy.
	 */
	if (list_member_int(ctxt->qdSlices, motId))
	{
		if (plan->flow)
		{
			Assert(plan->flow->flotype == FLOW_SINGLETON);
			plan->flow->segindex = -1;
		}
	}
	return true;
}

Plan *
apply_shareinput_xslice(Plan *plan, PlannerInfo *root)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	ListCell   *lp, *lr;

	ctxt->motStack = NULL;
	ctxt->qdShares = NULL;
	ctxt->qdSlices = NULL;
	ctxt->nextPlanId = 0;

	ctxt->sliceMarks = palloc0(ctxt->producer_count * sizeof(int));

	shareinput_pushmot(ctxt, 0);

	/*
	 * Walk the tree.  See comment for each pass for what each pass will do.
	 * The context is used to carry information from one pass to another, as
	 * well as within a pass.
	 */

	/*
	 * A subplan might have a SharedScan consumer while the SharedScan
	 * producer is in the main plan, or vice versa. So in the first pass, we
	 * walk through all plans and collect all producer subplans into the
	 * context, before processing the consumers.
	 */
	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot =  (PlannerInfo *) lfirst(lr);

		shareinput_walker(shareinput_mutator_xslice_1, (Node *) subplan, subroot);
	}
	shareinput_walker(shareinput_mutator_xslice_1, (Node *) plan, root);

	/* Now walk the tree again, and process all the consumers. */
	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot =  (PlannerInfo *) lfirst(lr);

		shareinput_walker(shareinput_mutator_xslice_2, (Node *) subplan, subroot);
	}
	shareinput_walker(shareinput_mutator_xslice_2, (Node *) plan, root);

	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot =  (PlannerInfo *) lfirst(lr);

		shareinput_walker(shareinput_mutator_xslice_3, (Node *) subplan, subroot);
	}
	shareinput_walker(shareinput_mutator_xslice_3, (Node *) plan, root);

	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot =  (PlannerInfo *) lfirst(lr);

		shareinput_walker(shareinput_mutator_xslice_4, (Node *) subplan, subroot);
	}
	shareinput_walker(shareinput_mutator_xslice_4, (Node *) plan, root);

	return plan;
}

/*
 * assign_plannode_id - Assign an id for each plan node.
 * Used by gpmon and instrument.
 */
void
assign_plannode_id(PlannedStmt *stmt)
{
	assign_plannode_id_context ctxt;
	ListCell   *lc;

	ctxt.base.node = (Node *) stmt->planTree;
	ctxt.nextPlanId = 0;

	assign_plannode_id_walker((Node *) stmt->planTree, &ctxt);

	foreach(lc, stmt->subplans)
	{
		Plan	   *subplan = lfirst(lc);

		Assert(subplan);

		assign_plannode_id_walker((Node *) subplan, &ctxt);
	}
}

/*
 * Hash a list of const values with GPDB's hash function
 */
int32
cdbhash_const_list(List *plConsts, int iSegments, Oid *hashfuncs)
{
	ListCell   *lc;
	CdbHash    *pcdbhash;
	int			i;

	Assert(0 < list_length(plConsts));

	pcdbhash = makeCdbHash(iSegments, list_length(plConsts), hashfuncs);

	cdbhashinit(pcdbhash);

	Assert(0 < list_length(plConsts));

	i = 0;
	foreach(lc, plConsts)
	{
		Const	   *pconst = (Const *) lfirst(lc);

		cdbhash(pcdbhash, i + 1, pconst->constvalue, pconst->constisnull);
		i++;
	}

	return cdbhashreduce(pcdbhash);
}

/*
 * Construct an expression that checks whether the current segment is
 * 'segid'.
 */
Node *
makeSegmentFilterExpr(int segid)
{
	/* Build an expression: gp_execution_segment() = <segid> */
	return (Node *)
		make_opclause(Int4EqualOperator,
					  BOOLOID,
					  false,	/* opretset */
					  (Expr *) makeFuncExpr(F_MPP_EXECUTION_SEGMENT,
											INT4OID,
											NIL,	/* args */
											InvalidOid,
											InvalidOid,
											COERCE_EXPLICIT_CALL),
					  (Expr *) makeConst(INT4OID,
										 -1,		/* consttypmod */
										 InvalidOid, /* constcollid */
										 sizeof(int32),
										 Int32GetDatum(segid),
										 false,		/* constisnull */
										 true),		/* constbyval */
					  InvalidOid,	/* opcollid */
					  InvalidOid	/* inputcollid */
			);
}

typedef struct ParamWalkerContext
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	Bitmapset  *paramids;		/* Bitmapset for Param */
	Bitmapset  *scanrelids;		/* Bitmapset for scanrelid */
} ParamWalkerContext;

static bool
param_walker(Node *node, ParamWalkerContext *context)
{
	Param	   *param;
	Scan	   *scan;

	if (node == NULL)
		return false;

	switch (nodeTag(node))
	{
		case T_Param:
			param = (Param *) node;
			if (!bms_is_member(param->paramid, context->paramids))
				context->paramids = bms_add_member(context->paramids,
												   param->paramid);
			return false;

		case T_SubqueryScan:
		case T_ValuesScan:
		case T_FunctionScan:
		case T_TableFunctionScan:
			scan = (Scan *) node;
			if (!bms_is_member(scan->scanrelid, context->scanrelids))
				context->scanrelids = bms_add_member(context->scanrelids,
													 scan->scanrelid);
			break;

		default:
			break;
	}

	return plan_tree_walker(node, param_walker, context);
}

/*
 * Retrieve param ids that are referenced in RTEs.
 * We can't simply use range_table_walker() here, because we only
 * want to walk through RTEs that are referenced in the plan.
 */
static void
rte_param_walker(List *rtable, ParamWalkerContext *context)
{
	ListCell   *lc;
	int			rteid = 0;
	ListCell   *func_lc;

	foreach(lc, rtable)
	{
		rteid++;
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		if (!bms_is_member(rteid, context->scanrelids))
		{
			/* rte not referenced in the plan */
			continue;
		}

		switch (rte->rtekind)
		{
			case RTE_RELATION:
			case RTE_VOID:
			case RTE_CTE:
				/* nothing to do */
				break;
			case RTE_SUBQUERY:
				param_walker((Node *) rte->subquery, context);
				break;
			case RTE_JOIN:
				param_walker((Node *) rte->joinaliasvars, context);
				break;
			case RTE_FUNCTION:
				foreach(func_lc, rte->functions)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(func_lc);
					param_walker(rtfunc->funcexpr, context);
				}
				break;
			case RTE_TABLEFUNCTION:
				param_walker((Node *) rte->subquery, context);
				foreach(func_lc, rte->functions)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(func_lc);
					param_walker(rtfunc->funcexpr, context);
				}
				break;
			case RTE_VALUES:
				param_walker((Node *) rte->values_lists, context);
				break;
		}
	}
}

static bool
initplan_walker(Node *node, ParamWalkerContext *context)
{
	PlannerInfo *root;
	List	   *new_initplans = NIL;
	ListCell   *lc,
			   *lp;
	bool		anyused;		/* Are any of this Init Plan's output
								 * parameters actually used? */

	if (node == NULL)
		return false;

	if (is_plan_node(node))
	{
		root = (PlannerInfo *) context->base.node;
		Plan	   *plan = (Plan *) node;

		foreach(lc, plan->initPlan)
		{
			SubPlan    *subplan = (SubPlan *) lfirst(lc);

			Assert(subplan->is_initplan);

			anyused = false;
			foreach(lp, subplan->setParam)
			{
				int			paramid = lfirst_int(lp);

				if (bms_is_member(paramid, context->paramids))
				{
					anyused = true;
					break;
				}
			}

			/* If none of its params are used, leave out from the new list */
			if (anyused)
				new_initplans = lappend(new_initplans, subplan);
			else
			{
				/*
				 * This init plan is unused. Leave it out of this plan node's
				 * initPlan list, and also replace it in the global list of
				 * subplans with a dummy. (We can't just remove it from the
				 * global list, because that would screw up the plan_id
				 * numbering of the subplans).
				 */
				Result	   *dummy = make_result(root, NIL,
												(Node *) list_make1(makeBoolConst(false, false)),
												NULL);

				planner_subplan_put_plan(root, subplan, (Plan *) dummy);
			}
		}

		/* remove unused params */
		plan->allParam = bms_intersect(plan->allParam, context->paramids);

		list_free(plan->initPlan);
		plan->initPlan = new_initplans;
	}

	return plan_tree_walker(node, initplan_walker, context);
}

/*
 * Remove unused initplans from the given plan object
 */
void
remove_unused_initplans(Plan *top_plan, PlannerInfo *root)
{
	ParamWalkerContext context;

	if (!root->glob->subplans)
		return;

	context.base.node = (Node *) root;
	context.paramids = NULL;
	context.scanrelids = NULL;

	/*
	 * Collect param ids of all the Params that are referenced in the plan,and
	 * the IDs of all the range table entries that are referenced in the Plan.
	 */
	param_walker((Node *) top_plan, &context);

	/*
	 * Now that we know which range table entries are referenced in the plan,
	 * also collect Params from those range table entries.
	 */
	rte_param_walker(root->glob->finalrtable, &context);

	/* workhorse to remove unused initplans */
	initplan_walker((Node *) top_plan, &context);

	bms_free(context.paramids);
	bms_free(context.scanrelids);
}

/*
 * Append duplicate subplans and update plan id to refer them if same
 * subplan is referred at multiple places
 */
static bool
fixup_subplan_walker(Node *node, SubPlanWalkerContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, SubPlan))
	{
		SubPlan    *subplan = (SubPlan *) node;
		int			plan_id = subplan->plan_id;

		if (!bms_is_member(plan_id, context->bms_subplans))

			/*
			 * Add plan_id to bitmapset to maintain the plan_id's found while
			 * traversing the plan
			 */
			context->bms_subplans = bms_add_member(context->bms_subplans, plan_id);
		else if (!((SubPlan *) node)->is_initplan)
		{
			/*
			 * If plan_id is already available in the bitmapset, it means that
			 * there is more than one subplan node which refer to the same
			 * plan_id. In this case create a duplicate subplan, append it to
			 * the glob->subplans and update the plan_id of the subplan to
			 * refer to the new copy of the subplan node. Note since subroot
			 * is not used there is no need of new subroot, and initplans are
			 * not needed to be duplicated.
			 */
			PlannerInfo *root = (PlannerInfo *) context->base.node;
			Plan	    *dupsubplan = (Plan *) copyObject(planner_subplan_get_plan(root, subplan));
			int			 newplan_id = list_length(root->glob->subplans) + 1;

			subplan->plan_id = newplan_id;
			root->glob->subplans = lappend(root->glob->subplans, dupsubplan);
			context->bms_subplans = bms_add_member(context->bms_subplans, newplan_id);
			return false;
		}
	}
	return plan_tree_walker(node, fixup_subplan_walker, context);
}

/*
 * Entry point for fixing subplan references. A SubPlan node
 * cannot be parallelized twice, so if multiple subplan node
 * refer to the same plan_id, create a duplicate subplan and update
 * the plan_id of the subplan to refer to the new copy of subplan node
 * created in PlannedStmt subplans
 */
void
fixup_subplans(Plan *top_plan, PlannerInfo *root, SubPlanWalkerContext *context)
{
	context->base.node = (Node *) root;
	context->bms_subplans = NULL;

	/*
	 * If there are no subplans, no fixup will be required
	 */
	if (!root->glob->subplans)
		return;

	fixup_subplan_walker((Node *) top_plan, context);
}


/*
 * Remove unused subplans from PlannerGlobal subplans
 */
void
remove_unused_subplans(PlannerInfo *root, SubPlanWalkerContext *context)
{

	ListCell   *lc;

	for (int i = 1; i <= list_length(root->glob->subplans); i++)
	{
		if (!bms_is_member(i, context->bms_subplans))
		{
			/*
			 * This subplan is unused. Replace it in the global list of
			 * subplans with a dummy. (We can't just remove it from the global
			 * list, because that would screw up the plan_id numbering of the
			 * subplans).
			 */
			lc = list_nth_cell(root->glob->subplans, i - 1);
			pfree(lfirst(lc));
			lfirst(lc) = make_result(root, NIL,
									 (Node *) list_make1(makeBoolConst(false, false)),
									 NULL);
		}
	}
}

static Node *
planner_make_plan_constant(struct PlannerInfo *root, Node *n, bool is_SRI)
{
	pre_dispatch_function_evaluation_context pcontext;

	planner_init_plan_tree_base(&pcontext.base, root);
	pcontext.single_row_insert = is_SRI;
	pcontext.cursorPositions = NIL;
	pcontext.estate = NULL;

	return plan_tree_mutator(n, pre_dispatch_function_evaluation_mutator, &pcontext);
}

/*
 * Evaluate functions to constants.
 */
Node *
exec_make_plan_constant(struct PlannedStmt *stmt, EState *estate, bool is_SRI,
						List **cursorPositions)
{
	pre_dispatch_function_evaluation_context pcontext;
	Node	   *result;

	Assert(stmt);
	exec_init_plan_tree_base(&pcontext.base, stmt);
	pcontext.single_row_insert = is_SRI;
	pcontext.cursorPositions = NIL;
	pcontext.estate = estate;

	result = pre_dispatch_function_evaluation_mutator((Node *) stmt->planTree, &pcontext);

	*cursorPositions = pcontext.cursorPositions;
	return result;
}

/*
 * Remove subquery field in RTE's with subquery kind.
 */
void
remove_subquery_in_RTEs(Node *node)
{
	if (node == NULL)
	{
		return;
	}

	if (IsA(node, RangeTblEntry))
	{
		RangeTblEntry *rte = (RangeTblEntry *) node;

		if (RTE_SUBQUERY == rte->rtekind && NULL != rte->subquery)
		{
			/*
			 * Replace subquery with a dummy subquery.
			 *
			 * XXX: We could save a lot more memory by deep-freeing the many
			 * fields in the Query too. But I'm not sure which of them might
			 * be shared by other objects in the tree.
			 */
			pfree(rte->subquery);
			rte->subquery = NULL;
		}

		return;
	}

	if (IsA(node, List))
	{
		List	   *list = (List *) node;
		ListCell   *lc = NULL;

		foreach(lc, list)
		{
			remove_subquery_in_RTEs((Node *) lfirst(lc));
		}
	}
}

/*
 * Let's evaluate all STABLE functions that have constant args before
 * dispatch, so we get a consistent view across QEs
 *
 * Also, if this is a single_row insert, let's evaluate nextval() and
 * currval() before dispatching
 */
static Node *
pre_dispatch_function_evaluation_mutator(Node *node,
										 pre_dispatch_function_evaluation_context *context)
{
	Node	   *new_node = 0;

	if (node == NULL)
		return NULL;

	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		/* Not replaceable, so just copy the Param (no need to recurse) */
		return (Node *) copyObject(param);
	}
	else if (IsA(node, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) node;
		List	   *args;
		ListCell   *arg;
		Expr	   *simple;
		FuncExpr   *newexpr;
		bool		has_nonconst_input;

		Form_pg_proc funcform;
		EState	   *estate;
		ExprState  *exprstate;
		MemoryContext oldcontext;
		Datum		const_val;
		bool		const_is_null;
		int16		resultTypLen;
		bool		resultTypByVal;

		Oid			funcid;
		HeapTuple	func_tuple;

		/*
		 * Reduce constants in the FuncExpr's arguments. We know args is
		 * either NIL or a List node, so we can call expression_tree_mutator
		 * directly rather than recursing to self.
		 */
		args = (List *) expression_tree_mutator((Node *) expr->args,
												pre_dispatch_function_evaluation_mutator,
												(void *) context);

		funcid = expr->funcid;

		newexpr = makeNode(FuncExpr);
		newexpr->funcid = expr->funcid;
		newexpr->funcresulttype = expr->funcresulttype;
		newexpr->funcretset = expr->funcretset;
		newexpr->funcvariadic = expr->funcvariadic;
		newexpr->funcformat = expr->funcformat;
		newexpr->funccollid = expr->funccollid;
		newexpr->inputcollid = expr->inputcollid;
		newexpr->args = args;
		newexpr->location = expr->location;
		newexpr->is_tablefunc = expr->is_tablefunc;

		/*
		 * Check for constant inputs
		 */
		has_nonconst_input = false;

		foreach(arg, args)
		{
			if (!IsA(lfirst(arg), Const))
			{
				has_nonconst_input = true;
				break;
			}
		}

		if (!has_nonconst_input)
		{
			bool		is_seq_func = false;
			bool		tup_or_set;

			func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
			if (!HeapTupleIsValid(func_tuple))
				elog(ERROR, "cache lookup failed for function %u", funcid);

			funcform = (Form_pg_proc) GETSTRUCT(func_tuple);

			/* can't handle set returning or row returning functions */
			tup_or_set = (funcform->proretset || type_is_rowtype(funcform->prorettype));

			ReleaseSysCache(func_tuple);

			/* can't handle it */
			if (tup_or_set)
			{
				/*
				 * We haven't mutated this node, but we still return the
				 * mutated arguments.
				 *
				 * If we don't do this, we'll miss out on transforming
				 * function arguments which are themselves functions we need
				 * to mutated. For example, select foo(now()).
				 */
				return (Node *) newexpr;
			}

			/*
			 * Here we want to mark any statement that is going to use a
			 * sequence as dirty.  Doing this means that the QD will flush the
			 * xlog which will also flush any xlog writes that the sequence
			 * server might do.
			 */
			if (funcid == F_NEXTVAL_OID || funcid == F_CURRVAL_OID ||
				funcid == F_SETVAL_OID)
			{
				ExecutorMarkTransactionUsesSequences();
				is_seq_func = true;
			}

			if (funcform->provolatile == PROVOLATILE_IMMUTABLE)
				 /* okay */ ;
			else if (funcform->provolatile == PROVOLATILE_STABLE)
				 /* okay */ ;
			else if (context->single_row_insert && is_seq_func)
				;				/* Volatile, but special sequence function */
			else
				return (Node *) newexpr;

			/*
			 * Ok, we have a function that is STABLE (or IMMUTABLE), with
			 * constant args. Let's try to evaluate it.
			 */

			/*
			 * To use the executor, we need an EState.
			 */
			estate = CreateExecutorState();

			/* We can use the estate's working context to avoid memory leaks. */
			oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

			/*
			 * Prepare expr for execution.
			 */
			exprstate = ExecPrepareExpr((Expr *) newexpr, estate);

			/*
			 * And evaluate it.
			 *
			 * It is OK to use a default econtext because none of the
			 * ExecEvalExpr() code used in this situation will use econtext.
			 * That might seem fortuitous, but it's not so unreasonable --- a
			 * constant expression does not depend on context, by definition,
			 * n'est-ce pas?
			 */
			const_val = ExecEvalExprSwitchContext(exprstate,
												  GetPerTupleExprContext(estate),
												  &const_is_null, NULL);

			/* Get info needed about result datatype */
			get_typlenbyval(expr->funcresulttype, &resultTypLen, &resultTypByVal);

			/* Get back to outer memory context */
			MemoryContextSwitchTo(oldcontext);

			/* Must copy result out of sub-context used by expression eval */
			if (!const_is_null)
				const_val = datumCopy(const_val, resultTypByVal, resultTypLen);

			/* Release all the junk we just created */
			FreeExecutorState(estate);

			/*
			 * Make the constant result node.
			 */
			simple = (Expr *) makeConst(expr->funcresulttype,
										-1,
										expr->funccollid,
										resultTypLen,
										const_val, const_is_null,
										resultTypByVal);

			/* successfully simplified it */
			if (simple)
				return (Node *) simple;
		}

		/*
		 * The expression cannot be simplified any further, so build and
		 * return a replacement FuncExpr node using the possibly-simplified
		 * arguments.
		 */
		return (Node *) newexpr;
	}
	else if (IsA(node, OpExpr))
	{
		OpExpr	   *expr = (OpExpr *) node;
		List	   *args;

		OpExpr	   *newexpr;

		/*
		 * Reduce constants in the OpExpr's arguments.  We know args is either
		 * NIL or a List node, so we can call expression_tree_mutator directly
		 * rather than recursing to self.
		 */
		args = (List *) expression_tree_mutator((Node *) expr->args,
												pre_dispatch_function_evaluation_mutator,
												(void *) context);

		/*
		 * Need to get OID of underlying function.	Okay to scribble on input
		 * to this extent.
		 */
		set_opfuncid(expr);

		newexpr = makeNode(OpExpr);
		newexpr->opno = expr->opno;
		newexpr->opfuncid = expr->opfuncid;
		newexpr->opresulttype = expr->opresulttype;
		newexpr->opretset = expr->opretset;
		newexpr->opcollid = expr->opcollid;
		newexpr->inputcollid = expr->inputcollid;
		newexpr->args = args;
		newexpr->location = expr->location;

		return (Node *) newexpr;
	}
	else if (IsA(node, CurrentOfExpr))
	{
		/*
		 * updatable cursors
		 *
		 * During constant folding, we collect the current position of each
		 * cursor mentioned in the plan into a list, and dispatch them to the
		 * QEs.
		 *
		 * We should not get here if called from planner_make_plan_constant().
		 * That is only used for simple Result plans, which should not contain
		 * CURRENT OF expressions.
		 */
		if (context->estate)
		{
			CurrentOfExpr *expr = (CurrentOfExpr *) node;
			CursorPosInfo *cpos;

			cpos = makeNode(CursorPosInfo);

			getCurrentOf(expr,
						 GetPerTupleExprContext(context->estate),
						 expr->target_relid,
						 &cpos->ctid,
						 &cpos->gp_segment_id,
						 &cpos->table_oid,
						 &cpos->cursor_name);

			context->cursorPositions = lappend(context->cursorPositions, cpos);
		}
	}

	/*
	 * For any node type not handled above, we recurse using
	 * plan_tree_mutator, which will copy the node unchanged but try to
	 * simplify its arguments (if any) using this routine.
	 */
	new_node = plan_tree_mutator(node,
								 pre_dispatch_function_evaluation_mutator,
								 (void *) context);

	return new_node;
}


/*
 * sri_optimize_for_result
 *
 * Optimize for single-row-insertion for const result. If result is const, and
 * result relation is partitioned, we could decide partition relation during
 * plan time and replace targetPolicy by partition relation's targetPolicy.
 * In addition, we don't need tuple distribution, but do filter on each writer
 * segment.
 *
 * Inputs:
 *
 * root		PlannerInfo passed by caller
 * plan		should always be result node
 * rte		is the target relation entry
 *
 * Inputs/Outputs:
 *
 * targetPolicy(in/out) is the target relation policy, and would be replaced
 * by partition relation.
 * hashExpr is distribution expression of target relation, and would be
 * replaced by partition relation.
 */
void
sri_optimize_for_result(PlannerInfo *root, Plan *plan, RangeTblEntry *rte,
						GpPolicy **targetPolicy, List **hashExprs_p, List **hashOpfamilies_p)
{
	ListCell   *cell;
	bool		typesOK = true;
	PartitionNode *pn;
	Relation	rel;

	Insist(gp_enable_fast_sri && IsA(plan, Result));

	/* Suppose caller already hold proper locks for relation. */
	rel = relation_open(rte->relid, NoLock);

	/* 1: See if it's partitioned */
	pn = RelationBuildPartitionDesc(rel, false);

	if (pn && !partition_policies_equal(*targetPolicy, pn))
	{
		/*
		 * 2: See if partitioning columns are constant
		 */
		List	   *partatts = get_partition_attrs(pn);
		ListCell   *lc;
		bool		all_const = true;

		foreach(lc, partatts)
		{
			List	   *tl = plan->targetlist;
			ListCell   *cell;
			AttrNumber	attnum = lfirst_int(lc);
			bool		found = false;

			foreach(cell, tl)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(cell);

				Assert(tle->expr);
				if (tle->resno == attnum)
				{
					found = true;
					if (!IsA(tle->expr, Const))
						all_const = false;
					break;
				}
			}
			Assert(found);
		}

		/* 3: if not, mutate plan to make constant */
		if (!all_const)
			plan->targetlist = (List *)
				planner_make_plan_constant(root, (Node *) plan->targetlist, true);

		/* better be constant now */
		if (allConstantValuesClause(plan))
		{
			bool	   *nulls;
			Datum	   *values;
			EState	   *estate = CreateExecutorState();
			ResultRelInfo *rri;

			/*
			 * 4: build tuple, look up partitioning key
			 */
			nulls = palloc0(sizeof(bool) * rel->rd_att->natts);
			values = palloc(sizeof(Datum) * rel->rd_att->natts);

			foreach(lc, partatts)
			{
				AttrNumber	attnum = lfirst_int(lc);
				List	   *tl = plan->targetlist;
				ListCell   *cell;

				foreach(cell, tl)
				{
					TargetEntry *tle = (TargetEntry *) lfirst(cell);

					Assert(tle->expr);

					if (tle->resno == attnum)
					{
						Assert(IsA(tle->expr, Const));

						nulls[attnum - 1] = ((Const *) tle->expr)->constisnull;
						if (!nulls[attnum - 1])
							values[attnum - 1] = ((Const *) tle->expr)->constvalue;
					}
				}
			}
			estate->es_result_partitions = pn;
			estate->es_partition_state =
				createPartitionState(estate->es_result_partitions, 1 /* resultPartSize */ );

			rri = makeNode(ResultRelInfo);
			rri->ri_RangeTableIndex = 1;	/* dummy */
			rri->ri_RelationDesc = rel;

			estate->es_result_relations = rri;
			estate->es_num_result_relations = 1;
			estate->es_result_relation_info = rri;
			rri = values_get_partition(values, nulls, RelationGetDescr(rel),
									   estate, false);

			/*
			 * 5: get target policy for destination table
			 */
			*targetPolicy = RelationGetPartitioningKey(rri->ri_RelationDesc);

			if ((*targetPolicy)->ptype != POLICYTYPE_PARTITIONED)
				elog(ERROR, "policy must be partitioned");

			heap_close(rri->ri_RelationDesc, NoLock);
			FreeExecutorState(estate);
		}

	}
	relation_close(rel, NoLock);

	*hashExprs_p = getExprListFromTargetList(plan->targetlist,
										   (*targetPolicy)->nattrs,
										   (*targetPolicy)->attrs,
										   false);
	*hashOpfamilies_p = NIL;
	for (int i = 0; i < (*targetPolicy)->nattrs; i++)
	{
		Oid			opfamily = get_opclass_family((*targetPolicy)->opclasses[i]);

		*hashOpfamilies_p = lappend_oid(*hashOpfamilies_p, opfamily);
	}

	/*
	 * If there is no distribution key, don't do direct dispatch.
	 *
	 * GPDB_90_MERGE_FIXME: Is that the right thing to do? Couldn't we
	 * direct dispatch to any arbitrarily chosen segment, in that case?
	 */
	if (allConstantValuesClause(plan))
	{
		int			numHashAttrs;
		AttrNumber *hashAttrs;
		Oid		   *hashFuncs;
		int			i;

		numHashAttrs = (*targetPolicy)->nattrs;

		if (numHashAttrs == 0)
			typesOK = false;

		/* Get hash functions for the columns. */
		hashFuncs = palloc(numHashAttrs * sizeof(Oid));
		i = 0;
		foreach(cell, *hashExprs_p)
		{
			Expr	   *elem = (Expr *) lfirst(cell);
			Oid			att_type = exprType((Node *) elem);
			Oid			opclass = (*targetPolicy)->opclasses[i];

			hashFuncs[i++] =
				cdb_hashproc_in_opfamily(get_opclass_family(opclass),
										 att_type);
		}

		/*
		 * all constants in values clause -- no need to repartition.
		 */
		if (typesOK)
		{
			Result	   *rNode = (Result *) plan;
			int			i;

			/*
			 * If this table has child tables, we need to find out destination
			 * partition.
			 *
			 * See partition check above.
			 */

			/* copy the attributes array */
			hashAttrs = palloc(numHashAttrs * sizeof(AttrNumber));
			for (i = 0; i < numHashAttrs; i++)
				hashAttrs[i] = (*targetPolicy)->attrs[i];

			if (root->config->gp_enable_direct_dispatch)
			{
				directDispatchCalculateHash(plan, *targetPolicy, hashFuncs);

				/*
				 * we now either have a hash-code, or we've marked the plan
				 * non-directed.
				 */
			}

			rNode->numHashFilterCols = numHashAttrs;
			rNode->hashFilterColIdx = hashAttrs;
			rNode->hashFilterFuncs = hashFuncs;

			/* Build a partitioned flow */
			plan->flow->flotype = FLOW_PARTITIONED;
			plan->flow->locustype = CdbLocusType_Hashed;
			plan->flow->hashExprs = *hashExprs_p;
			plan->flow->hashOpfamilies = *hashOpfamilies_p;
		}
	}
}

/*
 * Does the given expression contain Params that are passed down from
 * outer query?
 */
bool
contains_outer_params(Node *node, void *context)
{
	PlannerInfo *root = (PlannerInfo *) context;

	if (node == NULL)
		return false;
	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_EXEC)
		{
			/* Does this Param refer to a value that an outer query provides? */
			PlannerInfo *parent = root->parent_root;

			while (parent)
			{
				ListCell   *lc;

				foreach (lc, parent->plan_params)
				{
					PlannerParamItem *ppi = (PlannerParamItem *) lfirst(lc);

					if (ppi->paramId == param->paramid)
						return true;		/* abort the tree traversal and return true */
				}

				parent = parent->parent_root;
			}
		}
	}
	return expression_tree_walker(node, contains_outer_params, context);
}
