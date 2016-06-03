/*-------------------------------------------------------------------------
 *
 * cdbmutate.c
 *	  Parallelize a PostgreSQL sequential plan tree.
 *
 * Copyright (c) 2004-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "parser/analyze.h"		/* for createRandomDistribution() */
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "parser/parse_expr.h"	/* for expr_type() */
#include "parser/parse_oper.h"	/* for compatible_oper_opid() */
#include "utils/relcache.h"     /* RelationGetPartitioningKey() */
#include "optimizer/tlist.h"	/* get_sortgroupclause_tle() */
#include "optimizer/predtest.h"
#include "optimizer/var.h"
#include "parser/parse_relation.h"
#include "utils/lsyscache.h"
#include "utils/datum.h"
#include "utils/syscache.h"
#include "utils/portal.h"
#include "optimizer/clauses.h"
#include "optimizer/planmain.h"
#include "catalog/catquery.h"
#include "nodes/makefuncs.h"

#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "catalog/catalog.h"
#include "catalog/gp_policy.h"
#include "catalog/pg_type.h"

#include "catalog/pg_proc.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbhash.h"        /* isGreenplumDbHashable() */
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
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	int			nextMotionID;
    int         sliceDepth;
    int         numInitPlanMotionNodes;
    List       *initPlans;
}	ApplyMotionState;

typedef struct
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	bool single_row_insert;
} pre_dispatch_function_evaluation_context;

static void
bindCurrentOfParams(char *cursor_name,
					Oid target_relid,
					ItemPointer ctid,
					int *gp_segment_id,
					Oid *tableoid);

static Node *
pre_dispatch_function_evaluation_mutator(Node *node,
										 pre_dispatch_function_evaluation_context * context);

/*
 * Forward Declarations
 */
static void assignMotionID(Node *newnode, ApplyMotionState * context, Node *oldnode);

static int *makeDefaultSegIdxArray(int numSegs);

static Motion *
make_motion(Plan       *lefttree,
            bool        sendSorted,
            int         numSortCols,
            AttrNumber *sortColIdx,
            Oid        *sortOperators,
			bool	   *nullsFirst,
            bool		useExecutorVarFormat);
	
static Node *apply_motion_mutator(Node *node, ApplyMotionState * context);

static void request_explicit_motion(Plan *plan, Index resultRelationIdx, List *rtable);

static void request_explicit_motion_append(Append *append, List *resultRelationsIdx, List *rtable);

static AttrNumber find_segid_column(List *tlist, Index relid);

static bool
doesUpdateAffectPartitionCols(PlannerInfo  *root,
							  Plan         *plan,
                              Query        *query);

/*
 * Given an expression, return true if it contains anything non-constant.
 *
 * an expression is considered "constant" if they consist of constant
 * expressions and non-volatile functions with constant arguments.
 */
static bool
contains_nonconstant_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

    if (IsA(node, Const))
        return false;
    if (contain_volatile_functions(node))
        return true;
    return expression_tree_walker(node, contains_nonconstant_walker, context);
}

/*
 * Is target list all-constant ?
 *
 * That means that there is no subplan and the elements of the
 * targetlist are either constant or guaranteed to produce constants
 * (non-volatile functions with all constant arguments).
 */
static bool
allConstantValuesClause(Plan *node)
{
	ListCell	*cell;
	TargetEntry	*tle;

	Assert(IsA(node, Result));

	if (node->lefttree != NULL)
		return false;

	foreach(cell, node->targetlist)
	{
		tle = (TargetEntry *)lfirst(cell);

		Assert(tle->expr);

		if (contains_nonconstant_walker((Node *)tle->expr, NULL))
			return false;
	}

	return true;
}

