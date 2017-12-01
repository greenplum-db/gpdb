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

#include "access/xact.h"
#include "parser/analyze.h"		/* for createRandomDistribution() */
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "parser/parse_expr.h"	/* for expr_type() */
#include "parser/parse_oper.h"	/* for compatible_oper_opid() */
#include "utils/relcache.h"		/* RelationGetPartitioningKey() */
#include "optimizer/tlist.h"	/* get_sortgroupclause_tle() */
#include "optimizer/planmain.h"
#include "optimizer/predtest.h"
#include "optimizer/var.h"
#include "parser/parse_relation.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/datum.h"
#include "utils/syscache.h"
#include "utils/portal.h"
#include "optimizer/clauses.h"
#include "optimizer/planmain.h"
#include "nodes/makefuncs.h"

#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "catalog/catalog.h"
#include "catalog/gp_policy.h"
#include "catalog/pg_type.h"

#include "catalog/pg_proc.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbhash.h"		/* isGreenplumDbHashable() */
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbtargeteddispatch.h"

#include "nodes/print.h"

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
	List	   *initPlans;
} ApplyMotionState;

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

static int *makeDefaultSegIdxArray(int numSegs);

static void add_slice_to_motion(Motion *motion,
					MotionType motionType, List *hashExpr,
					int numOutputSegs, int *outputSegIdx);

static Node *apply_motion_mutator(Node *node, ApplyMotionState *context);

static void request_explicit_motion(Plan *plan, Index resultRelationIdx, List *rtable);

static void request_explicit_motion_append(Append *append, List *resultRelationsIdx, List *rtable);

static AttrNumber find_segid_column(List *tlist, Index relid);

static bool doesUpdateAffectPartitionCols(PlannerInfo *root,
							  Plan *plan,
							  Query *query);

