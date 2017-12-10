/*-------------------------------------------------------------------------
 *
 * predtest.c
 *	  Routines to attempt to prove logical implications between predicate
 *	  expressions.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/util/predtest.c,v 1.27 2009/06/11 14:48:59 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_amop.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/planmain.h"
#include "optimizer/predtest.h"
#include "utils/array.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "cdb/cdbhash.h"
#include "access/hash.h"
#include "nodes/makefuncs.h"

#include "catalog/pg_operator.h"
#include "optimizer/paths.h"

#define INT16MAX (32767)
#define INT16MIN (-32768)
#define INT32MAX (2147483647)
#define INT32MIN (-2147483648)

static const bool kUseFnEvaluationForPredicates = true;

/*
 * Proof attempts involving large arrays in ScalarArrayOpExpr nodes are
 * likely to require O(N^2) time, and more often than not fail anyway.
 * So we set an arbitrary limit on the number of array elements that
 * we will allow to be treated as an AND or OR clause.
 * XXX is it worth exposing this as a GUC knob?
 */
#define MAX_SAOP_ARRAY_SIZE		100

/*
 * To avoid redundant coding in predicate_implied_by_recurse and
 * predicate_refuted_by_recurse, we need to abstract out the notion of
 * iterating over the components of an expression that is logically an AND
 * or OR structure.  There are multiple sorts of expression nodes that can
 * be treated as ANDs or ORs, and we don't want to code each one separately.
 * Hence, these types and support routines.
 */
typedef enum
{
	CLASS_ATOM,					/* expression that's not AND or OR */
	CLASS_AND,					/* expression with AND semantics */
	CLASS_OR					/* expression with OR semantics */
} PredClass;

typedef struct PredIterInfoData *PredIterInfo;

typedef struct PredIterInfoData
{
	/* node-type-specific iteration state */
	void	   *state;
	/* initialize to do the iteration */
	void		(*startup_fn) (Node *clause, PredIterInfo info);
	/* next-component iteration function */
	Node	   *(*next_fn) (PredIterInfo info);
	/* release resources when done with iteration */
	void		(*cleanup_fn) (PredIterInfo info);
} PredIterInfoData;

#define iterate_begin(item, clause, info)	\
	do { \
		Node   *item; \
		(info).startup_fn((clause), &(info)); \
		while ((item = (info).next_fn(&(info))) != NULL)

#define iterate_end(info)	\
		(info).cleanup_fn(&(info)); \
	} while (0)


static bool predicate_implied_by_recurse(Node *clause, Node *predicate);
static bool predicate_refuted_by_recurse(Node *clause, Node *predicate);
static PredClass predicate_classify(Node *clause, PredIterInfo info);
static void list_startup_fn(Node *clause, PredIterInfo info);
static Node *list_next_fn(PredIterInfo info);
static void list_cleanup_fn(PredIterInfo info);
static void boolexpr_startup_fn(Node *clause, PredIterInfo info);
static void arrayconst_startup_fn(Node *clause, PredIterInfo info);
static Node *arrayconst_next_fn(PredIterInfo info);
static void arrayconst_cleanup_fn(PredIterInfo info);
static void arrayexpr_startup_fn(Node *clause, PredIterInfo info);
static Node *arrayexpr_next_fn(PredIterInfo info);
static void arrayexpr_cleanup_fn(PredIterInfo info);
static bool predicate_implied_by_simple_clause(Expr *predicate, Node *clause);
static bool predicate_refuted_by_simple_clause(Expr *predicate, Node *clause);
static Node *extract_not_arg(Node *clause);
static Node *extract_strong_not_arg(Node *clause);
static bool list_member_strip(List *list, Expr *datum);
static bool btree_predicate_proof(Expr *predicate, Node *clause,
					  bool refute_it);
static Oid	get_btree_test_op(Oid pred_op, Oid clause_op, bool refute_it);
static void InvalidateOprProofCacheCallBack(Datum arg, int cacheid, ItemPointer tuplePtr);

static HTAB* CreateNodeSetHashTable();
static void AddValue(PossibleValueSet *pvs, Const *valueToCopy);
static void RemoveValue(PossibleValueSet *pvs, Const *value);
static bool ContainsValue(PossibleValueSet *pvs, Const *value);
static void AddUnmatchingValues( PossibleValueSet *pvs, PossibleValueSet *toCheck );
static void RemoveUnmatchingValues(PossibleValueSet *pvs, PossibleValueSet *toCheck);
static PossibleValueSet ProcessAndClauseForPossibleValues( PredIterInfoData *clauseInfo, Node *clause, Node *variable);
static PossibleValueSet ProcessOrClauseForPossibleValues( PredIterInfoData *clauseInfo, Node *clause, Node *variable);
static bool TryProcessEqualityNodeForPossibleValues(OpExpr *expr, Node *variable, PossibleValueSet *resultOut );

static bool simple_equality_predicate_refuted(Node *clause, Node *predicate);

/*
 * predicate_implied_by
 *	  Recursively checks whether the clauses in restrictinfo_list imply
 *	  that the given predicate is true.
 *
 * The top-level List structure of each list corresponds to an AND list.
 * We assume that eval_const_expressions() has been applied and so there
 * are no un-flattened ANDs or ORs (e.g., no AND immediately within an AND,
 * including AND just below the top-level List structure).
 * If this is not true we might fail to prove an implication that is
 * valid, but no worse consequences will ensue.
 *
 * We assume the predicate has already been checked to contain only
 * immutable functions and operators.  (In most current uses this is true
 * because the predicate is part of an index predicate that has passed
 * CheckPredicate().)  We dare not make deductions based on non-immutable
 * functions, because they might change answers between the time we make
 * the plan and the time we execute the plan.
 */
bool
predicate_implied_by(List *predicate_list, List *restrictinfo_list)
{
	Node	   *p,
			   *r;

	if (predicate_list == NIL)
		return true;			/* no predicate: implication is vacuous */
	if (restrictinfo_list == NIL)
		return false;			/* no restriction: implication must fail */

	/*
	 * If either input is a single-element list, replace it with its lone
	 * member; this avoids one useless level of AND-recursion.	We only need
	 * to worry about this at top level, since eval_const_expressions should
	 * have gotten rid of any trivial ANDs or ORs below that.
	 */
	if (list_length(predicate_list) == 1)
		p = (Node *) linitial(predicate_list);
	else
		p = (Node *) predicate_list;
	if (list_length(restrictinfo_list) == 1)
		r = (Node *) linitial(restrictinfo_list);
	else
		r = (Node *) restrictinfo_list;

	/* And away we go ... */
	return predicate_implied_by_recurse(r, p);
}

/*
 * predicate_refuted_by
 *	  Recursively checks whether the clauses in restrictinfo_list refute
 *	  the given predicate (that is, prove it false).
 *
 * This is NOT the same as !(predicate_implied_by), though it is similar
 * in the technique and structure of the code.
 *
 * An important fine point is that truth of the clauses must imply that
 * the predicate returns FALSE, not that it does not return TRUE.  This
 * is normally used to try to refute CHECK constraints, and the only
 * thing we can assume about a CHECK constraint is that it didn't return
 * FALSE --- a NULL result isn't a violation per the SQL spec.  (Someday
 * perhaps this code should be extended to support both "strong" and
 * "weak" refutation, but for now we only need "strong".)
 *
 * The top-level List structure of each list corresponds to an AND list.
 * We assume that eval_const_expressions() has been applied and so there
 * are no un-flattened ANDs or ORs (e.g., no AND immediately within an AND,
 * including AND just below the top-level List structure).
 * If this is not true we might fail to prove an implication that is
 * valid, but no worse consequences will ensue.
 *
 * We assume the predicate has already been checked to contain only
 * immutable functions and operators.  We dare not make deductions based on
 * non-immutable functions, because they might change answers between the
 * time we make the plan and the time we execute the plan.
 */
bool
predicate_refuted_by(List *predicate_list, List *restrictinfo_list)
{
	Node	   *p,
			   *r;

	if (predicate_list == NIL)
		return false;			/* no predicate: no refutation is possible */
	if (restrictinfo_list == NIL)
		return false;			/* no restriction: refutation must fail */

	/*
	 * If either input is a single-element list, replace it with its lone
	 * member; this avoids one useless level of AND-recursion.	We only need
	 * to worry about this at top level, since eval_const_expressions should
	 * have gotten rid of any trivial ANDs or ORs below that.
	 */
	if (list_length(predicate_list) == 1)
		p = (Node *) linitial(predicate_list);
	else
		p = (Node *) predicate_list;
	if (list_length(restrictinfo_list) == 1)
		r = (Node *) linitial(restrictinfo_list);
	else
		r = (Node *) restrictinfo_list;

	/* And away we go ... */
	if ( predicate_refuted_by_recurse(r, p))
        return true;

    if ( ! kUseFnEvaluationForPredicates )
        return false;
    return simple_equality_predicate_refuted((Node*)restrictinfo_list, (Node*)predicate_list);
}

