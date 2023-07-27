//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLimit2IndexGet.cpp
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalIndexGet if order by columns
//		match any of the index prefix
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformLimit2IndexGet.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/md/CMDIndexGPDB.h"
#include "naucrates/md/CMDRelationGPDB.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGet::CXformLimit2IndexGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformLimit2IndexGet::CXformLimit2IndexGet(CMemoryPool *mp)
	:  // pattern
	  CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalLimit(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CLogicalGet(mp)),  // relational child
			  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(
											   mp)),  // scalar child for offset
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp)
						  CPatternLeaf(mp))	 // scalar child for number of rows
			  ))
{
}


//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGet::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLimit2IndexGet::Exfp(CExpressionHandle &exprhdl) const
{
	if (exprhdl.DeriveHasSubquery(1))
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGet::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformLimit2IndexGet::Transform(CXformContext *pxfctxt, CXformResult *pxfres,
								CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();

	CLogicalLimit *popLimit = CLogicalLimit::PopConvert(pexpr->Pop());
	// extract components
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalarOffset = (*pexpr)[1];
	CExpression *pexprScalarRows = (*pexpr)[2];

	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprRelational->Pop());
	// get the indices count of this relation
	const ULONG ulIndices = popGet->Ptabdesc()->IndexCount();
	// Ignore xform if relation doesn't have any indices
	if (0 == ulIndices)
	{
		return;
	}

	CExpression *boolConstExpr = nullptr;
	boolConstExpr = CUtils::PexprScalarConstBool(mp, true);

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	boolConstExpr->AddRef();
	pdrgpexpr->Append(boolConstExpr);

	popGet->AddRef();
	CExpression *pexprUpdtdRltn =
		GPOS_NEW(mp) CExpression(mp, popGet, boolConstExpr);

	CColRefSet *pcrsScalarExpr = popLimit->PcrsLocalUsed();

	// find the indexes whose included columns meet the required columns
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(popGet->Ptabdesc()->MDId());
	CColRefArray *pdrgpcrIndexColumns = nullptr;

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);
		// get columns in the index
		pdrgpcrIndexColumns = CXformUtils::PdrgpcrIndexKeys(
			mp, popGet->PdrgpcrOutput(), pmdindex, pmdrel);
		COrderSpec *pos = popLimit->Pos();
		if (FIndexApplicableForOrderBy(mp, pos, pdrgpcrIndexColumns, pmdindex))
		{
			// get IndexScan direction
			EIndexScanDirection scandirection =
				FGetIndexScanDirection(pos, pmdindex);
			// build IndexGet expression
			CExpression *pexprIndexGet = CXformUtils::PexprLogicalIndexGet(
				mp, md_accessor, pexprUpdtdRltn, popLimit->UlOpId(), pdrgpexpr,
				pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel, true,
				scandirection);

			if (pexprIndexGet != nullptr)
			{
				pexprScalarOffset->AddRef();
				pexprScalarRows->AddRef();
				pos->AddRef();

				// build Limit expression
				CExpression *pexprLimit = GPOS_NEW(mp) CExpression(
					mp,
					GPOS_NEW(mp)
						CLogicalLimit(mp, pos, popLimit->FGlobal(),	 // fGlobal
									  popLimit->FHasCount(),
									  popLimit->IsTopLimitUnderDMLorCTAS()),
					pexprIndexGet, pexprScalarOffset, pexprScalarRows);

				pxfres->Add(pexprLimit);
			}
		}
		pdrgpcrIndexColumns->Release();
	}

	pdrgpexpr->Release();
	pexprUpdtdRltn->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGet::FIndexApplicableForOrderBy
//
//	@doc:
//		Function to validate if index is applicable, given OrderSpec and index
//		columns. This function checks if ORDER BY columns are prefix of
//		the index columns.
//---------------------------------------------------------------------------
BOOL
CXformLimit2IndexGet::FIndexApplicableForOrderBy(
	CMemoryPool *mp, COrderSpec *pos, CColRefArray *pdrgpcrIndexColumns,
	const IMDIndex *pmdindex)
{
	// Ordered IndexScan is only applicable for BTree index
	if (pmdindex->IndexType() != IMDIndex::EmdindBtree)
	{
		return false;
	}
	// get order by columns size
	ULONG totalOrderByCols = pos->UlSortColumns();
	if (pdrgpcrIndexColumns->Size() < totalOrderByCols || totalOrderByCols == 0)
	{
		return false;
	}
	BOOL indexApplicable = true;
	CBitVector *req_sort_order = GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);
	CBitVector *derived_sort_order =
		GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);
	CBitVector *req_nulls_order = GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);
	CBitVector *derived_nulls_order =
		GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);


	for (ULONG i = 0; i < totalOrderByCols; i++)
	{
		// Index is not applicable if either
		// 1. Order By Column do not match with index key OR
		// 2. NULLs are not Last in the specified Order by Clause.
		const CColRef *colref = pos->Pcr(i);
		if (!CColRef::Equals(colref, (*pdrgpcrIndexColumns)[i]))
		{
			indexApplicable = false;
			break;
		}
		// ASC - 0 DESC - 1
		// NULLS LAST - 0 NULLS FIRST - 1
		//IMDId *less_than_mdid = colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
		IMDId *greater_than_mdid =
			colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptG);
		if (greater_than_mdid->Equals(pos->GetMdIdSortOp(i)))
		{
			req_sort_order->ExchangeSet(i);
		}
		if (pmdindex->KeySortOrderAt(i) == 1)
		{
			derived_sort_order->ExchangeSet(i);
		}
		if (pos->Ent(i) == COrderSpec::EntFirst)
		{
			req_nulls_order->ExchangeSet(i);
		}
		if (pmdindex->KeyNullOrderAt(i) == 1)
		{
			derived_nulls_order->ExchangeSet(i);
		}

		if (!(req_sort_order->Equals(derived_sort_order) &&
			  req_nulls_order->Equals(derived_nulls_order)) &&
			!(FAreIndicesCommutative(req_sort_order, derived_sort_order, i) &&
			  FAreIndicesCommutative(req_nulls_order, derived_nulls_order, i)))
		{
			indexApplicable = false;
			break;
		}
	}

	GPOS_DELETE(req_sort_order);
	GPOS_DELETE(derived_sort_order);
	GPOS_DELETE(req_nulls_order);
	GPOS_DELETE(derived_nulls_order);

	return indexApplicable;
}


EIndexScanDirection
CXformLimit2IndexGet::FGetIndexScanDirection(COrderSpec *pos,
											 const IMDIndex *pmdindex)
{
	const CColRef *colref = pos->Pcr(0);
	IMDId *greater_than_mdid =
		colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptG);
	IMDId *pos_mdid = pos->GetMdIdSortOp(0);

	return (pos_mdid->Equals(greater_than_mdid) &&
			pmdindex->KeySortOrderAt(0) == 1) ||
				   (!pos_mdid->Equals(greater_than_mdid) &&
					pmdindex->KeySortOrderAt(0) == 0)
			   ? EisdForward
			   : EisdBackward;
}
//
BOOL
CXformLimit2IndexGet::FAreIndicesCommutative(CBitVector *index1,
											 CBitVector *index2, ULONG size)
{
	for (ULONG i = 0; i <= size; i++)
	{
		if (index1->Get(i) == index2->Get(i))
		{
			return false;
		}
	}
	return true;
}
// EOF
