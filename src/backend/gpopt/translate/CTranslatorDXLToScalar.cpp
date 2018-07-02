//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorDXLToScalar.cpp
//
//	@doc:
//		Implementation of the methods used to translate DXL Scalar Node into
//		GPDB's Expr.
//
//	@test:
//---------------------------------------------------------------------------

#include "postgres.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "nodes/makefuncs.h"
#include "catalog/pg_collation.h"
#include "utils/datum.h"

#include "gpos/base.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDAccessorUtils.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/translate/CTranslatorDXLToScalar.h"
#include "gpopt/translate/CTranslatorDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CMappingColIdVarPlStmt.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDatumInt2.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/operators/CDXLDatumGeneric.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDTypeBool.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpos;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CTranslatorDXLToScalar
//
//	@doc:
//		Constructor
//---------------------------------------------------------------------------
CTranslatorDXLToScalar::CTranslatorDXLToScalar
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	ULONG num_segments
	)
	:
	m_memory_pool(memory_pool),
	m_md_accessor(md_accessor),
	m_has_subqueries(false),
	m_num_of_segments(num_segments)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarExprFromDXL
//
//	@doc:
//		Translates a DXL scalar expression into a GPDB Expression node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarExprFromDXL
	(
	const CDXLNode *dxlnode,
	CMappingColIdVar *col_id_var
	)
{
	static const STranslatorElem translators[] =
	{
		{EdxlopScalarIdent, &CTranslatorDXLToScalar::CreateScalarIdExprFromDXL},
		{EdxlopScalarCmp, &CTranslatorDXLToScalar::CreateScalarCmpExprFromDXL},
		{EdxlopScalarDistinct, &CTranslatorDXLToScalar::CreateScalarDistinctCmpExprFromDXL},
		{EdxlopScalarOpExpr, &CTranslatorDXLToScalar::CreateScalarOpExprFromDXL},
		{EdxlopScalarArrayComp, &CTranslatorDXLToScalar::CreateScalarArrayCompFromDXLNode},
		{EdxlopScalarCoalesce, &CTranslatorDXLToScalar::CreateScalarCoalesceExprFromDXL},
		{EdxlopScalarMinMax, &CTranslatorDXLToScalar::CreateScalarMinMaxExprFromDXL},
		{EdxlopScalarConstValue, &CTranslatorDXLToScalar::CreateScalarConstExprFromDXL},
		{EdxlopScalarBoolExpr, &CTranslatorDXLToScalar::CreateScalarBoolExprFromDXL},
		{EdxlopScalarBooleanTest, &CTranslatorDXLToScalar::CreateScalarBoolTestExprFromDXL},
		{EdxlopScalarNullTest, &CTranslatorDXLToScalar::CreateScalarNULLTestExprFromDXL},
		{EdxlopScalarNullIf, &CTranslatorDXLToScalar::CreateScalarNULLIfExprFromDXL},
		{EdxlopScalarIfStmt, &CTranslatorDXLToScalar::CreateScalarIfStmtExprFromDXL},
		{EdxlopScalarSwitch, &CTranslatorDXLToScalar::CreateScalarSwitchExprFromDXL},
		{EdxlopScalarCaseTest, &CTranslatorDXLToScalar::CreateScalarCaseTestExprFromDXL},
		{EdxlopScalarFuncExpr, &CTranslatorDXLToScalar::CreateScalarFuncExprFromDXL},
		{EdxlopScalarAggref, &CTranslatorDXLToScalar::CreateScalarAggrefExprFromDXL},
		{EdxlopScalarWindowRef, &CTranslatorDXLToScalar::CreateScalarWindowRefExprFromDXL},
		{EdxlopScalarCast, &CTranslatorDXLToScalar::CreateScalarCastExprFromDXL},
		{EdxlopScalarCoerceToDomain, &CTranslatorDXLToScalar::CreateScalarCoerceToDomainExprFromDXL},
		{EdxlopScalarCoerceViaIO, &CTranslatorDXLToScalar::CreateScalarCoerceViaIOExprFromDXL},
		{EdxlopScalarArrayCoerceExpr, &CTranslatorDXLToScalar::CreateScalarArrayCoerceExprFromDXL},
		{EdxlopScalarSubPlan, &CTranslatorDXLToScalar::CreateScalarSubplanExprFromDXL},
		{EdxlopScalarArray, &CTranslatorDXLToScalar::CreateArrayExprFromDXL},
		{EdxlopScalarArrayRef, &CTranslatorDXLToScalar::CreateArrayRefExprFromDXL},
		{EdxlopScalarDMLAction, &CTranslatorDXLToScalar::CreateDMLActionExprFromDXL},
		{EdxlopScalarPartDefault, &CTranslatorDXLToScalar::CreatePartDefaultExprFromDXL},
		{EdxlopScalarPartBound, &CTranslatorDXLToScalar::CreatePartBoundExprFromDXL},
		{EdxlopScalarPartBoundInclusion, &CTranslatorDXLToScalar::CreatePartBoundInclusionExprFromDXL},
		{EdxlopScalarPartBoundOpen, &CTranslatorDXLToScalar::CreatePartBoundOpenExprFromDXL},
		{EdxlopScalarPartListValues, &CTranslatorDXLToScalar::CreatePartListValuesExprFromDXL},
		{EdxlopScalarPartListNullTest, &CTranslatorDXLToScalar::CreatePartListNullTestExprFromDXL},
	};

	const ULONG num_translators = GPOS_ARRAY_SIZE(translators);

	GPOS_ASSERT(NULL != dxlnode);
	Edxlopid eopid = dxlnode->GetOperator()->GetDXLOperator();

	// find translator for the node type
	expr_func_ptr translate_func = NULL;
	for (ULONG ul = 0; ul < num_translators; ul++)
	{
		STranslatorElem elem = translators[ul];
		if (eopid == elem.eopid)
		{
			translate_func = elem.translate_func;
			break;
		}
	}

	if (NULL == translate_func)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, dxlnode->GetOperator()->GetOpNameStr()->GetBuffer());
	}

	return (this->*translate_func)(dxlnode, col_id_var);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarIfStmtExprFromDXL
//
//	@doc:
//		Translates a DXL scalar if stmt into a GPDB CaseExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarIfStmtExprFromDXL
	(
	const CDXLNode *scalar_if_stmt_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_if_stmt_node);
	CDXLScalarIfStmt *scalar_if_stmt_dxl = CDXLScalarIfStmt::Cast(scalar_if_stmt_node->GetOperator());

	CaseExpr *case_expr = MakeNode(CaseExpr);
	case_expr->casetype = CMDIdGPDB::CastMdid(scalar_if_stmt_dxl->GetResultTypeMdId())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	case_expr->casecollid = gpdb::TypeCollation(case_expr->casetype);

	CDXLNode *curr = const_cast<CDXLNode*>(scalar_if_stmt_node);
	Expr *else_expr = NULL;

	// An If statement is of the format: IF <condition> <then> <else>
	// The leaf else statement is the def result of the case statement
	BOOL is_leaf_else_stmt = false;

	while (!is_leaf_else_stmt)
	{

		if (3 != curr->Arity())
		{
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiDXLIncorrectNumberOfChildren
				);
			return NULL;
		}

		Expr *when_expr = CreateScalarExprFromDXL((*curr)[0], col_id_var);
		Expr *then_expr = CreateScalarExprFromDXL((*curr)[1], col_id_var);

		CaseWhen *case_when = MakeNode(CaseWhen);
		case_when->expr = when_expr;
		case_when->result = then_expr;
		case_expr->args = gpdb::LAppend(case_expr->args,case_when);

		if (EdxlopScalarIfStmt == (*curr)[2]->GetOperator()->GetDXLOperator())
		{
			curr = (*curr)[2];
		}
		else
		{
			is_leaf_else_stmt = true;
			else_expr = CreateScalarExprFromDXL((*curr)[2], col_id_var);
		}
	}

	case_expr->defresult = else_expr;

	return (Expr *)case_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarSwitchExprFromDXL
//
//	@doc:
//		Translates a DXL scalar switch into a GPDB CaseExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarSwitchExprFromDXL
	(
	const CDXLNode *scalar_switch_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_switch_node);
	CDXLScalarSwitch *dxlop = CDXLScalarSwitch::Cast(scalar_switch_node->GetOperator());

	CaseExpr *case_expr = MakeNode(CaseExpr);
	case_expr->casetype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	case_expr->casecollid = gpdb::TypeCollation(case_expr->casetype);

	// translate arg child
	case_expr->arg = CreateScalarExprFromDXL((*scalar_switch_node)[0], col_id_var);
	GPOS_ASSERT(NULL != case_expr->arg);

	const ULONG arity = scalar_switch_node->Arity();
	GPOS_ASSERT(1 < arity);
	for (ULONG ul = 1; ul < arity; ul++)
	{
		const CDXLNode *child_dxl = (*scalar_switch_node)[ul];

		if (EdxlopScalarSwitchCase == child_dxl->GetOperator()->GetDXLOperator())
		{
			CaseWhen *case_when = MakeNode(CaseWhen);
			case_when->expr = CreateScalarExprFromDXL((*child_dxl)[0], col_id_var);
			case_when->result = CreateScalarExprFromDXL((*child_dxl)[1], col_id_var);
			case_expr->args = gpdb::LAppend(case_expr->args, case_when);
		}
		else
		{
			// default return value
			GPOS_ASSERT(ul == arity - 1);
			case_expr->defresult = CreateScalarExprFromDXL((*scalar_switch_node)[ul], col_id_var);
		}
	}

	return (Expr *)case_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarCaseTestExprFromDXL
//
//	@doc:
//		Translates a DXL scalar case test into a GPDB CaseTestExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarCaseTestExprFromDXL
	(
	const CDXLNode *scalar_case_test_node,
	CMappingColIdVar * //col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_case_test_node);
	CDXLScalarCaseTest *dxlop = CDXLScalarCaseTest::Cast(scalar_case_test_node->GetOperator());

	CaseTestExpr *casetestexpr = MakeNode(CaseTestExpr);
	casetestexpr->typeId = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	casetestexpr->typeMod = -1;
	// GDPB_91_MERGE_FIXME: collation
	casetestexpr->collation = gpdb::TypeCollation(casetestexpr->typeId);

	return (Expr *)casetestexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarOpExprFromDXL
//
//	@doc:
//		Translates a DXL scalar opexpr into a GPDB OpExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarOpExprFromDXL
	(
	const CDXLNode *scalar_op_expr_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_op_expr_node);
	CDXLScalarOpExpr *scalar_op_expr_dxl = CDXLScalarOpExpr::Cast(scalar_op_expr_node->GetOperator());

	OpExpr *op_expr = MakeNode(OpExpr);
	op_expr->opno = CMDIdGPDB::CastMdid(scalar_op_expr_dxl->MDId())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(scalar_op_expr_dxl->MDId());
	op_expr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();

	IMDId *return_type_mdid = scalar_op_expr_dxl->GetReturnTypeMdId();
	if (NULL != return_type_mdid)
	{
		op_expr->opresulttype = CMDIdGPDB::CastMdid(return_type_mdid)->OidObjectId();
	}
	else 
	{
		op_expr->opresulttype = GetFunctionReturnTypeOid(md_scalar_op->FuncMdId());
	}

	const IMDFunction *md_func = m_md_accessor->RetrieveFunc(md_scalar_op->FuncMdId());
	op_expr->opretset = md_func->ReturnsSet();

	GPOS_ASSERT(1 == scalar_op_expr_node->Arity() || 2 == scalar_op_expr_node->Arity());

	// translate children
	op_expr->args = PlistTranslateScalarChildren(op_expr->args, scalar_op_expr_node, col_id_var);

	// GDPB_91_MERGE_FIXME: collation
	op_expr->inputcollid = gpdb::ExprCollation((Node *) op_expr->args);
	op_expr->opcollid = gpdb::TypeCollation(op_expr->opresulttype);

	return (Expr *)op_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarArrayCompFromDXLNode
//
//	@doc:
//		Translates a CDXLScalarArrayComp into a GPDB ScalarArrayOpExpr
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarArrayCompFromDXLNode
	(
	const CDXLNode *scalar_array_comp_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_array_comp_node);
	CDXLScalarArrayComp *array_comp_dxl = CDXLScalarArrayComp::Cast(scalar_array_comp_node->GetOperator());

	ScalarArrayOpExpr *arrayop_expr = MakeNode(ScalarArrayOpExpr);
	arrayop_expr->opno = CMDIdGPDB::CastMdid(array_comp_dxl->MDId())->OidObjectId();
	const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(array_comp_dxl->MDId());
	arrayop_expr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();

	switch(array_comp_dxl->GetDXLArrayCmpType())
	{
		case Edxlarraycomptypeany:
				arrayop_expr->useOr = true;
				break;

		case Edxlarraycomptypeall:
				arrayop_expr->useOr = false;
				break;

		default:
				GPOS_RAISE
					(
					gpdxl::ExmaDXL,
					gpdxl::ExmiPlStmt2DXLConversion,
					GPOS_WSZ_LIT("Scalar Array Comparison: Specified operator type is invalid")
					);
	}

	// translate left and right child

	GPOS_ASSERT(2 == scalar_array_comp_node->Arity());

	CDXLNode *left_node = (*scalar_array_comp_node)[EdxlsccmpIndexLeft];
	CDXLNode *right_node = (*scalar_array_comp_node)[EdxlsccmpIndexRight];

	Expr *left_expr = CreateScalarExprFromDXL(left_node, col_id_var);
	Expr *right_expr = CreateScalarExprFromDXL(right_node, col_id_var);

	arrayop_expr->args = ListMake2(left_expr, right_expr);
	// GDPB_91_MERGE_FIXME: collation
	arrayop_expr->inputcollid = gpdb::ExprCollation((Node *) arrayop_expr->args);

	return (Expr *)arrayop_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarDistinctCmpExprFromDXL
//
//	@doc:
//		Translates a DXL scalar distinct comparison into a GPDB DistinctExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarDistinctCmpExprFromDXL
	(
	const CDXLNode *scalar_distinct_cmp_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_distinct_cmp_node);
	CDXLScalarDistinctComp *dxlop = CDXLScalarDistinctComp::Cast(scalar_distinct_cmp_node->GetOperator());

	DistinctExpr *dist_expr = MakeNode(DistinctExpr);
	dist_expr->opno = CMDIdGPDB::CastMdid(dxlop->MDId())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(dxlop->MDId());

	dist_expr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
	dist_expr->opresulttype = GetFunctionReturnTypeOid(md_scalar_op->FuncMdId());
	// GDPB_91_MERGE_FIXME: collation
	dist_expr->opcollid = gpdb::TypeCollation(dist_expr->opresulttype);

	// translate left and right child
	GPOS_ASSERT(2 == scalar_distinct_cmp_node->Arity());
	CDXLNode *left_node = (*scalar_distinct_cmp_node)[EdxlscdistcmpIndexLeft];
	CDXLNode *right_node = (*scalar_distinct_cmp_node)[EdxlscdistcmpIndexRight];

	Expr *left_expr = CreateScalarExprFromDXL(left_node, col_id_var);
	Expr *right_expr = CreateScalarExprFromDXL(right_node, col_id_var);

	dist_expr->args = ListMake2(left_expr, right_expr);

	return (Expr *)dist_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarAggrefExprFromDXL
//
//	@doc:
//		Translates a DXL scalar aggref_node into a GPDB Aggref node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarAggrefExprFromDXL
	(
	const CDXLNode *aggref_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != aggref_node);
	CDXLScalarAggref *dxlop = CDXLScalarAggref::Cast(aggref_node->GetOperator());

	Aggref *aggref = MakeNode(Aggref);
	aggref->aggfnoid = CMDIdGPDB::CastMdid(dxlop->GetDXLAggFuncMDid())->OidObjectId();
	aggref->aggdistinct = NIL;
	aggref->agglevelsup = 0;
	aggref->aggkind = 'n';
	aggref->location = -1;

	CMDIdGPDB *agg_mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(aggref->aggfnoid);
	const IMDAggregate *pmdagg = m_md_accessor->RetrieveAgg(agg_mdid);
	agg_mdid->Release();

	EdxlAggrefStage edxlaggstage = dxlop->GetDXLAggStage();
	if (NULL != dxlop->GetDXLResolvedRetTypeMDid())
	{
		// use resolved type
		aggref->aggtype = CMDIdGPDB::CastMdid(dxlop->GetDXLResolvedRetTypeMDid())->OidObjectId();
	}
	else if (EdxlaggstageIntermediate == edxlaggstage || EdxlaggstagePartial == edxlaggstage)
	{
		aggref->aggtype = CMDIdGPDB::CastMdid(pmdagg->GetIntermediateResultTypeMdid())->OidObjectId();
	}
	else
	{
		aggref->aggtype = CMDIdGPDB::CastMdid(pmdagg->GetResultTypeMdid())->OidObjectId();
	}

	switch(dxlop->GetDXLAggStage())
	{
		case EdxlaggstageNormal:
					aggref->aggstage = AGGSTAGE_NORMAL;
					break;
		case EdxlaggstagePartial:
					aggref->aggstage = AGGSTAGE_PARTIAL;
					break;
		case EdxlaggstageIntermediate:
					aggref->aggstage = AGGSTAGE_INTERMEDIATE;
					break;
		case EdxlaggstageFinal:
					aggref->aggstage = AGGSTAGE_FINAL;
					break;
		default:
				GPOS_RAISE
					(
					gpdxl::ExmaDXL,
					gpdxl::ExmiPlStmt2DXLConversion,
					GPOS_WSZ_LIT("AGGREF: Specified AggStage value is invalid")
					);
	}

	// translate each DXL argument
	List *exprs = PlistTranslateScalarChildren(aggref->args, aggref_node, col_id_var);

	int attno;
	aggref->args = NIL;
	ListCell *lc;
	int sortgrpindex = 1;
	ForEachWithCount (lc, exprs, attno)
	{
		TargetEntry *new_target_entry = gpdb::MakeTargetEntry((Expr *) lfirst(lc), attno + 1, NULL, false);
		/*
		 * Translate the aggdistinct bool set to true (in ORCA),
		 * to a List of SortGroupClause in the PLNSTMT
		 */
		if(dxlop->IsDistinct())
		{
			new_target_entry->ressortgroupref = sortgrpindex;
			SortGroupClause *gc = makeNode(SortGroupClause);
			gc->tleSortGroupRef = sortgrpindex;
			gc->eqop = gpdb::GetEqualityOp(gpdb::ExprType((Node*) new_target_entry->expr));
			gc->sortop = gpdb::GetOrderingOpForEqualityOp(gc->eqop, NULL);
			/*
			 * Since ORCA doesn't yet support ordered aggregates, we are
			 * setting nulls_first to false. This is also the default behavior
			 * when no order by clause is provided so it is OK to set it to
			 * false.
			 */
			gc->nulls_first = false;
			aggref->aggdistinct = gpdb::LAppend(aggref->aggdistinct, gc);
			sortgrpindex++;
		}
		aggref->args = gpdb::LAppend(aggref->args, new_target_entry);
	}

	// GDPB_91_MERGE_FIXME: collation
	aggref->inputcollid = gpdb::ExprCollation((Node *) exprs);
	aggref->aggcollid = gpdb::TypeCollation(aggref->aggtype);

	return (Expr *)aggref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarWindowRefExprFromDXL
//
//	@doc:
//		Translate a DXL scalar window ref into a GPDB WindowRef node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarWindowRefExprFromDXL
	(
	const CDXLNode *scalar_winref_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_winref_node);
	CDXLScalarWindowRef *dxlop = CDXLScalarWindowRef::Cast(scalar_winref_node->GetOperator());

	WindowFunc *window_func = MakeNode(WindowFunc);
	window_func->winfnoid = CMDIdGPDB::CastMdid(dxlop->FuncMdId())->OidObjectId();

	window_func->windistinct = dxlop->IsDistinct();
	window_func->location = -1;
	window_func->winref = dxlop->GetWindSpecPos() + 1;
	window_func->wintype = CMDIdGPDB::CastMdid(dxlop->ReturnTypeMdId())->OidObjectId();
	window_func->winstar = dxlop->IsStarArg();
	window_func->winagg = dxlop->IsSimpleAgg();

	EdxlWinStage dxl_win_stage = dxlop->GetDxlWinStage();
	GPOS_ASSERT(dxl_win_stage != EdxlwinstageSentinel);

	ULONG mapping[][2] =
			{
			{WINSTAGE_IMMEDIATE, EdxlwinstageImmediate},
			{WINSTAGE_PRELIMINARY, EdxlwinstagePreliminary},
			{WINSTAGE_ROWKEY, EdxlwinstageRowKey},
			};

	const ULONG arity = GPOS_ARRAY_SIZE(mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *elem = mapping[ul];
		if ((ULONG) dxl_win_stage == elem[1])
		{
			window_func->winstage = (WinStage) elem[0];
			break;
		}
	}

	// translate the arguments of the window function
	window_func->args = PlistTranslateScalarChildren(window_func->args, scalar_winref_node, col_id_var);
	// GDPB_91_MERGE_FIXME: collation
	window_func->wincollid = gpdb::TypeCollation(window_func->wintype);
	window_func->inputcollid = gpdb::ExprCollation((Node *) window_func->args);

	return (Expr *) window_func;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarFuncExprFromDXL
//
//	@doc:
//		Translates a DXL scalar opexpr into a GPDB FuncExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarFuncExprFromDXL
	(
	const CDXLNode *scalar_func_expr_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_func_expr_node);
	CDXLScalarFuncExpr *dxlop = CDXLScalarFuncExpr::Cast(scalar_func_expr_node->GetOperator());

	FuncExpr *func_expr = MakeNode(FuncExpr);
	func_expr->funcid = CMDIdGPDB::CastMdid(dxlop->FuncMdId())->OidObjectId();
	func_expr->funcretset = dxlop->ReturnsSet();
	func_expr->funcformat = COERCE_DONTCARE;
	func_expr->funcresulttype = CMDIdGPDB::CastMdid(dxlop->ReturnTypeMdId())->OidObjectId();
	func_expr->args = PlistTranslateScalarChildren(func_expr->args, scalar_func_expr_node, col_id_var);

	// GDPB_91_MERGE_FIXME: collation
	func_expr->inputcollid = gpdb::ExprCollation((Node *) func_expr->args);
	func_expr->funccollid = gpdb::TypeCollation(func_expr->funcresulttype);

	return (Expr *)func_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarSubplanExprFromDXL
//
//	@doc:
//		Translates a DXL scalar SubPlan into a GPDB SubPlan node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarSubplanExprFromDXL
	(
	const CDXLNode *scalar_sub_plan_node,
	CMappingColIdVar *col_id_var
	)
{
	CDXLTranslateContext *output_context = (dynamic_cast<CMappingColIdVarPlStmt*>(col_id_var))->GetOutputContext();

	CContextDXLToPlStmt *dxl_to_plstmt_ctxt = (dynamic_cast<CMappingColIdVarPlStmt*>(col_id_var))->GetDXLToPlStmtContext();

	CDXLScalarSubPlan *dxlop = CDXLScalarSubPlan::Cast(scalar_sub_plan_node->GetOperator());

	// translate subplan test expression
	List *param_ids = NIL;

	SubLinkType slink = CTranslatorUtils::MapDXLSubplanToSublinkType(dxlop->GetDxlSubplanType());
	Expr *test_expr = CreateSubplanTestExprFromDXL(dxlop->GetDxlTestExpr(), slink, col_id_var, &param_ids);

	const DrgPdxlcr *outer_refs= dxlop->GetDxlOuterColRefsArray();

	const ULONG len = outer_refs->Size();

	// create a copy of the translate context: the param mappings from the outer scope get copied in the constructor
	CDXLTranslateContext sub_plan_translate_ctxt(m_memory_pool, output_context->IsParentAggNode(), output_context->GetColIdToParamIdMap());

	// insert new outer ref mappings in the subplan translate context
	for (ULONG ul = 0; ul < len; ul++)
	{
		CDXLColRef *dxl_colref = (*outer_refs)[ul];
		IMDId *mdid = dxl_colref->MDIdType();
		ULONG col_id = dxl_colref->Id();
		INT type_modifier = dxl_colref->TypeModifier();

		if (NULL == sub_plan_translate_ctxt.GetParamIdMappingElement(col_id))
		{
			// keep outer reference mapping to the original column for subsequent subplans
			CMappingElementColIdParamId *col_id_to_param_id_map = GPOS_NEW(m_memory_pool) CMappingElementColIdParamId(col_id, dxl_to_plstmt_ctxt->GetNextParamId(), mdid, type_modifier);

#ifdef GPOS_DEBUG
			BOOL is_inserted =
#endif
			sub_plan_translate_ctxt.FInsertParamMapping(col_id, col_id_to_param_id_map);
			GPOS_ASSERT(is_inserted);
		}
	}

	CDXLNode *child_dxl = (*scalar_sub_plan_node)[0];
        GPOS_ASSERT(EdxloptypePhysical == child_dxl->GetOperator()->GetDXLOperatorType());

	GPOS_ASSERT(NULL != scalar_sub_plan_node);
	GPOS_ASSERT(EdxlopScalarSubPlan == scalar_sub_plan_node->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(1 == scalar_sub_plan_node->Arity());

	// generate the child plan,
	// create DXL->PlStmt translator to handle subplan's relational children
	CTranslatorDXLToPlStmt dxl_to_plstmt_translator
							(
							m_memory_pool,
							m_md_accessor,
							(dynamic_cast<CMappingColIdVarPlStmt*>(col_id_var))->GetDXLToPlStmtContext(),
							m_num_of_segments
							);
	DXLTranslationContextArr *prev_siblings_ctxt_arr = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	Plan *plan_child = dxl_to_plstmt_translator.TranslateDXLOperatorToPlan(child_dxl, &sub_plan_translate_ctxt, prev_siblings_ctxt_arr);
	prev_siblings_ctxt_arr->Release();

	GPOS_ASSERT(NULL != plan_child->targetlist && 1 <= gpdb::ListLength(plan_child->targetlist));

	// translate subplan and set test expression
	SubPlan *sub_plan = CreateSubplanFromChildPlan(plan_child, slink, dxl_to_plstmt_ctxt);
	sub_plan->testexpr = (Node *) test_expr;
	sub_plan->paramIds = param_ids;

	// translate other subplan params
	TranslateSubplanParams(sub_plan, &sub_plan_translate_ctxt, outer_refs, col_id_var);

	return (Expr *)sub_plan;
}

inline BOOL FDXLCastedId(CDXLNode *dxlnode)
{
	return EdxlopScalarCast == dxlnode->GetOperator()->GetDXLOperator() &&
		   dxlnode->Arity() > 0 && EdxlopScalarIdent == (*dxlnode)[0]->GetOperator()->GetDXLOperator();
}

inline CTranslatorDXLToScalar::STypeOidAndTypeModifier OidParamOidFromDXLIdentOrDXLCastIdent(CDXLNode *ident_or_cast_ident_node)
{
	GPOS_ASSERT(EdxlopScalarIdent == ident_or_cast_ident_node->GetOperator()->GetDXLOperator() || FDXLCastedId(ident_or_cast_ident_node));

	CDXLScalarIdent *inner_ident;
	if (EdxlopScalarIdent == ident_or_cast_ident_node->GetOperator()->GetDXLOperator())
	{
		inner_ident = CDXLScalarIdent::Cast(ident_or_cast_ident_node->GetOperator());
	}
	else
	{
		inner_ident = CDXLScalarIdent::Cast((*ident_or_cast_ident_node)[0]->GetOperator());
	}
	Oid inner_type_oid = CMDIdGPDB::CastMdid(inner_ident->MDIdType())->OidObjectId();
	INT type_modifier = inner_ident->TypeModifier();
	return {inner_type_oid, type_modifier};
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::CreateSubplanTestExprFromDXL
//
//      @doc:
//              Translate subplan test expression
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateSubplanTestExprFromDXL
	(
	CDXLNode *test_expr_node,
	SubLinkType slink,
	CMappingColIdVar *col_id_var,
	List **param_ids
	)
{
	if (EXPR_SUBLINK == slink || EXISTS_SUBLINK == slink || NOT_EXISTS_SUBLINK == slink)
	{
		// expr/exists/not-exists sublinks have no test expression
		return NULL;
	}
	GPOS_ASSERT(NULL != test_expr_node);

	if (HasConstTrue(test_expr_node, m_md_accessor))
	{
		// dummy test expression
		return (Expr *) CreateScalarConstExprFromDXL(test_expr_node, NULL);
	}

	if (EdxlopScalarCmp != test_expr_node->GetOperator()->GetDXLOperator())
	{
		// test expression is expected to be a comparison
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,  GPOS_WSZ_LIT("Unexpected subplan test expression"));
	}

	GPOS_ASSERT(2 == test_expr_node->Arity());
	GPOS_ASSERT(ANY_SUBLINK == slink || ALL_SUBLINK == slink);

	CDXLNode *outer_child_node = (*test_expr_node)[0];
	CDXLNode *inner_child_node = (*test_expr_node)[1];

	if (EdxlopScalarIdent != inner_child_node->GetOperator()->GetDXLOperator() && !FDXLCastedId(inner_child_node))
	{
		// test expression is expected to be a comparison between an outer expression 
		// and a scalar identifier from subplan child
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,  GPOS_WSZ_LIT("Unexpected subplan test expression"));
	}

	// extract type of inner column
        CDXLScalarComp *scalar_cmp_dxl = CDXLScalarComp::Cast(test_expr_node->GetOperator());

		// create an OpExpr for subplan test expression
        OpExpr *op_expr = MakeNode(OpExpr);
        op_expr->opno = CMDIdGPDB::CastMdid(scalar_cmp_dxl->MDId())->OidObjectId();
        const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(scalar_cmp_dxl->MDId());
        op_expr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
        op_expr->opresulttype = CMDIdGPDB::CastMdid(m_md_accessor->PtMDType<IMDTypeBool>()->MDId())->OidObjectId();
        op_expr->opretset = false;

        // translate outer expression (can be a deep scalar tree)
        Expr *outer_arg_expr = CreateScalarExprFromDXL(outer_child_node, col_id_var);

        // add translated outer expression as first arg of OpExpr
        List *args = NIL;
        args = gpdb::LAppend(args, outer_arg_expr);

	// second arg must be an EXEC param which is replaced during query execution with subplan output
	Param *param = MakeNode(Param);
	param->paramkind = PARAM_EXEC;
	CContextDXLToPlStmt *dxl_to_plstmt_ctxt = (dynamic_cast<CMappingColIdVarPlStmt *>(col_id_var))->GetDXLToPlStmtContext();
	param->paramid = dxl_to_plstmt_ctxt->GetNextParamId();
	CTranslatorDXLToScalar::STypeOidAndTypeModifier oidAndTypeModifier = OidParamOidFromDXLIdentOrDXLCastIdent(inner_child_node);
	param->paramtype = oidAndTypeModifier.oid_type;
	param->paramtypmod = oidAndTypeModifier.type_modifier;

	// test expression is used for non-scalar subplan,
	// second arg of test expression must be an EXEC param referring to subplan output,
	// we add this param to subplan param ids before translating other params

	*param_ids = gpdb::LAppendInt(*param_ids, param->paramid);

	if (EdxlopScalarIdent == inner_child_node->GetOperator()->GetDXLOperator())
	{
		args = gpdb::LAppend(args, param);
	}
	else // we have a cast
	{
		CDXLScalarCast *scalar_cast = CDXLScalarCast::Cast(inner_child_node->GetOperator());
		Expr *pexprCastParam = CreateRelabelTypeOrFuncExprFromDXL(scalar_cast, (Expr *) param);
		args = gpdb::LAppend(args, pexprCastParam);
	}
	op_expr->args = args;

	return (Expr *) op_expr;
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::TranslateSubplanParams
//
//      @doc:
//              Translate subplan parameters
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToScalar::TranslateSubplanParams
	(
	SubPlan *sub_plan,
	CDXLTranslateContext *dxl_translator_ctxt,
	const DrgPdxlcr *outer_refs,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != sub_plan);
	GPOS_ASSERT(NULL != dxl_translator_ctxt);
	GPOS_ASSERT(NULL != outer_refs);
	GPOS_ASSERT(NULL != col_id_var);

	// Create the PARAM and ARG nodes
	const ULONG size = outer_refs->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CDXLColRef *dxl_colref = (*outer_refs)[ul];
		dxl_colref->AddRef();
		const CMappingElementColIdParamId *col_id_to_param_id_map = dxl_translator_ctxt->GetParamIdMappingElement(dxl_colref->Id());

		// TODO: eliminate param, it's not *really* used, and it's (short-term) leaked
		Param *param = CreateParamFromMapping(col_id_to_param_id_map);
		sub_plan->parParam = gpdb::LAppendInt(sub_plan->parParam, param->paramid);

		GPOS_ASSERT(col_id_to_param_id_map->MDIdType()->Equals(dxl_colref->MDIdType()));

		CDXLScalarIdent *scalar_ident_dxl = GPOS_NEW(m_memory_pool) CDXLScalarIdent(m_memory_pool, dxl_colref);
		Expr *arg = (Expr *) col_id_var->VarFromDXLNodeScId(scalar_ident_dxl);

		// not found in mapping, it must be an external parameter
		if (NULL == arg)
		{
			arg = (Expr*) CreateParamFromMapping(col_id_to_param_id_map);
			GPOS_ASSERT(NULL != arg);
		}

		scalar_ident_dxl->Release();
		sub_plan->args = gpdb::LAppend(sub_plan->args, arg);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateSubplanFromChildPlan
//
//	@doc:
//		add child plan to translation context, and build a subplan from it
//
//---------------------------------------------------------------------------
SubPlan *
CTranslatorDXLToScalar::CreateSubplanFromChildPlan
	(
	Plan *plan,
	SubLinkType slink,
	CContextDXLToPlStmt *dxl_to_plstmt_ctxt
	)
{
	dxl_to_plstmt_ctxt->AddSubplan(plan);

	SubPlan *sub_plan = MakeNode(SubPlan);
	sub_plan->plan_id = gpdb::ListLength(dxl_to_plstmt_ctxt->GetSubplanEntriesList());
	sub_plan->plan_name = GetSubplanAlias(sub_plan->plan_id);
	sub_plan->is_initplan = false;
	sub_plan->firstColType = gpdb::ExprType( (Node*) ((TargetEntry*) gpdb::ListNth(plan->targetlist, 0))->expr);
	// GDPB_91_MERGE_FIXME: collation
	sub_plan->firstColCollation = gpdb::TypeCollation(sub_plan->firstColType);
	sub_plan->firstColTypmod = -1;
	sub_plan->subLinkType = slink;
	sub_plan->is_multirow = false;
	sub_plan->unknownEqFalse = false;

	return sub_plan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::GetSubplanAlias
//
//	@doc:
//		build plan name, for explain purposes
//
//---------------------------------------------------------------------------
CHAR *
CTranslatorDXLToScalar::GetSubplanAlias
	(
	ULONG plan_id
	)
{
	CWStringDynamic *wcstr = GPOS_NEW(m_memory_pool) CWStringDynamic(m_memory_pool);
	wcstr->AppendFormat(GPOS_WSZ_LIT("SubPlan %d"), plan_id);
	const WCHAR *wchar_str = wcstr->GetBuffer();

	ULONG max_length = (GPOS_WSZ_LENGTH(wchar_str) + 1) * GPOS_SIZEOF(WCHAR);
	CHAR *str = (CHAR *) gpdb::GPDBAlloc(max_length);
	gpos::clib::Wcstombs(str, const_cast<WCHAR *>(wchar_str), max_length);
	str[max_length - 1] = '\0';
	GPOS_DELETE(wcstr);

	return str;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateParamFromMapping
//
//	@doc:
//		Creates a GPDB param from the given mapping
//
//---------------------------------------------------------------------------
Param *
CTranslatorDXLToScalar::CreateParamFromMapping
	(
	const CMappingElementColIdParamId *col_id_to_param_id_map
	)
{
	Param *param = MakeNode(Param);
	param->paramid = col_id_to_param_id_map->ParamId();
	param->paramkind = PARAM_EXEC;
	param->paramtype = CMDIdGPDB::CastMdid(col_id_to_param_id_map->MDIdType())->OidObjectId();
	param->paramtypmod = col_id_to_param_id_map->TypeModifier();
	// GDPB_91_MERGE_FIXME: collation
	param->paramcollid = gpdb::TypeCollation(param->paramtype);

	return param;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarBoolExprFromDXL
//
//	@doc:
//		Translates a DXL scalar BoolExpr into a GPDB OpExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarBoolExprFromDXL
	(
	const CDXLNode *scalar_bool_expr_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_bool_expr_node);
	CDXLScalarBoolExpr *dxlop = CDXLScalarBoolExpr::Cast(scalar_bool_expr_node->GetOperator());
	BoolExpr *scalar_bool_expr = MakeNode(BoolExpr);

	GPOS_ASSERT(1 <= scalar_bool_expr_node->Arity());
	switch (dxlop->GetDxlBoolTypeStr())
	{
		case Edxlnot:
		{
			GPOS_ASSERT(1 == scalar_bool_expr_node->Arity());
			scalar_bool_expr->boolop = NOT_EXPR;
			break;
		}
		case Edxland:
		{
			GPOS_ASSERT(2 <= scalar_bool_expr_node->Arity());
			scalar_bool_expr->boolop = AND_EXPR;
			break;
		}
		case Edxlor:
		{
			GPOS_ASSERT(2 <= scalar_bool_expr_node->Arity());
			scalar_bool_expr->boolop = OR_EXPR;
			break;
		}
		default:
		{
			GPOS_ASSERT(!"Boolean Operation: Must be either or/ and / not");
			return NULL;
		}
	}

	scalar_bool_expr->args = PlistTranslateScalarChildren(scalar_bool_expr->args, scalar_bool_expr_node, col_id_var);
	scalar_bool_expr->location = -1;

	return (Expr *)scalar_bool_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarBoolTestExprFromDXL
//
//	@doc:
//		Translates a DXL scalar BooleanTest into a GPDB OpExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarBoolTestExprFromDXL
	(
	const CDXLNode *scalar_boolean_test_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_boolean_test_node);
	CDXLScalarBooleanTest *dxlop = CDXLScalarBooleanTest::Cast(scalar_boolean_test_node->GetOperator());
	BooleanTest *scalar_boolean_test = MakeNode(BooleanTest);

	switch (dxlop->GetDxlBoolTypeStr())
	{
		case EdxlbooleantestIsTrue:
				scalar_boolean_test->booltesttype = IS_TRUE;
				break;
		case EdxlbooleantestIsNotTrue:
				scalar_boolean_test->booltesttype = IS_NOT_TRUE;
				break;
		case EdxlbooleantestIsFalse:
				scalar_boolean_test->booltesttype = IS_FALSE;
				break;
		case EdxlbooleantestIsNotFalse:
				scalar_boolean_test->booltesttype = IS_NOT_FALSE;
				break;
		case EdxlbooleantestIsUnknown:
				scalar_boolean_test->booltesttype = IS_UNKNOWN;
				break;
		case EdxlbooleantestIsNotUnknown:
				scalar_boolean_test->booltesttype = IS_NOT_UNKNOWN;
				break;
		default:
				{
				GPOS_ASSERT(!"Invalid Boolean Test Operation");
				return NULL;
				}
	}

	GPOS_ASSERT(1 == scalar_boolean_test_node->Arity());
	CDXLNode *dxlnode_arg = (*scalar_boolean_test_node)[0];

	Expr *arg_expr = CreateScalarExprFromDXL(dxlnode_arg, col_id_var);
	scalar_boolean_test->arg = arg_expr;

	return (Expr *)scalar_boolean_test;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarNULLTestExprFromDXL
//
//	@doc:
//		Translates a DXL scalar NullTest into a GPDB NullTest node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarNULLTestExprFromDXL
	(
	const CDXLNode *scalar_null_test_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_null_test_node);
	CDXLScalarNullTest *dxlop = CDXLScalarNullTest::Cast(scalar_null_test_node->GetOperator());
	NullTest *null_test = MakeNode(NullTest);

	GPOS_ASSERT(1 == scalar_null_test_node->Arity());
	CDXLNode *child_dxl = (*scalar_null_test_node)[0];
	Expr *child_expr = CreateScalarExprFromDXL(child_dxl, col_id_var);

	if (dxlop->IsNullTest())
	{
		null_test->nulltesttype = IS_NULL;
	}
	else
	{
		null_test->nulltesttype = IS_NOT_NULL;
	}

	null_test->arg = child_expr;
	return (Expr *)null_test;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarNULLIfExprFromDXL
//
//	@doc:
//		Translates a DXL scalar nullif into a GPDB NullIfExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarNULLIfExprFromDXL
	(
	const CDXLNode *scalar_null_if_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_null_if_node);
	CDXLScalarNullIf *dxlop = CDXLScalarNullIf::Cast(scalar_null_if_node->GetOperator());

	NullIfExpr *scalar_null_if_expr = MakeNode(NullIfExpr);
	scalar_null_if_expr->opno = CMDIdGPDB::CastMdid(dxlop->MdIdOp())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(dxlop->MdIdOp());

	scalar_null_if_expr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
	scalar_null_if_expr->opresulttype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	scalar_null_if_expr->opretset = false;

	// translate children
	GPOS_ASSERT(2 == scalar_null_if_node->Arity());
	scalar_null_if_expr->args = PlistTranslateScalarChildren(scalar_null_if_expr->args, scalar_null_if_node, col_id_var);
	// GDPB_91_MERGE_FIXME: collation
	scalar_null_if_expr->opcollid = gpdb::TypeCollation(scalar_null_if_expr->opresulttype);
	scalar_null_if_expr->inputcollid = gpdb::ExprCollation((Node *) scalar_null_if_expr->args);

	return (Expr *) scalar_null_if_expr;
}

Expr *
CTranslatorDXLToScalar::CreateRelabelTypeOrFuncExprFromDXL(const CDXLScalarCast *scalar_cast, Expr *child_expr)
{
	if (IMDId::IsValid(scalar_cast->FuncMdId()))
	{
		FuncExpr *func_expr = MakeNode(FuncExpr);
		func_expr->funcid = CMDIdGPDB::CastMdid(scalar_cast->FuncMdId())->OidObjectId();

		const IMDFunction *pmdfunc = m_md_accessor->RetrieveFunc(scalar_cast->FuncMdId());
		func_expr->funcretset = pmdfunc->ReturnsSet();;

		func_expr->funcformat = COERCE_IMPLICIT_CAST;
		func_expr->funcresulttype = CMDIdGPDB::CastMdid(scalar_cast->MDIdType())->OidObjectId();

		func_expr->args = NIL;
		func_expr->args = gpdb::LAppend(func_expr->args, child_expr);

		// GDPB_91_MERGE_FIXME: collation
		func_expr->inputcollid = gpdb::ExprCollation((Node *) func_expr->args);
		func_expr->funccollid = gpdb::TypeCollation(func_expr->funcresulttype);

		return (Expr *) func_expr;
	}

	RelabelType *relabel_type = MakeNode(RelabelType);

	relabel_type->resulttype = CMDIdGPDB::CastMdid(scalar_cast->MDIdType())->OidObjectId();
	relabel_type->arg = child_expr;
	relabel_type->resulttypmod = -1;
	relabel_type->location = -1;
	relabel_type->relabelformat = COERCE_DONTCARE;
	// GDPB_91_MERGE_FIXME: collation
	relabel_type->resultcollid = gpdb::TypeCollation(relabel_type->resulttype);

	return (Expr *) relabel_type;
}

// Translates a DXL scalar cast into a GPDB RelabelType / FuncExpr node
Expr *
CTranslatorDXLToScalar::CreateScalarCastExprFromDXL
	(
	const CDXLNode *scalar_cast_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_cast_node);
	const CDXLScalarCast *dxlop = CDXLScalarCast::Cast(scalar_cast_node->GetOperator());

	GPOS_ASSERT(1 == scalar_cast_node->Arity());
	CDXLNode *child_dxl = (*scalar_cast_node)[0];

	Expr *child_expr = CreateScalarExprFromDXL(child_dxl, col_id_var);

	return CreateRelabelTypeOrFuncExprFromDXL(dxlop, child_expr);
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::CreateScalarCoerceToDomainExprFromDXL
//
//      @doc:
//              Translates a DXL scalar coerce into a GPDB coercetodomain node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarCoerceToDomainExprFromDXL
	(
	const CDXLNode *scalar_coerce_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_coerce_node);
	CDXLScalarCoerceToDomain *dxlop = CDXLScalarCoerceToDomain::Cast(scalar_coerce_node->GetOperator());

	GPOS_ASSERT(1 == scalar_coerce_node->Arity());
	CDXLNode *child_dxl = (*scalar_coerce_node)[0];
	Expr *child_expr = CreateScalarExprFromDXL(child_dxl, col_id_var);

	CoerceToDomain *coerce = MakeNode(CoerceToDomain);

	coerce->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	coerce->arg = child_expr;
	coerce->resulttypmod = dxlop->TypeModifier();
	coerce->location = dxlop->GetLocation();
	coerce->coercionformat = (CoercionForm)  dxlop->GetDXLCoercionForm();
	// GDPB_91_MERGE_FIXME: collation
	coerce->resultcollid = gpdb::TypeCollation(coerce->resulttype);

	return (Expr *) coerce;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::CreateScalarCoerceViaIOExprFromDXL
//
//      @doc:
//              Translates a DXL scalar coerce into a GPDB coerceviaio node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarCoerceViaIOExprFromDXL
	(
	const CDXLNode *scalar_coerce_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_coerce_node);
	CDXLScalarCoerceViaIO *dxlop = CDXLScalarCoerceViaIO::Cast(scalar_coerce_node->GetOperator());

	GPOS_ASSERT(1 == scalar_coerce_node->Arity());
	CDXLNode *child_dxl = (*scalar_coerce_node)[0];
	Expr *child_expr = CreateScalarExprFromDXL(child_dxl, col_id_var);

	CoerceViaIO *coerce = MakeNode(CoerceViaIO);

	coerce->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	coerce->arg = child_expr;
	coerce->location = dxlop->GetLocation();
	coerce->coerceformat = (CoercionForm)  dxlop->GetDXLCoercionForm();
	// GDPB_91_MERGE_FIXME: collation
	coerce->resultcollid = gpdb::TypeCollation(coerce->resulttype);

  return (Expr *) coerce;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::CreateScalarArrayCoerceExprFromDXL
//
//      @doc:
//              Translates a DXL scalar array coerce expr into a GPDB T_ArrayCoerceExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarArrayCoerceExprFromDXL
	(
	const CDXLNode *scalar_coerce_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_coerce_node);
	CDXLScalarArrayCoerceExpr *dxlop = CDXLScalarArrayCoerceExpr::Cast(scalar_coerce_node->GetOperator());

	GPOS_ASSERT(1 == scalar_coerce_node->Arity());
	CDXLNode *child_dxl = (*scalar_coerce_node)[0];

	Expr *child_expr = CreateScalarExprFromDXL(child_dxl, col_id_var);

	ArrayCoerceExpr *coerce = MakeNode(ArrayCoerceExpr);

	coerce->arg = child_expr;
	coerce->elemfuncid = CMDIdGPDB::CastMdid(dxlop->GetCoerceFuncMDid())->OidObjectId();
	coerce->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	coerce->resulttypmod = dxlop->TypeModifier();
	// GDPB_91_MERGE_FIXME: collation
	coerce->resultcollid = gpdb::TypeCollation(coerce->resulttype);
	coerce->isExplicit = dxlop->IsExplicit();
	coerce->coerceformat = (CoercionForm)  dxlop->GetDXLCoercionForm();
	coerce->location = dxlop->GetLocation();

	return (Expr *) coerce;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarCoalesceExprFromDXL
//
//	@doc:
//		Translates a DXL scalar coalesce operator into a GPDB coalesce node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarCoalesceExprFromDXL
	(
	const CDXLNode *scalar_coalesce_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_coalesce_node);
	CDXLScalarCoalesce *dxlop = CDXLScalarCoalesce::Cast(scalar_coalesce_node->GetOperator());
	CoalesceExpr *coalesce = MakeNode(CoalesceExpr);

	coalesce->coalescetype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	coalesce->coalescecollid = gpdb::TypeCollation(coalesce->coalescetype);
	coalesce->args = PlistTranslateScalarChildren(coalesce->args, scalar_coalesce_node, col_id_var);
	coalesce->location = -1;

	return (Expr *) coalesce;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarMinMaxExprFromDXL
//
//	@doc:
//		Translates a DXL scalar minmax operator into a GPDB minmax node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarMinMaxExprFromDXL
	(
	const CDXLNode *scalar_min_max_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_min_max_node);
	CDXLScalarMinMax *dxlop = CDXLScalarMinMax::Cast(scalar_min_max_node->GetOperator());
	MinMaxExpr *min_max_expr = MakeNode(MinMaxExpr);

	min_max_expr->minmaxtype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	min_max_expr->minmaxcollid = gpdb::TypeCollation(min_max_expr->minmaxtype);
	min_max_expr->args = PlistTranslateScalarChildren(min_max_expr->args, scalar_min_max_node, col_id_var);
	// GDPB_91_MERGE_FIXME: collation
	min_max_expr->inputcollid = gpdb::ExprCollation((Node *) min_max_expr->args);
	min_max_expr->location = -1;

	CDXLScalarMinMax::EdxlMinMaxType min_max_type = dxlop->GetMinMaxType();
	if (CDXLScalarMinMax::EmmtMax == min_max_type)
	{
		min_max_expr->op = IS_GREATEST;
	}
	else
	{
		GPOS_ASSERT(CDXLScalarMinMax::EmmtMin == min_max_type);
		min_max_expr->op = IS_LEAST;
	}

	return (Expr *) min_max_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PlistTranslateScalarChildren
//
//	@doc:
//		Translate children of DXL node, and add them to list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToScalar::PlistTranslateScalarChildren
	(
	List *list,
	const CDXLNode *dxlnode,
	CMappingColIdVar *col_id_var
	)
{
	List *new_list = list;

	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *child_dxl = (*dxlnode)[ul];
		Expr *child_expr = CreateScalarExprFromDXL(child_dxl, col_id_var);
		new_list = gpdb::LAppend(new_list, child_expr);
	}

	return new_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarConstExprFromDXL
//
//	@doc:
//		Translates a DXL scalar constant operator into a GPDB scalar const node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarConstExprFromDXL
	(
	const CDXLNode *const_node,
	CMappingColIdVar * //col_id_var
	)
{
	GPOS_ASSERT(NULL != const_node);
	CDXLScalarConstValue *dxlop = CDXLScalarConstValue::Cast(const_node->GetOperator());
	CDXLDatum *datum_dxl = const_cast<CDXLDatum*>(dxlop->GetDatumVal());

	return CreateConstExprFromDXL(datum_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateConstExprFromDXL
//
//	@doc:
//		Translates a DXL datum into a GPDB scalar const node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateConstExprFromDXL
	(
	CDXLDatum *datum_dxl
	)
{
	GPOS_ASSERT(NULL != datum_dxl);
	static const SDatumTranslatorElem translators[] =
		{
			{CDXLDatum::EdxldatumInt2 , &CTranslatorDXLToScalar::ConvertDXLDatumToConstInt2},
			{CDXLDatum::EdxldatumInt4 , &CTranslatorDXLToScalar::ConvertDXLDatumToConstInt4},
			{CDXLDatum::EdxldatumInt8 , &CTranslatorDXLToScalar::ConvertDXLDatumToConstInt8},
			{CDXLDatum::EdxldatumBool , &CTranslatorDXLToScalar::ConvertDXLDatumToConstBool},
			{CDXLDatum::EdxldatumOid , &CTranslatorDXLToScalar::ConvertDXLDatumToConstOid},
			{CDXLDatum::EdxldatumGeneric, &CTranslatorDXLToScalar::CreateConstGenericExprFromDXL},
			{CDXLDatum::EdxldatumStatsDoubleMappable, &CTranslatorDXLToScalar::CreateConstGenericExprFromDXL},
			{CDXLDatum::EdxldatumStatsLintMappable, &CTranslatorDXLToScalar::CreateConstGenericExprFromDXL}
		};

	const ULONG num_translators = GPOS_ARRAY_SIZE(translators);
	CDXLDatum::EdxldatumType edxldatumtype = datum_dxl->GetDatumType();

	// find translator for the node type
	const_func_ptr translate_func = NULL;
	for (ULONG ul = 0; ul < num_translators; ul++)
	{
		SDatumTranslatorElem elem = translators[ul];
		if (edxldatumtype == elem.edxldt)
		{
			translate_func = elem.translate_func;
			break;
		}
	}

	if (NULL == translate_func)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, CDXLTokens::GetDXLTokenStr(EdxltokenScalarConstValue)->GetBuffer());
	}

	return (Expr*) (this->*translate_func)(datum_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::ConvertDXLDatumToConstOid
//
//	@doc:
//		Translates an oid datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::ConvertDXLDatumToConstOid
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumOid *oid_datum_dxl = CDXLDatumOid::Cast(datum_dxl);

	Const *constant = MakeNode(Const);
	constant->consttype = CMDIdGPDB::CastMdid(oid_datum_dxl->MDId())->OidObjectId();
	constant->consttypmod = -1;
	constant->constcollid = InvalidOid;
	constant->constbyval = oid_datum_dxl->IsPassedByValue();
	constant->constisnull = oid_datum_dxl->IsNull();
	constant->constlen = oid_datum_dxl->Length();

	if (constant->constisnull)
	{
		constant->constvalue = (Datum) 0;
	}
	else
	{
		constant->constvalue = gpdb::DatumFromInt32(oid_datum_dxl->OidValue());
	}

	return constant;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::ConvertDXLDatumToConstInt2
//
//	@doc:
//		Translates an int2 datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::ConvertDXLDatumToConstInt2
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumInt2 *datum_int2_dxl = CDXLDatumInt2::Cast(datum_dxl);

	Const *constant = MakeNode(Const);
	constant->consttype = CMDIdGPDB::CastMdid(datum_int2_dxl->MDId())->OidObjectId();
	constant->consttypmod = -1;
	constant->constcollid = InvalidOid;
	constant->constbyval = datum_int2_dxl->IsPassedByValue();
	constant->constisnull = datum_int2_dxl->IsNull();
	constant->constlen = datum_int2_dxl->Length();

	if (constant->constisnull)
	{
		constant->constvalue = (Datum) 0;
	}
	else
	{
		constant->constvalue = gpdb::DatumFromInt16(datum_int2_dxl->Value());
	}

	return constant;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::ConvertDXLDatumToConstInt4
//
//	@doc:
//		Translates an int4 datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::ConvertDXLDatumToConstInt4
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumInt4 *datum_int4_dxl = CDXLDatumInt4::Cast(datum_dxl);

	Const *constant = MakeNode(Const);
	constant->consttype = CMDIdGPDB::CastMdid(datum_int4_dxl->MDId())->OidObjectId();
	constant->consttypmod = -1;
	constant->constcollid = InvalidOid;
	constant->constbyval = datum_int4_dxl->IsPassedByValue();
	constant->constisnull = datum_int4_dxl->IsNull();
	constant->constlen = datum_int4_dxl->Length();

	if (constant->constisnull)
	{
		constant->constvalue = (Datum) 0;
	}
	else
	{
		constant->constvalue = gpdb::DatumFromInt32(datum_int4_dxl->Value());
	}

	return constant;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::ConvertDXLDatumToConstInt8
//
//	@doc:
//		Translates an int8 datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::ConvertDXLDatumToConstInt8
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumInt8 *datum_int8_dxl = CDXLDatumInt8::Cast(datum_dxl);

	Const *constant = MakeNode(Const);
	constant->consttype = CMDIdGPDB::CastMdid(datum_int8_dxl->MDId())->OidObjectId();
	constant->consttypmod = -1;
	constant->constcollid = InvalidOid;
	constant->constbyval = datum_int8_dxl->IsPassedByValue();
	constant->constisnull = datum_int8_dxl->IsNull();
	constant->constlen = datum_int8_dxl->Length();

	if (constant->constisnull)
	{
		constant->constvalue = (Datum) 0;
	}
	else
	{
		constant->constvalue = gpdb::DatumFromInt64(datum_int8_dxl->Value());
	}

	return constant;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::ConvertDXLDatumToConstBool
//
//	@doc:
//		Translates a boolean datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::ConvertDXLDatumToConstBool
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumBool *datum_bool_dxl = CDXLDatumBool::Cast(datum_dxl);

	Const *constant = MakeNode(Const);
	constant->consttype = CMDIdGPDB::CastMdid(datum_bool_dxl->MDId())->OidObjectId();
	constant->consttypmod = -1;
	constant->constcollid = InvalidOid;
	constant->constbyval = datum_bool_dxl->IsPassedByValue();
	constant->constisnull = datum_bool_dxl->IsNull();
	constant->constlen = datum_bool_dxl->Length();

	if (constant->constisnull)
	{
		constant->constvalue = (Datum) 0;
	}
	else
	{
		constant->constvalue = gpdb::DatumFromBool(datum_bool_dxl->GetValue());
	}


	return constant;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateConstGenericExprFromDXL
//
//	@doc:
//		Translates a datum of generic type into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::CreateConstGenericExprFromDXL
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumGeneric *datum_generic_dxl = CDXLDatumGeneric::Cast(datum_dxl);

	Const *constant = MakeNode(Const);
	constant->consttype = CMDIdGPDB::CastMdid(datum_generic_dxl->MDId())->OidObjectId();
	constant->consttypmod = datum_generic_dxl->TypeModifier();
	// GDPB_91_MERGE_FIXME: collation
	constant->constcollid = gpdb::TypeCollation(constant->consttype);
	constant->constbyval = datum_generic_dxl->IsPassedByValue();
	constant->constisnull = datum_generic_dxl->IsNull();
	constant->constlen = datum_generic_dxl->Length();

	if (constant->constisnull)
	{
		constant->constvalue = (Datum) 0;
	}
	else if (constant->constbyval)
	{
		// if it is a by-value constant, the value is stored in the datum.
		GPOS_ASSERT(constant->constlen >= 0);
		GPOS_ASSERT((ULONG) constant->constlen <= sizeof(Datum));
		memcpy(&constant->constvalue, datum_generic_dxl->GetByteArray(), sizeof(Datum));
	}
	else
	{
		Datum val = gpdb::DatumFromPointer(datum_generic_dxl->GetByteArray());
		ULONG length = (ULONG) gpdb::DatumSize(val, false, constant->constlen);

		CHAR *str = (CHAR *) gpdb::GPDBAlloc(length + 1);
		memcpy(str, datum_generic_dxl->GetByteArray(), length);
		str[length] = '\0';
		constant->constvalue = gpdb::DatumFromPointer(str);
	}

	return constant;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreatePartDefaultExprFromDXL
//
//	@doc:
//		Translates a DXL part default into a GPDB part default
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreatePartDefaultExprFromDXL
	(
	const CDXLNode *part_default_node,
	CMappingColIdVar * //col_id_var
	)
{
	CDXLScalarPartDefault *dxlop = CDXLScalarPartDefault::Cast(part_default_node->GetOperator());

	PartDefaultExpr *expr = MakeNode(PartDefaultExpr);
	expr->level = dxlop->GetPartitioningLevel();

	return (Expr *) expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreatePartBoundExprFromDXL
//
//	@doc:
//		Translates a DXL part bound into a GPDB part bound
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreatePartBoundExprFromDXL
	(
	const CDXLNode *part_bound_node,
	CMappingColIdVar * //col_id_var
	)
{
	CDXLScalarPartBound *dxlop = CDXLScalarPartBound::Cast(part_bound_node->GetOperator());

	PartBoundExpr *expr = MakeNode(PartBoundExpr);
	expr->level = dxlop->GetPartitioningLevel();
	expr->boundType = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	expr->isLowerBound = dxlop->IsLowerBound();

	return (Expr *) expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreatePartBoundInclusionExprFromDXL
//
//	@doc:
//		Translates a DXL part bound inclusion into a GPDB part bound inclusion
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreatePartBoundInclusionExprFromDXL
	(
	const CDXLNode *part_bound_incl_node,
	CMappingColIdVar * //col_id_var
	)
{
	CDXLScalarPartBoundInclusion *dxlop = CDXLScalarPartBoundInclusion::Cast(part_bound_incl_node->GetOperator());

	PartBoundInclusionExpr *expr = MakeNode(PartBoundInclusionExpr);
	expr->level = dxlop->GetPartitioningLevel();
	expr->isLowerBound = dxlop->IsLowerBound();

	return (Expr *) expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreatePartBoundOpenExprFromDXL
//
//	@doc:
//		Translates a DXL part bound openness into a GPDB part bound openness
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreatePartBoundOpenExprFromDXL
	(
	const CDXLNode *part_bound_open_node,
	CMappingColIdVar * //col_id_var
	)
{
	CDXLScalarPartBoundOpen *dxlop = CDXLScalarPartBoundOpen::Cast(part_bound_open_node->GetOperator());

	PartBoundOpenExpr *expr = MakeNode(PartBoundOpenExpr);
	expr->level = dxlop->GetPartitioningLevel();
	expr->isLowerBound = dxlop->IsLowerBound();

	return (Expr *) expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreatePartListValuesExprFromDXL
//
//	@doc:
//		Translates a DXL part list values into a GPDB part list values
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreatePartListValuesExprFromDXL
	(
	const CDXLNode *part_list_values_node,
	CMappingColIdVar * //col_id_var
	)
{
	CDXLScalarPartListValues *dxlop = CDXLScalarPartListValues::Cast(part_list_values_node->GetOperator());

	PartListRuleExpr *expr = MakeNode(PartListRuleExpr);
	expr->level = dxlop->GetPartitioningLevel();
	expr->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	expr->elementtype = CMDIdGPDB::CastMdid(dxlop->GetElemTypeMdId())->OidObjectId();

	return (Expr *) expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreatePartListNullTestExprFromDXL
//
//	@doc:
//		Translates a DXL part list values into a GPDB part list null test
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreatePartListNullTestExprFromDXL
	(
	const CDXLNode *part_list_null_test_node,
	CMappingColIdVar * //col_id_var
	)
{
	CDXLScalarPartListNullTest *dxlop = CDXLScalarPartListNullTest::Cast(part_list_null_test_node->GetOperator());

	PartListNullTestExpr *expr = MakeNode(PartListNullTestExpr);
	expr->level = dxlop->GetPartitioningLevel();
	expr->nulltesttype = dxlop->IsNull() ? IS_NULL : IS_NOT_NULL;

	return (Expr *) expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarIdExprFromDXL
//
//	@doc:
//		Translates a DXL scalar ident into a GPDB Expr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarIdExprFromDXL
	(
	const CDXLNode *scalar_id_node,
	CMappingColIdVar *col_id_var
	)
{
	CMappingColIdVarPlStmt *col_id_var_plstmt_map = dynamic_cast<CMappingColIdVarPlStmt*>(col_id_var);

	// scalar identifier
	CDXLScalarIdent *dxlop = CDXLScalarIdent::Cast(scalar_id_node->GetOperator());
	Expr *result_expr = NULL;
	if (NULL == col_id_var_plstmt_map || NULL == col_id_var_plstmt_map->GetOutputContext()->GetParamIdMappingElement(dxlop->MakeDXLColRef()->Id()))
	{
		// not an outer ref -> create var node
		result_expr = (Expr *) col_id_var->VarFromDXLNodeScId(dxlop);
	}
	else
	{
		// outer ref -> create param node
		result_expr = (Expr *) col_id_var_plstmt_map->ParamFromDXLNodeScId(dxlop);
	}

	if (NULL  == result_expr)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, dxlop->MakeDXLColRef()->Id());
	}
	return result_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateScalarCmpExprFromDXL
//
//	@doc:
//		Translates a DXL scalar comparison operator or a scalar distinct comparison into a GPDB OpExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateScalarCmpExprFromDXL
	(
	const CDXLNode *scalar_cmp_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_cmp_node);
	CDXLScalarComp *dxlop = CDXLScalarComp::Cast(scalar_cmp_node->GetOperator());

	OpExpr *op_expr = MakeNode(OpExpr);
	op_expr->opno = CMDIdGPDB::CastMdid(dxlop->MDId())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(dxlop->MDId());

	op_expr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
	op_expr->opresulttype = CMDIdGPDB::CastMdid(m_md_accessor->PtMDType<IMDTypeBool>()->MDId())->OidObjectId();
	op_expr->opretset = false;

	// translate left and right child
	GPOS_ASSERT(2 == scalar_cmp_node->Arity());

	CDXLNode *left_node = (*scalar_cmp_node)[EdxlsccmpIndexLeft];
	CDXLNode *right_node = (*scalar_cmp_node)[EdxlsccmpIndexRight];

	Expr *left_expr = CreateScalarExprFromDXL(left_node, col_id_var);
	Expr *right_expr = CreateScalarExprFromDXL(right_node, col_id_var);

	op_expr->args = ListMake2(left_expr, right_expr);

	// GDPB_91_MERGE_FIXME: collation
	op_expr->inputcollid = gpdb::ExprCollation((Node *) op_expr->args);
	op_expr->opcollid = gpdb::TypeCollation(op_expr->opresulttype);;

	return (Expr *) op_expr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateArrayExprFromDXL
//
//	@doc:
//		Translates a DXL scalar array into a GPDB ArrayExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateArrayExprFromDXL
	(
	const CDXLNode *scalar_array_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_array_node);
	CDXLScalarArray *dxlop = CDXLScalarArray::Cast(scalar_array_node->GetOperator());

	ArrayExpr *expr = MakeNode(ArrayExpr);
	expr->element_typeid = CMDIdGPDB::CastMdid(dxlop->ElementTypeMDid())->OidObjectId();
	expr->array_typeid = CMDIdGPDB::CastMdid(dxlop->ArrayTypeMDid())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	expr->array_collid = gpdb::TypeCollation(expr->array_typeid);
	expr->multidims = dxlop->IsMultiDimensional();
	expr->elements = PlistTranslateScalarChildren(expr->elements, scalar_array_node, col_id_var);
	expr->elements = PlistTranslateScalarChildren(expr->elements, scalar_array_node, col_id_var);

	/*
	 * ORCA doesn't know how to construct array constants, so it will
	 * return any arrays as ArrayExprs. Convert them to array constants,
	 * for more efficient evaluation at runtime. (This will try to further
	 * simplify the elements, too, but that is most likely futile, as the
	 * expressions were already simplified as much as we could before they
	 * were passed to ORCA. But there's little harm in trying).
	 */
	return (Expr *) gpdb::EvalConstExpressions((Node *) expr);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateArrayRefExprFromDXL
//
//	@doc:
//		Translates a DXL scalar arrayref into a GPDB ArrayRef node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateArrayRefExprFromDXL
	(
	const CDXLNode *scalar_array_ref_node,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != scalar_array_ref_node);
	CDXLScalarArrayRef *dxlop = CDXLScalarArrayRef::Cast(scalar_array_ref_node->GetOperator());

	ArrayRef *arrayref = MakeNode(ArrayRef);
	arrayref->refarraytype = CMDIdGPDB::CastMdid(dxlop->ArrayTypeMDid())->OidObjectId();
	arrayref->refelemtype = CMDIdGPDB::CastMdid(dxlop->ElementTypeMDid())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	arrayref->refcollid = gpdb::TypeCollation(arrayref->refelemtype);
	arrayref->reftypmod = dxlop->TypeModifier();

	const ULONG arity = scalar_array_ref_node->Arity();
	GPOS_ASSERT(3 == arity || 4 == arity);

	arrayref->reflowerindexpr = CreateArrayRefIndexListExprFromDXL((*scalar_array_ref_node)[0], CDXLScalarArrayRefIndexList::EilbLower, col_id_var);
	arrayref->refupperindexpr = CreateArrayRefIndexListExprFromDXL((*scalar_array_ref_node)[1], CDXLScalarArrayRefIndexList::EilbUpper, col_id_var);

	arrayref->refexpr = CreateScalarExprFromDXL((*scalar_array_ref_node)[2], col_id_var);
	arrayref->refassgnexpr = NULL;
	if (4 == arity)
	{
		arrayref->refassgnexpr = CreateScalarExprFromDXL((*scalar_array_ref_node)[3], col_id_var);
	}

	return (Expr *) arrayref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateArrayRefIndexListExprFromDXL
//
//	@doc:
//		Translates a DXL arrayref index list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToScalar::CreateArrayRefIndexListExprFromDXL
	(
	const CDXLNode *index_list_node,
	CDXLScalarArrayRefIndexList::EIndexListBound
#ifdef GPOS_DEBUG
	index_list_bound
#endif //GPOS_DEBUG
	,
	CMappingColIdVar *col_id_var
	)
{
	GPOS_ASSERT(NULL != index_list_node);
	GPOS_ASSERT(index_list_bound == CDXLScalarArrayRefIndexList::Cast(index_list_node->GetOperator())->GetDXLIndexListBound());

	List *children = NIL;
	children = PlistTranslateScalarChildren(children, index_list_node, col_id_var);

	return children;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::CreateDMLActionExprFromDXL
//
//	@doc:
//		Translates a DML action expression 
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::CreateDMLActionExprFromDXL
	(
	const CDXLNode *
#ifdef GPOS_DEBUG
	dml_action_node
#endif
	,
	CMappingColIdVar * // col_id_var
	)
{
	GPOS_ASSERT(NULL != dml_action_node);
	GPOS_ASSERT(EdxlopScalarDMLAction == dml_action_node->GetOperator()->GetDXLOperator());

	DMLActionExpr *expr = MakeNode(DMLActionExpr);

	return (Expr *) expr;
}



//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::GetFunctionReturnTypeOid
//
//	@doc:
//		Returns the operator return type oid for the operator funcid from the translation context
//
//---------------------------------------------------------------------------
Oid
CTranslatorDXLToScalar::GetFunctionReturnTypeOid
	(
	IMDId *mdid
	)
	const
{
	return CMDIdGPDB::CastMdid(m_md_accessor->RetrieveFunc(mdid)->GetResultTypeMdid())->OidObjectId();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::HasBoolResult
//
//	@doc:
//		Check to see if the operator returns a boolean result
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToScalar::HasBoolResult
	(
	CDXLNode *dxlnode,
	CMDAccessor *md_accessor
	)
{
	GPOS_ASSERT(NULL != dxlnode);

	if(EdxloptypeScalar != dxlnode->GetOperator()->GetDXLOperatorType())
	{
		return false;
	}

	CDXLScalar *dxlop = dynamic_cast<CDXLScalar*>(dxlnode->GetOperator());

	return dxlop->HasBoolResult(md_accessor);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::HasConstTrue
//
//	@doc:
//		Check if the operator is a "true" bool constant
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToScalar::HasConstTrue
	(
	CDXLNode *dxlnode,
	CMDAccessor *md_accessor
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	if (!HasBoolResult(dxlnode, md_accessor) || EdxlopScalarConstValue != dxlnode->GetOperator()->GetDXLOperator())
	{
		return false;
	}

	CDXLScalarConstValue *dxlop = CDXLScalarConstValue::Cast(dxlnode->GetOperator());
	CDXLDatumBool *datum_bool_dxl = CDXLDatumBool::Cast(const_cast<CDXLDatum *>(dxlop->GetDatumVal()));

	return datum_bool_dxl->GetValue();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::HasConstNull
//
//	@doc:
//		Check if the operator is a NULL constant
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToScalar::HasConstNull
	(
	CDXLNode *dxlnode
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	if (EdxlopScalarConstValue != dxlnode->GetOperator()->GetDXLOperator())
	{
		return false;
	}

	CDXLScalarConstValue *dxlop = CDXLScalarConstValue::Cast(dxlnode->GetOperator());

	return dxlop->GetDatumVal()->IsNull();
}

// EOF