/*----------
 * predicate_implied_by_recurse
 *	  Does the predicate implication test for non-NULL restriction and
 *	  predicate clauses.
 *
 * The logic followed here is ("=>" means "implies"):
 *	atom A => atom B iff:			predicate_implied_by_simple_clause says so
 *	atom A => AND-expr B iff:		A => each of B's components
 *	atom A => OR-expr B iff:		A => any of B's components
 *	AND-expr A => atom B iff:		any of A's components => B
 *	AND-expr A => AND-expr B iff:	A => each of B's components
 *	AND-expr A => OR-expr B iff:	A => any of B's components,
 *									*or* any of A's components => B
 *	OR-expr A => atom B iff:		each of A's components => B
 *	OR-expr A => AND-expr B iff:	A => each of B's components
 *	OR-expr A => OR-expr B iff:		each of A's components => any of B's
 *
 * An "atom" is anything other than an AND or OR node.	Notice that we don't
 * have any special logic to handle NOT nodes; these should have been pushed
 * down or eliminated where feasible by prepqual.c.
 *
 * We can't recursively expand either side first, but have to interleave
 * the expansions per the above rules, to be sure we handle all of these
 * examples:
 *		(x OR y) => (x OR y OR z)
 *		(x AND y AND z) => (x AND y)
 *		(x AND y) => ((x AND y) OR z)
 *		((x OR y) AND z) => (x OR y)
 * This is still not an exhaustive test, but it handles most normal cases
 * under the assumption that both inputs have been AND/OR flattened.
 *
 * We have to be prepared to handle RestrictInfo nodes in the restrictinfo
 * tree, though not in the predicate tree.
 *----------
 */
