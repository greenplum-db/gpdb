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

/*
 * GPDB_91_MERGE_FIXME: This allows us to call numeric_is_nan(). This is probably
 * a violation of some ORCA coding rule, because we don't do this elsewhere...
 */
extern "C" {
#include "utils/numeric.h"
}

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
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *pulidgtorCol,
	CIdGenerator *pulidgtorCTE,
	ULONG query_level,
	BOOL fQuery,
	HMUlCTEListEntry *phmulCTEEntries,
	DXLNodeArray *cte_dxlnode_array
	)
	:
	m_memory_pool(memory_pool),
	m_pmda(md_accessor),
	m_pidgtorCol(pulidgtorCol),
	m_pidgtorCTE(pulidgtorCTE),
	m_ulQueryLevel(query_level),
	m_fHasDistributedTables(false),
	m_fQuery(fQuery),
	m_eplsphoptype(EpspotNone),
	m_phmulCTEEntries(phmulCTEEntries),
	m_pdrgpdxlnCTE(cte_dxlnode_array)
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

	const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < arity; ul++)
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, Var));
	const Var * var = (Var *) pexpr;

	if (var->varattno == 0)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Whole-row variable"));
	}

	// column name
	const CWStringBase *str = var_col_id_mapping->GetOptColName(m_ulQueryLevel, var, m_eplsphoptype);

	// column id
	ULONG id;

	if(var->varattno != 0 || EpspotIndexScan == m_eplsphoptype || EpspotIndexOnlyScan == m_eplsphoptype)
	{
		id = var_col_id_mapping->GetColId(m_ulQueryLevel, var, m_eplsphoptype);
	}
	else
	{
		id = m_pidgtorCol->next_id();
	}
	CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, str);

	// create a column reference for the given var
	CDXLColRef *dxl_colref = GPOS_NEW(m_memory_pool) CDXLColRef
												(
												m_memory_pool,
												mdname,
												id,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(var->vartype),
												var->vartypmod
												);

	// create the scalar ident operator
	CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_memory_pool) CDXLScalarIdent
													(
													m_memory_pool,
													dxl_colref
													);

	// create the DXL node holding the scalar ident operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopIdent);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping,
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
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, sz);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion, str->GetBuffer());
	}

	CDXLNode *pdxlnReturn = (this->*pf)(pexpr, var_col_id_mapping);

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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, DistinctExpr));
	const DistinctExpr *pdistexpr = (DistinctExpr *) pexpr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::ListLength(pdistexpr->args));

	CDXLNode *pdxlnLeft = PdxlnScOpFromExpr
							(
							(Expr *) gpdb::PvListNth(pdistexpr->args, 0),
							var_col_id_mapping
							);

	CDXLNode *pdxlnRight = PdxlnScOpFromExpr
							(
							(Expr *) gpdb::PvListNth(pdistexpr->args, 1),
							var_col_id_mapping
							);

	GPOS_ASSERT(NULL != pdxlnLeft);
	GPOS_ASSERT(NULL != pdxlnRight);

	CDXLScalarDistinctComp *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarDistinctComp
														(
														m_memory_pool,
														GPOS_NEW(m_memory_pool) CMDIdGPDB(pdistexpr->opno)
														);

	// create the DXL node holding the scalar distinct comparison operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// add children in the right order
	dxlnode->AddChild(pdxlnLeft);
	dxlnode->AddChild(pdxlnRight);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, OpExpr));
	const OpExpr *popexpr = (OpExpr *) pexpr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::ListLength(popexpr->args));

	Expr *pexprLeft = (Expr *) gpdb::PvListNth(popexpr->args, 0);
	Expr *pexprRight = (Expr *) gpdb::PvListNth(popexpr->args, 1);

	CDXLNode *pdxlnLeft = PdxlnScOpFromExpr(pexprLeft, var_col_id_mapping);
	CDXLNode *pdxlnRight = PdxlnScOpFromExpr(pexprRight, var_col_id_mapping);

	GPOS_ASSERT(NULL != pdxlnLeft);
	GPOS_ASSERT(NULL != pdxlnRight);

	CMDIdGPDB *pmdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(popexpr->opno);

	// get operator name
	const CWStringConst *str = GetDXLArrayCmpType(pmdid);

	CDXLScalarComp *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarComp(m_memory_pool, pmdid, GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()));

	// create the DXL node holding the scalar comparison operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// add children in the right order
	dxlnode->AddChild(pdxlnLeft);
	dxlnode->AddChild(pdxlnRight);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, OpExpr));

	const OpExpr *popexpr = (OpExpr *) pexpr;

	// check if this is a scalar comparison
	CMDIdGPDB *return_type_mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(((OpExpr *) pexpr)->opresulttype);
	const IMDType *pmdtype= m_pmda->Pmdtype(return_type_mdid);

	const ULONG ulArgs = gpdb::ListLength(popexpr->args);

	if (IMDType::EtiBool ==  pmdtype->GetDatumType() && 2 == ulArgs)
	{
		return_type_mdid->Release();
		return PdxlnScCmpFromOpExpr(pexpr, var_col_id_mapping);
	}

	// get operator name and id
	IMDId *pmdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(popexpr->opno);
	const CWStringConst *str = GetDXLArrayCmpType(pmdid);

	CDXLScalarOpExpr *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarOpExpr(m_memory_pool, pmdid, return_type_mdid, GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()));

	// create the DXL node holding the scalar opexpr
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// process arguments
	TranslateScalarChildren(dxlnode, popexpr->args, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, NullIfExpr));
	const NullIfExpr *pnullifexpr = (NullIfExpr *) pexpr;

	GPOS_ASSERT(2 == gpdb::ListLength(pnullifexpr->args));

	CDXLScalarNullIf *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarNullIf(m_memory_pool, GPOS_NEW(m_memory_pool) CMDIdGPDB(pnullifexpr->opno), GPOS_NEW(m_memory_pool) CMDIdGPDB(pnullifexpr->opresulttype));

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// process arguments
	TranslateScalarChildren(dxlnode, pnullifexpr->args, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	return PdxlnScArrayCompFromExpr(pexpr, var_col_id_mapping);
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, ScalarArrayOpExpr));
	const ScalarArrayOpExpr *pscarrayopexpr = (ScalarArrayOpExpr *) pexpr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::ListLength(pscarrayopexpr->args));

	Expr *pexprLeft = (Expr*) gpdb::PvListNth(pscarrayopexpr->args, 0);
	CDXLNode *pdxlnLeft = PdxlnScOpFromExpr(pexprLeft, var_col_id_mapping);

	Expr *pexprRight = (Expr*) gpdb::PvListNth(pscarrayopexpr->args, 1);

	// If the argument array is an array Const, try to transform it to an
	// ArrayExpr, to allow ORCA to optimize it better. (ORCA knows how to
	// extract elements of an ArrayExpr, but doesn't currently know how
	// to do it from an array-typed Const.)
	if (IsA(pexprRight, Const))
		pexprRight = gpdb::PexprTransformArrayConstToArrayExpr((Const *) pexprRight);

	CDXLNode *pdxlnRight = PdxlnScOpFromExpr(pexprRight, var_col_id_mapping);

	GPOS_ASSERT(NULL != pdxlnLeft);
	GPOS_ASSERT(NULL != pdxlnRight);

	// get operator name
	CMDIdGPDB *mdid_op = GPOS_NEW(m_memory_pool) CMDIdGPDB(pscarrayopexpr->opno);
	const IMDScalarOp *md_scalar_op = m_pmda->Pmdscop(mdid_op);
	mdid_op->Release();

	const CWStringConst *str = md_scalar_op->Mdname().GetMDName();
	GPOS_ASSERT(NULL != str);

	EdxlArrayCompType edxlarraycomptype = Edxlarraycomptypeany;

	if(!pscarrayopexpr->useOr)
	{
		edxlarraycomptype = Edxlarraycomptypeall;
	}

	CDXLScalarArrayComp *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarArrayComp(m_memory_pool, GPOS_NEW(m_memory_pool) CMDIdGPDB(pscarrayopexpr->opno), GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()), edxlarraycomptype);

	// create the DXL node holding the scalar opexpr
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// add children in the right order
	dxlnode->AddChild(pdxlnLeft);
	dxlnode->AddChild(pdxlnRight);

	return dxlnode;
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
	const CMappingVarColId * // var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, Const));
	const Const *pconst = (Const *) pexpr;

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarConstValue
												(
												m_memory_pool,
												GetDatumVal(pconst)
												)
									);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDatumVal
//
//	@doc:
//		Create a DXL datum from a GPDB Const
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::GetDatumVal
	(
	IMemoryPool *memory_pool,
	CMDAccessor *mda,
	const Const *pconst
	)
{
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(pconst->consttype);
	const IMDType *pmdtype= mda->Pmdtype(pmdid);
	pmdid->Release();

 	// translate gpdb datum into a DXL datum
	CDXLDatum *datum_dxl = CTranslatorScalarToDXL::GetDatumVal(memory_pool, pmdtype, pconst->consttypmod, pconst->constisnull,
															 pconst->constlen,
															 pconst->constvalue);

	return datum_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDatumVal
//
//	@doc:
//		Create a DXL datum from a GPDB Const
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::GetDatumVal
	(
	const Const *pconst
	)
	const
{
	return GetDatumVal(m_memory_pool, m_pmda, pconst);
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, BoolExpr));
	const BoolExpr *pboolexpr = (BoolExpr *) pexpr;
	GPOS_ASSERT(0 < gpdb::ListLength(pboolexpr->args));

	EdxlBoolExprType boolexptype = EdxlbooltypeFromGPDBBoolType(pboolexpr->boolop);
	GPOS_ASSERT(EdxlBoolExprTypeSentinel != boolexptype);

	// create the DXL node holding the scalar boolean operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarBoolExpr(m_memory_pool, boolexptype));

	ULONG ulCount = gpdb::ListLength(pboolexpr->args);

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

	TranslateScalarChildren(dxlnode, pboolexpr->args, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
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
	const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < arity; ul++)
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
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarBooleanTest(m_memory_pool,edxlbt)
									);

	CDXLNode *dxlnode_arg = PdxlnScOpFromExpr(pbooleantest->arg, var_col_id_mapping);
	GPOS_ASSERT(NULL != dxlnode_arg);

	dxlnode->AddChild(dxlnode_arg);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, NullTest));
	const NullTest *pnulltest = (NullTest *) pexpr;

	GPOS_ASSERT(NULL != pnulltest->arg);
	CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pnulltest->arg, var_col_id_mapping);

	GPOS_ASSERT(NULL != pdxlnChild);
	GPOS_ASSERT(IS_NULL == pnulltest->nulltesttype || IS_NOT_NULL == pnulltest->nulltesttype);

	BOOL is_null = false;
	if (IS_NULL == pnulltest->nulltesttype)
	{
		is_null = true;
	}

	// create the DXL node holding the scalar NullTest operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarNullTest(m_memory_pool, is_null));
	dxlnode->AddChild(pdxlnChild);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, CoalesceExpr));

	CoalesceExpr *pcoalesceexpr = (CoalesceExpr *) pexpr;
	GPOS_ASSERT(NULL != pcoalesceexpr->args);

	CDXLScalarCoalesce *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarCoalesce
											(
											m_memory_pool,
											GPOS_NEW(m_memory_pool) CMDIdGPDB(pcoalesceexpr->coalescetype)
											);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	TranslateScalarChildren(dxlnode, pcoalesceexpr->args, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, MinMaxExpr));

	MinMaxExpr *pminmaxexpr = (MinMaxExpr *) pexpr;
	GPOS_ASSERT(NULL != pminmaxexpr->args);

	CDXLScalarMinMax::EdxlMinMaxType min_max_type = CDXLScalarMinMax::EmmtSentinel;
	if (IS_GREATEST == pminmaxexpr->op)
	{
		min_max_type = CDXLScalarMinMax::EmmtMax;
	}
	else
	{
		GPOS_ASSERT(IS_LEAST == pminmaxexpr->op);
		min_max_type = CDXLScalarMinMax::EmmtMin;
	}

	CDXLScalarMinMax *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarMinMax
											(
											m_memory_pool,
											GPOS_NEW(m_memory_pool) CMDIdGPDB(pminmaxexpr->minmaxtype),
											min_max_type
											);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	TranslateScalarChildren(dxlnode, pminmaxexpr->args, var_col_id_mapping);

	return dxlnode;
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
	CDXLNode *dxlnode,
	List *plist,
	const CMappingVarColId* var_col_id_mapping,
	BOOL *pfHasDistributedTables // output
	)
{
	ListCell *lc = NULL;
	ForEach (lc, plist)
	{
		Expr *pexprChild = (Expr *) lfirst(lc);
		CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pexprChild, var_col_id_mapping, pfHasDistributedTables);
		GPOS_ASSERT(NULL != pdxlnChild);
		dxlnode->AddChild(pdxlnChild);
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
	const CMappingVarColId* var_col_id_mapping
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
		return PdxlnScIfStmtFromCaseExpr(pcaseexpr, var_col_id_mapping);
	}

	return PdxlnScSwitchFromCaseExpr(pcaseexpr, var_col_id_mapping);
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT (NULL != pcaseexpr->arg);

	CDXLScalarSwitch *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarSwitch
												(
												m_memory_pool,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(pcaseexpr->casetype)
												);
	CDXLNode *pdxlnSwitch = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// translate the switch expression
	CDXLNode *dxlnode_arg = PdxlnScOpFromExpr(pcaseexpr->arg, var_col_id_mapping);
	pdxlnSwitch->AddChild(dxlnode_arg);

	// translate the cases
	ListCell *lc = NULL;
	ForEach (lc, pcaseexpr->args)
	{
		CaseWhen *pexpr = (CaseWhen *) lfirst(lc);

		CDXLScalarSwitchCase *pdxlopCase = GPOS_NEW(m_memory_pool) CDXLScalarSwitchCase(m_memory_pool);
		CDXLNode *pdxlnCase = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopCase);

		CDXLNode *pdxlnCmpExpr = PdxlnScOpFromExpr(pexpr->expr, var_col_id_mapping);
		GPOS_ASSERT(NULL != pdxlnCmpExpr);

		CDXLNode *pdxlnResult = PdxlnScOpFromExpr(pexpr->result, var_col_id_mapping);
		GPOS_ASSERT(NULL != pdxlnResult);

		pdxlnCase->AddChild(pdxlnCmpExpr);
		pdxlnCase->AddChild(pdxlnResult);

		// add current case to switch node
		pdxlnSwitch->AddChild(pdxlnCase);
	}

	// translate the "else" clause
	if (NULL != pcaseexpr->defresult)
	{
		CDXLNode *pdxlnDefaultResult = PdxlnScOpFromExpr(pcaseexpr->defresult, var_col_id_mapping);
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
	const CMappingVarColId* //var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, CaseTestExpr));
	const CaseTestExpr *pcasetestexpr = (CaseTestExpr *) pexpr;
	CDXLScalarCaseTest *pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarCaseTest
												(
												m_memory_pool,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(pcasetestexpr->typeId)
												);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT (NULL == pcaseexpr->arg);
	const ULONG ulWhenClauseCount = gpdb::ListLength(pcaseexpr->args);

	CDXLNode *pdxlnRootIfTree = NULL;
	CDXLNode *pdxlnCurr = NULL;

	for (ULONG ul = 0; ul < ulWhenClauseCount; ul++)
	{
		CDXLScalarIfStmt *pdxlopIfstmtNew = GPOS_NEW(m_memory_pool) CDXLScalarIfStmt
															(
															m_memory_pool,
															GPOS_NEW(m_memory_pool) CMDIdGPDB(pcaseexpr->casetype)
															);

		CDXLNode *pdxlnIfStmtNew = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopIfstmtNew);

		CaseWhen *pexpr = (CaseWhen *) gpdb::PvListNth(pcaseexpr->args, ul);
		GPOS_ASSERT(IsA(pexpr, CaseWhen));

		CDXLNode *pdxlnCond = PdxlnScOpFromExpr(pexpr->expr, var_col_id_mapping);
		CDXLNode *pdxlnResult = PdxlnScOpFromExpr(pexpr->result, var_col_id_mapping);

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
		CDXLNode *pdxlnDefaultResult = PdxlnScOpFromExpr(pcaseexpr->defresult, var_col_id_mapping);
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, RelabelType));

	const RelabelType *prelabeltype = (RelabelType *) pexpr;

	GPOS_ASSERT(NULL != prelabeltype->arg);

	CDXLNode *pdxlnChild = PdxlnScOpFromExpr(prelabeltype->arg, var_col_id_mapping);

	GPOS_ASSERT(NULL != pdxlnChild);

	// create the DXL node holding the scalar boolean operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarCast
												(
												m_memory_pool,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(prelabeltype->resulttype),
												GPOS_NEW(m_memory_pool) CMDIdGPDB(0) // casting function oid
												)
									);
	dxlnode->AddChild(pdxlnChild);

	return dxlnode;
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
        const CMappingVarColId* var_col_id_mapping
        )
{
        GPOS_ASSERT(IsA(pexpr, CoerceToDomain));

        const CoerceToDomain *pcoerce = (CoerceToDomain *) pexpr;

        GPOS_ASSERT(NULL != pcoerce->arg);

        CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pcoerce->arg, var_col_id_mapping);

        GPOS_ASSERT(NULL != pdxlnChild);

        // create the DXL node holding the scalar boolean operator
        CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
                                                                        (
                                                                        m_memory_pool,
                                                                        GPOS_NEW(m_memory_pool) CDXLScalarCoerceToDomain
                                                                                                (
                                                                                                m_memory_pool,
                                                                                                GPOS_NEW(m_memory_pool) CMDIdGPDB(pcoerce->resulttype),
                                                                                               pcoerce->resulttypmod,
                                                                                               (EdxlCoercionForm) pcoerce->coercionformat,
                                                                                               pcoerce->location
                                                                                                )
                                                                        );
        dxlnode->AddChild(pdxlnChild);

        return dxlnode;
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
        const CMappingVarColId* var_col_id_mapping
        )
{
        GPOS_ASSERT(IsA(pexpr, CoerceViaIO));

        const CoerceViaIO *pcoerce = (CoerceViaIO *) pexpr;

        GPOS_ASSERT(NULL != pcoerce->arg);

        CDXLNode *pdxlnChild = PdxlnScOpFromExpr(pcoerce->arg, var_col_id_mapping);

        GPOS_ASSERT(NULL != pdxlnChild);

        // create the DXL node holding the scalar boolean operator
        CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
                                                                        (
                                                                        m_memory_pool,
                                                                        GPOS_NEW(m_memory_pool) CDXLScalarCoerceViaIO
                                                                                                (
                                                                                                m_memory_pool,
                                                                                                GPOS_NEW(m_memory_pool) CMDIdGPDB(pcoerce->resulttype),
                                                                                               -1,
                                                                                               (EdxlCoercionForm) pcoerce->coerceformat,
                                                                                               pcoerce->location
                                                                                                )
                                                                        );
        dxlnode->AddChild(pdxlnChild);

        return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, ArrayCoerceExpr));
	const ArrayCoerceExpr *parraycoerce = (ArrayCoerceExpr *) pexpr;
	
	GPOS_ASSERT(NULL != parraycoerce->arg);
	
	CDXLNode *pdxlnChild = PdxlnScOpFromExpr(parraycoerce->arg, var_col_id_mapping);
	
	GPOS_ASSERT(NULL != pdxlnChild);
	
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
					(
					m_memory_pool,
					GPOS_NEW(m_memory_pool) CDXLScalarArrayCoerceExpr
							(
							m_memory_pool,
							GPOS_NEW(m_memory_pool) CMDIdGPDB(parraycoerce->elemfuncid),
							GPOS_NEW(m_memory_pool) CMDIdGPDB(parraycoerce->resulttype),
							parraycoerce->resulttypmod,
							parraycoerce->isExplicit,
							(EdxlCoercionForm) parraycoerce->coerceformat,
							parraycoerce->location
							)
					);
	
        dxlnode->AddChild(pdxlnChild);

        return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, FuncExpr));
	const FuncExpr *pfuncexpr = (FuncExpr *) pexpr;
	int32 type_modifier = gpdb::IExprTypeMod((Node *) pexpr);

	CMDIdGPDB *mdid_func = GPOS_NEW(m_memory_pool) CMDIdGPDB(pfuncexpr->funcid);

	// create the DXL node holding the scalar funcexpr
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarFuncExpr
												(
												m_memory_pool,
												mdid_func,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(pfuncexpr->funcresulttype),
												type_modifier,
												pfuncexpr->funcretset
												)
									);

	const IMDFunction *pmdfunc = m_pmda->Pmdfunc(mdid_func);
	if (IMDFunction::EfsVolatile == pmdfunc->GetFuncStability())
	{
		ListCell *lc = NULL;
		ForEach (lc, pfuncexpr->args)
		{
			Node *pnodeArg = (Node *) lfirst(lc);
			if (CTranslatorUtils::HasSubquery(pnodeArg))
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
						GPOS_WSZ_LIT("Volatile functions with subqueries in arguments"));
			}
		}
	}

	TranslateScalarChildren(dxlnode, pfuncexpr->args, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
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
	const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) paggref->aggstage == pulElem[0])
		{
			edxlaggstage = (EdxlAggrefStage) pulElem[1];
			break;
		}
	}
	GPOS_ASSERT(EdxlaggstageSentinel != edxlaggstage && "Invalid agg stage");

	CMDIdGPDB *pmdidAgg = GPOS_NEW(m_memory_pool) CMDIdGPDB(paggref->aggfnoid);
	const IMDAggregate *pmdagg = m_pmda->Pmdagg(pmdidAgg);

	GPOS_ASSERT(!pmdagg->IsOrdered());

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

	IMDId *mdid_return_type = CScalarAggFunc::PmdidLookupReturnType(pmdidAgg, (EdxlaggstageNormal == edxlaggstage), m_pmda);
	IMDId *pmdidResolvedRetType = NULL;
	if (m_pmda->Pmdtype(mdid_return_type)->IsAmbiguous())
	{
		// if return type given by MD cache is ambiguous, use type provided by aggref node
		pmdidResolvedRetType = GPOS_NEW(m_memory_pool) CMDIdGPDB(paggref->aggtype);
	}

	CDXLScalarAggref *pdxlopAggref = GPOS_NEW(m_memory_pool) CDXLScalarAggref(m_memory_pool, pmdidAgg, pmdidResolvedRetType, aggDistinct, edxlaggstage);

	// create the DXL node holding the scalar aggref
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopAggref);

	// translate args
	ListCell *lc;
	ForEach (lc, paggref->args)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		CDXLNode *pdxlnChild = PdxlnScOpFromExpr(tle->expr, var_col_id_mapping, NULL);
		GPOS_ASSERT(NULL != pdxlnChild);
		dxlnode->AddChild(pdxlnChild);
	}

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetWindowFrame
//
//	@doc:
//		Create a DXL window frame from a GPDB WindowFrame
//---------------------------------------------------------------------------
CDXLWindowFrame *
CTranslatorScalarToDXL::GetWindowFrame
	(
	int frameOptions,
	const Node *startOffset,
	const Node *endOffset,
	const CMappingVarColId* var_col_id_mapping,
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

	CDXLNode *pdxlnLeadEdge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, true /* fLeading */, edxlfbLead));
	CDXLNode *pdxlnTrailEdge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, false /* fLeading */, edxlfbTrail));

	// translate the lead and trail value
	if (NULL != endOffset)
	{
		pdxlnLeadEdge->AddChild(PdxlnWindowFrameEdgeVal(endOffset, var_col_id_mapping, pdxlnNewChildScPrL, pfHasDistributedTables));
	}

	if (NULL != startOffset)
	{
		pdxlnTrailEdge->AddChild(PdxlnWindowFrameEdgeVal(startOffset, var_col_id_mapping, pdxlnNewChildScPrL, pfHasDistributedTables));
	}

	CDXLWindowFrame *pdxlWf = GPOS_NEW(m_memory_pool) CDXLWindowFrame(m_memory_pool, edxlfs, edxlfes, pdxlnLeadEdge, pdxlnTrailEdge);

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
	const CMappingVarColId* var_col_id_mapping,
	CDXLNode *pdxlnNewChildScPrL,
	BOOL *pfHasDistributedTables
	)
{
	CDXLNode *pdxlnVal = PdxlnScOpFromExpr((Expr *) pnode, var_col_id_mapping, pfHasDistributedTables);

	if (m_fQuery && !IsA(pnode, Var) && !IsA(pnode, Const))
	{
		GPOS_ASSERT(NULL != pdxlnNewChildScPrL);
		CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
		CMDName *pmdnameAlias = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol);
		ULONG ulPrElId = m_pidgtorCol->next_id();

		// construct a projection element
		CDXLNode *pdxlnPrEl = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, ulPrElId, pmdnameAlias));
		pdxlnPrEl->AddChild(pdxlnVal);

		// add it to the computed columns project list
		pdxlnNewChildScPrL->AddChild(pdxlnPrEl);

		// construct a new scalar ident
		CDXLScalarIdent *pdxlopIdent = GPOS_NEW(m_memory_pool) CDXLScalarIdent
													(
													m_memory_pool,
													GPOS_NEW(m_memory_pool) CDXLColRef
																(
																m_memory_pool,
																GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &strUnnamedCol),
																ulPrElId,
																GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::OidExprType(const_cast<Node*>(pnode))),
																gpdb::IExprTypeMod(const_cast<Node*>(pnode))
																)
													);

		pdxlnVal = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopIdent);
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
	const CMappingVarColId* var_col_id_mapping
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

	const ULONG arity = GPOS_ARRAY_SIZE(rgrgulMapping);
	EdxlWinStage dxl_win_stage = EdxlwinstageSentinel;

	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *pulElem = rgrgulMapping[ul];
		if ((ULONG) pwindowfunc->winstage == pulElem[0])
		{
			dxl_win_stage = (EdxlWinStage) pulElem[1];
			break;
		}
	}

	ULONG ulWinSpecPos = (ULONG) 0;
	if (m_fQuery)
	{
		ulWinSpecPos = (ULONG) pwindowfunc->winref - 1;
	}

	GPOS_ASSERT(EdxlwinstageSentinel != dxl_win_stage && "Invalid window stage");

	/*
	 * ORCA's ScalarWindowRef object doesn't have fields for the 'winstar'
	 * and 'winagg', so we lose that information in the translation.
	 * Fortunately, the executor currently doesn't need those fields to
	 * be set correctly.
	 */
	CDXLScalarWindowRef *pdxlopWinref = GPOS_NEW(m_memory_pool) CDXLScalarWindowRef
													(
													m_memory_pool,
													GPOS_NEW(m_memory_pool) CMDIdGPDB(pwindowfunc->winfnoid),
													GPOS_NEW(m_memory_pool) CMDIdGPDB(pwindowfunc->wintype),
													pwindowfunc->windistinct,
													pwindowfunc->winstar,
													pwindowfunc->winagg,
													dxl_win_stage,
													ulWinSpecPos
													);

	// create the DXL node holding the scalar aggref
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopWinref);

	TranslateScalarChildren(dxlnode, pwindowfunc->args, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping,
	BOOL *pfHasDistributedTables
	)
{
	if (NULL == plQual || 0 == gpdb::ListLength(plQual))
	{
		return NULL;
	}

	if (1 == gpdb::ListLength(plQual))
	{
		Expr *pexpr = (Expr *) gpdb::PvListNth(plQual, 0);
		return PdxlnScOpFromExpr(pexpr, var_col_id_mapping, pfHasDistributedTables);
	}
	else
	{
		// GPDB assumes that if there are a list of qual conditions then it is an implicit AND operation
		// Here we build the left deep AND tree
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarBoolExpr(m_memory_pool, Edxland));

		TranslateScalarChildren(dxlnode, plQual, var_col_id_mapping, pfHasDistributedTables);

		return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping,
	Edxlopid edxlopFilterType,
	BOOL *pfHasDistributedTables // output
	)
{
	CDXLScalarFilter *pdxlop = NULL;

	switch (edxlopFilterType)
	{
		case EdxlopScalarFilter:
			pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarFilter(m_memory_pool);
			break;
		case EdxlopScalarJoinFilter:
			pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarJoinFilter(m_memory_pool);
			break;
		case EdxlopScalarOneTimeFilter:
			pdxlop = GPOS_NEW(m_memory_pool) CDXLScalarOneTimeFilter(m_memory_pool);
			break;
		default:
			GPOS_ASSERT(!"Unrecognized filter type");
	}


	CDXLNode *filter_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	CDXLNode *pdxlnCond = PdxlnScCondFromQual(plQual, var_col_id_mapping, pfHasDistributedTables);

	if (NULL != pdxlnCond)
	{
		filter_dxlnode->AddChild(pdxlnCond);
	}

	return filter_dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	const SubLink *psublink = (SubLink *) pexpr;

	switch (psublink->subLinkType)
	{
		case EXPR_SUBLINK:
			return PdxlnScSubqueryFromSublink(psublink, var_col_id_mapping);

		case ALL_SUBLINK:
		case ANY_SUBLINK:
			return PdxlnQuantifiedSubqueryFromSublink(psublink, var_col_id_mapping);

		case EXISTS_SUBLINK:
			return PdxlnExistSubqueryFromSublink(psublink, var_col_id_mapping);

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
	const CMappingVarColId* var_col_id_mapping
	)
{
	CMappingVarColId *pmapvarcolidCopy = var_col_id_mapping->CopyMapColId(m_memory_pool);

	CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
	ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_memory_pool,
							m_pmda,
							m_pidgtorCol,
							m_pidgtorCTE,
							pmapvarcolidCopy,
							(Query *) psublink->subselect,
							m_ulQueryLevel + 1,
							m_phmulCTEEntries
							);

	CDXLNode *pdxlnInner = ptrquerytodxl->PdxlnFromQueryInternal();

	DXLNodeArray *query_output_dxlnode_array = ptrquerytodxl->PdrgpdxlnQueryOutput();
	DXLNodeArray *cte_dxlnode_array = ptrquerytodxl->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_pdrgpdxlnCTE, cte_dxlnode_array);

	if (1 != query_output_dxlnode_array->Size())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Non-Scalar Subquery"));
	}

	m_fHasDistributedTables = m_fHasDistributedTables || ptrquerytodxl->FHasDistributedTables();

	CDXLNode *dxl_sc_ident = (*query_output_dxlnode_array)[0];
	GPOS_ASSERT(NULL != dxl_sc_ident);

	// get dxl scalar identifier
	CDXLScalarIdent *pdxlopIdent = dynamic_cast<CDXLScalarIdent *>(dxl_sc_ident->GetOperator());

	// get the dxl column reference
	const CDXLColRef *dxl_colref = pdxlopIdent->MakeDXLColRef();
	const ULONG col_id = dxl_colref->Id();

	// get the test expression
	GPOS_ASSERT(IsA(psublink->testexpr, OpExpr));
	OpExpr *popexpr = (OpExpr*) psublink->testexpr;

	IMDId *pmdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(popexpr->opno);

	// get operator name
	const CWStringConst *str = GetDXLArrayCmpType(pmdid);

	// translate left hand side of the expression
	GPOS_ASSERT(NULL != popexpr->args);
	Expr* pexprLHS = (Expr*) gpdb::PvListNth(popexpr->args, 0);

	CDXLNode *pdxlnOuter = PdxlnScOpFromExpr(pexprLHS, var_col_id_mapping);

	CDXLNode *dxlnode = NULL;
	CDXLScalar *pdxlopSubquery = NULL;

	GPOS_ASSERT(ALL_SUBLINK == psublink->subLinkType || ANY_SUBLINK == psublink->subLinkType);
	if (ALL_SUBLINK == psublink->subLinkType)
	{
		pdxlopSubquery = GPOS_NEW(m_memory_pool) CDXLScalarSubqueryAll
								(
								m_memory_pool,
								pmdid,
								GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, str),
								col_id
								);

	}
	else
	{
		pdxlopSubquery = GPOS_NEW(m_memory_pool) CDXLScalarSubqueryAny
								(
								m_memory_pool,
								pmdid,
								GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, str),
								col_id
								);
	}

	dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopSubquery);

	dxlnode->AddChild(pdxlnOuter);
	dxlnode->AddChild(pdxlnInner);

