/*-------------------------------------------------------------------------
 *
 * nodeFuncs.c
 *		Various general-purpose manipulations of Node trees
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

#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "nodes/relation.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

static int     leftmostLoc(int loc1, int loc2);
static bool expression_returns_set_walker(Node *node, void *context);


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
		case T_WindowFunc:
			/* function name should always be the first thing */
			loc = ((WindowFunc *) expr)->location;
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
 *	exprType -
 *	  returns the Oid of the type of the expression. (Used for typechecking.)
 */
Oid
exprType(Node *expr)
{
	Oid			type;
	
	if (!expr)
		return InvalidOid;
	
	switch (nodeTag(expr))
	{
		case T_Var:
			type = ((Var *) expr)->vartype;
			break;
		case T_Const:
			type = ((Const *) expr)->consttype;
			break;
		case T_Param:
			type = ((Param *) expr)->paramtype;
			break;
		case T_Aggref:
			type = ((Aggref *) expr)->aggtype;
			break;
		case T_WindowFunc:
			type = ((WindowFunc *) expr)->wintype;
			break;
		case T_ArrayRef:
		{
			ArrayRef   *arrayref = (ArrayRef *) expr;
			
			/* slice and/or store operations yield the array type */
			if (arrayref->reflowerindexpr || arrayref->refassgnexpr)
				type = arrayref->refarraytype;
			else
				type = arrayref->refelemtype;
		}
			break;
		case T_FuncExpr:
			type = ((FuncExpr *) expr)->funcresulttype;
			break;
		case T_OpExpr:
			type = ((OpExpr *) expr)->opresulttype;
			break;
		case T_DistinctExpr:
			type = ((DistinctExpr *) expr)->opresulttype;
			break;
		case T_ScalarArrayOpExpr:
			type = BOOLOID;
			break;
		case T_BoolExpr:
			type = BOOLOID;
			break;
		case T_SubLink:
		{
			SubLink    *sublink = (SubLink *) expr;
			
			if (sublink->subLinkType == EXPR_SUBLINK ||
				sublink->subLinkType == ARRAY_SUBLINK)
			{
				/* get the type of the subselect's first target column */
				Query	   *qtree = (Query *) sublink->subselect;
				TargetEntry *tent;
				
				if (!qtree || !IsA(qtree, Query))
					elog(ERROR, "cannot get type for untransformed sublink");
				tent = (TargetEntry *) linitial(qtree->targetList);
				Assert(IsA(tent, TargetEntry));
				Assert(!tent->resjunk);
				type = exprType((Node *) tent->expr);
				if (sublink->subLinkType == ARRAY_SUBLINK)
				{
					type = get_array_type(type);
					if (!OidIsValid(type))
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_OBJECT),
								 errmsg("could not find array type for data type %s",
										format_type_be(exprType((Node *) tent->expr)))));
				}
			}
			else
			{
				/* for all other sublink types, result is boolean */
				type = BOOLOID;
			}
		}
			break;
		case T_SubPlan:
		{
			/*
			 * Although the parser does not ever deal with already-planned
			 * expression trees, we support SubPlan nodes in this routine
			 * for the convenience of ruleutils.c.
			 */
			SubPlan    *subplan = (SubPlan *) expr;
			
			if (subplan->subLinkType == EXPR_SUBLINK ||
				subplan->subLinkType == ARRAY_SUBLINK)
			{
				/* get the type of the subselect's first target column */
				type = subplan->firstColType;
				if (subplan->subLinkType == ARRAY_SUBLINK)
				{
					type = get_array_type(subplan->firstColType);
					if (!OidIsValid(type))
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_OBJECT),
								 errmsg("could not find array type for data type %s",
										format_type_be(subplan->firstColType))));
				}
			}
			else
			{
				/* for all other subplan types, result is boolean */
				type = BOOLOID;
			}
		}
			break;
		case T_AlternativeSubPlan:
		{
			/* As above, supported for the convenience of ruleutils.c */
			AlternativeSubPlan *asplan = (AlternativeSubPlan *) expr;
			
			/* subplans should all return the same thing */
			type = exprType((Node *) linitial(asplan->subplans));
		}
			break;
		case T_FieldSelect:
			type = ((FieldSelect *) expr)->resulttype;
			break;
		case T_FieldStore:
			type = ((FieldStore *) expr)->resulttype;
			break;
		case T_RelabelType:
			type = ((RelabelType *) expr)->resulttype;
			break;
		case T_CoerceViaIO:
			type = ((CoerceViaIO *) expr)->resulttype;
			break;
		case T_ArrayCoerceExpr:
			type = ((ArrayCoerceExpr *) expr)->resulttype;
			break;
		case T_ConvertRowtypeExpr:
			type = ((ConvertRowtypeExpr *) expr)->resulttype;
			break;
		case T_CaseExpr:
			type = ((CaseExpr *) expr)->casetype;
			break;
		case T_CaseTestExpr:
			type = ((CaseTestExpr *) expr)->typeId;
			break;
		case T_ArrayExpr:
			type = ((ArrayExpr *) expr)->array_typeid;
			break;
		case T_RowExpr:
			type = ((RowExpr *) expr)->row_typeid;
			break;
		case T_TableValueExpr:
			type = ANYTABLEOID;  /* MULTISET values are a special pseudotype */
			break;
		case T_RowCompareExpr:
			type = BOOLOID;
			break;
		case T_CoalesceExpr:
			type = ((CoalesceExpr *) expr)->coalescetype;
			break;
		case T_MinMaxExpr:
			type = ((MinMaxExpr *) expr)->minmaxtype;
			break;
		case T_NullIfExpr:
			type = exprType((Node *) linitial(((NullIfExpr *) expr)->args));
			break;
		case T_NullTest:
			type = BOOLOID;
			break;
		case T_BooleanTest:
			type = BOOLOID;
			break;
		case T_XmlExpr:
			if (((XmlExpr *) expr)->op == IS_DOCUMENT)
				type = BOOLOID;
			else if (((XmlExpr *) expr)->op == IS_XMLSERIALIZE)
				type = TEXTOID;
			else
				type = XMLOID;
			break;
		case T_CoerceToDomain:
			type = ((CoerceToDomain *) expr)->resulttype;
			break;
		case T_CoerceToDomainValue:
			type = ((CoerceToDomainValue *) expr)->typeId;
			break;
		case T_SetToDefault:
			type = ((SetToDefault *) expr)->typeId;
			break;
		case T_CurrentOfExpr:
			type = BOOLOID;
			break;
		case T_GroupingFunc:
			type = INT8OID;
			break;
		case T_Grouping:
			type = INT8OID;
			break;
		case T_GroupId:
			type = INT4OID;
			break;
		case T_PercentileExpr:
			type = ((PercentileExpr *) expr)->perctype;
			break;
		case T_DMLActionExpr:
			type = INT4OID;
			break;
		case T_PartDefaultExpr:
			type = BOOLOID;
			break;
		case T_PartBoundExpr:
			type = ((PartBoundExpr *) expr)->boundType;
			break;
		case T_PartBoundInclusionExpr:
			type = BOOLOID;
			break;
		case T_PartBoundOpenExpr:
			type = BOOLOID;
			break;
		case T_PartListRuleExpr:
			type = ((PartListRuleExpr *) expr)->resulttype;
			break;
		case T_PartListNullTestExpr:
			type = BOOLOID;
			break;
		case T_PlaceHolderVar:
			type = exprType((Node *) ((PlaceHolderVar *) expr)->phexpr);
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(expr));
			type = InvalidOid;	/* keep compiler quiet */
			break;
	}
	return type;
}