static bool
predicate_implied_by_recurse(Node *clause, Node *predicate)
{
	PredIterInfoData clause_info;
	PredIterInfoData pred_info;
	PredClass	pclass;
	bool		result;

	/* skip through RestrictInfo */
	Assert(clause != NULL);
	if (IsA(clause, RestrictInfo))
		clause = (Node *) ((RestrictInfo *) clause)->clause;

	pclass = predicate_classify(predicate, &pred_info);

	switch (predicate_classify(clause, &clause_info))
	{
		case CLASS_AND:
			switch (pclass)
			{
				case CLASS_AND:

					/*
					 * AND-clause => AND-clause if A implies each of B's items
					 */
					result = true;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (!predicate_implied_by_recurse(clause, pitem))
						{
							result = false;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_OR:

					/*
					 * AND-clause => OR-clause if A implies any of B's items
					 *
					 * Needed to handle (x AND y) => ((x AND y) OR z)
					 */
					result = false;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (predicate_implied_by_recurse(clause, pitem))
						{
							result = true;
							break;
						}
					}
					iterate_end(pred_info);
					if (result)
						return result;

					/*
					 * Also check if any of A's items implies B
					 *
					 * Needed to handle ((x OR y) AND z) => (x OR y)
					 */
					iterate_begin(citem, clause, clause_info)
					{
						if (predicate_implied_by_recurse(citem, predicate))
						{
							result = true;
							break;
						}
					}
					iterate_end(clause_info);
					return result;

				case CLASS_ATOM:

					/*
					 * AND-clause => atom if any of A's items implies B
					 */
					result = false;
					iterate_begin(citem, clause, clause_info)
					{
						if (predicate_implied_by_recurse(citem, predicate))
						{
							result = true;
							break;
						}
					}
					iterate_end(clause_info);
					return result;
			}
			break;

		case CLASS_OR:
			switch (pclass)
			{
				case CLASS_OR:

					/*
					 * OR-clause => OR-clause if each of A's items implies any
					 * of B's items.  Messy but can't do it any more simply.
					 */
					result = true;
					iterate_begin(citem, clause, clause_info)
					{
						bool		presult = false;

						iterate_begin(pitem, predicate, pred_info)
						{
							if (predicate_implied_by_recurse(citem, pitem))
							{
								presult = true;
								break;
							}
						}
						iterate_end(pred_info);
						if (!presult)
						{
							result = false;		/* doesn't imply any of B's */
							break;
						}
					}
					iterate_end(clause_info);
					return result;

				case CLASS_AND:
				case CLASS_ATOM:

					/*
					 * OR-clause => AND-clause if each of A's items implies B
					 *
					 * OR-clause => atom if each of A's items implies B
					 */
					result = true;
					iterate_begin(citem, clause, clause_info)
					{
						if (!predicate_implied_by_recurse(citem, predicate))
						{
							result = false;
							break;
						}
					}
					iterate_end(clause_info);
					return result;
			}
			break;

		case CLASS_ATOM:
			switch (pclass)
			{
				case CLASS_AND:

					/*
					 * atom => AND-clause if A implies each of B's items
					 */
					result = true;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (!predicate_implied_by_recurse(clause, pitem))
						{
							result = false;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_OR:

					/*
					 * atom => OR-clause if A implies any of B's items
					 */
					result = false;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (predicate_implied_by_recurse(clause, pitem))
						{
							result = true;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_ATOM:

					/*
					 * atom => atom is the base case
					 */
					return
						predicate_implied_by_simple_clause((Expr *) predicate,
														   clause);
			}
			break;
	}

	/* can't get here */
	elog(ERROR, "predicate_classify returned a bogus value");
	return false;
}

/*----------
 * predicate_refuted_by_recurse
 *	  Does the predicate refutation test for non-NULL restriction and
 *	  predicate clauses.
 *
 * The logic followed here is ("R=>" means "refutes"):
 *	atom A R=> atom B iff:			predicate_refuted_by_simple_clause says so
 *	atom A R=> AND-expr B iff:		A R=> any of B's components
 *	atom A R=> OR-expr B iff:		A R=> each of B's components
 *	AND-expr A R=> atom B iff:		any of A's components R=> B
 *	AND-expr A R=> AND-expr B iff:	A R=> any of B's components,
 *									*or* any of A's components R=> B
 *	AND-expr A R=> OR-expr B iff:	A R=> each of B's components
 *	OR-expr A R=> atom B iff:		each of A's components R=> B
 *	OR-expr A R=> AND-expr B iff:	each of A's components R=> any of B's
 *	OR-expr A R=> OR-expr B iff:	A R=> each of B's components
 *
 * In addition, if the predicate is a NOT-clause then we can use
 *	A R=> NOT B if:					A => B
 * This works for several different SQL constructs that assert the non-truth
 * of their argument, ie NOT, IS FALSE, IS NOT TRUE, IS UNKNOWN.
 * Unfortunately we *cannot* use
 *	NOT A R=> B if:					B => A
 * because this type of reasoning fails to prove that B doesn't yield NULL.
 * We can however make the more limited deduction that
 *	NOT A R=> A
 *
 * Other comments are as for predicate_implied_by_recurse().
 *----------
 */
static bool
predicate_refuted_by_recurse(Node *clause, Node *predicate)
{
	PredIterInfoData clause_info;
	PredIterInfoData pred_info;
	PredClass	pclass;
	Node	   *not_arg;
	bool		result;

	CHECK_FOR_INTERRUPTS();

	/* skip through RestrictInfo */
	Assert(clause != NULL);
	if (IsA(clause, RestrictInfo))
		clause = (Node *) ((RestrictInfo *) clause)->clause;

	pclass = predicate_classify(predicate, &pred_info);

	switch (predicate_classify(clause, &clause_info))
	{
		case CLASS_AND:
			switch (pclass)
			{
				case CLASS_AND:

					/*
					 * AND-clause R=> AND-clause if A refutes any of B's items
					 *
					 * Needed to handle (x AND y) R=> ((!x OR !y) AND z)
					 */
					result = false;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (predicate_refuted_by_recurse(clause, pitem))
						{
							result = true;
							break;
						}
					}
					iterate_end(pred_info);
					if (result)
						return result;

					/*
					 * Also check if any of A's items refutes B
					 *
					 * Needed to handle ((x OR y) AND z) R=> (!x AND !y)
					 */
					iterate_begin(citem, clause, clause_info)
					{
						if (predicate_refuted_by_recurse(citem, predicate))
						{
							result = true;
							break;
						}
					}
					iterate_end(clause_info);
					return result;

				case CLASS_OR:

					/*
					 * AND-clause R=> OR-clause if A refutes each of B's items
					 */
					result = true;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (!predicate_refuted_by_recurse(clause, pitem))
						{
							result = false;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_ATOM:

					/*
					 * If B is a NOT-clause, A R=> B if A => B's arg
					 */
					not_arg = extract_not_arg(predicate);
					if (not_arg &&
						predicate_implied_by_recurse(clause, not_arg))
						return true;

					/*
					 * AND-clause R=> atom if any of A's items refutes B
					 */
					result = false;
					iterate_begin(citem, clause, clause_info)
					{
						if (predicate_refuted_by_recurse(citem, predicate))
						{
							result = true;
							break;
						}
					}
					iterate_end(clause_info);
					return result;
			}
			break;

		case CLASS_OR:
			switch (pclass)
			{
				case CLASS_OR:

					/*
					 * OR-clause R=> OR-clause if A refutes each of B's items
					 */
					result = true;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (!predicate_refuted_by_recurse(clause, pitem))
						{
							result = false;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_AND:

					/*
					 * OR-clause R=> AND-clause if each of A's items refutes
					 * any of B's items.
					 */
					result = true;
					iterate_begin(citem, clause, clause_info)
					{
						bool		presult = false;

						iterate_begin(pitem, predicate, pred_info)
						{
							if (predicate_refuted_by_recurse(citem, pitem))
							{
								presult = true;
								break;
							}
						}
						iterate_end(pred_info);
						if (!presult)
						{
							result = false;		/* citem refutes nothing */
							break;
						}
					}
					iterate_end(clause_info);
					return result;

				case CLASS_ATOM:

					/*
					 * If B is a NOT-clause, A R=> B if A => B's arg
					 */
					not_arg = extract_not_arg(predicate);
					if (not_arg &&
						predicate_implied_by_recurse(clause, not_arg))
						return true;

					/*
					 * OR-clause R=> atom if each of A's items refutes B
					 */
					result = true;
					iterate_begin(citem, clause, clause_info)
					{
						if (!predicate_refuted_by_recurse(citem, predicate))
						{
							result = false;
							break;
						}
					}
					iterate_end(clause_info);
					return result;
			}
			break;

		case CLASS_ATOM:

			/*
			 * If A is a strong NOT-clause, A R=> B if B equals A's arg
			 *
			 * We cannot make the stronger conclusion that B is refuted if
			 * B implies A's arg; that would only prove that B is not-TRUE,
			 * not that it's not NULL either.  Hence use equal() rather than
			 * predicate_implied_by_recurse().  We could do the latter if we
			 * ever had a need for the weak form of refutation.
			 */
			not_arg = extract_strong_not_arg(clause);
			if (not_arg && equal(predicate, not_arg))
				return true;

			switch (pclass)
			{
				case CLASS_AND:

					/*
					 * atom R=> AND-clause if A refutes any of B's items
					 */
					result = false;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (predicate_refuted_by_recurse(clause, pitem))
						{
							result = true;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_OR:

					/*
					 * atom R=> OR-clause if A refutes each of B's items
					 */
					result = true;
					iterate_begin(pitem, predicate, pred_info)
					{
						if (!predicate_refuted_by_recurse(clause, pitem))
						{
							result = false;
							break;
						}
					}
					iterate_end(pred_info);
					return result;

				case CLASS_ATOM:

					/*
					 * If B is a NOT-clause, A R=> B if A => B's arg
					 */
					not_arg = extract_not_arg(predicate);
					if (not_arg &&
						predicate_implied_by_recurse(clause, not_arg))
						return true;

					/*
					 * atom R=> atom is the base case
					 */
					return
						predicate_refuted_by_simple_clause((Expr *) predicate,
														   clause);
			}
			break;
	}

	/* can't get here */
	elog(ERROR, "predicate_classify returned a bogus value");
	return false;
}


/*
 * predicate_classify
 *	  Classify an expression node as AND-type, OR-type, or neither (an atom).
 *
 * If the expression is classified as AND- or OR-type, then *info is filled
 * in with the functions needed to iterate over its components.
 *
 * This function also implements enforcement of MAX_SAOP_ARRAY_SIZE: if a
 * ScalarArrayOpExpr's array has too many elements, we just classify it as an
 * atom.  (This will result in its being passed as-is to the simple_clause
 * functions, which will fail to prove anything about it.)	Note that we
 * cannot just stop after considering MAX_SAOP_ARRAY_SIZE elements; in general
 * that would result in wrong proofs, rather than failing to prove anything.
 */
static PredClass
predicate_classify(Node *clause, PredIterInfo info)
{
	/* Caller should not pass us NULL, nor a RestrictInfo clause */
	Assert(clause != NULL);
	Assert(!IsA(clause, RestrictInfo));

	/*
	 * If we see a List, assume it's an implicit-AND list; this is the correct
	 * semantics for lists of RestrictInfo nodes.
	 */
	if (IsA(clause, List))
	{
		info->startup_fn = list_startup_fn;
		info->next_fn = list_next_fn;
		info->cleanup_fn = list_cleanup_fn;
		return CLASS_AND;
	}

	/* Handle normal AND and OR boolean clauses */
	if (and_clause(clause))
	{
		info->startup_fn = boolexpr_startup_fn;
		info->next_fn = list_next_fn;
		info->cleanup_fn = list_cleanup_fn;
		return CLASS_AND;
	}
	if (or_clause(clause))
	{
		info->startup_fn = boolexpr_startup_fn;
		info->next_fn = list_next_fn;
		info->cleanup_fn = list_cleanup_fn;
		return CLASS_OR;
	}

	/* Handle ScalarArrayOpExpr */
	if (IsA(clause, ScalarArrayOpExpr))
	{
		ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *) clause;
		Node	   *arraynode = (Node *) lsecond(saop->args);

		/*
		 * We can break this down into an AND or OR structure, but only if we
		 * know how to iterate through expressions for the array's elements.
		 * We can do that if the array operand is a non-null constant or a
		 * simple ArrayExpr.
		 */
		if (arraynode && IsA(arraynode, Const) &&
			!((Const *) arraynode)->constisnull)
		{
			ArrayType  *arrayval;
			int			nelems;

			arrayval = DatumGetArrayTypeP(((Const *) arraynode)->constvalue);
			nelems = ArrayGetNItems(ARR_NDIM(arrayval), ARR_DIMS(arrayval));
			if (nelems <= MAX_SAOP_ARRAY_SIZE)
			{
				info->startup_fn = arrayconst_startup_fn;
				info->next_fn = arrayconst_next_fn;
				info->cleanup_fn = arrayconst_cleanup_fn;
				return saop->useOr ? CLASS_OR : CLASS_AND;
			}
		}
		else if (arraynode && IsA(arraynode, ArrayExpr) &&
				 !((ArrayExpr *) arraynode)->multidims &&
				 list_length(((ArrayExpr *) arraynode)->elements) <= MAX_SAOP_ARRAY_SIZE)
		{
			info->startup_fn = arrayexpr_startup_fn;
			info->next_fn = arrayexpr_next_fn;
			info->cleanup_fn = arrayexpr_cleanup_fn;
			return saop->useOr ? CLASS_OR : CLASS_AND;
		}
	}

	/* None of the above, so it's an atom */
	return CLASS_ATOM;
}

/*
 * PredIterInfo routines for iterating over regular Lists.	The iteration
 * state variable is the next ListCell to visit.
 */
static void
list_startup_fn(Node *clause, PredIterInfo info)
{
	info->state = (void *) list_head((List *) clause);
}

static Node *
list_next_fn(PredIterInfo info)
{
	ListCell   *l = (ListCell *) info->state;
	Node	   *n;

	if (l == NULL)
		return NULL;
	n = lfirst(l);
	info->state = (void *) lnext(l);
	return n;
}

static void
list_cleanup_fn(PredIterInfo info)
{
	/* Nothing to clean up */
}

/*
 * BoolExpr needs its own startup function, but can use list_next_fn and
 * list_cleanup_fn.
 */
static void
boolexpr_startup_fn(Node *clause, PredIterInfo info)
{
	info->state = (void *) list_head(((BoolExpr *) clause)->args);
}

/*
 * PredIterInfo routines for iterating over a ScalarArrayOpExpr with a
 * constant array operand.
 */
typedef struct
{
	OpExpr		opexpr;
	Const		constexpr;
	int			next_elem;
	int			num_elems;
	Datum	   *elem_values;
	bool	   *elem_nulls;
} ArrayConstIterState;

static void
arrayconst_startup_fn(Node *clause, PredIterInfo info)
{
	ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *) clause;
	ArrayConstIterState *state;
	Const	   *arrayconst;
	ArrayType  *arrayval;
	int16		elmlen;
	bool		elmbyval;
	char		elmalign;

	/* Create working state struct */
	state = (ArrayConstIterState *) palloc(sizeof(ArrayConstIterState));
	info->state = (void *) state;

	/* Deconstruct the array literal */
	arrayconst = (Const *) lsecond(saop->args);
	arrayval = DatumGetArrayTypeP(arrayconst->constvalue);
	get_typlenbyvalalign(ARR_ELEMTYPE(arrayval),
						 &elmlen, &elmbyval, &elmalign);
	deconstruct_array(arrayval,
					  ARR_ELEMTYPE(arrayval),
					  elmlen, elmbyval, elmalign,
					  &state->elem_values, &state->elem_nulls,
					  &state->num_elems);

	/* Set up a dummy OpExpr to return as the per-item node */
	state->opexpr.xpr.type = T_OpExpr;
	state->opexpr.opno = saop->opno;
	state->opexpr.opfuncid = saop->opfuncid;
	state->opexpr.opresulttype = BOOLOID;
	state->opexpr.opretset = false;
	state->opexpr.args = list_copy(saop->args);

	/* Set up a dummy Const node to hold the per-element values */
	state->constexpr.xpr.type = T_Const;
	state->constexpr.consttype = ARR_ELEMTYPE(arrayval);
	state->constexpr.consttypmod = -1;
	state->constexpr.constlen = elmlen;
	state->constexpr.constbyval = elmbyval;
	lsecond(state->opexpr.args) = &state->constexpr;

	/* Initialize iteration state */
	state->next_elem = 0;
}

static Node *
arrayconst_next_fn(PredIterInfo info)
{
	ArrayConstIterState *state = (ArrayConstIterState *) info->state;

	if (state->next_elem >= state->num_elems)
		return NULL;
	state->constexpr.constvalue = state->elem_values[state->next_elem];
	state->constexpr.constisnull = state->elem_nulls[state->next_elem];
	state->next_elem++;
	return (Node *) &(state->opexpr);
}

static void
arrayconst_cleanup_fn(PredIterInfo info)
{
	ArrayConstIterState *state = (ArrayConstIterState *) info->state;

	pfree(state->elem_values);
	pfree(state->elem_nulls);
	list_free(state->opexpr.args);
	pfree(state);
}

/*
 * PredIterInfo routines for iterating over a ScalarArrayOpExpr with a
 * one-dimensional ArrayExpr array operand.
 */
typedef struct
{
	OpExpr		opexpr;
	ListCell   *next;
} ArrayExprIterState;

static void
arrayexpr_startup_fn(Node *clause, PredIterInfo info)
{
	ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *) clause;
	ArrayExprIterState *state;
	ArrayExpr  *arrayexpr;

	/* Create working state struct */
	state = (ArrayExprIterState *) palloc(sizeof(ArrayExprIterState));
	info->state = (void *) state;

	/* Set up a dummy OpExpr to return as the per-item node */
	state->opexpr.xpr.type = T_OpExpr;
	state->opexpr.opno = saop->opno;
	state->opexpr.opfuncid = saop->opfuncid;
	state->opexpr.opresulttype = BOOLOID;
	state->opexpr.opretset = false;
	state->opexpr.args = list_copy(saop->args);

	/* Initialize iteration variable to first member of ArrayExpr */
	arrayexpr = (ArrayExpr *) lsecond(saop->args);
	state->next = list_head(arrayexpr->elements);
}

static Node *
arrayexpr_next_fn(PredIterInfo info)
{
	ArrayExprIterState *state = (ArrayExprIterState *) info->state;

	if (state->next == NULL)
		return NULL;
	lsecond(state->opexpr.args) = lfirst(state->next);
	state->next = lnext(state->next);
	return (Node *) &(state->opexpr);
}

static void
arrayexpr_cleanup_fn(PredIterInfo info)
{
	ArrayExprIterState *state = (ArrayExprIterState *) info->state;

	list_free(state->opexpr.args);
	pfree(state);
}


/*----------
 * predicate_implied_by_simple_clause
 *	  Does the predicate implication test for a "simple clause" predicate
 *	  and a "simple clause" restriction.
 *
 * We return TRUE if able to prove the implication, FALSE if not.
 *
 * We have three strategies for determining whether one simple clause
 * implies another:
 *
 * A simple and general way is to see if they are equal(); this works for any
 * kind of expression.	(Actually, there is an implied assumption that the
 * functions in the expression are immutable, ie dependent only on their input
 * arguments --- but this was checked for the predicate by the caller.)
 *
 * When the predicate is of the form "foo IS NOT NULL", we can conclude that
 * the predicate is implied if the clause is a strict operator or function
 * that has "foo" as an input.	In this case the clause must yield NULL when
 * "foo" is NULL, which we can take as equivalent to FALSE because we know
 * we are within an AND/OR subtree of a WHERE clause.  (Again, "foo" is
 * already known immutable, so the clause will certainly always fail.)
 *
 * Finally, we may be able to deduce something using knowledge about btree
 * operator families; this is encapsulated in btree_predicate_proof().
 *----------
 */
static bool
predicate_implied_by_simple_clause(Expr *predicate, Node *clause)
{
	/* Allow interrupting long proof attempts */
	CHECK_FOR_INTERRUPTS();

	/* First try the equal() test */
	if (equal((Node *) predicate, clause))
		return true;

	/* Next try the IS NOT NULL case */
	if (predicate && IsA(predicate, NullTest) &&
		((NullTest *) predicate)->nulltesttype == IS_NOT_NULL)
	{
		Expr	   *nonnullarg = ((NullTest *) predicate)->arg;

		/* row IS NOT NULL does not act in the simple way we have in mind */
		if (!type_is_rowtype(exprType((Node *) nonnullarg)))
		{
			if (is_opclause(clause) &&
				list_member_strip(((OpExpr *) clause)->args, nonnullarg) &&
				op_strict(((OpExpr *) clause)->opno))
				return true;
			if (is_funcclause(clause) &&
				list_member_strip(((FuncExpr *) clause)->args, nonnullarg) &&
				func_strict(((FuncExpr *) clause)->funcid))
				return true;
		}
		return false;			/* we can't succeed below... */
	}

	/* Else try btree operator knowledge */
	return btree_predicate_proof(predicate, clause, false);
}

/*----------
 * predicate_refuted_by_simple_clause
 *	  Does the predicate refutation test for a "simple clause" predicate
 *	  and a "simple clause" restriction.
 *
 * We return TRUE if able to prove the refutation, FALSE if not.
 *
 * Unlike the implication case, checking for equal() clauses isn't
 * helpful.
 *
 * When the predicate is of the form "foo IS NULL", we can conclude that
 * the predicate is refuted if the clause is a strict operator or function
 * that has "foo" as an input (see notes for implication case), or if the
 * clause is "foo IS NOT NULL".  A clause "foo IS NULL" refutes a predicate
 * "foo IS NOT NULL", but unfortunately does not refute strict predicates,
 * because we are looking for strong refutation.  (The motivation for covering
 * these cases is to support using IS NULL/IS NOT NULL as partition-defining
 * constraints.)
 *
 * Finally, we may be able to deduce something using knowledge about btree
 * operator families; this is encapsulated in btree_predicate_proof().
 *----------
 */
static bool
predicate_refuted_by_simple_clause(Expr *predicate, Node *clause)
{
	/* Allow interrupting long proof attempts */
	CHECK_FOR_INTERRUPTS();

	/* A simple clause can't refute itself */
	/* Worth checking because of relation_excluded_by_constraints() */
	if ((Node *) predicate == clause)
		return false;

	/* Try the predicate-IS-NULL case */
	if (predicate && IsA(predicate, NullTest) &&
		((NullTest *) predicate)->nulltesttype == IS_NULL)
	{
		Expr	   *isnullarg = ((NullTest *) predicate)->arg;

		/* row IS NULL does not act in the simple way we have in mind */
		if (type_is_rowtype(exprType((Node *) isnullarg)))
			return false;

		/* Any strict op/func on foo refutes foo IS NULL */
		if (is_opclause(clause) &&
			list_member_strip(((OpExpr *) clause)->args, isnullarg) &&
			op_strict(((OpExpr *) clause)->opno))
			return true;
		if (is_funcclause(clause) &&
			list_member_strip(((FuncExpr *) clause)->args, isnullarg) &&
			func_strict(((FuncExpr *) clause)->funcid))
			return true;

		/* foo IS NOT NULL refutes foo IS NULL */
		if (clause && IsA(clause, NullTest) &&
			((NullTest *) clause)->nulltesttype == IS_NOT_NULL &&
			equal(((NullTest *) clause)->arg, isnullarg))
			return true;

		return false;			/* we can't succeed below... */
	}

	/* Try the clause-IS-NULL case */
	if (clause && IsA(clause, NullTest) &&
		((NullTest *) clause)->nulltesttype == IS_NULL)
	{
		Expr	   *isnullarg = ((NullTest *) clause)->arg;

		/* row IS NULL does not act in the simple way we have in mind */
		if (type_is_rowtype(exprType((Node *) isnullarg)))
			return false;

		/* foo IS NULL refutes foo IS NOT NULL */
		if (predicate && IsA(predicate, NullTest) &&
			((NullTest *) predicate)->nulltesttype == IS_NOT_NULL &&
			equal(((NullTest *) predicate)->arg, isnullarg))
			return true;

		return false;			/* we can't succeed below... */
	}

	/* Else try btree operator knowledge */
	return btree_predicate_proof(predicate, clause, true);
}

/**
 * If n is a List, then return an AND tree of the nodes of the list
 * Otherwise return n.
 */
static Node *
convertToExplicitAndsShallowly( Node *n)
{
    if ( IsA(n, List))
    {
        ListCell *cell;
        List *list = (List*)n;
        Node *result = NULL;

        Assert(list_length(list) != 0 );

        foreach( cell, list )
        {
            Node *value = (Node*) lfirst(cell);
            if ( result == NULL)
            {
                result = value;
            }
            else
            {
                result = (Node *) makeBoolExpr(AND_EXPR, list_make2(result, value), -1 /* parse location */);
            }
        }
        return result;
    }
    else return n;
}

/**
 * Check to see if the predicate is expr=constant or constant=expr. In that case, try to evaluate the clause
 *   by replacing every occurrence of expr with the constant.  If the clause can then be reduced to FALSE, we
 *   conclude that the expression is refuted
 *
 * Returns true only if evaluation is possible AND expression is refuted based on evaluation results
 *
 * MPP-18979:
 * This mechanism cannot be used to prove implication. One example expression is
 * "F(x)=1 and x=2", where F(x) is an immutable function that returns 1 for any input x.
 * In this case, replacing x with 2 produces F(2)=1 and 2=2. Although evaluating the resulting
 * expression gives TRUE, we cannot conclude that (x=2) is implied by the whole expression.
 *
 */
static bool
simple_equality_predicate_refuted(Node *clause, Node *predicate)
{
	OpExpr *predicateExpr;
	Node *leftPredicateOp, *rightPredicateOp;
    Node *constExprInPredicate, *varExprInPredicate;
	List *list;

    /* BEGIN inspecting the predicate: this only works for a simple equality predicate */
    if ( nodeTag(predicate) != T_List )
        return false;

    if ( clause == predicate )
        return false; /* don't both doing for self-refutation ... let normal behavior handle that */

    list = (List *) predicate;
    if ( list_length(list) != 1 )
        return false;

    predicate = linitial(list);
	if ( ! is_opclause(predicate))
		return false;

	predicateExpr = (OpExpr*) predicate;
	leftPredicateOp = get_leftop((Expr*)predicate);
	rightPredicateOp = get_rightop((Expr*)predicate);
	if (!leftPredicateOp || !rightPredicateOp)
		return false;

	/* check if it's equality operation */
	if ( ! is_builtin_true_equality_between_same_type(predicateExpr->opno))
		return false;

	/* check if one operand is a constant */
	if ( IsA(rightPredicateOp, Const))
	{
		varExprInPredicate = leftPredicateOp;
		constExprInPredicate = rightPredicateOp;
	}
	else if ( IsA(leftPredicateOp, Const))
	{
		constExprInPredicate = leftPredicateOp;
		varExprInPredicate = rightPredicateOp;
	}
	else
	{
	    return false;
	}

    if ( IsA(varExprInPredicate, RelabelType))
    {
        RelabelType *rt = (RelabelType*) varExprInPredicate;
        varExprInPredicate = (Node*) rt->arg;
    }

    if ( ! IsA(varExprInPredicate, Var))
    {
        /* for now, this code is targeting predicates used in value partitions ...
         *   so don't apply it for other expressions.  This check can probably
         *   simply be removed and some test cases built. */
        return false;
    }

    /* DONE inspecting the predicate */

	/* clause may have non-immutable functions...don't eval if that's the case:
	 *
	 * Note that since we are replacing elements of the clause that match
	 *   varExprInPredicate, there is no need to also check varExprInPredicate
	 *   for mutable functions (note that this is only relevant when the
	 *   earlier check for varExprInPredicate being a Var is removed.
	 */
	if ( contain_mutable_functions(clause))
		return false;

	/* now do the evaluation */
	{
		Node *newClause, *reducedExpression;
		ReplaceExpressionMutatorReplacement replacement;
		bool result = false;
		SwitchedMemoryContext memContext;

		replacement.replaceThis = varExprInPredicate;
		replacement.withThis = constExprInPredicate;
        replacement.numReplacementsDone = 0;

        memContext = AllocSetCreateDefaultContextInCurrentAndSwitchTo( "Predtest");

		newClause = replace_expression_mutator(clause, &replacement);

        if ( replacement.numReplacementsDone > 0)
        {
            newClause = convertToExplicitAndsShallowly(newClause);
            reducedExpression = eval_const_expressions(NULL, newClause);

            if ( IsA(reducedExpression, Const ))
            {
                Const *c = (Const *) reducedExpression;
                if ( c->consttype == BOOLOID &&
                     ! c->constisnull )
                {
                	result = (DatumGetBool(c->constvalue) == false);
                }
            }
        }

        DeleteAndRestoreSwitchedMemoryContext(memContext);
        return result;
	}
}

/*
 * If clause asserts the non-truth of a subclause, return that subclause;
 * otherwise return NULL.
 */
static Node *
extract_not_arg(Node *clause)
{
	if (clause == NULL)
		return NULL;
	if (IsA(clause, BoolExpr))
	{
		BoolExpr   *bexpr = (BoolExpr *) clause;

		if (bexpr->boolop == NOT_EXPR)
			return (Node *) linitial(bexpr->args);
	}
	else if (IsA(clause, BooleanTest))
	{
		BooleanTest *btest = (BooleanTest *) clause;

		if (btest->booltesttype == IS_NOT_TRUE ||
			btest->booltesttype == IS_FALSE ||
			btest->booltesttype == IS_UNKNOWN)
			return (Node *) btest->arg;
	}
	return NULL;
}

/*
 * If clause asserts the falsity of a subclause, return that subclause;
 * otherwise return NULL.
 */
static Node *
extract_strong_not_arg(Node *clause)
{
	if (clause == NULL)
		return NULL;
	if (IsA(clause, BoolExpr))
	{
		BoolExpr   *bexpr = (BoolExpr *) clause;

		if (bexpr->boolop == NOT_EXPR)
			return (Node *) linitial(bexpr->args);
	}
	else if (IsA(clause, BooleanTest))
	{
		BooleanTest *btest = (BooleanTest *) clause;

		if (btest->booltesttype == IS_FALSE)
			return (Node *) btest->arg;
	}
	return NULL;
}


/*
 * Check whether an Expr is equal() to any member of a list, ignoring
 * any top-level RelabelType nodes.  This is legitimate for the purposes
 * we use it for (matching IS [NOT] NULL arguments to arguments of strict
 * functions) because RelabelType doesn't change null-ness.  It's helpful
 * for cases such as a varchar argument of a strict function on text.
 */
static bool
list_member_strip(List *list, Expr *datum)
{
	ListCell   *cell;

	if (datum && IsA(datum, RelabelType))
		datum = ((RelabelType *) datum)->arg;

	foreach(cell, list)
	{
		Expr	   *elem = (Expr *) lfirst(cell);

		if (elem && IsA(elem, RelabelType))
			elem = ((RelabelType *) elem)->arg;

		if (equal(elem, datum))
			return true;
	}

	return false;
}


/*
 * Define an "operator implication table" for btree operators ("strategies"),
 * and a similar table for refutation.
 *
 * The strategy numbers defined by btree indexes (see access/skey.h) are:
 *		(1) <	(2) <=	 (3) =	 (4) >=   (5) >
 * and in addition we use (6) to represent <>.	<> is not a btree-indexable
 * operator, but we assume here that if an equality operator of a btree
 * opfamily has a negator operator, the negator behaves as <> for the opfamily.
 *
 * The interpretation of:
 *
 *		test_op = BT_implic_table[given_op-1][target_op-1]
 *
 * where test_op, given_op and target_op are strategy numbers (from 1 to 6)
 * of btree operators, is as follows:
 *
 *	 If you know, for some ATTR, that "ATTR given_op CONST1" is true, and you
 *	 want to determine whether "ATTR target_op CONST2" must also be true, then
 *	 you can use "CONST2 test_op CONST1" as a test.  If this test returns true,
 *	 then the target expression must be true; if the test returns false, then
 *	 the target expression may be false.
 *
 * For example, if clause is "Quantity > 10" and pred is "Quantity > 5"
 * then we test "5 <= 10" which evals to true, so clause implies pred.
 *
 * Similarly, the interpretation of a BT_refute_table entry is:
 *
 *	 If you know, for some ATTR, that "ATTR given_op CONST1" is true, and you
 *	 want to determine whether "ATTR target_op CONST2" must be false, then
 *	 you can use "CONST2 test_op CONST1" as a test.  If this test returns true,
 *	 then the target expression must be false; if the test returns false, then
 *	 the target expression may be true.
 *
 * For example, if clause is "Quantity > 10" and pred is "Quantity < 5"
 * then we test "5 <= 10" which evals to true, so clause refutes pred.
 *
 * An entry where test_op == 0 means the implication cannot be determined.
 */

#define BTLT BTLessStrategyNumber
#define BTLE BTLessEqualStrategyNumber
#define BTEQ BTEqualStrategyNumber
#define BTGE BTGreaterEqualStrategyNumber
#define BTGT BTGreaterStrategyNumber
#define BTNE 6

static const StrategyNumber BT_implic_table[6][6] = {
/*
 *			The target operator:
 *
 *	 LT    LE	 EQ    GE	 GT    NE
 */
	{BTGE, BTGE, 0, 0, 0, BTGE},	/* LT */
	{BTGT, BTGE, 0, 0, 0, BTGT},	/* LE */
	{BTGT, BTGE, BTEQ, BTLE, BTLT, BTNE},		/* EQ */
	{0, 0, 0, BTLE, BTLT, BTLT},	/* GE */
	{0, 0, 0, BTLE, BTLE, BTLE},	/* GT */
	{0, 0, 0, 0, 0, BTEQ}		/* NE */
};

static const StrategyNumber BT_refute_table[6][6] = {
/*
 *			The target operator:
 *
 *	 LT    LE	 EQ    GE	 GT    NE
 */
	{0, 0, BTGE, BTGE, BTGE, 0},	/* LT */
	{0, 0, BTGT, BTGT, BTGE, 0},	/* LE */
	{BTLE, BTLT, BTNE, BTGT, BTGE, BTEQ},		/* EQ */
	{BTLE, BTLT, BTLT, 0, 0, 0},	/* GE */
	{BTLE, BTLE, BTLE, 0, 0, 0},	/* GT */
	{0, 0, BTEQ, 0, 0, 0}		/* NE */
};


/*
 * btree_predicate_proof
 *	  Does the predicate implication or refutation test for a "simple clause"
 *	  predicate and a "simple clause" restriction, when both are simple
 *	  operator clauses using related btree operators.
 *
 * When refute_it == false, we want to prove the predicate true;
 * when refute_it == true, we want to prove the predicate false.
 * (There is enough common code to justify handling these two cases
 * in one routine.)  We return TRUE if able to make the proof, FALSE
 * if not able to prove it.
 *
 * What we look for here is binary boolean opclauses of the form
 * "foo op constant", where "foo" is the same in both clauses.	The operators
 * and constants can be different but the operators must be in the same btree
 * operator family.  We use the above operator implication tables to
 * derive implications between nonidentical clauses.  (Note: "foo" is known
 * immutable, and constants are surely immutable, but we have to check that
 * the operators are too.  As of 8.0 it's possible for opfamilies to contain
 * operators that are merely stable, and we dare not make deductions with
 * these.)
 */
static bool
btree_predicate_proof(Expr *predicate, Node *clause, bool refute_it)
{
	Node	   *leftop,
			   *rightop;
	Node	   *pred_var,
			   *clause_var;
	Const	   *pred_const,
			   *clause_const;
	bool		pred_var_on_left,
				clause_var_on_left;
	Oid			pred_op,
				clause_op,
				test_op;
	Expr	   *test_expr;
	ExprState  *test_exprstate;
	Datum		test_result;
	bool		isNull;
	EState	   *estate;
	MemoryContext oldcontext;

	/*
	 * Both expressions must be binary opclauses with a Const on one side, and
	 * identical subexpressions on the other sides. Note we don't have to
	 * think about binary relabeling of the Const node, since that would have
	 * been folded right into the Const.
	 *
	 * If either Const is null, we also fail right away; this assumes that the
	 * test operator will always be strict.
	 */
	if (!is_opclause(predicate))
		return false;
	leftop = get_leftop(predicate);
	rightop = get_rightop(predicate);
	if (rightop == NULL)
		return false;			/* not a binary opclause */
	if (IsA(rightop, Const))
	{
		pred_var = leftop;
		pred_const = (Const *) rightop;
		pred_var_on_left = true;
	}
	else if (IsA(leftop, Const))
	{
		pred_var = rightop;
		pred_const = (Const *) leftop;
		pred_var_on_left = false;
	}
	else
		return false;			/* no Const to be found */
	if (pred_const->constisnull)
		return false;

	if (!is_opclause(clause))
		return false;
	leftop = get_leftop((Expr *) clause);
	rightop = get_rightop((Expr *) clause);
	if (rightop == NULL)
		return false;			/* not a binary opclause */
	if (IsA(rightop, Const))
	{
		clause_var = leftop;
		clause_const = (Const *) rightop;
		clause_var_on_left = true;
	}
	else if (IsA(leftop, Const))
	{
		clause_var = rightop;
		clause_const = (Const *) leftop;
		clause_var_on_left = false;
	}
	else
		return false;			/* no Const to be found */
	if (clause_const->constisnull)
		return false;

	/*
	 * Check for matching subexpressions on the non-Const sides.  We used to
	 * only allow a simple Var, but it's about as easy to allow any
	 * expression.	Remember we already know that the pred expression does not
	 * contain any non-immutable functions, so identical expressions should
	 * yield identical results.
	 */
	if (!equal(pred_var, clause_var))
		return false;

	/*
	 * Okay, get the operators in the two clauses we're comparing. Commute
	 * them if needed so that we can assume the variables are on the left.
	 */
	pred_op = ((OpExpr *) predicate)->opno;
	if (!pred_var_on_left)
	{
		pred_op = get_commutator(pred_op);
		if (!OidIsValid(pred_op))
			return false;
	}

	clause_op = ((OpExpr *) clause)->opno;
	if (!clause_var_on_left)
	{
		clause_op = get_commutator(clause_op);
		if (!OidIsValid(clause_op))
			return false;
	}

	/*
	 * Lookup the comparison operator using the system catalogs and the
	 * operator implication tables.
	 */
	test_op = get_btree_test_op(pred_op, clause_op, refute_it);

	if (!OidIsValid(test_op))
	{
		/* couldn't find a suitable comparison operator */
		return false;
	}

	/*
	 * Evaluate the test.  For this we need an EState.
	 */
	estate = CreateExecutorState();

	/* We can use the estate's working context to avoid memory leaks. */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/* Build expression tree */
	test_expr = make_opclause(test_op,
							  BOOLOID,
							  false,
							  (Expr *) pred_const,
							  (Expr *) clause_const);

	/* Fill in opfuncids */
	fix_opfuncids((Node *) test_expr);

	/* Prepare it for execution */
	test_exprstate = ExecInitExpr(test_expr, NULL);

	/* And execute it. */
	test_result = ExecEvalExprSwitchContext(test_exprstate,
											GetPerTupleExprContext(estate),
											&isNull, NULL);

	/* Get back to outer memory context */
	MemoryContextSwitchTo(oldcontext);

	/* Release all the junk we just created */
	FreeExecutorState(estate);

	if (isNull)
	{
		/* Treat a null result as non-proof ... but it's a tad fishy ... */
		elog(DEBUG2, "null predicate test result");
		return false;
	}
	return DatumGetBool(test_result);
}


/*
 * We use a lookaside table to cache the result of btree proof operator
 * lookups, since the actual lookup is pretty expensive and doesn't change
 * for any given pair of operators (at least as long as pg_amop doesn't
 * change).  A single hash entry stores both positive and negative results
 * for a given pair of operators.
 */
typedef struct OprProofCacheKey
{
	Oid			pred_op;		/* predicate operator */
	Oid			clause_op;		/* clause operator */
} OprProofCacheKey;

typedef struct OprProofCacheEntry
{
	/* the hash lookup key MUST BE FIRST */
	OprProofCacheKey key;

	bool		have_implic;	/* do we know the implication result? */
	bool		have_refute;	/* do we know the refutation result? */
	Oid			implic_test_op; /* OID of the operator, or 0 if none */
	Oid			refute_test_op; /* OID of the operator, or 0 if none */
} OprProofCacheEntry;

static HTAB *OprProofCacheHash = NULL;


/*
 * get_btree_test_op
 *	  Identify the comparison operator needed for a btree-operator
 *	  proof or refutation.
 *
 * Given the truth of a predicate "var pred_op const1", we are attempting to
 * prove or refute a clause "var clause_op const2".  The identities of the two
 * operators are sufficient to determine the operator (if any) to compare
 * const2 to const1 with.
 *
 * Returns the OID of the operator to use, or InvalidOid if no proof is
 * possible.
 */
static Oid
get_btree_test_op(Oid pred_op, Oid clause_op, bool refute_it)
{
	OprProofCacheKey key;
	OprProofCacheEntry *cache_entry;
	bool		cfound;
	bool		pred_op_negated;
	Oid			pred_op_negator,
				clause_op_negator,
				test_op = InvalidOid;
	Oid			opfamily_id;
	bool		found = false;
	StrategyNumber pred_strategy,
				clause_strategy,
				test_strategy;
	Oid			clause_righttype;
	CatCList   *catlist;
	int			i;

	/*
	 * Find or make a cache entry for this pair of operators.
	 */
	if (OprProofCacheHash == NULL)
	{
		/* First time through: initialize the hash table */
		HASHCTL		ctl;

		if (!CacheMemoryContext)
			CreateCacheMemoryContext();

		MemSet(&ctl, 0, sizeof(ctl));
		ctl.keysize = sizeof(OprProofCacheKey);
		ctl.entrysize = sizeof(OprProofCacheEntry);
		ctl.hash = tag_hash;
		OprProofCacheHash = hash_create("Btree proof lookup cache", 256,
										&ctl, HASH_ELEM | HASH_FUNCTION);

		/* Arrange to flush cache on pg_amop changes */
		CacheRegisterSyscacheCallback(AMOPOPID,
									  InvalidateOprProofCacheCallBack,
									  (Datum) 0);
	}

	key.pred_op = pred_op;
	key.clause_op = clause_op;
	cache_entry = (OprProofCacheEntry *) hash_search(OprProofCacheHash,
													 (void *) &key,
													 HASH_ENTER, &cfound);
	if (!cfound)
	{
		/* new cache entry, set it invalid */
		cache_entry->have_implic = false;
		cache_entry->have_refute = false;
	}
	else
	{
		/* pre-existing cache entry, see if we know the answer */
		if (refute_it)
		{
			if (cache_entry->have_refute)
				return cache_entry->refute_test_op;
		}
		else
		{
			if (cache_entry->have_implic)
				return cache_entry->implic_test_op;
		}
	}

	/*
	 * Try to find a btree opfamily containing the given operators.
	 *
	 * We must find a btree opfamily that contains both operators, else the
	 * implication can't be determined.  Also, the opfamily must contain a
	 * suitable test operator taking the operators' righthand datatypes.
	 *
	 * If there are multiple matching opfamilies, assume we can use any one to
	 * determine the logical relationship of the two operators and the correct
	 * corresponding test operator.  This should work for any logically
	 * consistent opfamilies.
	 */
	catlist = SearchSysCacheList(AMOPOPID, 1,
								 ObjectIdGetDatum(pred_op),
								 0, 0, 0);

	/*
	 * If we couldn't find any opfamily containing the pred_op, perhaps it is
	 * a <> operator.  See if it has a negator that is in an opfamily.
	 */
	pred_op_negated = false;
	if (catlist->n_members == 0)
	{
		pred_op_negator = get_negator(pred_op);
		if (OidIsValid(pred_op_negator))
		{
			pred_op_negated = true;
			ReleaseSysCacheList(catlist);
			catlist = SearchSysCacheList(AMOPOPID, 1,
										 ObjectIdGetDatum(pred_op_negator),
										 0, 0, 0);
		}
	}

	/* Also may need the clause_op's negator */
	clause_op_negator = get_negator(clause_op);

	/* Now search the opfamilies */
	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	pred_tuple = &catlist->members[i]->tuple;
		Form_pg_amop pred_form = (Form_pg_amop) GETSTRUCT(pred_tuple);
		HeapTuple	clause_tuple;

		/* Must be btree */
		if (pred_form->amopmethod != BTREE_AM_OID)
			continue;

		/* Get the predicate operator's btree strategy number */
		opfamily_id = pred_form->amopfamily;
		pred_strategy = (StrategyNumber) pred_form->amopstrategy;
		Assert(pred_strategy >= 1 && pred_strategy <= 5);

		if (pred_op_negated)
		{
			/* Only consider negators that are = */
			if (pred_strategy != BTEqualStrategyNumber)
				continue;
			pred_strategy = BTNE;
		}

		/*
		 * From the same opfamily, find a strategy number for the clause_op,
		 * if possible
		 */
		clause_tuple = SearchSysCache(AMOPOPID,
									  ObjectIdGetDatum(clause_op),
									  ObjectIdGetDatum(opfamily_id),
									  0, 0);
		if (HeapTupleIsValid(clause_tuple))
		{
			Form_pg_amop clause_form = (Form_pg_amop) GETSTRUCT(clause_tuple);

			/* Get the restriction clause operator's strategy/datatype */
			clause_strategy = (StrategyNumber) clause_form->amopstrategy;
			Assert(clause_strategy >= 1 && clause_strategy <= 5);
			Assert(clause_form->amoplefttype == pred_form->amoplefttype);
			clause_righttype = clause_form->amoprighttype;
			ReleaseSysCache(clause_tuple);
		}
		else if (OidIsValid(clause_op_negator))
		{
			clause_tuple = SearchSysCache(AMOPOPID,
										  ObjectIdGetDatum(clause_op_negator),
										  ObjectIdGetDatum(opfamily_id),
										  0, 0);
			if (HeapTupleIsValid(clause_tuple))
			{
				Form_pg_amop clause_form = (Form_pg_amop) GETSTRUCT(clause_tuple);

				/* Get the restriction clause operator's strategy/datatype */
				clause_strategy = (StrategyNumber) clause_form->amopstrategy;
				Assert(clause_strategy >= 1 && clause_strategy <= 5);
				Assert(clause_form->amoplefttype == pred_form->amoplefttype);
				clause_righttype = clause_form->amoprighttype;
				ReleaseSysCache(clause_tuple);

				/* Only consider negators that are = */
				if (clause_strategy != BTEqualStrategyNumber)
					continue;
				clause_strategy = BTNE;
			}
			else
				continue;
		}
		else
			continue;

		/*
		 * Look up the "test" strategy number in the implication table
		 */
		if (refute_it)
			test_strategy = BT_refute_table[clause_strategy - 1][pred_strategy - 1];
		else
			test_strategy = BT_implic_table[clause_strategy - 1][pred_strategy - 1];

		if (test_strategy == 0)
		{
			/* Can't determine implication using this interpretation */
			continue;
		}

		/*
		 * See if opfamily has an operator for the test strategy and the
		 * datatypes.
		 */
		if (test_strategy == BTNE)
		{
			test_op = get_opfamily_member(opfamily_id,
										  pred_form->amoprighttype,
										  clause_righttype,
										  BTEqualStrategyNumber);
			if (OidIsValid(test_op))
				test_op = get_negator(test_op);
		}
		else
		{
			test_op = get_opfamily_member(opfamily_id,
										  pred_form->amoprighttype,
										  clause_righttype,
										  test_strategy);
		}
		if (OidIsValid(test_op))
		{
			/*
			 * Last check: test_op must be immutable.
			 *
			 * Note that we require only the test_op to be immutable, not the
			 * original clause_op.	(pred_op is assumed to have been checked
			 * immutable by the caller.)  Essentially we are assuming that the
			 * opfamily is consistent even if it contains operators that are
			 * merely stable.
			 */
			if (op_volatile(test_op) == PROVOLATILE_IMMUTABLE)
			{
				found = true;
				break;
			}
		}
	}

	ReleaseSysCacheList(catlist);

	if (!found)
	{
		/* couldn't find a suitable comparison operator */
		test_op = InvalidOid;
	}

	/* Cache the result, whether positive or negative */
	if (refute_it)
	{
		cache_entry->refute_test_op = test_op;
		cache_entry->have_refute = true;
	}
	else
	{
		cache_entry->implic_test_op = test_op;
		cache_entry->have_implic = true;
	}

	return test_op;
}


