/*--------------------------------------------------------------------------
 *
 * partitionselection.c
 *	  Provides utility routines to support partition selection.
 *
 * Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/partitionselection.c
 *
 *--------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "cdb/partitionselection.h"
#include "cdb/cdbpartition.h"
#include "executor/executor.h"
#include "parser/parse_expr.h"
#include "utils/memutils.h"

/*
 * During attribute re-mapping for heterogeneous partitions, we use
 * this struct to identify which varno's attributes will be re-mapped.
 * Using this struct as a *context* during expression tree walking, we
 * can skip varattnos that do not belong to a given varno.
 */
typedef struct AttrMapContext
{
	const AttrNumber *newattno; /* The mapping table to remap the varattno */
	Index		varno;			/* Which rte's varattno to re-map */
} AttrMapContext;

static bool change_varattnos_varno_walker(Node *node, const AttrMapContext *attrMapCxt);

/* ----------------------------------------------------------------
 *		eval_propagation_expression
 *
 *		Evaluate the propagation expression for the given leaf part Oid
 *		and return the result
 *
 * ----------------------------------------------------------------
 */
static int32
eval_propagation_expression(PartitionSelectorState *node, Oid part_oid)
{
	ExprState  *propagationExprState = node->propagationExprState;

	ExprContext *econtext = node->ps.ps_ExprContext;

	ResetExprContext(econtext);
	bool		isNull = false;
	ExprDoneCond isDone = ExprSingleResult;
	Datum		result = ExecEvalExpr(propagationExprState, econtext, &isNull, &isDone);

	return DatumGetInt32(result);
}

/* ----------------------------------------------------------------
 *		eval_part_qual
 *
 *		Evaluate a qualification expression that consists of
 *		PartDefaultExpr, PartBoundExpr, PartBoundInclusionExpr, PartBoundOpenExpr,
 *		PartListRuleExpr and PartListNullTestExpr.
 *
 *		Return true is passed, otherwise false.
 *
 * ----------------------------------------------------------------
 */
static bool
eval_part_qual(ExprState *exprstate, PartitionSelectorState *node, TupleTableSlot *inputTuple)
{
	/* evaluate generalPredicate */
	ExprContext *econtext = node->ps.ps_ExprContext;

	ResetExprContext(econtext);
	econtext->ecxt_outertuple = inputTuple;
	econtext->ecxt_scantuple = inputTuple;

	List	   *qualList = list_make1(exprstate);

	return ExecQual(qualList, econtext, false /* result is not for null */ );
}

/* ----------------------------------------------------------------
 *		partition_selection
 *
 *		It finds a child PartitionRule for a given parent partitionNode, which
 *		satisfies with the given partition key value.
 *
 *		If no such a child partitionRule is found, return NULL.
 *
 *		Input parameters:
 *		pn: parent PartitionNode
 *		accessMethods: PartitionAccessMethods
 *		root_oid: root table Oid
 *		value: partition key value
 *		exprTypid: type of the expression
 *
 * ----------------------------------------------------------------
 */
static PartitionRule *
partition_selection(PartitionNode *pn, PartitionAccessMethods *accessMethods, Oid root_oid, Datum value, Oid exprTypid, bool isNull)
{
	Assert(NULL != pn);
	Assert(NULL != accessMethods);
	Partition  *part = pn->part;

	Assert(1 == part->parnatts);
	AttrNumber	partAttno = part->paratts[0];

	Assert(0 < partAttno);

	Relation	rel = relation_open(root_oid, NoLock);
	TupleDesc	tupDesc = RelationGetDescr(rel);

	Assert(tupDesc->natts >= partAttno);

	int			i;
	Datum	   *values = palloc0(partAttno * sizeof(Datum));
	bool	   *isnull = palloc(partAttno * sizeof(bool));

	for (i = 0; i < partAttno - 1; i++)
		isnull[i] = true;
	isnull[partAttno - 1] = isNull;
	values[partAttno - 1] = value;

	PartitionRule *result = get_next_level_matched_partition(pn, values, isnull, tupDesc, accessMethods, exprTypid);

	pfree(values);
	pfree(isnull);
	relation_close(rel, NoLock);

	return result;
}

