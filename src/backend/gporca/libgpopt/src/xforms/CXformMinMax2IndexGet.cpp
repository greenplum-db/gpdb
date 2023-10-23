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
#include "gpopt/operators/CScalarProjectList.h"
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
				  GPOS_NEW(mp) CScalarProjectList(mp),	// scalar project list
				  GPOS_NEW(mp) CExpression(
					  mp,
					  GPOS_NEW(mp)
						  CScalarProjectElement(mp),  // scalar project element
					  GPOS_NEW(mp)
						  CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))))))
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

	// extract components
	CExpression *pexprRel = (*pexpr)[0];
	CExpression *pexprScalarPrjList = (*pexpr)[1];

	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprRel->Pop());
	// get the indices count of this relation
	const ULONG ulIndices = popGet->Ptabdesc()->IndexCount();

	// Ignore xform if relation doesn't have any indices
	if (0 == ulIndices)
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

	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(popGet->Ptabdesc()->MDId());

	const CColRef *agg_colref = nullptr;

	// Check if agg function is min/max and aggregate is on a column
	if (!IsMinMaxAggOnColumn(agg_func_type, pexprAggFunc, &agg_colref))
	{
		return;
	}

	IMdIdArray *btree_indices =
		GetApplicableIndices(mp, agg_colref, popGet->PdrgpcrOutput(),
							 md_accessor, pmdrel, ulIndices);

	// Check if relation has btree indices with leading index key as agg column
	if (btree_indices->Size() == 0)
	{
		btree_indices->Release();
		return;
	}

	// Generate index column not null condition.
	CExpression *notNullExpr =
		CUtils::PexprIsNotNull(mp, CUtils::PexprScalarIdent(mp, agg_colref));

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	notNullExpr->AddRef();
	pdrgpexpr->Append(notNullExpr);

	popGet->AddRef();
	CExpression *pexprGetNotNull =
		GPOS_NEW(mp) CExpression(mp, popGet, notNullExpr);

	CColRefSet *pcrsScalarExpr = GPOS_NEW(mp) CColRefSet(mp);
	pcrsScalarExpr->Include(agg_colref);
	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(pexpr->Pop());

	for (ULONG ul = 0; ul < btree_indices->Size(); ul++)
	{
		IMDId *pmdidIndex = (*btree_indices)[ul];
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);

		// Check if index is applicable and get scan direction
		EIndexScanDirection scan_direction =
			GetScanDirection(pmdindex, popScAggFunc, agg_func_type);

		// build IndexGet expression
		CExpression *pexprIndexGet = CXformUtils::PexprBuildBtreeIndexPlan(
			mp, md_accessor, pexprGetNotNull, popAgg->UlOpId(), pdrgpexpr,
			pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel,
			scan_direction, false);

		if (pexprIndexGet != nullptr)
		{
			COrderSpec *pos = nullptr;
			// Compute and update the required OrderSpec for first index key
			CXformUtils::ComputeOrderSpecForIndexKey(mp, &pos, pmdindex,
													 scan_direction, agg_colref,
													 0 /*key position*/);

			// build Limit expression
			CExpression *pexprLimit = CUtils::BuildLimitExprWithOrderSpec(
				mp, pexprIndexGet, pos, 0 /*limitOffset*/, 1 /*limitCount*/);

			popAgg->AddRef();
			pexprScalarPrjList->AddRef();

			// build Aggregate expression
			CExpression *finalpexpr = GPOS_NEW(mp)
				CExpression(mp, popAgg, pexprLimit, pexprScalarPrjList);

			pxfres->Add(finalpexpr);
		}
	}

	btree_indices->Release();
	pcrsScalarExpr->Release();
	pdrgpexpr->Release();
	pexprGetNotNull->Release();
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
CXformMinMax2IndexGet::GetScanDirection(const IMDIndex *pmdindex,
										CScalarAggFunc *popScAggFunc,
										const IMDType *agg_col_type)
{
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

//---------------------------------------------------------------------------
//	@function:
//		CXformMinMax2IndexGet::IsMinMaxAggOnColumn
//
//	@doc:
//		This function checks if the aggregate is min or max and is
//		performed on a column to determine if this transform
//		applies for a given pattern.
//---------------------------------------------------------------------------
BOOL
CXformMinMax2IndexGet::IsMinMaxAggOnColumn(const IMDType *agg_func_type,
										   CExpression *pexprAggFunc,
										   const CColRef **agg_colref)
{
	CScalarAggFunc *popScAggFunc =
		CScalarAggFunc::PopConvert(pexprAggFunc->Pop());

	// Check if aggregate function is either min() or max()
	if (!popScAggFunc->IsMinMax(agg_func_type))
	{
		return false;
	}

	// Extracts aggregate column from below part in the expression tree
	//   +--CScalarAggFunc
	//     |--CScalarValuesList
	//      | +--CScalarIdent "a" (0)
	*agg_colref = CCastUtils::PcrExtractFromScIdOrCastScId(
		(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexArgs])[0]);

	// Check if min/max aggregation performed on a column or cast
	// of column. This optimization isn't necessary for min/max on constants.
	if (nullptr == *agg_colref)
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformMinMax2IndexGet::GetApplicableIndices
//
//	@doc:
//		This function checks if the relation has any btree indices that have
//		leading index key as the aggregate column to determine if this transform
//		applies for a given pattern.
//---------------------------------------------------------------------------
IMdIdArray *
CXformMinMax2IndexGet::GetApplicableIndices(
	CMemoryPool *mp, const CColRef *agg_colref, CColRefArray *output_col_array,
	CMDAccessor *md_accessor, const IMDRelation *pmdrel, ULONG ulIndices)
{
	CColRefArray *pdrgpcrIndexColumns = nullptr;
	IMdIdArray *btree_indices = GPOS_NEW(mp) IMdIdArray(mp);

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);
		// get columns in the index
		pdrgpcrIndexColumns = CXformUtils::PdrgpcrIndexKeys(
			mp, output_col_array, pmdindex, pmdrel);

		// Ordered IndexScan is only applicable if index type is Btree and
		// if aggregate function's column matches with first index key
		if (pmdindex->IndexType() == IMDIndex::EmdindBtree &&
			CColRef::Equals(agg_colref, (*pdrgpcrIndexColumns)[0]))
		{
			pmdidIndex->AddRef();
			btree_indices->Append(pmdidIndex);
		}
		pdrgpcrIndexColumns->Release();
	}

	return btree_indices;
}

// EOF