#ifdef GPOS_DEBUG
	dxlnode->GetOperator()->AssertValid(dxlnode, false /* fValidateChildren */);
#endif

	return dxlnode;
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
	const CMappingVarColId *var_col_id_mapping
	)
{
	CMappingVarColId *pmapvarcolidCopy = var_col_id_mapping->CopyMapColId(m_memory_pool);

	Query *pquerySublink = (Query *) psublink->subselect;
	CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
	ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_memory_pool,
							m_pmda,
							m_pidgtorCol,
							m_pidgtorCTE,
							pmapvarcolidCopy,
							pquerySublink,
							m_ulQueryLevel + 1,
							m_phmulCTEEntries
							);
	CDXLNode *pdxlnSubQuery = ptrquerytodxl->PdxlnFromQueryInternal();

	DXLNodeArray *query_output_dxlnode_array = ptrquerytodxl->PdrgpdxlnQueryOutput();

	GPOS_ASSERT(1 == query_output_dxlnode_array->Size());

	DXLNodeArray *cte_dxlnode_array = ptrquerytodxl->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_pdrgpdxlnCTE, cte_dxlnode_array);
	m_fHasDistributedTables = m_fHasDistributedTables || ptrquerytodxl->FHasDistributedTables();

	// get dxl scalar identifier
	CDXLNode *dxl_sc_ident = (*query_output_dxlnode_array)[0];
	GPOS_ASSERT(NULL != dxl_sc_ident);

	CDXLScalarIdent *pdxlopIdent = CDXLScalarIdent::Cast(dxl_sc_ident->GetOperator());

	// get the dxl column reference
	const CDXLColRef *dxl_colref = pdxlopIdent->MakeDXLColRef();
	const ULONG col_id = dxl_colref->Id();

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSubquery(m_memory_pool, col_id));

	dxlnode->AddChild(pdxlnSubQuery);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, ArrayExpr));

	const ArrayExpr *parrayexpr = (ArrayExpr *) pexpr;

	CDXLScalarArray *pdxlop =
			GPOS_NEW(m_memory_pool) CDXLScalarArray
						(
						m_memory_pool,
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayexpr->element_typeid),
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayexpr->array_typeid),
						parrayexpr->multidims
						);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	TranslateScalarChildren(dxlnode, parrayexpr->elements, var_col_id_mapping);

	return dxlnode;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(pexpr, ArrayRef));

	const ArrayRef *parrayref = (ArrayRef *) pexpr;
	Oid restype;

	INT type_modifier = parrayref->reftypmod;
	/* slice and/or store operations yield the array type */
	if (parrayref->reflowerindexpr || parrayref->refassgnexpr)
		restype = parrayref->refarraytype;
	else
		restype = parrayref->refelemtype;

	CDXLScalarArrayRef *pdxlop =
			GPOS_NEW(m_memory_pool) CDXLScalarArrayRef
						(
						m_memory_pool,
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayref->refelemtype),
						type_modifier,
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayref->refarraytype),
						GPOS_NEW(m_memory_pool) CMDIdGPDB(restype)
						);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlop);

	// add children
	AddArrayIndexList(dxlnode, parrayref->reflowerindexpr, CDXLScalarArrayRefIndexList::EilbLower, var_col_id_mapping);
	AddArrayIndexList(dxlnode, parrayref->refupperindexpr, CDXLScalarArrayRefIndexList::EilbUpper, var_col_id_mapping);

	dxlnode->AddChild(PdxlnScOpFromExpr(parrayref->refexpr, var_col_id_mapping));

	if (NULL != parrayref->refassgnexpr)
	{
		dxlnode->AddChild(PdxlnScOpFromExpr(parrayref->refassgnexpr, var_col_id_mapping));
	}

	return dxlnode;
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
	CDXLNode *dxlnode,
	List *plist,
	CDXLScalarArrayRefIndexList::EIndexListBound eilb,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(EdxlopScalarArrayRef == dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(CDXLScalarArrayRefIndexList::EilbSentinel > eilb);

	CDXLNode *pdxlnIndexList =
			GPOS_NEW(m_memory_pool) CDXLNode
					(
					m_memory_pool,
					GPOS_NEW(m_memory_pool) CDXLScalarArrayRefIndexList(m_memory_pool, eilb)
					);

	TranslateScalarChildren(pdxlnIndexList, plist, var_col_id_mapping);
	dxlnode->AddChild(pdxlnIndexList);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDXLArrayCmpType
//
//	@doc:
//		Get the operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CTranslatorScalarToDXL::GetDXLArrayCmpType
	(
	IMDId *pmdid
	)
	const
{
	// get operator name
	const IMDScalarOp *md_scalar_op = m_pmda->Pmdscop(pmdid);

	const CWStringConst *str = md_scalar_op->Mdname().GetMDName();

	return str;
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
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(NULL != psublink);
	CMappingVarColId *pmapvarcolidCopy = var_col_id_mapping->CopyMapColId(m_memory_pool);

	CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
	ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_memory_pool,
							m_pmda,
							m_pidgtorCol,
							m_pidgtorCTE,
							pmapvarcolidCopy,
							(Query *) psublink->subselect,
							m_ulQueryLevel + 1,
							m_phmulCTEEntries
							);
	CDXLNode *root_dxl_node = ptrquerytodxl->PdxlnFromQueryInternal();
	
	DXLNodeArray *cte_dxlnode_array = ptrquerytodxl->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_pdrgpdxlnCTE, cte_dxlnode_array);
	m_fHasDistributedTables = m_fHasDistributedTables || ptrquerytodxl->FHasDistributedTables();
	
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSubqueryExists(m_memory_pool));
	dxlnode->AddChild(root_dxl_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Pdatum
//
//	@doc:
//		Create CDXLDatum from GPDB datum
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::GetDatumVal
	(
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	INT type_modifier,
	BOOL is_null,
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
		if (pmdtype->GetDatumType() == elem.type_info)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		// generate a datum of generic type
		return PdxldatumGeneric(memory_pool, pmdtype, type_modifier, is_null, ulLen, datum);
	}
	else
	{
		return (*pf)(memory_pool, pmdtype, is_null, ulLen, datum);
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	INT type_modifier,
	BOOL is_null,
	ULONG ulLen,
	Datum datum
	)
{
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(*pmdidMDC);

	BOOL fConstByVal = pmdtype->IsPassedByValue();
	BYTE *pba = GetByteArray(memory_pool, pmdtype, is_null, ulLen, datum);
	ULONG length = 0;
	if (!is_null)
	{
		length = (ULONG) gpdb::SDatumSize(datum, pmdtype->IsPassedByValue(), ulLen);
	}

	CDouble dValue(0);
	if (CMDTypeGenericGPDB::HasByte2DoubleMapping(pmdid))
	{
		dValue = DValue(pmdid, is_null, pba, datum);
	}

	LINT lValue = 0;
	if (CMDTypeGenericGPDB::HasByte2IntMapping(pmdid))
	{
		lValue = Value(pmdid, is_null, pba, length);
	}

	return CMDTypeGenericGPDB::CreateDXLDatumVal(memory_pool, pmdid, type_modifier, fConstByVal, is_null, pba, length, lValue, dValue);
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->IsPassedByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(memory_pool) CDXLDatumBool(memory_pool, pmdid, is_null, gpdb::FBoolFromDatum(datum));
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->IsPassedByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(memory_pool) CDXLDatumOid(memory_pool, pmdid, is_null, gpdb::OidFromDatum(datum));
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->IsPassedByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(memory_pool) CDXLDatumInt2(memory_pool, pmdid, is_null, gpdb::SInt16FromDatum(datum));
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->IsPassedByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(memory_pool) CDXLDatumInt4(memory_pool, pmdid, is_null, gpdb::IInt32FromDatum(datum));
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	ULONG , //ulLen,
	Datum datum
	)
{
	GPOS_ASSERT(pmdtype->IsPassedByValue());
	CMDIdGPDB *pmdidMDC = CMDIdGPDB::CastMdid(pmdtype->MDId());
	CMDIdGPDB *pmdid = GPOS_NEW(memory_pool) CMDIdGPDB(*pmdidMDC);

	return GPOS_NEW(memory_pool) CDXLDatumInt8(memory_pool, pmdid, is_null, gpdb::LlInt64FromDatum(datum));
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
	BOOL is_null,
	BYTE *pba,
	Datum datum
	)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::HasByte2DoubleMapping(pmdid));

	double d = 0;

	if (is_null)
	{
		return CDouble(d);
	}

	if (pmdid->Equals(&CMDIdGPDB::m_mdid_numeric))
	{
		Numeric num = (Numeric) (pba);

		// NOTE: we assume that numeric_is_nan() cannot throw an error!
		if (numeric_is_nan(num))
		{
			// in GPDB NaN is considered the largest numeric number.
			return CDouble(GPOS_FP_ABS_MAX);
		}

		d = gpdb::DNumericToDoubleNoOverflow(num);
	}
	else if (pmdid->Equals(&CMDIdGPDB::m_mdid_float4))
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
	else if (pmdid->Equals(&CMDIdGPDB::m_mdid_float8))
	{
		d = gpdb::DFloat8FromDatum(datum);

		if (isnan(d))
		{
			d = GPOS_FP_ABS_MAX;
		}
	}
	else if (CMDTypeGenericGPDB::IsTimeRelatedType(pmdid))
	{
		d = gpdb::DConvertTimeValueToScalar(datum, CMDIdGPDB::CastMdid(pmdid)->OidObjectId());
	}
	else if (CMDTypeGenericGPDB::IsNetworkRelatedType(pmdid))
	{
		d = gpdb::DConvertNetworkToScalar(datum, CMDIdGPDB::CastMdid(pmdid)->OidObjectId());
	}

	return CDouble(d);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetByteArray
//
//	@doc:
//		Extract the byte array value of the datum. The result is NULL if datum is NULL
//---------------------------------------------------------------------------
BYTE *
CTranslatorScalarToDXL::GetByteArray
	(
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	ULONG ulLen,
	Datum datum
	)
{
	ULONG length = 0;
	BYTE *pba = NULL;

	if (is_null)
	{
		return pba;
	}

	length = (ULONG) gpdb::SDatumSize(datum, pmdtype->IsPassedByValue(), ulLen);
	GPOS_ASSERT(length > 0);

	pba = GPOS_NEW_ARRAY(memory_pool, BYTE, length);

	if (pmdtype->IsPassedByValue())
	{
		GPOS_ASSERT(length <= ULONG(sizeof(Datum)));
		clib::Memcpy(pba, &datum, length);
	}
	else
	{
		clib::Memcpy(pba, gpdb::PvPointerFromDatum(datum), length);
	}

	return pba;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::Value
//
//	@doc:
//		Extract the long int value of a datum
//---------------------------------------------------------------------------
LINT
CTranslatorScalarToDXL::Value
	(
	IMDId *pmdid,
	BOOL is_null,
	BYTE *pba,
	ULONG length
	)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::HasByte2IntMapping(pmdid));

	LINT lValue = 0;
	if (is_null)
	{
		return lValue;
	}

	if (pmdid->Equals(&CMDIdGPDB::m_mdid_cash))
	{
		// cash is a pass-by-ref type
		Datum datumConstVal = (Datum) 0;
		clib::Memcpy(&datumConstVal, pba, length);
		// Date is internally represented as an int32
		lValue = (LINT) (gpdb::IInt32FromDatum(datumConstVal));

	}
	else
	{
		// use hash value
		ULONG ulHash = 0;
		if (is_null)
		{
			ulHash = gpos::HashValue<ULONG>(&ulHash);
		}
		else
		{
			ulHash = gpos::HashValue<BYTE>(pba);
			for (ULONG ul = 1; ul < length; ul++)
			{
				ulHash = gpos::CombineHashes(ulHash, gpos::HashValue<BYTE>(&pba[ul]));
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
	IMemoryPool *memory_pool,
	const IMDType *pmdtype,
	BOOL is_null,
	Datum datum
	)
{
	ULONG length = pmdtype->Length();
	if (!pmdtype->IsPassedByValue() && !is_null)
	{
		INT iLen = dynamic_cast<const CMDTypeGenericGPDB *>(pmdtype)->GetGPDBLength();
		length = (ULONG) gpdb::SDatumSize(datum, pmdtype->IsPassedByValue(), iLen);
	}
	GPOS_ASSERT(is_null || length > 0);

	CDXLDatum *datum_dxl = CTranslatorScalarToDXL::GetDatumVal(memory_pool, pmdtype, gpmd::default_type_modifier, is_null, length, datum);
	IDatum *pdatum = pmdtype->GetDatumForDXLDatum(memory_pool, datum_dxl);
	datum_dxl->Release();
	return pdatum;
}

// EOF
