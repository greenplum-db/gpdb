/*-------------------------------------------------------------------------
 *
 * planpartition.c
 *	  Transforms to produce plans that achieve dynamic partition elimination.
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Author: Siva
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "catalog/catquery.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/tablecmds.h"
#include "optimizer/planmain.h"
#include "optimizer/planpartition.h"
#include "optimizer/walkers.h"
#include "optimizer/clauses.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/subselect.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbsetop.h"
#include "parser/parsetree.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "nodes/makefuncs.h"
#include "utils/typcache.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "cdb/cdbvars.h"
#include "parser/parse_expr.h"
#include "parser/parse_coerce.h"
#include "parser/parse_oper.h"

extern bool	gp_log_dynamic_partition_pruning;

/**
 * Notes:
 * - walk the current plan and bind to pattern of the following kind:
 *                  JOIN
 *                  / \
 *                 /   \
 *            (Outer)   Append
 *                          \_P0
 *                          |_p1
 */

typedef struct PartitionJoinMutatorContext
{
	PlannerInfo *root;
	List		*rtable;    /* List of RTEs to refer to. Note that this may not be identical to root->parse->rtable */
	MemoryContext localCtx;  /* Allocate transient information during transformation */
	MemoryContext callerCtx; /* Allocate persistent information in caller's context */

	/**
	 * All these objects are transient. They are allocated in localCtx only.
	 */
	bool converted; /* converted at least one tree */
	bool safeToConvert;
	Oid partitionOid;
	Index partitionVarno; /* What is the varno corresponding to the append? */
	PartitionNode *partitionInfo; /* This structure has info about partitioned relation */
	Bitmapset *partitionKeySet; /* bitmap set for all attributes that are part of partitioning key */
	Bitmapset *joinKeySet;		/* bitmap set for all attributes involved in a join */
	/**
	 * These objects are allocated in callerCtx. They correspond to the result
	 * of transformations.
	 */
	Plan *outerPlan; /* Plan for outer side */
	List *outerTargetList; /* What should be the target list of modified outer query */
	Param *oidArrayParam; /* Initplan fixes this parameter */

} PartitionJoinMutatorContext;

/**
 * Forward declare static functions
 */
static bool IsJoin(Node *node);
static Plan *TransformPartitionJoin(Plan *plan, PartitionJoinMutatorContext *ctx);
static void ConstructAllNullOuterTargetList(PartitionJoinMutatorContext *ctx);
static Node *CopyObjectInCallerCtxt(Node *node, PartitionJoinMutatorContext *ctx);
static void ResetPMJC(PartitionJoinMutatorContext *ctx);
static void InitPJMC(PartitionJoinMutatorContext *ctx, PlannerInfo *root, List *rtable, MemoryContext callerCtx, MemoryContext localCtx);
static Const* makeOidConst(Oid o);

/**
 * Reset the context.
 */
static void ResetPMJC(PartitionJoinMutatorContext *ctx)
{
	ctx->safeToConvert = true;
	ctx->partitionOid = InvalidOid;
	ctx->partitionVarno = 0;
	ctx->partitionInfo = NULL;
	ctx->partitionKeySet = NULL;
	ctx->joinKeySet = NULL;
	ctx->outerPlan = NULL;
	ctx->outerTargetList = NULL;

	MemoryContextReset(ctx->localCtx);
}

/**
 * Initialize partition join mutator context.
 */
static void InitPJMC(PartitionJoinMutatorContext *ctx, PlannerInfo *root, List *rtable, MemoryContext callerCtx, MemoryContext localCtx)
{
	Assert(ctx);
	Assert(root);
	ctx->root = root;
	ctx->rtable = rtable;
	ctx->converted = false;
	ctx->callerCtx = callerCtx;
	ctx->localCtx = localCtx;
	ResetPMJC(ctx);
}


/**
 * This method looks at the Append part of the plan under a join and extracts out interesting bits of information.
 * These include: partitionOid of the master table, partitionInfo that is a struct containing partitioning information,
 * what are the partitioning keys and what does an all null row of the partitioned table look like.
 */