static bool replace_shareinput_targetlists_walker(Node *node, PlannerGlobal *glob, bool fPop);

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
directDispatchCalculateHash(Plan *plan, GpPolicy *targetPolicy)
{
	int			i;
	CdbHash    *h = NULL;
	ListCell   *cell = NULL;
	bool		directDispatch;

	h = makeCdbHash(GpIdentity.numsegments);
	cdbhashinit(h);

	/*
	 * the nested loops here seem scary -- especially since we've already
	 * walked them before -- but I think this is still required since they may
	 * not be in the same order. (also typically we don't distribute by more
	 * than a handful of attributes).
	 */
	directDispatch = true;
	for (i = 0; i < targetPolicy->nattrs; i++)
	{
		Const	   *c;
		TargetEntry *tle = NULL;

		foreach(cell, plan->targetlist)
		{
			tle = (TargetEntry *) lfirst(cell);

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
			if (c->constisnull)
			{
				cdbhashnull(h);
			}
			else
			{
				Assert(isGreenplumDbHashable(c->consttype));
				cdbhash(h, c->constvalue, typeIsArrayType(c->consttype) ? ANYARRAYOID : c->consttype);
			}

			break;
		}

		if (!directDispatch)
			break;
	}

	/* We now have the hash-partition that this row belong to */
	plan->directDispatch.isDirectDispatch = directDispatch;
	if (directDispatch)
	{
		uint32		hashcode = cdbhashreduce(h);

		plan->directDispatch.contentIds = list_make1_int(hashcode);

		elog(DEBUG1, "sending single row constant insert to content %d", hashcode);
	}
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
	GpPolicyType targetPolicyType = POLICYTYPE_ENTRY;
	ApplyMotionState state;
	bool		needToAssignDirectDispatchContentIds = false;

	/* Initialize mutator context. */

	planner_init_plan_tree_base(&state.base, root); /* error on attempt to
													 * descend into subplan
													 * plan */
	state.nextMotionID = 1;		/* Start at 1 so zero will mean "unassigned". */
	state.sliceDepth = 0;
	state.containMotionNodes = false;
	state.initPlans = NIL;

	Assert(is_plan_node((Node *) plan) && IsA(query, Query));
	Assert(plan->flow && plan->flow->flotype != FLOW_REPLICATED);

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

		targetPolicy = GpPolicyFetch(CurrentMemoryContext, rte->relid);
		targetPolicyType = targetPolicy->ptype;
	}

	switch (query->commandType)
	{
		case CMD_SELECT:


			if (query->intoClause)
			{
				List	   *hashExpr;
				ListCell   *exp1;

				if (query->intoPolicy != NULL)
				{
					targetPolicy = query->intoPolicy;

					Assert(query->intoPolicy->ptype == POLICYTYPE_PARTITIONED);
					Assert(query->intoPolicy->nattrs >= 0);
					Assert(query->intoPolicy->nattrs <= MaxPolicyAttributeNumber);
				}
				else if (gp_create_table_random_default_distribution)
				{
					targetPolicy = createRandomDistribution();
					ereport(NOTICE,
							(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
							 errmsg("Using default RANDOM distribution since no distribution was specified."),
							 errhint("Consider including the 'DISTRIBUTED BY' clause to determine the distribution of rows.")));
				}
				else
				{
					/* Find out what the flow is partitioned on */
					hashExpr = plan->flow->hashExpr;

					/* User did not specify a DISTRIBUTED BY clause */
					if (hashExpr)
						targetPolicy = palloc0(sizeof(GpPolicy) - sizeof(targetPolicy->attrs) + list_length(hashExpr) * sizeof(targetPolicy->attrs[0]));
					else
						targetPolicy = palloc0(sizeof(GpPolicy));

					targetPolicy->ptype = POLICYTYPE_PARTITIONED;
					targetPolicy->nattrs = 0;

					if (hashExpr)
						foreach(exp1, hashExpr)
					{
						AttrNumber	n;
						bool		found_expr = false;


						/* See if this Expr is a column of the result table */

						for (n = 1; n <= list_length(plan->targetlist); n++)
						{
							Var		   *new_var = NULL;

							TargetEntry *target = get_tle_by_resno(plan->targetlist, n);

							if (!target->resjunk)
							{
								Expr	   *var1 = (Expr *) lfirst(exp1);

								/*
								 * right side variable may be encapsulated by
								 * a relabel node. motion, however, does not
								 * care about relabel nodes.
								 */
								if (IsA(var1, RelabelType))
									var1 = ((RelabelType *) var1)->arg;

								/*
								 * If subplan expr is a Var, copy to preserve
								 * its EXPLAIN info.
								 */
								if (IsA(target->expr, Var))
								{
									new_var = copyObject(target->expr);
									new_var->varno = OUTER;
									new_var->varattno = n;
								}

								/*
								 * Make a Var that references the target list
								 * entry at this offset, using OUTER as the
								 * varno
								 */
								else
									new_var = makeVar(OUTER,
													  n,
													  exprType((Node *) target->expr),
													  exprTypmod((Node *) target->expr),
													  0);

								if (equal(var1, new_var))
								{
									/*
									 * If it is, use it to partition the
									 * result table, to avoid unnecessary
									 * redistibution of data
									 */
									Assert(targetPolicy->nattrs < MaxPolicyAttributeNumber);
									targetPolicy->attrs[targetPolicy->nattrs++] = n;
									found_expr = true;
									break;

								}

							}
						}

						if (!found_expr)
							break;
					}

					/* do we know how to partition? */
					if (targetPolicy->nattrs == 0)
					{
						/*
						 * hash on the first hashable column we can find.
						 */
						int			i;

						for (i = 0; i < list_length(plan->targetlist); i++)
						{
							TargetEntry *target =
							get_tle_by_resno(plan->targetlist, i + 1);

							if (target)
							{
								Oid			typeOid = exprType((Node *) target->expr);

								if (isGreenplumDbHashable(typeOid))
								{
									targetPolicy->attrs[targetPolicy->nattrs++] = i + 1;
									break;
								}
							}
						}
					}

					if (query->intoPolicy == NULL)
					{
						char	   *columns;
						int			i;

						columns = palloc((NAMEDATALEN + 3) * targetPolicy->nattrs + 1);
						columns[0] = '\0';
						for (i = 0; i < targetPolicy->nattrs; i++)
						{
							TargetEntry *target = get_tle_by_resno(plan->targetlist, targetPolicy->attrs[i]);

							if (i > 0)
								strcat(columns, ", ");
							if (target->resname)
								strcat(columns, target->resname);
							else
								strcat(columns, "???");

						}

						ereport(NOTICE,
								(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
								 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column(s) "
										"named '%s' as the Greenplum Database data distribution key for this "
										"table. ", columns),
								 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
										 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
					}
				}

				query->intoPolicy = targetPolicy;

				/*
				 * Make sure the top level flow is partitioned on the
				 * partitioning key of the target relation.	Since this is a
				 * SELECT INTO (basically same as an INSERT) command, the
				 * target list will correspond to the attributes of the target
				 * relation in order.
				 */
				hashExpr = getExprListFromTargetList(plan->targetlist,
													 targetPolicy->nattrs,
													 targetPolicy->attrs,
													 true);
				if (!repartitionPlan(plan, false, false, hashExpr))
					ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_YET),
									errmsg("Cannot parallelize that SELECT INTO yet")
									));

				Assert(query->intoPolicy->ptype == POLICYTYPE_PARTITIONED);

			}

			if (plan->flow->flotype == FLOW_PARTITIONED && !query->intoClause)
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
			needToAssignDirectDispatchContentIds = root->config->gp_enable_direct_dispatch && !query->intoClause;
			break;

		case CMD_INSERT:
			{
				switch (targetPolicyType)
				{
					case POLICYTYPE_PARTITIONED:
						{
							List	   *hashExpr = NIL;

							/*
							 * Make sure the top level flow is partitioned on
							 * the partitioning key of the target relation.
							 * Since this is an INSERT command, the target
							 * list will correspond to the attributes of the
							 * target relation in order.
							 */


							/* Is this plan an all-const insert ? */
							if (gp_enable_fast_sri && IsA(plan, Result))
							{
								ListCell   *cell;
								bool		typesOK = true;
								PartitionNode *pn;
								RangeTblEntry *rte;
								Relation	rel;

								rte = rt_fetch(query->resultRelation, query->rtable);
								rel = relation_open(rte->relid, NoLock);

								/* 1: See if it's partitioned */
								pn = RelationBuildPartitionDesc(rel, false);

								if (pn && !partition_policies_equal(targetPolicy, pn))
								{
									/*
									 * 2: See if partitioning columns are
									 * constant
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
										 * 4: build tuple, look up
										 * partitioning key
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
																   estate);

										/*
										 * 5: get target policy for
										 * destination table
										 */
										targetPolicy = RelationGetPartitioningKey(rri->ri_RelationDesc);

										if (targetPolicy->ptype != POLICYTYPE_PARTITIONED)
											elog(ERROR, "policy must be partitioned");

										ExecCloseIndices(rri);
										heap_close(rri->ri_RelationDesc, NoLock);
										FreeExecutorState(estate);
									}

								}
								relation_close(rel, NoLock);

								hashExpr = getExprListFromTargetList(plan->targetlist,
																	 targetPolicy->nattrs,
																	 targetPolicy->attrs,
																	 true);
								/* check the types. */
								foreach(cell, hashExpr)
								{
									Expr	   *elem = NULL;
									Oid			att_type = InvalidOid;

									elem = (Expr *) lfirst(cell);
									att_type = exprType((Node *) elem);
									Assert(att_type != InvalidOid);
									if (!isGreenplumDbHashable(att_type))
									{
										typesOK = false;
										break;
									}
								}

								/*
								 * all constants in values clause -- no need
								 * to repartition.
								 */
								if (typesOK && allConstantValuesClause(plan))
								{
									Result	   *rNode = (Result *) plan;
									List	   *hList = NIL;
									int			i;

									/*
									 * If this table has child tables, we need
									 * to find out destination partition.
									 *
									 * See partition check above.
									 */

									/* build our list */
									for (i = 0; i < targetPolicy->nattrs; i++)
									{
										Assert(targetPolicy->attrs[i] > 0);

										hList = lappend_int(hList, targetPolicy->attrs[i]);
									}

									if (root->config->gp_enable_direct_dispatch)
									{
										directDispatchCalculateHash(plan, targetPolicy);

										/*
										 * we now either have a hash-code, or
										 * we've marked the plan non-directed.
										 */
									}

									rNode->hashFilter = true;
									rNode->hashList = hList;

									/* Build a partitioned flow */
									plan->flow->flotype = FLOW_PARTITIONED;
									plan->flow->locustype = CdbLocusType_Hashed;
									plan->flow->hashExpr = hashExpr;
								}
							}

							if (!hashExpr)
								hashExpr = getExprListFromTargetList(plan->targetlist,
																	 targetPolicy->nattrs,
																	 targetPolicy->attrs,
																	 true);

							if (!repartitionPlan(plan, false, false, hashExpr))
								ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_YET),
												errmsg("Cannot parallelize that INSERT yet")));
							break;
						}

					case POLICYTYPE_ENTRY:
						/* All's well if query result is already on the QD. */
						if (plan->flow->flotype == FLOW_SINGLETON &&
							plan->flow->segindex < 0)
							break;

						/*
						 * Query result needs to be brought back to the QD.
						 * Ask for motion to a single QE.  Later, apply_motion
						 * will override that to bring it to the QD instead.
						 */
						if (!focusPlan(plan, false, false))
							ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_YET),
											errmsg("Cannot parallelize that INSERT yet")));
						break;

					default:
						Insist(0);
				}
			}
			break;

		case CMD_UPDATE:
		case CMD_DELETE:
			{
				/* Distributed target table? */
				if (targetPolicyType == POLICYTYPE_PARTITIONED)
				{
					/*
					 * The planner does not support updating any of the
					 * partitioning columns.
					 */
					if (query->commandType == CMD_UPDATE &&
						doesUpdateAffectPartitionCols(root, plan, query))
					{
						ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_YET),
										errmsg("Cannot parallelize an UPDATE statement that updates the distribution columns")));
					}

					if (IsA(plan, Append))
					{
						/* updating a partitioned table */
						request_explicit_motion_append((Append *) plan, root->resultRelations, root->glob->finalrtable);
					}
					else
					{
						/* updating a non-partitioned table */
						Assert(list_length(root->resultRelations) == 1);
						int			relid = list_nth_int(root->resultRelations, 0);

						Assert(relid > 0);
						request_explicit_motion(plan, relid, root->glob->finalrtable);
					}

					needToAssignDirectDispatchContentIds = root->config->gp_enable_direct_dispatch;
				}

				/* Target table is not distributed.  Must be in entry db. */
				else
				{
					if (plan->flow->flotype == FLOW_PARTITIONED ||
						plan->flow->flotype == FLOW_REPLICATED ||
						(plan->flow->flotype == FLOW_SINGLETON && plan->flow->segindex != -1))
					{
						/*
						 * target table is master-only but flow is
						 * distributed: add a GatherMotion on top
						 */

						/* create a shallow copy of the plan flow */
						Flow	   *flow = plan->flow;

						plan->flow = (Flow *) palloc(sizeof(Flow));
						*(plan->flow) = *flow;

						/* save original flow information */
						plan->flow->flow_before_req_move = flow;

						/* request a GatherMotion node */
						plan->flow->req_move = MOVEMENT_FOCUS;
						plan->flow->hashExpr = NIL;
						plan->flow->segindex = 0;
					}
					else
					{
						/*
						 * Source is, presumably, a dispatcher singleton.
						 */
						plan->flow->req_move = MOVEMENT_NONE;
					}
					break;
				}
			}
			break;

		default:
			Insist(0);			/* Never see non-DML in here! */
	}

	result = (Plan *) apply_motion_mutator((Node *) plan, &state);

	if (needToAssignDirectDispatchContentIds)
	{
		/* figure out if we can run on a reduced set of nodes */
		AssignContentIdsToPlanData(query, result, root);
	}

	Assert(result->nMotionNodes == state.nextMotionID - 1);
	Assert(result->nInitPlans == list_length(state.initPlans));

	/* Assign slice numbers to the initplans. */
	foreach(cell, state.initPlans)
	{
		SubPlan    *subplan = (SubPlan *) lfirst(cell);

		Assert(IsA(subplan, SubPlan) &&subplan->qDispSliceId == 0);
		subplan->qDispSliceId = state.nextMotionID++;
	}

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
Node *
apply_motion_mutator(Node *node, ApplyMotionState *context)
{
	Node	   *newnode;
	Plan	   *plan;
	Flow	   *flow;
	int			saveNextMotionID;
	int			saveNumInitPlans;
	int			saveSliceDepth;
	PlannerInfo *root = (PlannerInfo *) context->base.node;

	Assert(root && IsA(root, PlannerInfo));

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
			bool		saveContainMotionNodes = context->containMotionNodes;

			node = plan_tree_mutator(node, apply_motion_mutator, context);

			context->containMotionNodes = saveContainMotionNodes;

			return node;
		}
		else
			return plan_tree_mutator(node, apply_motion_mutator, context);
	}

	plan = (Plan *) node;
	flow = plan->flow;

	saveNextMotionID = context->nextMotionID;
	saveNumInitPlans = list_length(context->initPlans);

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

		foreach(cell, plan->initPlan)
		{
			subplan = (SubPlan *) lfirst(cell);
			Assert(IsA(subplan, SubPlan));
			Assert(root);
			Assert(planner_subplan_get_plan(root, subplan));

			context->initPlans = lappend(context->initPlans, subplan);
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

		/* Assign unique node number to the new node. */
		assignMotionID(newnode, context, node);

		/* If top slice marked as singleton, make it a dispatcher singleton. */
		if (motion->motionType == MOTIONTYPE_FIXED
			&& motion->numOutputSegs == 1
			&& motion->outputSegIdx[0] >= 0
			&& context->sliceDepth == 0)
		{
			Assert(flow->flotype == FLOW_SINGLETON &&
				   flow->segindex == motion->outputSegIdx[0]);
			flow->segindex = -1;
			motion->outputSegIdx[0] = -1;
		}
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
			else if (flow->segindex >= 0)
				flow->segindex = gp_singleton_segindex;

			newnode = (Node *) make_union_motion(plan,
												 flow->segindex,
												 true /* useExecutorVarFormat */ );
			break;

		case MOVEMENT_BROADCAST:
			newnode = (Node *) make_broadcast_motion(plan, true /* useExecutorVarFormat */ );
			break;

		case MOVEMENT_REPARTITION:
			newnode = (Node *) make_hashed_motion(plan,
												  flow->hashExpr,
												  true	/* useExecutorVarFormat */
				);
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
	plan->nInitPlans = list_length(context->initPlans) - saveNumInitPlans;

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
void
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
					MotionType motionType, List *hashExpr,
					int numOutputSegs, int *outputSegIdx)
{
	/* sanity checks */
	/* check numOutputSegs and outputSegIdx are in sync.  */
	AssertEquivalent(numOutputSegs == 0, outputSegIdx == NULL);

	/* numOutputSegs are either 0, for hash or broadcast, or 1, for focus */
	Assert(numOutputSegs == 0 || numOutputSegs == 1);
	AssertImply(motionType == MOTIONTYPE_HASH, numOutputSegs == 0);
	AssertImply(numOutputSegs == 1, (outputSegIdx[0] == -1 || outputSegIdx[0] == gp_singleton_segindex));


	motion->motionType = motionType;
	motion->hashExpr = hashExpr;
	motion->hashDataTypes = NULL;
	motion->numOutputSegs = numOutputSegs;
	motion->outputSegIdx = outputSegIdx;

	Assert(motion->plan.lefttree);

	/* Build list of hash key expression data types. */
	if (hashExpr)
	{
		List	   *eq = list_make1(makeString("="));
		ListCell   *cell;

		foreach(cell, hashExpr)
		{
			Node	   *expr = (Node *) lfirst(cell);
			Oid			typeoid = exprType(expr);
			Oid			eqopoid;
			Oid			lefttype;
			Oid			righttype;

			/* Get oid of the equality operator for this data type. */
			eqopoid = compatible_oper_opid(eq, typeoid, typeoid, true);
			if (eqopoid == InvalidOid)
				ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
								errmsg("no equality operator for typid %d",
									   typeoid)));

			/* Get the equality operator's operand type. */
			op_input_types(eqopoid, &lefttype, &righttype);
			Assert(lefttype == righttype);

			/* If this type is a domain type, get its base type. */
			if (get_typtype(lefttype) == 'd')
				lefttype = getBaseType(lefttype);

			motion->hashDataTypes = lappend_oid(motion->hashDataTypes, lefttype);
		}
		list_free_deep(eq);
	}


	/* Attach a descriptive Flow. */
	switch (motion->motionType)
	{
		case MOTIONTYPE_HASH:
			motion->plan.flow = makeFlow(FLOW_PARTITIONED);
			motion->plan.flow->locustype = CdbLocusType_Hashed;
			motion->plan.flow->hashExpr = copyObject(motion->hashExpr);
			motion->numOutputSegs = getgpsegmentCount();
			motion->outputSegIdx = makeDefaultSegIdxArray(getgpsegmentCount());

			break;
		case MOTIONTYPE_FIXED:
			if (motion->numOutputSegs == 0)
			{
				/* broadcast */
				motion->plan.flow = makeFlow(FLOW_REPLICATED);
				motion->plan.flow->locustype = CdbLocusType_Replicated;

			}
			else if (motion->numOutputSegs == 1)
			{
				/* Focus motion */
				motion->plan.flow = makeFlow(FLOW_SINGLETON);
				motion->plan.flow->segindex = motion->outputSegIdx[0];
				motion->plan.flow->locustype = (motion->plan.flow->segindex < 0) ?
					CdbLocusType_Entry :
					CdbLocusType_SingleQE;

			}
			else
			{
				Assert(!"Invalid numoutputSegs for motion type fixed");
				motion->plan.flow = NULL;
			}
			break;
		case MOTIONTYPE_EXPLICIT:
			motion->plan.flow = makeFlow(FLOW_PARTITIONED);

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
make_union_motion(Plan *lefttree, int destSegIndex, bool useExecutorVarFormat)
{
	Motion	   *motion;
	int		   *outSegIdx = (int *) palloc(sizeof(int));

	outSegIdx[0] = destSegIndex;

	motion = make_motion(NULL, lefttree, NIL, useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_FIXED, NULL, 1, outSegIdx);
	return motion;
}

Motion *
make_sorted_union_motion(PlannerInfo *root,
						 Plan *lefttree,
						 int destSegIndex,
						 List *sortPathKeys,
						 bool useExecutorVarFormat)
{
	Motion	   *motion;
	int		   *outSegIdx = (int *) palloc(sizeof(int));

	outSegIdx[0] = destSegIndex;

	motion = make_motion(root, lefttree, sortPathKeys, useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_FIXED, NULL, 1, outSegIdx);
	return motion;
}

Motion *
make_hashed_motion(Plan *lefttree,
				   List *hashExpr, bool useExecutorVarFormat)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree, NIL, useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_HASH, hashExpr, 0, NULL);
	return motion;
}