static void
directDispatchCalculateHash(Plan *plan, GpPolicy *targetPolicy)
{
	int i;
	CdbHash *h=NULL;
	ListCell *cell=NULL;
	bool directDispatch;

	h = makeCdbHash(GpIdentity.numsegments, HASH_FNV_1);
	cdbhashinit(h);

	/*
	 * the nested loops here seem scary --
	 * especially since we've already
	 * walked them before -- but I think
	 * this is still required since they
	 * may not be in the same order.
	 * (also typically we don't distribute
	 * by more than a handful of
	 * attributes).
	 */
	directDispatch = true;
	for (i = 0; i < targetPolicy->nattrs; i++)
	{
		Const *c;
		TargetEntry *tle = NULL;

		foreach(cell, plan->targetlist)
		{
			tle = (TargetEntry *)lfirst(cell);

			Assert(tle->expr);

			if (tle->resno != targetPolicy->attrs[i])
				continue;

			if (!IsA(tle->expr, Const))
			{
				/* the planner could not simplify this */
				directDispatch = false;
				break;
			}

			c = (Const *)tle->expr;
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
		uint32 hashcode = cdbhashreduce(h);

		plan->directDispatch.contentIds = list_make1_int(hashcode );

		elog(DEBUG1, "sending single row constant insert to content %d", hashcode);
	}
}

/* ------------------------------------------------------------------------- *
 * Function apply_motion() and apply_motion_mutator() add motion nodes to
 * a top-level Plan tree as directed by the Flow nodes in the plan.
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
 * ------------------------------------------------------------------------- *
 */
Plan *
apply_motion(PlannerInfo *root, Plan *plan, Query *query)
{
	Plan				*result;
    ListCell			*cell;
    GpPolicy			*targetPolicy = NULL;
    GpPolicyType		targetPolicyType = POLICYTYPE_ENTRY;
	ApplyMotionState	state;
	bool needToAssignDirectDispatchContentIds = false;

    /* Initialize mutator context. */
	
	planner_init_plan_tree_base(&state.base, root); /* error on attempt to descend into subplan plan */
	state.nextMotionID = 1;		/* Start at 1 so zero will mean "unassigned". */
    state.sliceDepth = 0;
    state.numInitPlanMotionNodes = 0;
    state.initPlans = NIL;

	Assert(is_plan_node((Node *) plan) && IsA(query, Query));
    Assert(plan->flow && plan->flow->flotype != FLOW_REPLICATED);

	/* Does query have a target relation?  (INSERT/DELETE/UPDATE) */
	
	/*
	 *  NOTE: This code makes the assumption that if we are working on a hierarchy of tables,
	 *  all the tables are distributed, or all are on the entry DB.  Any mixture will fail
	 */
    if (query->resultRelation > 0)
    {
	    RangeTblEntry	*rte = rt_fetch(query->resultRelation, query->rtable);
	    Assert(rte->rtekind == RTE_RELATION);

	    targetPolicy = GpPolicyFetch(CurrentMemoryContext, rte->relid);
		targetPolicyType = targetPolicy->ptype;
    }

	switch (query->commandType)
	{
		case CMD_SELECT:
			

			if (query->intoClause)
			{
				List   *hashExpr;
				ListCell *exp1;
				
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
						targetPolicy = palloc0(sizeof(GpPolicy)- sizeof(targetPolicy->attrs) + list_length(hashExpr) * sizeof(targetPolicy->attrs[0]));
					else
						targetPolicy = palloc0(sizeof(GpPolicy));

					targetPolicy->ptype = POLICYTYPE_PARTITIONED;
					targetPolicy->nattrs = 0;
					
					if(hashExpr)
						foreach(exp1, hashExpr)
						{
							AttrNumber	n;
							bool found_expr = false;

							
							/* See if this Expr is a column of the result table */
						
							for(n=1;n<=list_length(plan->targetlist);n++)
							{
								Var		   *new_var = NULL;
							
								TargetEntry *target = get_tle_by_resno(plan->targetlist, n);
							
								if (!target->resjunk)
								{
									Expr *var1 = (Expr *) lfirst(exp1);

									/* right side variable may be encapsulated by a relabel node. motion, however, does not care about relabel nodes. */
									if (IsA(var1, RelabelType))
										var1 = ((RelabelType *)var1)->arg;
									
									/* If subplan expr is a Var, copy to preserve its EXPLAIN info. */
									if (IsA(target->expr, Var))
									{
										new_var = copyObject(target->expr);
										new_var->varno = OUTER;
										new_var->varattno = n;
									}
					
									/*
									 * Make a Var that references the target list entry at this offset,
									 * using OUTER as the varno
									 */
									else
										new_var = makeVar(OUTER,
														  n,
														  exprType((Node *) target->expr),
														  exprTypmod((Node *) target->expr),
														  0);
					
									if (equal(var1,new_var))
									{
										/* If it is, use it to partition the result table, to avoid
										 * unnecessary redistibution of data */
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
						int i;
						for (i = 0; i < list_length(plan->targetlist); i++)
						{
							TargetEntry *target = 
								get_tle_by_resno(plan->targetlist, i + 1);

							if (target)
							{
								Oid typeOid = exprType((Node *) target->expr);
								
								if(isGreenplumDbHashable(typeOid))
								{
									targetPolicy->attrs[targetPolicy->nattrs++] = i+1;
									break;
								}
							}
						}
					}
					
					if (query->intoPolicy == NULL)
					{
						char * columns;
						int i;
						columns = palloc((NAMEDATALEN+3)*targetPolicy->nattrs+1);
						columns[0] = '\0';
						for (i=0;i<targetPolicy->nattrs;i++)
						{
							TargetEntry *target = get_tle_by_resno(plan->targetlist, targetPolicy->attrs[i]);
							if (i>0)
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
                 * partitioning key of the target relation.	Since this
                 * is a SELECT INTO (basically same as an INSERT) command, the target list will correspond
                 * to the attributes of the target relation in order.
	             */
                hashExpr = getExprListFromTargetList(plan->targetlist,
                                                     targetPolicy->nattrs,
                                                     targetPolicy->attrs,
                                                     true);
                if (!repartitionPlan(plan, false, false, hashExpr))
                    ereport(ERROR, (errcode(ERRCODE_CDB_FEATURE_NOT_YET),
                                    errmsg("Cannot parallelize that SELECT INTO yet")
								));

                Assert(query->intoPolicy->ptype == POLICYTYPE_PARTITIONED);

			}
			
			if (plan->flow->flotype == FLOW_PARTITIONED && !query->intoClause)
			{
			    /*
                 * Query result needs to be brought back to the QD.
                 * Ask for motion to a single QE.  Later, apply_motion
                 * will override that to bring it to the QD instead.
                 *
                 * If the query has an ORDER BY clause, use Merge Receive to
                 * preserve the ordering.
                 *
                 * The plan has already been set up to ensure each qExec's 
                 * result is properly ordered according to the ORDER BY 
                 * specification.  The existing ordering could be stronger 
                 * than required; if so, omit the extra trailing columns 
                 * from the Merge Receive key.
                 *
                 * An unordered result is ok if the ORDER BY ordering is
                 * degenerate (on constant exprs) or the result is known to
                 * have at most one row. 
                 */
                if (query->sortClause &&
                    plan->flow->numSortCols > 0)
                {
                    if (plan->flow->numSortCols > list_length(query->sortClause))
                        plan->flow->numSortCols = list_length(query->sortClause);

                    Insist(focusPlan(plan, true, false));
                }
                else if (plan->flow->numOrderbyCols > 0 && plan->flow->numSortCols > 0)
                {
                    if (plan->flow->numSortCols > plan->flow->numOrderbyCols)
                        plan->flow->numSortCols = plan->flow->numOrderbyCols;

                    Insist(focusPlan(plan, true, false));
                }

                /* Use UNION RECEIVE.  Does not preserve ordering. */
                else
                    Insist(focusPlan(plan, false, false));
			}
			needToAssignDirectDispatchContentIds = root->config->gp_enable_direct_dispatch && ! query->intoClause;
			break;

		case CMD_INSERT:
		{
			switch (targetPolicyType)
			{
				case POLICYTYPE_PARTITIONED:
				{
					List   *hashExpr = NIL;

					/*
					 * Make sure the top level flow is partitioned on the
					 * partitioning key of the target relation.	Since this
					 * is an INSERT command, the target list will correspond
					 * to the attributes of the target relation in order.
					 */
					

					/* Is this plan an all-const insert ? */
					if (gp_enable_fast_sri && IsA(plan, Result))
					{
						ListCell *cell;
						bool	typesOK=true;
						PartitionNode *pn;
						RangeTblEntry	*rte;
						Relation		rel;
		
						rte = rt_fetch(query->resultRelation, query->rtable);
						rel = relation_open(rte->relid, NoLock);

						/* 1: See if it's partitioned */
						pn = RelationBuildPartitionDesc(rel, false);

						if (pn && !partition_policies_equal(targetPolicy, pn))
						{
							/* 2: See if partitioning columns are constant */
							List *partatts = get_partition_attrs(pn);
							ListCell *lc;
							bool all_const = true;

							foreach(lc, partatts)
							{
								List *tl = plan->targetlist;
								ListCell *cell;
								AttrNumber attnum = lfirst_int(lc);
								bool found = false;

								foreach(cell, tl)
								{
									TargetEntry *tle = (TargetEntry *)lfirst(cell);

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
									planner_make_plan_constant(root, (Node *)plan->targetlist, true);

							/* better be constant now */
							if (allConstantValuesClause(plan))
							{
								bool *nulls;
								Datum *values;
								EState *estate = CreateExecutorState();
								ResultRelInfo *rri;

								/* 4: build tuple, look up partitioning key */
								nulls = palloc0(sizeof(bool) * rel->rd_att->natts);
								values = palloc(sizeof(Datum) * rel->rd_att->natts);

								foreach(lc, partatts)
								{
									AttrNumber attnum = lfirst_int(lc);
									List *tl = plan->targetlist;
									ListCell *cell;

									foreach(cell, tl)
									{
										TargetEntry *tle = (TargetEntry *)lfirst(cell);

										Assert(tle->expr);

										if (tle->resno == attnum)
										{
											Assert(IsA(tle->expr, Const));
												
											nulls[attnum - 1] = ((Const *)tle->expr)->constisnull;
											if (!nulls[attnum - 1])
												values[attnum - 1] = ((Const *)tle->expr)->constvalue;
										}
									}
								}
								estate->es_result_partitions = pn;
								estate->es_partition_state =
									createPartitionState(estate->es_result_partitions, 1 /*resultPartSize */);

								rri = makeNode(ResultRelInfo);
								rri->ri_RangeTableIndex = 1;      /* dummy */
								rri->ri_RelationDesc = rel;

								estate->es_result_relations = rri;
								estate->es_num_result_relations = 1;
								estate->es_result_relation_info = rri;
								rri = values_get_partition(values, nulls, RelationGetDescr(rel),
														   estate);

								/* 5: get target policy for destination table */
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
							Expr	*elem = NULL;
							Oid		 att_type = InvalidOid;
							elem = (Expr *)lfirst(cell);
							att_type = exprType((Node *) elem);
							Assert(att_type != InvalidOid);
							if (!isGreenplumDbHashable(att_type))
							{
								typesOK=false;
								break;
							}
						}

						/* all constants in values clause -- no need to repartition. */
						if (typesOK && allConstantValuesClause(plan))
						{
							Result	*rNode = (Result *)plan;
							List	*hList = NIL;
							int		i;

							/* 
							 * If this table has child tables, we need to
							 * find out destination partition.
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
								/* we now either have a hash-code, or we've marked the plan non-directed. */
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
						ereport(ERROR, (errcode(ERRCODE_CDB_FEATURE_NOT_YET),
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
						ereport(ERROR, (errcode(ERRCODE_CDB_FEATURE_NOT_YET),
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
				 * The planner does not support updating any of the partitioning columns.
				 */
				if (query->commandType == CMD_UPDATE &&
					doesUpdateAffectPartitionCols(root, plan,query))
				{
					ereport(ERROR, (errcode(ERRCODE_CDB_FEATURE_NOT_YET),
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
					int relid = list_nth_int(root->resultRelations, 0);
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
					 * target table is master-only but flow is distributed:
					 * add a GatherMotion on top
					 */
					
					/* create a shallow copy of the plan flow */
					Flow *flow = plan->flow;
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
			Insist(0);		/* Never see non-DML in here! */
	}

	result = (Plan *) apply_motion_mutator((Node *) plan, &state);

	if ( needToAssignDirectDispatchContentIds )
	{
		/* figure out if we can run on a reduced set of nodes */
		AssignContentIdsToPlanData(query, result, root);
	}

	Assert(result->nMotionNodes == state.nextMotionID - 1);
    Assert(result->nInitPlans == list_length(state.initPlans));

    /* Assign slice numbers to the initplans. */
    foreach(cell, state.initPlans)
    {
        SubPlan    *subplan = (SubPlan *)lfirst(cell);

        Assert(IsA(subplan, SubPlan) && subplan->qDispSliceId == 0);
        subplan->qDispSliceId = state.nextMotionID++;
    }

    /* 
	 * Discard subtrees of Query node that aren't needed for execution. 
	 * Note the targetlist (query->targetList) is used in execution of
	 * prepared statements, so we leave that alone.
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
apply_motion_mutator(Node *node, ApplyMotionState * context)
{
	Node   *newnode;
    Plan   *plan;
    Flow   *flow;
    int     saveNextMotionID;
    int     saveNumInitPlans;
    int     saveSliceDepth;

	if (node == NULL)
		return NULL;
	
    /* An expression node might have subtrees containing plans to be mutated. */
    if (!is_plan_node(node))
		return plan_tree_mutator(node, apply_motion_mutator, context);

    plan = (Plan *)node;
    flow = plan->flow;

    saveNextMotionID = context->nextMotionID;
    saveNumInitPlans = list_length(context->initPlans);

    /* Descending into a subquery or a new slice? */
    saveSliceDepth = context->sliceDepth;
	Assert( !IsA(node,Query) );
    if (IsA(node, Motion))
        context->sliceDepth++;
    else if (flow && flow->req_move != MOVEMENT_NONE )
        context->sliceDepth++;

	/*
	 * Copy this node and mutate its children. Afterward, this node should be
	 * an exact image of the input node, except that contained nodes requiring
	 * parallelization will have had it applied.
	 */
    newnode = plan_tree_mutator(node, apply_motion_mutator, context);

    context->sliceDepth = saveSliceDepth;

    plan = (Plan *)newnode;
    flow = plan->flow;

    /* Make one big list of all of the initplans. */
    if (plan->initPlan)
    {
        ListCell   *cell;
        SubPlan    *subplan;
        int         nMotion = 0;

        foreach(cell, plan->initPlan)
        {
        	PlannerInfo *root = (PlannerInfo *) context->base.node;
            subplan = (SubPlan *)lfirst(cell);
            Assert(IsA(subplan, SubPlan));
            Assert(root);
            Assert(planner_subplan_get_plan(root, subplan));
            nMotion += planner_subplan_get_plan(root, subplan)->nMotionNodes;
            context->initPlans = lappend(context->initPlans, subplan);
        }

        /* Keep track of the number of Motion nodes found in Init Plans. */
        context->numInitPlanMotionNodes += nMotion;
    }

    /* Pre-existing Motion nodes must be renumbered. */
    if (IsA(newnode, Motion))
    {
        Motion *motion = (Motion *)newnode;

#ifdef USE_ASSERT_CHECKING
		/* Sanity check */
		/* Sub plan must have flow */
        Assert(flow && motion->plan.lefttree->flow);
		Assert(flow->req_move == MOVEMENT_NONE && !flow->flow_before_req_move);
#endif

        /* Assign unique node number to the new node. */
        assignMotionID(newnode, context, node);

        /* If top slice marked as singleton, make it a dispatcher singleton. */
		if(motion->motionType == MOTIONTYPE_FIXED
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

            if (flow->numSortCols > 0)
            {
                newnode = (Node *)make_sorted_union_motion(plan,
                                                           flow->segindex,
                                                           flow->numSortCols,
                                                           flow->sortColIdx,
                                                           flow->sortOperators,
                                                           flow->nullsFirst,
                                                           true /* useExecutorVarFormat */);
            }
            else
            {
                newnode = (Node *)make_union_motion(plan,
                                                    flow->segindex,
                                                    true /* useExecutorVarFormat */);
            }
            break;

        case MOVEMENT_BROADCAST:
            newnode = (Node *) make_broadcast_motion(plan, true /* useExecutorVarFormat */);
            break;

        case MOVEMENT_REPARTITION:
            newnode = (Node *)make_hashed_motion(plan,
                                                 flow->hashExpr,
                                                 true /* useExecutorVarFormat */
												);
            break;

        case MOVEMENT_EXPLICIT:
        	/* add an ExplicitRedistribute motion node only if child plan nodes
        	 * have a motion node
        	 */ 
        	if (context->nextMotionID - context->numInitPlanMotionNodes - saveNextMotionID > 0)
        	{
        		/* motion node in child nodes: add a ExplicitRedistribute motion */
				newnode = (Node *) make_explicit_motion(plan,
													 	flow->segidColIdx,
													 	true /* useExecutorVarFormat */
													 	);
        	}
        	else
        	{
        		/* no motion nodes in child plan nodes - no need for ExplicitRedistribute:
        		 * restore flow */
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
               plan == ((Motion *)newnode)->plan.lefttree);

        /* Assign unique node number to the new node. */
        assignMotionID(newnode, context, NULL);

        /* Replace the subplan's modified Flow with the original. */
        plan->flow = flow->flow_before_req_move;
    }
    else
        Assert(flow->req_move == MOVEMENT_NONE);

done:
    /*
     * Label the Plan node with the number of Motion nodes and Init Plans
     * in this subtree, inclusive of the node itself.  This info is used
     * only in the top node of a query or subquery.  Someday, find a better
     * place to keep it.
     */
    plan = (Plan *)newnode;
    plan->nMotionNodes = context->nextMotionID - saveNextMotionID;
    plan->nInitPlans = list_length(context->initPlans) - saveNumInitPlans;
	return newnode;
}                               /* apply_motion_mutator */


/*
 * Helper code for ApplyMotionState -- Assign motion id to new Motion node.
 */
void
assignMotionID(Node *newnode, ApplyMotionState * context, Node *oldnode)
{
	Assert(IsA(newnode, Motion) && context != NULL);
	Assert(oldnode == NULL || (IsA(oldnode, Motion) && oldnode != newnode));

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


/* --------------------------------------------------------------------
 * make_motion -- creates a Motion node.
 * Caller must have built the pHashDefn, pFixedDefn,
 * and pSortDefn structs already.
 * useExecutorVarFormat is true if make_motion is called after setrefs
 * This call only make a motion node, without filling in flow info
 * After calling this function, caller need to call add_slice_to_motion
 * --------------------------------------------------------------------
 */
Motion *
make_motion(Plan       *lefttree,
            bool        sendSorted,
			int         numSortCols,
            AttrNumber *sortColIdx,
            Oid        *sortOperators,
			bool	   *nullsFirst,
            bool        useExecutorVarFormat)
{
    Motion *node = makeNode(Motion);
    Plan   *plan = &node->plan;

	Assert(lefttree);
	Assert(!IsA(lefttree, Motion));
	AssertImply(sendSorted, (numSortCols > 0 && sortColIdx != NULL && sortOperators != NULL));
	AssertImply(!sendSorted, (numSortCols == 0 && sortColIdx == NULL && sortOperators == NULL));

	plan->startup_cost = lefttree->startup_cost;
	plan->total_cost = lefttree->total_cost;
	plan->plan_rows = lefttree->plan_rows;
	plan->plan_width = lefttree->plan_width;

	plan->targetlist = cdbpullup_targetlist(lefttree, useExecutorVarFormat);
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;
	plan->dispatch = DISPATCH_PARALLEL;

	node->sendSorted = sendSorted;

	node->numSortCols = numSortCols;
	node->sortColIdx = sortColIdx;
	node->sortOperators = sortOperators;
	node->nullsFirst = nullsFirst;
	Assert(numSortCols == 0 || nullsFirst);

	plan->extParam = bms_copy(lefttree->extParam);
	plan->allParam = bms_copy(lefttree->allParam);

	plan->flow = NULL;

	node->outputSegIdx = NULL;
	node->numOutputSegs = 0;
	
	return node;
}

void add_slice_to_motion(Motion *motion,
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
        List       *eq = list_make1(makeString("="));
        ListCell   *cell;
        foreach(cell, hashExpr)
        {
            Node   *expr = (Node *)lfirst(cell);
            Oid     typeoid = exprType(expr);
            Oid	    eqopoid;
            Oid     lefttype;
            Oid     righttype;

            /* Get oid of the equality operator for this data type. */
            eqopoid = compatible_oper_opid(eq, typeoid, typeoid, true);
            if ( eqopoid == InvalidOid )
                ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
                                errmsg("no equality operator for typid %d",
                                       typeoid)));

            /* Get the equality operator's operand type. */
            op_input_types( eqopoid, &lefttype, &righttype );
            Assert( lefttype == righttype );

			/* If this type is a domain type, get its base type. */
			if (get_typtype(lefttype) == 'd')
				lefttype = getBaseType(lefttype);

            motion->hashDataTypes = lappend_oid(motion->hashDataTypes, lefttype);
        }
        list_free_deep(eq);
    }


	/* Attach a descriptive Flow. */
	switch(motion->motionType)
	{
		case MOTIONTYPE_HASH:
			motion->plan.flow = makeFlow(FLOW_PARTITIONED);
			motion->plan.flow->locustype = CdbLocusType_Hashed;
			motion->plan.flow->hashExpr = copyObject(motion->hashExpr);
			motion->numOutputSegs = getgpsegmentCount();
			motion->outputSegIdx = makeDefaultSegIdxArray(getgpsegmentCount());

			break;
		case MOTIONTYPE_FIXED:
			if (motion->numOutputSegs == 0 )
			{
				/* broadcast */
				motion->plan.flow = makeFlow(FLOW_REPLICATED);
				motion->plan.flow->locustype = CdbLocusType_Replicated;

			}
			else if (motion->numOutputSegs == 1 )
			{
				/* Focus motion */
				motion->plan.flow = makeFlow(FLOW_SINGLETON);
				motion->plan.flow->segindex = motion->outputSegIdx[0];
				motion->plan.flow->locustype = ( motion->plan.flow->segindex<0 ) ?
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
			/* TODO: antova - Nov 18, 2010; add a special locus type for ExplicitRedistribute flows */ 
			motion->plan.flow->locustype = CdbLocusType_Strewn;
			break;
		default:
			Assert(!"Invalid motion type");
			motion->plan.flow = NULL;
			break;
	}

	Assert(motion->plan.flow);

	/* if the motion has a sort order, preserve it in each route */
	if(motion->sendSorted && motion->numSortCols > 0)
	{
		int i, n;
		n = motion->numSortCols;
		motion->plan.flow->numSortCols = n;
		motion->plan.flow->sortColIdx = palloc(n * sizeof(AttrNumber));
		motion->plan.flow->sortOperators = palloc (n * sizeof(Oid));
		motion->plan.flow->nullsFirst = palloc (n * sizeof(bool));
		for ( i = 0; i < n; i++ )
		{
			motion->plan.flow->sortColIdx[i] = motion->sortColIdx[i];
			motion->plan.flow->sortOperators[i] = motion->sortOperators[i];
			motion->plan.flow->nullsFirst[i] = motion->nullsFirst[i];
		}
	}
}

Motion *
make_union_motion(Plan *lefttree, int destSegIndex, bool useExecutorVarFormat)
{
	Motion 	*motion;
	int		*outSegIdx = (int *) palloc(sizeof(int)); 
	outSegIdx[0] = destSegIndex; 

	motion = make_motion(lefttree, false,
						 0, NULL, NULL, NULL, /* numSortCols, sortColIdx, sortOperators, nullsFirst */
						 useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_FIXED, 
			NULL, 1, outSegIdx);
	return motion;
}

Motion *
make_sorted_union_motion(Plan *lefttree,
                         int destSegIndex,
						 int numSortCols, AttrNumber *sortColIdx,
						 Oid *sortOperators, bool *nullsFirst,
						 bool useExecutorVarFormat)
{
	Motion 	*motion;
	int		*outSegIdx = (int *) palloc(sizeof(int)); 
	outSegIdx[0] = destSegIndex; 

	motion = make_motion(lefttree, true,
						 numSortCols, sortColIdx, sortOperators, nullsFirst,
						 useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_FIXED,
			NULL, 1, outSegIdx); 
	return motion;
}

Motion *
make_hashed_motion(Plan *lefttree,
				   List *hashExpr, bool useExecutorVarFormat)
{
	Motion *motion;

	motion = make_motion(lefttree, false,
						 0, NULL, NULL, NULL, useExecutorVarFormat);
	add_slice_to_motion(motion, MOTIONTYPE_HASH, 
			hashExpr,
			0, NULL);
	return motion;
}

Motion *
make_broadcast_motion(Plan *lefttree, bool useExecutorVarFormat)
{
	Motion *motion;

	motion = make_motion(lefttree, false,
						 0, NULL, NULL, NULL,
						 useExecutorVarFormat);

	add_slice_to_motion(motion, MOTIONTYPE_FIXED,
			NULL, 0, NULL);
	return motion;
}

Motion *
make_explicit_motion(Plan *lefttree, AttrNumber segidColIdx, bool useExecutorVarFormat)
{
	Motion *motion;

	motion = make_motion(lefttree, false,
						 0, NULL, NULL, NULL, /* numSortCols, sortColIdx, sortOperators, nullsFirst */
						 useExecutorVarFormat);

	Assert(segidColIdx > 0 && segidColIdx <= list_length(lefttree->targetlist));
	
	motion->segidColIdx = segidColIdx;
	
	add_slice_to_motion(motion, MOTIONTYPE_EXPLICIT,
			NULL, 0, NULL); /* hashExpr, numOutputSegs, outputSegIdx */
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
	 * The following assumes that the default is to have segindexes
	 * numbered sequentially starting at 0.
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
getExprListFromTargetList(List         *tlist,
                          int           numCols,
                          AttrNumber   *colIdx,
                          bool          useExecutorVarFormat)
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
isAnyColChangedByUpdate(Index       targetvarno,
                        List       *targetlist,
                        int         nattrs,
                        AttrNumber *attrs)
{
	/*
	 * Want to test whether an update statement possibly changes the
	 * value of any of the partitioning columns.
	 */

	/*
	 * For each partitioning column, the TargetListEntry for that varattno
	 * should be just a Var node with the same varattno.
	 */
	int     i;

	for (i = 0; i < nattrs; i++)
	{
		TargetEntry    *tle = get_tle_by_resno(targetlist, attrs[i]);
		Var		       *var;

		Insist(tle);

		if (!IsA(tle->expr, Var))
			return true;

		var = (Var *)tle->expr;
		if (var->varnoold != targetvarno ||
            var->varoattno != attrs[i])
			return true;
	}

	return false;
}                               /* isAnyColChangedByUpdate */

/*
 * Request an ExplicitRedistribute motion node for a plan node
 */
void request_explicit_motion(Plan *plan, Index resultRelationsIdx, List *rtable)
{
	/* request a segid redistribute motion */
	/* create a shallow copy of the plan flow */
	Flow *flow = plan->flow;
	plan->flow = (Flow *) palloc(sizeof(*(plan->flow)));
	*(plan->flow) = *flow;
	
	/* save original flow information */
	plan->flow->flow_before_req_move = flow;
	
	/* request a SegIdRedistribute motion node */
	plan->flow->req_move = MOVEMENT_EXPLICIT;
	
	/* find segid column in target list */
	AttrNumber segidColIdx = find_segid_column(plan->targetlist, resultRelationsIdx);
	
	Assert(-1 != segidColIdx);
	
	plan->flow->segidColIdx = segidColIdx;
}

/*
 * Request SegIdRedistribute motion node for an Append plan updating a partitioned
 * table
 */
void request_explicit_motion_append(Append *append, List *resultRelationsIdx, List *rtable)
{
	Assert(IsA(append, Append));
	Assert(list_length(append->appendplans) == list_length(resultRelationsIdx));
	
	ListCell *lcAppendPlan = NULL;
	ListCell *lcResultRel = NULL;
	
	forboth(lcAppendPlan, append->appendplans,
			lcResultRel, resultRelationsIdx)
	{
		/* request a segid redistribute motion node to be put above each
		 * append plan */ 
		Plan *appendChildPlan = (Plan *) lfirst(lcAppendPlan);
		int relid = lfirst_int(lcResultRel);
		Assert(relid > 0);
		request_explicit_motion(appendChildPlan, relid, rtable);
	}
	
}


/*
 * Find the index of the segid column of the requested relation (relid) in the 
 * target list
 */
AttrNumber find_segid_column(List *tlist, Index relid)
{	
	if (NIL == tlist)
	{
		return -1;
	}
	
	ListCell *lcte = NULL;
	
	foreach(lcte, tlist)
	{
		TargetEntry *te = (TargetEntry *) lfirst(lcte);
		
		Var *var = (Var *) te->expr;
		if (!IsA(var, Var))
		{
			continue;
		}
		
		if ((var->varno    == relid && var->varattno  == GpSegmentIdAttributeNumber) ||
			(var->varnoold == relid && var->varoattno == GpSegmentIdAttributeNumber))
		{
			return te->resno;
		}
	}
	
	// no segid column found
	return -1;
}

/*
 * doesUpdateAffectPartitionCols
 */
bool
doesUpdateAffectPartitionCols(PlannerInfo  *root,
							  Plan         *plan,
                              Query        *query)
{
    Append     *append;
    ListCell   *rticell;
    ListCell   *plancell;
    GpPolicy  *policy;

    if (list_length(root->resultRelations) == 1)
    {
	    RangeTblEntry  *rte;
        Relation        relation;
		int				resultRelation = linitial_int(root->resultRelations);
		
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
     * Multiple target relations (inheritance)...
     * Plan should have an Append node on top, with a subplan per target.
     */
    append = (Append *)plan;

	Insist(IsA(append, Append) &&
	       append->isTarget &&
           list_length(append->appendplans) == list_length(root->resultRelations));

    forboth(rticell, root->resultRelations,
            plancell, append->appendplans)
    {
        int     rti = lfirst_int(rticell);
        Plan   *subplan = (Plan *)lfirst(plancell);

        RangeTblEntry  *rte;
        Relation        relation;

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
}                               /* doesUpdateAffectPartitionCols */



/* ----------------------------------------------------------------------- *
 * cdbmutate_warn_ctid_without_segid() warns the user if the plan refers to a
 * partitioned table's ctid column without also referencing its
 * gp_segment_id column.
 * ----------------------------------------------------------------------- *
 */
typedef struct ctid_inventory_context
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	bool        uses_ctid;
    bool        uses_segid;
    Index       relid;
} ctid_inventory_context;

static bool
ctid_inventory_walker(Node *node, ctid_inventory_context *inv)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
        Var    *var = (Var *)node;

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
	ctid_inventory_context  inv;
    Relids                  relids_to_ignore;
    ListCell               *cell;
    AttrNumber              attno;
    int                     ndx;
	
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
        RestrictInfo   *rinfo = (RestrictInfo *)lfirst(cell);

        Assert(IsA(rinfo, RestrictInfo));

        ctid_inventory_walker((Node *)rinfo->clause, &inv);
    }

    /* Notify client if found a reference to ctid only, without gp_segment_id */
    if (inv.uses_ctid && 
        !inv.uses_segid)
    {
        RangeTblEntry  *rte = rt_fetch(rel->relid, root->parse->rtable);
        const char     *cmd;
        int             elevel;

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
}                               /* cdbmutate_warn_ctid_without_segid */


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
typedef bool (*SHAREINPUT_MUTATOR) (Node *node, ApplyShareInputContext *ctxt, bool fPop);
static void shareinput_walker(SHAREINPUT_MUTATOR f, Node *node, ApplyShareInputContext *ctxt)
{
	Plan *plan = NULL; 
	bool recursive_down;

	if(node == NULL)
		return;

	if(IsA(node, List))
	{
		List *l = (List *) node;
		ListCell *lc;
		foreach(lc, l)
		{
			Node* n = lfirst(lc);
			shareinput_walker(f, n, ctxt);
		}
		return;
	}

	if(!is_plan_node(node))
		return;

	plan = (Plan *) node;
	recursive_down = (*f)(node, ctxt, false);

	if(recursive_down) 
	{
		if(IsA(node, Append))
		{
			ListCell *cell;
			Append *app = (Append *) node;
			foreach(cell, app->appendplans)
				shareinput_walker(f, (Node *)lfirst(cell), ctxt);
		}
		else if(IsA(node, SubqueryScan))
		{
			SubqueryScan *subqscan = (SubqueryScan *) node;
			shareinput_walker(f, (Node *) subqscan->subplan, ctxt);
		}
		else if(IsA(node, BitmapAnd))
		{
			ListCell *cell;
			BitmapAnd *ba = (BitmapAnd *) node;
			foreach(cell, ba->bitmapplans)
				shareinput_walker(f, (Node *)lfirst(cell), ctxt);
		}
		else if(IsA(node, BitmapOr))
		{
			ListCell *cell;
			BitmapOr *bo = (BitmapOr *) node;
			foreach(cell, bo->bitmapplans)
				shareinput_walker(f, (Node *)lfirst(cell), ctxt);
		}
		else if(IsA(node, NestLoop))
		{
			/* Nest loop join is strange.  Exec order depends on prefetch_inner */
			NestLoop *nl = (NestLoop *) node;
			if(nl->join.prefetch_inner)
			{
				shareinput_walker(f, (Node *)plan->righttree, ctxt);
				shareinput_walker(f, (Node *)plan->lefttree, ctxt);
			}
			else	
			{
				shareinput_walker(f, (Node *)plan->lefttree, ctxt);
				shareinput_walker(f, (Node *)plan->righttree, ctxt);
			}
		}
		else if(IsA(node, HashJoin))
		{
			/* Hash join the hash table is at inner */
			shareinput_walker(f, (Node *)plan->righttree, ctxt);
			shareinput_walker(f, (Node *)plan->lefttree, ctxt);
		}
		else if (IsA(node, MergeJoin))
		{
			MergeJoin *mj = (MergeJoin *) node;
			if (mj->unique_outer)
			{
				shareinput_walker(f, (Node *)plan->lefttree, ctxt);
				shareinput_walker(f, (Node *)plan->righttree, ctxt);
			}
			else
			{
				shareinput_walker(f, (Node *)plan->righttree, ctxt);
				shareinput_walker(f, (Node *)plan->lefttree, ctxt);
			}
		}
		else if (IsA(node, Sequence))
		{
			ListCell *cell = NULL;
			Sequence *sequence = (Sequence *) node;
			foreach(cell, sequence->subplans)
			{
				shareinput_walker(f, (Node *)lfirst(cell), ctxt);
			}
		}
		else
		{
			shareinput_walker(f, (Node *)plan->lefttree, ctxt);
			shareinput_walker(f, (Node *)plan->righttree, ctxt);
			shareinput_walker(f, (Node *)plan->initPlan, ctxt);
		}
	}

	(*f)(node, ctxt, true);
}

static void leftdeep_walker(SHAREINPUT_MUTATOR f, Node *node, ApplyShareInputContext *ctxt)
{
	Plan *plan = NULL; 
	bool recursive_down;

	if(node == NULL)
		return;

	if(IsA(node, List))
	{
		List *l = (List *) node;
		ListCell *lc;
		foreach(lc, l)
		{
			Node* n = lfirst(lc);
			leftdeep_walker(f, n, ctxt);
		}
		return;
	}

	if(!is_plan_node(node))
		return;

	plan = (Plan *) node;
	recursive_down = (*f)(node, ctxt, false);

	if(recursive_down) 
	{
		if(IsA(node, Append))
		{
			ListCell *cell;
			Append *app = (Append *) node;
			foreach(cell, app->appendplans)
				leftdeep_walker(f, (Node *)lfirst(cell), ctxt);
		}
		else if(IsA(node, BitmapAnd))
		{
			ListCell *cell;
			BitmapAnd *ba = (BitmapAnd *) node;
			foreach(cell, ba->bitmapplans)
				leftdeep_walker(f, (Node *)lfirst(cell), ctxt);
		}
		else if(IsA(node, BitmapOr))
		{
			ListCell *cell;
			BitmapOr *bo = (BitmapOr *) node;
			foreach(cell, bo->bitmapplans)
				leftdeep_walker(f, (Node *)lfirst(cell), ctxt);
		}
		else if(IsA(node, SubqueryScan))
		{
			SubqueryScan *subqscan = (SubqueryScan *) node;
			leftdeep_walker(f, (Node *) subqscan->subplan, ctxt);
		}
		else
		{
			leftdeep_walker(f, (Node *)plan->lefttree, ctxt);
			leftdeep_walker(f, (Node *)plan->righttree, ctxt);
			leftdeep_walker(f, (Node *)plan->initPlan, ctxt);
		}
	}

	(*f)(node, ctxt, true);
}

/* 
 * DAG to Tree
 * Zap children if I am not the first sharer (not recursive down).  
 * Assign share_id to both ShareInputScan and Material/Sort.
 */
static bool shareinput_mutator_dag_to_tree(Node *node, ApplyShareInputContext* ctxt, bool fPop)
{
	Plan *plan = (Plan *) node;

	if(fPop)
		return true;

	if(IsA(plan, ShareInputScan))
	{
		Plan *subplan = plan->lefttree;

		Assert(subplan);
		Assert(IsA(subplan, Material) || IsA(subplan, Sort));
		Assert(get_plan_share_id(plan) == SHARE_ID_NOT_ASSIGNED);
		Assert(plan->righttree == NULL);
		Assert(plan->initPlan == NULL);

		if(list_member_ptr(ctxt->sharedNodes, subplan))
		{
			Assert(get_plan_share_id(subplan) >= 0);
			set_plan_share_id(plan, get_plan_share_id(subplan));
			plan->lefttree = NULL;
			return false;
		}
		else
		{
			Assert(get_plan_share_id(subplan) == SHARE_ID_NOT_ASSIGNED);
			set_plan_share_id(subplan, list_length(ctxt->sharedNodes));
			set_plan_share_id(plan, get_plan_share_id(subplan));
			ctxt->sharedNodes = lappend(ctxt->sharedNodes, subplan);
			return true;
		}
	}

	return true;
}

/* 
 * Apply the share input mutator.
 */
Plan *apply_shareinput_dag_to_tree(Plan *plan, ApplyShareInputContext *ctxt)
{
	shareinput_walker(shareinput_mutator_dag_to_tree, (Node *) plan, ctxt);
	return plan;
}

/* Some helper: implements a stack using List. */
static void shareinput_pushmot(ApplyShareInputContext *ctxt, int motid)
{
	ctxt->motStack = lcons_int(motid, ctxt->motStack);
}
static void shareinput_popmot(ApplyShareInputContext *ctxt)
{
	list_delete_first(ctxt->motStack);
}
static int shareinput_peekmot(ApplyShareInputContext *ctxt)
{
	return linitial_int(ctxt->motStack);
}

typedef struct ShareNodeWithSliceMark
{
	Plan *plan;
	int slice_mark;
} ShareNodeWithSliceMark;

static bool shareinput_find_sharenode(ApplyShareInputContext *ctxt, int share_id, ShareNodeWithSliceMark *result)
{
	ListCell *lc;
	ListCell *lc_slicemark;
	
	if(ctxt->sharedNodes == NULL)
		return false;

	forboth(lc, ctxt->sharedNodes, lc_slicemark, ctxt->sliceMarks)
	{
		Plan *plan = (Plan *) lfirst(lc);

		Assert(IsA(plan, Material) || IsA(plan, Sort));
		if(get_plan_share_id(plan) == share_id)
		{
			if(result)
			{
				result->plan = plan;
				result->slice_mark = lfirst_int(lc_slicemark);
			}
			return true;
		}
	}

	return false;
}

/* 
 * First walk on shareinput xslice.  It does the following:
 *
 * 1. Build the sharedNodes in context.
 * 2. Build the sliceMarks in context.
 * 3. Build a list a share on QD
 */
static bool
shareinput_mutator_xslice_1(Node* node, ApplyShareInputContext *ctxt, bool fPop)
{
	Plan *plan = (Plan *) node;

	if(fPop)
	{
		if(IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if(IsA(plan, Motion))
	{
		Motion *motion = (Motion *) plan;
		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if(IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int motId = shareinput_peekmot(ctxt);
		Plan *shared = plan->lefttree;

		Assert(sisc->plan.flow);
		if(sisc->plan.flow->flotype == FLOW_SINGLETON)
		{
			if(sisc->plan.flow->segindex < 0) 
				ctxt->qdShares = list_append_unique_int(ctxt->qdShares, sisc->share_id);
		}

		if (shared)
		{
			Assert(shareinput_find_sharenode(ctxt, sisc->share_id, NULL) == false);
			Assert(get_plan_share_id(plan) == get_plan_share_id(shared));
			set_plan_driver_slice(shared, motId);

			ctxt->sharedNodes = lappend(ctxt->sharedNodes, shared);
			ctxt->sliceMarks = lappend_int(ctxt->sliceMarks, motId);
		}
	}

	return true;
}

/*
 * Second pass on shareinput xslice.  It marks the shared node xslice,
 * if a 'shared' is cross-slice.
 */
static bool
shareinput_mutator_xslice_2(Node* node, ApplyShareInputContext *ctxt, bool fPop)
{
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

		if(!shared)
		{
			ShareNodeWithSliceMark plan_slicemark = {NULL /* plan */,0 /* slice_mark */};
			int  shareSliceId = 0;
			
			shareinput_find_sharenode(ctxt, sisc->share_id, &plan_slicemark);
			if (plan_slicemark.plan == NULL)
				elog(ERROR, "could not find shared input node with id %d", sisc->share_id);

			shareSliceId = get_plan_driver_slice(plan_slicemark.plan);

			if(shareSliceId != motId)
			{
				ShareType stype = get_plan_share_type(plan_slicemark.plan);
				if(stype == SHARE_MATERIAL || stype == SHARE_SORT)
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
shareinput_mutator_xslice_3(Node *node, ApplyShareInputContext *ctxt, bool fPop)
{
	Plan *plan = (Plan *) node;

	if(fPop)
	{
		if(IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if(IsA(plan, Motion))
	{
		Motion *motion = (Motion *) plan;
		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if(IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int motId = shareinput_peekmot(ctxt);

		ShareNodeWithSliceMark plan_slicemark = { NULL, 0 };
		ShareType stype = SHARE_NOTSHARED;

		shareinput_find_sharenode(ctxt, sisc->share_id, &plan_slicemark);

		if(!plan_slicemark.plan)
			elog(ERROR, "sub-plan for share_id %d cannot be NULL", sisc->share_id);

		stype = get_plan_share_type(plan_slicemark.plan);

		switch(stype)
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

		if(list_member_int(ctxt->qdShares, sisc->share_id))
		{
			Assert(sisc->plan.flow);
			Assert(sisc->plan.flow->flotype == FLOW_SINGLETON);
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
shareinput_mutator_xslice_4(Node *node, ApplyShareInputContext *ctxt, bool fPop)
{
	Plan *plan = (Plan *) node;
	int motId = shareinput_peekmot(ctxt);

	if(fPop)
	{
		if(IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if(IsA(plan, Motion))
	{
		Motion *motion = (Motion *) plan;
		shareinput_pushmot(ctxt, motion->motionID);
		/* Do not return.  Motion need to be adjusted as well */
	}

	/* Well, the following test can be optimized if we record the test
	 * result so we test just once for all node in one slice.  But this
	 * code is not perf critical so be lazy.
	 */
	if(list_member_int(ctxt->qdSlices, motId))
	{
		if(plan->flow)
		{
			Assert(plan->flow->flotype == FLOW_SINGLETON);
			plan->flow->segindex = -1;
		}
	}
	return true;
}

static void gpmon_pushplannode(ApplyShareInputContext *ctxt, Plan *plan)
{
	ctxt->planNodes = lcons(plan, ctxt->planNodes);
}
static void gpmon_popplannode(ApplyShareInputContext *ctxt)
{
	list_delete_first(ctxt->planNodes);
}
static Plan *gpmon_peekplannode(ApplyShareInputContext *ctxt)
{
	return ctxt->planNodes == NULL ? NULL : linitial(ctxt->planNodes);
}

static bool assign_plannode_id_walker(Node *node, ApplyShareInputContext *ctxt, bool fPop)
{
	Plan *p = (Plan *) node;
	Plan *parent;

	if(fPop)
	{
		gpmon_popplannode(ctxt);
		return false;
	}

	p->plan_node_id = ++ctxt->nextPlanId;

	parent = gpmon_peekplannode(ctxt);

	if(!parent)
		p->plan_parent_node_id = -1;
	else
		p->plan_parent_node_id = parent->plan_node_id;

	gpmon_pushplannode(ctxt, p);
	return true;
}

Plan *
apply_shareinput_xslice(Plan *plan, PlannerGlobal *glob)
{
	ApplyShareInputContext *ctxt = &glob->share;
	ListCell *lp;

	ctxt->sharedNodes = NULL;
	ctxt->sliceMarks = NULL;
	ctxt->motStack = NULL;
	ctxt->qdShares = NULL;
	ctxt->qdSlices = NULL;
	ctxt->planNodes = NIL;
	ctxt->nextPlanId = 0;

	shareinput_pushmot(ctxt, 0);

	/* 
	 * Walk the tree.  See comment for each pass for what each pass will do.
	 * The context is used to carry information from one pass to another,
	 * as well as within a pass.
	 */

	/*
	 * A subplan might have a SharedScan consumer while the SharedScan
	 * producer is in the main plan, or vice versa. So in the first pass, we
	 * walk through all plans and collect all producer subplans into the
	 * context, before processing the consumers.
	 */
	foreach (lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		shareinput_walker(shareinput_mutator_xslice_1, (Node *) subplan, ctxt);
	}
	shareinput_walker(shareinput_mutator_xslice_1, (Node *) plan, ctxt);

	/* Now walk the tree again, and process all the consumers. */
	foreach (lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		shareinput_walker(shareinput_mutator_xslice_2, (Node *) subplan, ctxt);
	}
	shareinput_walker(shareinput_mutator_xslice_2, (Node *) plan, ctxt);

	foreach (lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		shareinput_walker(shareinput_mutator_xslice_3, (Node *) subplan, ctxt);
	}
	shareinput_walker(shareinput_mutator_xslice_3, (Node *) plan, ctxt);

	foreach (lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		shareinput_walker(shareinput_mutator_xslice_4, (Node *) subplan, ctxt);
	}
	shareinput_walker(shareinput_mutator_xslice_4, (Node *) plan, ctxt);

	return plan;
}

/* 
 * assign_plannode_id - Assign an id for each plan node.  Used by gpmon. 
 */
void assign_plannode_id(PlannedStmt *stmt)
{
	ApplyShareInputContext ctxt;

	/* We only care about planNodes and nextPlanId, so initialize them. */
	ctxt.planNodes = NIL;
	ctxt.nextPlanId = 0;
	
	leftdeep_walker(assign_plannode_id_walker, (Node *) stmt->planTree, &ctxt);
	
	ctxt.planNodes = NIL;
	
	ListCell *lc = NULL;
	foreach (lc, stmt->subplans)
	{
		Plan *subplan = lfirst(lc);
		Assert(subplan);

		ctxt.planNodes = NIL;

		leftdeep_walker(assign_plannode_id_walker, (Node *) subplan, &ctxt);
	}
}

/* 
 * We can "zap" a Result node if it has an Append node as its left tree and
 * no other complications (initplans, etc.) and its target list is
 * composed of nothing but Var nodes.
 */
static bool can_zap_result(Result *res)
{
	ListCell *lc;

	if(!res->plan.lefttree
      || res->plan.initPlan
	  || !IsA(res->plan.lefttree, Append)
	  || res->resconstantqual
	  || res->hashFilter
	  )
		return false;

	foreach(lc, res->plan.targetlist)
	{
		TargetEntry *te = (TargetEntry *) lfirst(lc);
		if(!IsA(te->expr, Var))
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
Plan *zap_trivial_result(PlannerInfo *root, Plan *plan)
{
	if(plan == NULL)
		return NULL;

	if(IsA(plan, SubPlan))
	{
		SubPlan *subplan = (SubPlan *) plan;
		Plan *subplan_plan = planner_subplan_get_plan(root, subplan);
		
		planner_subplan_put_plan(root, subplan, zap_trivial_result(root, subplan_plan));
		return plan;
	}

	if(IsA(plan, List))
	{
		List *l = (List *) plan;
		ListCell *lc;

		foreach(lc, l)
		{
			lfirst(lc) = zap_trivial_result(root, lfirst(lc));
		}

		return plan;
	}

	if(IsA(plan, Append))
	{
		Append *app = (Append *) plan;
		app->appendplans = (List *) zap_trivial_result(root, (Plan *) app->appendplans);
		return plan;
	}

	if(IsA(plan, SubqueryScan))
	{
		SubqueryScan *subq = (SubqueryScan *) plan;
		subq->subplan = zap_trivial_result(root, subq->subplan);
		return plan;
	}

	if(IsA(plan, Result))
	{
		Result *res = (Result *) plan;

		plan->lefttree = zap_trivial_result(root, plan->lefttree);
		if(can_zap_result(res))
		{
			Append *app = (Append *) plan->lefttree;
			app->plan.targetlist = plan->targetlist;
			app->plan.qual = plan->qual;
			app->isZapped = true;
			/* pass on the dispatch flag of Result to Append, which may has been set by cdbparallelize */
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
	CdbHash *pcdbhash = makeCdbHash(iSegments, HASH_FNV_1);
	cdbhashinit(pcdbhash);
			
	if (pconst->constisnull)
	{
		cdbhashnull(pcdbhash);
	}
	else
	{
		Assert(isGreenplumDbHashable(pconst->consttype));
		Oid oidType = pconst->consttype;
			
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
	
	CdbHash *pcdbhash = makeCdbHash(iSegments, HASH_FNV_1);
	cdbhashinit(pcdbhash);

	ListCell   *lc = NULL;

	foreach(lc, plConsts)
	{
		Const *pconst = (Const *) lfirst(lc);
		Assert(IsA(pconst, Const));
		
		if (pconst->constisnull)
		{
			cdbhashnull(pcdbhash);
		}
		else
		{
			Assert(isGreenplumDbHashable(pconst->consttype));
			Oid oidType = pconst->consttype;
				
			if (typeIsArrayType(oidType))
			{
				oidType = ANYARRAYOID;
			}
			cdbhash(pcdbhash, pconst->constvalue, oidType);
		}		
	}

	return cdbhashreduce(pcdbhash);
}

/*
 * return the IDs of RTEs that are referenced in the given plan
 */
static Bitmapset *
referenced_rtes(Plan *plan)
{
	Bitmapset *scanrelids = NULL;

	List *scans = extract_nodes_plan(plan, T_SubqueryScan, true /*descendIntoSubqueries*/);
	scans = list_concat(scans, extract_nodes_plan(plan, T_ValuesScan, true /*descendIntoSubqueries*/));
	scans = list_concat(scans, extract_nodes_plan(plan, T_FunctionScan, true /*descendIntoSubqueries*/));
	scans = list_concat(scans, extract_nodes_plan(plan, T_TableFunctionScan, true /*descendIntoSubqueries*/));

	ListCell *lc = NULL;
	foreach (lc, scans)
	{
		Scan *scan = (Scan *) lfirst(lc);
		int scanrelid = scan->scanrelid;
		if (!bms_is_member(scanrelid, scanrelids))
		{
			scanrelids = bms_add_member(scanrelids, scanrelid);
		}
	}

	list_free(scans);
	return scanrelids;
}

/*
 * walker context for finding all param IDs
 */
typedef struct find_params_context
{
	Bitmapset *paramids;
} find_params_context;

/*
 * find all param IDs in the given query or plan, represented by its root node
 */
static bool find_params_walker(Node *node, find_params_context *context)
{
	if (NULL == node)
	{
		return false;
	}

	if (IsA(node, Param))
	{
		Param *param = (Param *) node;
		if (!bms_is_member(param->paramid, context->paramids))
		{
			context->paramids = bms_add_member(context->paramids, param->paramid);
		}
		return false;
	}

	if (IsA(node, Query))
	{
		return query_tree_walker((Query *) node, find_params_walker, (void *) context, 0);
	}

	return expression_tree_walker(node, find_params_walker, (void *) context);
}

/*
 * Remove unused initplans. This may happen if an initplan has been created, but
 * its corresponding param has been removed because of contradiction detection
 */
static void remove_unused_initplans_helper(Plan *plan, Bitmapset **usedParams, Bitmapset *rte_params)
{
	if (NULL == plan || IsA(plan, ValuesScan) || IsA(plan, FunctionScan))
	{
		Bitmapset *oldbms = *usedParams;
		*usedParams = bms_union(*usedParams, rte_params);
		bms_free(oldbms);
		return;
	}

	if (IsA(plan, List))
	{
		List *l = (List *) plan;
		ListCell *lc;

		foreach(lc, l)
		{
			remove_unused_initplans_helper(lfirst(lc), usedParams, rte_params);
		}

		return;
	}

	find_params_context context;
	context.paramids = bms_copy(*usedParams);

	/* collect params used in target list and qual of current node */
	find_params_walker((Node *) plan->qual, &context);
	find_params_walker((Node *) plan->targetlist, &context);

	/* collect params used in special fields based on node type */
	switch (((Node*)(plan))->type)
	{
		case T_Result:
		{
			Result *result = (Result *) plan;
			find_params_walker(result->resconstantqual, &context);
			break;
		}
		case T_IndexScan:
		{
			IndexScan *iscan = (IndexScan *) plan;
			find_params_walker((Node *) iscan->indexqual, &context);
			break;
		}
		case T_BitmapIndexScan:
		{
			BitmapIndexScan *iscan = (BitmapIndexScan *) plan;
			find_params_walker((Node *) iscan->indexqual, &context);
			break;
		}
		case T_TidScan:
		{
			TidScan *tscan = (TidScan *) plan;
			find_params_walker((Node *) tscan->tidquals, &context);
			break;
		}
		case T_Limit:
		{
			Limit *limit = (Limit *) plan;
			find_params_walker((Node *) limit->limitCount, &context);
			find_params_walker((Node *) limit->limitOffset, &context);
			break;
		}
		case T_Join:
		{
			Join *join = (Join *) plan;
			find_params_walker((Node *) join->joinqual, &context);
			break;
		}
		case T_NestLoop:
		{
			NestLoop *nljoin = (NestLoop *) plan;
			find_params_walker((Node *) nljoin->join.joinqual, &context);
			break;
		}
		case T_MergeJoin:
		{
			MergeJoin *mgjoin = (MergeJoin *) plan;
			find_params_walker((Node *) mgjoin->join.joinqual, &context);
			find_params_walker((Node *) mgjoin->mergeclauses, &context);
			break;
		}
		case T_HashJoin:
		{
			HashJoin *hsjoin = (HashJoin *) plan;
			find_params_walker((Node *) hsjoin->join.joinqual, &context);
			find_params_walker((Node *) hsjoin->hashclauses, &context);
			find_params_walker((Node *) hsjoin->hashqualclauses, &context);
			break;
		}
		case T_Motion:
		{
			Motion *motion = (Motion *) plan;
			find_params_walker((Node *) motion->hashExpr, &context);
			break;
		}
		case T_Window:
		{
			Window *w = (Window *) plan;
			find_params_walker((Node *) w->windowKeys, &context);
			break;
		}
		default:
			break;
	}

	/* process children, and collect params that they use */
	if (IsA(plan, Append))
	{
		Append *app = (Append *) plan;
		remove_unused_initplans_helper((Plan *) app->appendplans, &(context.paramids), rte_params);
	}
	else if (IsA(plan, SubqueryScan))
	{
		SubqueryScan *subq = (SubqueryScan *) plan;
		remove_unused_initplans_helper(subq->subplan, &(context.paramids), rte_params);
	}
	else
	{
		remove_unused_initplans_helper(plan->lefttree, &(context.paramids), rte_params);
		remove_unused_initplans_helper(plan->righttree, &(context.paramids), rte_params);
	}

	if (NIL != plan->initPlan)
	{
		List	   *newInitPlans = NIL;
		ListCell *lc;

		foreach (lc, plan->initPlan)
		{
			SubPlan *initplan = (SubPlan *) lfirst(lc);
			ListCell *lc_paramid;
			bool		anyused;

			Assert(initplan->is_initplan);

			/* Are any of this Init Plan's output parameters actually used? */
			anyused = false;
			foreach (lc_paramid, initplan->setParam)
			{
				int			paramid = lfirst_int(lc_paramid);

				if (bms_is_member(paramid, context.paramids))
				{
					anyused = true;
					break;
				}
			}

			/* If none of its params are used, leave out from the new list */
			if (anyused)
				newInitPlans = lappend(newInitPlans, initplan);
			else
				elog(DEBUG2, "removing unused InitPlan %s", initplan->plan_name);
		}

		/* remove unused params */
		plan->allParam = bms_intersect(plan->allParam, context.paramids);

		list_free(plan->initPlan);
		plan->initPlan = newInitPlans;
	}

	Bitmapset *oldbms = *usedParams;
	*usedParams = bms_union(*usedParams, context.paramids);
	bms_free(oldbms);
	bms_free(context.paramids);
}

/* return a list of param ids that are referenced in RTEs */
static Bitmapset *params_in_rtable(Plan *plan, List *rtable)
{
	if (NIL == rtable)
	{
		return NULL;
	}

	Bitmapset *scanrelids = referenced_rtes(plan);

	find_params_context context;
	context.paramids = NULL;

	ListCell *lc = NULL;
	int rteid = 0;
	foreach (lc, rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);
		rteid ++;

		if (!bms_is_member(rteid, scanrelids))
		{
			// rte not referenced in the plan
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
				find_params_walker((Node *) rte->subquery, &context);
				break;
			case RTE_JOIN:
				find_params_walker((Node *) rte->joinaliasvars, &context);
				break;
			case RTE_FUNCTION:
				find_params_walker(rte->funcexpr, &context);
				break;
			case RTE_TABLEFUNCTION:
			{
				find_params_walker((Node *) rte->subquery, &context);
				find_params_walker(rte->funcexpr, &context);
				break;
			}
			case RTE_VALUES:
				find_params_walker((Node *) rte->values_lists, &context);
				break;
		}
	}

	return context.paramids;
}

/*
 * Remove unused initplans from the given plan object
 */
void remove_unused_initplans(Plan *plan, PlannerInfo *root)
{
	/*
	 * RTEs may have params as well, so we need to get param IDs from them
	 * before walking the plan to remove unused initplans
	 */
	Bitmapset *rte_params = params_in_rtable(plan, root->parse->rtable);

	/* now do the actual cleanup */
	Bitmapset *params = NULL;
	remove_unused_initplans_helper(plan, &params, rte_params);
	bms_free(params);
	bms_free(rte_params);
}

Node *
planner_make_plan_constant(struct PlannerInfo *root, Node *n, bool is_SRI)
{
	pre_dispatch_function_evaluation_context pcontext;

	planner_init_plan_tree_base(&pcontext.base, root);
	pcontext.single_row_insert = is_SRI;

	return plan_tree_mutator(n, pre_dispatch_function_evaluation_mutator, &pcontext);
}

/*
 * Evaluate functions to constants.
 */
Node *
exec_make_plan_constant(struct PlannedStmt *stmt, bool is_SRI)
{
	pre_dispatch_function_evaluation_context pcontext;

	Assert(stmt);
	exec_init_plan_tree_base(&pcontext.base, stmt);
	pcontext.single_row_insert = is_SRI;

	return plan_tree_mutator((Node *)stmt->planTree, pre_dispatch_function_evaluation_mutator, &pcontext);
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
        List *list = (List *) node;
        ListCell *lc = NULL;
		
		foreach(lc, list)
        {
            remove_subquery_in_RTEs((Node *) lfirst(lc));
        }
    }
}

#define GP_PARTITION_SELECTION_OID 6084
#define GP_PARTITION_EXPANSION_OID 6085
#define GP_PARTITION_INVERSE_OID 6086
 
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
	Node * new_node = 0;
	
	if (node == NULL)
		return NULL;

	if (IsA(node, Param))
	{
		Param *param = (Param *) node;

		/* Not replaceable, so just copy the Param (no need to recurse) */
		return (Node *) copyObject(param);
	}
	else if (IsA(node, FuncExpr))
	{
		FuncExpr *expr = (FuncExpr *) node;
		List *args;
		ListCell *arg;
		Expr *simple;
		FuncExpr *newexpr;
		bool has_nonconst_input;

		Form_pg_proc funcform;
		EState *estate;
		ExprState *exprstate;
		MemoryContext oldcontext;
		Datum const_val;
		bool const_is_null;
		int16 resultTypLen;
		bool resultTypByVal;

		Oid	funcid;
		HeapTuple func_tuple;

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
			bool is_seq_func = false;
			bool tup_or_set;
			cqContext *pcqCtx;

			pcqCtx = caql_beginscan(NULL,
									cql("SELECT * FROM pg_proc " " WHERE oid = :1 ", ObjectIdGetDatum(funcid)));

			func_tuple = caql_getnext(pcqCtx);

			if (!HeapTupleIsValid(func_tuple))
				elog(ERROR, "cache lookup failed for function %u", funcid);

			funcform = (Form_pg_proc) GETSTRUCT(func_tuple);

			/* can't handle set returning or row returning functions */
			tup_or_set = (funcform->proretset || type_is_rowtype(funcform->prorettype));

			caql_endscan(pcqCtx);
			
			/* can't handle it */
			if (tup_or_set)
			{
				/* 
				 * We haven't mutated this node, but we still return the
				 * mutated arguments.
				 *
				 * If we don't do this, we'll miss out on transforming function
				 * arguments which are themselves functions we need to mutated.
				 * For example, select foo(now()).
				 */
				return (Node *)newexpr;
			}

			/* 
			 * Ignored evaluation of gp_partition stable functions.
			 * TODO: refactor gp_partition stable functions to be truly
			 * stable
			 */
			if (funcid == GP_PARTITION_SELECTION_OID 
				|| funcid == GP_PARTITION_EXPANSION_OID 
				|| funcid == GP_PARTITION_INVERSE_OID)
			{
				return (Node *)newexpr;
			}

			/* 
			 * Here we want to mark any statement that is
			 * going to use a sequence as dirty.  Doing this means that the
			 * QD will flush the xlog which will also flush any xlog writes that
			 * the sequence server might do. 
			 */
			if (funcid == NEXTVAL_FUNC_OID || funcid == CURRVAL_FUNC_OID ||
				funcid == SETVAL_FUNC_OID)
			{
				ExecutorMarkTransactionUsesSequences();
				is_seq_func = true;
			}

			if (funcform->provolatile == PROVOLATILE_IMMUTABLE)
				/* okay */ ;
			else if (funcform->provolatile == PROVOLATILE_STABLE)
				/* okay */ ;
			else if (context->single_row_insert && is_seq_func)
				;/* Volatile, but special sequence function */
			else
				return (Node *)newexpr;

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
		OpExpr *expr = (OpExpr *) node;
		List *args;

		OpExpr *newexpr;

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
		 * During constant folding, the CurrentOfExpr's gp_segment_id, ctid, 
		 * and tableoid fields are filled in with observed values from the 
		 * referenced cursor. For more detail, see bindCurrentOfParams below.
		 */
		CurrentOfExpr *expr = (CurrentOfExpr *) node,
					  *newexpr = copyObject(expr);

		bindCurrentOfParams(newexpr->cursor_name,
							newexpr->target_relid,
			   		   		&newexpr->ctid,
					  	   	&newexpr->gp_segment_id,
					  	   	&newexpr->tableoid);
		return (Node *) newexpr;
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
 * bindCurrentOfParams
 *
 * During constant folding, we evaluate STABLE functions to give QEs a consistent view
 * of the query. At this stage, we will also bind observed values of 
 * gp_segment_id/ctid/tableoid into the CurrentOfExpr.
 * This binding must happen only after planning, otherwise we disrupt prepared statements.
 * Furthermore, this binding must occur before dispatch, because a QE lacks the 
 * the information needed to discern whether it's responsible for the currently 
 * positioned tuple.
 *
 * The design of this parameter binding is very tightly bound to the parse/analyze
 * and subsequent planning of DECLARE CURSOR. We depend on the "is_simply_updatable"
 * calculation of parse/analyze to decide whether CURRENT OF makes sense for the
 * referenced cursor. Moreover, we depend on the ensuing planning of DECLARE CURSOR
 * to provide the junk metadata of gp_segment_id/ctid/tableoid (per tuple).
 *
 * This function will lookup the portal given by "cursor_name". If it's simply updatable,
 * we'll glean gp_segment_id/ctid/tableoid from the portal's most recently fetched 
 * (raw) tuple. We bind this information into the CurrentOfExpr to precisely identify
 * the currently scanned tuple, ultimately for consumption of TidScan/execQual by the QEs.
 */
static void
bindCurrentOfParams(char *cursor_name, Oid target_relid, ItemPointer ctid, int *gp_segment_id, Oid *tableoid)
{
	char *table_name;
	Portal portal;
	QueryDesc *queryDesc;
	AttrNumber gp_segment_id_attno;
	AttrNumber ctid_attno;
	AttrNumber tableoid_attno;
	bool isnull;
	Datum value;

	portal = GetPortalByName(cursor_name);
	if (!PortalIsValid(portal))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" does not exist", cursor_name)));

	queryDesc = PortalGetQueryDesc(portal);
	if (queryDesc == NULL)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is held from a previous transaction", cursor_name)));

	/* obtain table_name for potential error messages */
	table_name = get_rel_name(target_relid);

	/* 
	 * The referenced cursor must be simply updatable. This has already
	 * been discerned by parse/analyze for the DECLARE CURSOR of the given
	 * cursor. This flag assures us that gp_segment_id, ctid, and tableoid (if necessary)
 	 * will be available as junk metadata, courtesy of preprocess_targetlist.
	 */
	if (!portal->is_simply_updatable)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
						cursor_name, table_name)));

	/* 
	 * The target relation must directly match the cursor's relation. This throws out
	 * the simple case in which a cursor is declared against table X and the update is
	 * issued against Y. Moreover, this disallows some subtler inheritance cases where
	 * Y inherits from X. While such cases could be implemented, it seems wiser to
	 * simply error out cleanly.
	 */
	Index varno = extractSimplyUpdatableRTEIndex(queryDesc->plannedstmt->rtable);
	Oid cursor_relid = getrelid(varno, queryDesc->plannedstmt->rtable);
	if (target_relid != cursor_relid)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
						cursor_name, table_name)));
	/* 
	 * The cursor must have a current result row: per the SQL spec, it's 
	 * an error if not.
	 */
	if (portal->atStart || portal->atEnd)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not positioned on a row", cursor_name)));

	/*
	 * As mentioned above, if parse/analyze recognized this cursor as simply
	 * updatable during DECLARE CURSOR, then its subsequent planning must have
	 * made gp_segment_id, ctid, and tableoid available as junk for each tuple.
	 *
	 * To retrieve this junk metadeta, we leverage the EState's junkfilter against
	 * the raw tuple yielded by the highest most node in the plan.
	 */
	TupleTableSlot *slot = queryDesc->planstate->ps_ResultTupleSlot;
	Insist(!TupIsNull(slot));
	Assert(queryDesc->estate->es_junkFilter);

	/* extract gp_segment_id metadata */
	gp_segment_id_attno = ExecFindJunkAttribute(queryDesc->estate->es_junkFilter, "gp_segment_id");
	if (!AttributeNumberIsValid(gp_segment_id_attno))
		elog(ERROR, "could not find junk gp_segment_id column");
	value = ExecGetJunkAttribute(slot, gp_segment_id_attno, &isnull);
	if (isnull)
		elog(ERROR, "gp_segment_id is NULL");
	*gp_segment_id = DatumGetInt32(value);

	/* extract ctid metadata */
	ctid_attno = ExecFindJunkAttribute(queryDesc->estate->es_junkFilter, "ctid");
	if (!AttributeNumberIsValid(ctid_attno))
		elog(ERROR, "could not find junk ctid column");
	value = ExecGetJunkAttribute(slot, ctid_attno, &isnull);
	if (isnull)
		elog(ERROR, "ctid is NULL");
	ItemPointerCopy(DatumGetItemPointer(value), ctid);

	/* 
	 * extract tableoid metadata
	 *
	 * DECLARE CURSOR planning only includes tableoid metadata when
	 * scrolling a partitioned table, as this is the only case in which
	 * gp_segment_id/ctid alone do not suffice to uniquely identify a tuple.
	 */
	tableoid_attno = ExecFindJunkAttribute(queryDesc->estate->es_junkFilter, "tableoid");
	if (AttributeNumberIsValid(tableoid_attno))
	{
		value = ExecGetJunkAttribute(slot, tableoid_attno, &isnull);
		if (isnull)
			elog(ERROR, "tableoid is NULL");
		*tableoid = DatumGetObjectId(value);

		/*
		 * This is our last opportunity to verify that the physical table given
		 * by tableoid is, indeed, simply updatable.
		 */
		if (!isSimplyUpdatableRelation(*tableoid))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("%s is not updatable",
							get_rel_name_partition(*tableoid))));
	} else
		*tableoid = InvalidOid;

	pfree(table_name);
}
