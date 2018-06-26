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
	ULONG ulSegments
	)
	:
	m_memory_pool(memory_pool),
	m_md_accessor(md_accessor),
	m_fHasSubqueries(false),
	m_num_of_segments(ulSegments)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprFromDXLNodeScalar
//
//	@doc:
//		Translates a DXL scalar expression into a GPDB Expression node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprFromDXLNodeScalar
	(
	const CDXLNode *dxlnode,
	CMappingColIdVar *pmapcidvar
	)
{
	static const STranslatorElem rgTranslators[] =
	{
		{EdxlopScalarIdent, &CTranslatorDXLToScalar::PexprFromDXLNodeScId},
		{EdxlopScalarCmp, &CTranslatorDXLToScalar::PopexprFromDXLNodeScCmp},
		{EdxlopScalarDistinct, &CTranslatorDXLToScalar::PdistexprFromDXLNodeScDistinctComp},
		{EdxlopScalarOpExpr, &CTranslatorDXLToScalar::PopexprFromDXLNodeScOpExpr},
		{EdxlopScalarArrayComp, &CTranslatorDXLToScalar::PstrarrayopexprFromDXLNodeScArrayComp},
		{EdxlopScalarCoalesce, &CTranslatorDXLToScalar::PcoalesceFromDXLNodeScCoalesce},
		{EdxlopScalarMinMax, &CTranslatorDXLToScalar::PminmaxFromDXLNodeScMinMax},
		{EdxlopScalarConstValue, &CTranslatorDXLToScalar::PconstFromDXLNodeScConst},
		{EdxlopScalarBoolExpr, &CTranslatorDXLToScalar::PboolexprFromDXLNodeScBoolExpr},
		{EdxlopScalarBooleanTest, &CTranslatorDXLToScalar::PbooleantestFromDXLNodeScBooleanTest},
		{EdxlopScalarNullTest, &CTranslatorDXLToScalar::PnulltestFromDXLNodeScNullTest},
		{EdxlopScalarNullIf, &CTranslatorDXLToScalar::PnullifFromDXLNodeScNullIf},
		{EdxlopScalarIfStmt, &CTranslatorDXLToScalar::PcaseexprFromDXLNodeScIfStmt},
		{EdxlopScalarSwitch, &CTranslatorDXLToScalar::PcaseexprFromDXLNodeScSwitch},
		{EdxlopScalarCaseTest, &CTranslatorDXLToScalar::PcasetestexprFromDXLNodeScCaseTest},
		{EdxlopScalarFuncExpr, &CTranslatorDXLToScalar::PfuncexprFromDXLNodeScFuncExpr},
		{EdxlopScalarAggref, &CTranslatorDXLToScalar::PaggrefFromDXLNodeScAggref},
		{EdxlopScalarWindowRef, &CTranslatorDXLToScalar::PwindowrefFromDXLNodeScWindowRef},
		{EdxlopScalarCast, &CTranslatorDXLToScalar::PrelabeltypeFromDXLNodeScCast},
		{EdxlopScalarCoerceToDomain, &CTranslatorDXLToScalar::PcoerceFromDXLNodeScCoerceToDomain},
		{EdxlopScalarCoerceViaIO, &CTranslatorDXLToScalar::PcoerceFromDXLNodeScCoerceViaIO},
		{EdxlopScalarArrayCoerceExpr, &CTranslatorDXLToScalar::PcoerceFromDXLNodeScArrayCoerceExpr},
		{EdxlopScalarSubPlan, &CTranslatorDXLToScalar::PsubplanFromDXLNodeScSubPlan},
		{EdxlopScalarArray, &CTranslatorDXLToScalar::PexprArray},
		{EdxlopScalarArrayRef, &CTranslatorDXLToScalar::PexprArrayRef},
		{EdxlopScalarDMLAction, &CTranslatorDXLToScalar::PexprDMLAction},
		{EdxlopScalarPartDefault, &CTranslatorDXLToScalar::PexprPartDefault},
		{EdxlopScalarPartBound, &CTranslatorDXLToScalar::PexprPartBound},
		{EdxlopScalarPartBoundInclusion, &CTranslatorDXLToScalar::PexprPartBoundInclusion},
		{EdxlopScalarPartBoundOpen, &CTranslatorDXLToScalar::PexprPartBoundOpen},
		{EdxlopScalarPartListValues, &CTranslatorDXLToScalar::PexprPartListValues},
		{EdxlopScalarPartListNullTest, &CTranslatorDXLToScalar::PexprPartListNullTest},
	};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);

	GPOS_ASSERT(NULL != dxlnode);
	Edxlopid eopid = dxlnode->GetOperator()->GetDXLOperator();

	// find translator for the node type
	PfPexpr pf = NULL;
	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		STranslatorElem elem = rgTranslators[ul];
		if (eopid == elem.eopid)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, dxlnode->GetOperator()->GetOpNameStr()->GetBuffer());
	}

	return (this->*pf)(dxlnode, pmapcidvar);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PcaseexprFromDXLNodeScIfStmt
//
//	@doc:
//		Translates a DXL scalar if stmt into a GPDB CaseExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcaseexprFromDXLNodeScIfStmt
	(
	const CDXLNode *pdxlnIfStmt,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnIfStmt);
	CDXLScalarIfStmt *pdxlopIfStmt = CDXLScalarIfStmt::Cast(pdxlnIfStmt->GetOperator());

	CaseExpr *pcaseexpr = MakeNode(CaseExpr);
	pcaseexpr->casetype = CMDIdGPDB::CastMdid(pdxlopIfStmt->GetResultTypeMdId())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	pcaseexpr->casecollid = gpdb::OidTypeCollation(pcaseexpr->casetype);

	CDXLNode *pdxlnCurr = const_cast<CDXLNode*>(pdxlnIfStmt);
	Expr *pexprElse = NULL;

	// An If statement is of the format: IF <condition> <then> <else>
	// The leaf else statement is the def result of the case statement
	BOOL fLeafElseStatement = false;

	while (!fLeafElseStatement)
	{

		if (3 != pdxlnCurr->Arity())
		{
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiDXLIncorrectNumberOfChildren
				);
			return NULL;
		}

		Expr *pexprWhen = PexprFromDXLNodeScalar((*pdxlnCurr)[0], pmapcidvar);
		Expr *pexprThen = PexprFromDXLNodeScalar((*pdxlnCurr)[1], pmapcidvar);

		CaseWhen *pwhen = MakeNode(CaseWhen);
		pwhen->expr = pexprWhen;
		pwhen->result = pexprThen;
		pcaseexpr->args = gpdb::PlAppendElement(pcaseexpr->args,pwhen);

		if (EdxlopScalarIfStmt == (*pdxlnCurr)[2]->GetOperator()->GetDXLOperator())
		{
			pdxlnCurr = (*pdxlnCurr)[2];
		}
		else
		{
			fLeafElseStatement = true;
			pexprElse = PexprFromDXLNodeScalar((*pdxlnCurr)[2], pmapcidvar);
		}
	}

	pcaseexpr->defresult = pexprElse;

	return (Expr *)pcaseexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PcaseexprFromDXLNodeScSwitch
//
//	@doc:
//		Translates a DXL scalar switch into a GPDB CaseExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcaseexprFromDXLNodeScSwitch
	(
	const CDXLNode *pdxlnSwitch,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnSwitch);
	CDXLScalarSwitch *dxlop = CDXLScalarSwitch::Cast(pdxlnSwitch->GetOperator());

	CaseExpr *pcaseexpr = MakeNode(CaseExpr);
	pcaseexpr->casetype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	pcaseexpr->casecollid = gpdb::OidTypeCollation(pcaseexpr->casetype);

	// translate arg child
	pcaseexpr->arg = PexprFromDXLNodeScalar((*pdxlnSwitch)[0], pmapcidvar);
	GPOS_ASSERT(NULL != pcaseexpr->arg);

	const ULONG arity = pdxlnSwitch->Arity();
	GPOS_ASSERT(1 < arity);
	for (ULONG ul = 1; ul < arity; ul++)
	{
		const CDXLNode *pdxlnChild = (*pdxlnSwitch)[ul];

		if (EdxlopScalarSwitchCase == pdxlnChild->GetOperator()->GetDXLOperator())
		{
			CaseWhen *pwhen = MakeNode(CaseWhen);
			pwhen->expr = PexprFromDXLNodeScalar((*pdxlnChild)[0], pmapcidvar);
			pwhen->result = PexprFromDXLNodeScalar((*pdxlnChild)[1], pmapcidvar);
			pcaseexpr->args = gpdb::PlAppendElement(pcaseexpr->args, pwhen);
		}
		else
		{
			// default return value
			GPOS_ASSERT(ul == arity - 1);
			pcaseexpr->defresult = PexprFromDXLNodeScalar((*pdxlnSwitch)[ul], pmapcidvar);
		}
	}

	return (Expr *)pcaseexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PcasetestexprFromDXLNodeScCaseTest