Motion *
make_broadcast_motion(Plan *lefttree, bool useExecutorVarFormat)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree, NIL, useExecutorVarFormat);

	add_slice_to_motion(motion, MOTIONTYPE_FIXED, NULL, 0, NULL);
	return motion;
}

Motion *
make_explicit_motion(Plan *lefttree, AttrNumber segidColIdx, bool useExecutorVarFormat)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree, NIL, useExecutorVarFormat);

	Assert(segidColIdx > 0 && segidColIdx <= list_length(lefttree->targetlist));

	motion->segidColIdx = segidColIdx;

	add_slice_to_motion(motion, MOTIONTYPE_EXPLICIT, NULL, 0, NULL);
	return motion;
}

/* --------------------------------------------------------------------
 * makeDefaultSegIdxArray -- creates an int array with numSegs elements,
 * with values between 0 and numSegs-1
 * --------------------------------------------------------------------
 */
int *
makeDefaultSegIdxArray(int numSegs)
{
	int		   *segIdx = palloc(numSegs * sizeof(int));

	/*
	 * The following assumes that the default is to have segindexes numbered
	 * sequentially starting at 0.
	 */
	int			i;

	for (i = 0; i < numSegs; i++)
		segIdx[i] = i;

	return segIdx;
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
 * The entries have the varno field replaced by references in OUTER.
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
			elist = lappend(elist, cdbpullup_make_expr(OUTER, n, target->expr, false));

		/* Before set_plan_references(), copy the subplan's result expr. */
		else
			elist = lappend(elist, copyObject(target->expr));
	}

	return elist;
}