static void MatchAppendPattern(Plan *append, PartitionJoinMutatorContext *ctx)
{
	ctx->partitionOid = InvalidOid;

	/**
	 * It is possible we have already applied the transformation to this subtree.
	 * Check for result node as child.
	 */
	Append *appendNode = (Append *) append;
	Assert(list_length(appendNode->appendplans) > 0);
	Plan *child = (Plan *) list_nth(appendNode->appendplans, 0);
	if (IsA(child, Result))
	{
		ctx->safeToConvert = false;
		return;
	}

	/**
	 * Find all the vars in the targetlist and check if they're from a partitioned table.
	 */
	List *allVars = extract_nodes(NULL /* PlannerGlobal */, (Node *) append->targetlist, T_Var);
	ListCell *lc = NULL;
	foreach (lc, allVars)
	{
		Var *v = (Var *) lfirst(lc);
		RangeTblEntry *rte = rt_fetch(v->varno, ctx->rtable);
		Assert(rte);

		if (!rel_is_partitioned(rte->relid))
		{
			ctx->safeToConvert = false;
			return;
		}

		if (ctx->partitionOid == InvalidOid)
		{
			ctx->partitionOid = rte->relid;
			ctx->partitionVarno = v->varno;
		}

		if (ctx->partitionOid != rte->relid)
		{
			ctx->safeToConvert = false;
			return;
		}
	}

	ctx->partitionInfo = RelationBuildPartitionDescByOid(ctx->partitionOid, false);

	if (!ctx->partitionInfo)
	{
		ctx->safeToConvert = false;
		return;
	}

	List *partitionAttNums = get_partition_attrs(ctx->partitionInfo);

	/**
	 * Create a bitmapset from partition attribute numbers
	 */
	foreach (lc, partitionAttNums)
	{
		ctx->partitionKeySet = bms_add_member(ctx->partitionKeySet, lfirst_int(lc));
	}

	Assert(ctx->partitionKeySet);

	ConstructAllNullOuterTargetList(ctx);

	return;
}

/**
 * This constructs initial all-null targetlist for outer query. We need to create null expressions of the right type.
 */
static void ConstructAllNullOuterTargetList(PartitionJoinMutatorContext *ctx)
{
	Assert(ctx->outerTargetList == NULL);
	Assert(ctx->partitionOid != InvalidOid);

	/* Assume we already have adequate lock */
	Relation relation = heap_open(ctx->partitionOid, AccessShareLock);

	for (int2 attNum = 1; attNum <= RelationGetNumberOfAttributes(relation); attNum++)
	{
		Form_pg_attribute attribute = relation->rd_att->attrs[attNum - 1];
		Oid typeId = INT4OID;
		int4 typeMod = -1;
		char *attName = pstrdup("dropped");
		if (!attribute->attisdropped)
		{
			typeId = attribute->atttypid;
			typeMod = attribute->atttypmod;
			attName = pstrdup(NameStr(attribute->attname));
		}
		/**
		 * We have to include even dropped columns.
		 */
		Const *nullConst = makeNullConst(typeId, typeMod);

		ctx->outerTargetList = lappend(ctx->outerTargetList,
				makeTargetEntry((Expr *) nullConst,
						attNum,
						attName,
						false));
	}

	heap_close(relation, AccessShareLock);
	Assert(ctx->outerTargetList);
}

/**
 * Given an expression, this returns another one which strips out the top relabel node, if any.
 */
static Expr* StripRelabel(Expr *expr)
{
	Expr *result = expr;
	if (IsA(result, RelabelType))
	{
		result = ((RelabelType *) result)->arg;
	}
	return result;
}

/**
 * Does the given expression correspond to a var on partitioned relation. This method
 * ignores relabeling wrappers
 */
static bool IsAppendVar(Expr *expr, PartitionJoinMutatorContext *ctx)
{
	Assert(expr);
	Assert(ctx);

	expr = StripRelabel(expr);

	return (IsA(expr, Var) && ((Var *) expr)->varno == ctx->partitionVarno);
}

/**
 * Is opexp of the sort (outer_expr = append.partition_key)? If so, we need to extract interesting
 * information out.
 */
static void MatchEqualityOpExpr(OpExpr *opexp, PartitionJoinMutatorContext *ctx)
{
	Assert(ctx);
	Assert(opexp);
	Assert(list_length(opexp->args) > 1);
	bool isEqualityOp = op_mergejoinable(opexp->opno);

	/**
	 * If this is not an equijoin, then bail out early.
	 */

	if (!isEqualityOp
			|| list_length(opexp->args) != 2)
	{
		return;
	}

	Expr *expr1 = (Expr *) list_nth(opexp->args, 0);
	Expr *expr2 = (Expr *) list_nth(opexp->args, 1);

	Var *appendVar = NULL;
	Expr *otherExpr = NULL;

	if (IsAppendVar(expr1, ctx))
	{
		appendVar = (Var *) StripRelabel(expr1);
		otherExpr = (Expr *) expr2;
	}
	else if (IsAppendVar(expr2, ctx))
	{
		appendVar = (Var *) StripRelabel(expr2);
		otherExpr = (Expr *) expr1;
	}

	/**
	 * May have to cooerce the other expression
	 */
	otherExpr = (Expr *) coerce_to_target_type(NULL,
			(Node *) otherExpr,
			exprType( (Node *) otherExpr),
			exprType((Node *) appendVar),
			exprTypmod((Node *) appendVar),
			COERCION_ASSIGNMENT,
			COERCE_IMPLICIT_CAST,
			-1);

	if (appendVar && otherExpr)
	{
		if (bms_is_member(appendVar->varattno, ctx->partitionKeySet))
		{
			ctx->joinKeySet = bms_add_member(ctx->joinKeySet, appendVar->varattno);
			Assert(appendVar->varattno < list_length(ctx->outerTargetList) + 1);
			/**
			 * Replace element in the outer targetlist with the var from the outer side of the join condition.
			 */
			TargetEntry *currentTle = list_nth(ctx->outerTargetList, appendVar->varattno - 1);

			TargetEntry *tle = makeTargetEntry((Expr *) copyObject(otherExpr),
					appendVar->varattno,
					pstrdup(currentTle->resname),
					false);
			list_nth_replace(ctx->outerTargetList, appendVar->varattno - 1, tle);
		}
	}
}