//
//	@doc:
//		Translates a DXL scalar case test into a GPDB CaseTestExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcasetestexprFromDXLNodeScCaseTest
	(
	const CDXLNode *pdxlnCaseTest,
	CMappingColIdVar * //pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCaseTest);
	CDXLScalarCaseTest *dxlop = CDXLScalarCaseTest::Cast(pdxlnCaseTest->GetOperator());

	CaseTestExpr *pcasetestexpr = MakeNode(CaseTestExpr);
	pcasetestexpr->typeId = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	pcasetestexpr->typeMod = -1;
	// GDPB_91_MERGE_FIXME: collation
	pcasetestexpr->collation = gpdb::OidTypeCollation(pcasetestexpr->typeId);

	return (Expr *)pcasetestexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PopexprFromDXLNodeScOpExpr
//
//	@doc:
//		Translates a DXL scalar opexpr into a GPDB OpExpr node
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PopexprFromDXLNodeScOpExpr
	(
	const CDXLNode *pdxlnOpExpr,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnOpExpr);
	CDXLScalarOpExpr *pdxlopOpExpr = CDXLScalarOpExpr::Cast(pdxlnOpExpr->GetOperator());

	OpExpr *popexpr = MakeNode(OpExpr);
	popexpr->opno = CMDIdGPDB::CastMdid(pdxlopOpExpr->MDId())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(pdxlopOpExpr->MDId());
	popexpr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();

	IMDId *return_type_mdid = pdxlopOpExpr->GetReturnTypeMdId();
	if (NULL != return_type_mdid)
	{
		popexpr->opresulttype = CMDIdGPDB::CastMdid(return_type_mdid)->OidObjectId();
	}
	else 
	{
		popexpr->opresulttype = OidFunctionReturnType(md_scalar_op->FuncMdId());
	}

	const IMDFunction *pmdfunc = m_md_accessor->Pmdfunc(md_scalar_op->FuncMdId());
	popexpr->opretset = pmdfunc->ReturnsSet();

	GPOS_ASSERT(1 == pdxlnOpExpr->Arity() || 2 == pdxlnOpExpr->Arity());

	// translate children
	popexpr->args = PlistTranslateScalarChildren(popexpr->args, pdxlnOpExpr, pmapcidvar);

	// GDPB_91_MERGE_FIXME: collation
	popexpr->inputcollid = gpdb::OidExprCollation((Node *) popexpr->args);
	popexpr->opcollid = gpdb::OidTypeCollation(popexpr->opresulttype);

	return (Expr *)popexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PstrarrayopexprFromDXLNodeScArrayComp
//
//	@doc:
//		Translates a CDXLScalarArrayComp into a GPDB ScalarArrayOpExpr
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PstrarrayopexprFromDXLNodeScArrayComp
	(
	const CDXLNode *pdxlnArrayComp,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnArrayComp);
	CDXLScalarArrayComp *pdxlopArrayComp = CDXLScalarArrayComp::Cast(pdxlnArrayComp->GetOperator());

	ScalarArrayOpExpr *parrayopexpr = MakeNode(ScalarArrayOpExpr);
	parrayopexpr->opno = CMDIdGPDB::CastMdid(pdxlopArrayComp->MDId())->OidObjectId();
	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(pdxlopArrayComp->MDId());
	parrayopexpr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();

	switch(pdxlopArrayComp->GetDXLArrayCmpType())
	{
		case Edxlarraycomptypeany:
				parrayopexpr->useOr = true;
				break;

		case Edxlarraycomptypeall:
				parrayopexpr->useOr = false;
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

	GPOS_ASSERT(2 == pdxlnArrayComp->Arity());

	CDXLNode *pdxlnLeft = (*pdxlnArrayComp)[EdxlsccmpIndexLeft];
	CDXLNode *pdxlnRight = (*pdxlnArrayComp)[EdxlsccmpIndexRight];

	Expr *pexprLeft = PexprFromDXLNodeScalar(pdxlnLeft, pmapcidvar);
	Expr *pexprRight = PexprFromDXLNodeScalar(pdxlnRight, pmapcidvar);

	parrayopexpr->args = ListMake2(pexprLeft, pexprRight);
	// GDPB_91_MERGE_FIXME: collation
	parrayopexpr->inputcollid = gpdb::OidExprCollation((Node *) parrayopexpr->args);

	return (Expr *)parrayopexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PdistexprFromDXLNodeScDistinctComp
//
//	@doc:
//		Translates a DXL scalar distinct comparison into a GPDB DistinctExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PdistexprFromDXLNodeScDistinctComp
	(
	const CDXLNode *pdxlnDistComp,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnDistComp);
	CDXLScalarDistinctComp *dxlop = CDXLScalarDistinctComp::Cast(pdxlnDistComp->GetOperator());

	DistinctExpr *pdistexpr = MakeNode(DistinctExpr);
	pdistexpr->opno = CMDIdGPDB::CastMdid(dxlop->MDId())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(dxlop->MDId());

	pdistexpr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
	pdistexpr->opresulttype = OidFunctionReturnType(md_scalar_op->FuncMdId());
	// GDPB_91_MERGE_FIXME: collation
	pdistexpr->opcollid = gpdb::OidTypeCollation(pdistexpr->opresulttype);

	// translate left and right child
	GPOS_ASSERT(2 == pdxlnDistComp->Arity());
	CDXLNode *pdxlnLeft = (*pdxlnDistComp)[EdxlscdistcmpIndexLeft];
	CDXLNode *pdxlnRight = (*pdxlnDistComp)[EdxlscdistcmpIndexRight];

	Expr *pexprLeft = PexprFromDXLNodeScalar(pdxlnLeft, pmapcidvar);
	Expr *pexprRight = PexprFromDXLNodeScalar(pdxlnRight, pmapcidvar);

	pdistexpr->args = ListMake2(pexprLeft, pexprRight);

	return (Expr *)pdistexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PaggrefFromDXLNodeScAggref
//
//	@doc:
//		Translates a DXL scalar aggref into a GPDB Aggref node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PaggrefFromDXLNodeScAggref
	(
	const CDXLNode *pdxlnAggref,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnAggref);
	CDXLScalarAggref *dxlop = CDXLScalarAggref::Cast(pdxlnAggref->GetOperator());

	Aggref *paggref = MakeNode(Aggref);
	paggref->aggfnoid = CMDIdGPDB::CastMdid(dxlop->GetDXLAggFuncMDid())->OidObjectId();
	paggref->aggdistinct = NIL;
	paggref->agglevelsup = 0;
	paggref->aggkind = 'n';
	paggref->location = -1;

	CMDIdGPDB *pmdidAgg = GPOS_NEW(m_memory_pool) CMDIdGPDB(paggref->aggfnoid);
	const IMDAggregate *pmdagg = m_md_accessor->Pmdagg(pmdidAgg);
	pmdidAgg->Release();

	EdxlAggrefStage edxlaggstage = dxlop->GetDXLAggStage();
	if (NULL != dxlop->GetDXLResolvedRetTypeMDid())
	{
		// use resolved type
		paggref->aggtype = CMDIdGPDB::CastMdid(dxlop->GetDXLResolvedRetTypeMDid())->OidObjectId();
	}
	else if (EdxlaggstageIntermediate == edxlaggstage || EdxlaggstagePartial == edxlaggstage)
	{
		paggref->aggtype = CMDIdGPDB::CastMdid(pmdagg->GetIntermediateResultTypeMdid())->OidObjectId();
	}
	else
	{
		paggref->aggtype = CMDIdGPDB::CastMdid(pmdagg->GetResultTypeMdid())->OidObjectId();
	}

	switch(dxlop->GetDXLAggStage())
	{
		case EdxlaggstageNormal:
					paggref->aggstage = AGGSTAGE_NORMAL;
					break;
		case EdxlaggstagePartial:
					paggref->aggstage = AGGSTAGE_PARTIAL;
					break;
		case EdxlaggstageIntermediate:
					paggref->aggstage = AGGSTAGE_INTERMEDIATE;
					break;
		case EdxlaggstageFinal:
					paggref->aggstage = AGGSTAGE_FINAL;
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
	List *argExprs = PlistTranslateScalarChildren(paggref->args, pdxlnAggref, pmapcidvar);

	int attno;
	paggref->args = NIL;
	ListCell *lc;
	int sortgrpindex = 1;
	ForEachWithCount (lc, argExprs, attno)
	{
		TargetEntry *pteNew = gpdb::PteMakeTargetEntry((Expr *) lfirst(lc), attno + 1, NULL, false);
		/*
		 * Translate the aggdistinct bool set to true (in ORCA),
		 * to a List of SortGroupClause in the PLNSTMT
		 */
		if(dxlop->IsDistinct())
		{
			pteNew->ressortgroupref = sortgrpindex;
			SortGroupClause *gc = makeNode(SortGroupClause);
			gc->tleSortGroupRef = sortgrpindex;
			gc->eqop = gpdb::OidEqualityOp(gpdb::OidExprType((Node*) pteNew->expr));
			gc->sortop = gpdb::OidOrderingOpForEqualityOp(gc->eqop, NULL);
			/*
			 * Since ORCA doesn't yet support ordered aggregates, we are
			 * setting nulls_first to false. This is also the default behavior
			 * when no order by clause is provided so it is OK to set it to
			 * false.
			 */
			gc->nulls_first = false;
			paggref->aggdistinct = gpdb::PlAppendElement(paggref->aggdistinct, gc);
			sortgrpindex++;
		}
		paggref->args = gpdb::PlAppendElement(paggref->args, pteNew);
	}

	// GDPB_91_MERGE_FIXME: collation
	paggref->inputcollid = gpdb::OidExprCollation((Node *) argExprs);
	paggref->aggcollid = gpdb::OidTypeCollation(paggref->aggtype);

	return (Expr *)paggref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PwindowrefFromDXLNodeScWindowRef
//
//	@doc:
//		Translate a DXL scalar window ref into a GPDB WindowRef node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PwindowrefFromDXLNodeScWindowRef
	(
	const CDXLNode *pdxlnWinref,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnWinref);
	CDXLScalarWindowRef *dxlop = CDXLScalarWindowRef::Cast(pdxlnWinref->GetOperator());

	WindowFunc *pwindowfunc = MakeNode(WindowFunc);
	pwindowfunc->winfnoid = CMDIdGPDB::CastMdid(dxlop->FuncMdId())->OidObjectId();

	pwindowfunc->windistinct = dxlop->IsDistinct();
	pwindowfunc->location = -1;
	pwindowfunc->winref = dxlop->GetWindSpecPos() + 1;
	pwindowfunc->wintype = CMDIdGPDB::CastMdid(dxlop->ReturnTypeMdId())->OidObjectId();
	pwindowfunc->winstar = dxlop->IsStarArg();
	pwindowfunc->winagg = dxlop->IsSimpleAgg();

	EdxlWinStage dxl_win_stage = dxlop->GetDxlWinStage();
	GPOS_ASSERT(dxl_win_stage != EdxlwinstageSentinel);

	ULONG rgrgulMapping[][2] =
			{
			{WINSTAGE_IMMEDIATE, EdxlwinstageImmediate},
			{WINSTAGE_PRELIMINARY, EdxlwinstagePreliminary},
			{WINSTAGE_ROWKEY, EdxlwinstageRowKey},
			};

	const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) dxl_win_stage == pulElem[1])
		{
			pwindowfunc->winstage = (WinStage) pulElem[0];
			break;
		}
	}

	// translate the arguments of the window function
	pwindowfunc->args = PlistTranslateScalarChildren(pwindowfunc->args, pdxlnWinref, pmapcidvar);
	// GDPB_91_MERGE_FIXME: collation
	pwindowfunc->wincollid = gpdb::OidTypeCollation(pwindowfunc->wintype);
	pwindowfunc->inputcollid = gpdb::OidExprCollation((Node *) pwindowfunc->args);

	return (Expr *) pwindowfunc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PfuncexprFromDXLNodeScFuncExpr
