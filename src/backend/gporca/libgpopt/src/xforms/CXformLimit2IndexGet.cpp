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

	CExpressionArray *pdrgpexpr =
		CPredicateUtils::PdrgpexprConjuncts(mp, boolConstExpr);
	GPOS_ASSERT(pdrgpexpr->Size() > 0);

	popGet->AddRef();
	CExpression *pexprUpdtdRltn =
		GPOS_NEW(mp) CExpression(mp, popGet, boolConstExpr);

	CColRefSet *pcrsScalarExpr = popLimit->PcrsLocalUsed();

	// get order by columns specified
	ULONG totalOrderByCols = popLimit->Pos()->UlSortColumns();
	CColRefArray *pOrderByCols = GPOS_NEW(mp) CColRefArray(mp);

	for (ULONG i = 0; i < totalOrderByCols; i++)
	{
		const CColRef *orderByCol = popLimit->Pos()->Pcr(i);
		pOrderByCols->Append(const_cast<CColRef *>(orderByCol));
	}

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
		if (FIndexApplicableForOrderBy(pOrderByCols, pdrgpcrIndexColumns))
		{
			// build IndexGet expression
			CExpression *pexprIndexGet = CXformUtils::PexprLogicalIndexGet(
				mp, md_accessor, pexprUpdtdRltn, popLimit->UlOpId(), pdrgpexpr,
				pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel, true);

			if (pexprIndexGet != nullptr)
			{
				// build Limit expression
				CExpression *pexprNewLimit =
					PexprLimit(mp, pexprIndexGet, pexprScalarOffset,
							   pexprScalarRows, popLimit->Pos(),
							   popLimit->FGlobal(),	 // fGlobal
							   popLimit->FHasCount(),
							   popLimit->IsTopLimitUnderDMLorCTAS());

				pxfres->Add(pexprNewLimit);
			}
		}
		pdrgpcrIndexColumns->Release();
	}

	pdrgpexpr->Release();
	pOrderByCols->Release();
	pexprUpdtdRltn->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGet::FIndexApplicableForOrderBy
//
//	@doc:
//		Function to validate if index is applicable, given order by and index
// 	    columns
//---------------------------------------------------------------------------
BOOL
CXformLimit2IndexGet::FIndexApplicableForOrderBy(
	CColRefArray *pOrderByCols, CColRefArray *pdrgpcrIndexColumns)
{
	if (pdrgpcrIndexColumns->Size() < pOrderByCols->Size())
	{
		return false;
	}
	BOOL indexApplicable = true;
	for (ULONG i = 0; i < pOrderByCols->Size(); i++)
	{
		if ((*pOrderByCols)[i] != (*pdrgpcrIndexColumns)[i])
		{
			indexApplicable = false;
			break;
		}
	}
	return indexApplicable;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGet::PexprLimit
//
//	@doc:
//		Generate a limit operator
//
//---------------------------------------------------------------------------
CExpression *
CXformLimit2IndexGet::PexprLimit(CMemoryPool *mp, CExpression *pexprRelational,
								 CExpression *pexprScalarStart,
								 CExpression *pexprScalarRows, COrderSpec *pos,
								 BOOL fGlobal, BOOL fHasCount,
								 BOOL fTopLimitUnderDML)
{
	pexprScalarStart->AddRef();
	pexprScalarRows->AddRef();
	pos->AddRef();

	CExpression *pexprLimit = GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp)
			CLogicalLimit(mp, pos, fGlobal, fHasCount, fTopLimitUnderDML),
		pexprRelational, pexprScalarStart, pexprScalarRows);

	return pexprLimit;
}

// EOF