/*
 *	exprTypmod -
 *	  returns the type-specific attrmod of the expression, if it can be
 *	  determined.  In most cases, it can't and we return -1.
 */
int32
exprTypmod(Node *expr)
{
	if (!expr)
		return -1;

	switch (nodeTag(expr))
	{
		case T_Var:
			return ((Var *) expr)->vartypmod;
		case T_Const:
			return ((Const *) expr)->consttypmod;
		case T_Param:
			return ((Param *) expr)->paramtypmod;
		case T_ArrayRef:
			/* typmod is the same for array or element */
			return ((ArrayRef *) expr)->reftypmod;
		case T_FuncExpr:
			{
				int32		coercedTypmod;

				/* Be smart about length-coercion functions... */
				if (exprIsLengthCoercion(expr, &coercedTypmod))
					return coercedTypmod;
			}
			break;
		case T_SubLink:
			{
				SubLink    *sublink = (SubLink *) expr;

				if (sublink->subLinkType == EXPR_SUBLINK ||
					sublink->subLinkType == ARRAY_SUBLINK)
				{
					/* get the typmod of the subselect's first target column */
					Query	   *qtree = (Query *) sublink->subselect;
					TargetEntry *tent;

					if (!qtree || !IsA(qtree, Query))
						elog(ERROR, "cannot get type for untransformed sublink");
					tent = (TargetEntry *) linitial(qtree->targetList);
					Assert(IsA(tent, TargetEntry));
					Assert(!tent->resjunk);
					return exprTypmod((Node *) tent->expr);
					/* note we don't need to care if it's an array */
				}
			}
			break;
		case T_FieldSelect:
			return ((FieldSelect *) expr)->resulttypmod;
		case T_RelabelType:
			return ((RelabelType *) expr)->resulttypmod;
		case T_ArrayCoerceExpr:
			return ((ArrayCoerceExpr *) expr)->resulttypmod;
		case T_CaseExpr:
			{
				/*
				 * If all the alternatives agree on type/typmod, return that
				 * typmod, else use -1
				 */
				CaseExpr   *cexpr = (CaseExpr *) expr;
				Oid			casetype = cexpr->casetype;
				int32		typmod;
				ListCell   *arg;

				if (!cexpr->defresult)
					return -1;
				if (exprType((Node *) cexpr->defresult) != casetype)
					return -1;
				typmod = exprTypmod((Node *) cexpr->defresult);
				if (typmod < 0)
					return -1;	/* no point in trying harder */
				foreach(arg, cexpr->args)
				{
					CaseWhen   *w = (CaseWhen *) lfirst(arg);

					Assert(IsA(w, CaseWhen));
					if (exprType((Node *) w->result) != casetype)
						return -1;
					if (exprTypmod((Node *) w->result) != typmod)
						return -1;
				}
				return typmod;
			}
			break;
		case T_CaseTestExpr:
			return ((CaseTestExpr *) expr)->typeMod;
		case T_ArrayExpr:
			{
				/*
				 * If all the elements agree on type/typmod, return that
				 * typmod, else use -1
				 */
				ArrayExpr  *arrayexpr = (ArrayExpr *) expr;
				Oid			commontype;
				int32		typmod;
				ListCell   *elem;

				if (arrayexpr->elements == NIL)
					return -1;
				typmod = exprTypmod((Node *) linitial(arrayexpr->elements));
				if (typmod < 0)
					return -1;	/* no point in trying harder */
				if (arrayexpr->multidims)
					commontype = arrayexpr->array_typeid;
				else
					commontype = arrayexpr->element_typeid;
				foreach(elem, arrayexpr->elements)
				{
					Node	   *e = (Node *) lfirst(elem);

					if (exprType(e) != commontype)
						return -1;
					if (exprTypmod(e) != typmod)
						return -1;
				}
				return typmod;
			}
			break;
		case T_CoalesceExpr:
			{
				/*
				 * If all the alternatives agree on type/typmod, return that
				 * typmod, else use -1
				 */
				CoalesceExpr *cexpr = (CoalesceExpr *) expr;
				Oid			coalescetype = cexpr->coalescetype;
				int32		typmod;
				ListCell   *arg;

				if (exprType((Node *) linitial(cexpr->args)) != coalescetype)
					return -1;
				typmod = exprTypmod((Node *) linitial(cexpr->args));
				if (typmod < 0)
					return -1;	/* no point in trying harder */
				for_each_cell(arg, lnext(list_head(cexpr->args)))
				{
					Node	   *e = (Node *) lfirst(arg);

					if (exprType(e) != coalescetype)
						return -1;
					if (exprTypmod(e) != typmod)
						return -1;
				}
				return typmod;
			}
			break;
		case T_MinMaxExpr:
			{
				/*
				 * If all the alternatives agree on type/typmod, return that
				 * typmod, else use -1
				 */
				MinMaxExpr *mexpr = (MinMaxExpr *) expr;
				Oid			minmaxtype = mexpr->minmaxtype;
				int32		typmod;
				ListCell   *arg;

				if (exprType((Node *) linitial(mexpr->args)) != minmaxtype)
					return -1;
				typmod = exprTypmod((Node *) linitial(mexpr->args));
				if (typmod < 0)
					return -1;	/* no point in trying harder */
				for_each_cell(arg, lnext(list_head(mexpr->args)))
				{
					Node	   *e = (Node *) lfirst(arg);

					if (exprType(e) != minmaxtype)
						return -1;
					if (exprTypmod(e) != typmod)
						return -1;
				}
				return typmod;
			}
			break;
		case T_NullIfExpr:
			{
				NullIfExpr *nexpr = (NullIfExpr *) expr;

				return exprTypmod((Node *) linitial(nexpr->args));
			}
			break;
		case T_CoerceToDomain:
			return ((CoerceToDomain *) expr)->resulttypmod;
		case T_CoerceToDomainValue:
			return ((CoerceToDomainValue *) expr)->typeMod;
		case T_SetToDefault:
			return ((SetToDefault *) expr)->typeMod;
		default:
			break;
	}
	return -1;
}