//
//	@doc:
//		Translates a DXL scalar opexpr into a GPDB FuncExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PfuncexprFromDXLNodeScFuncExpr
	(
	const CDXLNode *pdxlnFuncExpr,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnFuncExpr);
	CDXLScalarFuncExpr *dxlop = CDXLScalarFuncExpr::Cast(pdxlnFuncExpr->GetOperator());

	FuncExpr *pfuncexpr = MakeNode(FuncExpr);
	pfuncexpr->funcid = CMDIdGPDB::CastMdid(dxlop->FuncMdId())->OidObjectId();
	pfuncexpr->funcretset = dxlop->ReturnsSet();
	pfuncexpr->funcformat = COERCE_DONTCARE;
	pfuncexpr->funcresulttype = CMDIdGPDB::CastMdid(dxlop->ReturnTypeMdId())->OidObjectId();
	pfuncexpr->args = PlistTranslateScalarChildren(pfuncexpr->args, pdxlnFuncExpr, pmapcidvar);

	// GDPB_91_MERGE_FIXME: collation
	pfuncexpr->inputcollid = gpdb::OidExprCollation((Node *) pfuncexpr->args);
	pfuncexpr->funccollid = gpdb::OidTypeCollation(pfuncexpr->funcresulttype);

	return (Expr *)pfuncexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PsubplanFromDXLNodeScSubPlan
//
//	@doc:
//		Translates a DXL scalar SubPlan into a GPDB SubPlan node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PsubplanFromDXLNodeScSubPlan
	(
	const CDXLNode *pdxlnSubPlan,
	CMappingColIdVar *pmapcidvar
	)
{
	CDXLTranslateContext *output_context = (dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar))->GetOutputContext();

	CContextDXLToPlStmt *dxl_to_plstmt_context = (dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar))->GetDXLToPlStmtContext();

	CDXLScalarSubPlan *dxlop = CDXLScalarSubPlan::Cast(pdxlnSubPlan->GetOperator());

	// translate subplan test expression
	List *plparamIds = NIL;

	SubLinkType slink = CTranslatorUtils::MapDXLSubplanToSublinkType(dxlop->GetDxlSubplanType());
	Expr *pexprTestExpr = PexprSubplanTestExpr(dxlop->GetDxlTestExpr(), slink, pmapcidvar, &plparamIds);

	const DrgPdxlcr *pdrgdxlcrOuterRefs = dxlop->GetDxlOuterColRefsArray();

	const ULONG ulLen = pdrgdxlcrOuterRefs->Size();

	// create a copy of the translate context: the param mappings from the outer scope get copied in the constructor
	CDXLTranslateContext dxltrctxSubplan(m_memory_pool, output_context->IsParentAggNode(), output_context->GetColIdToParamIdMap());

	// insert new outer ref mappings in the subplan translate context
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		CDXLColRef *dxl_colref = (*pdrgdxlcrOuterRefs)[ul];
		IMDId *pmdid = dxl_colref->MDIdType();
		ULONG ulColid = dxl_colref->Id();
		INT type_modifier = dxl_colref->TypeModifier();

		if (NULL == dxltrctxSubplan.GetParamIdMappingElement(ulColid))
		{
			// keep outer reference mapping to the original column for subsequent subplans
			CMappingElementColIdParamId *pmecolidparamid = GPOS_NEW(m_memory_pool) CMappingElementColIdParamId(ulColid, dxl_to_plstmt_context->GetNextParamId(), pmdid, type_modifier);

#ifdef GPOS_DEBUG
			BOOL fInserted =
#endif
			dxltrctxSubplan.FInsertParamMapping(ulColid, pmecolidparamid);
			GPOS_ASSERT(fInserted);
		}
	}

	CDXLNode *pdxlnChild = (*pdxlnSubPlan)[0];
        GPOS_ASSERT(EdxloptypePhysical == pdxlnChild->GetOperator()->GetDXLOperatorType());

	GPOS_ASSERT(NULL != pdxlnSubPlan);
	GPOS_ASSERT(EdxlopScalarSubPlan == pdxlnSubPlan->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(1 == pdxlnSubPlan->Arity());

	// generate the child plan,
	// create DXL->PlStmt translator to handle subplan's relational children
	CTranslatorDXLToPlStmt trdxltoplstmt
							(
							m_memory_pool,
							m_md_accessor,
							(dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar))->GetDXLToPlStmtContext(),
							m_num_of_segments
							);
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	Plan *pplanChild = trdxltoplstmt.PplFromDXL(pdxlnChild, &dxltrctxSubplan, pdrgpdxltrctxPrevSiblings);
	pdrgpdxltrctxPrevSiblings->Release();

	GPOS_ASSERT(NULL != pplanChild->targetlist && 1 <= gpdb::ListLength(pplanChild->targetlist));

	// translate subplan and set test expression
	SubPlan *psubplan = PsubplanFromChildPlan(pplanChild, slink, dxl_to_plstmt_context);
	psubplan->testexpr = (Node *) pexprTestExpr;
	psubplan->paramIds = plparamIds;

	// translate other subplan params
	TranslateSubplanParams(psubplan, &dxltrctxSubplan, pdrgdxlcrOuterRefs, pmapcidvar);

	return (Expr *)psubplan;
}

