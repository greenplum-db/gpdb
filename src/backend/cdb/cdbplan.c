/*-------------------------------------------------------------------------
 *
 * cdbplan.c
 *	  Provides routines supporting plan tree manipulation.
 *
 * Portions Copyright (c) 2004-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbplan.c
 *
 * NOTES
 *	See src/backend/optimizer/util/clauses.c for background information
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbgroup.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbsetop.h"
#include "miscadmin.h"
#include "nodes/primnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "optimizer/clauses.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"


static void mutate_plan_fields(Plan *newplan, Plan *oldplan, Node *(*mutator) (), void *context);
static void mutate_join_fields(Join *newplan, Join *oldplan, Node *(*mutator) (), void *context);




/* ----------------------------------------------------------------------- *
 * Plan Tree Mutator Framework
 * ----------------------------------------------------------------------- *
 */

/*--------------------
 * expression_tree_mutator() is designed to support routines that make a
 * modified copy of an expression tree, with some nodes being added,
 * removed, or replaced by new subtrees.  The original tree is (normally)
 * not changed.  Each recursion level is responsible for returning a copy of
 * (or appropriately modified substitute for) the subtree it is handed.
 * A mutator routine should look like this:
 *
 * Node * my_mutator (Node *node, my_struct *context)
 * {
 *		if (node == NULL)
 *			return NULL;
 *		// check for nodes that special work is required for, eg:
 *		if (IsA(node, Var))
 *		{
 *			... create and return modified copy of Var node
 *		}
 *		else if (IsA(node, ...))
 *		{
 *			... do special transformations of other node types
 *		}
 *		// for any node type not specially processed, do:
 *		return expression_tree_mutator(node, my_mutator, (void *) context);
 * }
 *
 * The "context" argument points to a struct that holds whatever context
 * information the mutator routine needs --- it can be used to return extra
 * data gathered by the mutator, too.  This argument is not touched by
 * expression_tree_mutator, but it is passed down to recursive sub-invocations
 * of my_mutator.  The tree walk is started from a setup routine that
 * fills in the appropriate context struct, calls my_mutator with the
 * top-level node of the tree, and does any required post-processing.
 *
 * Each level of recursion must return an appropriately modified Node.
 * If expression_tree_mutator() is called, it will make an exact copy
 * of the given Node, but invoke my_mutator() to copy the sub-node(s)
 * of that Node.  In this way, my_mutator() has full control over the
 * copying process but need not directly deal with expression trees
 * that it has no interest in.
 *
 * Just as for expression_tree_walker, the node types handled by
 * expression_tree_mutator include all those normally found in target lists
 * and qualifier clauses during the planning stage.
 *
 * expression_tree_mutator will handle SubLink nodes by recursing normally
 * into the "lefthand" arguments (which are expressions belonging to the outer
 * plan).  It will also call the mutator on the sub-Query node; however, when
 * expression_tree_mutator itself is called on a Query node, it does nothing
 * and returns the unmodified Query node.  The net effect is that unless the
 * mutator does something special at a Query node, sub-selects will not be
 * visited or modified; the original sub-select will be linked to by the new
 * SubLink node.  Mutators that want to descend into sub-selects will usually
 * do so by recognizing Query nodes and calling query_tree_mutator (below).
 *
 * expression_tree_mutator will handle a SubPlan node by recursing into
 * the "exprs" and "args" lists (which belong to the outer plan), but it
 * will simply copy the link to the inner plan, since that's typically what
 * expression tree mutators want.  A mutator that wants to modify the subplan
 * can force appropriate behavior by recognizing SubPlan expression nodes
 * and doing the right thing.
 *--------------------
 */