/*
 * isAnyColChangedByUpdate
 */
static bool
isAnyColChangedByUpdate(Index targetvarno,
						List *targetlist,
						int nattrs,
						AttrNumber *attrs)
{
	/*
	 * Want to test whether an update statement possibly changes the value of
	 * any of the partitioning columns.
	 */

	/*
	 * For each partitioning column, the TargetListEntry for that varattno
	 * should be just a Var node with the same varattno.
	 */
	int			i;

	for (i = 0; i < nattrs; i++)
	{
		TargetEntry *tle = get_tle_by_resno(targetlist, attrs[i]);
		Var		   *var;

		Insist(tle);

		if (!IsA(tle->expr, Var))
			return true;

		var = (Var *) tle->expr;
		if (var->varnoold != targetvarno ||
			var->varoattno != attrs[i])
			return true;
	}

	return false;
}								/* isAnyColChangedByUpdate */

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
	AttrNumber	segidColIdx = find_segid_column(plan->targetlist, resultRelationsIdx);

	Assert(-1 != segidColIdx);

	plan->flow->segidColIdx = segidColIdx;
}

/*
 * Request SegIdRedistribute motion node for an Append plan updating a partitioned
 * table
 */
void
request_explicit_motion_append(Append *append, List *resultRelationsIdx, List *rtable)
{
	Assert(IsA(append, Append));
	Assert(list_length(append->appendplans) == list_length(resultRelationsIdx));

	ListCell   *lcAppendPlan = NULL;
	ListCell   *lcResultRel = NULL;

	forboth(lcAppendPlan, append->appendplans,
			lcResultRel, resultRelationsIdx)
	{
		/*
		 * request a segid redistribute motion node to be put above each
		 * append plan
		 */
		Plan	   *appendChildPlan = (Plan *) lfirst(lcAppendPlan);
		int			relid = lfirst_int(lcResultRel);

		Assert(relid > 0);
		request_explicit_motion(appendChildPlan, relid, rtable);
	}

}