/*
 * Callback for pg_amop inval events
 */
static void
InvalidateOprProofCacheCallBack(Datum arg, int cacheid, ItemPointer tuplePtr)
{
	HASH_SEQ_STATUS status;
	OprProofCacheEntry *hentry;

	Assert(OprProofCacheHash != NULL);

	/* Currently we just reset all entries; hard to be smarter ... */
	hash_seq_init(&status, OprProofCacheHash);

	while ((hentry = (OprProofCacheEntry *) hash_seq_search(&status)) != NULL)
	{
		hentry->have_implic = false;
		hentry->have_refute = false;
	}
}

typedef struct ConstHashValue
{
	Const * c;
} ConstHashValue;

static void
CalculateHashWithHashAny(void *clientData, void *buf, size_t len)
{
	uint32 *result = (uint32*) clientData;
	*result = hash_any((unsigned char *)buf, len );
}

static uint32
ConstHashTableHash(const void *keyPtr, Size keysize)
{
	uint32 result;
	Const *c = *((Const **)keyPtr);

	if ( c->constisnull)
	{
		hashNullDatum(CalculateHashWithHashAny, &result);
	}
	else
	{
		hashDatum(c->constvalue, c->consttype, CalculateHashWithHashAny, &result);
	}
	return result;
}

static int
ConstHashTableMatch(const void*keyPtr1, const void *keyPtr2, Size keysize)
{
	Node *left = *((Node **)keyPtr1);
	Node *right = *((Node **)keyPtr2);
	return equal(left, right) ? 0 : 1;
}

