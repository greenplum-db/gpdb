//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CTranslatorScalarToDXL.cpp
//
//	@doc:
//		Implementing the methods needed to translate a GPDB Scalar Operation (in a Query / PlStmt object)
//		into a DXL trees
//
//	@test:
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "utils/datum.h"
#include "utils/date.h"
#include "utils/numeric.h"

#include "gpopt/translate/CTranslatorScalarToDXL.h"
#include "gpopt/translate/CTranslatorQueryToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CCTEListEntry.h"

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDatumInt2.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"

#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CTranslatorScalarToDXL
//
//	@doc:
//		Ctor
//---------------------------------------------------------------------------
CTranslatorScalarToDXL::CTranslatorScalarToDXL
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	CIdGenerator *pulidgtorCol,
	CIdGenerator *pulidgtorCTE,
	ULONG ulQueryLevel,
	BOOL fQuery,
	HMUlCTEListEntry *phmulCTEEntries,
	DrgPdxln *pdrgpdxlnCTE
	)
	:
	m_pmp(pmp),
	m_pmda(pmda),
	m_pidgtorCol(pulidgtorCol),
	m_pidgtorCTE(pulidgtorCTE),
	m_ulQueryLevel(ulQueryLevel),
	m_fHasDistributedTables(false),
	m_fQuery(fQuery),
	m_eplsphoptype(EpspotNone),
	m_phmulCTEEntries(phmulCTEEntries),
	m_pdrgpdxlnCTE(pdrgpdxlnCTE)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::EdxlbooltypeFromGPDBBoolType
//
//	@doc:
//		Return the EdxlBoolExprType for a given GPDB BoolExprType
//---------------------------------------------------------------------------
EdxlBoolExprType
CTranslatorScalarToDXL::EdxlbooltypeFromGPDBBoolType
	(
	BoolExprType boolexprtype
	)
	const
{
	static ULONG rgrgulMapping[][2] =
		{
		{NOT_EXPR, Edxlnot},
		{AND_EXPR, Edxland},
		{OR_EXPR, Edxlor},
		};

	EdxlBoolExprType edxlbt = EdxlBoolExprTypeSentinel;

	const ULONG ulArity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) boolexprtype == pulElem[0])
		{
			edxlbt = (EdxlBoolExprType) pulElem[1];
			break;
		}
	}

	GPOS_ASSERT(EdxlBoolExprTypeSentinel != edxlbt && "Invalid bool expr type");

	return edxlbt;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScIdFromVar
//
//	@doc:
//		Create a DXL node for a scalar ident expression from a GPDB Var expression.
//		This function can be used for constructing a scalar ident operator appearing in a
//		base node (e.g. a scan node) or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScIdFromVar
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, Var));
	const Var * pvar = (Var *) pexpr;

	if (pvar->varattno == 0)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Whole-row variable"));
	}

	// column name
	const CWStringBase *pstr = pmapvarcolid->PstrColName(m_ulQueryLevel, pvar, m_eplsphoptype);

	// column id
	ULONG ulId;

	if(pvar->varattno != 0 || EpspotIndexScan == m_eplsphoptype || EpspotIndexOnlyScan == m_eplsphoptype)
	{
		ulId = pmapvarcolid->UlColId(m_ulQueryLevel, pvar, m_eplsphoptype);
	}
	else
	{
		ulId = m_pidgtorCol->UlNextId();
	}
	CMDName *pmdname = GPOS_NEW(m_pmp) CMDName(m_pmp, pstr);

	// create a column reference for the given var
	CDXLColRef *pdxlcr = GPOS_NEW(m_pmp) CDXLColRef(m_pmp, pmdname, ulId);

	// create the scalar ident operator
	CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_pmp) CDXLScalarIdent
													(
													m_pmp,
													pdxlcr,
													GPOS_NEW(m_pmp) CMDIdGPDB(pvar->vartype)
													);

	// create the DXL node holding the scalar ident operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopIdent);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScOpFromExpr
//
//	@doc:
//		Create a DXL node for a scalar expression from a GPDB expression node.
//		This function can be used for constructing a scalar operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScOpFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid,
	BOOL *pfHasDistributedTables // output
	)
{
	static const STranslatorElem rgTranslators[] =
	{
		{T_Var, &CTranslatorScalarToDXL::PdxlnScIdFromVar},
		{T_OpExpr, &CTranslatorScalarToDXL::PdxlnScOpExprFromExpr},
		{T_ScalarArrayOpExpr, &CTranslatorScalarToDXL::PdxlnArrayOpExpr},
		{T_DistinctExpr, &CTranslatorScalarToDXL::PdxlnScDistCmpFromDistExpr},
		{T_Const, &CTranslatorScalarToDXL::PdxlnScConstFromExpr},
		{T_BoolExpr, &CTranslatorScalarToDXL::PdxlnScBoolExprFromExpr},
		{T_BooleanTest, &CTranslatorScalarToDXL::PdxlnScBooleanTestFromExpr},
		{T_CaseExpr, &CTranslatorScalarToDXL::PdxlnScCaseStmtFromExpr},
		{T_CaseTestExpr, &CTranslatorScalarToDXL::PdxlnScCaseTestFromExpr},
		{T_CoalesceExpr, &CTranslatorScalarToDXL::PdxlnScCoalesceFromExpr},
		{T_MinMaxExpr, &CTranslatorScalarToDXL::PdxlnScMinMaxFromExpr},
		{T_FuncExpr, &CTranslatorScalarToDXL::PdxlnScFuncExprFromFuncExpr},
		{T_Aggref, &CTranslatorScalarToDXL::PdxlnScAggrefFromAggref},
		{T_WindowFunc, &CTranslatorScalarToDXL::PdxlnScWindowFunc},
		{T_NullTest, &CTranslatorScalarToDXL::PdxlnScNullTestFromNullTest},
		{T_NullIfExpr, &CTranslatorScalarToDXL::PdxlnScNullIfFromExpr},
		{T_RelabelType, &CTranslatorScalarToDXL::PdxlnScCastFromRelabelType},
		{T_CoerceToDomain, &CTranslatorScalarToDXL::PdxlnScCoerceFromCoerce},
		{T_CoerceViaIO, &CTranslatorScalarToDXL::PdxlnScCoerceFromCoerceViaIO},
		{T_ArrayCoerceExpr, &CTranslatorScalarToDXL::PdxlnScArrayCoerceExprFromExpr},
		{T_SubLink, &CTranslatorScalarToDXL::PdxlnFromSublink},
		{T_ArrayExpr, &CTranslatorScalarToDXL::PdxlnArray},
		{T_ArrayRef, &CTranslatorScalarToDXL::PdxlnArrayRef},
	};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);
	NodeTag ent = pexpr->type;

	// if an output variable is provided, we need to reset the member variable
	if (NULL != pfHasDistributedTables)
	{
		m_fHasDistributedTables = false;
	}

	// save old value for distributed tables flag
	BOOL fHasDistributedTablesOld = m_fHasDistributedTables;

	// find translator for the expression type
	PfPdxln pf = NULL;
	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		STranslatorElem elem = rgTranslators[ul];
		if (ent == elem.ent)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		CHAR *sz = (CHAR*) gpdb::SzNodeToString(const_cast<Expr*>(pexpr));
		CWStringDynamic *pstr = CDXLUtils::PstrFromSz(m_pmp, sz);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion, pstr->Wsz());
	}

	CDXLNode *pdxlnReturn = (this->*pf)(pexpr, pmapvarcolid);

	// combine old and current values for distributed tables flag
	m_fHasDistributedTables = m_fHasDistributedTables || fHasDistributedTablesOld;

	if (NULL != pfHasDistributedTables && m_fHasDistributedTables)
	{
		*pfHasDistributedTables = true;
	}

	return pdxlnReturn;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScDistCmpFromDistExpr
//
//	@doc:
//		Create a DXL node for a scalar distinct comparison expression from a GPDB
//		DistinctExpr.
//		This function can be used for constructing a scalar comparison operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScDistCmpFromDistExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, DistinctExpr));
	const DistinctExpr *pdistexpr = (DistinctExpr *) pexpr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::UlListLength(pdistexpr->args));

	CDXLNode *pdxlnLeft = PdxlnScOpFromExpr
							(
							(Expr *) gpdb::PvListNth(pdistexpr->args, 0),
							pmapvarcolid
							);

	CDXLNode *pdxlnRight = PdxlnScOpFromExpr
							(
							(Expr *) gpdb::PvListNth(pdistexpr->args, 1),
							pmapvarcolid
							);

	GPOS_ASSERT(NULL != pdxlnLeft);
	GPOS_ASSERT(NULL != pdxlnRight);

	CDXLScalarDistinctComp *pdxlop = GPOS_NEW(m_pmp) CDXLScalarDistinctComp
														(
														m_pmp,
														GPOS_NEW(m_pmp) CMDIdGPDB(pdistexpr->opno)
														);

	// create the DXL node holding the scalar distinct comparison operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// add children in the right order
	pdxln->AddChild(pdxlnLeft);
	pdxln->AddChild(pdxlnRight);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScCmpFromOpExpr
//
//	@doc:
//		Create a DXL node for a scalar comparison expression from a GPDB OpExpr.
//		This function can be used for constructing a scalar comparison operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCmpFromOpExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, OpExpr));
	const OpExpr *popexpr = (OpExpr *) pexpr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::UlListLength(popexpr->args));

	Expr *pexprLeft = (Expr *) gpdb::PvListNth(popexpr->args, 0);
	Expr *pexprRight = (Expr *) gpdb::PvListNth(popexpr->args, 1);

	CDXLNode *pdxlnLeft = PdxlnScOpFromExpr(pexprLeft, pmapvarcolid);
	CDXLNode *pdxlnRight = PdxlnScOpFromExpr(pexprRight, pmapvarcolid);

	GPOS_ASSERT(NULL != pdxlnLeft);
	GPOS_ASSERT(NULL != pdxlnRight);

	CMDIdGPDB *pmdid = GPOS_NEW(m_pmp) CMDIdGPDB(popexpr->opno);

	// get operator name
	const CWStringConst *pstr = PstrOpName(pmdid);

	CDXLScalarComp *pdxlop = GPOS_NEW(m_pmp) CDXLScalarComp(m_pmp, pmdid, GPOS_NEW(m_pmp) CWStringConst(pstr->Wsz()));

	// create the DXL node holding the scalar comparison operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// add children in the right order
	pdxln->AddChild(pdxlnLeft);
	pdxln->AddChild(pdxlnRight);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScOpExprFromExpr
//
//	@doc:
//		Create a DXL node for a scalar opexpr from a GPDB OpExpr.
//		This function can be used for constructing a scalar opexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScOpExprFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, OpExpr));

	const OpExpr *popexpr = (OpExpr *) pexpr;

	// check if this is a scalar comparison
	CMDIdGPDB *pmdidReturnType = GPOS_NEW(m_pmp) CMDIdGPDB(((OpExpr *) pexpr)->opresulttype);
	const IMDType *pmdtype= m_pmda->Pmdtype(pmdidReturnType);

	const ULONG ulArgs = gpdb::UlListLength(popexpr->args);

	if (IMDType::EtiBool ==  pmdtype->Eti() && 2 == ulArgs)
	{
		pmdidReturnType->Release();
		return PdxlnScCmpFromOpExpr(pexpr, pmapvarcolid);
	}

	// get operator name and id
	IMDId *pmdid = GPOS_NEW(m_pmp) CMDIdGPDB(popexpr->opno);
	const CWStringConst *pstr = PstrOpName(pmdid);

	CDXLScalarOpExpr *pdxlop = GPOS_NEW(m_pmp) CDXLScalarOpExpr(m_pmp, pmdid, pmdidReturnType, GPOS_NEW(m_pmp) CWStringConst(pstr->Wsz()));

	// create the DXL node holding the scalar opexpr
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// process arguments
	TranslateScalarChildren(pdxln, popexpr->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScNullIfFromExpr
//
//	@doc:
//		Create a DXL node for a scalar nullif from a GPDB Expr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScNullIfFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, NullIfExpr));
	const NullIfExpr *pnullifexpr = (NullIfExpr *) pexpr;

	GPOS_ASSERT(2 == gpdb::UlListLength(pnullifexpr->args));

	CDXLScalarNullIf *pdxlop = GPOS_NEW(m_pmp) CDXLScalarNullIf(m_pmp, GPOS_NEW(m_pmp) CMDIdGPDB(pnullifexpr->opno), GPOS_NEW(m_pmp) CMDIdGPDB(gpdb::OidExprType((Node *)pnullifexpr)));

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// process arguments
	TranslateScalarChildren(pdxln, pnullifexpr->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnArrayOpExpr
//
//	@doc:
//		Create a DXL node for a scalar array expression from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnArrayOpExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	return PdxlnScArrayCompFromExpr(pexpr, pmapvarcolid);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScArrayCompFromExpr
//
//	@doc:
//		Create a DXL node for a scalar array comparison from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScArrayCompFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, ScalarArrayOpExpr));
	const ScalarArrayOpExpr *pscarrayopexpr = (ScalarArrayOpExpr *) pexpr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::UlListLength(pscarrayopexpr->args));

	Expr *pexprLeft = (Expr*) gpdb::PvListNth(pscarrayopexpr->args, 0);
	CDXLNode *pdxlnLeft = PdxlnScOpFromExpr(pexprLeft, pmapvarcolid);

	Expr *pexprRight = (Expr*) gpdb::PvListNth(pscarrayopexpr->args, 1);

	// If the argument array is an array Const, try to transform it to an
	// ArrayExpr, to allow ORCA to optimize it better. (ORCA knows how to
	// extract elements of an ArrayExpr, but doesn't currently know how
	// to do it from an array-typed Const.)
	if (IsA(pexprRight, Const))
		pexprRight = gpdb::PexprTransformArrayConstToArrayExpr((Const *) pexprRight);

	CDXLNode *pdxlnRight = PdxlnScOpFromExpr(pexprRight, pmapvarcolid);

	GPOS_ASSERT(NULL != pdxlnLeft);
	GPOS_ASSERT(NULL != pdxlnRight);

	// get operator name
	CMDIdGPDB *pmdidOp = GPOS_NEW(m_pmp) CMDIdGPDB(pscarrayopexpr->opno);
	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pmdidOp);
	pmdidOp->Release();

	const CWStringConst *pstr = pmdscop->Mdname().Pstr();
	GPOS_ASSERT(NULL != pstr);

	EdxlArrayCompType edxlarraycomptype = Edxlarraycomptypeany;

	if(!pscarrayopexpr->useOr)
	{
		edxlarraycomptype = Edxlarraycomptypeall;
	}

	CDXLScalarArrayComp *pdxlop = GPOS_NEW(m_pmp) CDXLScalarArrayComp(m_pmp, GPOS_NEW(m_pmp) CMDIdGPDB(pscarrayopexpr->opno), GPOS_NEW(m_pmp) CWStringConst(pstr->Wsz()), edxlarraycomptype);

	// create the DXL node holding the scalar opexpr
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// add children in the right order
	pdxln->AddChild(pdxlnLeft);
	pdxln->AddChild(pdxlnRight);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScConstFromExpr
//
//	@doc:
//		Create a DXL node for a scalar const value from a GPDB Const
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScConstFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId * // pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, Const));
	const Const *pconst = (Const *) pexpr;

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
									(
									m_pmp,
									GPOS_NEW(m_pmp) CDXLScalarConstValue
												(
												m_pmp,
												Pdxldatum(pconst)
												)
									);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pdxldatum
