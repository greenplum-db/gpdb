/*-------------------------------------------------------------------------
 *
 * nodeFuncs.c
 *	  All node routines more complicated than simple access/modification
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/nodes/nodeFuncs.c,v 1.35 2008/10/21 20:42:52 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/nodeFuncs.h"

static int	leftmostLoc(int loc1, int loc2);
static bool var_is_inner(Var *var);



/*
 *	exprLocation -
 *	  returns the parse location of an expression tree, for error reports
 *
 * -1 is returned if the location can't be determined.
 *
 * For expressions larger than a single token, the intent here is to
 * return the location of the expression's leftmost token, not necessarily
 * the topmost Node's location field.  For example, an OpExpr's location
 * field will point at the operator name, but if it is not a prefix operator
 * then we should return the location of the left-hand operand instead.
 * The reason is that we want to reference the entire expression not just
 * that operator, and pointing to its start seems to be the most natural way.
 *
 * The location is not perfect --- for example, since the grammar doesn't
 * explicitly represent parentheses in the parsetree, given something that
 * had been written "(a + b) * c" we are going to point at "a" not "(".
 * But it should be plenty good enough for error reporting purposes.
 *
 * You might think that this code is overly general, for instance why check
 * the operands of a FuncExpr node, when the function name can be expected
 * to be to the left of them?  There are a couple of reasons.  The grammar
 * sometimes builds expressions that aren't quite what the user wrote;
 * for instance x IS NOT BETWEEN ... becomes a NOT-expression whose keyword
 * pointer is to the right of its leftmost argument.  Also, nodes that were
 * inserted implicitly by parse analysis (such as FuncExprs for implicit
 * coercions) will have location -1, and so we can have odd combinations of
 * known and unknown locations in a tree.
 */