/**
 * returns a hashtable that can be used to map from a node to itself
 */
static HTAB*
CreateNodeSetHashTable(MemoryContext memoryContext)
{
	HASHCTL	hash_ctl;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));

	hash_ctl.keysize = sizeof(Const**);
	hash_ctl.entrysize = sizeof(ConstHashValue);
	hash_ctl.hash = ConstHashTableHash;
	hash_ctl.match = ConstHashTableMatch;
	hash_ctl.hcxt = memoryContext;

	return hash_create("ConstantSet", 16, &hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);
}

/**
 * basic operation on PossibleValueSet:  initialize to "any value possible"
 */
void
InitPossibleValueSetData(PossibleValueSet *pvs)
{
	pvs->memoryContext = NULL;
	pvs->set = NULL;
	pvs->isAnyValuePossible = true;
}

/**
 * Take the values from the given PossibleValueSet and return them as an allocated array.
 *
 * @param pvs the set to turn into an array
 * @param numValuesOut receives the length of the returned array
 * @return the array of Node objects
 */
Node **
GetPossibleValuesAsArray( PossibleValueSet *pvs, int *numValuesOut )
{
	HASH_SEQ_STATUS status;
	ConstHashValue *value;
	List *list = NULL;
	Node ** result;
	int numValues, i;
	ListCell *lc;

	if ( pvs->set == NULL)
	{
		*numValuesOut = 0;
		return NULL;
	}

	hash_seq_init(&status, pvs->set);
	while ((value = (ConstHashValue*) hash_seq_search(&status)) != NULL)
	{
		list = lappend(list, copyObject(value->c));
	}

	numValues = list_length(list);
	result = palloc(sizeof(Node*) * numValues);
	foreach_with_count( lc, list, i)
	{
		result[i] = (Node*) lfirst(lc);
	}

	*numValuesOut = numValues;
	return result;
}

