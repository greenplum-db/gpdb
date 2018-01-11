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
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	ULONG ulSegments
	)
	:
	m_pmp(pmp),
	m_pmda(pmda),
	m_fHasSubqueries(false),
	m_ulSegments(ulSegments)
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
	const CDXLNode *pdxln,
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

	GPOS_ASSERT(NULL != pdxln);
	Edxlopid eopid = pdxln->Pdxlop()->Edxlop();

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
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, pdxln->Pdxlop()->PstrOpName()->Wsz());
	}

	return (this->*pf)(pdxln, pmapcidvar);
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
	CDXLScalarIfStmt *pdxlopIfStmt = CDXLScalarIfStmt::PdxlopConvert(pdxlnIfStmt->Pdxlop());

	CaseExpr *pcaseexpr = MakeNode(CaseExpr);
	pcaseexpr->casetype = CMDIdGPDB::PmdidConvert(pdxlopIfStmt->PmdidResultType())->OidObjectId();

	CDXLNode *pdxlnCurr = const_cast<CDXLNode*>(pdxlnIfStmt);
	Expr *pexprElse = NULL;

	// An If statement is of the format: IF <condition> <then> <else>
	// The leaf else statement is the def result of the case statement
	BOOL fLeafElseStatement = false;

	while (!fLeafElseStatement)
	{

		if (3 != pdxlnCurr->UlArity())
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

		if (EdxlopScalarIfStmt == (*pdxlnCurr)[2]->Pdxlop()->Edxlop())
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
	CDXLScalarSwitch *pdxlop = CDXLScalarSwitch::PdxlopConvert(pdxlnSwitch->Pdxlop());

	CaseExpr *pcaseexpr = MakeNode(CaseExpr);
	pcaseexpr->casetype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidType())->OidObjectId();

	// translate arg child
	pcaseexpr->arg = PexprFromDXLNodeScalar((*pdxlnSwitch)[0], pmapcidvar);
	GPOS_ASSERT(NULL != pcaseexpr->arg);

	const ULONG ulArity = pdxlnSwitch->UlArity();
	GPOS_ASSERT(1 < ulArity);
	for (ULONG ul = 1; ul < ulArity; ul++)
	{
		const CDXLNode *pdxlnChild = (*pdxlnSwitch)[ul];

		if (EdxlopScalarSwitchCase == pdxlnChild->Pdxlop()->Edxlop())
		{
			CaseWhen *pwhen = MakeNode(CaseWhen);
			pwhen->expr = PexprFromDXLNodeScalar((*pdxlnChild)[0], pmapcidvar);
			pwhen->result = PexprFromDXLNodeScalar((*pdxlnChild)[1], pmapcidvar);
			pcaseexpr->args = gpdb::PlAppendElement(pcaseexpr->args, pwhen);
		}
		else
		{
			// default return value
			GPOS_ASSERT(ul == ulArity - 1);
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
	CDXLScalarCaseTest *pdxlop = CDXLScalarCaseTest::PdxlopConvert(pdxlnCaseTest->Pdxlop());

	CaseTestExpr *pcasetestexpr = MakeNode(CaseTestExpr);
	pcasetestexpr->typeId = CMDIdGPDB::PmdidConvert(pdxlop->PmdidType())->OidObjectId();
	pcasetestexpr->typeMod = -1;

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
	CDXLScalarOpExpr *pdxlopOpExpr = CDXLScalarOpExpr::PdxlopConvert(pdxlnOpExpr->Pdxlop());

	OpExpr *popexpr = MakeNode(OpExpr);
	popexpr->opno = CMDIdGPDB::PmdidConvert(pdxlopOpExpr->Pmdid())->OidObjectId();

	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pdxlopOpExpr->Pmdid());
	popexpr->opfuncid = CMDIdGPDB::PmdidConvert(pmdscop->PmdidFunc())->OidObjectId();
	
	IMDId *pmdidReturnType = pdxlopOpExpr->PmdidReturnType();
	if (NULL != pmdidReturnType)
	{
		popexpr->opresulttype = CMDIdGPDB::PmdidConvert(pmdidReturnType)->OidObjectId();
	}
	else 
	{
		popexpr->opresulttype = OidFunctionReturnType(pmdscop->PmdidFunc());
	}

	const IMDFunction *pmdfunc = m_pmda->Pmdfunc(pmdscop->PmdidFunc());
	popexpr->opretset = pmdfunc->FReturnsSet();

	GPOS_ASSERT(1 == pdxlnOpExpr->UlArity() || 2 == pdxlnOpExpr->UlArity());

	// translate children
	popexpr->args = PlistTranslateScalarChildren(popexpr->args, pdxlnOpExpr, pmapcidvar);

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
	CDXLScalarArrayComp *pdxlopArrayComp = CDXLScalarArrayComp::PdxlopConvert(pdxlnArrayComp->Pdxlop());

	ScalarArrayOpExpr *parrayopexpr = MakeNode(ScalarArrayOpExpr);
	parrayopexpr->opno = CMDIdGPDB::PmdidConvert(pdxlopArrayComp->Pmdid())->OidObjectId();
	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pdxlopArrayComp->Pmdid());
	parrayopexpr->opfuncid = CMDIdGPDB::PmdidConvert(pmdscop->PmdidFunc())->OidObjectId();

	switch(pdxlopArrayComp->Edxlarraycomptype())
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

	GPOS_ASSERT(2 == pdxlnArrayComp->UlArity());

	CDXLNode *pdxlnLeft = (*pdxlnArrayComp)[EdxlsccmpIndexLeft];
	CDXLNode *pdxlnRight = (*pdxlnArrayComp)[EdxlsccmpIndexRight];

	Expr *pexprLeft = PexprFromDXLNodeScalar(pdxlnLeft, pmapcidvar);
	Expr *pexprRight = PexprFromDXLNodeScalar(pdxlnRight, pmapcidvar);

	parrayopexpr->args = ListMake2(pexprLeft, pexprRight);

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
	CDXLScalarDistinctComp *pdxlop = CDXLScalarDistinctComp::PdxlopConvert(pdxlnDistComp->Pdxlop());

	DistinctExpr *pdistexpr = MakeNode(DistinctExpr);
	pdistexpr->opno = CMDIdGPDB::PmdidConvert(pdxlop->Pmdid())->OidObjectId();

	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pdxlop->Pmdid());

	pdistexpr->opfuncid = CMDIdGPDB::PmdidConvert(pmdscop->PmdidFunc())->OidObjectId();
	pdistexpr->opresulttype = OidFunctionReturnType(pmdscop->PmdidFunc());

	// translate left and right child
	GPOS_ASSERT(2 == pdxlnDistComp->UlArity());
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
	CDXLScalarAggref *pdxlop = CDXLScalarAggref::PdxlopConvert(pdxlnAggref->Pdxlop());

	Aggref *paggref = MakeNode(Aggref);
	paggref->aggfnoid = CMDIdGPDB::PmdidConvert(pdxlop->PmdidAgg())->OidObjectId();
	paggref->aggdistinct = NIL;
	paggref->agglevelsup = 0;
	paggref->aggkind = 'n';
	paggref->location = -1;

	CMDIdGPDB *pmdidAgg = GPOS_NEW(m_pmp) CMDIdGPDB(paggref->aggfnoid);
	const IMDAggregate *pmdagg = m_pmda->Pmdagg(pmdidAgg);
	pmdidAgg->Release();

	EdxlAggrefStage edxlaggstage = pdxlop->Edxlaggstage();
	if (NULL != pdxlop->PmdidResolvedRetType())
	{
		// use resolved type
		paggref->aggtype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidResolvedRetType())->OidObjectId();
	}
	else if (EdxlaggstageIntermediate == edxlaggstage || EdxlaggstagePartial == edxlaggstage)
	{
		paggref->aggtype = CMDIdGPDB::PmdidConvert(pmdagg->PmdidTypeIntermediate())->OidObjectId();
	}
	else
	{
		paggref->aggtype = CMDIdGPDB::PmdidConvert(pmdagg->PmdidTypeResult())->OidObjectId();
	}

	switch(pdxlop->Edxlaggstage())
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
	ListCell *plc;
	int sortgrpindex = 1;
	ForEachWithCount (plc, argExprs, attno)
	{
		TargetEntry *pteNew = gpdb::PteMakeTargetEntry((Expr *) lfirst(plc), attno + 1, NULL, false);
		/*
		 * Translate the aggdistinct bool set to true (in ORCA),
		 * to a List of SortGroupClause in the PLNSTMT
		 */
		if(pdxlop->FDistinct())
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
	CDXLScalarWindowRef *pdxlop = CDXLScalarWindowRef::PdxlopConvert(pdxlnWinref->Pdxlop());

	WindowFunc *pwindowfunc = MakeNode(WindowFunc);
	pwindowfunc->winfnoid = CMDIdGPDB::PmdidConvert(pdxlop->PmdidFunc())->OidObjectId();

	// GPDB_84_MERGE_FIXME: The OIDS of a few built-in window
	// functions have been hard-coded in ORCA. But the OIDs
	// were changed when we merged the upstream window function
	// implementation, to match the upstream OIDs. Map the old
	// OIDs to the upstream ones.
	if (pwindowfunc->winfnoid == 7000)	// ROW_NUMBER()
		pwindowfunc->winfnoid = 3100;
	if (pwindowfunc->winfnoid == 7002)	// DENSE_RANK()
		pwindowfunc->winfnoid = 3102;
	if (pwindowfunc->winfnoid == 7003)	// PERCENT_RANK()
		pwindowfunc->winfnoid = 3103;
	if (pwindowfunc->winfnoid == 7004)	// CUME_DIST()
		pwindowfunc->winfnoid = 3104;
	if (pwindowfunc->winfnoid == 7005)	// NTILE(int4)
		pwindowfunc->winfnoid = 3105;

	pwindowfunc->windistinct = pdxlop->FDistinct();
	pwindowfunc->location = -1;
	pwindowfunc->winref = pdxlop->UlWinSpecPos() + 1;
	pwindowfunc->wintype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidRetType())->OidObjectId();
	pwindowfunc->winstar = pdxlop->FStarArg();
	pwindowfunc->winagg = pdxlop->FSimpleAgg();

	EdxlWinStage edxlwinstage = pdxlop->Edxlwinstage();
	GPOS_ASSERT(edxlwinstage != EdxlwinstageSentinel);

	ULONG rgrgulMapping[][2] =
			{
			{WINSTAGE_IMMEDIATE, EdxlwinstageImmediate},
			{WINSTAGE_PRELIMINARY, EdxlwinstagePreliminary},
			{WINSTAGE_ROWKEY, EdxlwinstageRowKey},
			};

	const ULONG ulArity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) edxlwinstage == pulElem[1])
		{
			pwindowfunc->winstage = (WinStage) pulElem[0];
			break;
		}
	}

	// translate the arguments of the window function
	pwindowfunc->args = PlistTranslateScalarChildren(pwindowfunc->args, pdxlnWinref, pmapcidvar);

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
	CDXLScalarFuncExpr *pdxlop = CDXLScalarFuncExpr::PdxlopConvert(pdxlnFuncExpr->Pdxlop());

	FuncExpr *pfuncexpr = MakeNode(FuncExpr);
	pfuncexpr->funcid = CMDIdGPDB::PmdidConvert(pdxlop->PmdidFunc())->OidObjectId();
	pfuncexpr->funcretset = pdxlop->FReturnSet();
	pfuncexpr->funcformat = COERCE_DONTCARE;
	pfuncexpr->funcresulttype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidRetType())->OidObjectId();
	pfuncexpr->args = PlistTranslateScalarChildren(pfuncexpr->args, pdxlnFuncExpr, pmapcidvar);

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
	CDXLTranslateContext *pdxltrctxOut = (dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar))->PpdxltrctxOut();

	CContextDXLToPlStmt *pctxdxltoplstmt = (dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar))->Pctxdxltoplstmt();

	CDXLScalarSubPlan *pdxlop = CDXLScalarSubPlan::PdxlopConvert(pdxlnSubPlan->Pdxlop());

	// translate subplan test expression
	List *plparamIds = NIL;

	SubLinkType slink = CTranslatorUtils::Slink(pdxlop->Edxlsptype());
	Expr *pexprTestExpr = PexprSubplanTestExpr(pdxlop->PdxlnTestExpr(), slink, pmapcidvar, &plparamIds);

	const DrgPdxlcr *pdrgdxlcrOuterRefs = pdxlop->DrgdxlcrOuterRefs();
	const DrgPmdid *pdrgmdidOuterRefs = pdxlop->DrgmdidOuterRefs();

	const ULONG ulLen = pdrgdxlcrOuterRefs->UlLength();

	// create a copy of the translate context: the param mappings from the outer scope get copied in the constructor
	CDXLTranslateContext dxltrctxSubplan(m_pmp, pdxltrctxOut->FParentAggNode(), pdxltrctxOut->PhmColParam());

	// insert new outer ref mappings in the subplan translate context
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		IMDId *pmdid = (*pdrgmdidOuterRefs)[ul];
		CDXLColRef *pdxlcr = (*pdrgdxlcrOuterRefs)[ul];
		ULONG ulColid = pdxlcr->UlID();

		if (NULL == dxltrctxSubplan.Pmecolidparamid(ulColid))
		{
			// keep outer reference mapping to the original column for subsequent subplans
			CMappingElementColIdParamId *pmecolidparamid = GPOS_NEW(m_pmp) CMappingElementColIdParamId(ulColid, pctxdxltoplstmt->UlNextParamId(), pmdid);

#ifdef GPOS_DEBUG
			BOOL fInserted =
#endif
			dxltrctxSubplan.FInsertParamMapping(ulColid, pmecolidparamid);
			GPOS_ASSERT(fInserted);
		}
	}

	CDXLNode *pdxlnChild = (*pdxlnSubPlan)[0];
        GPOS_ASSERT(EdxloptypePhysical == pdxlnChild->Pdxlop()->Edxloperatortype());

	GPOS_ASSERT(NULL != pdxlnSubPlan);
	GPOS_ASSERT(EdxlopScalarSubPlan == pdxlnSubPlan->Pdxlop()->Edxlop());
	GPOS_ASSERT(1 == pdxlnSubPlan->UlArity());

	// generate the child plan,
	// create DXL->PlStmt translator to handle subplan's relational children
	CTranslatorDXLToPlStmt trdxltoplstmt
							(
							m_pmp,
							m_pmda,
							(dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar))->Pctxdxltoplstmt(),
							m_ulSegments
							);
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings = GPOS_NEW(m_pmp) DrgPdxltrctx(m_pmp);
	Plan *pplanChild = trdxltoplstmt.PplFromDXL(pdxlnChild, &dxltrctxSubplan, pdrgpdxltrctxPrevSiblings);
	pdrgpdxltrctxPrevSiblings->Release();

	GPOS_ASSERT(NULL != pplanChild->targetlist && 1 <= gpdb::UlListLength(pplanChild->targetlist));

	// translate subplan and set test expression
	SubPlan *psubplan = PsubplanFromChildPlan(pplanChild, slink, pctxdxltoplstmt);
	psubplan->testexpr = (Node *) pexprTestExpr;
	psubplan->paramIds = plparamIds;

	// translate other subplan params
	TranslateSubplanParams(psubplan, &dxltrctxSubplan, pdrgdxlcrOuterRefs, pmapcidvar);

	return (Expr *)psubplan;
}