/*
 * Find the index of the segid column of the requested relation (relid) in the
 * target list
 */
AttrNumber
find_segid_column(List *tlist, Index relid)
{
	if (NIL == tlist)
	{
		return -1;
	}

	ListCell   *lcte = NULL;

	foreach(lcte, tlist)
	{
		TargetEntry *te = (TargetEntry *) lfirst(lcte);

		Var		   *var = (Var *) te->expr;

		if (!IsA(var, Var))
		{
			continue;
		}

		if ((var->varno == relid && var->varattno == GpSegmentIdAttributeNumber) ||
			(var->varnoold == relid && var->varoattno == GpSegmentIdAttributeNumber))
		{
			return te->resno;
		}
	}

	/* no segid column found */
	return -1;
}

/*
 * doesUpdateAffectPartitionCols
 */
bool
doesUpdateAffectPartitionCols(PlannerInfo *root,
							  Plan *plan,
							  Query *query)
{
	Append	   *append;
	ListCell   *rticell;
	ListCell   *plancell;
	GpPolicy   *policy;

	if (list_length(root->resultRelations) == 1)
	{
		RangeTblEntry *rte;
		Relation	relation;
		int			resultRelation = linitial_int(root->resultRelations);

		rte = rt_fetch(resultRelation, query->rtable);
		Assert(rte->rtekind == RTE_RELATION);

		/* Get a copy of the rel's GpPolicy from the relcache. */
		relation = relation_open(rte->relid, NoLock);
		policy = RelationGetPartitioningKey(relation);
		relation_close(relation, NoLock);


		/*
		 * Single target relation?
		 */

		return isAnyColChangedByUpdate(resultRelation,
									   plan->targetlist,
									   policy->nattrs,
									   policy->attrs);
	}

	/*
	 * Multiple target relations (inheritance)... Plan should have an Append
	 * node on top, with a subplan per target.
	 */
	append = (Append *) plan;

	Insist(IsA(append, Append) &&
		   append->isTarget &&
		   list_length(append->appendplans) == list_length(root->resultRelations));

	forboth(rticell, root->resultRelations,
			plancell, append->appendplans)
	{
		int			rti = lfirst_int(rticell);
		Plan	   *subplan = (Plan *) lfirst(plancell);

		RangeTblEntry *rte;
		Relation	relation;

		rte = rt_fetch(rti, query->rtable);
		Assert(rte->rtekind == RTE_RELATION);

		/* Get a copy of the rel's GpPolicy from the relcache. */
		relation = relation_open(rte->relid, NoLock);
		policy = RelationGetPartitioningKey(relation);
		relation_close(relation, NoLock);

		if (isAnyColChangedByUpdate(rti, subplan->targetlist, policy->nattrs, policy->attrs))
			return true;
	}

	return false;
}								/* doesUpdateAffectPartitionCols */



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
		rel->cdbpolicy->ptype != POLICYTYPE_PARTITIONED)
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

		ereport(elevel, (errmsg("%s uses system-defined column \"%s.ctid\" "
								"without the necessary companion column "
								"\"%s.gp_segment_id\"",
								cmd,
								rte->eref->aliasname,
								rte->eref->aliasname),
						 errhint("To uniquely identify a row within a "
								 "distributed table, use the \"gp_segment_id\" "
								 "column together with the \"ctid\" column.")
						 ));
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
typedef bool (*SHAREINPUT_MUTATOR) (Node *node, PlannerGlobal *glob, bool fPop);
static void
shareinput_walker(SHAREINPUT_MUTATOR f, Node *node, PlannerGlobal *glob)
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

			shareinput_walker(f, n, glob);
		}
		return;
	}

	if (!is_plan_node(node))
		return;

	plan = (Plan *) node;
	recursive_down = (*f) (node, glob, false);

	if (recursive_down)
	{
		if (IsA(node, Append))
		{
			ListCell   *cell;
			Append	   *app = (Append *) node;

			foreach(cell, app->appendplans)
				shareinput_walker(f, (Node *) lfirst(cell), glob);
		}
		else if (IsA(node, SubqueryScan))
		{
			SubqueryScan *subqscan = (SubqueryScan *) node;
			List	   *save_rtable;

			save_rtable = glob->share.curr_rtable;
			glob->share.curr_rtable = subqscan->subrtable;
			shareinput_walker(f, (Node *) subqscan->subplan, glob);
			glob->share.curr_rtable = save_rtable;
		}
		else if (IsA(node, BitmapAnd))
		{
			ListCell   *cell;
			BitmapAnd  *ba = (BitmapAnd *) node;

			foreach(cell, ba->bitmapplans)
				shareinput_walker(f, (Node *) lfirst(cell), glob);
		}
		else if (IsA(node, BitmapOr))
		{
			ListCell   *cell;
			BitmapOr   *bo = (BitmapOr *) node;

			foreach(cell, bo->bitmapplans)
				shareinput_walker(f, (Node *) lfirst(cell), glob);
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
				shareinput_walker(f, (Node *) plan->righttree, glob);
				shareinput_walker(f, (Node *) plan->lefttree, glob);
			}
			else
			{
				shareinput_walker(f, (Node *) plan->lefttree, glob);
				shareinput_walker(f, (Node *) plan->righttree, glob);
			}
		}
		else if (IsA(node, HashJoin))
		{
			/* Hash join the hash table is at inner */
			shareinput_walker(f, (Node *) plan->righttree, glob);
			shareinput_walker(f, (Node *) plan->lefttree, glob);
		}
		else if (IsA(node, MergeJoin))
		{
			MergeJoin  *mj = (MergeJoin *) node;

			if (mj->unique_outer)
			{
				shareinput_walker(f, (Node *) plan->lefttree, glob);
				shareinput_walker(f, (Node *) plan->righttree, glob);
			}
			else
			{
				shareinput_walker(f, (Node *) plan->righttree, glob);
				shareinput_walker(f, (Node *) plan->lefttree, glob);
			}
		}
		else if (IsA(node, Sequence))
		{
			ListCell   *cell = NULL;
			Sequence   *sequence = (Sequence *) node;

			foreach(cell, sequence->subplans)
			{
				shareinput_walker(f, (Node *) lfirst(cell), glob);
			}
		}
		else
		{
			shareinput_walker(f, (Node *) plan->lefttree, glob);
			shareinput_walker(f, (Node *) plan->righttree, glob);
			shareinput_walker(f, (Node *) plan->initPlan, glob);
		}
	}

	(*f) (node, glob, true);
}