/* ----------------------------------------------------------------
 *		partition_rules_for_general_predicate
 *
 *		Returns a list of PartitionRule for the general predicate
 *		of current partition level
 *
 * ----------------------------------------------------------------
 */
static List *
partition_rules_for_general_predicate(PartitionSelectorState *node, int level,
									  TupleTableSlot *inputTuple, PartitionNode *parentNode)
{
	Assert(NULL != node);
	Assert(NULL != parentNode);

	List	   *result = NIL;
	ListCell   *lc = NULL;

	foreach(lc, parentNode->rules)
	{
		PartitionRule *rule = (PartitionRule *) lfirst(lc);

		/*
		 * We need to register it to allLevelParts to evaluate the current
		 * predicate
		 */
		node->levelPartRules[level] = rule;

		/* evaluate generalPredicate */
		ExprState  *exprstate = (ExprState *) lfirst(list_nth_cell(node->levelExprStates, level));

		if (eval_part_qual(exprstate, node, inputTuple))
		{
			result = lappend(result, rule);
		}
	}

	if (parentNode->default_part)
	{
		/*
		 * We need to register it to allLevelParts to evaluate the current
		 * predicate
		 */
		node->levelPartRules[level] = parentNode->default_part;

		/* evaluate generalPredicate */
		ExprState  *exprstate = (ExprState *) lfirst(list_nth_cell(node->levelExprStates, level));

		if (eval_part_qual(exprstate, node, inputTuple))
		{
			result = lappend(result, parentNode->default_part);
		}
	}
	/* reset allLevelPartConstraints */
	node->levelPartRules[level] = NULL;

	return result;
}

/* ----------------------------------------------------------------
 *		partition_rules_for_equality_predicate
 *
 *		Return the PartitionRule for the equality predicate
 *		of current partition level
 *
 * ----------------------------------------------------------------
 */
static PartitionRule *
partition_rules_for_equality_predicate(PartitionSelectorState *node, int level,
									   TupleTableSlot *inputTuple, PartitionNode *parentNode)
{
	Assert(NULL != node);
	Assert(NULL != node->ps.plan);
	Assert(NULL != parentNode);
	PartitionSelector *ps = (PartitionSelector *) node->ps.plan;

	Assert(level < ps->nLevels);

	/* evaluate equalityPredicate to get partition identifier value */
	ExprState  *exprState = (ExprState *) lfirst(list_nth_cell(node->levelEqExprStates, level));

	ExprContext *econtext = node->ps.ps_ExprContext;

	ResetExprContext(econtext);
	econtext->ecxt_outertuple = inputTuple;
	econtext->ecxt_scantuple = inputTuple;

	bool		isNull = false;
	ExprDoneCond isDone = ExprSingleResult;
	Datum		value = ExecEvalExpr(exprState, econtext, &isNull, &isDone);

	/*
	 * Compute the type of the expression result. Sometimes this can be
	 * different than the type of the partition rules (MPP-25707), and we'll
	 * need this type to choose the correct comparator.
	 */
	Oid			exprTypid = exprType((Node *) exprState->expr);

	return partition_selection(parentNode, node->accessMethods, ps->relid, value, exprTypid, isNull);
}

/* ----------------------------------------------------------------
 *		processLevel
 *
 *		find out satisfied PartOids for the given predicates in the
 *		given partition level
 *
 *		The function is recursively called:
 *		1. If we are in the intermediate level, we register the
 *		satisfied PartOids and continue with the next level
 *		2. If we are in the leaf level, we will propagate satisfied
 *		PartOids.
 *
 *		The return structure contains the leaf part oids and the ids of the scan
 *		operators to which they should be propagated
 *
 *		Input parameters:
 *		node: PartitionSelectorState
 *		level: the current partition level, starting with 0.
 *		inputTuple: input tuple from outer child for join partition
 *		elimination
 *
 * ----------------------------------------------------------------
 */