inline BOOL FDXLCastedId(CDXLNode *dxlnode)
{
	return EdxlopScalarCast == dxlnode->GetOperator()->GetDXLOperator() &&
		   dxlnode->Arity() > 0 && EdxlopScalarIdent == (*dxlnode)[0]->GetOperator()->GetDXLOperator();
}

inline CTranslatorDXLToScalar::STypeOidAndTypeModifier OidParamOidFromDXLIdentOrDXLCastIdent(CDXLNode *pdxlnIdentOrCastIdent)
{
	GPOS_ASSERT(EdxlopScalarIdent == pdxlnIdentOrCastIdent->GetOperator()->GetDXLOperator() || FDXLCastedId(pdxlnIdentOrCastIdent));

	CDXLScalarIdent *pdxlopInnerIdent;
	if (EdxlopScalarIdent == pdxlnIdentOrCastIdent->GetOperator()->GetDXLOperator())
	{
		pdxlopInnerIdent = CDXLScalarIdent::Cast(pdxlnIdentOrCastIdent->GetOperator());
	}
	else
	{
		pdxlopInnerIdent = CDXLScalarIdent::Cast((*pdxlnIdentOrCastIdent)[0]->GetOperator());
	}
	Oid oidInnerType = CMDIdGPDB::CastMdid(pdxlopInnerIdent->MDIdType())->OidObjectId();
	INT type_modifier = pdxlopInnerIdent->TypeModifier();
	return {oidInnerType, type_modifier};
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::PexprSubplanTestExpr
//
//      @doc:
//              Translate subplan test expression
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprSubplanTestExpr
	(
	CDXLNode *dxlnode_test_expr,
	SubLinkType slink,
	CMappingColIdVar *pmapcidvar,
	List **plparamIds
	)
{
	if (EXPR_SUBLINK == slink || EXISTS_SUBLINK == slink || NOT_EXISTS_SUBLINK == slink)
	{
		// expr/exists/not-exists sublinks have no test expression
		return NULL;
	}
	GPOS_ASSERT(NULL != dxlnode_test_expr);

	if (FConstTrue(dxlnode_test_expr, m_md_accessor))
	{
		// dummy test expression
		return (Expr *) PconstFromDXLNodeScConst(dxlnode_test_expr, NULL);
	}

	if (EdxlopScalarCmp != dxlnode_test_expr->GetOperator()->GetDXLOperator())
	{
		// test expression is expected to be a comparison
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,  GPOS_WSZ_LIT("Unexpected subplan test expression"));
	}

	GPOS_ASSERT(2 == dxlnode_test_expr->Arity());
	GPOS_ASSERT(ANY_SUBLINK == slink || ALL_SUBLINK == slink);

	CDXLNode *pdxlnOuterChild = (*dxlnode_test_expr)[0];
	CDXLNode *pdxlnInnerChild = (*dxlnode_test_expr)[1];

	if (EdxlopScalarIdent != pdxlnInnerChild->GetOperator()->GetDXLOperator() && !FDXLCastedId(pdxlnInnerChild))
	{
		// test expression is expected to be a comparison between an outer expression 
		// and a scalar identifier from subplan child
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,  GPOS_WSZ_LIT("Unexpected subplan test expression"));
	}

	// extract type of inner column
        CDXLScalarComp *pdxlopCmp = CDXLScalarComp::Cast(dxlnode_test_expr->GetOperator());

		// create an OpExpr for subplan test expression
        OpExpr *popexpr = MakeNode(OpExpr);
        popexpr->opno = CMDIdGPDB::CastMdid(pdxlopCmp->MDId())->OidObjectId();
        const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(pdxlopCmp->MDId());
        popexpr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
        popexpr->opresulttype = CMDIdGPDB::CastMdid(m_md_accessor->PtMDType<IMDTypeBool>()->MDId())->OidObjectId();
        popexpr->opretset = false;

        // translate outer expression (can be a deep scalar tree)
        Expr *pexprTestExprOuterArg = PexprFromDXLNodeScalar(pdxlnOuterChild, pmapcidvar);

        // add translated outer expression as first arg of OpExpr
        List *plistArgs = NIL;
        plistArgs = gpdb::PlAppendElement(plistArgs, pexprTestExprOuterArg);

	// second arg must be an EXEC param which is replaced during query execution with subplan output
	Param *pparam = MakeNode(Param);
	pparam->paramkind = PARAM_EXEC;
	CContextDXLToPlStmt *dxl_to_plstmt_context = (dynamic_cast<CMappingColIdVarPlStmt *>(pmapcidvar))->GetDXLToPlStmtContext();
	pparam->paramid = dxl_to_plstmt_context->GetNextParamId();
	CTranslatorDXLToScalar::STypeOidAndTypeModifier oidAndTypeModifier = OidParamOidFromDXLIdentOrDXLCastIdent(pdxlnInnerChild);
	pparam->paramtype = oidAndTypeModifier.OidType;
	pparam->paramtypmod = oidAndTypeModifier.TypeModifier;

	// test expression is used for non-scalar subplan,
	// second arg of test expression must be an EXEC param referring to subplan output,
	// we add this param to subplan param ids before translating other params

	*plparamIds = gpdb::PlAppendInt(*plparamIds, pparam->paramid);

	if (EdxlopScalarIdent == pdxlnInnerChild->GetOperator()->GetDXLOperator())
	{
		plistArgs = gpdb::PlAppendElement(plistArgs, pparam);
	}
	else // we have a cast
	{
		CDXLScalarCast *pdxlScalaCast = CDXLScalarCast::Cast(pdxlnInnerChild->GetOperator());
		Expr *pexprCastParam = PrelabeltypeOrFuncexprFromDXLNodeScalarCast(pdxlScalaCast, (Expr *) pparam);
		plistArgs = gpdb::PlAppendElement(plistArgs, pexprCastParam);
	}
	popexpr->args = plistArgs;

	return (Expr *) popexpr;
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
	SubPlan *psubplan,
	CDXLTranslateContext *pdxltrctx,
	const DrgPdxlcr *pdrgdxlcrOuterRefs,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != psubplan);
	GPOS_ASSERT(NULL != pdxltrctx);
	GPOS_ASSERT(NULL != pdrgdxlcrOuterRefs);
	GPOS_ASSERT(NULL != pmapcidvar);

	// Create the PARAM and ARG nodes
	const ULONG size = pdrgdxlcrOuterRefs->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CDXLColRef *dxl_colref = (*pdrgdxlcrOuterRefs)[ul];
		dxl_colref->AddRef();
		const CMappingElementColIdParamId *pmecolidparamid = pdxltrctx->GetParamIdMappingElement(dxl_colref->Id());

		// TODO: eliminate pparam, it's not *really* used, and it's (short-term) leaked
		Param *pparam = PparamFromMapping(pmecolidparamid);
		psubplan->parParam = gpdb::PlAppendInt(psubplan->parParam, pparam->paramid);

		GPOS_ASSERT(pmecolidparamid->MDIdType()->Equals(dxl_colref->MDIdType()));

		CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_memory_pool) CDXLScalarIdent(m_memory_pool, dxl_colref);
		Expr *parg = (Expr *) pmapcidvar->VarFromDXLNodeScId(pdxlopIdent);

		// not found in mapping, it must be an external parameter
		if (NULL == parg)
		{
			parg = (Expr*) PparamFromMapping(pmecolidparamid);
			GPOS_ASSERT(NULL != parg);
		}

		pdxlopIdent->Release();
		psubplan->args = gpdb::PlAppendElement(psubplan->args, parg);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PsubplanFromChildPlan