typedef struct
{
	int			nextPlanId;
} assign_plannode_id_walker_context;

static void
assign_plannode_id_walker(Node *node, assign_plannode_id_walker_context *ctxt)
{
	Plan	   *plan;

	if (node == NULL)
		return;

	if (IsA(node, List))
	{
		List	   *l = (List *) node;
		ListCell   *lc;

		foreach(lc, l)
		{
			Node	   *n = lfirst(lc);

			assign_plannode_id_walker(n, ctxt);
		}
		return;
	}

	if (!is_plan_node(node))
		return;

	plan = (Plan *) node;

	plan->plan_node_id = ++ctxt->nextPlanId;

	if (IsA(node, Append))
	{
		ListCell   *cell;
		Append	   *app = (Append *) node;

		foreach(cell, app->appendplans)
			assign_plannode_id_walker((Node *) lfirst(cell), ctxt);
	}
	else if (IsA(node, BitmapAnd))
	{
		ListCell   *cell;
		BitmapAnd  *ba = (BitmapAnd *) node;

		foreach(cell, ba->bitmapplans)
			assign_plannode_id_walker((Node *) lfirst(cell), ctxt);
	}
	else if (IsA(node, BitmapOr))
	{
		ListCell   *cell;
		BitmapOr   *bo = (BitmapOr *) node;

		foreach(cell, bo->bitmapplans)
			assign_plannode_id_walker((Node *) lfirst(cell), ctxt);
	}
	else if (IsA(node, SubqueryScan))
	{
		SubqueryScan *subqscan = (SubqueryScan *) node;

		assign_plannode_id_walker((Node *) subqscan->subplan, ctxt);
	}
	else
	{
		assign_plannode_id_walker((Node *) plan->lefttree, ctxt);
		assign_plannode_id_walker((Node *) plan->righttree, ctxt);
		assign_plannode_id_walker((Node *) plan->initPlan, ctxt);
	}
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
	ShareInputScan *producer;

	Assert(ctxt->producer_count > share_id);
	producer = ctxt->producers[share_id];
	subplan = producer->scan.plan.lefttree;

	foreach(lc, subplan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		Oid			vartype;
		int32		vartypmod;
		char	   *resname;

		vartype = exprType((Node *) tle->expr);
		vartypmod = exprTypmod((Node *) tle->expr);

		/*
		 * We should've filled in tle->resname in shareinput_save_producer().
		 * Note that it's too late to call get_tle_name() here, because this
		 * runs after all the varnos in Vars have already been changed to
		 * INNER/OUTER.
		 */
		resname = tle->resname;
		if (!resname)
			resname = pstrdup("unnamed_attr");

		colnames = lappend(colnames, makeString(resname));
		coltypes = lappend_oid(coltypes, vartype);
		coltypmods = lappend_int(coltypmods, vartypmod);
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
shareinput_mutator_dag_to_tree(Node *node, PlannerGlobal *glob, bool fPop)
{
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
apply_shareinput_dag_to_tree(PlannerGlobal *glob, Plan *plan, List *rtable)
{
	glob->share.curr_rtable = rtable;
	shareinput_walker(shareinput_mutator_dag_to_tree, (Node *) plan, glob);
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
collect_shareinput_producers_walker(Node *node, PlannerGlobal *glob, bool fPop)
{
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
collect_shareinput_producers(PlannerGlobal *glob, Plan *plan, List *rtable)
{
	glob->share.curr_rtable = rtable;
	shareinput_walker(collect_shareinput_producers_walker, (Node *) plan, glob);
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
replace_shareinput_targetlists(PlannerGlobal *glob, Plan *plan, List *rtable)
{
	shareinput_walker(replace_shareinput_targetlists_walker, (Node *) plan, glob);
	return plan;
}

static bool
replace_shareinput_targetlists_walker(Node *node, PlannerGlobal *glob, bool fPop)
{
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
shareinput_mutator_xslice_1(Node *node, PlannerGlobal *glob, bool fPop)
{
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
shareinput_mutator_xslice_2(Node *node, PlannerGlobal *glob, bool fPop)
{
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
shareinput_mutator_xslice_3(Node *node, PlannerGlobal *glob, bool fPop)
{
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
shareinput_mutator_xslice_4(Node *node, PlannerGlobal *glob, bool fPop)
{
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
apply_shareinput_xslice(Plan *plan, PlannerGlobal *glob)
{
	ApplyShareInputContext *ctxt = &glob->share;
	ListCell   *lp;

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
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		shareinput_walker(shareinput_mutator_xslice_1, (Node *) subplan, glob);
	}
	shareinput_walker(shareinput_mutator_xslice_1, (Node *) plan, glob);

	/* Now walk the tree again, and process all the consumers. */
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		shareinput_walker(shareinput_mutator_xslice_2, (Node *) subplan, glob);
	}
	shareinput_walker(shareinput_mutator_xslice_2, (Node *) plan, glob);

	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		shareinput_walker(shareinput_mutator_xslice_3, (Node *) subplan, glob);
	}
	shareinput_walker(shareinput_mutator_xslice_3, (Node *) plan, glob);

	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		shareinput_walker(shareinput_mutator_xslice_4, (Node *) subplan, glob);
	}
	shareinput_walker(shareinput_mutator_xslice_4, (Node *) plan, glob);

	return plan;
}

/*
 * assign_plannode_id - Assign an id for each plan node.  Used by gpmon.
 */
void
assign_plannode_id(PlannedStmt *stmt)
{
	assign_plannode_id_walker_context ctxt;
	ListCell   *lc;

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
 * We can "zap" a Result node if it has an Append node as its left tree and
 * no other complications (initplans, etc.) and its target list is
 * composed of nothing but Var nodes.
 */
static bool
can_zap_result(Result *res)
{
	ListCell   *lc;

	if (!res->plan.lefttree
		|| res->plan.initPlan
		|| !IsA(res->plan.lefttree, Append)
		||res->resconstantqual
		|| res->hashFilter
		)
		return false;

	foreach(lc, res->plan.targetlist)
	{
		TargetEntry *te = (TargetEntry *) lfirst(lc);

		if (!IsA(te->expr, Var))
			return false;
	}

	return true;
}


/* "Zap" eligible Result nodes in the given plan by pushing their quals and
 * targets down onto their Append argument, marking the Append node isZapped,
 * and using it in place of the Result node.
 *
 * XXX Note the call to planner_subplan_put_plan which modifies the
 *     subplan's plan.  This is unfortunate.  We should look for a
 *     better way.
 */
Plan *
zap_trivial_result(PlannerInfo *root, Plan *plan)
{
	if (plan == NULL)
		return NULL;

	if (IsA(plan, SubPlan))
	{
		SubPlan    *subplan = (SubPlan *) plan;
		Plan	   *subplan_plan = planner_subplan_get_plan(root, subplan);

		planner_subplan_put_plan(root, subplan, zap_trivial_result(root, subplan_plan));
		return plan;
	}

	if (IsA(plan, List))
	{
		List	   *l = (List *) plan;
		ListCell   *lc;

		foreach(lc, l)
		{
			lfirst(lc) = zap_trivial_result(root, lfirst(lc));
		}

		return plan;
	}

	if (IsA(plan, Append))
	{
		Append	   *app = (Append *) plan;

		app->appendplans = (List *) zap_trivial_result(root, (Plan *) app->appendplans);
		return plan;
	}

	if (IsA(plan, SubqueryScan))
	{
		SubqueryScan *subq = (SubqueryScan *) plan;

		subq->subplan = zap_trivial_result(root, subq->subplan);
		return plan;
	}

	if (IsA(plan, Result))
	{
		Result	   *res = (Result *) plan;

		plan->lefttree = zap_trivial_result(root, plan->lefttree);
		if (can_zap_result(res))
		{
			Append	   *app = (Append *) plan->lefttree;

			app->plan.targetlist = plan->targetlist;
			app->plan.qual = plan->qual;
			app->isZapped = true;

			/*
			 * pass on the dispatch flag of Result to Append, which may has
			 * been set by cdbparallelize
			 */
			app->plan.dispatch = plan->dispatch;
			return (Plan *) app;
		}
		else
			return plan;
	}

	plan->lefttree = zap_trivial_result(root, plan->lefttree);
	plan->righttree = zap_trivial_result(root, plan->righttree);
	plan->initPlan = (List *) zap_trivial_result(root, (Plan *) plan->initPlan);

	return plan;
}

/*
 * Hash a const value with GPDB's hash function
 */
int32
cdbhash_const(Const *pconst, int iSegments)
{
	CdbHash    *pcdbhash = makeCdbHash(iSegments);

	cdbhashinit(pcdbhash);

	if (pconst->constisnull)
	{
		cdbhashnull(pcdbhash);
	}
	else
	{
		Assert(isGreenplumDbHashable(pconst->consttype));
		Oid			oidType = pconst->consttype;

		if (typeIsArrayType(oidType))
		{
			oidType = ANYARRAYOID;
		}
		cdbhash(pcdbhash, pconst->constvalue, oidType);
	}
	return cdbhashreduce(pcdbhash);
}

/*
 * Hash a list of const values with GPDB's hash function
 */
int32
cdbhash_const_list(List *plConsts, int iSegments)
{
	Assert(0 < list_length(plConsts));

	CdbHash    *pcdbhash = makeCdbHash(iSegments);

	cdbhashinit(pcdbhash);

	ListCell   *lc = NULL;

	foreach(lc, plConsts)
	{
		Const	   *pconst = (Const *) lfirst(lc);

		Assert(IsA(pconst, Const));

		if (pconst->constisnull)
		{
			cdbhashnull(pcdbhash);
		}
		else
		{
			Assert(isGreenplumDbHashable(pconst->consttype));
			Oid			oidType = pconst->consttype;

			if (typeIsArrayType(oidType))
			{
				oidType = ANYARRAYOID;
			}
			cdbhash(pcdbhash, pconst->constvalue, oidType);
		}
	}

	return cdbhashreduce(pcdbhash);
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
			case RTE_SPECIAL:
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
				param_walker(rte->funcexpr, context);
				break;
			case RTE_TABLEFUNCTION:
				param_walker((Node *) rte->subquery, context);
				param_walker(rte->funcexpr, context);
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
		else
		{
			/*
			 * If plan_id is already available in the bitmapset, it means that
			 * there is more than one subplan node which refer to the same
			 * plan_id. In this case create a duplicate subplan, append it to
			 * the glob->subplans and update the plan_id of the subplan to
			 * refer to the new copy of the subplan node
			 */
			PlannerInfo *root = (PlannerInfo *) context->base.node;
			Plan	   *dupsubplan = (Plan *) copyObject(planner_subplan_get_plan(root, subplan));
			int			newplan_id = list_length(root->glob->subplans) + 1;

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
			rte->subquery = makeNode(Query);
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
		newexpr->funcformat = expr->funcformat;
		newexpr->args = args;

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
			simple = (Expr *) makeConst(expr->funcresulttype, -1, resultTypLen,
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
		newexpr->args = args;

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