/**
 * Perform hash-join specific join pattern checking
 */
static void MatchHashJoinPredicate(HashJoin *hj, PartitionJoinMutatorContext *ctx)
{
	ListCell *lc = NULL;
	foreach (lc, hj->hashclauses)
	{
		Assert(IsA(lfirst(lc), OpExpr));
		OpExpr *opexp = (OpExpr *) lfirst(lc);
		MatchEqualityOpExpr(opexp, ctx);
	}
}

/**
 * Perform merge-join specific join pattern checking
 */
static void MatchMergeJoinPredicate(MergeJoin *mj, PartitionJoinMutatorContext *ctx)
{
	ListCell *lc = NULL;
	foreach (lc, mj->mergeclauses)
	{
		Assert(IsA(lfirst(lc), OpExpr));
		OpExpr *opexp = (OpExpr *) lfirst(lc);
		MatchEqualityOpExpr(opexp, ctx);
	}
}

/**
 * Perform NL-join specific join pattern checking
 */
static void MatchNLJoinPredicate(NestLoop *nlj, PartitionJoinMutatorContext *ctx)
{
	ListCell *lc = NULL;
	foreach (lc, nlj->join.joinqual)
	{
		if (IsA(lfirst(lc), OpExpr))
		{
			OpExpr *opexp = (OpExpr *) lfirst(lc);
			MatchEqualityOpExpr(opexp, ctx);
		}
	}
}

/**
 * Does a plan node have a non-initplan subplan somewhere?
 */
static bool ContainsSubplan(Plan *p)
{
	List *subplans = extract_nodes_plan(p, T_SubPlan, true);
	ListCell *lc = NULL;
	foreach (lc, subplans)
	{
		SubPlan *sp = (SubPlan *) lfirst(lc);
		if (!sp->is_initplan)
		{
			return true;
		}
	}
	return false;
}

/**
 * Collects a bitmapset of all varnos referred to in a plan.
 */
typedef struct CollectVarnoContext
{
	plan_tree_base_prefix base;
	Bitmapset	*varnos;
} CollectVarnoContext;

/**
 * Walker method to adjust varnos by offset.
 */
static bool
CollectVarnoWalker(Node *node, CollectVarnoContext *ctx)
{
	Assert(ctx);

	if (node == NULL)
	{
		return false;
	}
	else if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;
		ctx->varnos = bms_add_member(ctx->varnos, var->varno);
		return false;

	}
	return plan_tree_walker(node, CollectVarnoWalker, ctx);
}

/**
 * Inner plan and outer plan refer to a common varno. This is possible in a plan that involves a NL-Index Scan.
 * In this case, DPE cannot be applied
 */
static bool ContainsCrossReference(Plan *join, PartitionJoinMutatorContext *ctx)
{
	CollectVarnoContext vctx;
	vctx.base.node = (Node *) ctx->root;
	vctx.varnos = NULL;

	CollectVarnoWalker((Node *) join->lefttree, &vctx);

	/**
	 * Save varnos referred to in the left child.
	 */
	Bitmapset *lbms = vctx.varnos;

	vctx.varnos = NULL;
	CollectVarnoWalker((Node *) join->righttree, &vctx);

	/**
	 * Save varnos referred to in the right child.
	 */
	Bitmapset *rbms = vctx.varnos;

	return (bms_num_members(bms_intersect(lbms, rbms)) > 0);
}


/**
 * Match join condition.
 */