//
//	@doc:
//		add child plan to translation context, and build a subplan from it
//
//---------------------------------------------------------------------------
SubPlan *
CTranslatorDXLToScalar::PsubplanFromChildPlan
	(
	Plan *pplan,
	SubLinkType slink,
	CContextDXLToPlStmt *dxl_to_plstmt_context
	)
{
	dxl_to_plstmt_context->AddSubplan(pplan);

	SubPlan *psubplan = MakeNode(SubPlan);
	psubplan->plan_id = gpdb::ListLength(dxl_to_plstmt_context->GetSubplanEntriesList());
	psubplan->plan_name = SzSubplanAlias(psubplan->plan_id);
	psubplan->is_initplan = false;
	psubplan->firstColType = gpdb::OidExprType( (Node*) ((TargetEntry*) gpdb::PvListNth(pplan->targetlist, 0))->expr);
	// GDPB_91_MERGE_FIXME: collation
	psubplan->firstColCollation = gpdb::OidTypeCollation(psubplan->firstColType);
	psubplan->firstColTypmod = -1;
	psubplan->subLinkType = slink;
	psubplan->is_multirow = false;
	psubplan->unknownEqFalse = false;

	return psubplan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::SzSubplanAlias
//
//	@doc:
//		build plan name, for explain purposes
//
//---------------------------------------------------------------------------
CHAR *
CTranslatorDXLToScalar::SzSubplanAlias
	(
	ULONG ulPlanId
	)
{
	CWStringDynamic *str = GPOS_NEW(m_memory_pool) CWStringDynamic(m_memory_pool);
	str->AppendFormat(GPOS_WSZ_LIT("SubPlan %d"), ulPlanId);
	const WCHAR *wsz = str->GetBuffer();

	ULONG ulMaxLength = (GPOS_WSZ_LENGTH(wsz) + 1) * GPOS_SIZEOF(WCHAR);
	CHAR *sz = (CHAR *) gpdb::GPDBAlloc(ulMaxLength);
	gpos::clib::Wcstombs(sz, const_cast<WCHAR *>(wsz), ulMaxLength);
	sz[ulMaxLength - 1] = '\0';
	GPOS_DELETE(str);

	return sz;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PparamFromMapping
//
//	@doc:
//		Creates a GPDB param from the given mapping
//
//---------------------------------------------------------------------------
Param *
CTranslatorDXLToScalar::PparamFromMapping
	(
	const CMappingElementColIdParamId *pmecolidparamid
	)
{
	Param *pparam = MakeNode(Param);
	pparam->paramid = pmecolidparamid->ParamId();
	pparam->paramkind = PARAM_EXEC;
	pparam->paramtype = CMDIdGPDB::CastMdid(pmecolidparamid->MDIdType())->OidObjectId();
	pparam->paramtypmod = pmecolidparamid->TypeModifier();
	// GDPB_91_MERGE_FIXME: collation
	pparam->paramcollid = gpdb::OidTypeCollation(pparam->paramtype);

	return pparam;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PboolexprFromDXLNodeScBoolExpr
//
//	@doc:
//		Translates a DXL scalar BoolExpr into a GPDB OpExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PboolexprFromDXLNodeScBoolExpr
	(
	const CDXLNode *pdxlnBoolExpr,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnBoolExpr);
	CDXLScalarBoolExpr *dxlop = CDXLScalarBoolExpr::Cast(pdxlnBoolExpr->GetOperator());
	BoolExpr *pboolexpr = MakeNode(BoolExpr);

	GPOS_ASSERT(1 <= pdxlnBoolExpr->Arity());
	switch (dxlop->GetDxlBoolTypeStr())
	{
		case Edxlnot:
		{
			GPOS_ASSERT(1 == pdxlnBoolExpr->Arity());
			pboolexpr->boolop = NOT_EXPR;
			break;
		}
		case Edxland:
		{
			GPOS_ASSERT(2 <= pdxlnBoolExpr->Arity());
			pboolexpr->boolop = AND_EXPR;
			break;
		}
		case Edxlor:
		{
			GPOS_ASSERT(2 <= pdxlnBoolExpr->Arity());
			pboolexpr->boolop = OR_EXPR;
			break;
		}
		default:
		{
			GPOS_ASSERT(!"Boolean Operation: Must be either or/ and / not");
			return NULL;
		}
	}

	pboolexpr->args = PlistTranslateScalarChildren(pboolexpr->args, pdxlnBoolExpr, pmapcidvar);
	pboolexpr->location = -1;

	return (Expr *)pboolexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PbooleantestFromDXLNodeScBooleanTest
//
//	@doc:
//		Translates a DXL scalar BooleanTest into a GPDB OpExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PbooleantestFromDXLNodeScBooleanTest
	(
	const CDXLNode *pdxlnBooleanTest,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnBooleanTest);
	CDXLScalarBooleanTest *dxlop = CDXLScalarBooleanTest::Cast(pdxlnBooleanTest->GetOperator());
	BooleanTest *pbooleantest = MakeNode(BooleanTest);

	switch (dxlop->GetDxlBoolTypeStr())
	{
		case EdxlbooleantestIsTrue:
				pbooleantest->booltesttype = IS_TRUE;
				break;
		case EdxlbooleantestIsNotTrue:
				pbooleantest->booltesttype = IS_NOT_TRUE;
				break;
		case EdxlbooleantestIsFalse:
				pbooleantest->booltesttype = IS_FALSE;
				break;
		case EdxlbooleantestIsNotFalse:
				pbooleantest->booltesttype = IS_NOT_FALSE;
				break;
		case EdxlbooleantestIsUnknown:
				pbooleantest->booltesttype = IS_UNKNOWN;
				break;
		case EdxlbooleantestIsNotUnknown:
				pbooleantest->booltesttype = IS_NOT_UNKNOWN;
				break;
		default:
				{
				GPOS_ASSERT(!"Invalid Boolean Test Operation");
				return NULL;
				}
	}

	GPOS_ASSERT(1 == pdxlnBooleanTest->Arity());
	CDXLNode *dxlnode_arg = (*pdxlnBooleanTest)[0];

	Expr *pexprArg = PexprFromDXLNodeScalar(dxlnode_arg, pmapcidvar);
	pbooleantest->arg = pexprArg;

	return (Expr *)pbooleantest;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PnulltestFromDXLNodeScNullTest
//
//	@doc:
//		Translates a DXL scalar NullTest into a GPDB NullTest node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PnulltestFromDXLNodeScNullTest
	(
	const CDXLNode *pdxlnNullTest,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnNullTest);
	CDXLScalarNullTest *dxlop = CDXLScalarNullTest::Cast(pdxlnNullTest->GetOperator());
	NullTest *pnulltest = MakeNode(NullTest);

	GPOS_ASSERT(1 == pdxlnNullTest->Arity());
	CDXLNode *pdxlnChild = (*pdxlnNullTest)[0];
	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	if (dxlop->IsNullTest())
	{
		pnulltest->nulltesttype = IS_NULL;
	}
	else
	{
		pnulltest->nulltesttype = IS_NOT_NULL;
	}

	pnulltest->arg = pexprChild;
	return (Expr *)pnulltest;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PnullifFromDXLNodeScNullIf
//
//	@doc:
//		Translates a DXL scalar nullif into a GPDB NullIfExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PnullifFromDXLNodeScNullIf
	(
	const CDXLNode *pdxlnNullIf,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnNullIf);
	CDXLScalarNullIf *dxlop = CDXLScalarNullIf::Cast(pdxlnNullIf->GetOperator());

	NullIfExpr *pnullifexpr = MakeNode(NullIfExpr);
	pnullifexpr->opno = CMDIdGPDB::CastMdid(dxlop->MdIdOp())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(dxlop->MdIdOp());

	pnullifexpr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
	pnullifexpr->opresulttype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	pnullifexpr->opretset = false;

	// translate children
	GPOS_ASSERT(2 == pdxlnNullIf->Arity());
	pnullifexpr->args = PlistTranslateScalarChildren(pnullifexpr->args, pdxlnNullIf, pmapcidvar);
	// GDPB_91_MERGE_FIXME: collation
	pnullifexpr->opcollid = gpdb::OidTypeCollation(pnullifexpr->opresulttype);
	pnullifexpr->inputcollid = gpdb::OidExprCollation((Node *) pnullifexpr->args);

	return (Expr *) pnullifexpr;
}

Expr *
CTranslatorDXLToScalar::PrelabeltypeOrFuncexprFromDXLNodeScalarCast(const CDXLScalarCast *pdxlscalarcast, Expr *pexprChild)
{
	if (IMDId::IsValid(pdxlscalarcast->FuncMdId()))
	{
		FuncExpr *pfuncexpr = MakeNode(FuncExpr);
		pfuncexpr->funcid = CMDIdGPDB::CastMdid(pdxlscalarcast->FuncMdId())->OidObjectId();

		const IMDFunction *pmdfunc = m_md_accessor->Pmdfunc(pdxlscalarcast->FuncMdId());
		pfuncexpr->funcretset = pmdfunc->ReturnsSet();;

		pfuncexpr->funcformat = COERCE_IMPLICIT_CAST;
		pfuncexpr->funcresulttype = CMDIdGPDB::CastMdid(pdxlscalarcast->MDIdType())->OidObjectId();

		pfuncexpr->args = NIL;
		pfuncexpr->args = gpdb::PlAppendElement(pfuncexpr->args, pexprChild);

		// GDPB_91_MERGE_FIXME: collation
		pfuncexpr->inputcollid = gpdb::OidExprCollation((Node *) pfuncexpr->args);
		pfuncexpr->funccollid = gpdb::OidTypeCollation(pfuncexpr->funcresulttype);

		return (Expr *) pfuncexpr;
	}

	RelabelType *prelabeltype = MakeNode(RelabelType);

	prelabeltype->resulttype = CMDIdGPDB::CastMdid(pdxlscalarcast->MDIdType())->OidObjectId();
	prelabeltype->arg = pexprChild;
	prelabeltype->resulttypmod = -1;
	prelabeltype->location = -1;
	prelabeltype->relabelformat = COERCE_DONTCARE;
	// GDPB_91_MERGE_FIXME: collation
	prelabeltype->resultcollid = gpdb::OidTypeCollation(prelabeltype->resulttype);

	return (Expr *) prelabeltype;
}

// Translates a DXL scalar cast into a GPDB RelabelType / FuncExpr node
Expr *
CTranslatorDXLToScalar::PrelabeltypeFromDXLNodeScCast
	(
	const CDXLNode *pdxlnCast,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCast);
	const CDXLScalarCast *dxlop = CDXLScalarCast::Cast(pdxlnCast->GetOperator());

	GPOS_ASSERT(1 == pdxlnCast->Arity());
	CDXLNode *pdxlnChild = (*pdxlnCast)[0];

	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	return PrelabeltypeOrFuncexprFromDXLNodeScalarCast(dxlop, pexprChild);
}


//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::PcoerceFromDXLNodeScCoerceToDomain
//
//      @doc:
//              Translates a DXL scalar coerce into a GPDB coercetodomain node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcoerceFromDXLNodeScCoerceToDomain
	(
	const CDXLNode *pdxlnCoerce,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCoerce);
	CDXLScalarCoerceToDomain *dxlop = CDXLScalarCoerceToDomain::Cast(pdxlnCoerce->GetOperator());

	GPOS_ASSERT(1 == pdxlnCoerce->Arity());
	CDXLNode *pdxlnChild = (*pdxlnCoerce)[0];
	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	CoerceToDomain *pcoerce = MakeNode(CoerceToDomain);

	pcoerce->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	pcoerce->arg = pexprChild;
	pcoerce->resulttypmod = dxlop->TypeModifier();
	pcoerce->location = dxlop->GetLocation();
	pcoerce->coercionformat = (CoercionForm)  dxlop->GetDXLCoercionForm();
	// GDPB_91_MERGE_FIXME: collation
	pcoerce->resultcollid = gpdb::OidTypeCollation(pcoerce->resulttype);

	return (Expr *) pcoerce;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::PcoerceFromDXLNodeScCoerceViaIO
//
//      @doc:
//              Translates a DXL scalar coerce into a GPDB coerceviaio node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcoerceFromDXLNodeScCoerceViaIO
	(
	const CDXLNode *pdxlnCoerce,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCoerce);
	CDXLScalarCoerceViaIO *dxlop = CDXLScalarCoerceViaIO::Cast(pdxlnCoerce->GetOperator());

	GPOS_ASSERT(1 == pdxlnCoerce->Arity());
	CDXLNode *pdxlnChild = (*pdxlnCoerce)[0];
	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	CoerceViaIO *pcoerce = MakeNode(CoerceViaIO);

	pcoerce->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	pcoerce->arg = pexprChild;
	pcoerce->location = dxlop->GetLocation();
	pcoerce->coerceformat = (CoercionForm)  dxlop->GetDXLCoercionForm();
	// GDPB_91_MERGE_FIXME: collation
	pcoerce->resultcollid = gpdb::OidTypeCollation(pcoerce->resulttype);

  return (Expr *) pcoerce;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorDXLToScalar::PcoerceFromDXLNodeScArrayCoerceExpr
//
//      @doc:
//              Translates a DXL scalar array coerce expr into a GPDB T_ArrayCoerceExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcoerceFromDXLNodeScArrayCoerceExpr
	(
	const CDXLNode *pdxlnCoerce,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCoerce);
	CDXLScalarArrayCoerceExpr *dxlop = CDXLScalarArrayCoerceExpr::Cast(pdxlnCoerce->GetOperator());

	GPOS_ASSERT(1 == pdxlnCoerce->Arity());
	CDXLNode *pdxlnChild = (*pdxlnCoerce)[0];

	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	ArrayCoerceExpr *pcoerce = MakeNode(ArrayCoerceExpr);

	pcoerce->arg = pexprChild;
	pcoerce->elemfuncid = CMDIdGPDB::CastMdid(dxlop->GetCoerceFuncMDid())->OidObjectId();
	pcoerce->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	pcoerce->resulttypmod = dxlop->TypeModifier();
	// GDPB_91_MERGE_FIXME: collation
	pcoerce->resultcollid = gpdb::OidTypeCollation(pcoerce->resulttype);
	pcoerce->isExplicit = dxlop->IsExplicit();
	pcoerce->coerceformat = (CoercionForm)  dxlop->GetDXLCoercionForm();
	pcoerce->location = dxlop->GetLocation();

	return (Expr *) pcoerce;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PcoalesceFromDXLNodeScCoalesce
//
//	@doc:
//		Translates a DXL scalar coalesce operator into a GPDB coalesce node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PcoalesceFromDXLNodeScCoalesce
	(
	const CDXLNode *pdxlnCoalesce,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCoalesce);
	CDXLScalarCoalesce *dxlop = CDXLScalarCoalesce::Cast(pdxlnCoalesce->GetOperator());
	CoalesceExpr *pcoalesce = MakeNode(CoalesceExpr);

	pcoalesce->coalescetype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	pcoalesce->coalescecollid = gpdb::OidTypeCollation(pcoalesce->coalescetype);
	pcoalesce->args = PlistTranslateScalarChildren(pcoalesce->args, pdxlnCoalesce, pmapcidvar);
	pcoalesce->location = -1;

	return (Expr *) pcoalesce;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PminmaxFromDXLNodeScMinMax
//
//	@doc:
//		Translates a DXL scalar minmax operator into a GPDB minmax node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PminmaxFromDXLNodeScMinMax
	(
	const CDXLNode *pdxlnMinMax,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnMinMax);
	CDXLScalarMinMax *dxlop = CDXLScalarMinMax::Cast(pdxlnMinMax->GetOperator());
	MinMaxExpr *pminmax = MakeNode(MinMaxExpr);

	pminmax->minmaxtype = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	pminmax->minmaxcollid = gpdb::OidTypeCollation(pminmax->minmaxtype);
	pminmax->args = PlistTranslateScalarChildren(pminmax->args, pdxlnMinMax, pmapcidvar);
	// GDPB_91_MERGE_FIXME: collation
	pminmax->inputcollid = gpdb::OidExprCollation((Node *) pminmax->args);
	pminmax->location = -1;

	CDXLScalarMinMax::EdxlMinMaxType min_max_type = dxlop->GetMinMaxType();
	if (CDXLScalarMinMax::EmmtMax == min_max_type)
	{
		pminmax->op = IS_GREATEST;
	}
	else
	{
		GPOS_ASSERT(CDXLScalarMinMax::EmmtMin == min_max_type);
		pminmax->op = IS_LEAST;
	}

	return (Expr *) pminmax;
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
	List *plist,
	const CDXLNode *dxlnode,
	CMappingColIdVar *pmapcidvar
	)
{
	List *plistNew = plist;

	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnChild = (*dxlnode)[ul];
		Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);
		plistNew = gpdb::PlAppendElement(plistNew, pexprChild);
	}

	return plistNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstFromDXLNodeScConst
//
//	@doc:
//		Translates a DXL scalar constant operator into a GPDB scalar const node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PconstFromDXLNodeScConst
	(
	const CDXLNode *pdxlnConst,
	CMappingColIdVar * //pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnConst);
	CDXLScalarConstValue *dxlop = CDXLScalarConstValue::Cast(pdxlnConst->GetOperator());
	CDXLDatum *datum_dxl = const_cast<CDXLDatum*>(dxlop->GetDatumVal());

	return PconstFromDXLDatum(datum_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstFromDXLDatum
//
//	@doc:
//		Translates a DXL datum into a GPDB scalar const node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PconstFromDXLDatum
	(
	CDXLDatum *datum_dxl
	)
{
	GPOS_ASSERT(NULL != datum_dxl);
	static const SDatumTranslatorElem rgTranslators[] =
		{
			{CDXLDatum::EdxldatumInt2 , &CTranslatorDXLToScalar::PconstInt2},
			{CDXLDatum::EdxldatumInt4 , &CTranslatorDXLToScalar::PconstInt4},
			{CDXLDatum::EdxldatumInt8 , &CTranslatorDXLToScalar::PconstInt8},
			{CDXLDatum::EdxldatumBool , &CTranslatorDXLToScalar::PconstBool},
			{CDXLDatum::EdxldatumOid , &CTranslatorDXLToScalar::PconstOid},
			{CDXLDatum::EdxldatumGeneric, &CTranslatorDXLToScalar::PconstGeneric},
			{CDXLDatum::EdxldatumStatsDoubleMappable, &CTranslatorDXLToScalar::PconstGeneric},
			{CDXLDatum::EdxldatumStatsLintMappable, &CTranslatorDXLToScalar::PconstGeneric}
		};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);
	CDXLDatum::EdxldatumType edxldatumtype = datum_dxl->GetDatumType();

	// find translator for the node type
	PfPconst pf = NULL;
	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		SDatumTranslatorElem elem = rgTranslators[ul];
		if (edxldatumtype == elem.edxldt)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, CDXLTokens::GetDXLTokenStr(EdxltokenScalarConstValue)->GetBuffer());
	}

	return (Expr*) (this->*pf)(datum_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstOid
//
//	@doc:
//		Translates an oid datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::PconstOid
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumOid *pdxldatumOid = CDXLDatumOid::Cast(datum_dxl);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::CastMdid(pdxldatumOid->MDId())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constcollid = InvalidOid;
	pconst->constbyval = pdxldatumOid->IsPassedByValue();
	pconst->constisnull = pdxldatumOid->IsNull();
	pconst->constlen = pdxldatumOid->Length();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt32(pdxldatumOid->OidValue());
	}

	return pconst;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstInt2