/**
 * basic operation on PossibleValueSet:  cleanup
 */
void
DeletePossibleValueSetData(PossibleValueSet *pvs)
{
	if ( pvs->set != NULL)
	{
		Assert(pvs->memoryContext != NULL);

		MemoryContextDelete(pvs->memoryContext);
		pvs->memoryContext = NULL;
		pvs->set = NULL;
	}
	pvs->isAnyValuePossible = true;
}

/**
 * basic operation on PossibleValueSet:  add a value to the set field of PossibleValueSet
 *
 * The caller must verify that the valueToCopy is greenplum hashable
 */
static void
AddValue(PossibleValueSet *pvs, Const *valueToCopy)
{
	Assert( isGreenplumDbHashable(valueToCopy->consttype));

	if ( pvs->set == NULL)
	{
		Assert(pvs->memoryContext == NULL);

		pvs->memoryContext = AllocSetContextCreate(CurrentMemoryContext,
													   "PossibleValueSet",
													   ALLOCSET_DEFAULT_MINSIZE,
													   ALLOCSET_DEFAULT_INITSIZE,
													   ALLOCSET_DEFAULT_MAXSIZE);
		pvs->set = CreateNodeSetHashTable(pvs->memoryContext);
	}

	if ( ! ContainsValue(pvs, valueToCopy))
	{
		bool found; /* unused but needed in call */
		MemoryContext oldContext = MemoryContextSwitchTo(pvs->memoryContext);

		Const *key = copyObject(valueToCopy);
		void *entry = hash_search(pvs->set, &key, HASH_ENTER, &found);

		if ( entry == NULL)
		{
			ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
		}
		((ConstHashValue*)entry)->c = key;

		MemoryContextSwitchTo(oldContext);
	}
}