static void MatchJoinPattern(Plan *join, PartitionJoinMutatorContext *ctx)
{
	Assert(ctx->safeToConvert);

	/**
	 * If join type is not an inner join or this contains a non-initplan subplan, do not apply DPE
	 */
	Join *joinNode = (Join *) join;

	if (!(joinNode->jointype == JOIN_INNER
			|| joinNode->jointype == JOIN_IN)
			|| ContainsSubplan(join->righttree)
			|| ContainsCrossReference(join, ctx))
	{
		ctx->safeToConvert = false;
		return;
	}

	/**
	 * Look at join predicates to find suitable equality conditions on partition key
	 */
	if (IsA(join, HashJoin))
	{
		MatchHashJoinPredicate((HashJoin *) join, ctx);
	}
	else if (IsA(join, MergeJoin))
	{
		MatchMergeJoinPredicate((MergeJoin *) join, ctx);
	}
	else
	{
		Assert(IsA(join, NestLoop));
		MatchNLJoinPredicate((NestLoop *) join, ctx);
	}

	/**
	 * The join key set must be a subset of the partition key set. We ignore other join conditions.
	 */
	Assert(bms_is_subset(ctx->joinKeySet, ctx->partitionKeySet));

	/**
	 * If the join key set does not contain even a single partition column,
	 * then we cannot proceed with transformation.
	 */
	if (bms_is_empty(ctx->joinKeySet))
	{
		ctx->safeToConvert = false;
		return;
	}

	/**
	 * Copy out targetlist in caller's context
	 */
	ctx->outerTargetList = (List *) CopyObjectInCallerCtxt( (Node *) ctx->outerTargetList, ctx);

	/**
	 * Find the outer side of the join
	 */

	ctx->outerPlan = join->righttree;

	if (IsA(ctx->outerPlan, Hash))
	{
		/**
		 * Skip the hash node in the hash-join.
		 */
		ctx->outerPlan = (Plan *) ctx->outerPlan->lefttree;
	}

	if (IsA(ctx->outerPlan, Material))
	{
		/**
		 * Skip materialize node
		 */
		ctx->outerPlan = (Plan *) ctx->outerPlan->lefttree;
	}

	/**
	 * If this is a motion node (as it could be a broadcast/redistribute under hash,
	 * then skip that one as well.
	 */
	if (IsA(ctx->outerPlan, Motion))
	{
		ctx->outerPlan = ctx->outerPlan->lefttree;
	}

	/**
	 * Copy the object in caller's context
	 */
	ctx->outerPlan = (Plan *) CopyObjectInCallerCtxt((Node *) ctx->outerPlan, ctx);

	Assert(ctx->outerPlan);
	return;
}

/**
 * Given a node, copy it in caller's context
 */
static Node *CopyObjectInCallerCtxt(Node *node, PartitionJoinMutatorContext *ctx)
{
	MemoryContext oldCtx = MemoryContextSwitchTo(ctx->callerCtx);
	Node *result = copyObject(node);
	MemoryContextSwitchTo(oldCtx);
	return result;
}

/**
 * Perform detailed matching.
 */
static void MatchPartitionJoinPattern(Plan *plan, PartitionJoinMutatorContext *ctx)
{
	MemoryContextSwitchTo(ctx->localCtx);

	/**
	 * First match the append side of the tree
	 */
	Assert(IsA(plan->lefttree, Append));
	MatchAppendPattern(plan->lefttree, ctx);

	if (ctx->safeToConvert)
	{
		/**
		 * Match the join condition.
		 */
		Assert(IsJoin((Node *) plan));
		MatchJoinPattern(plan, ctx);
	}

	MemoryContextSwitchTo(ctx->callerCtx);
}

/**
 * Is a given node a join plan node?
 */
static bool IsJoin(Node *node)
{
	return (IsA(node, HashJoin)
			|| IsA(node, NestLoop)
			|| IsA(node, MergeJoin));
}

/**
 * Given a relation's Oid, return the oid of the type of its row
 */
static Oid RelationGetTypeOid(Oid relationOid)
{
	int fetchCount;
	Oid typeOid;

	Assert(relationOid != InvalidOid);
	typeOid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT reltype FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(relationOid)));

	Assert(fetchCount);

	Assert(typeOid != InvalidOid);

	return typeOid;
}

/**
 * Convenience function to create a const node with a given oid value.
 */
static Const* makeOidConst(Oid o)
{
	int16 typeLength = -1;
	bool typeByVal = true;

	get_typlenbyval(OIDOID, &typeLength, &typeByVal);

	/**
	 * Oids are always by value
	 */
	Assert(typeByVal == true);
	Assert(typeLength == 4);

	return makeConst(OIDOID, -1, typeLength, ObjectIdGetDatum(o), false, typeByVal);
}

/**
 * Constructs a targetlist for agg node for the initplan.
 */
static TargetEntry *ConstructAggTLE(PartitionJoinMutatorContext *ctx)
{
	/**
	 * The aggref takes two inputs: (oid, record)
	 * oid - oid of the partition table on the inner side
	 * record - constructed record which is of the type of the partitioned table
	 */

	List *aggRefArgs = NIL;

	/**
	 * First add oid of partitioned table as a constant.
	 */
	Const *partOidConst = makeOidConst(ctx->partitionOid);

	aggRefArgs = lappend(aggRefArgs, partOidConst);

	RowExpr *rexpr = makeNode(RowExpr);
	rexpr->colnames = NULL;
	rexpr->location = -1;
	rexpr->row_format = COERCE_DONTCARE;
	rexpr->row_typeid = RelationGetTypeOid(ctx->partitionOid);

	ListCell *lc = NULL;
	foreach (lc, ctx->outerPlan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		rexpr->args = lappend(rexpr->args, copyObject(tle->expr));
	}

	aggRefArgs = lappend(aggRefArgs, rexpr);

	Aggref *aggref = makeNode(Aggref);

	aggref->aggfnoid = PG_PARTITION_OID_OID;
	aggref->aggtype = OIDARRAYOID;
	aggref->args = aggRefArgs;
	aggref->agglevelsup = 0;
	aggref->aggstar = false;
	aggref->aggdistinct = false;
	aggref->aggstage = AGGSTAGE_NORMAL;

	TargetEntry *tle = makeTargetEntry((Expr *) aggref,
						  1,
						  pstrdup("agg_target"),
						  false);

	return tle;
}