//
//	@doc:
//		Create a DXL datum from a GPDB Const
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::Pdxldatum
	(
	IMemoryPool *pmp,
	CMDAccessor *mda,
	const Const *pconst
	)
{
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(pconst->consttype);
	const IMDType *pmdtype= mda->Pmdtype(pmdid);
	pmdid->Release();

 	// translate gpdb datum into a DXL datum
	CDXLDatum *pdxldatum = CTranslatorScalarToDXL::Pdxldatum(pmp, pmdtype, pconst->constisnull, pconst->constlen, pconst->constvalue);

	return pdxldatum;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pdxldatum
//
//	@doc:
//		Create a DXL datum from a GPDB Const
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::Pdxldatum
	(
	const Const *pconst
	)
	const
{
	return Pdxldatum(m_pmp, m_pmda, pconst);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScBoolExprFromExpr
//
//	@doc:
//		Create a DXL node for a scalar boolean expression from a GPDB OpExpr.
//		This function can be used for constructing a scalar boolexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScBoolExprFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, BoolExpr));
	const BoolExpr *pboolexpr = (BoolExpr *) pexpr;
	GPOS_ASSERT(0 < gpdb::UlListLength(pboolexpr->args));

	EdxlBoolExprType boolexptype = EdxlbooltypeFromGPDBBoolType(pboolexpr->boolop);
	GPOS_ASSERT(EdxlBoolExprTypeSentinel != boolexptype);

	// create the DXL node holding the scalar boolean operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarBoolExpr(m_pmp, boolexptype));

	ULONG ulCount = gpdb::UlListLength(pboolexpr->args);

	if ((NOT_EXPR != pboolexpr->boolop) && (2 > ulCount))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiPlStmt2DXLConversion,
			GPOS_WSZ_LIT("Boolean Expression (OR / AND): Incorrect Number of Children ")
			);
	}
	else if ((NOT_EXPR == pboolexpr->boolop) && (1 != ulCount))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiPlStmt2DXLConversion,
			GPOS_WSZ_LIT("Boolean Expression (NOT): Incorrect Number of Children ")
			);
	}

	TranslateScalarChildren(pdxln, pboolexpr->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScBooleanTestFromExpr
//
//	@doc:
//		Create a DXL node for a scalar boolean test from a GPDB OpExpr.
//		This function can be used for constructing a scalar boolexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScBooleanTestFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, BooleanTest));

	const BooleanTest *pbooleantest = (BooleanTest *) pexpr;

	GPOS_ASSERT(NULL != pbooleantest->arg);

	static ULONG rgrgulMapping[][2] =
		{
		{IS_TRUE, EdxlbooleantestIsTrue},
		{IS_NOT_TRUE, EdxlbooleantestIsNotTrue},
		{IS_FALSE, EdxlbooleantestIsFalse},
		{IS_NOT_FALSE, EdxlbooleantestIsNotFalse},
		{IS_UNKNOWN, EdxlbooleantestIsUnknown},
		{IS_NOT_UNKNOWN, EdxlbooleantestIsNotUnknown},
		};

	EdxlBooleanTestType edxlbt = EdxlbooleantestSentinel;
	const ULONG ulArity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) pbooleantest->booltesttype == pulElem[0])
		{
			edxlbt = (EdxlBooleanTestType) pulElem[1];
			break;
		}
	}
	GPOS_ASSERT(EdxlbooleantestSentinel != edxlbt && "Invalid boolean test type");

	// create the DXL node holding the scalar boolean test operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
									(
									m_pmp,
									GPOS_NEW(m_pmp) CDXLScalarBooleanTest(m_pmp,edxlbt)
									);

	CDXLNode *pdxlnArg = PdxlnScOpFromExpr(pbooleantest->arg, pmapvarcolid);
	GPOS_ASSERT(NULL != pdxlnArg);

	pdxln->AddChild(pdxlnArg);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScNullTestFromNullTest
//
//	@doc:
//		Create a DXL node for a scalar nulltest expression from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScNullTestFromNullTest
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, NullTest));
	const NullTest *pnulltest = (NullTest *) pexpr;

	GPOS_ASSERT(NULL != pnulltest->arg);
	CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pnulltest->arg, pmapvarcolid);

	GPOS_ASSERT(NULL != pdxlnChild);
	GPOS_ASSERT(IS_NULL == pnulltest->nulltesttype || IS_NOT_NULL == pnulltest->nulltesttype);

	BOOL fIsNull = false;
	if (IS_NULL == pnulltest->nulltesttype)
	{
		fIsNull = true;
	}

	// create the DXL node holding the scalar NullTest operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarNullTest(m_pmp, fIsNull));
	pdxln->AddChild(pdxlnChild);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScCoalesceFromExpr
//
//	@doc:
//		Create a DXL node for a coalesce function from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCoalesceFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, CoalesceExpr));

	CoalesceExpr *pcoalesceexpr = (CoalesceExpr *) pexpr;
	GPOS_ASSERT(NULL != pcoalesceexpr->args);

	CDXLScalarCoalesce *pdxlop = GPOS_NEW(m_pmp) CDXLScalarCoalesce
											(
											m_pmp,
											GPOS_NEW(m_pmp) CMDIdGPDB(pcoalesceexpr->coalescetype)
											);

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	TranslateScalarChildren(pdxln, pcoalesceexpr->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScMinMaxFromExpr
//
//	@doc:
//		Create a DXL node for a min/max operator from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScMinMaxFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, MinMaxExpr));

	MinMaxExpr *pminmaxexpr = (MinMaxExpr *) pexpr;
	GPOS_ASSERT(NULL != pminmaxexpr->args);

	CDXLScalarMinMax::EdxlMinMaxType emmt = CDXLScalarMinMax::EmmtSentinel;
	if (IS_GREATEST == pminmaxexpr->op)
	{
		emmt = CDXLScalarMinMax::EmmtMax;
	}
	else
	{
		GPOS_ASSERT(IS_LEAST == pminmaxexpr->op);
		emmt = CDXLScalarMinMax::EmmtMin;
	}

	CDXLScalarMinMax *pdxlop = GPOS_NEW(m_pmp) CDXLScalarMinMax
											(
											m_pmp,
											GPOS_NEW(m_pmp) CMDIdGPDB(pminmaxexpr->minmaxtype),
											emmt
											);

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	TranslateScalarChildren(pdxln, pminmaxexpr->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::TranslateScalarChildren
//
//	@doc:
//		Translate list elements and add them as children of the DXL node
//---------------------------------------------------------------------------
void
CTranslatorScalarToDXL::TranslateScalarChildren
	(
	CDXLNode *pdxln,
	List *plist,
	const CMappingVarColId* pmapvarcolid,
	BOOL *pfHasDistributedTables // output
	)
{
	ListCell *plc = NULL;
	ForEach (plc, plist)
	{
		Expr *pexprChild = (Expr *) lfirst(plc);
		CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pexprChild, pmapvarcolid, pfHasDistributedTables);
		GPOS_ASSERT(NULL != pdxlnChild);
		pdxln->AddChild(pdxlnChild);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScCaseStmtFromExpr
//
//	@doc:
//		Create a DXL node for a case statement from a GPDB OpExpr.
//		This function can be used for constructing a scalar opexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCaseStmtFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, CaseExpr));

	const CaseExpr *pcaseexpr = (CaseExpr *) pexpr;

	if (NULL == pcaseexpr->args)
	{
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Do not support SIMPLE CASE STATEMENT")
				);
			return NULL;
	}

	if (NULL == pcaseexpr->arg)
	{
		return PdxlnScIfStmtFromCaseExpr(pcaseexpr, pmapvarcolid);
	}

	return PdxlnScSwitchFromCaseExpr(pcaseexpr, pmapvarcolid);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScSwitchFromCaseExpr
//
//	@doc:
//		Create a DXL Switch node from a GPDB CaseExpr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScSwitchFromCaseExpr
	(
	const CaseExpr *pcaseexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT (NULL != pcaseexpr->arg);

	CDXLScalarSwitch *pdxlop = GPOS_NEW(m_pmp) CDXLScalarSwitch
												(
												m_pmp,
												GPOS_NEW(m_pmp) CMDIdGPDB(pcaseexpr->casetype)
												);
	CDXLNode *pdxlnSwitch = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// translate the switch expression
	CDXLNode *pdxlnArg = PdxlnScOpFromExpr(pcaseexpr->arg, pmapvarcolid);
	pdxlnSwitch->AddChild(pdxlnArg);

	// translate the cases
	ListCell *plc = NULL;
	ForEach (plc, pcaseexpr->args)
	{
		CaseWhen *pexpr = (CaseWhen *) lfirst(plc);

		CDXLScalarSwitchCase *pdxlopCase = GPOS_NEW(m_pmp) CDXLScalarSwitchCase(m_pmp);
		CDXLNode *pdxlnCase = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopCase);

		CDXLNode *pdxlnCmpExpr = PdxlnScOpFromExpr(pexpr->expr, pmapvarcolid);
		GPOS_ASSERT(NULL != pdxlnCmpExpr);

		CDXLNode *pdxlnResult = PdxlnScOpFromExpr(pexpr->result, pmapvarcolid);
		GPOS_ASSERT(NULL != pdxlnResult);

		pdxlnCase->AddChild(pdxlnCmpExpr);
		pdxlnCase->AddChild(pdxlnResult);

		// add current case to switch node
		pdxlnSwitch->AddChild(pdxlnCase);
	}

	// translate the "else" clause
	if (NULL != pcaseexpr->defresult)
	{
		CDXLNode *pdxlnDefaultResult = PdxlnScOpFromExpr(pcaseexpr->defresult, pmapvarcolid);
		GPOS_ASSERT(NULL != pdxlnDefaultResult);

		pdxlnSwitch->AddChild(pdxlnDefaultResult);

	}

	return pdxlnSwitch;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScCaseTestFromExpr
//
//	@doc:
//		Create a DXL node for a case test from a GPDB Expr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCaseTestFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* //pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, CaseTestExpr));
	const CaseTestExpr *pcasetestexpr = (CaseTestExpr *) pexpr;
	CDXLScalarCaseTest *pdxlop = GPOS_NEW(m_pmp) CDXLScalarCaseTest
												(
												m_pmp,
												GPOS_NEW(m_pmp) CMDIdGPDB(pcasetestexpr->typeId)
												);

	return GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScIfStmtFromCaseExpr
//
//	@doc:
//		Create a DXL If node from a GPDB CaseExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScIfStmtFromCaseExpr
	(
	const CaseExpr *pcaseexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT (NULL == pcaseexpr->arg);
	const ULONG ulWhenClauseCount = gpdb::UlListLength(pcaseexpr->args);

	CDXLNode *pdxlnRootIfTree = NULL;
	CDXLNode *pdxlnCurr = NULL;

	for (ULONG ul = 0; ul < ulWhenClauseCount; ul++)
	{
		CDXLScalarIfStmt *pdxlopIfstmtNew = GPOS_NEW(m_pmp) CDXLScalarIfStmt
															(
															m_pmp,
															GPOS_NEW(m_pmp) CMDIdGPDB(pcaseexpr->casetype)
															);

		CDXLNode *pdxlnIfStmtNew = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopIfstmtNew);

		CaseWhen *pexpr = (CaseWhen *) gpdb::PvListNth(pcaseexpr->args, ul);
		GPOS_ASSERT(IsA(pexpr, CaseWhen));

		CDXLNode *pdxlnCond = PdxlnScOpFromExpr(pexpr->expr, pmapvarcolid);
		CDXLNode *pdxlnResult = PdxlnScOpFromExpr(pexpr->result, pmapvarcolid);

		GPOS_ASSERT(NULL != pdxlnCond);
		GPOS_ASSERT(NULL != pdxlnResult);

		pdxlnIfStmtNew->AddChild(pdxlnCond);
		pdxlnIfStmtNew->AddChild(pdxlnResult);

		if(NULL == pdxlnRootIfTree)
		{
			pdxlnRootIfTree = pdxlnIfStmtNew;
		}
		else
		{
			pdxlnCurr->AddChild(pdxlnIfStmtNew);
		}
		pdxlnCurr = pdxlnIfStmtNew;
	}

	if (NULL != pcaseexpr->defresult)
	{
		CDXLNode *pdxlnDefaultResult = PdxlnScOpFromExpr(pcaseexpr->defresult, pmapvarcolid);
		GPOS_ASSERT(NULL != pdxlnDefaultResult);
		pdxlnCurr->AddChild(pdxlnDefaultResult);
	}

	return pdxlnRootIfTree;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScCastFromRelabelType
//
//	@doc:
//		Create a DXL node for a scalar RelabelType expression from a GPDB RelabelType
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCastFromRelabelType
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, RelabelType));

	const RelabelType *prelabeltype = (RelabelType *) pexpr;

	GPOS_ASSERT(NULL != prelabeltype->arg);

	CDXLNode *pdxlnChild = PdxlnScOpFromExpr(prelabeltype->arg, pmapvarcolid);

	GPOS_ASSERT(NULL != pdxlnChild);

	// create the DXL node holding the scalar boolean operator
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
									(
									m_pmp,
									GPOS_NEW(m_pmp) CDXLScalarCast
												(
												m_pmp,
												GPOS_NEW(m_pmp) CMDIdGPDB(prelabeltype->resulttype),
												GPOS_NEW(m_pmp) CMDIdGPDB(0) // casting function oid
												)
									);
	pdxln->AddChild(pdxlnChild);

	return pdxln;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorScalarToDXL::PdxlnScCoerceFromCoerce
//
//      @doc:
//              Create a DXL node for a scalar coerce expression from a
//             GPDB coerce expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCoerceFromCoerce
        (
        const Expr *pexpr,
        const CMappingVarColId* pmapvarcolid
        )
{
        GPOS_ASSERT(IsA(pexpr, CoerceToDomain));

        const CoerceToDomain *pcoerce = (CoerceToDomain *) pexpr;

        GPOS_ASSERT(NULL != pcoerce->arg);

        CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pcoerce->arg, pmapvarcolid);

        GPOS_ASSERT(NULL != pdxlnChild);

        // create the DXL node holding the scalar boolean operator
        CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
                                                                        (
                                                                        m_pmp,
                                                                        GPOS_NEW(m_pmp) CDXLScalarCoerceToDomain
                                                                                                (
                                                                                                m_pmp,
                                                                                                GPOS_NEW(m_pmp) CMDIdGPDB(pcoerce->resulttype),
                                                                                               pcoerce->resulttypmod,
                                                                                               (EdxlCoercionForm) pcoerce->coercionformat,
                                                                                               pcoerce->location
                                                                                                )
                                                                        );
        pdxln->AddChild(pdxlnChild);

        return pdxln;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorScalarToDXL::PdxlnScCoerceFromCoerceViaIO
//
//      @doc:
//              Create a DXL node for a scalar coerce expression from a
//             GPDB coerce expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCoerceFromCoerceViaIO
        (
        const Expr *pexpr,
        const CMappingVarColId* pmapvarcolid
        )
{
        GPOS_ASSERT(IsA(pexpr, CoerceViaIO));

        const CoerceViaIO *pcoerce = (CoerceViaIO *) pexpr;

        GPOS_ASSERT(NULL != pcoerce->arg);

        CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pcoerce->arg, pmapvarcolid);

        GPOS_ASSERT(NULL != pdxlnChild);

        // create the DXL node holding the scalar boolean operator
        CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
                                                                        (
                                                                        m_pmp,
                                                                        GPOS_NEW(m_pmp) CDXLScalarCoerceViaIO
                                                                                                (
                                                                                                m_pmp,
                                                                                                GPOS_NEW(m_pmp) CMDIdGPDB(pcoerce->resulttype),
                                                                                               -1,
                                                                                               (EdxlCoercionForm) pcoerce->coerceformat,
                                                                                               pcoerce->location
                                                                                                )
                                                                        );
        pdxln->AddChild(pdxlnChild);

        return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScArrayCoerceExprFromExpr
//	@doc:
//		Create a DXL node for a scalar array coerce expression from a
// 		GPDB Array Coerce expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScArrayCoerceExprFromExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, ArrayCoerceExpr));
	const ArrayCoerceExpr *parraycoerce = (ArrayCoerceExpr *) pexpr;
	
	GPOS_ASSERT(NULL != parraycoerce->arg);
	
	CDXLNode *pdxlnChild = PdxlnScOpFromExpr(parraycoerce->arg, pmapvarcolid);
	
	GPOS_ASSERT(NULL != pdxlnChild);
	
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
					(
					m_pmp,
					GPOS_NEW(m_pmp) CDXLScalarArrayCoerceExpr
							(
							m_pmp,
							GPOS_NEW(m_pmp) CMDIdGPDB(parraycoerce->elemfuncid),
							GPOS_NEW(m_pmp) CMDIdGPDB(parraycoerce->resulttype),
							parraycoerce->resulttypmod,
							parraycoerce->isExplicit,
							(EdxlCoercionForm) parraycoerce->coerceformat,
							parraycoerce->location
							)
					);
	
        pdxln->AddChild(pdxlnChild);

        return pdxln;
}




