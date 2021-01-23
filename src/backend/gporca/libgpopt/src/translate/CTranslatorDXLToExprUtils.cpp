//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CTranslatorDXLToExprUtils.cpp
//
//	@doc:
//		Implementation of the helper methods used during DXL to Expr translation
//
//---------------------------------------------------------------------------

#include "gpopt/translate/CTranslatorDXLToExprUtils.h"

#include "gpopt/mdcache/CMDAccessorUtils.h"
#include "naucrates/base/IDatumInt4.h"
#include "naucrates/base/IDatumInt8.h"
#include "naucrates/dxl/operators/CDXLScalarFuncExpr.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"
#include "naucrates/md/IMDCast.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/md/IMDTypeInt8.h"

using namespace gpos;
using namespace gpmd;
using namespace gpdxl;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::PopConst
//
//	@doc:
// 		Construct const operator from a DXL const value operator
//
//---------------------------------------------------------------------------
CScalarConst *
CTranslatorDXLToExprUtils::PopConst(CMemoryPool *mp, CMDAccessor *md_accessor,
									const CDXLScalarConstValue *dxl_op)
{
	IDatum *datum = CTranslatorDXLToExprUtils::GetDatum(md_accessor, dxl_op);
	return GPOS_NEW(mp) CScalarConst(mp, datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::GetDatum
//
//	@doc:
// 		Construct a datum from a DXL const value operator
//
//---------------------------------------------------------------------------
IDatum *
CTranslatorDXLToExprUtils::GetDatum(CMDAccessor *md_accessor,
									const CDXLScalarConstValue *dxl_op)
{
	IMDId *mdid = dxl_op->GetDatumVal()->MDId();
	IDatum *datum =
		md_accessor->RetrieveType(mdid)->GetDatumForDXLConstVal(dxl_op);

	return datum;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::Pdrgpdatum
//
//	@doc:
// 		Construct a datum array from a DXL datum array
//
//---------------------------------------------------------------------------
IDatumArray *
CTranslatorDXLToExprUtils::Pdrgpdatum(CMemoryPool *mp, CMDAccessor *md_accessor,
									  const CDXLDatumArray *pdrgpdxldatum)
{
	GPOS_ASSERT(NULL != pdrgpdxldatum);

	IDatumArray *pdrgdatum = GPOS_NEW(mp) IDatumArray(mp);
	const ULONG length = pdrgpdxldatum->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLDatum *dxl_datum = (*pdrgpdxldatum)[ul];
		IMDId *mdid = dxl_datum->MDId();
		IDatum *datum =
			md_accessor->RetrieveType(mdid)->GetDatumForDXLDatum(mp, dxl_datum);
		pdrgdatum->Append(datum);
	}

	return pdrgdatum;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::PexprConstInt8
//
//	@doc:
// 		Construct an expression representing the given value in INT8 format
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExprUtils::PexprConstInt8(CMemoryPool *mp,
										  CMDAccessor *md_accessor,
										  CSystemId sysid, LINT val)
{
	IDatumInt8 *datum =
		md_accessor->PtMDType<IMDTypeInt8>(sysid)->CreateInt8Datum(
			mp, val, false /* is_null */);
	CExpression *pexprConst =
		GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarConst(mp, datum));

	return pexprConst;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::AddKeySets
//
//	@doc:
// 		Add key sets info from the MD relation to the table descriptor
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToExprUtils::AddKeySets(CMemoryPool *mp,
									  CTableDescriptor *ptabdesc,
									  const IMDRelation *pmdrel,
									  UlongToUlongMap *phmululColMapping)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pmdrel);

	const ULONG ulKeySets = pmdrel->KeySetCount();
	for (ULONG ul = 0; ul < ulKeySets; ul++)
	{
		CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, ptabdesc->ColumnCount());
		const ULongPtrArray *pdrgpulKeys = pmdrel->KeySetAt(ul);
		const ULONG ulKeys = pdrgpulKeys->Size();

		for (ULONG ulKey = 0; ulKey < ulKeys; ulKey++)
		{
			// populate current keyset
			ULONG ulOriginalKey = *((*pdrgpulKeys)[ulKey]);
			ULONG *pulRemappedKey = phmululColMapping->Find(&ulOriginalKey);
			GPOS_ASSERT(NULL != pulRemappedKey);

			pbs->ExchangeSet(*pulRemappedKey);
		}

		if (!ptabdesc->FAddKeySet(pbs))
		{
			pbs->Release();
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::FScalarBool
//
//	@doc:
//		Check if a dxl node is a scalar bool
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToExprUtils::FScalarBool(const CDXLNode *dxlnode,
									   EdxlBoolExprType edxlboolexprtype)
{
	GPOS_ASSERT(NULL != dxlnode);

	CDXLOperator *dxl_op = dxlnode->GetOperator();
	if (EdxlopScalarBoolExpr == dxl_op->GetDXLOperator())
	{
		EdxlBoolExprType edxlboolexprtypeNode =
			CDXLScalarBoolExpr::Cast(dxl_op)->GetDxlBoolTypeStr();
		return edxlboolexprtype == edxlboolexprtypeNode;
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::EBoolOperator
//
//	@doc:
// 		returns the equivalent bool expression type in the optimizer for
// 		a given DXL bool expression type

//---------------------------------------------------------------------------
CScalarBoolOp::EBoolOperator
CTranslatorDXLToExprUtils::EBoolOperator(EdxlBoolExprType edxlbooltype)
{
	CScalarBoolOp::EBoolOperator eboolop = CScalarBoolOp::EboolopSentinel;

	switch (edxlbooltype)
	{
		case Edxlnot:
			eboolop = CScalarBoolOp::EboolopNot;
			break;
		case Edxland:
			eboolop = CScalarBoolOp::EboolopAnd;
			break;
		case Edxlor:
			eboolop = CScalarBoolOp::EboolopOr;
			break;
		default:
			GPOS_ASSERT(!"Unrecognized boolean expression type");
	}

	GPOS_ASSERT(CScalarBoolOp::EboolopSentinel > eboolop);

	return eboolop;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::Pdrgpcr
//
//	@doc:
// 		Construct a dynamic array of column references corresponding to the
//		given col ids
//
//---------------------------------------------------------------------------
CColRefArray *
CTranslatorDXLToExprUtils::Pdrgpcr(CMemoryPool *mp,
								   UlongToColRefMap *colref_mapping,
								   const ULongPtrArray *colids)
{
	GPOS_ASSERT(NULL != colids);

	CColRefArray *colref_array = GPOS_NEW(mp) CColRefArray(mp);

	for (ULONG ul = 0; ul < colids->Size(); ul++)
	{
		ULONG *pulColId = (*colids)[ul];
		CColRef *colref = colref_mapping->Find(pulColId);
		colref->MarkAsUsed();
		GPOS_ASSERT(NULL != colref);

		colref_array->Append(const_cast<CColRef *>(colref));
	}

	return colref_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprUtils::FCastFunc
//
//	@doc:
//		Is the given expression is a scalar function that is used to cast
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToExprUtils::FCastFunc(CMDAccessor *md_accessor,
									 const CDXLNode *dxlnode, IMDId *pmdidInput)
{
	GPOS_ASSERT(NULL != dxlnode);

	if (1 != dxlnode->Arity())
	{
		return false;
	}

	if (NULL == pmdidInput)
	{
		return false;
	}

	if (EdxlopScalarFuncExpr != dxlnode->GetOperator()->GetDXLOperator())
	{
		return false;
	}

	CDXLScalarFuncExpr *pdxlopScFunc =
		CDXLScalarFuncExpr::Cast(dxlnode->GetOperator());

	IMDId *mdid_dest = pdxlopScFunc->ReturnTypeMdId();

	if (!CMDAccessorUtils::FCastExists(md_accessor, pmdidInput, mdid_dest))
	{
		return false;
	}

	const IMDCast *pmdcast = md_accessor->Pmdcast(pmdidInput, mdid_dest);

	return (pmdcast->GetCastFuncMdId()->Equals(pdxlopScFunc->FuncMdId()));
}


// EOF