/**
 * When constructing an initplan, we may have to adjust its varnos because the original plan
 * from which it was constructed might have been under an subqueryscan.
 */

typedef struct AdjustVarnoContext
{
	plan_tree_base_prefix base;
	int			offset;
} AdjustVarnoContext;

/**
 * Walker method to adjust varnos by offset.
 */
static bool
AdjustVarnoWalker(Node *node, AdjustVarnoContext *ctx)
{
	Assert(ctx);

	if (node == NULL)
	{
		return false;
	}

	switch (nodeTag(node))
	{
		case T_Var:
		{
			/**
			 * Adjust varno by offset.
			 */
			Var		   *var = (Var *) node;
			var->varno += ctx->offset;
			var->varnoold += ctx->offset;
			return false;
		}
		case T_Flow:
		{
			/**
			 * flow's hashexpression needs to be walked explicitly
			 */
			Flow *flow = (Flow *) node;
			return AdjustVarnoWalker((Node *) flow->hashExpr, ctx);
		}
		case T_SeqScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_ExternalScan:
		case T_IndexScan:
		case T_BitmapIndexScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TidScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_SubqueryScan:
		case T_TableFunctionScan:
		{
			/**
			 * Scan nodes have a scanrelid that is semantically identical to varno.
			 * These must be adjusted as well.
			 */
			Scan    *splan = (Scan *) node;
			splan->scanrelid += ctx->offset;

			Plan *plan = (Plan *) node;
			AdjustVarnoWalker((Node *) plan->targetlist, ctx);
			AdjustVarnoWalker((Node *) plan->qual, ctx);
			AdjustVarnoWalker((Node *) plan->flow, ctx);

			return false;
		}
		case T_SubPlan:
		{
			/**
			 * These nodes maintain rtables internally. We should not attempt to adjust
			 * anything inside these nodes.
			 */
			return false;
		}
		default:
			break;
	}

	return plan_tree_walker(node, AdjustVarnoWalker, ctx);
}

/**
 * Method to adjust varnos in plan by a given offset
 */
static void AdjustVarno(PlannerInfo *root, Node *node, int offset)
{
	Assert(offset != 0);
	AdjustVarnoContext ctx;
	ctx.base.node = (Node *) root;
	ctx.offset = offset;
	AdjustVarnoWalker(node, &ctx);
}

/**
 * Ensure that all rows are gathered on QD.
 */
static Plan* GatherToQD(Plan *outerPlan)
{
	bool gatheredOnMaster = outerPlan->flow
			&& outerPlan->flow->flotype == FLOW_SINGLETON
			&& outerPlan->flow->segindex == -1;

	if (gatheredOnMaster)
	{
		return outerPlan;
	}

	/**
	 * Some plan nodes don't maintain flow. If it is not set,
	 * assume it is partitioned.
	 */
	if (!outerPlan->flow)
	{
		mark_plan_strewn(outerPlan);
	}

	return (Plan *) make_motion_gather_to_QD(outerPlan, false);
}

/**
 * Construct the initplan and create a parameter. Save reference to parameter.
 */
static void ConstructInitPlan(PartitionJoinMutatorContext *ctx)
{
	Assert(CurrentMemoryContext == ctx->callerCtx);

	ctx->outerPlan = plan_pushdown_tlist(ctx->root, ctx->outerPlan, ctx->outerTargetList);

	ctx->outerPlan = GatherToQD(ctx->outerPlan);

	/* single tlist entry that is the aggregate target */
	TargetEntry *tle = ConstructAggTLE(ctx);

	List *aggregateTargetList = list_make1(tle);

	ctx->outerPlan = (Plan *)
			make_agg(ctx->root, aggregateTargetList,  NULL /* qual */,
					AGG_PLAIN, false /* streaming */,
					0 /* numGroupCols */,
					NULL /* grpColIdx */,
					NULL /* grpOperators */,
					0 /* numGroups */,
					0 /* int num_nullcols */,
					0 /* input_grouping */,
					0 /* grouping */,
					0 /* rollupGSTimes */,
					1 /* numAggs */,
					80000 /* transSpace */,
					ctx->outerPlan);

	/* Pull up the Flow from the subplan */
	ctx->outerPlan->flow = pull_up_Flow(ctx->outerPlan, ctx->outerPlan->lefttree, false);

	//elog(NOTICE, "modified outer plan is %s", nodeToString(ctx->outerPlan));

	/**
	 * Some gymnastics needs to be performed here. It is possible that the transformation
	 * is being applied below a subquery scan. If this is the case, we need to add all the rtable
	 * entries to the top-level rtable and adjust vars accordingly.
	 */
	if (ctx->root->parse->rtable != ctx->rtable)
	{
		int offset = list_length(ctx->root->parse->rtable);
		AdjustVarno(ctx->root, (Node *) ctx->outerPlan, offset);
		ctx->root->parse->rtable = list_concat(ctx->root->parse->rtable, copyObject(ctx->rtable));
	}

	/*
	 * Convert the plan into an InitPlan, and make a Param for its result.
	 */
	ctx->oidArrayParam = SS_make_initplan_from_plan(ctx->root, ctx->outerPlan,
											 exprType((Node *) tle->expr),
											 -1);
	return;
}