//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScFuncExprFromFuncExpr
//
//	@doc:
//		Create a DXL node for a scalar funcexpr from a GPDB FuncExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScFuncExprFromFuncExpr
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, FuncExpr));
	const FuncExpr *pfuncexpr = (FuncExpr *) pexpr;

	CMDIdGPDB *pmdidFunc = GPOS_NEW(m_pmp) CMDIdGPDB(pfuncexpr->funcid);

	// create the DXL node holding the scalar funcexpr
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode
									(
									m_pmp,
									GPOS_NEW(m_pmp) CDXLScalarFuncExpr
												(
												m_pmp,
												pmdidFunc,
												GPOS_NEW(m_pmp) CMDIdGPDB(pfuncexpr->funcresulttype),
												pfuncexpr->funcretset
												)
									);

	const IMDFunction *pmdfunc = m_pmda->Pmdfunc(pmdidFunc);
	if (IMDFunction::EfsVolatile == pmdfunc->EfsStability())
	{
		ListCell *plc = NULL;
		ForEach (plc, pfuncexpr->args)
		{
			Node *pnodeArg = (Node *) lfirst(plc);
			if (CTranslatorUtils::FHasSubquery(pnodeArg))
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
						GPOS_WSZ_LIT("Volatile functions with subqueries in arguments"));
			}
		}
	}

	TranslateScalarChildren(pdxln, pfuncexpr->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScAggrefFromAggref
//
//	@doc:
//		Create a DXL node for a scalar aggref from a GPDB FuncExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScAggrefFromAggref
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, Aggref));
	const Aggref *paggref = (Aggref *) pexpr;
	BOOL aggDistinct = false;

	static ULONG rgrgulMapping[][2] =
		{
		{AGGSTAGE_NORMAL, EdxlaggstageNormal},
		{AGGSTAGE_PARTIAL, EdxlaggstagePartial},
		{AGGSTAGE_INTERMEDIATE, EdxlaggstageIntermediate},
		{AGGSTAGE_FINAL, EdxlaggstageFinal},
		};

	if (paggref->aggorder != NIL)
	{
		GPOS_RAISE
		(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Ordered aggregates")
		);
	}

	if (paggref->aggdistinct)
	{
		aggDistinct = true;
	}

	EdxlAggrefStage edxlaggstage = EdxlaggstageSentinel;
	const ULONG ulArity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) paggref->aggstage == pulElem[0])
		{
			edxlaggstage = (EdxlAggrefStage) pulElem[1];
			break;
		}
	}
	GPOS_ASSERT(EdxlaggstageSentinel != edxlaggstage && "Invalid agg stage");

	CMDIdGPDB *pmdidAgg = GPOS_NEW(m_pmp) CMDIdGPDB(paggref->aggfnoid);
	const IMDAggregate *pmdagg = m_pmda->Pmdagg(pmdidAgg);

	GPOS_ASSERT(!pmdagg->FOrdered());

	if (0 != paggref->agglevelsup)
	{
		// TODO: Feb 05 2015, remove temporary fix to avoid erroring out during execution
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLError, GPOS_WSZ_LIT("Aggregate functions with outer references"));
	}

	// ORCA doesn't support the FILTER clause yet.
	if (paggref->aggfilter)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Aggregate functions with FILTER"));
	}

	IMDId *pmdidRetType = CScalarAggFunc::PmdidLookupReturnType(pmdidAgg, (EdxlaggstageNormal == edxlaggstage), m_pmda);
	IMDId *pmdidResolvedRetType = NULL;
	if (m_pmda->Pmdtype(pmdidRetType)->FAmbiguous())
	{
		// if return type given by MD cache is ambiguous, use type provided by aggref node
		pmdidResolvedRetType = GPOS_NEW(m_pmp) CMDIdGPDB(paggref->aggtype);
	}

	CDXLScalarAggref *pdxlopAggref = GPOS_NEW(m_pmp) CDXLScalarAggref(m_pmp, pmdidAgg, pmdidResolvedRetType, aggDistinct, edxlaggstage);

	// create the DXL node holding the scalar aggref
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopAggref);

	// translate args
	ListCell *plc;
	ForEach (plc, paggref->args)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(plc);
		CDXLNode *pdxlnChild = PdxlnScOpFromExpr(tle->expr, pmapvarcolid, NULL);
		GPOS_ASSERT(NULL != pdxlnChild);
		pdxln->AddChild(pdxlnChild);
	}

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pdxlwf
//
//	@doc:
//		Create a DXL window frame from a GPDB WindowFrame
//---------------------------------------------------------------------------
CDXLWindowFrame *
CTranslatorScalarToDXL::Pdxlwf
	(
	int frameOptions,
	const Node *startOffset,
	const Node *endOffset,
	const CMappingVarColId* pmapvarcolid,
	CDXLNode *pdxlnNewChildScPrL,
	BOOL *pfHasDistributedTables // output
	)
{
	EdxlFrameSpec edxlfs;

	if ((frameOptions & FRAMEOPTION_ROWS) != 0)
		edxlfs = EdxlfsRow;
	else
		edxlfs = EdxlfsRange;

	EdxlFrameBoundary edxlfbLead;
	if ((frameOptions & FRAMEOPTION_END_UNBOUNDED_PRECEDING) != 0)
		edxlfbLead = EdxlfbUnboundedPreceding;
	else if ((frameOptions & FRAMEOPTION_END_VALUE_PRECEDING) != 0)
		edxlfbLead = EdxlfbBoundedPreceding;
	else if ((frameOptions & FRAMEOPTION_END_CURRENT_ROW) != 0)
		edxlfbLead = EdxlfbCurrentRow;
	else if ((frameOptions & FRAMEOPTION_END_VALUE_FOLLOWING) != 0)
		edxlfbLead = EdxlfbBoundedFollowing;
	else if ((frameOptions & FRAMEOPTION_END_UNBOUNDED_FOLLOWING) != 0)
		edxlfbLead = EdxlfbUnboundedFollowing;
	else
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion,
			   GPOS_WSZ_LIT("Unrecognized window frame option"));

	EdxlFrameBoundary edxlfbTrail;
	if ((frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING) != 0)
		edxlfbTrail = EdxlfbUnboundedPreceding;
	else if ((frameOptions & FRAMEOPTION_START_VALUE_PRECEDING) != 0)
		edxlfbTrail = EdxlfbBoundedPreceding;
	else if ((frameOptions & FRAMEOPTION_START_CURRENT_ROW) != 0)
		edxlfbTrail = EdxlfbCurrentRow;
	else if ((frameOptions & FRAMEOPTION_START_VALUE_FOLLOWING) != 0)
		edxlfbTrail = EdxlfbBoundedFollowing;
	else if ((frameOptions & FRAMEOPTION_START_UNBOUNDED_FOLLOWING) != 0)
		edxlfbTrail = EdxlfbUnboundedFollowing;
	else
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion,
			   GPOS_WSZ_LIT("Unrecognized window frame option"));

	// We don't support non-default EXCLUDE [CURRENT ROW | GROUP | TIES |
	// NO OTHERS] options.
	EdxlFrameExclusionStrategy edxlfes = EdxlfesNulls;

	CDXLNode *pdxlnLeadEdge = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarWindowFrameEdge(m_pmp, true /* fLeading */, edxlfbLead));
	CDXLNode *pdxlnTrailEdge = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarWindowFrameEdge(m_pmp, false /* fLeading */, edxlfbTrail));

	// translate the lead and trail value
	if (NULL != endOffset)
	{
		pdxlnLeadEdge->AddChild(PdxlnWindowFrameEdgeVal(endOffset, pmapvarcolid, pdxlnNewChildScPrL, pfHasDistributedTables));
	}

	if (NULL != startOffset)
	{
		pdxlnTrailEdge->AddChild(PdxlnWindowFrameEdgeVal(startOffset, pmapvarcolid, pdxlnNewChildScPrL, pfHasDistributedTables));
	}

	CDXLWindowFrame *pdxlWf = GPOS_NEW(m_pmp) CDXLWindowFrame(m_pmp, edxlfs, edxlfes, pdxlnLeadEdge, pdxlnTrailEdge);

	return pdxlWf;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnWindowFrameEdgeVal
//
//	@doc:
//		Translate the window frame edge, if the column used in the edge is a
// 		computed column then add it to the project list
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnWindowFrameEdgeVal
	(
	const Node *pnode,
	const CMappingVarColId* pmapvarcolid,
	CDXLNode *pdxlnNewChildScPrL,
	BOOL *pfHasDistributedTables
	)
{
	CDXLNode *pdxlnVal = PdxlnScOpFromExpr((Expr *) pnode, pmapvarcolid, pfHasDistributedTables);

	if (m_fQuery && !IsA(pnode, Var) && !IsA(pnode, Const))
	{
		GPOS_ASSERT(NULL != pdxlnNewChildScPrL);
		CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
		CMDName *pmdnameAlias = GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol);
		ULONG ulPrElId = m_pidgtorCol->UlNextId();

		// construct a projection element
		CDXLNode *pdxlnPrEl = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarProjElem(m_pmp, ulPrElId, pmdnameAlias));
		pdxlnPrEl->AddChild(pdxlnVal);

		// add it to the computed columns project list
		pdxlnNewChildScPrL->AddChild(pdxlnPrEl);

		// construct a new scalar ident
		CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_pmp) CDXLScalarIdent
													(
													m_pmp,
													GPOS_NEW(m_pmp) CDXLColRef
																(
																m_pmp,
																GPOS_NEW(m_pmp) CMDName(m_pmp, &strUnnamedCol),
																ulPrElId
																),
													GPOS_NEW(m_pmp) CMDIdGPDB(gpdb::OidExprType(const_cast<Node*>(pnode)))
													);

		pdxlnVal = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopIdent);
	}

	return pdxlnVal;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScWindowFunc
//
//	@doc:
//		Create a DXL node for a scalar window ref from a GPDB WindowFunc
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScWindowFunc
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, WindowFunc));

	const WindowFunc *pwindowfunc = (WindowFunc *) pexpr;

	static ULONG rgrgulMapping[][2] =
		{
		{WINSTAGE_IMMEDIATE, EdxlwinstageImmediate},
		{WINSTAGE_PRELIMINARY, EdxlwinstagePreliminary},
		{WINSTAGE_ROWKEY, EdxlwinstageRowKey},
		};

	// ORCA doesn't support the FILTER clause yet.
	if (pwindowfunc->aggfilter)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Aggregate functions with FILTER"));
	}

	const ULONG ulArity = GPOS_ARRAY_SIZE(rgrgulMapping);
	EdxlWinStage edxlwinstage = EdxlwinstageSentinel;

	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) pwindowfunc->winstage == pulElem[0])
		{
			edxlwinstage = (EdxlWinStage) pulElem[1];
			break;
		}
	}

	ULONG ulWinSpecPos = (ULONG) 0;
	if (m_fQuery)
	{
		ulWinSpecPos = (ULONG) pwindowfunc->winref - 1;
	}

	GPOS_ASSERT(EdxlwinstageSentinel != edxlwinstage && "Invalid window stage");

	/*
	 * ORCA's ScalarWindowRef object doesn't have fields for the 'winstar'
	 * and 'winagg', so we lose that information in the translation.
	 * Fortunately, the executor currently doesn't need those fields to
	 * be set correctly.
	 */
	CDXLScalarWindowRef *pdxlopWinref = GPOS_NEW(m_pmp) CDXLScalarWindowRef
													(
													m_pmp,
													GPOS_NEW(m_pmp) CMDIdGPDB(pwindowfunc->winfnoid),
													GPOS_NEW(m_pmp) CMDIdGPDB(pwindowfunc->wintype),
													pwindowfunc->windistinct,
													pwindowfunc->winstar,
													pwindowfunc->winagg,
													edxlwinstage,
													ulWinSpecPos
													);

	// create the DXL node holding the scalar aggref
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopWinref);

	TranslateScalarChildren(pdxln, pwindowfunc->args, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScCondFromQual
//
//	@doc:
//		Create a DXL scalar boolean operator node from a GPDB qual list.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScCondFromQual
	(
	List *plQual,
	const CMappingVarColId* pmapvarcolid,
	BOOL *pfHasDistributedTables
	)
{
	if (NULL == plQual || 0 == gpdb::UlListLength(plQual))
	{
		return NULL;
	}

	if (1 == gpdb::UlListLength(plQual))
	{
		Expr *pexpr = (Expr *) gpdb::PvListNth(plQual, 0);
		return PdxlnScOpFromExpr(pexpr, pmapvarcolid, pfHasDistributedTables);
	}
	else
	{
		// GPDB assumes that if there are a list of qual conditions then it is an implicit AND operation
		// Here we build the left deep AND tree
		CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarBoolExpr(m_pmp, Edxland));

		TranslateScalarChildren(pdxln, plQual, pmapvarcolid, pfHasDistributedTables);

		return pdxln;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnFilterFromQual
//
//	@doc:
//		Create a DXL scalar filter node from a GPDB qual list.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnFilterFromQual
	(
	List *plQual,
	const CMappingVarColId* pmapvarcolid,
	Edxlopid edxlopFilterType,
	BOOL *pfHasDistributedTables // output
	)
{
	CDXLScalarFilter *pdxlop = NULL;

	switch (edxlopFilterType)
	{
		case EdxlopScalarFilter:
			pdxlop = GPOS_NEW(m_pmp) CDXLScalarFilter(m_pmp);
			break;
		case EdxlopScalarJoinFilter:
			pdxlop = GPOS_NEW(m_pmp) CDXLScalarJoinFilter(m_pmp);
			break;
		case EdxlopScalarOneTimeFilter:
			pdxlop = GPOS_NEW(m_pmp) CDXLScalarOneTimeFilter(m_pmp);
			break;
		default:
			GPOS_ASSERT(!"Unrecognized filter type");
	}


	CDXLNode *pdxlnFilter = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	CDXLNode *pdxlnCond = PdxlnScCondFromQual(plQual, pmapvarcolid, pfHasDistributedTables);

	if (NULL != pdxlnCond)
	{
		pdxlnFilter->AddChild(pdxlnCond);
	}

	return pdxlnFilter;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnFromSublink
//
//	@doc:
//		Create a DXL node from a GPDB sublink node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnFromSublink
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	const SubLink *psublink = (SubLink *) pexpr;

	switch (psublink->subLinkType)
	{
		case EXPR_SUBLINK:
			return PdxlnScSubqueryFromSublink(psublink, pmapvarcolid);

		case ALL_SUBLINK:
		case ANY_SUBLINK:
			return PdxlnQuantifiedSubqueryFromSublink(psublink, pmapvarcolid);

		case EXISTS_SUBLINK:
			return PdxlnExistSubqueryFromSublink(psublink, pmapvarcolid);

		default:
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Non-Scalar Subquery"));
				return NULL;
			}

	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnQuantifiedSubqueryFromSublink
//
//	@doc:
//		Create ANY / ALL quantified sub query from a GPDB sublink node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnQuantifiedSubqueryFromSublink
	(
	const SubLink *psublink,
	const CMappingVarColId* pmapvarcolid
	)
{
	CMappingVarColId *pmapvarcolidCopy = pmapvarcolid->PmapvarcolidCopy(m_pmp);

	CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
	ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_pmp,
							m_pmda,
							m_pidgtorCol,
							m_pidgtorCTE,
							pmapvarcolidCopy,
							(Query *) psublink->subselect,
							m_ulQueryLevel + 1,
							m_phmulCTEEntries
							);

	CDXLNode *pdxlnInner = ptrquerytodxl->PdxlnFromQueryInternal();

	DrgPdxln *pdrgpdxlnQueryOutput = ptrquerytodxl->PdrgpdxlnQueryOutput();
	DrgPdxln *pdrgpdxlnCTE = ptrquerytodxl->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_pdrgpdxlnCTE, pdrgpdxlnCTE);

	if (1 != pdrgpdxlnQueryOutput->UlLength())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Non-Scalar Subquery"));
	}

	m_fHasDistributedTables = m_fHasDistributedTables || ptrquerytodxl->FHasDistributedTables();

	CDXLNode *pdxlnIdent = (*pdrgpdxlnQueryOutput)[0];
	GPOS_ASSERT(NULL != pdxlnIdent);

	// get dxl scalar identifier
	CDXLScalarIdent *pdxlopIdent = dynamic_cast<CDXLScalarIdent *>(pdxlnIdent->Pdxlop());

	// get the dxl column reference
	const CDXLColRef *pdxlcr = pdxlopIdent->Pdxlcr();
	const ULONG ulColId = pdxlcr->UlID();

	// get the test expression
	GPOS_ASSERT(IsA(psublink->testexpr, OpExpr));
	OpExpr *popexpr = (OpExpr*) psublink->testexpr;

	IMDId *pmdid = GPOS_NEW(m_pmp) CMDIdGPDB(popexpr->opno);

	// get operator name
	const CWStringConst *pstr = PstrOpName(pmdid);

	// translate left hand side of the expression
	GPOS_ASSERT(NULL != popexpr->args);
	Expr* pexprLHS = (Expr*) gpdb::PvListNth(popexpr->args, 0);

	CDXLNode *pdxlnOuter = PdxlnScOpFromExpr(pexprLHS, pmapvarcolid);

	CDXLNode *pdxln = NULL;
	CDXLScalar *pdxlopSubquery = NULL;

	GPOS_ASSERT(ALL_SUBLINK == psublink->subLinkType || ANY_SUBLINK == psublink->subLinkType);
	if (ALL_SUBLINK == psublink->subLinkType)
	{
		pdxlopSubquery = GPOS_NEW(m_pmp) CDXLScalarSubqueryAll
								(
								m_pmp,
								pmdid,
								GPOS_NEW(m_pmp) CMDName(m_pmp, pstr),
								ulColId
								);

	}
	else
	{
		pdxlopSubquery = GPOS_NEW(m_pmp) CDXLScalarSubqueryAny
								(
								m_pmp,
								pmdid,
								GPOS_NEW(m_pmp) CMDName(m_pmp, pstr),
								ulColId
								);
	}

	pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlopSubquery);

	pdxln->AddChild(pdxlnOuter);
	pdxln->AddChild(pdxlnInner);