Node *
plan_tree_mutator(Node *node,
				  Node *(*mutator) (),
				  void *context,
				  bool recurse_into_subplans)
{
	/*
	 * The mutator has already decided not to modify the current node, but we
	 * must call the mutator for any sub-nodes.
	 */

#define FLATCOPY(newnode, node, nodetype)  \
	( (newnode) = makeNode(nodetype), \
	  memcpy((newnode), (node), sizeof(nodetype)) )

#define CHECKFLATCOPY(newnode, node, nodetype)	\
	( AssertMacro(IsA((node), nodetype)), \
	  (newnode) = makeNode(nodetype), \
	  memcpy((newnode), (node), sizeof(nodetype)) )

#define MUTATE(newfield, oldfield, fieldtype)  \
		( (newfield) = (fieldtype) mutator((Node *) (oldfield), context) )

#define PLANMUTATE(newplan, oldplan) \
		mutate_plan_fields((Plan*)(newplan), (Plan*)(oldplan), mutator, context)

/* This is just like  PLANMUTATE because Scan adds only scalar fields. */
#define SCANMUTATE(newplan, oldplan) \
		mutate_plan_fields((Plan*)(newplan), (Plan*)(oldplan), mutator, context)

#define JOINMUTATE(newplan, oldplan) \
		mutate_join_fields((Join*)(newplan), (Join*)(oldplan), mutator, context)

#define COPYARRAY(dest,src,lenfld,datfld) \
	do { \
		(dest)->lenfld = (src)->lenfld; \
		if ( (src)->lenfld > 0  && \
             (src)->datfld != NULL) \
		{ \
			Size _size = ((src)->lenfld*sizeof(*((src)->datfld))); \
			(dest)->datfld = palloc(_size); \
			memcpy((dest)->datfld, (src)->datfld, _size); \
		} \
		else \
		{ \
			(dest)->datfld = NULL; \
		} \
	} while (0)



	if (node == NULL)
		return NULL;

	/* Guard against stack overflow due to overly complex expressions */
	check_stack_depth();

	switch (nodeTag(node))
	{
			/*
			 * Plan nodes aren't handled by expression_tree_walker, so we need
			 * to do them here.
			 */
		case T_Plan:
			/* Abstract: Should see only subclasses. */
			elog(ERROR, "abstract node type not allowed: T_Plan");
			break;

		case T_Result:
			{
				Result	   *result = (Result *) node;
				Result	   *newresult;

				FLATCOPY(newresult, result, Result);
				PLANMUTATE(newresult, result);
				MUTATE(newresult->resconstantqual, result->resconstantqual, Node *);
				return (Node *) newresult;
			}
			break;

		case T_ProjectSet:
			{
				ProjectSet *ps = (ProjectSet *) node;
				ProjectSet *newps;

				FLATCOPY(newps, ps, ProjectSet);
				PLANMUTATE(newps, ps);
				return (Node *) newps;
			}
			break;

		case T_ModifyTable:
			{
				ModifyTable *mt = (ModifyTable *) node;
				ModifyTable *newmt;

				FLATCOPY(newmt, mt, ModifyTable);
				PLANMUTATE(newmt, mt);
				MUTATE(newmt->plans, mt->plans, List *);
				MUTATE(newmt->onConflictSet, mt->onConflictSet, List *);
				MUTATE(newmt->onConflictWhere, mt->onConflictWhere , Node *);
				MUTATE(newmt->withCheckOptionLists, mt->withCheckOptionLists, List *);
				MUTATE(newmt->returningLists, mt->returningLists, List *);
				return (Node *) newmt;
			}
			break;

		case T_LockRows:
			{
				LockRows   *lockrows = (LockRows *) node;
				LockRows   *newlockrows;

				FLATCOPY(newlockrows, lockrows, LockRows);
				PLANMUTATE(newlockrows, lockrows);
				return (Node *) newlockrows;
			}
			break;

		case T_Append:
			{
				Append	   *append = (Append *) node;
				Append	   *newappend;

				FLATCOPY(newappend, append, Append);
				PLANMUTATE(newappend, append);
				MUTATE(newappend->appendplans, append->appendplans, List *);
				return (Node *) newappend;
			}
			break;

		case T_MergeAppend:
			{
				MergeAppend	   *merge = (MergeAppend *) node;
				MergeAppend	   *newmerge;

				FLATCOPY(newmerge, merge, MergeAppend);
				PLANMUTATE(newmerge, merge);
				MUTATE(newmerge->mergeplans, merge->mergeplans, List *);
				return (Node *) newmerge;
			}
			break;

		case T_RecursiveUnion:
			{
				RecursiveUnion *ru = (RecursiveUnion *) node;
				RecursiveUnion *newru;

				FLATCOPY(newru, ru, RecursiveUnion);
				PLANMUTATE(newru, ru);
				return (Node *) newru;
			}

		case T_Sequence:
			{
				Sequence   *sequence = (Sequence *) node;
				Sequence   *newSequence = NULL;

				FLATCOPY(newSequence, sequence, Sequence);
				PLANMUTATE(newSequence, sequence);
				MUTATE(newSequence->subplans, sequence->subplans, List *);

				return (Node *) newSequence;
			}

		case T_AssertOp:
			{
				AssertOp   *assert = (AssertOp *) node;
				AssertOp   *newAssert = NULL;

				FLATCOPY(newAssert, assert, AssertOp);
				PLANMUTATE(newAssert, assert);

				return (Node *) newAssert;
			}

		case T_PartitionSelector:
			{
				PartitionSelector *partsel = (PartitionSelector *) node;
				PartitionSelector *newPartsel = NULL;

				FLATCOPY(newPartsel, partsel, PartitionSelector);
				PLANMUTATE(newPartsel, partsel);

				return (Node *) newPartsel;
			}

		case T_BitmapAnd:
			{
				BitmapAnd  *old = (BitmapAnd *) node;
				BitmapAnd  *mut;

				FLATCOPY(mut, old, BitmapAnd);
				PLANMUTATE(mut, old);
				MUTATE(mut->bitmapplans, old->bitmapplans, List *);
				return (Node *) mut;
			}
			break;
		case T_BitmapOr:
			{
				BitmapOr   *old = (BitmapOr *) node;
				BitmapOr   *mut;

				FLATCOPY(mut, old, BitmapOr);
				PLANMUTATE(mut, old);
				MUTATE(mut->bitmapplans, old->bitmapplans, List *);
				return (Node *) mut;
			}
			break;

		case T_Scan:
			/* Abstract: Should see only subclasses. */
			elog(ERROR, "abstract node type not allowed: T_Scan");
			break;

		case T_SampleScan:
			{
				SampleScan    *samplescan = (SampleScan *) node;
				SampleScan    *newsamplescan;

				FLATCOPY(newsamplescan, samplescan, SampleScan);
				SCANMUTATE(newsamplescan, samplescan);
				return (Node *) newsamplescan;
			}
			break;

		case T_CustomScan:
			{
				CustomScan    *cscan = (CustomScan *) node;
				CustomScan    *newcscan;

				FLATCOPY(newcscan, cscan, CustomScan);
				SCANMUTATE(newcscan, cscan);
				return (Node *) newcscan;
			}
			break;

		case T_SeqScan:
			{
				SeqScan    *seqscan = (SeqScan *) node;
				SeqScan    *newseqscan;

				FLATCOPY(newseqscan, seqscan, SeqScan);
				SCANMUTATE(newseqscan, seqscan);
				/* A SeqScan is really just a Scan, so we're done. */
				return (Node *) newseqscan;
			}
			break;

		case T_DynamicSeqScan:
			{
				DynamicSeqScan *dynamicSeqScan = (DynamicSeqScan *) node;
				DynamicSeqScan *newDynamicSeqScan = NULL;

				FLATCOPY(newDynamicSeqScan, dynamicSeqScan, DynamicSeqScan);
				SCANMUTATE(newDynamicSeqScan, dynamicSeqScan);
				return (Node *) newDynamicSeqScan;
			}
			break;

		case T_IndexScan:
		case T_DynamicIndexScan:
			{
				IndexScan  *idxscan = (IndexScan *) node;
				IndexScan  *newidxscan;

				if (IsA(node, DynamicIndexScan))
				{
					/*
					 * A DynamicIndexScan is identical to IndexScan, except for
					 * additional fields. This convoluted coding is to avoid
					 * copy-pasting this code and risking bugs of omission if
					 * new fields are added to IndexScan in upstream.
					 */
					DynamicIndexScan *newdiscan;

					FLATCOPY(newdiscan, idxscan, DynamicIndexScan);
					newidxscan = (IndexScan *) newdiscan;
				}
				else
					FLATCOPY(newidxscan, idxscan, IndexScan);
				SCANMUTATE(newidxscan, idxscan);
				newidxscan->indexid = idxscan->indexid;
				/* MUTATE(newidxscan->indexid, idxscan->indexid, List *); */
				MUTATE(newidxscan->indexqual, idxscan->indexqual, List *);
				MUTATE(newidxscan->indexqualorig, idxscan->indexqualorig, List *);
				/* indxorderdir  is  scalar */
				return (Node *) newidxscan;
			}
			break;

		case T_IndexOnlyScan:
			{
				IndexOnlyScan  *idxonlyscan = (IndexOnlyScan *) node;
				IndexOnlyScan  *newidxonlyscan;

				FLATCOPY(newidxonlyscan, idxonlyscan, IndexOnlyScan);
				SCANMUTATE(newidxonlyscan, idxonlyscan);
				newidxonlyscan->indexid = idxonlyscan->indexid;
				/* MUTATE(newidxonlyscan->indexid, idxonlyscan->indexid, List *); */
				MUTATE(newidxonlyscan->indexqual, idxonlyscan->indexqual, List *);
				MUTATE(newidxonlyscan->indextlist, idxonlyscan->indextlist, List *);
				/* indxorderdir  is  scalar */
				return (Node *) newidxonlyscan;
			}
			break;

		case T_BitmapIndexScan:
		case T_DynamicBitmapIndexScan:
			{
				BitmapIndexScan *idxscan = (BitmapIndexScan *) node;
				BitmapIndexScan *newidxscan;

				if (IsA(node, DynamicBitmapIndexScan))
				{
					/* see comment above on DynamicIndexScan */
					DynamicBitmapIndexScan *newdbiscan;

					FLATCOPY(newdbiscan, idxscan, DynamicBitmapIndexScan);
					newidxscan = (BitmapIndexScan *) newdbiscan;
				}
				else
					FLATCOPY(newidxscan, idxscan, BitmapIndexScan);
				SCANMUTATE(newidxscan, idxscan);
				newidxscan->indexid = idxscan->indexid;

				MUTATE(newidxscan->indexqual, idxscan->indexqual, List *);
				MUTATE(newidxscan->indexqualorig, idxscan->indexqualorig, List *);

				return (Node *) newidxscan;
			}
			break;

		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
			{
				BitmapHeapScan *bmheapscan = (BitmapHeapScan *) node;
				BitmapHeapScan *newbmheapscan;

				if (IsA(node, DynamicBitmapHeapScan))
				{
					/* see comment above on DynamicIndexScan */
					DynamicBitmapHeapScan *newdbhscan;

					FLATCOPY(newdbhscan, bmheapscan, DynamicBitmapHeapScan);
					newbmheapscan = (BitmapHeapScan *) newdbhscan;
				}
				else
					FLATCOPY(newbmheapscan, bmheapscan, BitmapHeapScan);
				SCANMUTATE(newbmheapscan, bmheapscan);

				MUTATE(newbmheapscan->bitmapqualorig, bmheapscan->bitmapqualorig, List *);

				return (Node *) newbmheapscan;
			}
			break;

		case T_TidScan:
			{
				TidScan    *tidscan = (TidScan *) node;
				TidScan    *newtidscan;

				FLATCOPY(newtidscan, tidscan, TidScan);
				SCANMUTATE(newtidscan, tidscan);
				MUTATE(newtidscan->tidquals, tidscan->tidquals, List *);
				/* isTarget is scalar. */
				return (Node *) newtidscan;
			}
			break;

		case T_SubqueryScan:
			{
				SubqueryScan *sqscan = (SubqueryScan *) node;
				SubqueryScan *newsqscan;

				FLATCOPY(newsqscan, sqscan, SubqueryScan);
				SCANMUTATE(newsqscan, sqscan);
				MUTATE(newsqscan->subplan, sqscan->subplan, Plan *);
				return (Node *) newsqscan;
			}
			break;

		case T_FunctionScan:
			{
				FunctionScan *fnscan = (FunctionScan *) node;
				FunctionScan *newfnscan;

				FLATCOPY(newfnscan, fnscan, FunctionScan);
				SCANMUTATE(newfnscan, fnscan);
				/* A FunctionScan is really just a Scan, so we're done. */
				return (Node *) newfnscan;
			}
			break;

		case T_ValuesScan:
			{
				ValuesScan *scan = (ValuesScan *) node;
				ValuesScan *newscan;

				FLATCOPY(newscan, scan, ValuesScan);
				MUTATE(newscan->values_lists, scan->values_lists, List *);
				SCANMUTATE(newscan, scan);
				return (Node *) newscan;
			}
			break;

		case T_TableFuncScan:
			{
				TableFuncScan *scan = (TableFuncScan *) node;
				TableFuncScan *newscan;

				FLATCOPY(newscan, scan, TableFuncScan);
				MUTATE(newscan->tablefunc, scan->tablefunc, TableFunc *);
				SCANMUTATE(newscan, scan);
				return (Node *) newscan;
			}
			break;

		case T_WorkTableScan:
			{
				WorkTableScan *wts = (WorkTableScan *) node;
				WorkTableScan *newwts;

				FLATCOPY(newwts, wts, WorkTableScan);
				SCANMUTATE(newwts, wts);

				return (Node *) newwts;
			}

		case T_NamedTuplestoreScan:
			{
				NamedTuplestoreScan *ntscan = (NamedTuplestoreScan *) node;
				NamedTuplestoreScan *newntscan;

				FLATCOPY(newntscan, ntscan, NamedTuplestoreScan);
				SCANMUTATE(newntscan, ntscan);
				return (Node *) newntscan;
			}
			break;

		case T_Join:
			/* Abstract: Should see only subclasses. */
			elog(ERROR, "abstract node type not allowed: T_Join");
			break;

		case T_NestLoop:
			{
				NestLoop   *loopscan = (NestLoop *) node;
				NestLoop   *newloopscan;

				FLATCOPY(newloopscan, loopscan, NestLoop);
				JOINMUTATE(newloopscan, loopscan);

				/* A NestLoop is really just a Join. */
				return (Node *) newloopscan;
			}
			break;
		case T_MergeJoin:
			{
				MergeJoin  *merge = (MergeJoin *) node;
				MergeJoin  *newmerge;

				FLATCOPY(newmerge, merge, MergeJoin);
				JOINMUTATE(newmerge, merge);
				MUTATE(newmerge->mergeclauses, merge->mergeclauses, List *);
				return (Node *) newmerge;
			}
			break;

		case T_HashJoin:
			{
				HashJoin   *hjoin = (HashJoin *) node;
				HashJoin   *newhjoin;

				FLATCOPY(newhjoin, hjoin, HashJoin);
				JOINMUTATE(newhjoin, hjoin);
				MUTATE(newhjoin->hashclauses, hjoin->hashclauses, List *);
				MUTATE(newhjoin->hashqualclauses, hjoin->hashqualclauses, List *);
				return (Node *) newhjoin;
			}
			break;

		case T_ShareInputScan:
			{
				ShareInputScan *sis = (ShareInputScan *) node;
				ShareInputScan *newsis;

				FLATCOPY(newsis, sis, ShareInputScan);
				PLANMUTATE(newsis, sis);
				return (Node *) newsis;
			}
			break;

		case T_Material:
			{
				Material   *material = (Material *) node;
				Material   *newmaterial;

				FLATCOPY(newmaterial, material, Material);
				PLANMUTATE(newmaterial, material);
				return (Node *) newmaterial;
			}
			break;

		case T_Sort:
			{
				Sort	   *sort = (Sort *) node;
				Sort	   *newsort;

				FLATCOPY(newsort, sort, Sort);
				PLANMUTATE(newsort, sort);
				COPYARRAY(newsort, sort, numCols, sortColIdx);
				COPYARRAY(newsort, sort, numCols, sortOperators);
				COPYARRAY(newsort, sort, numCols, nullsFirst);
				return (Node *) newsort;
			}
			break;

		case T_Agg:
			{
				Agg		   *agg = (Agg *) node;
				Agg		   *newagg;

				FLATCOPY(newagg, agg, Agg);
				PLANMUTATE(newagg, agg);
				COPYARRAY(newagg, agg, numCols, grpColIdx);
				return (Node *) newagg;
			}
			break;

		case T_TupleSplit:
			{
				TupleSplit  *tup_split = (TupleSplit *) node;
				TupleSplit  *new_tup_split;

				FLATCOPY(new_tup_split, tup_split, TupleSplit);
				PLANMUTATE(new_tup_split, tup_split);
				COPYARRAY(new_tup_split, tup_split, numCols, grpColIdx);
				MUTATE(new_tup_split->dqa_expr_lst, tup_split->dqa_expr_lst, List *);

				return (Node *) new_tup_split;
			}
			break;

		case T_TableFunctionScan:
			{
				TableFunctionScan *tabfunc = (TableFunctionScan *) node;
				TableFunctionScan *newtabfunc;

				FLATCOPY(newtabfunc, tabfunc, TableFunctionScan);
				PLANMUTATE(newtabfunc, tabfunc);
				return (Node *) newtabfunc;
			}
			break;

		case T_WindowAgg:
			{
				WindowAgg  *window = (WindowAgg *) node;
				WindowAgg  *newwindow;

				FLATCOPY(newwindow, window, WindowAgg);
				PLANMUTATE(newwindow, window);

				COPYARRAY(newwindow, window, partNumCols, partColIdx);
				COPYARRAY(newwindow, window, partNumCols, partOperators);

				COPYARRAY(newwindow, window, ordNumCols, ordColIdx);
				COPYARRAY(newwindow, window, ordNumCols, ordOperators);
				MUTATE(newwindow->startOffset, window->startOffset, Node *);
				MUTATE(newwindow->endOffset, window->endOffset, Node *);

				return (Node *) newwindow;
			}
			break;

		case T_Unique:
			{
				Unique	   *uniq = (Unique *) node;
				Unique	   *newuniq;

				FLATCOPY(newuniq, uniq, Unique);
				PLANMUTATE(newuniq, uniq);
				COPYARRAY(newuniq, uniq, numCols, uniqColIdx);
				return (Node *) newuniq;
			}
			break;

		case T_Gather:
			{
				Gather	   *gather = (Gather *) node;
				Gather	   *newgather;

				FLATCOPY(newgather, gather, Gather);
				PLANMUTATE(newgather, gather);
				return (Node *) newgather;
			}
			break;

		case T_Hash:
			{
				Hash	   *hash = (Hash *) node;
				Hash	   *newhash;

				FLATCOPY(newhash, hash, Hash);
				PLANMUTATE(newhash, hash);
				return (Node *) newhash;
			}
			break;

		case T_SetOp:
			{
				SetOp	   *setop = (SetOp *) node;
				SetOp	   *newsetop;

				FLATCOPY(newsetop, setop, SetOp);
				PLANMUTATE(newsetop, setop);
				COPYARRAY(newsetop, setop, numCols, dupColIdx);
				return (Node *) newsetop;
			}
			break;

		case T_Limit:
			{
				Limit	   *limit = (Limit *) node;
				Limit	   *newlimit;

				FLATCOPY(newlimit, limit, Limit);
				PLANMUTATE(newlimit, limit);
				MUTATE(newlimit->limitOffset, limit->limitOffset, Node *);
				MUTATE(newlimit->limitCount, limit->limitCount, Node *);
				return (Node *) newlimit;
			}
			break;

		case T_Motion:
			{
				Motion	   *motion = (Motion *) node;
				Motion	   *newmotion;

				FLATCOPY(newmotion, motion, Motion);
				PLANMUTATE(newmotion, motion);
				MUTATE(newmotion->hashExprs, motion->hashExprs, List *);
				COPYARRAY(newmotion, motion, numSortCols, sortColIdx);
				COPYARRAY(newmotion, motion, numSortCols, sortOperators);
				COPYARRAY(newmotion, motion, numSortCols, nullsFirst);
				return (Node *) newmotion;
			}
			break;


		case T_Flow:
			{
				Flow	   *flow = (Flow *) node;
				Flow	   *newflow;

				FLATCOPY(newflow, flow, Flow);
				return (Node *) newflow;
			}
			break;

		case T_IntList:
		case T_OidList:

			/*
			 * Note that expression_tree_mutator handles T_List but not these.
			 * A shallow copy will do.
			 */
			return (Node *) list_copy((List *) node);

			break;

		case T_Query:

			/*
			 * Since expression_tree_mutator doesn't descend into Query nodes,
			 * we use ...
			 */
			return (Node *) query_tree_mutator((Query *) node, mutator, context, 0);
			break;

		case T_SubPlan:

			/*
			 * Since expression_tree_mutator doesn't descend into the plan in
			 * a SubPlan node, we handle the case directly.
			 */
			{
				SubPlan    *subplan = (SubPlan *) node;
				SubPlan    *newnode;

				FLATCOPY(newnode, subplan, SubPlan);

				MUTATE(newnode->testexpr, subplan->testexpr, Node *);
				MUTATE(newnode->args, subplan->args, List *);

				if (recurse_into_subplans)
				{
					Plan	   *subplan_plan = plan_tree_base_subplan_get_plan(context, subplan);
					Plan	   *newsubplan_plan;

					MUTATE(newsubplan_plan, subplan_plan, Plan *);

					if (newsubplan_plan != subplan_plan)
						plan_tree_base_subplan_put_plan(context, newnode, newsubplan_plan);
				}

				/* An IntList isn't interesting to mutate; just copy. */
				newnode->paramIds = (List *) copyObject(subplan->paramIds);
				newnode->setParam = (List *) copyObject(subplan->setParam);
				newnode->parParam = (List *) copyObject(subplan->parParam);
				newnode->extParam = (List *) copyObject(subplan->extParam);

				return (Node *) newnode;
			}
			break;

		case T_RangeTblEntry:

			/*
			 * Also expression_tree_mutator doesn't recognize range table
			 * entries.
			 *
			 * TODO Figure out what's to do and handle this case. ***************************************************
			 *
			 */
			{
				RangeTblEntry *rte = (RangeTblEntry *) node;
				RangeTblEntry *newrte;

				FLATCOPY(newrte, rte, RangeTblEntry);
				switch (rte->rtekind)
				{
					case RTE_RELATION:	/* ordinary relation reference */
					case RTE_VOID:	/* deleted entry */
					case RTE_RESULT:
					case RTE_NAMEDTUPLESTORE:
						/* No extras. */
						break;

					case RTE_SUBQUERY:	/* subquery in FROM */
						newrte->subquery = copyObject(rte->subquery);
						break;

					case RTE_CTE:
						newrte->ctename = pstrdup(rte->ctename);
						newrte->ctelevelsup = rte->ctelevelsup;
						newrte->self_reference = rte->self_reference;
						MUTATE(newrte->coltypes, rte->coltypes, List *);
						MUTATE(newrte->coltypmods, rte->coltypmods, List *);
						break;

					case RTE_JOIN:	/* join */
						newrte->joinaliasvars = copyObject(rte->joinaliasvars);
						break;

					case RTE_FUNCTION:	/* functions in FROM */
						MUTATE(newrte->functions, rte->functions, List *);
						break;

					case RTE_TABLEFUNCTION:
						newrte->subquery = copyObject(rte->subquery);
						MUTATE(newrte->functions, rte->functions, List *);
						break;

					case RTE_TABLEFUNC:
						newrte->tablefunc = copyObject(rte->tablefunc);
						MUTATE(newrte->tablefunc, rte->tablefunc, TableFunc *);
						break;

					case RTE_VALUES:
						MUTATE(newrte->values_lists, rte->values_lists, List *);
						break;
				}
				return (Node *) newrte;
			}
			break;

		case T_RangeTblFunction:
			{
				RangeTblFunction *rtfunc = (RangeTblFunction *) node;
				RangeTblFunction *newrtfunc;

				FLATCOPY(newrtfunc, rtfunc, RangeTblFunction);
				MUTATE(newrtfunc->funcexpr, rtfunc->funcexpr, Node *);

				/*
				 * TODO is this right? //newrte->coldeflist = (List *)
				 * copyObject(rte->coldeflist);
				 */
			}
			break;

		case T_ForeignScan:
			{
				ForeignScan *fdwscan = (ForeignScan *) node;
				ForeignScan *newfdwscan;

				FLATCOPY(newfdwscan, fdwscan, ForeignScan);
				SCANMUTATE(newfdwscan, fdwscan);

				MUTATE(newfdwscan->fdw_exprs, fdwscan->fdw_exprs, List *);

				/*
				 * Don't mutate fdw_private, it's private to the FDW. Must make
				 * a copy of it, though.
				 */
				newfdwscan->fdw_private = copyObject(fdwscan->fdw_private);
				newfdwscan->fsSystemCol = fdwscan->fsSystemCol;

				return (Node *) newfdwscan;
			}
			break;

		case T_SplitUpdate:
			{
				SplitUpdate	*splitUpdate = (SplitUpdate *) node;
				SplitUpdate	*newSplitUpdate;

				FLATCOPY(newSplitUpdate, splitUpdate, SplitUpdate);
				PLANMUTATE(newSplitUpdate, splitUpdate);
				return (Node *) newSplitUpdate;
			}
			break;

			/*
			 * The following cases are handled by expression_tree_mutator.	In
			 * addition, we let expression_tree_mutator handle unrecognized
			 * nodes.
			 *
			 * TODO: Identify node types that should never appear in plan
			 * trees and disallow them here by issuing an error or asserting
			 * false.
			 */
		case T_Var:
		case T_Const:
		case T_Param:
		case T_CoerceToDomainValue:
		case T_CaseTestExpr:
		case T_SetToDefault:
		case T_RangeTblRef:
		case T_Aggref:
		case T_WindowFunc:
		case T_SubscriptingRef:
		case T_FuncExpr:
		case T_OpExpr:
		case T_DistinctExpr:
		case T_ScalarArrayOpExpr:
		case T_BoolExpr:
		case T_SubLink:
		case T_FieldSelect:
		case T_FieldStore:
		case T_RelabelType:
		case T_CaseExpr:
		case T_CaseWhen:
		case T_ArrayExpr:
		case T_RowExpr:
		case T_CoalesceExpr:
		case T_NullIfExpr:
		case T_NullTest:
		case T_BooleanTest:
		case T_CoerceToDomain:
		case T_TargetEntry:
		case T_List:
		case T_FromExpr:
		case T_JoinExpr:
		case T_SetOperationStmt:
		case T_SpecialJoinInfo:

		default:

			/*
			 * Let expression_tree_mutator handle remaining cases or complain
			 * of unrecognized node type.
			 */
			return expression_tree_mutator(node, mutator, context);
			break;
	}
	/* can't get here, but keep compiler happy */
	return NULL;
}