/**
 * This method is responsible for constructing the transformed plan fragment in the caller context.
 */
static Plan *TransformPartitionJoin(Plan *plan, PartitionJoinMutatorContext *ctx)
{
	Assert(plan);
	Assert(ctx);

	Assert(CurrentMemoryContext == ctx->callerCtx);

	ConstructInitPlan(ctx);

	/**
	 * Insert a result node between top append node and its children.
	 */
	Append *append = (Append *) plan->lefttree;
	ListCell *lc = NULL;
	List *newChildren = NIL;

	foreach (lc, append->appendplans)
	{
		Plan *child = (Plan *) lfirst(lc);

		List *allVars = extract_nodes(NULL /* PlannerGlobal */, (Node *) child->targetlist, T_Var);
		Var *v = (Var *) list_nth(allVars, 0);
		RangeTblEntry *rte = rt_fetch(v->varno, ctx->rtable);
		Oid partitionOid = rte->relid;

		ScalarArrayOpExpr *saop = makeNode(ScalarArrayOpExpr);

		Assert(partitionOid != InvalidOid);

		Const *saopArg1 = makeOidConst(partitionOid);

		saop->args = NIL;
		saop->args = lappend(saop->args, saopArg1);
		saop->args = lappend(saop->args, ctx->oidArrayParam);
		saop->opno = 607;
		saop->opfuncid = 184;
		saop->useOr = true;

		Result *result = make_result(ctx->root, child->targetlist, (Node *) list_make1(saop) /*resconstantqual */, child);
		newChildren = lappend(newChildren, result);
	}
	append->appendplans = newChildren;

	return plan;
}

/**
 * Walker finds pattern and call
 */
static Node* PartitionJoinMutator(Node *node, PartitionJoinMutatorContext *ctx)
{
	Assert(ctx);

	Assert(CurrentMemoryContext == ctx->callerCtx);

	if (node == NULL)
	{
		return NULL;
	}

	if(IsJoin(node))
	{
		Plan *plan = (Plan *) node;
		if (IsA(plan->lefttree, Append))
		{
			ResetPMJC(ctx);

			MatchPartitionJoinPattern(plan, ctx);

			//elog(NOTICE, "safe to convert = %s", ctx->safeToConvert ? "true" : "false");

			if (ctx->safeToConvert)
			{
				Plan *transformedPlan = TransformPartitionJoin(plan, ctx);
				ctx->converted = true;

				return (Node *) transformedPlan;
			}
		}
	}

	if (IsA(node, ShareInputScan)
			|| IsA(node, TableFunctionScan)
			|| IsA(node, SubPlan)
			|| IsA(node, Append))
	{
		/**
		 * Do not descend further. Their children have already been processed at some point in the past.
		 */
		return node;
	}

	if (IsA(node, SubqueryScan))
	{
		/**
		 * This case is tricky. Under a subqueryscan all vars refer to entries in the rtable for the subqueryscan.
		 * We must start a new transformation with modified rtable entry.
		 */
		SubqueryScan *ss = (SubqueryScan *) node;
		PartitionJoinMutatorContext pctx;
		InitPJMC(&pctx, ctx->root, ss->subrtable, ctx->callerCtx, ctx->localCtx);

		/**
		 * Newly constructed plan pieces will be in the caller context.
		 */
		ss->subplan = (Plan *) PartitionJoinMutator((Node *) ss->subplan, &pctx);

		return (Node *) ss;
	}

	return plan_tree_mutator(node, PartitionJoinMutator, ctx);
}

/**
 * Main entry point to dynamic partition elimination transform.
 */