int
exprLocation(Node *expr)
{
	int			loc;

	if (expr == NULL)
		return -1;
	switch (nodeTag(expr))
	{
		case T_RangeVar:
			loc = ((RangeVar *) expr)->location;
			break;
		case T_Var:
			loc = ((Var *) expr)->location;
			break;
		case T_Const:
			loc = ((Const *) expr)->location;
			break;
		case T_Param:
			loc = ((Param *) expr)->location;
			break;
		case T_Aggref:
			/* function name should always be the first thing */
			loc = ((Aggref *) expr)->location;
			break;
		case T_WindowRef:
			/* function name should always be the first thing */
			loc = ((WindowRef *) expr)->location;
			break;
		case T_ArrayRef:
			/* just use array argument's location */
			loc = exprLocation((Node *) ((ArrayRef *) expr)->refexpr);
			break;
		case T_FuncExpr:
			{
				FuncExpr   *fexpr = (FuncExpr *) expr;

				/* consider both function name and leftmost arg */
				loc = leftmostLoc(fexpr->location,
								  exprLocation((Node *) fexpr->args));
			}
			break;
		case T_OpExpr:
		case T_DistinctExpr:	/* struct-equivalent to OpExpr */
		case T_NullIfExpr:		/* struct-equivalent to OpExpr */
			{
				OpExpr   *opexpr = (OpExpr *) expr;

				/* consider both operator name and leftmost arg */
				loc = leftmostLoc(opexpr->location,
								  exprLocation((Node *) opexpr->args));
			}
			break;
		case T_ScalarArrayOpExpr:
			{
				ScalarArrayOpExpr *saopexpr = (ScalarArrayOpExpr *) expr;

				/* consider both operator name and leftmost arg */
				loc = leftmostLoc(saopexpr->location,
								  exprLocation((Node *) saopexpr->args));
			}
			break;
		case T_BoolExpr:
			{
				BoolExpr   *bexpr = (BoolExpr *) expr;

				/*
				 * Same as above, to handle either NOT or AND/OR.  We can't
				 * special-case NOT because of the way that it's used for
				 * things like IS NOT BETWEEN.
				 */
				loc = leftmostLoc(bexpr->location,
								  exprLocation((Node *) bexpr->args));
			}
			break;
		case T_SubLink:
			{
				SubLink *sublink = (SubLink *) expr;

				/* check the testexpr, if any, and the operator/keyword */
				loc = leftmostLoc(exprLocation(sublink->testexpr),
								  sublink->location);
			}
			break;
		case T_FieldSelect:
			/* just use argument's location */
			loc = exprLocation((Node *) ((FieldSelect *) expr)->arg);
			break;
		case T_FieldStore:
			/* just use argument's location */
			loc = exprLocation((Node *) ((FieldStore *) expr)->arg);
			break;
		case T_RelabelType:
			{
				RelabelType *rexpr = (RelabelType *) expr;

				/* Much as above */
				loc = leftmostLoc(rexpr->location,
								  exprLocation((Node *) rexpr->arg));
			}
			break;
		case T_CoerceViaIO:
			{
				CoerceViaIO *cexpr = (CoerceViaIO *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_ArrayCoerceExpr:
			{
				ArrayCoerceExpr *cexpr = (ArrayCoerceExpr *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_ConvertRowtypeExpr:
			{
				ConvertRowtypeExpr *cexpr = (ConvertRowtypeExpr *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_CaseExpr:
			/* CASE keyword should always be the first thing */
			loc = ((CaseExpr *) expr)->location;
			break;
		case T_CaseWhen:
			/* WHEN keyword should always be the first thing */
			loc = ((CaseWhen *) expr)->location;
			break;
		case T_ArrayExpr:
			/* the location points at ARRAY or [, which must be leftmost */
			loc = ((ArrayExpr *) expr)->location;
			break;
		case T_RowExpr:
			/* the location points at ROW or (, which must be leftmost */
			loc = ((RowExpr *) expr)->location;
			break;
		case T_TableValueExpr:
			/* the location points at TABLE, which must be leftmost */
			loc = ((TableValueExpr *) expr)->location;
			break;
		case T_RowCompareExpr:
			/* just use leftmost argument's location */
			loc = exprLocation((Node *) ((RowCompareExpr *) expr)->largs);
			break;
		case T_CoalesceExpr:
			/* COALESCE keyword should always be the first thing */
			loc = ((CoalesceExpr *) expr)->location;
			break;
		case T_MinMaxExpr:
			/* GREATEST/LEAST keyword should always be the first thing */
			loc = ((MinMaxExpr *) expr)->location;
			break;
		case T_XmlExpr:
			{
				XmlExpr    *xexpr = (XmlExpr *) expr;

				/* consider both function name and leftmost arg */
				loc = leftmostLoc(xexpr->location,
								  exprLocation((Node *) xexpr->args));
			}
			break;
		case T_NullTest:
			/* just use argument's location */
			loc = exprLocation((Node *) ((NullTest *) expr)->arg);
			break;
		case T_BooleanTest:
			/* just use argument's location */
			loc = exprLocation((Node *) ((BooleanTest *) expr)->arg);
			break;
		case T_CoerceToDomain:
			{
				CoerceToDomain *cexpr = (CoerceToDomain *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_CoerceToDomainValue:
			loc = ((CoerceToDomainValue *) expr)->location;
			break;
		case T_SetToDefault:
			loc = ((SetToDefault *) expr)->location;
			break;
		case T_TargetEntry:
			/* just use argument's location */
			loc = exprLocation((Node *) ((TargetEntry *) expr)->expr);
			break;
		case T_IntoClause:
			/* use the contained RangeVar's location --- close enough */
			loc = exprLocation((Node *) ((IntoClause *) expr)->rel);
			break;
		case T_List:
			{
				/* report location of first list member that has a location */
				ListCell   *lc;

				loc = -1;		/* just to suppress compiler warning */
				foreach(lc, (List *) expr)
				{
					loc = exprLocation((Node *) lfirst(lc));
					if (loc >= 0)
						break;
				}
			}
			break;
		case T_A_Expr:
			{
				A_Expr *aexpr = (A_Expr *) expr;

				/* use leftmost of operator or left operand (if any) */
				/* we assume right operand can't be to left of operator */
				loc = leftmostLoc(aexpr->location,
								  exprLocation(aexpr->lexpr));
			}
			break;
		case T_ColumnRef:
			loc = ((ColumnRef *) expr)->location;
			break;
		case T_ParamRef:
			loc = ((ParamRef *) expr)->location;
			break;
		case T_A_Const:
			loc = ((A_Const *) expr)->location;
			break;
		case T_FuncCall:
			{
				FuncCall *fc = (FuncCall *) expr;

				/* consider both function name and leftmost arg */
				loc = leftmostLoc(fc->location,
								  exprLocation((Node *) fc->args));
			}
			break;
		case T_A_ArrayExpr:
			/* the location points at ARRAY or [, which must be leftmost */
			loc = ((A_ArrayExpr *) expr)->location;
			break;
		case T_ResTarget:
			/* we need not examine the contained expression (if any) */
			loc = ((ResTarget *) expr)->location;
			break;
		case T_TypeCast:
			{
				TypeCast *tc = (TypeCast *) expr;

				/*
				 * This could represent CAST(), ::, or TypeName 'literal',
				 * so any of the components might be leftmost.
				 */
				loc = exprLocation(tc->arg);
				loc = leftmostLoc(loc, tc->typeName->location);
				loc = leftmostLoc(loc, tc->location);
			}
			break;
		case T_SortBy:
			/* just use argument's location (ignore operator, if any) */
			loc = exprLocation(((SortBy *) expr)->node);
			break;
		case T_TypeName:
			loc = ((TypeName *) expr)->location;
			break;
		case T_XmlSerialize:
			/* XMLSERIALIZE keyword should always be the first thing */
			loc = ((XmlSerialize *) expr)->location;
			break;
		case T_WithClause:
			loc = ((WithClause *) expr)->location;
			break;
		case T_CommonTableExpr:
			loc = ((CommonTableExpr *) expr)->location;
			break;
		case T_PlaceHolderVar:
			/* just use argument's location */
			loc = exprLocation((Node *) ((PlaceHolderVar *) expr)->phexpr);
			break;
		default:
			/* for any other node type it's just unknown... */
			loc = -1;
			break;
	}
	return loc;
}

/*
 * leftmostLoc - support for exprLocation
 *
 * Take the minimum of two parse location values, but ignore unknowns
 */
static int
leftmostLoc(int loc1, int loc2)
{
	if (loc1 < 0)
		return loc2;
	else if (loc2 < 0)
		return loc1;
	else
		return Min(loc1, loc2);
}


/*
 * single_node -
 *	  Returns t if node corresponds to a single-noded expression
 */
bool
single_node(Node *node)
{
	if (IsA(node, Const) ||
		IsA(node, Var) ||
		IsA(node, Param))
		return true;
	else
		return false;
}

/*****************************************************************************
 *		VAR nodes
 *****************************************************************************/

/*
 *		var_is_outer
 *		var_is_inner
 *		var_is_mat
 *		var_is_rel
 *
 *		Returns t iff the var node corresponds to (respectively):
 *		the outer relation in a join
 *		the inner relation of a join
 *		a materialized relation
 *		a base relation (i.e., not an attribute reference, a variable from
 *				some lower join level, or a sort result)
 *		var node is an array reference
 *
 */
bool
var_is_outer(Var *var)
{
	return (bool) (var->varno == OUTER);
}

static bool
var_is_inner(Var *var)
{
	return (bool) (var->varno == INNER);
}

bool
var_is_rel(Var *var)
{
	return (bool)
		!(var_is_inner(var) || var_is_outer(var));
}