//
//	@doc:
//		Translates an int2 datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::PconstInt2
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumInt2 *pdxldatumint2 = CDXLDatumInt2::Cast(datum_dxl);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::CastMdid(pdxldatumint2->MDId())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constcollid = InvalidOid;
	pconst->constbyval = pdxldatumint2->IsPassedByValue();
	pconst->constisnull = pdxldatumint2->IsNull();
	pconst->constlen = pdxldatumint2->Length();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt16(pdxldatumint2->Value());
	}

	return pconst;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstInt4
//
//	@doc:
//		Translates an int4 datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::PconstInt4
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumInt4 *pdxldatumint4 = CDXLDatumInt4::Cast(datum_dxl);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::CastMdid(pdxldatumint4->MDId())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constcollid = InvalidOid;
	pconst->constbyval = pdxldatumint4->IsPassedByValue();
	pconst->constisnull = pdxldatumint4->IsNull();
	pconst->constlen = pdxldatumint4->Length();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt32(pdxldatumint4->Value());
	}

	return pconst;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstInt8
//
//	@doc:
//		Translates an int8 datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::PconstInt8
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumInt8 *pdxldatumint8 = CDXLDatumInt8::Cast(datum_dxl);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::CastMdid(pdxldatumint8->MDId())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constcollid = InvalidOid;
	pconst->constbyval = pdxldatumint8->IsPassedByValue();
	pconst->constisnull = pdxldatumint8->IsNull();
	pconst->constlen = pdxldatumint8->Length();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt64(pdxldatumint8->Value());
	}

	return pconst;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstBool
//
//	@doc:
//		Translates a boolean datum into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::PconstBool
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumBool *pdxldatumbool = CDXLDatumBool::Cast(datum_dxl);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::CastMdid(pdxldatumbool->MDId())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constcollid = InvalidOid;
	pconst->constbyval = pdxldatumbool->IsPassedByValue();
	pconst->constisnull = pdxldatumbool->IsNull();
	pconst->constlen = pdxldatumbool->Length();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromBool(pdxldatumbool->GetValue());
	}


	return pconst;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PconstGeneric