SelectedParts *
processLevel(PartitionSelectorState *node, int level, TupleTableSlot *inputTuple)
{
	SelectedParts *selparts = makeNode(SelectedParts);

	selparts->partOids = NIL;
	selparts->scanIds = NIL;

	Assert(NULL != node->ps.plan);
	PartitionSelector *ps = (PartitionSelector *) node->ps.plan;

	Assert(level < ps->nLevels);

	/* get equality and general predicate for the current level */
	Expr	   *equalityPredicate = (Expr *) lfirst(list_nth_cell(ps->levelEqExpressions, level));
	Expr	   *generalPredicate = (Expr *) lfirst(list_nth_cell(ps->levelExpressions, level));

	/* get parent PartitionNode if in level 0, it's the root PartitionNode */
	PartitionNode *parentNode = node->rootPartitionNode;

	if (0 != level)
	{
		Assert(NULL != node->levelPartRules[level - 1]);
		parentNode = node->levelPartRules[level - 1]->children;
	}

	/* list of PartitionRule that satisfied the predicates */
	List	   *satisfiedRules = NIL;

	/* If equalityPredicate exists */
	if (NULL != equalityPredicate)
	{
		Assert(NULL == generalPredicate);

		PartitionRule *chosenRule = partition_rules_for_equality_predicate(node, level, inputTuple, parentNode);

		if (chosenRule != NULL)
		{
			satisfiedRules = lappend(satisfiedRules, chosenRule);
		}
	}
	/* If generalPredicate exists */
	else if (NULL != generalPredicate)
	{
		List	   *chosenRules = partition_rules_for_general_predicate(node, level, inputTuple, parentNode);

		satisfiedRules = list_concat(satisfiedRules, chosenRules);
	}
	/* None of the predicate exists */
	else
	{
		/*
		 * Neither equality predicate nor general predicate exists. Return all
		 * the next level PartitionRule.
		 *
		 * WARNING: Do NOT use list_concat with satisfiedRules and
		 * parentNode->rules. list_concat will destructively modify
		 * satisfiedRules to point to parentNode->rules, which will then be
		 * freed when we free satisfiedRules. This does not apply when we
		 * execute partition_rules_for_general_predicate as it creates its own
		 * list.
		 */
		ListCell   *lc = NULL;

		foreach(lc, parentNode->rules)
		{
			PartitionRule *rule = (PartitionRule *) lfirst(lc);

			satisfiedRules = lappend(satisfiedRules, rule);
		}

		if (NULL != parentNode->default_part)
		{
			satisfiedRules = lappend(satisfiedRules, parentNode->default_part);
		}
	}

	/*
	 * Based on the satisfied PartitionRules, go to next level or propagate
	 * PartOids if we are in the leaf level
	 */
	ListCell   *lc = NULL;

	foreach(lc, satisfiedRules)
	{
		PartitionRule *rule = (PartitionRule *) lfirst(lc);

		node->levelPartRules[level] = rule;

		/* If we already in the leaf level */
		if (level == ps->nLevels - 1)
		{
			bool		shouldPropagate = true;

			/* if residual predicate exists */
			if (NULL != ps->residualPredicate)
			{
				/* evaluate residualPredicate */
				ExprState  *exprstate = node->residualPredicateExprState;

				shouldPropagate = eval_part_qual(exprstate, node, inputTuple);
			}

			if (shouldPropagate)
			{
				if (NULL != ps->propagationExpression)
				{
					if (!list_member_oid(selparts->partOids, rule->parchildrelid))
					{
						selparts->partOids = lappend_oid(selparts->partOids, rule->parchildrelid);
						int			scanId = eval_propagation_expression(node, rule->parchildrelid);

						selparts->scanIds = lappend_int(selparts->scanIds, scanId);
					}
				}
			}
		}

		/*
		 * Recursively call this function for next level's partition
		 * elimination
		 */
		else
		{
			SelectedParts *selpartsChild = processLevel(node, level + 1, inputTuple);

			selparts->partOids = list_concat(selparts->partOids, selpartsChild->partOids);
			selparts->scanIds = list_concat(selparts->scanIds, selpartsChild->scanIds);
			pfree(selpartsChild);
		}
	}

	list_free(satisfiedRules);

	/* After finish iteration, reset this level's PartitionRule */
	node->levelPartRules[level] = NULL;

	return selparts;
}

