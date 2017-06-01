/*-------------------------------------------------------------------------
 *
 * cdbplan.c
 *	  Provides routines supporting plan tree manipulation.
 *
 * Copyright (c) 2004-2008, Greenplum inc
 *
 * NOTES
 *	See src/backend/optimizer/util/clauses.c for background information
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "nodes/primnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "cdb/cdbplan.h"


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
				  void *context)
{
	/*
	 * The mutator has already decided not to modify the current node, but
	 * we must call the mutator for any sub-nodes.
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

		case T_Repeat:
			{
				Repeat	   *repeat = (Repeat *) node;
				Repeat	   *newrepeat;

				FLATCOPY(newrepeat, repeat, Repeat);
				PLANMUTATE(newrepeat, repeat);
				return (Node *)newrepeat;
			}
			break;

		case T_Append:
			{
				Append	   *append = (Append *) node;
				Append	   *newappend;

				FLATCOPY(newappend, append, Append);
				PLANMUTATE(newappend, append);
				MUTATE(newappend->appendplans, append->appendplans, List *);
				/* isTarget is scalar. */
				return (Node *) newappend;
			}
			break;

		case T_Sequence:
			{
				Sequence *sequence = (Sequence *) node;
				Sequence *newSequence = NULL;

				FLATCOPY(newSequence, sequence, Sequence);
				PLANMUTATE(newSequence, sequence);
				MUTATE(newSequence->subplans, sequence->subplans, List *);

				return (Node *) newSequence;
			}
			
		case T_AssertOp:
			{
				AssertOp *assert = (AssertOp *) node;
				AssertOp *newAssert = NULL;

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
				MUTATE(newPartsel->levelEqExpressions, partsel->levelEqExpressions, List *);
				MUTATE(newPartsel->levelExpressions, partsel->levelExpressions, List *);
				MUTATE(newPartsel->residualPredicate, partsel->residualPredicate, Node *);
				MUTATE(newPartsel->propagationExpression, partsel->propagationExpression, Node *);
				MUTATE(newPartsel->printablePredicate, partsel->printablePredicate, Node *);
				MUTATE(newPartsel->staticPartOids, partsel->staticPartOids, List *);
				MUTATE(newPartsel->staticScanIds, partsel->staticScanIds, List *);
				newPartsel->nLevels = partsel->nLevels;
				newPartsel->scanId = partsel->scanId;
				newPartsel->selectorId = partsel->selectorId;
				newPartsel->relid = partsel->relid;
				newPartsel->staticSelection = partsel->staticSelection;

				return (Node *) newPartsel;
			}

		case T_BitmapAnd:
			{
				BitmapAnd  *old = (BitmapAnd *) node;
                BitmapAnd  *mut;

				FLATCOPY(mut, old, BitmapAnd);
				PLANMUTATE(mut, old);
				MUTATE(mut->bitmapplans, old->bitmapplans, List *);
				return (Node *)mut;
			}
			break;
        case T_BitmapOr:
            {
                BitmapOr   *old = (BitmapOr *) node;
                BitmapOr   *mut;

                FLATCOPY(mut, old, BitmapOr);
                PLANMUTATE(mut, old);
                MUTATE(mut->bitmapplans, old->bitmapplans, List *);
                return (Node *)mut;
            }
            break;

		case T_Scan:
			/* Abstract: Should see only subclasses. */
			elog(ERROR, "abstract node type not allowed: T_Scan");

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

		case T_AppendOnlyScan:
			{
				AppendOnlyScan    *appendonlyscan = (AppendOnlyScan *) node;
				AppendOnlyScan    *newappendonlyscan;
				
				FLATCOPY(newappendonlyscan, appendonlyscan, AppendOnlyScan);
				SCANMUTATE(newappendonlyscan, appendonlyscan);
				/* (for now) A AppendOnlyScan is really just a Scan, so we're done. */
				return (Node *) newappendonlyscan;
			}
			break;
	
		case T_AOCSScan:
			{
				AOCSScan    *aocs = (AOCSScan *) node;
				AOCSScan    *newaocs; 
				
				FLATCOPY(newaocs, aocs, AOCSScan);
				SCANMUTATE(newaocs, aocs);
				/* (for now) A AOCSScan is really just a Scan, so we're done. */
				return (Node *) newaocs;
			}
			break;
				
		case T_TableScan:
			{
				TableScan *tableScan = (TableScan *) node;
				TableScan *newTableScan = NULL; 
				
				FLATCOPY(newTableScan, tableScan, TableScan);
				SCANMUTATE(newTableScan, tableScan);

				return (Node *) newTableScan;
			}
			break;
				
		case T_DynamicTableScan:
			{
				DynamicTableScan *tableScan = (DynamicTableScan *) node;
				DynamicTableScan *newTableScan = NULL;
				
				FLATCOPY(newTableScan, tableScan, DynamicTableScan);
				SCANMUTATE(newTableScan, tableScan);
				newTableScan->partIndex = tableScan->partIndex;
				newTableScan->partIndexPrintable = tableScan->partIndexPrintable;
				return (Node *) newTableScan;
			}
			break;
				
		case T_ExternalScan:
			{
				ExternalScan    *extscan = (ExternalScan *) node;
				ExternalScan    *newextscan;
				
				FLATCOPY(newextscan, extscan, ExternalScan);
				SCANMUTATE(newextscan, extscan);
				
				MUTATE(newextscan->uriList, extscan->uriList, List *);
				MUTATE(newextscan->fmtOpts, extscan->fmtOpts, List *);
				newextscan->fmtType = extscan->fmtType;
				newextscan->isMasterOnly = extscan->isMasterOnly;
				
				return (Node *) newextscan;
			}
			break;
			
		case T_IndexScan:
			{
				IndexScan  *idxscan = (IndexScan *) node;
				IndexScan  *newidxscan;

				FLATCOPY(newidxscan, idxscan, IndexScan);
				SCANMUTATE(newidxscan, idxscan);
				newidxscan->indexid = idxscan->indexid;
				//MUTATE(newidxscan->indexid, idxscan->indexid, List *);
				MUTATE(newidxscan->indexqual, idxscan->indexqual, List *);
				MUTATE(newidxscan->indexqualorig, idxscan->indexqualorig, List *);
				MUTATE(newidxscan->indexstrategy, idxscan->indexstrategy, List *);
				MUTATE(newidxscan->indexsubtype, idxscan->indexsubtype, List *);
				/* indxorderdir  is  scalar */
				return (Node *) newidxscan;
			}
			break;
			
		case T_BitmapIndexScan:
			{
				BitmapIndexScan  *idxscan = (BitmapIndexScan *) node;
				BitmapIndexScan  *newidxscan;

				FLATCOPY(newidxscan, idxscan, BitmapIndexScan);
				SCANMUTATE(newidxscan, idxscan);
				newidxscan->indexid = idxscan->indexid;

				MUTATE(newidxscan->indexqual, idxscan->indexqual, List *);
				MUTATE(newidxscan->indexqualorig, idxscan->indexqualorig, List *);
				MUTATE(newidxscan->indexstrategy, idxscan->indexstrategy, List *);
				MUTATE(newidxscan->indexsubtype, idxscan->indexsubtype, List *);

				return (Node *) newidxscan;
			}
			break;
			
		case T_BitmapHeapScan:
			{
				BitmapHeapScan  *bmheapscan = (BitmapHeapScan *) node;
				BitmapHeapScan  *newbmheapscan;

				FLATCOPY(newbmheapscan, bmheapscan, BitmapHeapScan);
				SCANMUTATE(newbmheapscan, bmheapscan);
				
				MUTATE(newbmheapscan->bitmapqualorig, bmheapscan->bitmapqualorig, List *);
	
				return (Node *) newbmheapscan;
			}
			break;
		

		case T_BitmapAppendOnlyScan:
			{
				BitmapAppendOnlyScan  *bmappendonlyscan = (BitmapAppendOnlyScan *) node;
				BitmapAppendOnlyScan  *newbmappendonlyscan;

				FLATCOPY(newbmappendonlyscan, bmappendonlyscan, BitmapAppendOnlyScan);
				SCANMUTATE(newbmappendonlyscan, bmappendonlyscan);
				
				MUTATE(newbmappendonlyscan->bitmapqualorig, bmappendonlyscan->bitmapqualorig, List *);
	
				return (Node *) newbmappendonlyscan;
			}
			break;
		
		case T_BitmapTableScan:
			{
				BitmapTableScan  *bmtablescan = (BitmapTableScan *) node;
				BitmapTableScan  *newbmtablescan = NULL;

				FLATCOPY(newbmtablescan, bmtablescan, BitmapTableScan);
				SCANMUTATE(newbmtablescan, bmtablescan);

				MUTATE(newbmtablescan->bitmapqualorig, bmtablescan->bitmapqualorig, List *);

				return (Node *) newbmtablescan;
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
				ValuesScan *scan = (ValuesScan *)node;
				ValuesScan *newscan;

				FLATCOPY(newscan, scan, ValuesScan);
				SCANMUTATE(newscan, scan);
				return (Node *) newscan;
			}
			break;

		case T_Join:
			/* Abstract: Should see only subclasses. */
			elog(ERROR, "abstract node type not allowed: T_Join");

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

		case T_TableFunctionScan:
			{
				TableFunctionScan		*tabfunc = (TableFunctionScan *) node;
				TableFunctionScan		*newtabfunc;

				FLATCOPY(newtabfunc, tabfunc, TableFunctionScan);
				PLANMUTATE(newtabfunc, tabfunc);
				return (Node *) newtabfunc;
			}
			break;

		case T_Window:
			{
				Window	   *window = (Window *) node;
				Window	   *newwindow;

				FLATCOPY(newwindow, window, Window);
				PLANMUTATE(newwindow, window);
				
				COPYARRAY(newwindow, window, numPartCols, partColIdx);
				COPYARRAY(newwindow, window, numPartCols, partOperators);
				MUTATE(newwindow->windowKeys, window->windowKeys, List *);
				
				return (Node *) newwindow;
			}
			break;
		
		case T_WindowKey:
			{
				WindowKey	*key = (WindowKey *)node;
				WindowKey	*newkey;
				
				FLATCOPY(newkey, key, WindowKey);
				COPYARRAY(newkey, key, numSortCols, sortColIdx);
				COPYARRAY(newkey, key, numSortCols, sortOperators);
				MUTATE(newkey->frame, key->frame, WindowFrame *);
				
				return (Node *) newkey;
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

				MUTATE(newmotion->hashExpr, motion->hashExpr, List *);
				MUTATE(newmotion->hashDataTypes, motion->hashDataTypes, List *);
				COPYARRAY(newmotion, motion, numOutputSegs, outputSegIdx);

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
				MUTATE(newflow->hashExpr, flow->hashExpr, List *);
				COPYARRAY(newflow, flow, numSortCols, sortColIdx);
				COPYARRAY(newflow, flow, numSortCols, sortOperators);
				COPYARRAY(newflow, flow, numSortCols, nullsFirst);
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
				Plan	   *subplan_plan = plan_tree_base_subplan_get_plan(context, subplan);
				SubPlan    *newnode;
				Plan	   *newsubplan_plan;

				FLATCOPY(newnode, subplan, SubPlan);

				MUTATE(newnode->testexpr, subplan->testexpr, Node *);
				MUTATE(newsubplan_plan, subplan_plan, Plan *);
				MUTATE(newnode->args, subplan->args, List *);

                /* An IntList isn't interesting to mutate; just copy. */
				newnode->paramIds = (List *)copyObject(subplan->paramIds);
				newnode->setParam = (List *)copyObject(subplan->setParam);
				newnode->parParam = (List *)copyObject(subplan->parParam);
				newnode->extParam = (List *)copyObject(subplan->extParam);
				
				if (newsubplan_plan != subplan_plan)
					plan_tree_base_subplan_put_plan(context, newnode, newsubplan_plan);

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
					case RTE_SPECIAL:	/* special rule relation (NEW or OLD) */
                    case RTE_VOID:      /* deleted entry */
						/* No extras. */
						break;

					case RTE_SUBQUERY:	/* subquery in FROM */
						newrte->subquery = copyObject(rte->subquery);
						break;
						
					case RTE_CTE:
						newrte->ctename = pstrdup(rte->ctename);
						newrte->ctelevelsup = rte->ctelevelsup;
						newrte->self_reference = rte->self_reference;
						MUTATE(newrte->ctecoltypes, rte->ctecoltypes, List *);
						MUTATE(newrte->ctecoltypmods, rte->ctecoltypmods, List *);
						break;
						
					case RTE_JOIN:		/* join */
						newrte->joinaliasvars = copyObject(rte->joinaliasvars);
						break;

					case RTE_FUNCTION:	/* function in FROM */
						MUTATE(newrte->funcexpr, rte->funcexpr, Node *);
						// TODO is this right? //newrte->coldeflist = (List *) copyObject(rte->coldeflist);
						break;

					case RTE_TABLEFUNCTION:
						newrte->subquery = copyObject(rte->subquery);
						MUTATE(newrte->funcexpr, rte->funcexpr, Node *);
						break;

                    case RTE_VALUES:
				        MUTATE(newrte->values_lists, rte->values_lists, List *);
				        break;
				}
				return (Node *) newrte;
			}
			break;

			/*
			 * The following cases are handled by expression_tree_mutator.	In
			 * addition, we let expression_tree_mutator handle unrecognized
			 * nodes.
			 *
			 * TODO: Identify node types that should never appear in plan trees
			 * and disallow them here by issuing an error or asserting false.
			 */
		case T_Var:
		case T_Const:
		case T_Param:
		case T_CoerceToDomainValue:
		case T_CaseTestExpr:
		case T_SetToDefault:
		case T_RangeTblRef:
		case T_Aggref:
		case T_AggOrder:
		case T_WindowRef:
		case T_WindowFrame:
		case T_WindowFrameEdge:
		case T_ArrayRef:
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
void		mutate_plan_fields(Plan *newplan, Plan *oldplan, Node *(*mutator) (), void *context)
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
void		mutate_join_fields(Join *newjoin, Join *oldjoin, Node *(*mutator) (), void *context)
{
	/* A Join node is a Plan node. */
	mutate_plan_fields((Plan *) newjoin, (Plan *) oldjoin, mutator, context);

	/* Scalar field jointype needs no mutation. */

	/* Node fields need mutation. */
	MUTATE(newjoin->joinqual, oldjoin->joinqual, List *);
}