/* Function mutate_plan_fields() is a subroutine for plan_tree_mutator().
 * It "hijacks" the macro MUTATE defined for use in that function, so don't
 * change the argument names "mutator" and "context" use in the macro
 * definition.
 *
 */
static void
mutate_plan_fields(Plan *newplan, Plan *oldplan, Node *(*mutator) (), void *context)
{
	/*
	 * Scalar fields startup_cost total_cost plan_rows plan_width nParamExec
	 * need no mutation.
	 */

	/* Node fields need mutation. */
	MUTATE(newplan->targetlist, oldplan->targetlist, List *);
	MUTATE(newplan->qual, oldplan->qual, List *);
	MUTATE(newplan->lefttree, oldplan->lefttree, Plan *);
	MUTATE(newplan->righttree, oldplan->righttree, Plan *);
	MUTATE(newplan->initPlan, oldplan->initPlan, List *);

	/* Bitmapsets aren't nodes but need to be copied to palloc'd space. */
	newplan->extParam = bms_copy(oldplan->extParam);
	newplan->allParam = bms_copy(oldplan->allParam);
}


/* Function mutate_plan_fields() is a subroutine for plan_tree_mutator().
 * It "hijacks" the macro MUTATE defined for use in that function, so don't
 * change the argument names "mutator" and "context" use in the macro
 * definition.
 *
 */