/* ----------------------------------------------------------------
 *		initPartitionSelection
 *
 *		Initialize partition selection state information
 *
 * ----------------------------------------------------------------
 */
PartitionSelectorState *
initPartitionSelection(PartitionSelector *node, EState *estate)
{
	/* create and initialize PartitionSelectorState structure */
	PartitionSelectorState *psstate;
	ListCell   *lc;

	psstate = makeNode(PartitionSelectorState);
	psstate->ps.plan = (Plan *) node;
	psstate->ps.state = estate;
	psstate->levelPartRules = (PartitionRule **) palloc0(node->nLevels * sizeof(PartitionRule *));

	/* ExprContext initialization */
	ExecAssignExprContext(estate, &psstate->ps);

	/* initialize ExprState for evaluating expressions */
	foreach(lc, node->levelEqExpressions)
	{
		Expr	   *eqExpr = (Expr *) lfirst(lc);

		psstate->levelEqExprStates = lappend(psstate->levelEqExprStates,
											 ExecInitExpr(eqExpr, (PlanState *) psstate));
	}

	foreach(lc, node->levelExpressions)
	{
		Expr	   *generalExpr = (Expr *) lfirst(lc);

		psstate->levelExprStates = lappend(psstate->levelExprStates,
										   ExecInitExpr(generalExpr, (PlanState *) psstate));
	}

	psstate->residualPredicateExprState = ExecInitExpr((Expr *) node->residualPredicate,
													   (PlanState *) psstate);
	psstate->propagationExprState = ExecInitExpr((Expr *) node->propagationExpression,
												 (PlanState *) psstate);

	psstate->ps.targetlist = (List *) ExecInitExpr((Expr *) node->plan.targetlist,
												   (PlanState *) psstate);

	return psstate;
}

/* ----------------------------------------------------------------
 *		getPartitionNodeAndAccessMethod
 *
 * 		Retrieve PartitionNode and access method from root table
 *
 * ----------------------------------------------------------------
 */
void
getPartitionNodeAndAccessMethod(Oid rootOid, List *partsMetadata, MemoryContext memoryContext,
								PartitionNode **partsAndRules, PartitionAccessMethods **accessMethods)
{
	Assert(NULL != partsMetadata);
	findPartitionMetadataEntry(partsMetadata, rootOid, partsAndRules, accessMethods);
	Assert(NULL != (*partsAndRules));
	Assert(NULL != (*accessMethods));
	(*accessMethods)->part_cxt = memoryContext;
}

/* ----------------------------------------------------------------
 *		static_part_selection
 *
 *		Statically select leaf part oids during optimization time
 *
 * ----------------------------------------------------------------
 */
SelectedParts *
static_part_selection(PartitionSelector *ps)
{
	List	   *partsMetadata;
	PartitionSelectorState *psstate;
	EState	   *estate;
	MemoryContext oldcxt;
	SelectedParts *selparts;

	estate = CreateExecutorState();

	oldcxt = MemoryContextSwitchTo(estate->es_query_cxt);

	partsMetadata = InitializePartsMetadata(ps->relid);
	psstate = initPartitionSelection(ps, estate);

	getPartitionNodeAndAccessMethod
		(
		 ps->relid,
		 partsMetadata,
		 estate->es_query_cxt,
		 &psstate->rootPartitionNode,
		 &psstate->accessMethods
		);

	MemoryContextSwitchTo(oldcxt);

	selparts = processLevel(psstate, 0 /* level */ , NULL /* inputSlot */ );

	/* cleanup */
	FreeExecutorState(estate);

	return selparts;
}

