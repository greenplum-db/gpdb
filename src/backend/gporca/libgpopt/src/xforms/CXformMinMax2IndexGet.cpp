//-------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformMinMax2IndexGet.cpp
//
//	@doc:
//		Transform aggregates min, max to queries with IndexScan with
//		Limit
//-------------------------------------------------------------------

#include "gpopt/xforms/CXformMinMax2IndexGet.h"

#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//-------------------------------------------------------------------
//	@function:
//		CXformMinMax2IndexGet::CXformMinMax2IndexGet
//
//	@doc:
//		Ctor
//
//-------------------------------------------------------------------
CXformMinMax2IndexGet::CXformMinMax2IndexGet(CMemoryPool *mp)
	:  // pattern
	  CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalGbAgg(mp),
			  GPOS_NEW(mp) CExpression(
				  mp,
				  GPOS_NEW(mp) CLogicalGet(mp)),  // relational child
			  GPOS_NEW(mp) CExpression(
				  mp,
				  GPOS_NEW(mp) CPatternTree(mp))))	// scalar project list
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformMinMax2IndexGet::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//		GbAgg must be global and have empty grouping columns
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformMinMax2IndexGet::Exfp(CExpressionHandle &exprhdl) const
{
	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(exprhdl.Pop());
	if (!popAgg->FGlobal() || 0 < popAgg->Pdrgpcr()->Size())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformMinMax2IndexGet::Transform
//
//	@doc:
//		Actual transformation
//
//		Input Query:  select max(b) from foo;
//		Output Query: select max(b) from (select * from foo where b is
//										   not null order by b limit 1);
//---------------------------------------------------------------------------
void
CXformMinMax2IndexGet::Transform(CXformContext *pxfctxt, CXformResult *pxfres,
								 CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();

	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(pexpr->Pop());

	// extract components
	CExpression *pexprRel = (*pexpr)[0];
	CExpression *pexprScalarPrjList = (*pexpr)[1];

	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprRel->Pop());
	// get the indices count of this relation
	const ULONG ulIndices = popGet->Ptabdesc()->IndexCount();

	// Ignore xform if relation doesn't have any b-tree indices
	if (0 == ulIndices)
	{
		return;
	}

	// Check if query has no aggregate function with empty group by,
	// or it has more than one aggregate function
	if (pexprScalarPrjList->Arity() != 1)
	{
		return;
	}

	CExpression *pexprPrjEl = (*pexprScalarPrjList)[0];
	CExpression *pexprAggFunc = (*pexprPrjEl)[0];
	CScalarAggFunc *popScAggFunc =
		CScalarAggFunc::PopConvert(pexprAggFunc->Pop());
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	IMDId *agg_func_mdid = CScalar::PopConvert(popScAggFunc)->MdidType();
	const IMDType *agg_func_type = md_accessor->RetrieveType(agg_func_mdid);

	// Check if aggregate function is either min() or max()
	if (!popScAggFunc->IsMinMax(agg_func_type))
	{
		return;
	}

	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(popGet->Ptabdesc()->MDId());

	const CColRef *agg_colref = CCastUtils::PcrExtractFromScIdOrCastScId(
		(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexArgs])[0]);

	// Check if min/max aggregation performed on a column or cast
	// of column. This optimization isn't necessary for min/max on constants.
	if (nullptr == agg_colref)
	{
		return;
	}

	// Generate index column not null condition.
	CExpression *notNullExpr =
		CUtils::PexprIsNotNull(mp, CUtils::PexprScalarIdent(mp, agg_colref));

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	notNullExpr->AddRef();
	pdrgpexpr->Append(notNullExpr);

	popGet->AddRef();
	CExpression *pexprUpdatedRltn =
		GPOS_NEW(mp) CExpression(mp, popGet, notNullExpr);

	CColRefSet *pcrsScalarExpr = GPOS_NEW(mp) CColRefSet(mp);
	pcrsScalarExpr->Include(agg_colref);
	CColRefArray *pdrgpcrIndexColumns = nullptr;

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);
		// get columns in the index
		pdrgpcrIndexColumns = CXformUtils::PdrgpcrIndexKeys(
			mp, popGet->PdrgpcrOutput(), pmdindex, pmdrel);
		// Check if index is applicable and get Scan direction
		EIndexScanDirection scan_direction =
			GetScanDirection(agg_colref, pdrgpcrIndexColumns, pmdindex,
							 popScAggFunc, agg_func_type);
		// Proceed if index is applicable
		if (scan_direction != EisdSentinel)
		{
			// build IndexGet expression
			CExpression *pexprIndexGet = CXformUtils::PexprLogicalIndexGet(
				mp, md_accessor, pexprUpdatedRltn, popAgg->UlOpId(), pdrgpexpr,
				pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel, false,
				scan_direction);

			if (pexprIndexGet != nullptr)
			{
				// build Limit expression
				CExpression *pexprLimit =
					CUtils::PexprLimit(mp, pexprIndexGet, 0, 1);
				CLogicalLimit *popLimit =
					CLogicalLimit::PopConvert(pexprLimit->Pop());
				// Compute the required OrderSpec for first index key
				CXformUtils::PosForIndexKey(pmdindex, scan_direction,
											agg_colref, popLimit->Pos(), 0);

				popAgg->AddRef();
				pexprScalarPrjList->AddRef();

				// build Aggregate expression
				CExpression *finalpexpr = GPOS_NEW(mp)
					CExpression(mp, popAgg, pexprLimit, pexprScalarPrjList);

				pxfres->Add(finalpexpr);
			}
		}
		pdrgpcrIndexColumns->Release();
	}
	pcrsScalarExpr->Release();
	pdrgpexpr->Release();
	pexprUpdatedRltn->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CXformMinMax2IndexGet::GetScanDirection
//
//	@doc:
//		Function to validate if index is applicable and determine Index Scan
//		direction, given index columns. This function checks if aggregate column
//		is prefix of the index columns.
//---------------------------------------------------------------------------
EIndexScanDirection
CXformMinMax2IndexGet::GetScanDirection(const CColRef *agg_col,
										CColRefArray *pdrgpcrIndexColumns,
										const IMDIndex *pmdindex,
										CScalarAggFunc *popScAggFunc,
										const IMDType *agg_col_type)
{
	// Ordered IndexScan is only applicable if index type is Btree and
	// if aggregate function's column matches with first index key
	if (pmdindex->IndexType() != IMDIndex::EmdindBtree ||
		!CColRef::Equals(agg_col, (*pdrgpcrIndexColumns)[0]))
	{
		return EisdSentinel;
	}

	// If Aggregate function is min()
	if (popScAggFunc->MDId()->Equals(
			agg_col_type->GetMdidForAggType(IMDType::EaggMin)))
	{
		// Find the minimum element by:
		// 1. Scanning Forward, if index is sorted in ascending order.
		// 2. Scanning Backward, if index is sorted in descending order.
		return pmdindex->KeySortDirectionAt(0) == SORT_DESC ? EBackwardScan
															: EForwardScan;
	}
	// If Aggregate function is max()
	else
	{
		// Find the maximum element by:
		// 1. Scanning Forward, if index is sorted in descending order.
		// 2. Scanning Backward, if index is sorted in ascending order.
		return pmdindex->KeySortDirectionAt(0) == SORT_DESC ? EForwardScan
															: EBackwardScan;
	}
}

// EOF