/*
 * exprIsLengthCoercion
 *		Detect whether an expression tree is an application of a datatype's
 *		typmod-coercion function.  Optionally extract the result's typmod.
 *
 * If coercedTypmod is not NULL, the typmod is stored there if the expression
 * is a length-coercion function, else -1 is stored there.
 *
 * Note that a combined type-and-length coercion will be treated as a
 * length coercion by this routine.
 */
bool
exprIsLengthCoercion(Node *expr, int32 *coercedTypmod)
{
	if (coercedTypmod != NULL)
		*coercedTypmod = -1;	/* default result on failure */

	/*
	 * Scalar-type length coercions are FuncExprs, array-type length coercions
	 * are ArrayCoerceExprs
	 */
	if (expr && IsA(expr, FuncExpr))
	{
		FuncExpr   *func = (FuncExpr *) expr;
		int			nargs;
		Const	   *second_arg;

		/*
		 * If it didn't come from a coercion context, reject.
		 */
		if (func->funcformat != COERCE_EXPLICIT_CAST &&
			func->funcformat != COERCE_IMPLICIT_CAST)
			return false;

		/*
		 * If it's not a two-argument or three-argument function with the
		 * second argument being an int4 constant, it can't have been created
		 * from a length coercion (it must be a type coercion, instead).
		 */
		nargs = list_length(func->args);
		if (nargs < 2 || nargs > 3)
			return false;

		second_arg = (Const *) lsecond(func->args);
		if (!IsA(second_arg, Const) ||
			second_arg->consttype != INT4OID ||
			second_arg->constisnull)
			return false;

		/*
		 * OK, it is indeed a length-coercion function.
		 */
		if (coercedTypmod != NULL)
			*coercedTypmod = DatumGetInt32(second_arg->constvalue);

		return true;
	}

	if (expr && IsA(expr, ArrayCoerceExpr))
	{
		ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *) expr;

		/* It's not a length coercion unless there's a nondefault typmod */
		if (acoerce->resulttypmod < 0)
			return false;

		/*
		 * OK, it is indeed a length-coercion expression.
		 */
		if (coercedTypmod != NULL)
			*coercedTypmod = acoerce->resulttypmod;

		return true;
	}

	return false;
}