static void 
mutate_join_fields(Join *newjoin, Join *oldjoin, Node *(*mutator) (), void *context)
{
	/* A Join node is a Plan node. */
	mutate_plan_fields((Plan *) newjoin, (Plan *) oldjoin, mutator, context);

	/* Scalar field jointype needs no mutation. */

	/* Node fields need mutation. */
	MUTATE(newjoin->joinqual, oldjoin->joinqual, List *);
}


/* Utility to get a name for a function to use as an eref. */
static char *
get_function_name(Oid proid, const char *dflt)
{
	char	   *result;

	if (!OidIsValid(proid))
	{
		result = pstrdup(dflt);
	}
	else
	{
		result = get_func_name(proid);

		if (result == NULL)
			result = pstrdup(dflt);
	}

	return result;
}

/* Utility to get a name for a tle to use as an eref. */
Value *
get_tle_name(TargetEntry *tle, List *rtable, const char *default_name)
{
	char *name = NULL;
	Node *expr = (Node*)tle->expr;
	
	if ( tle->resname != NULL )
	{
		name = pstrdup(tle->resname);
	}
	else if ( IsA(tle->expr, Var) && rtable != NULL )
	{
		Var *var = (Var*)tle->expr;
		RangeTblEntry *rte = rt_fetch(var->varno, rtable);
		name = pstrdup(get_rte_attribute_name(rte, var->varattno));
	}
	else if ( IsA(tle->expr, WindowFunc) )
	{
		if ( default_name == NULL ) default_name = "window_func";
		name = get_function_name(((WindowFunc *)expr)->winfnoid, default_name);
	}
	else if ( IsA(tle->expr, Aggref) )
	{
		if ( default_name == NULL ) default_name = "aggregate_func";
		name = get_function_name(((Aggref*)expr)->aggfnoid, default_name);
	}
	
	if ( name == NULL )
	{
		if (default_name == NULL ) default_name = "unnamed_attr";
		name = pstrdup(default_name);
	}
	
	return makeString(name);
}