Plan *apply_dyn_partition_transforms(PlannerInfo *root, Plan *plan)
{
	Assert(root);
	Assert(plan);

	MemoryContext callerCtx = CurrentMemoryContext;

	MemoryContext localCtx = AllocSetContextCreate(callerCtx,
			"Partition Join Transform Context",
			ALLOCSET_SMALL_MINSIZE,
			ALLOCSET_SMALL_INITSIZE,
			ALLOCSET_SMALL_MAXSIZE);

	PartitionJoinMutatorContext pctx;
	InitPJMC(&pctx, root, root->parse->rtable, callerCtx, localCtx);

	/**
	 * Newly constructed plan pieces will be in the caller context.
	 */
	Plan *result = (Plan *) PartitionJoinMutator((Node *) plan, &pctx);

	MemoryContextSwitchTo(callerCtx);
	MemoryContextDelete(localCtx);

	return result;
}

/**
 * These structs and functions support the aggregate function that computes a set of partition oids based
 * on rows that the outer side of a partitioned join produces.
 */
typedef struct PartitionMatchInfo
{
	MemoryContext mctx;			/* What memory context is this structure created in? */
	Oid partitionOid;			/* What is the oid of the partitioned table? */
	PartitionNode *partitionInfo; /* Metadata about the partitioned table */
	PartitionState *partitionState;	/* Optimizing queries to partition table's metadata */
	HTAB *matchingPartitionOids;	/* Keys correspond to matching partition oids. Value is counter of number of times chosen. */

	/**
	 * Record storage
	 */
	HeapTupleData tuple;

	Oid			tupleTypeOid;
	int32		tupleTypeMod;
	TupleDesc	tupleDesc;
	Datum		*values;
	bool		*nulls;

} PartitionMatchInfo;

static void AddPartitionMatch(PartitionMatchInfo *pmi, Oid partOid);

/**
 * Initialize aggregate transition function state.
 */
static void InitPMI(PartitionMatchInfo *pmi, Oid partitionOid, MemoryContext mctx)
{
	pmi->mctx = mctx;

	MemoryContext oldCtx = MemoryContextSwitchTo(pmi->mctx);

	pmi->partitionOid = partitionOid;
	pmi->partitionInfo = RelationBuildPartitionDescByOid(pmi->partitionOid, false);
	if (!pmi->partitionInfo)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("relation with OID %u does not exist", partitionOid)));

	pmi->partitionState = createPartitionState(pmi->partitionInfo, 0);

	/**
	 * Initialize hash table
	 */
	HASHCTL hash_ctl;
	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(Oid);
	hash_ctl.entrysize = sizeof(Oid);
	hash_ctl.hash = oid_hash;
	hash_ctl.hcxt = mctx;

	pmi->matchingPartitionOids =
			hash_create("matching partition map",
			10000,
			&hash_ctl,
			HASH_ELEM | HASH_CONTEXT | HASH_FUNCTION);


	/**
	 * Per-record structures
	 */
	MemSet(&pmi->tuple, 0, sizeof(pmi->tuple));
	pmi->tupleDesc = NULL;
	pmi->tupleTypeMod = -1;
	pmi->tupleTypeOid = InvalidOid;
	pmi->values = NULL;
	pmi->nulls = NULL;

	MemoryContextSwitchTo(oldCtx);
}

/**
 * Add a new incoming record.
 */
static void AddRecordPMI(PartitionMatchInfo *pmi, HeapTupleHeader record)
{
	MemoryContext oldCtx = MemoryContextSwitchTo(pmi->mctx);

	if (pmi->tupleDesc == NULL)
	{
		/* Extract type info from the tuple itself */
		pmi->tupleTypeOid = HeapTupleHeaderGetTypeId(record);
		pmi->tupleTypeMod = HeapTupleHeaderGetTypMod(record);
		pmi->tupleDesc = lookup_rowtype_tupdesc(pmi->tupleTypeOid, pmi->tupleTypeMod);

		int ncolumns = pmi->tupleDesc->natts;

		pmi->values = (Datum *) palloc(sizeof(Datum) * ncolumns);
		pmi->nulls = (bool *) palloc(sizeof(bool) * ncolumns);

	}

	/**
	 * Wire up incoming record. Build a temporary HeapTuple structure.
	 */
	pmi->tuple.t_len = HeapTupleHeaderGetDatumLength(record);
	ItemPointerSetInvalid(&(pmi->tuple.t_self));
	pmi->tuple.t_data = record;

	heap_deform_tuple(&pmi->tuple, pmi->tupleDesc, pmi->values, pmi->nulls);

	Assert(pmi->partitionState != NULL);
	List *partIds = selectPartitionMulti(pmi->partitionInfo, pmi->values, pmi->nulls, pmi->tupleDesc, pmi->partitionState->accessMethods);

	ListCell *lc = NULL;
	foreach (lc, partIds)
	{
		Oid partId = lfirst_oid(lc);
		Assert(partId != InvalidOid);
		AddPartitionMatch(pmi, partId);
	}

	list_free(partIds);
	MemoryContextSwitchTo(oldCtx);
}

/**
 * Free up per-record state
 */
static void FreeRecordPMI(PartitionMatchInfo *pmi)
{
	if (pmi->tupleDesc != NULL)
	{
		pfree(pmi->values);
		pfree(pmi->nulls);
		ReleaseTupleDesc(pmi->tupleDesc);
	}
}