/*
 * Generate a map for change_varattnos_of_a_node from old and new TupleDesc's,
 * matching according to column name. This function returns a NULL pointer (i.e.
 * null map) if no mapping is necessary (i.e., old and new TupleDesc are already
 * aligned).
 *
 * This function, and change_varattnos_of_a_varno below, used to be in
 * tablecmds.c, but were removed in upstream commit 188a0a00. But we still need
 * this for dynamic partition selection in GPDB, so copied them here.
 */
AttrNumber *
varattnos_map(TupleDesc old, TupleDesc new)
{
	AttrNumber *attmap;
	int			i,
				j;

	bool		mapRequired = false;

	attmap = (AttrNumber *) palloc0(sizeof(AttrNumber) * old->natts);
	for (i = 1; i <= old->natts; i++)
	{
		if (old->attrs[i - 1]->attisdropped)
			continue;			/* leave the entry as zero */

		for (j = 1; j <= new->natts; j++)
		{
			if (strcmp(NameStr(old->attrs[i - 1]->attname),
					   NameStr(new->attrs[j - 1]->attname)) == 0)
			{
				attmap[i - 1] = j;

				if (i != j)
				{
					mapRequired = true;
				}
				break;
			}
		}
	}

	if (!mapRequired)
	{
		pfree(attmap);

		/* No mapping required, so return NULL */
		attmap = NULL;
	}

	return attmap;
}

/*
 * Replace varattno values in an expression tree according to the given
 * map array, that is, varattno N is replaced by newattno[N-1].  It is
 * caller's responsibility to ensure that the array is long enough to
 * define values for all user varattnos present in the tree.  System column
 * attnos remain unchanged. For historical reason, we only map varattno of the first
 * range table entry from this method. So, we call the more general
 * change_varattnos_of_a_varno() with varno set to 1
 *
 * Note that the passed node tree is modified in-place!
 */
void
change_varattnos_of_a_node(Node *node, const AttrNumber *newattno)
{
	/*
	 * Only attempt re-mapping if re-mapping is necessary (i.e., non-null
	 * newattno map)
	 */
	if (newattno)
	{
		change_varattnos_of_a_varno(node, newattno, 1	/* varno is hard-coded
														 * to 1 (i.e., only
									  * first RTE) */ );
	}
}

/*
 * Replace varattno values for a given varno RTE index in an expression
 * tree according to the given map array, that is, varattno N is replaced
 * by newattno[N-1].  It is caller's responsibility to ensure that the array
 * is long enough to define values for all user varattnos present in the tree.
 * System column attnos remain unchanged.
 *
 * Note that the passed node tree is modified in-place!
 */
void
change_varattnos_of_a_varno(Node *node, const AttrNumber *newattno, Index varno)
{
	AttrMapContext attrMapCxt;

	attrMapCxt.newattno = newattno;
	attrMapCxt.varno = varno;

	(void) change_varattnos_varno_walker(node, &attrMapCxt);
}

/*
 * Remaps the varattno of a varattno in a Var node using an attribute map.
 */
static bool
change_varattnos_varno_walker(Node *node, const AttrMapContext *attrMapCxt)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varlevelsup == 0 && (var->varno == attrMapCxt->varno) &&
			var->varattno > 0)
		{
			/*
			 * ??? the following may be a problem when the node is multiply
			 * referenced though stringToNode() doesn't create such a node
			 * currently.
			 */
			Assert(attrMapCxt->newattno[var->varattno - 1] > 0);
			var->varattno = var->varoattno = attrMapCxt->newattno[var->varattno - 1];
		}
		return false;
	}
	return expression_tree_walker(node, change_varattnos_varno_walker,
								  (void *) attrMapCxt);
}