#ifdef GPOS_DEBUG
	pdxln->Pdxlop()->AssertValid(pdxln, false /* fValidateChildren */);
#endif

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnScSubqueryFromSublink
//
//	@doc:
//		Create a scalar subquery from a GPDB sublink node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnScSubqueryFromSublink
	(
	const SubLink *psublink,
	const CMappingVarColId *pmapvarcolid
	)
{
	CMappingVarColId *pmapvarcolidCopy = pmapvarcolid->PmapvarcolidCopy(m_pmp);

	Query *pquerySublink = (Query *) psublink->subselect;
	CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
	ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_pmp,
							m_pmda,
							m_pidgtorCol,
							m_pidgtorCTE,
							pmapvarcolidCopy,
							pquerySublink,
							m_ulQueryLevel + 1,
							m_phmulCTEEntries
							);
	CDXLNode *pdxlnSubQuery = ptrquerytodxl->PdxlnFromQueryInternal();

	DrgPdxln *pdrgpdxlnQueryOutput = ptrquerytodxl->PdrgpdxlnQueryOutput();

	GPOS_ASSERT(1 == pdrgpdxlnQueryOutput->UlLength());

	DrgPdxln *pdrgpdxlnCTE = ptrquerytodxl->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_pdrgpdxlnCTE, pdrgpdxlnCTE);
	m_fHasDistributedTables = m_fHasDistributedTables || ptrquerytodxl->FHasDistributedTables();

	// get dxl scalar identifier
	CDXLNode *pdxlnIdent = (*pdrgpdxlnQueryOutput)[0];
	GPOS_ASSERT(NULL != pdxlnIdent);

	CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::PdxlopConvert(pdxlnIdent->Pdxlop());

	// get the dxl column reference
	const CDXLColRef *pdxlcr = pdxlopIdent->Pdxlcr();
	const ULONG ulColId = pdxlcr->UlID();

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarSubquery(m_pmp, ulColId));

	pdxln->AddChild(pdxlnSubQuery);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnArray
//
//	@doc:
//		Translate array
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnArray
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, ArrayExpr));

	const ArrayExpr *parrayexpr = (ArrayExpr *) pexpr;

	CDXLScalarArray *pdxlop =
			GPOS_NEW(m_pmp) CDXLScalarArray
						(
						m_pmp,
						GPOS_NEW(m_pmp) CMDIdGPDB(parrayexpr->element_typeid),
						GPOS_NEW(m_pmp) CMDIdGPDB(parrayexpr->array_typeid),
						parrayexpr->multidims
						);

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	TranslateScalarChildren(pdxln, parrayexpr->elements, pmapvarcolid);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnArrayRef
//
//	@doc:
//		Translate arrayref
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnArrayRef
	(
	const Expr *pexpr,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(IsA(pexpr, ArrayRef));

	const ArrayRef *parrayref = (ArrayRef *) pexpr;
	Oid restype;

	/* slice and/or store operations yield the array type */
	if (parrayref->reflowerindexpr || parrayref->refassgnexpr)
		restype = parrayref->refarraytype;
	else
		restype = parrayref->refelemtype;

	CDXLScalarArrayRef *pdxlop =
			GPOS_NEW(m_pmp) CDXLScalarArrayRef
						(
						m_pmp,
						GPOS_NEW(m_pmp) CMDIdGPDB(parrayref->refelemtype),
						GPOS_NEW(m_pmp) CMDIdGPDB(parrayref->refarraytype),
						GPOS_NEW(m_pmp) CMDIdGPDB(restype)
						);

	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, pdxlop);

	// add children
	AddArrayIndexList(pdxln, parrayref->reflowerindexpr, CDXLScalarArrayRefIndexList::EilbLower, pmapvarcolid);
	AddArrayIndexList(pdxln, parrayref->refupperindexpr, CDXLScalarArrayRefIndexList::EilbUpper, pmapvarcolid);

	pdxln->AddChild(PdxlnScOpFromExpr(parrayref->refexpr, pmapvarcolid));

	if (NULL != parrayref->refassgnexpr)
	{
		pdxln->AddChild(PdxlnScOpFromExpr(parrayref->refassgnexpr, pmapvarcolid));
	}

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::AddArrayIndexList
//
//	@doc:
//		Add an indexlist to the given DXL arrayref node
//
//---------------------------------------------------------------------------
void
CTranslatorScalarToDXL::AddArrayIndexList
	(
	CDXLNode *pdxln,
	List *plist,
	CDXLScalarArrayRefIndexList::EIndexListBound eilb,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(NULL != pdxln);
	GPOS_ASSERT(EdxlopScalarArrayRef == pdxln->Pdxlop()->Edxlop());
	GPOS_ASSERT(CDXLScalarArrayRefIndexList::EilbSentinel > eilb);

	CDXLNode *pdxlnIndexList =
			GPOS_NEW(m_pmp) CDXLNode
					(
					m_pmp,
					GPOS_NEW(m_pmp) CDXLScalarArrayRefIndexList(m_pmp, eilb)
					);

	TranslateScalarChildren(pdxlnIndexList, plist, pmapvarcolid);
	pdxln->AddChild(pdxlnIndexList);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PstrOpName
//
//	@doc:
//		Get the operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CTranslatorScalarToDXL::PstrOpName
	(
	IMDId *pmdid
	)
	const
{
	// get operator name
	const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pmdid);

	const CWStringConst *pstr = pmdscop->Mdname().Pstr();

	return pstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxlnExistSubqueryFromSublink
//
//	@doc:
//		Create a DXL EXISTS subquery node from the respective GPDB
//		sublink node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::PdxlnExistSubqueryFromSublink
	(
	const SubLink *psublink,
	const CMappingVarColId* pmapvarcolid
	)
{
	GPOS_ASSERT(NULL != psublink);
	CMappingVarColId *pmapvarcolidCopy = pmapvarcolid->PmapvarcolidCopy(m_pmp);

	CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
	ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_pmp,
							m_pmda,
							m_pidgtorCol,
							m_pidgtorCTE,
							pmapvarcolidCopy,
							(Query *) psublink->subselect,
							m_ulQueryLevel + 1,
							m_phmulCTEEntries
							);
	CDXLNode *pdxlnRoot = ptrquerytodxl->PdxlnFromQueryInternal();
	
	DrgPdxln *pdrgpdxlnCTE = ptrquerytodxl->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_pdrgpdxlnCTE, pdrgpdxlnCTE);
	m_fHasDistributedTables = m_fHasDistributedTables || ptrquerytodxl->FHasDistributedTables();
	
	CDXLNode *pdxln = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarSubqueryExists(m_pmp));
	pdxln->AddChild(pdxlnRoot);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pdatum