/*
 * expression_returns_set
 *	  Test whether an expression returns a set result.
 *
 * Because we use expression_tree_walker(), this can also be applied to
 * whole targetlists; it'll produce TRUE if any one of the tlist items
 * returns a set.
 */
bool
expression_returns_set(Node *clause)
{
	return expression_returns_set_walker(clause, NULL);
}

static bool
expression_returns_set_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) node;
		
		if (expr->funcretset)
			return true;
		/* else fall through to check args */
	}
	if (IsA(node, OpExpr))
	{
		OpExpr	   *expr = (OpExpr *) node;
		
		if (expr->opretset)
			return true;
		/* else fall through to check args */
	}
	
	return expression_tree_walker(node, expression_returns_set_walker,
								  context);
}

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
 * into the "testexpr" subtree (which is an expression belonging to the outer
 * plan).  It will also call the mutator on the sub-Query node; however, when
 * expression_tree_mutator itself is called on a Query node, it does nothing
 * and returns the unmodified Query node.  The net effect is that unless the
 * mutator does something special at a Query node, sub-selects will not be
 * visited or modified; the original sub-select will be linked to by the new
 * SubLink node.  Mutators that want to descend into sub-selects will usually
 * do so by recognizing Query nodes and calling query_tree_mutator (below).
 *
 * expression_tree_mutator will handle a SubPlan node by recursing into the
 * "testexpr" and the "args" list (which belong to the outer plan), but it
 * will simply copy the link to the inner plan, since that's typically what
 * expression tree mutators want.  A mutator that wants to modify the subplan
 * can force appropriate behavior by recognizing SubPlan expression nodes
 * and doing the right thing.
 *--------------------
 */