/**
 * Partition was chosen. Update partition match info.
 */
static void AddPartitionMatch(PartitionMatchInfo *pmi, Oid partOid)
{
	Assert(pmi);
	Assert(partOid != InvalidOid);
	Assert(CurrentMemoryContext == pmi->mctx);

	bool found = false;

	if (gp_log_dynamic_partition_pruning)
	{
		elog(DEBUG2, "matched partition %s", get_rel_name(partOid));
	}

	Oid *value = hash_search(pmi->matchingPartitionOids, &partOid,
			HASH_ENTER, &found);

	if (!found)
	{
		*value = partOid;
	}
}

/**
 * Create a zero element array of Oids.
 */
static ArrayType *CreateEmptyPartitionOidArray()
{
	ArrayType *array = construct_array(NULL, 0, OIDOID, sizeof(Oid), true, 'i');
	return array;
}

/**
 * Construct an array of partition oids from partition match info.
 */
static ArrayType *ExtractPartitionOidArray(PartitionMatchInfo *pmi)
{
	Datum *elements = NULL;
	long numPartitions = 0;
	GetSelectedPartitionOids(pmi->matchingPartitionOids, &elements, &numPartitions);
	Assert(NULL != elements);

	ArrayType *array = construct_array(elements, numPartitions, OIDOID, sizeof(Oid), true, 'i');

	if (gp_log_dynamic_partition_pruning)
	{
		elog(LOG, "DPE matched partitions: %s", DebugPartitionOid(elements, numPartitions));
	}

	pfree(elements);

	return array;
}

/**
 * Transition function for the pg_partition_oid aggregate function.
 * Function takes three arguments:
 *  internal - pointer to internal state (instance of PartitionMatchInfo object)
 *  oid - oid of a partitioned table
 *  record - record of the same type as the partitioned table
 * Returns:
 *  internal - pointer to updated internal state
 */
Datum
pg_partition_oid_transfn(PG_FUNCTION_ARGS)
{
	/**
	 * Second and third arguments cannot be null
	 */
	Assert(!PG_ARGISNULL(1));
	Assert(!PG_ARGISNULL(2));

	/**
	 * Calling this function on a segment is not supported currently.
	 */
	insist_log(GpIdentity.segindex == -1, "function cannot be executed on segment");

	Oid				partitionOid = PG_GETARG_OID(1);
	HeapTupleHeader record = PG_GETARG_HEAPTUPLEHEADER(2);

	Assert(fcinfo->context && IS_AGG_EXECUTION_NODE(fcinfo->context));

	PartitionMatchInfo *pmi = NULL;
	if (PG_ARGISNULL(0))
	{
		MemoryContext oldCtx = MemoryContextSwitchTo(((AggState*)fcinfo->context)->aggcontext);
		pmi = (PartitionMatchInfo *) palloc(sizeof(PartitionMatchInfo));
		Assert(partitionOid != InvalidOid);
		InitPMI(pmi, partitionOid, CurrentMemoryContext);
		MemoryContextSwitchTo(oldCtx);
	}
	else
	{
		pmi = (PartitionMatchInfo *) PG_GETARG_POINTER(0);
	}

	AddRecordPMI(pmi, record);
	PG_RETURN_POINTER(pmi);
}

/**
 * The finalize function of pg_partition_oid aggregate function. It takes a pointer
 * to the internal state (possibly, NULL) to compute an array of Oids of partitions
 * that may hold matching tuples.
 */
Datum
pg_partition_oid_finalfn(PG_FUNCTION_ARGS)
{
	/**
	 * Calling this function on a segment is not supported currently.
	 */
	insist_log(GpIdentity.segindex == -1, "function cannot be executed on segment");

	if (PG_ARGISNULL(0))
	{
		/**
		 * Transition state not initialized. No input was received. Result is an empty array
		 */
		return PointerGetDatum(CreateEmptyPartitionOidArray());
	}
	else
	{
		PartitionMatchInfo *pmi = (PartitionMatchInfo *) PG_GETARG_POINTER(0);
		Datum result = PointerGetDatum(ExtractPartitionOidArray(pmi));
		FreeRecordPMI(pmi);
		return result;
	}
}

RestrictInfo *
make_mergeclause(Node *outer, Node *inner)
{
	OpExpr	   *opxpr;
	Expr	   *xpr;
	RestrictInfo *rinfo;

	opxpr = (OpExpr *) make_op(NULL, list_make1(makeString("=")),
							   outer,
							   inner, -1);
	opxpr->xpr.type = T_DistinctExpr;

	xpr = make_notclause((Expr *) opxpr);

	rinfo = make_restrictinfo(xpr, false, false, false, NULL, NULL, NULL);
	rinfo->mergeopfamilies = get_mergejoin_opfamilies(opxpr->opno);

	return rinfo;
}