inline BOOL FDXLCastedId(CDXLNode *pdxln)
{
	return EdxlopScalarCast == pdxln->Pdxlop()->Edxlop() &&
		   pdxln->UlArity() > 0 && EdxlopScalarIdent == (*pdxln)[0]->Pdxlop()->Edxlop();
}

inline Oid OidParamOidFromDXLIdentOrDXLCastIdent(CDXLNode *pdxlnIdentOrCastIdent)
{
	GPOS_ASSERT(EdxlopScalarIdent == pdxlnIdentOrCastIdent->Pdxlop()->Edxlop() || FDXLCastedId(pdxlnIdentOrCastIdent));

	CDXLScalarIdent *pdxlopInnerIdent;
	if (EdxlopScalarIdent == pdxlnIdentOrCastIdent->Pdxlop()->Edxlop())
	{
		pdxlopInnerIdent = CDXLScalarIdent::PdxlopConvert(pdxlnIdentOrCastIdent->Pdxlop());
	}
	else
	{
		pdxlopInnerIdent = CDXLScalarIdent::PdxlopConvert((*pdxlnIdentOrCastIdent)[0]->Pdxlop());
	}
	Oid oidInnerType = CMDIdGPDB::PmdidConvert(pdxlopInnerIdent->PmdidType())->OidObjectId();
	return oidInnerType;
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
	CDXLNode *pdxlnTestExpr,
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
	GPOS_ASSERT(NULL != pdxlnTestExpr);

	if (FConstTrue(pdxlnTestExpr, m_pmda))
	{
		// dummy test expression
		return (Expr *) PconstFromDXLNodeScConst(pdxlnTestExpr, NULL);
	}

	if (EdxlopScalarCmp != pdxlnTestExpr->Pdxlop()->Edxlop())
	{
		// test expression is expected to be a comparison
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,  GPOS_WSZ_LIT("Unexpected subplan test expression"));
	}

	GPOS_ASSERT(2 == pdxlnTestExpr->UlArity());
	GPOS_ASSERT(ANY_SUBLINK == slink || ALL_SUBLINK == slink);

	CDXLNode *pdxlnOuterChild = (*pdxlnTestExpr)[0];
	CDXLNode *pdxlnInnerChild = (*pdxlnTestExpr)[1];

	if (EdxlopScalarIdent != pdxlnInnerChild->Pdxlop()->Edxlop() && !FDXLCastedId(pdxlnInnerChild))
	{
		// test expression is expected to be a comparison between an outer expression 
		// and a scalar identifier from subplan child
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,  GPOS_WSZ_LIT("Unexpected subplan test expression"));
	}

	// extract type of inner column
        CDXLScalarComp *pdxlopCmp = CDXLScalarComp::PdxlopConvert(pdxlnTestExpr->Pdxlop());

		// create an OpExpr for subplan test expression
        OpExpr *popexpr = MakeNode(OpExpr);
        popexpr->opno = CMDIdGPDB::PmdidConvert(pdxlopCmp->Pmdid())->OidObjectId();
        const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pdxlopCmp->Pmdid());
        popexpr->opfuncid = CMDIdGPDB::PmdidConvert(pmdscop->PmdidFunc())->OidObjectId();
        popexpr->opresulttype = CMDIdGPDB::PmdidConvert(m_pmda->PtMDType<IMDTypeBool>()->Pmdid())->OidObjectId();
        popexpr->opretset = false;

        // translate outer expression (can be a deep scalar tree)
        Expr *pexprTestExprOuterArg = PexprFromDXLNodeScalar(pdxlnOuterChild, pmapcidvar);

        // add translated outer expression as first arg of OpExpr
        List *plistArgs = NIL;
        plistArgs = gpdb::PlAppendElement(plistArgs, pexprTestExprOuterArg);

	// second arg must be an EXEC param which is replaced during query execution with subplan output
	Param *pparam = MakeNode(Param);
	pparam->paramkind = PARAM_EXEC;
	CContextDXLToPlStmt *pctxdxltoplstmt = (dynamic_cast<CMappingColIdVarPlStmt *>(pmapcidvar))->Pctxdxltoplstmt();
	pparam->paramid = pctxdxltoplstmt->UlNextParamId();
	pparam->paramtype = OidParamOidFromDXLIdentOrDXLCastIdent(pdxlnInnerChild);

	// test expression is used for non-scalar subplan,
	// second arg of test expression must be an EXEC param referring to subplan output,
	// we add this param to subplan param ids before translating other params

	*plparamIds = gpdb::PlAppendInt(*plparamIds, pparam->paramid);

	if (EdxlopScalarIdent == pdxlnInnerChild->Pdxlop()->Edxlop())
	{
		plistArgs = gpdb::PlAppendElement(plistArgs, pparam);
	}
	else // we have a cast
	{
		CDXLScalarCast *pdxlScalaCast = CDXLScalarCast::PdxlopConvert(pdxlnInnerChild->Pdxlop());
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
        const ULONG ulSize = pdrgdxlcrOuterRefs->UlLength();
        for (ULONG ul = 0; ul < ulSize; ul++)
        {
                CDXLColRef *pdxlcr = (*pdrgdxlcrOuterRefs)[ul];
                pdxlcr->AddRef();
                const CMappingElementColIdParamId *pmecolidparamid = pdxltrctx->Pmecolidparamid(pdxlcr->UlID());

                Param *pparam = PparamFromMapping(pmecolidparamid);
                psubplan->parParam = gpdb::PlAppendInt(psubplan->parParam, pparam->paramid);

                IMDId *pmdidType = pmecolidparamid->PmdidType();
                pmdidType->AddRef();

                CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_pmp) CDXLScalarIdent(m_pmp, pdxlcr, pmdidType);
                Expr *parg = (Expr *) pmapcidvar->PvarFromDXLNodeScId(pdxlopIdent);

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
	CContextDXLToPlStmt *pctxdxltoplstmt
	)
{
	pctxdxltoplstmt->AddSubplan(pplan);

	SubPlan *psubplan = MakeNode(SubPlan);
	psubplan->plan_id = gpdb::UlListLength(pctxdxltoplstmt->PlPplanSubplan());
	psubplan->plan_name = SzSubplanAlias(psubplan->plan_id);
	psubplan->is_initplan = false;
	psubplan->firstColType = gpdb::OidExprType( (Node*) ((TargetEntry*) gpdb::PvListNth(pplan->targetlist, 0))->expr);
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
	CWStringDynamic *pstr = GPOS_NEW(m_pmp) CWStringDynamic(m_pmp);
	pstr->AppendFormat(GPOS_WSZ_LIT("SubPlan %d"), ulPlanId);
	const WCHAR *wsz = pstr->Wsz();

	ULONG ulMaxLength = (GPOS_WSZ_LENGTH(wsz) + 1) * GPOS_SIZEOF(WCHAR);
	CHAR *sz = (CHAR *) gpdb::GPDBAlloc(ulMaxLength);
	gpos::clib::LWcsToMbs(sz, const_cast<WCHAR *>(wsz), ulMaxLength);
	sz[ulMaxLength - 1] = '\0';
	GPOS_DELETE(pstr);

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
	pparam->paramid = pmecolidparamid->UlParamId();
	pparam->paramkind = PARAM_EXEC;
	pparam->paramtype = CMDIdGPDB::PmdidConvert(pmecolidparamid->PmdidType())->OidObjectId();

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
	CDXLScalarBoolExpr *pdxlop = CDXLScalarBoolExpr::PdxlopConvert(pdxlnBoolExpr->Pdxlop());
	BoolExpr *pboolexpr = MakeNode(BoolExpr);

	GPOS_ASSERT(1 <= pdxlnBoolExpr->UlArity());
	switch (pdxlop->EdxlBoolType())
	{
		case Edxlnot:
		{
			GPOS_ASSERT(1 == pdxlnBoolExpr->UlArity());
			pboolexpr->boolop = NOT_EXPR;
			break;
		}
		case Edxland:
		{
			GPOS_ASSERT(2 <= pdxlnBoolExpr->UlArity());
			pboolexpr->boolop = AND_EXPR;
			break;
		}
		case Edxlor:
		{
			GPOS_ASSERT(2 <= pdxlnBoolExpr->UlArity());
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
	CDXLScalarBooleanTest *pdxlop = CDXLScalarBooleanTest::PdxlopConvert(pdxlnBooleanTest->Pdxlop());
	BooleanTest *pbooleantest = MakeNode(BooleanTest);

	switch (pdxlop->EdxlBoolType())
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

	GPOS_ASSERT(1 == pdxlnBooleanTest->UlArity());
	CDXLNode *pdxlnArg = (*pdxlnBooleanTest)[0];

	Expr *pexprArg = PexprFromDXLNodeScalar(pdxlnArg, pmapcidvar);
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
	CDXLScalarNullTest *pdxlop = CDXLScalarNullTest::PdxlopConvert(pdxlnNullTest->Pdxlop());
	NullTest *pnulltest = MakeNode(NullTest);

	GPOS_ASSERT(1 == pdxlnNullTest->UlArity());
	CDXLNode *pdxlnChild = (*pdxlnNullTest)[0];
	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	if (pdxlop->FIsNullTest())
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
	CDXLScalarNullIf *pdxlop = CDXLScalarNullIf::PdxlopConvert(pdxlnNullIf->Pdxlop());

	NullIfExpr *pnullifexpr = MakeNode(NullIfExpr);
	pnullifexpr->opno = CMDIdGPDB::PmdidConvert(pdxlop->PmdidOp())->OidObjectId();

	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pdxlop->PmdidOp());

	pnullifexpr->opfuncid = CMDIdGPDB::PmdidConvert(pmdscop->PmdidFunc())->OidObjectId();
	pnullifexpr->opresulttype = OidFunctionReturnType(pmdscop->PmdidFunc());
	pnullifexpr->opretset = false;

	// translate children
	GPOS_ASSERT(2 == pdxlnNullIf->UlArity());
	pnullifexpr->args = PlistTranslateScalarChildren(pnullifexpr->args, pdxlnNullIf, pmapcidvar);

	return (Expr *) pnullifexpr;
}

Expr *
CTranslatorDXLToScalar::PrelabeltypeOrFuncexprFromDXLNodeScalarCast(const CDXLScalarCast *pdxlscalarcast, Expr *pexprChild)
{
	if (IMDId::FValid(pdxlscalarcast->PmdidFunc()))
	{
		FuncExpr *pfuncexpr = MakeNode(FuncExpr);
		pfuncexpr->funcid = CMDIdGPDB::PmdidConvert(pdxlscalarcast->PmdidFunc())->OidObjectId();

		const IMDFunction *pmdfunc = m_pmda->Pmdfunc(pdxlscalarcast->PmdidFunc());
		pfuncexpr->funcretset = pmdfunc->FReturnsSet();;

		pfuncexpr->funcformat = COERCE_IMPLICIT_CAST;
		pfuncexpr->funcresulttype = CMDIdGPDB::PmdidConvert(pdxlscalarcast->PmdidType())->OidObjectId();

		pfuncexpr->args = NIL;
		pfuncexpr->args = gpdb::PlAppendElement(pfuncexpr->args, pexprChild);

		return (Expr *) pfuncexpr;
	}

	RelabelType *prelabeltype = MakeNode(RelabelType);

	prelabeltype->resulttype = CMDIdGPDB::PmdidConvert(pdxlscalarcast->PmdidType())->OidObjectId();
	prelabeltype->arg = pexprChild;
	prelabeltype->resulttypmod = -1;
	prelabeltype->location = -1;
	prelabeltype->relabelformat = COERCE_DONTCARE;

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
	const CDXLScalarCast *pdxlop = CDXLScalarCast::PdxlopConvert(pdxlnCast->Pdxlop());

	GPOS_ASSERT(1 == pdxlnCast->UlArity());
	CDXLNode *pdxlnChild = (*pdxlnCast)[0];

	Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

	return PrelabeltypeOrFuncexprFromDXLNodeScalarCast(pdxlop, pexprChild);
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
        CDXLScalarCoerceToDomain *pdxlop = CDXLScalarCoerceToDomain::PdxlopConvert(pdxlnCoerce->Pdxlop());

        GPOS_ASSERT(1 == pdxlnCoerce->UlArity());
        CDXLNode *pdxlnChild = (*pdxlnCoerce)[0];

        Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);


        CoerceToDomain *pcoerce = MakeNode(CoerceToDomain);

        pcoerce->resulttype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidResultType())->OidObjectId();
        pcoerce->arg = pexprChild;
        pcoerce->resulttypmod = pdxlop->IMod();
        pcoerce->location = pdxlop->ILoc();
        pcoerce->coercionformat = (CoercionForm)  pdxlop->Edxlcf();

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
        CDXLScalarCoerceViaIO *pdxlop = CDXLScalarCoerceViaIO::PdxlopConvert(pdxlnCoerce->Pdxlop());

        GPOS_ASSERT(1 == pdxlnCoerce->UlArity());
        CDXLNode *pdxlnChild = (*pdxlnCoerce)[0];

        Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

        CoerceViaIO *pcoerce = MakeNode(CoerceViaIO);

        pcoerce->resulttype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidResultType())->OidObjectId();
        pcoerce->arg = pexprChild;
        pcoerce->location = pdxlop->ILoc();
        pcoerce->coerceformat = (CoercionForm)  pdxlop->Edxlcf();

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
        CDXLScalarArrayCoerceExpr *pdxlop = CDXLScalarArrayCoerceExpr::PdxlopConvert(pdxlnCoerce->Pdxlop());

        GPOS_ASSERT(1 == pdxlnCoerce->UlArity());
        CDXLNode *pdxlnChild = (*pdxlnCoerce)[0];

        Expr *pexprChild = PexprFromDXLNodeScalar(pdxlnChild, pmapcidvar);

        ArrayCoerceExpr *pcoerce = MakeNode(ArrayCoerceExpr);

        pcoerce->arg = pexprChild;
        pcoerce->elemfuncid = CMDIdGPDB::PmdidConvert(pdxlop->PmdidElementFunc())->OidObjectId();
        pcoerce->resulttype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidResultType())->OidObjectId();
        pcoerce->resulttypmod = pdxlop->IMod();
        pcoerce->isExplicit = pdxlop->FIsExplicit();
        pcoerce->coerceformat = (CoercionForm)  pdxlop->Edxlcf();
        pcoerce->location = pdxlop->ILoc();

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
	CDXLScalarCoalesce *pdxlop = CDXLScalarCoalesce::PdxlopConvert(pdxlnCoalesce->Pdxlop());
	CoalesceExpr *pcoalesce = MakeNode(CoalesceExpr);

	pcoalesce->coalescetype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidType())->OidObjectId();
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
	CDXLScalarMinMax *pdxlop = CDXLScalarMinMax::PdxlopConvert(pdxlnMinMax->Pdxlop());
	MinMaxExpr *pminmax = MakeNode(MinMaxExpr);

	pminmax->minmaxtype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidType())->OidObjectId();
	pminmax->args = PlistTranslateScalarChildren(pminmax->args, pdxlnMinMax, pmapcidvar);
	pminmax->location = -1;

	CDXLScalarMinMax::EdxlMinMaxType emmt = pdxlop->Emmt();
	if (CDXLScalarMinMax::EmmtMax == emmt)
	{
		pminmax->op = IS_GREATEST;
	}
	else
	{
		GPOS_ASSERT(CDXLScalarMinMax::EmmtMin == emmt);
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
	const CDXLNode *pdxln,
	CMappingColIdVar *pmapcidvar
	)
{
	List *plistNew = plist;

	const ULONG ulArity = pdxln->UlArity();
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		CDXLNode *pdxlnChild = (*pdxln)[ul];
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
	CDXLScalarConstValue *pdxlop = CDXLScalarConstValue::PdxlopConvert(pdxlnConst->Pdxlop());
	CDXLDatum *pdxldatum = const_cast<CDXLDatum*>(pdxlop->Pdxldatum());

	return PconstFromDXLDatum(pdxldatum);
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
	CDXLDatum *pdxldatum
	)
{
	GPOS_ASSERT(NULL != pdxldatum);
	
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
	CDXLDatum::EdxldatumType edxldatumtype = pdxldatum->Edxldt();

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
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, CDXLTokens::PstrToken(EdxltokenScalarConstValue)->Wsz());
	}

	return (Expr*) (this->*pf)(pdxldatum);
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
	CDXLDatum *pdxldatum
	)
{
	CDXLDatumOid *pdxldatumOid = CDXLDatumOid::PdxldatumConvert(pdxldatum);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::PmdidConvert(pdxldatumOid->Pmdid())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constbyval = pdxldatumOid->FByValue();
	pconst->constisnull = pdxldatumOid->FNull();
	pconst->constlen = pdxldatumOid->UlLength();

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
	CDXLDatum *pdxldatum
	)
{
	CDXLDatumInt2 *pdxldatumint2 = CDXLDatumInt2::PdxldatumConvert(pdxldatum);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::PmdidConvert(pdxldatumint2->Pmdid())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constbyval = pdxldatumint2->FByValue();
	pconst->constisnull = pdxldatumint2->FNull();
	pconst->constlen = pdxldatumint2->UlLength();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt16(pdxldatumint2->SValue());
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
	CDXLDatum *pdxldatum
	)
{
	CDXLDatumInt4 *pdxldatumint4 = CDXLDatumInt4::PdxldatumConvert(pdxldatum);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::PmdidConvert(pdxldatumint4->Pmdid())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constbyval = pdxldatumint4->FByValue();
	pconst->constisnull = pdxldatumint4->FNull();
	pconst->constlen = pdxldatumint4->UlLength();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt32(pdxldatumint4->IValue());
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
	CDXLDatum *pdxldatum
	)
{
	CDXLDatumInt8 *pdxldatumint8 = CDXLDatumInt8::PdxldatumConvert(pdxldatum);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::PmdidConvert(pdxldatumint8->Pmdid())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constbyval = pdxldatumint8->FByValue();
	pconst->constisnull = pdxldatumint8->FNull();
	pconst->constlen = pdxldatumint8->UlLength();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromInt64(pdxldatumint8->LValue());
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
	CDXLDatum *pdxldatum
	)
{
	CDXLDatumBool *pdxldatumbool = CDXLDatumBool::PdxldatumConvert(pdxldatum);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::PmdidConvert(pdxldatumbool->Pmdid())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constbyval = pdxldatumbool->FByValue();
	pconst->constisnull = pdxldatumbool->FNull();
	pconst->constlen = pdxldatumbool->UlLength();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else
	{
		pconst->constvalue = gpdb::DDatumFromBool(pdxldatumbool->FValue());
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
	CDXLDatum *pdxldatum
	)
{
	CDXLDatumGeneric *pdxldatumgeneric = CDXLDatumGeneric::PdxldatumConvert(pdxldatum);

	Const *pconst = MakeNode(Const);
	pconst->consttype = CMDIdGPDB::PmdidConvert(pdxldatumgeneric->Pmdid())->OidObjectId();
	pconst->consttypmod = -1;
	pconst->constbyval = pdxldatumgeneric->FByValue();
	pconst->constisnull = pdxldatumgeneric->FNull();
	pconst->constlen = pdxldatumgeneric->UlLength();

	if (pconst->constisnull)
	{
		pconst->constvalue = (Datum) 0;
	}
	else if (pconst->constbyval)
	{
		// if it is a by-value constant, the value is stored in the datum.
		GPOS_ASSERT(pconst->constlen >= 0);
		GPOS_ASSERT((ULONG) pconst->constlen <= sizeof(Datum));
		memcpy(&pconst->constvalue, pdxldatumgeneric->Pba(), sizeof(Datum));
	}
	else
	{
		Datum dVal = gpdb::DDatumFromPointer(pdxldatumgeneric->Pba());
		ULONG ulLength = (ULONG) gpdb::SDatumSize(dVal, false, pconst->constlen);

		CHAR *pcsz = (CHAR *) gpdb::GPDBAlloc(ulLength + 1);
		memcpy(pcsz, pdxldatumgeneric->Pba(), ulLength);
		pcsz[ulLength] = '\0';
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
	CDXLScalarPartDefault *pdxlop = CDXLScalarPartDefault::PdxlopConvert(pdxlnPartDefault->Pdxlop());

	PartDefaultExpr *pexpr = MakeNode(PartDefaultExpr);
	pexpr->level = pdxlop->UlLevel();

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
	CDXLScalarPartBound *pdxlop = CDXLScalarPartBound::PdxlopConvert(pdxlnPartBound->Pdxlop());

	PartBoundExpr *pexpr = MakeNode(PartBoundExpr);
	pexpr->level = pdxlop->UlLevel();
	pexpr->boundType = CMDIdGPDB::PmdidConvert(pdxlop->PmdidType())->OidObjectId();
	pexpr->isLowerBound = pdxlop->FLower();

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
	CDXLScalarPartBoundInclusion *pdxlop = CDXLScalarPartBoundInclusion::PdxlopConvert(pdxlnPartBoundIncl->Pdxlop());

	PartBoundInclusionExpr *pexpr = MakeNode(PartBoundInclusionExpr);
	pexpr->level = pdxlop->UlLevel();
	pexpr->isLowerBound = pdxlop->FLower();

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
	CDXLScalarPartBoundOpen *pdxlop = CDXLScalarPartBoundOpen::PdxlopConvert(pdxlnPartBoundOpen->Pdxlop());

	PartBoundOpenExpr *pexpr = MakeNode(PartBoundOpenExpr);
	pexpr->level = pdxlop->UlLevel();
	pexpr->isLowerBound = pdxlop->FLower();

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
	CDXLScalarPartListValues *pdxlop = CDXLScalarPartListValues::PdxlopConvert(pdxlnPartListValues->Pdxlop());

	PartListRuleExpr *pexpr = MakeNode(PartListRuleExpr);
	pexpr->level = pdxlop->UlLevel();
	pexpr->resulttype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidResult())->OidObjectId();
	pexpr->elementtype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidElement())->OidObjectId();

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
	CDXLScalarPartListNullTest *pdxlop = CDXLScalarPartListNullTest::PdxlopConvert(pdxlnPartListNullTest->Pdxlop());

	PartListNullTestExpr *pexpr = MakeNode(PartListNullTestExpr);
	pexpr->level = pdxlop->UlLevel();
	pexpr->nulltesttype = pdxlop->FIsNull() ? IS_NULL : IS_NOT_NULL;

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
	const CDXLNode *pdxlnIdent,
	CMappingColIdVar *pmapcidvar
	)
{
	CMappingColIdVarPlStmt *pmapcidvarplstmt = dynamic_cast<CMappingColIdVarPlStmt*>(pmapcidvar);

	// scalar identifier
	CDXLScalarIdent *pdxlop = CDXLScalarIdent::PdxlopConvert(pdxlnIdent->Pdxlop());
	Expr *pexprResult = NULL;
	if (NULL == pmapcidvarplstmt || NULL == pmapcidvarplstmt->PpdxltrctxOut()->Pmecolidparamid(pdxlop->Pdxlcr()->UlID()))
	{
		// not an outer ref -> create var node
		pexprResult = (Expr *) pmapcidvar->PvarFromDXLNodeScId(pdxlop);
	}
	else
	{
		// outer ref -> create param node
		pexprResult = (Expr *) pmapcidvarplstmt->PparamFromDXLNodeScId(pdxlop);
	}

	if (NULL  == pexprResult)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, pdxlop->Pdxlcr()->UlID());
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
	CDXLScalarComp *pdxlop = CDXLScalarComp::PdxlopConvert(pdxlnCmp->Pdxlop());

	OpExpr *popexpr = MakeNode(OpExpr);
	popexpr->opno = CMDIdGPDB::PmdidConvert(pdxlop->Pmdid())->OidObjectId();

	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pdxlop->Pmdid());

	popexpr->opfuncid = CMDIdGPDB::PmdidConvert(pmdscop->PmdidFunc())->OidObjectId();
	popexpr->opresulttype = CMDIdGPDB::PmdidConvert(m_pmda->PtMDType<IMDTypeBool>()->Pmdid())->OidObjectId();
	popexpr->opretset = false;

	// translate left and right child
	GPOS_ASSERT(2 == pdxlnCmp->UlArity());

	CDXLNode *pdxlnLeft = (*pdxlnCmp)[EdxlsccmpIndexLeft];
	CDXLNode *pdxlnRight = (*pdxlnCmp)[EdxlsccmpIndexRight];

	Expr *pexprLeft = PexprFromDXLNodeScalar(pdxlnLeft, pmapcidvar);
	Expr *pexprRight = PexprFromDXLNodeScalar(pdxlnRight, pmapcidvar);

	popexpr->args = ListMake2(pexprLeft, pexprRight);

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
	CDXLScalarArray *pdxlop = CDXLScalarArray::PdxlopConvert(pdxlnArray->Pdxlop());

	ArrayExpr *pexpr = MakeNode(ArrayExpr);
	pexpr->element_typeid = CMDIdGPDB::PmdidConvert(pdxlop->PmdidElem())->OidObjectId();
	pexpr->array_typeid = CMDIdGPDB::PmdidConvert(pdxlop->PmdidArray())->OidObjectId();
	pexpr->multidims = pdxlop->FMultiDimensional();
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
	CDXLScalarArrayRef *pdxlop = CDXLScalarArrayRef::PdxlopConvert(pdxlnArrayref->Pdxlop());

	ArrayRef *parrayref = MakeNode(ArrayRef);
	parrayref->refarraytype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidArray())->OidObjectId();
	parrayref->refelemtype = CMDIdGPDB::PmdidConvert(pdxlop->PmdidElem())->OidObjectId();

	const ULONG ulArity = pdxlnArrayref->UlArity();
	GPOS_ASSERT(3 == ulArity || 4 == ulArity);

	parrayref->reflowerindexpr = PlTranslateArrayRefIndexList((*pdxlnArrayref)[0], CDXLScalarArrayRefIndexList::EilbLower, pmapcidvar);
	parrayref->refupperindexpr = PlTranslateArrayRefIndexList((*pdxlnArrayref)[1], CDXLScalarArrayRefIndexList::EilbUpper, pmapcidvar);

	parrayref->refexpr = PexprFromDXLNodeScalar((*pdxlnArrayref)[2], pmapcidvar);
	parrayref->refassgnexpr = NULL;
	if (4 == ulArity)
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
	GPOS_ASSERT(eilb == CDXLScalarArrayRefIndexList::PdxlopConvert(pdxlnIndexlist->Pdxlop())->Eilb());

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
	GPOS_ASSERT(EdxlopScalarDMLAction == pdxlnDMLAction->Pdxlop()->Edxlop());

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
	return CMDIdGPDB::PmdidConvert(m_pmda->Pmdfunc(pmdid)->PmdidTypeResult())->OidObjectId();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::FBoolean