static void
SetToNoValuesPossible(PossibleValueSet *pvs)
{
	if ( pvs->memoryContext )
	{
		MemoryContextDelete(pvs->memoryContext);
	}
	pvs->memoryContext = AllocSetContextCreate(CurrentMemoryContext,
												   "PossibleValueSet",
												   ALLOCSET_DEFAULT_MINSIZE,
												   ALLOCSET_DEFAULT_INITSIZE,
												   ALLOCSET_DEFAULT_MAXSIZE);
	pvs->set = CreateNodeSetHashTable(pvs->memoryContext);
	pvs->isAnyValuePossible = false;
}

/**
 * basic operation on PossibleValueSet:  remove a value from the set field of PossibleValueSet
 */
static void
RemoveValue(PossibleValueSet *pvs, Const *value)
{
	bool found; /* unused, needed in call */
	Assert( pvs->set != NULL);
	hash_search(pvs->set, &value, HASH_REMOVE, &found);
}

/**
 * basic operation on PossibleValueSet:  determine if a value is contained in the set field of PossibleValueSet
 */
static bool
ContainsValue(PossibleValueSet *pvs, Const *value)
{
	bool found = false;
	Assert(!pvs->isAnyValuePossible);
	if ( pvs->set != NULL)
		hash_search(pvs->set, &value, HASH_FIND, &found);
	return found;
}

/**
 * in-place union operation
 */
static void
AddUnmatchingValues( PossibleValueSet *pvs, PossibleValueSet *toCheck )
{
	HASH_SEQ_STATUS status;
	ConstHashValue *value;

	Assert(!pvs->isAnyValuePossible);
	Assert(!toCheck->isAnyValuePossible);

	hash_seq_init(&status, toCheck->set);
	while ((value = (ConstHashValue*) hash_seq_search(&status)) != NULL)
	{
		AddValue(pvs, value->c);
	}
}