Node *
expression_tree_mutator(Node *node,
						Node *(*mutator) (),
						void *context)
{
	/*
	 * The mutator has already decided not to modify the current node, but we
	 * must call the mutator for any sub-nodes.
	 */

#define FLATCOPY(newnode, node, nodetype)  \
	( (newnode) = (nodetype *) palloc(sizeof(nodetype)), \
	  memcpy((newnode), (node), sizeof(nodetype)) )

#define CHECKFLATCOPY(newnode, node, nodetype)	\
	( AssertMacro(IsA((node), nodetype)), \
	  (newnode) = (nodetype *) palloc(sizeof(nodetype)), \
	  memcpy((newnode), (node), sizeof(nodetype)) )

#define MUTATE(newfield, oldfield, fieldtype)  \
		( (newfield) = (fieldtype) mutator((Node *) (oldfield), context) )

	if (node == NULL)
		return NULL;

	/* Guard against stack overflow due to overly complex expressions */
	check_stack_depth();

	switch (nodeTag(node))
	{
			/*
			 * Primitive node types with no expression subnodes.  Var and
			 * Const are frequent enough to deserve special cases, the others
			 * we just use copyObject for.
			 */
		case T_Var:
			{
				Var		   *var = (Var *) node;
				Var		   *newnode;

				FLATCOPY(newnode, var, Var);
				return (Node *) newnode;
			}
			break;
		case T_Const:
			{
				Const	   *oldnode = (Const *) node;
				Const	   *newnode;

				FLATCOPY(newnode, oldnode, Const);
				/* XXX we don't bother with datumCopy; should we? */
				return (Node *) newnode;
			}
			break;
		case T_Param:
		case T_CoerceToDomainValue:
		case T_CaseTestExpr:
		case T_DynamicIndexScan:
		case T_SetToDefault:
		case T_CurrentOfExpr:
		case T_RangeTblRef:
		case T_String:
		case T_Null:
		case T_DML:
		case T_RowTrigger:
		case T_PartSelectedExpr:
		case T_PartDefaultExpr:
		case T_PartBoundExpr:
		case T_PartBoundInclusionExpr:
		case T_PartBoundOpenExpr:
		case T_PartListRuleExpr:
		case T_PartListNullTestExpr:
			return (Node *) copyObject(node);
		case T_Aggref:
			{
				Aggref	   *aggref = (Aggref *) node;
				Aggref	   *newnode;

				FLATCOPY(newnode, aggref, Aggref);
				MUTATE(newnode->args, aggref->args, List *);
				MUTATE(newnode->aggorder, aggref->aggorder, AggOrder*);
				MUTATE(newnode->aggfilter, aggref->aggfilter, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_WindowFunc:
			{
				WindowFunc *wfunc = (WindowFunc *) node;
				WindowFunc *newnode;

				FLATCOPY(newnode, wfunc, WindowFunc);
				MUTATE(newnode->args, wfunc->args, List *);
				MUTATE(newnode->aggfilter, wfunc->aggfilter, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_AggOrder:
			{
				AggOrder	*aggorder = (AggOrder *)node;
				AggOrder	*newnode;

				FLATCOPY(newnode, aggorder, AggOrder);
				MUTATE(newnode->sortTargets, aggorder->sortTargets, List *);
				MUTATE(newnode->sortClause, aggorder->sortClause, List *);
				return (Node *) newnode;
			}
			break;
		case T_GroupingFunc:
			{
				GroupingFunc *newnode;

				newnode = copyObject(node);
				return (Node *)newnode;
			}
			break;
		case T_Grouping:
			{
				Grouping *grping = (Grouping *) node;
				Grouping *newnode;

				FLATCOPY(newnode, grping, Grouping);
				return (Node *) newnode;
			}
			break;
		case T_GroupId:
			{
				GroupId *grpid = (GroupId *) node;
				GroupId *newnode;

				FLATCOPY(newnode, grpid, GroupId);
				return (Node *) newnode;
			}
			break;
		case T_TableFunctionScan:
			{
				TableFunctionScan *tablefunc = (TableFunctionScan *) node;
				TableFunctionScan *newnode;

				FLATCOPY(newnode, tablefunc, TableFunctionScan);
				return (Node *) newnode;
			}
			break;
		case T_WindowDef:
			{
				WindowDef *windef = (WindowDef *) node;
				WindowDef *newnode;

				FLATCOPY(newnode, windef, WindowDef);

				MUTATE(newnode->partitionClause, windef->partitionClause, List *);
				MUTATE(newnode->orderClause, windef->orderClause, List *);
				MUTATE(newnode->startOffset, windef->startOffset, Node *);
				MUTATE(newnode->endOffset, windef->endOffset, Node *);

				return (Node *) newnode;

			}
		case T_WindowClause:
			{
				WindowClause *wc = (WindowClause *) node;
				WindowClause *newnode;

				FLATCOPY(newnode, wc, WindowClause);

				MUTATE(newnode->partitionClause, wc->partitionClause, List *);
				MUTATE(newnode->orderClause, wc->orderClause, List *);
				MUTATE(newnode->startOffset, wc->startOffset, Node *);
				MUTATE(newnode->endOffset, wc->endOffset, Node *);

				return (Node *) newnode;

			}
		case T_PercentileExpr:
			{
				PercentileExpr *perc = (PercentileExpr *) node;
				PercentileExpr *newnode;

				FLATCOPY(newnode, perc, PercentileExpr);

				MUTATE(newnode->args, perc->args, List *);
				MUTATE(newnode->sortClause, perc->sortClause, List *);
				MUTATE(newnode->sortTargets, perc->sortTargets, List *);
				MUTATE(newnode->pcExpr, perc->pcExpr, Expr *);
				MUTATE(newnode->tcExpr, perc->tcExpr, Expr *);

				return (Node *) newnode;
			}
		case T_SortGroupClause:
			{
				SortGroupClause *sortcl = (SortGroupClause *) node;
				SortGroupClause *newnode;

				FLATCOPY(newnode, sortcl, SortGroupClause);

				return (Node *) newnode;
			}
		case T_GroupingClause:
			{
				GroupingClause *grpingcl = (GroupingClause *) node;
				GroupingClause *newnode;

				FLATCOPY(newnode, grpingcl, GroupingClause);
				MUTATE(newnode->groupsets, grpingcl->groupsets, List *);
				return (Node *)newnode;
			}
		case T_ArrayRef:
			{
				ArrayRef   *arrayref = (ArrayRef *) node;
				ArrayRef   *newnode;

				FLATCOPY(newnode, arrayref, ArrayRef);
				MUTATE(newnode->refupperindexpr, arrayref->refupperindexpr,
					   List *);
				MUTATE(newnode->reflowerindexpr, arrayref->reflowerindexpr,
					   List *);
				MUTATE(newnode->refexpr, arrayref->refexpr,
					   Expr *);
				MUTATE(newnode->refassgnexpr, arrayref->refassgnexpr,
					   Expr *);
				return (Node *) newnode;
			}
			break;
		case T_FuncExpr:
			{
				FuncExpr   *expr = (FuncExpr *) node;
				FuncExpr   *newnode;

				FLATCOPY(newnode, expr, FuncExpr);
				MUTATE(newnode->args, expr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_TableValueExpr:
			{
				TableValueExpr   *expr = (TableValueExpr *) node;
				TableValueExpr   *newnode;

				FLATCOPY(newnode, expr, TableValueExpr);

				/* The subquery already pulled up into the T_TableFunctionScan node */
				newnode->subquery = (Node *) NULL;
				return (Node *) newnode;
			}
			break;
		case T_OpExpr:
			{
				OpExpr	   *expr = (OpExpr *) node;
				OpExpr	   *newnode;

				FLATCOPY(newnode, expr, OpExpr);
				MUTATE(newnode->args, expr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_DistinctExpr:
			{
				DistinctExpr *expr = (DistinctExpr *) node;
				DistinctExpr *newnode;

				FLATCOPY(newnode, expr, DistinctExpr);
				MUTATE(newnode->args, expr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_ScalarArrayOpExpr:
			{
				ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *) node;
				ScalarArrayOpExpr *newnode;

				FLATCOPY(newnode, expr, ScalarArrayOpExpr);
				MUTATE(newnode->args, expr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_BoolExpr:
			{
				BoolExpr   *expr = (BoolExpr *) node;
				BoolExpr   *newnode;

				FLATCOPY(newnode, expr, BoolExpr);
				MUTATE(newnode->args, expr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_SubLink:
			{
				SubLink    *sublink = (SubLink *) node;
				SubLink    *newnode;

				FLATCOPY(newnode, sublink, SubLink);
				MUTATE(newnode->testexpr, sublink->testexpr, Node *);

				/*
				 * Also invoke the mutator on the sublink's Query node, so it
				 * can recurse into the sub-query if it wants to.
				 */
				MUTATE(newnode->subselect, sublink->subselect, Node *);
				return (Node *) newnode;
			}
			break;
		case T_SubPlan:
			{
				SubPlan    *subplan = (SubPlan *) node;
				SubPlan    *newnode;

				FLATCOPY(newnode, subplan, SubPlan);
				/* transform testexpr */
				MUTATE(newnode->testexpr, subplan->testexpr, Node *);
				/* transform args list (params to be passed to subplan) */
				MUTATE(newnode->args, subplan->args, List *);
				/* but not the sub-Plan itself, which is referenced as-is */
				return (Node *) newnode;
			}
			break;
		case T_AlternativeSubPlan:
			{
				AlternativeSubPlan *asplan = (AlternativeSubPlan *) node;
				AlternativeSubPlan *newnode;

				FLATCOPY(newnode, asplan, AlternativeSubPlan);
				MUTATE(newnode->subplans, asplan->subplans, List *);
				return (Node *) newnode;
			}
			break;
		case T_FieldSelect:
			{
				FieldSelect *fselect = (FieldSelect *) node;
				FieldSelect *newnode;

				FLATCOPY(newnode, fselect, FieldSelect);
				MUTATE(newnode->arg, fselect->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_FieldStore:
			{
				FieldStore *fstore = (FieldStore *) node;
				FieldStore *newnode;

				FLATCOPY(newnode, fstore, FieldStore);
				MUTATE(newnode->arg, fstore->arg, Expr *);
				MUTATE(newnode->newvals, fstore->newvals, List *);
				newnode->fieldnums = list_copy(fstore->fieldnums);
				return (Node *) newnode;
			}
			break;
		case T_RelabelType:
			{
				RelabelType *relabel = (RelabelType *) node;
				RelabelType *newnode;

				FLATCOPY(newnode, relabel, RelabelType);
				MUTATE(newnode->arg, relabel->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_CoerceViaIO:
			{
				CoerceViaIO *iocoerce = (CoerceViaIO *) node;
				CoerceViaIO *newnode;

				FLATCOPY(newnode, iocoerce, CoerceViaIO);
				MUTATE(newnode->arg, iocoerce->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_ArrayCoerceExpr:
			{
				ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *) node;
				ArrayCoerceExpr *newnode;

				FLATCOPY(newnode, acoerce, ArrayCoerceExpr);
				MUTATE(newnode->arg, acoerce->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_ConvertRowtypeExpr:
			{
				ConvertRowtypeExpr *convexpr = (ConvertRowtypeExpr *) node;
				ConvertRowtypeExpr *newnode;

				FLATCOPY(newnode, convexpr, ConvertRowtypeExpr);
				MUTATE(newnode->arg, convexpr->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_CaseExpr:
			{
				CaseExpr   *caseexpr = (CaseExpr *) node;
				CaseExpr   *newnode;

				FLATCOPY(newnode, caseexpr, CaseExpr);
				MUTATE(newnode->arg, caseexpr->arg, Expr *);
				MUTATE(newnode->args, caseexpr->args, List *);
				MUTATE(newnode->defresult, caseexpr->defresult, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_CaseWhen:
			{
				CaseWhen   *casewhen = (CaseWhen *) node;
				CaseWhen   *newnode;

				FLATCOPY(newnode, casewhen, CaseWhen);
				MUTATE(newnode->expr, casewhen->expr, Expr *);
				MUTATE(newnode->result, casewhen->result, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_ArrayExpr:
			{
				ArrayExpr  *arrayexpr = (ArrayExpr *) node;
				ArrayExpr  *newnode;

				FLATCOPY(newnode, arrayexpr, ArrayExpr);
				MUTATE(newnode->elements, arrayexpr->elements, List *);
				return (Node *) newnode;
			}
			break;
		case T_RowExpr:
			{
				RowExpr    *rowexpr = (RowExpr *) node;
				RowExpr    *newnode;

				FLATCOPY(newnode, rowexpr, RowExpr);
				MUTATE(newnode->args, rowexpr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_RowCompareExpr:
			{
				RowCompareExpr *rcexpr = (RowCompareExpr *) node;
				RowCompareExpr *newnode;

				FLATCOPY(newnode, rcexpr, RowCompareExpr);
				MUTATE(newnode->largs, rcexpr->largs, List *);
				MUTATE(newnode->rargs, rcexpr->rargs, List *);
				return (Node *) newnode;
			}
			break;
		case T_CoalesceExpr:
			{
				CoalesceExpr *coalesceexpr = (CoalesceExpr *) node;
				CoalesceExpr *newnode;

				FLATCOPY(newnode, coalesceexpr, CoalesceExpr);
				MUTATE(newnode->args, coalesceexpr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_MinMaxExpr:
			{
				MinMaxExpr *minmaxexpr = (MinMaxExpr *) node;
				MinMaxExpr *newnode;

				FLATCOPY(newnode, minmaxexpr, MinMaxExpr);
				MUTATE(newnode->args, minmaxexpr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_XmlExpr:
			{
				XmlExpr    *xexpr = (XmlExpr *) node;
				XmlExpr    *newnode;

				FLATCOPY(newnode, xexpr, XmlExpr);
				MUTATE(newnode->named_args, xexpr->named_args, List *);
				/* assume mutator does not care about arg_names */
				MUTATE(newnode->args, xexpr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_NullIfExpr:
			{
				NullIfExpr *expr = (NullIfExpr *) node;
				NullIfExpr *newnode;

				FLATCOPY(newnode, expr, NullIfExpr);
				MUTATE(newnode->args, expr->args, List *);
				return (Node *) newnode;
			}
			break;
		case T_NullTest:
			{
				NullTest   *ntest = (NullTest *) node;
				NullTest   *newnode;

				FLATCOPY(newnode, ntest, NullTest);
				MUTATE(newnode->arg, ntest->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_BooleanTest:
			{
				BooleanTest *btest = (BooleanTest *) node;
				BooleanTest *newnode;

				FLATCOPY(newnode, btest, BooleanTest);
				MUTATE(newnode->arg, btest->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_CoerceToDomain:
			{
				CoerceToDomain *ctest = (CoerceToDomain *) node;
				CoerceToDomain *newnode;

				FLATCOPY(newnode, ctest, CoerceToDomain);
				MUTATE(newnode->arg, ctest->arg, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_TargetEntry:
			{
				TargetEntry *targetentry = (TargetEntry *) node;
				TargetEntry *newnode;

				FLATCOPY(newnode, targetentry, TargetEntry);
				MUTATE(newnode->expr, targetentry->expr, Expr *);
				return (Node *) newnode;
			}
			break;
		case T_Query:
			/* Do nothing with a sub-Query, per discussion above */
			return node;
		case T_CommonTableExpr:
			{
				CommonTableExpr *cte = (CommonTableExpr *) node;
				CommonTableExpr *newnode;

				FLATCOPY(newnode, cte, CommonTableExpr);

				/*
				 * Also invoke the mutator on the CTE's Query node, so it can
				 * recurse into the sub-query if it wants to.
				 */
				MUTATE(newnode->ctequery, cte->ctequery, Node *);
				return (Node *) newnode;
			}
			break;
		case T_List:
			{
				/*
				 * We assume the mutator isn't interested in the list nodes
				 * per se, so just invoke it on each list element. NOTE: this
				 * would fail badly on a list with integer elements!
				 */
				List	   *resultlist;
				ListCell   *temp;

				resultlist = NIL;
				foreach(temp, (List *) node)
				{
					resultlist = lappend(resultlist,
										 mutator((Node *) lfirst(temp),
												 context));
				}
				return (Node *) resultlist;
			}
			break;
		case T_FromExpr:
			{
				FromExpr   *from = (FromExpr *) node;
				FromExpr   *newnode;

				FLATCOPY(newnode, from, FromExpr);
				MUTATE(newnode->fromlist, from->fromlist, List *);
				MUTATE(newnode->quals, from->quals, Node *);
				return (Node *) newnode;
			}
			break;
		case T_JoinExpr:
			{
				JoinExpr   *join = (JoinExpr *) node;
				JoinExpr   *newnode;

				FLATCOPY(newnode, join, JoinExpr);
				MUTATE(newnode->larg, join->larg, Node *);
				MUTATE(newnode->rarg, join->rarg, Node *);
				MUTATE(newnode->quals, join->quals, Node *);
				/* We do not mutate alias or using by default */
				return (Node *) newnode;
			}
			break;
		case T_SetOperationStmt:
			{
				SetOperationStmt *setop = (SetOperationStmt *) node;
				SetOperationStmt *newnode;

				FLATCOPY(newnode, setop, SetOperationStmt);
				MUTATE(newnode->larg, setop->larg, Node *);
				MUTATE(newnode->rarg, setop->rarg, Node *);
				/* We do not mutate groupClauses by default */
				return (Node *) newnode;
			}
			break;
		case T_PlaceHolderVar:
			{
				PlaceHolderVar *phv = (PlaceHolderVar *) node;
				PlaceHolderVar *newnode;

				FLATCOPY(newnode, phv, PlaceHolderVar);
				MUTATE(newnode->phexpr, phv->phexpr, Expr *);
				/* Assume we need not copy the relids bitmapset */
				return (Node *) newnode;
			}
			break;
		case T_AppendRelInfo:
			{
				AppendRelInfo *appinfo = (AppendRelInfo *) node;
				AppendRelInfo *newnode;

				FLATCOPY(newnode, appinfo, AppendRelInfo);
				MUTATE(newnode->translated_vars, appinfo->translated_vars, List *);
				return (Node *) newnode;
			}
			break;
		case T_PlaceHolderInfo:
			{
				PlaceHolderInfo *phinfo = (PlaceHolderInfo *) node;
				PlaceHolderInfo *newnode;

				FLATCOPY(newnode, phinfo, PlaceHolderInfo);
				MUTATE(newnode->ph_var, phinfo->ph_var, PlaceHolderVar *);
				/* Assume we need not copy the relids bitmapsets */
				return (Node *) newnode;
			}
			break;
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(node));
			break;
	}
	/* can't get here, but keep compiler happy */
	return NULL;
}


/*
 * query_tree_mutator --- initiate modification of a Query's expressions
 *
 * This routine exists just to reduce the number of places that need to know
 * where all the expression subtrees of a Query are.  Note it can be used
 * for starting a walk at top level of a Query regardless of whether the
 * mutator intends to descend into subqueries.	It is also useful for
 * descending into subqueries within a mutator.
 *
 * Some callers want to suppress mutating of certain items in the Query,
 * typically because they need to process them specially, or don't actually
 * want to recurse into subqueries.  This is supported by the flags argument,
 * which is the bitwise OR of flag values to suppress mutating of
 * indicated items.  (More flag bits may be added as needed.)
 *
 * Normally the Query node itself is copied, but some callers want it to be
 * modified in-place; they must pass QTW_DONT_COPY_QUERY in flags.	All
 * modified substructure is safely copied in any case.
 */
Query *
query_tree_mutator(Query *query,
				   Node *(*mutator) (),
				   void *context,
				   int flags)
{
	Assert(query != NULL && IsA(query, Query));

	if (!(flags & QTW_DONT_COPY_QUERY))
	{
		Query	   *newquery;

		FLATCOPY(newquery, query, Query);
		query = newquery;
	}

	MUTATE(query->targetList, query->targetList, List *);
	MUTATE(query->returningList, query->returningList, List *);
	MUTATE(query->jointree, query->jointree, FromExpr *);
	MUTATE(query->setOperations, query->setOperations, Node *);
	MUTATE(query->groupClause, query->groupClause, List *);
	MUTATE(query->scatterClause, query->scatterClause, List *);
	MUTATE(query->havingQual, query->havingQual, Node *);
	MUTATE(query->windowClause, query->windowClause, List *);
	MUTATE(query->limitOffset, query->limitOffset, Node *);
	MUTATE(query->limitCount, query->limitCount, Node *);
	if (!(flags & QTW_IGNORE_CTE_SUBQUERIES))
		MUTATE(query->cteList, query->cteList, List *);
	else	/* else copy CTE list as-is */
		query->cteList = copyObject(query->cteList);
	query->rtable = range_table_mutator(query->rtable,
										mutator, context, flags);
	return query;
}

/*
 * range_table_mutator is just the part of query_tree_mutator that processes
 * a query's rangetable.  This is split out since it can be useful on
 * its own.
 */
List *
range_table_mutator(List *rtable,
					Node *(*mutator) (),
					void *context,
					int flags)
{
	List	   *newrt = NIL;
	ListCell   *rt;
	
	foreach(rt, rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(rt);
		RangeTblEntry *newrte;
		
		FLATCOPY(newrte, rte, RangeTblEntry);
		switch (rte->rtekind)
		{
			case RTE_RELATION:
			case RTE_SPECIAL:
			case RTE_VOID:
			case RTE_CTE:
				/* we don't bother to copy eref, aliases, etc; OK? */
				break;
			case RTE_SUBQUERY:
				if (!(flags & QTW_IGNORE_RT_SUBQUERIES))
				{
					CHECKFLATCOPY(newrte->subquery, rte->subquery, Query);
					MUTATE(newrte->subquery, newrte->subquery, Query *);
				}
				else
				{
					/* else, copy RT subqueries as-is */
					newrte->subquery = copyObject(rte->subquery);
				}
				break;
			case RTE_JOIN:
				if (!(flags & QTW_IGNORE_JOINALIASES))
					MUTATE(newrte->joinaliasvars, rte->joinaliasvars, List *);
				else
				{
					/* else, copy join aliases as-is */
					newrte->joinaliasvars = copyObject(rte->joinaliasvars);
				}
				break;
			case RTE_FUNCTION:
				MUTATE(newrte->funcexpr, rte->funcexpr, Node *);
				break;
			case RTE_TABLEFUNCTION:
				MUTATE(newrte->funcexpr, rte->funcexpr, Node *);
				MUTATE(newrte->subquery, rte->subquery, Query *);
				break;
			case RTE_VALUES:
				MUTATE(newrte->values_lists, rte->values_lists, List *);
				break;
		}
		newrt = lappend(newrt, newrte);
	}
	return newrt;
}

/*
 * query_or_expression_tree_mutator --- hybrid form
 *
 * This routine will invoke query_tree_mutator if called on a Query node,
 * else will invoke the mutator directly.  This is a useful way of starting
 * the recursion when the mutator's normal change of state is not appropriate
 * for the outermost Query node.
 */
Node *
query_or_expression_tree_mutator(Node *node,
								 Node *(*mutator) (),
								 void *context,
								 int flags)
{
	if (node && IsA(node, Query))
		return (Node *) query_tree_mutator((Query *) node,
										   mutator,
										   context,
										   flags);
	else
		return mutator(node, context);
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