//
//	@doc:
//		Check to see if the operator returns a boolean result
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToScalar::FBoolean
	(
	CDXLNode *pdxln,
	CMDAccessor *pmda
	)
{
	GPOS_ASSERT(NULL != pdxln);

	if(EdxloptypeScalar != pdxln->Pdxlop()->Edxloperatortype())
	{
		return false;
	}

	CDXLScalar *pdxlop = dynamic_cast<CDXLScalar*>(pdxln->Pdxlop());

	return pdxlop->FBoolean(pmda);
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
	CDXLNode *pdxln,
	CMDAccessor *pmda
	)
{
	GPOS_ASSERT(NULL != pdxln);
	if (!FBoolean(pdxln, pmda) || EdxlopScalarConstValue != pdxln->Pdxlop()->Edxlop())
	{
		return false;
	}

	CDXLScalarConstValue *pdxlop = CDXLScalarConstValue::PdxlopConvert(pdxln->Pdxlop());
	CDXLDatumBool *pdxldatumbool = CDXLDatumBool::PdxldatumConvert(const_cast<CDXLDatum *>(pdxlop->Pdxldatum()));

	return pdxldatumbool->FValue();
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
	CDXLNode *pdxln
	)
{
	GPOS_ASSERT(NULL != pdxln);
	if (EdxlopScalarConstValue != pdxln->Pdxlop()->Edxlop())
	{
		return false;
	}

	CDXLScalarConstValue *pdxlop = CDXLScalarConstValue::PdxlopConvert(pdxln->Pdxlop());

	return pdxlop->Pdxldatum()->FNull();
}

// EOF