/**
 * in-place intersection operation
 */
static void
RemoveUnmatchingValues(PossibleValueSet *pvs, PossibleValueSet *toCheck)
{
	List *toRemove = NULL;
	ListCell *lc;
	HASH_SEQ_STATUS status;
	ConstHashValue *value;

	Assert(!pvs->isAnyValuePossible);
	Assert(!toCheck->isAnyValuePossible);

	hash_seq_init(&status, pvs->set);
	while ((value = (ConstHashValue*) hash_seq_search(&status)) != NULL)
	{
		if ( ! ContainsValue(toCheck, value->c ))
		{
			toRemove = lappend(toRemove, value->c);
		}
	}

	/* remove after so we don't mod hashtable underneath iteration */
	foreach(lc, toRemove)
	{
		Const *value = (Const*) lfirst(lc);
		RemoveValue(pvs, value);
	}
	list_free(toRemove);
}

/**
 * Process an AND clause -- this can do a INTERSECTION between sets learned from child clauses
 */
static PossibleValueSet
ProcessAndClauseForPossibleValues( PredIterInfoData *clauseInfo, Node *clause, Node *variable)
{
	PossibleValueSet result;
	InitPossibleValueSetData(&result);

	iterate_begin(child, clause, *clauseInfo)
	{
		PossibleValueSet childPossible = DeterminePossibleValueSet( child, variable );
		if ( childPossible.isAnyValuePossible)
		{
			/* any value possible, this AND member does not add any information */
			DeletePossibleValueSetData( &childPossible);
		}
		else
		{
			/* a particular set so this AND member can refine our estimate */
			if ( result.isAnyValuePossible )
			{
				/* current result was not informative so just take the child */
				result = childPossible;
			}
			else
			{
				/* result.set AND childPossible.set: do intersection inside result */
				RemoveUnmatchingValues( &result, &childPossible );
				DeletePossibleValueSetData( &childPossible);
			}
		}
	}
	iterate_end(*clauseInfo);

	return result;
}

/**
 * Process an OR clause -- this can do a UNION between sets learned from child clauses
 */
static PossibleValueSet
ProcessOrClauseForPossibleValues( PredIterInfoData *clauseInfo, Node *clause, Node *variable)
{
	PossibleValueSet result;
	InitPossibleValueSetData(&result);

	iterate_begin(child, clause, *clauseInfo)
	{
		PossibleValueSet childPossible = DeterminePossibleValueSet( child, variable );
		if ( childPossible.isAnyValuePossible)
		{
			/* any value is possible for the entire AND */
			DeletePossibleValueSetData( &childPossible );
			DeletePossibleValueSetData( &result );

			/* it can't improve once a part of the OR accepts all, so just quit */
			result.isAnyValuePossible = true;
			break;
		}

		if ( result.isAnyValuePossible )
		{
			/* first one in loop so just take it */
			result = childPossible;
		}
		else
		{
			/* result.set OR childPossible.set --> do union into result */
			AddUnmatchingValues( &result, &childPossible );
			DeletePossibleValueSetData( &childPossible);
		}
	}
	iterate_end(*clauseInfo);

	return result;
}

/**
 * Check to see if the given OpExpr is a valid equality between the listed variable and a constant.
 *
 * @param expr the expression to check for being a valid quality
 * @param variable the varaible to look for
 * @param resultOut will be updated with the modified values
 */
static bool
TryProcessEqualityNodeForPossibleValues(OpExpr *expr, Node *variable, PossibleValueSet *resultOut )
{
	Node *leftop, *rightop, *varExpr;
    Const *constExpr;
    bool constOnRight;

	InitPossibleValueSetData(resultOut);

	leftop = get_leftop((Expr*)expr);
	rightop = get_rightop((Expr*)expr);
	if (!leftop || !rightop)
		return false;

	/* check if one operand is a constant */
	if ( IsA(rightop, Const))
	{
		varExpr = leftop;
		constExpr = (Const *) rightop;
		constOnRight = true;
	}
	else if ( IsA(leftop, Const))
	{
		constExpr = (Const *) leftop;
		varExpr = rightop;
		constOnRight = false;
	}
	else
	{
		/** not a constant?  Learned nothing */
		return false;
	}

	if ( constExpr->constisnull)
	{
		/* null doesn't help us */
		return false;
	}

	if ( IsA(varExpr, RelabelType))
	{
		RelabelType *rt = (RelabelType*) varExpr;
		varExpr = (Node*) rt->arg;
	}

	if ( ! equal(varExpr, variable))
	{
		/**
		 * Not talking about our variable?  Learned nothing
		 */
		return false;
	}

	/* check if it's equality operation */
	if ( is_builtin_greenplum_hashable_equality_between_same_type(expr->opno))
	{
		if ( isGreenplumDbHashable(constExpr->consttype))
		{
			/**
			 * Found a constant match!
			 */
			resultOut->isAnyValuePossible = false;
			AddValue(resultOut, constExpr);
		}
		else
		{
			/**
			 * Not cdb hashable, can't determine the value
			 */
			resultOut->isAnyValuePossible = true;
		}
		return true;
	}
	else
	{
		Oid consttype;
		Datum constvalue;

		/* try to handle equality between differently-sized integer types */
		bool isOverflow = false;
		switch ( expr->opno )
		{
			case Int84EqualOperator:
			case Int48EqualOperator:
			{
				bool bigOnRight = expr->opno == Int48EqualOperator;
				if ( constOnRight == bigOnRight )
				{
					// convert large constant to small
					int64 val =  DatumGetInt64(constExpr->constvalue);

					if ( val > INT32MAX || val < INT32MIN )
					{
						isOverflow = true;
					}
					else
					{
						consttype = INT4OID;
						constvalue = Int32GetDatum((int32)val);
					}
				}
				else
				{
					// convert small constant to small
					int32 val =  DatumGetInt32(constExpr->constvalue);

					consttype = INT8OID;
					constvalue = Int64GetDatum(val);
				}
				break;
			}
			case Int24EqualOperator:
			case Int42EqualOperator:
			{
				bool bigOnRight = expr->opno == Int24EqualOperator;
				if ( constOnRight == bigOnRight )
				{
					// convert large constant to small
					int32 val =  DatumGetInt32(constExpr->constvalue);

					if ( val > INT16MAX || val < INT16MIN )
					{
						isOverflow = true;
					}
					else
					{
						consttype = INT2OID;
						constvalue = Int16GetDatum((int16)val);
					}
				}
				else
				{
					// convert small constant to small
					int16 val =  DatumGetInt16(constExpr->constvalue);

					consttype = INT4OID;
					constvalue = Int32GetDatum(val);
				}
				break;
			}
			case Int28EqualOperator:
			case Int82EqualOperator:
			{
				bool bigOnRight = expr->opno == Int28EqualOperator;
				if ( constOnRight == bigOnRight )
				{
					// convert large constant to small
					int64 val =  DatumGetInt64(constExpr->constvalue);

					if ( val > INT16MAX || val < INT16MIN )
					{
						isOverflow = true;
					}
					else
					{
						consttype = INT2OID;
						constvalue = Int16GetDatum((int16)val);
					}
				}
				else
				{
					// convert small constant to small
					int16 val =  DatumGetInt16(constExpr->constvalue);

					consttype = INT8OID;
					constvalue = Int64GetDatum(val);
				}
				break;
			}
			default:
				/* not a useful operator ... */
				return false;
		}

		if ( isOverflow )
		{
			SetToNoValuesPossible(resultOut);
		}
		else
		{
			/* okay, got a new constant value .. set it and done!*/
			Const *newConst;
			int constlen = 0;

			Assert(isGreenplumDbHashable(consttype));

			switch ( consttype)
			{
				case INT8OID:
					constlen = sizeof(int64);
					break;
				case INT4OID:
					constlen = sizeof(int32);
					break;
				case INT2OID:
					constlen = sizeof(int16);
					break;
				default:
					Assert(!"unreachable");
			}

			newConst = makeConst(consttype, /* consttypmod */ 0, constlen, constvalue,
				/* constisnull */ false, /* constbyval */ true);


			resultOut->isAnyValuePossible = false;
			AddValue(resultOut, newConst);

			pfree(newConst);
		}
		return true;
	}
}

/**
 *
 * Get the possible values of variable, as determined by the given qualification clause
 *
 * Note that only variables whose type is greenplumDbHashtable will return an actual finite set of values.  All others
 *    will go to the default behavior -- return that any value is possible
 *
 * Note that if there are two variables to check, you must call this twice.  This then means that
 *    if the two variables are dependent you won't learn of that -- you only know that the set of
 *    possible values is within the cross-product of the two variables' sets
 */
PossibleValueSet
DeterminePossibleValueSet( Node *clause, Node *variable)
{
	PredIterInfoData clauseInfo;
	PossibleValueSet result;

	if ( clause == NULL )
	{
		InitPossibleValueSetData(&result);
		return result;
	}

	switch (predicate_classify(clause, &clauseInfo))
	{
		case CLASS_AND:
			return ProcessAndClauseForPossibleValues(&clauseInfo, clause, variable);
		case CLASS_OR:
			return ProcessOrClauseForPossibleValues(&clauseInfo, clause, variable);
		case CLASS_ATOM:
			if (IsA(clause, OpExpr) &&
				TryProcessEqualityNodeForPossibleValues((OpExpr*)clause, variable, &result))
			{
				return result;
			}
			/* can't infer anything, so return that any value is possible */
			InitPossibleValueSetData(&result);
			return result;
	}


	/* can't get here */
	elog(ERROR, "predicate_classify returned a bad value");
	return result;
}