//
//	@doc:
//		Create CDXLDatum from GPDB datum
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::Pdxldatum
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG ulLen,
	Datum datum
	)
{
	static const SDXLDatumTranslatorElem rgTranslators[] =
		{
			{IMDType::EtiInt2  , &CTranslatorScalarToDXL::PdxldatumInt2},
			{IMDType::EtiInt4  , &CTranslatorScalarToDXL::PdxldatumInt4},
			{IMDType::EtiInt8 , &CTranslatorScalarToDXL::PdxldatumInt8},
			{IMDType::EtiBool , &CTranslatorScalarToDXL::PdxldatumBool},
			{IMDType::EtiOid  , &CTranslatorScalarToDXL::PdxldatumOid},
		};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);
	// find translator for the datum type
	PfPdxldatumFromDatum *pf = NULL;
	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		SDXLDatumTranslatorElem elem = rgTranslators[ul];
		if (pmdtype->Eti() == elem.eti)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		// generate a datum of generic type
		return PdxldatumGeneric(pmp, pmdtype, fNull, ulLen, datum);
	}
	else
	{
		return (*pf)(pmp, pmdtype, fNull, ulLen, datum);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumGeneric
//
//	@doc:
//		Translate a datum of generic type
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumGeneric
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG ulLen,
	Datum datum
	)
{
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(*pmdidMDC);

	BOOL fConstByVal = pmdtype->FByValue();
	BYTE *pba = Pba(pmp, pmdtype, fNull, ulLen, datum);
	ULONG ulLength = 0;
	if (!fNull)
	{
		ulLength = (ULONG) gpdb::SDatumSize(datum, pmdtype->FByValue(), ulLen);
	}

	CDouble dValue(0);
	if (CMDTypeGenericGPDB::FHasByteDoubleMapping(pmdid))
	{
		dValue = DValue(pmdid, fNull, pba, datum);
	}

	LINT lValue = 0;
	if (CMDTypeGenericGPDB::FHasByteLintMapping(pmdid))
	{
		lValue = LValue(pmdid, fNull, pba, ulLength);
	}

	return CMDTypeGenericGPDB::Pdxldatum(pmp, pmdid, fConstByVal, fNull, pba, ulLength, lValue, dValue);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumBool
//
//	@doc:
//		Translate a datum of type bool
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumBool
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->FByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(pmp) CDXLDatumBool(pmp, pmdid, fNull, gpdb::FBoolFromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumOid
//
//	@doc:
//		Translate a datum of type oid
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumOid
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->FByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(pmp) CDXLDatumOid(pmp, pmdid, fNull, gpdb::OidFromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumInt2
//
//	@doc:
//		Translate a datum of type int2
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumInt2
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->FByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(pmp) CDXLDatumInt2(pmp, pmdid, fNull, gpdb::SInt16FromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumInt4
//
//	@doc:
//		Translate a datum of type int4
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumInt4
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->FByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(pmp) CDXLDatumInt4(pmp, pmdid, fNull, gpdb::IInt32FromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumInt8
//
//	@doc:
//		Translate a datum of type int8
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumInt8
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->FByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::PmdidConvert(pmdtype->Pmdid());
	CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(pmp) CDXLDatumInt8(pmp, pmdid, fNull, gpdb::LlInt64FromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::DValue
//
//	@doc:
//		Extract the double value of the datum
//---------------------------------------------------------------------------
CDouble
CTranslatorScalarToDXL::DValue
	(
	IMDId *pmdid,
	BOOL fNull,
	BYTE *pba,
	Datum datum
	)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::FHasByteDoubleMapping(pmdid));

	double d = 0;

	if (fNull)
	{
		return CDouble(d);
	}

	if (pmdid->FEquals(&CMDIdGPDB::m_mdidNumeric))
	{
		Numeric num = (Numeric) (pba);
		if (NUMERIC_IS_NAN(num))
		{
			// in GPDB NaN is considered the largest numeric number.
			return CDouble(GPOS_FP_ABS_MAX);
		}

		d = gpdb::DNumericToDoubleNoOverflow(num);
	}
	else if (pmdid->FEquals(&CMDIdGPDB::m_mdidFloat4))
	{
		float4 f = gpdb::FpFloat4FromDatum(datum);

		if (isnan(f))
		{
			d = GPOS_FP_ABS_MAX;
		}
		else
		{
			d = (double) f;
		}
	}
	else if (pmdid->FEquals(&CMDIdGPDB::m_mdidFloat8))
	{
		d = gpdb::DFloat8FromDatum(datum);

		if (isnan(d))
		{
			d = GPOS_FP_ABS_MAX;
		}
	}
	else if (CMDTypeGenericGPDB::FTimeRelatedType(pmdid))
	{
		d = gpdb::DConvertTimeValueToScalar(datum, CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId());
	}
	else if (CMDTypeGenericGPDB::FNetworkRelatedType(pmdid))
	{
		d = gpdb::DConvertNetworkToScalar(datum, CMDIdGPDB::PmdidConvert(pmdid)->OidObjectId());
	}

	return CDouble(d);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pba
//
//	@doc:
//		Extract the byte array value of the datum. The result is NULL if datum is NULL
//---------------------------------------------------------------------------
BYTE *
CTranslatorScalarToDXL::Pba
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	ULONG ulLen,
	Datum datum
	)
{
	ULONG ulLength = 0;
	BYTE *pba = NULL;

	if (fNull)
	{
		return pba;
	}

	ulLength = (ULONG) gpdb::SDatumSize(datum, pmdtype->FByValue(), ulLen);
	GPOS_ASSERT(ulLength > 0);

	pba = GPOS_NEW_ARRAY(pmp, BYTE, ulLength);

	if (pmdtype->FByValue())
	{
		GPOS_ASSERT(ulLength <= ULONG(sizeof(Datum)));
		clib::PvMemCpy(pba, &datum, ulLength);
	}
	else
	{
		clib::PvMemCpy(pba, gpdb::PvPointerFromDatum(datum), ulLength);
	}

	return pba;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::LValue
//
//	@doc:
//		Extract the long int value of a datum
//---------------------------------------------------------------------------
LINT
CTranslatorScalarToDXL::LValue
	(
	IMDId *pmdid,
	BOOL fNull,
	BYTE *pba,
	ULONG ulLength
	)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::FHasByteLintMapping(pmdid));

	LINT lValue = 0;
	if (fNull)
	{
		return lValue;
	}

	if (pmdid->FEquals(&CMDIdGPDB::m_mdidCash))
	{
		// cash is a pass-by-ref type
		Datum datumConstVal = (Datum) 0;
		clib::PvMemCpy(&datumConstVal, pba, ulLength);
		// Date is internally represented as an int32
		lValue = (LINT) (gpdb::IInt32FromDatum(datumConstVal));

	}
	else
	{
		// use hash value
		ULONG ulHash = 0;
		if (fNull)
		{
			ulHash = gpos::UlHash<ULONG>(&ulHash);
		}
		else
		{
			ulHash = gpos::UlHash<BYTE>(pba);
			for (ULONG ul = 1; ul < ulLength; ul++)
			{
				ulHash = gpos::UlCombineHashes(ulHash, gpos::UlHash<BYTE>(&pba[ul]));
			}
		}

		lValue = (LINT) (ulHash / 4);
	}

	return lValue;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pdatum
//
//	@doc:
//		Create IDatum from GPDB datum
//---------------------------------------------------------------------------
IDatum *
CTranslatorScalarToDXL::Pdatum
	(
	IMemoryPool *pmp,
	const IMDType *pmdtype,
	BOOL fNull,
	Datum datum
	)
{
	ULONG ulLength = pmdtype->UlLength();
	if (!pmdtype->FByValue() && !fNull)
	{
		INT iLen = dynamic_cast<const CMDTypeGenericGPDB *>(pmdtype)->ILength();
		ulLength = (ULONG) gpdb::SDatumSize(datum, pmdtype->FByValue(), iLen);
	}
	GPOS_ASSERT(fNull || ulLength > 0);

	CDXLDatum *pdxldatum = CTranslatorScalarToDXL::Pdxldatum(pmp, pmdtype, fNull, ulLength, datum);
	IDatum *pdatum = pmdtype->Pdatum(pmp, pdxldatum);
	pdxldatum->Release();
	return pdatum;
}

// EOF