//
//	@doc:
//		Translates a datum of generic type into a constant
//
//---------------------------------------------------------------------------
Const *
CTranslatorDXLToScalar::PconstGeneric
	(
	CDXLDatum *datum_dxl
	)
{
	CDXLDatumGeneric *pdxldatumgeneric = CDXLDatumGeneric::Cast(datum_dxl);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::CastMdid(pdxldatumgeneric->MDId())->OidObjectId();
	pconst->consttypmod = pdxldatumgeneric->TypeModifier();
	// GDPB_91_MERGE_FIXME: collation
	pconst->constcollid = gpdb::OidTypeCollation(pconst->consttype);
	pconst->constbyval = pdxldatumgeneric->IsPassedByValue();
	pconst->constisnull = pdxldatumgeneric->IsNull();
	pconst->constlen = pdxldatumgeneric->Length();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else if (pconst->constbyval)
	{
		// if it is a by-value constant, the value is stored in the datum.
		GPOS_ASSERT(pconst->constlen >= 0);
		GPOS_ASSERT((ULONG) pconst->constlen <= sizeof(Datum));
		memcpy(&pconst->constvalue, pdxldatumgeneric->GetByteArray(), sizeof(Datum));
	}
	else
	{
		Datum dVal = gpdb::DDatumFromPointer(pdxldatumgeneric->GetByteArray());
		ULONG length = (ULONG) gpdb::SDatumSize(dVal, false, pconst->constlen);

		CHAR *pcsz = (CHAR *) gpdb::GPDBAlloc(length + 1);
		memcpy(pcsz, pdxldatumgeneric->GetByteArray(), length);
		pcsz[length] = '\0';
		pconst->constvalue = gpdb::DDatumFromPointer(pcsz);
	}

	return pconst;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprPartDefault
//
//	@doc:
//		Translates a DXL part default into a GPDB part default
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprPartDefault
	(
	const CDXLNode *pdxlnPartDefault,
	CMappingColIdVar * //pmapcidvar
	)
{
	CDXLScalarPartDefault *dxlop = CDXLScalarPartDefault::Cast(pdxlnPartDefault->GetOperator());

	PartDefaultExpr *pexpr = MakeNode(PartDefaultExpr);
	pexpr->level = dxlop->GetPartitioningLevel();

	return (Expr *) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprPartBound
//
//	@doc:
//		Translates a DXL part bound into a GPDB part bound
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprPartBound
	(
	const CDXLNode *pdxlnPartBound,
	CMappingColIdVar * //pmapcidvar
	)
{
	CDXLScalarPartBound *dxlop = CDXLScalarPartBound::Cast(pdxlnPartBound->GetOperator());

	PartBoundExpr *pexpr = MakeNode(PartBoundExpr);
	pexpr->level = dxlop->GetPartitioningLevel();
	pexpr->boundType = CMDIdGPDB::CastMdid(dxlop->MDIdType())->OidObjectId();
	pexpr->isLowerBound = dxlop->IsLowerBound();

	return (Expr *) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprPartBoundInclusion
//
//	@doc:
//		Translates a DXL part bound inclusion into a GPDB part bound inclusion
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprPartBoundInclusion
	(
	const CDXLNode *pdxlnPartBoundIncl,
	CMappingColIdVar * //pmapcidvar
	)
{
	CDXLScalarPartBoundInclusion *dxlop = CDXLScalarPartBoundInclusion::Cast(pdxlnPartBoundIncl->GetOperator());

	PartBoundInclusionExpr *pexpr = MakeNode(PartBoundInclusionExpr);
	pexpr->level = dxlop->GetPartitioningLevel();
	pexpr->isLowerBound = dxlop->IsLowerBound();

	return (Expr *) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprPartBoundOpen
//
//	@doc:
//		Translates a DXL part bound openness into a GPDB part bound openness
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprPartBoundOpen
	(
	const CDXLNode *pdxlnPartBoundOpen,
	CMappingColIdVar * //pmapcidvar
	)
{
	CDXLScalarPartBoundOpen *dxlop = CDXLScalarPartBoundOpen::Cast(pdxlnPartBoundOpen->GetOperator());

	PartBoundOpenExpr *pexpr = MakeNode(PartBoundOpenExpr);
	pexpr->level = dxlop->GetPartitioningLevel();
	pexpr->isLowerBound = dxlop->IsLowerBound();

	return (Expr *) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprPartListValues
//
//	@doc:
//		Translates a DXL part list values into a GPDB part list values
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprPartListValues
	(
	const CDXLNode *pdxlnPartListValues,
	CMappingColIdVar * //pmapcidvar
	)
{
	CDXLScalarPartListValues *dxlop = CDXLScalarPartListValues::Cast(pdxlnPartListValues->GetOperator());

	PartListRuleExpr *pexpr = MakeNode(PartListRuleExpr);
	pexpr->level = dxlop->GetPartitioningLevel();
	pexpr->resulttype = CMDIdGPDB::CastMdid(dxlop->GetResultTypeMdId())->OidObjectId();
	pexpr->elementtype = CMDIdGPDB::CastMdid(dxlop->GetElemTypeMdId())->OidObjectId();

	return (Expr *) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprPartListNullTest
//
//	@doc:
//		Translates a DXL part list values into a GPDB part list null test
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprPartListNullTest
	(
	const CDXLNode *pdxlnPartListNullTest,
	CMappingColIdVar * //pmapcidvar
	)
{
	CDXLScalarPartListNullTest *dxlop = CDXLScalarPartListNullTest::Cast(pdxlnPartListNullTest->GetOperator());

	PartListNullTestExpr *pexpr = MakeNode(PartListNullTestExpr);
	pexpr->level = dxlop->GetPartitioningLevel();
	pexpr->nulltesttype = dxlop->IsNull() ? IS_NULL : IS_NOT_NULL;

	return (Expr *) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprFromDXLNodeScId
//
//	@doc:
//		Translates a DXL scalar ident into a GPDB Expr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprFromDXLNodeScId
	(
	const CDXLNode *dxl_sc_ident,
	CMappingColIdVar *pmapcidvar
	)
{
	CMappingColIdVarPlStmt *pmapcidvarplstmt = dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar);

	// scalar identifier
	CDXLScalarIdent *dxlop = CDXLScalarIdent::Cast(dxl_sc_ident->GetOperator());
	Expr *pexprResult = NULL;
	if (NULL == pmapcidvarplstmt || NULL == pmapcidvarplstmt->GetOutputContext()->GetParamIdMappingElement(dxlop->MakeDXLColRef()->Id()))
	{
		// not an outer ref -> create var node
		pexprResult = (Expr *) pmapcidvar->VarFromDXLNodeScId(dxlop);
	}
	else
	{
		// outer ref -> create param node
		pexprResult = (Expr *) pmapcidvarplstmt->ParamFromDXLNodeScId(dxlop);
	}

	if (NULL  == pexprResult)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, dxlop->MakeDXLColRef()->Id());
	}
	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PopexprFromDXLNodeScCmp
//
//	@doc:
//		Translates a DXL scalar comparison operator or a scalar distinct comparison into a GPDB OpExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PopexprFromDXLNodeScCmp
	(
	const CDXLNode *pdxlnCmp,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnCmp);
	CDXLScalarComp *dxlop = CDXLScalarComp::Cast(pdxlnCmp->GetOperator());

	OpExpr *popexpr = MakeNode(OpExpr);
	popexpr->opno = CMDIdGPDB::CastMdid(dxlop->MDId())->OidObjectId();

	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(dxlop->MDId());

	popexpr->opfuncid = CMDIdGPDB::CastMdid(md_scalar_op->FuncMdId())->OidObjectId();
	popexpr->opresulttype = CMDIdGPDB::CastMdid(m_md_accessor->PtMDType<IMDTypeBool>()->MDId())->OidObjectId();
	popexpr->opretset = false;

	// translate left and right child
	GPOS_ASSERT(2 == pdxlnCmp->Arity());

	CDXLNode *pdxlnLeft = (*pdxlnCmp)[EdxlsccmpIndexLeft];
	CDXLNode *pdxlnRight = (*pdxlnCmp)[EdxlsccmpIndexRight];

	Expr *pexprLeft = PexprFromDXLNodeScalar(pdxlnLeft, pmapcidvar);
	Expr *pexprRight = PexprFromDXLNodeScalar(pdxlnRight, pmapcidvar);

	popexpr->args = ListMake2(pexprLeft, pexprRight);

	// GDPB_91_MERGE_FIXME: collation
	popexpr->inputcollid = gpdb::OidExprCollation((Node *) popexpr->args);
	popexpr->opcollid = gpdb::OidTypeCollation(popexpr->opresulttype);;

	return (Expr *) popexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprArray
//
//	@doc:
//		Translates a DXL scalar array into a GPDB ArrayExpr node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprArray
	(
	const CDXLNode *pdxlnArray,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnArray);
	CDXLScalarArray *dxlop = CDXLScalarArray::Cast(pdxlnArray->GetOperator());

	ArrayExpr *pexpr = MakeNode(ArrayExpr);
	pexpr->element_typeid = CMDIdGPDB::CastMdid(dxlop->ElementTypeMDid())->OidObjectId();
	pexpr->array_typeid = CMDIdGPDB::CastMdid(dxlop->ArrayTypeMDid())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	pexpr->array_collid = gpdb::OidTypeCollation(pexpr->array_typeid);
	pexpr->multidims = dxlop->IsMultiDimensional();
	pexpr->elements = PlistTranslateScalarChildren(pexpr->elements, pdxlnArray, pmapcidvar);

	/*
	 * ORCA doesn't know how to construct array constants, so it will
	 * return any arrays as ArrayExprs. Convert them to array constants,
	 * for more efficient evaluation at runtime. (This will try to further
	 * simplify the elements, too, but that is most likely futile, as the
	 * expressions were already simplified as much as we could before they
	 * were passed to ORCA. But there's little harm in trying).
	 */
	return (Expr *) gpdb::PnodeEvalConstExpressions((Node *) pexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprArrayRef
//
//	@doc:
//		Translates a DXL scalar arrayref into a GPDB ArrayRef node
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprArrayRef
	(
	const CDXLNode *pdxlnArrayref,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnArrayref);
	CDXLScalarArrayRef *dxlop = CDXLScalarArrayRef::Cast(pdxlnArrayref->GetOperator());

	ArrayRef *parrayref = MakeNode(ArrayRef);
	parrayref->refarraytype = CMDIdGPDB::CastMdid(dxlop->ArrayTypeMDid())->OidObjectId();
	parrayref->refelemtype = CMDIdGPDB::CastMdid(dxlop->ElementTypeMDid())->OidObjectId();
	// GDPB_91_MERGE_FIXME: collation
	parrayref->refcollid = gpdb::OidTypeCollation(parrayref->refelemtype);
	parrayref->reftypmod = dxlop->TypeModifier();

	const ULONG arity = pdxlnArrayref->Arity();
	GPOS_ASSERT(3 == arity || 4 == arity);

	parrayref->reflowerindexpr = PlTranslateArrayRefIndexList((*pdxlnArrayref)[0], CDXLScalarArrayRefIndexList::EilbLower, pmapcidvar);
	parrayref->refupperindexpr = PlTranslateArrayRefIndexList((*pdxlnArrayref)[1], CDXLScalarArrayRefIndexList::EilbUpper, pmapcidvar);

	parrayref->refexpr = PexprFromDXLNodeScalar((*pdxlnArrayref)[2], pmapcidvar);
	parrayref->refassgnexpr = NULL;
	if (4 == arity)
	{
		parrayref->refassgnexpr = PexprFromDXLNodeScalar((*pdxlnArrayref)[3], pmapcidvar);
	}

	return (Expr *) parrayref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PlTranslateArrayRefIndexList
//
//	@doc:
//		Translates a DXL arrayref index list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToScalar::PlTranslateArrayRefIndexList
	(
	const CDXLNode *pdxlnIndexlist,
	CDXLScalarArrayRefIndexList::EIndexListBound
#ifdef GPOS_DEBUG
	eilb
#endif //GPOS_DEBUG
	,
	CMappingColIdVar *pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnIndexlist);
	GPOS_ASSERT(eilb == CDXLScalarArrayRefIndexList::Cast(pdxlnIndexlist->GetOperator())->GetDXLIndexListBound());

	List *plChildren = NIL;
	plChildren = PlistTranslateScalarChildren(plChildren, pdxlnIndexlist, pmapcidvar);

	return plChildren;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PexprDMLAction
//
//	@doc:
//		Translates a DML action expression 
//
//---------------------------------------------------------------------------
Expr *
CTranslatorDXLToScalar::PexprDMLAction
	(
	const CDXLNode *
#ifdef GPOS_DEBUG
	pdxlnDMLAction
#endif
	,
	CMappingColIdVar * // pmapcidvar
	)
{
	GPOS_ASSERT(NULL != pdxlnDMLAction);
	GPOS_ASSERT(EdxlopScalarDMLAction == pdxlnDMLAction->GetOperator()->GetDXLOperator());

	DMLActionExpr *pexpr = MakeNode(DMLActionExpr);

	return (Expr *) pexpr;
}



//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::OidFunctionReturnType
//
//	@doc:
//		Returns the operator return type oid for the operator funcid from the translation context
//
//---------------------------------------------------------------------------
Oid
CTranslatorDXLToScalar::OidFunctionReturnType
	(
	IMDId *pmdid
	)
	const
{
	return CMDIdGPDB::CastMdid(m_md_accessor->Pmdfunc(pmdid)->GetResultTypeMdid())->OidObjectId();
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
//		CTranslatorDXLToScalar::FConstTrue
//
//	@doc:
//		Check if the operator is a "true" bool constant
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToScalar::FConstTrue
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
	CDXLDatumBool *pdxldatumbool = CDXLDatumBool::Cast(const_cast<CDXLDatum *>(dxlop->GetDatumVal()));

	return pdxldatumbool->GetValue();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::FConstNull
//
//	@doc:
//		Check if the operator is a NULL constant
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToScalar::FConstNull
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
