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

	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(pexpr->Pop());

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

	IMdIdArray *btree_indices = IsMinMaxAggOnColumn(
		mp, agg_func_type, pexprAggFunc, popGet->PdrgpcrOutput(), md_accessor,
		pmdrel, ulIndices);

	// Check if agg function is min/max and relation has btree indices with
	// leading index key as agg column
	if (btree_indices == nullptr)
	{
		return;
	}

	const CColRef *agg_colref = CCastUtils::PcrExtractFromScIdOrCastScId(
		(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexArgs])[0]);

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

	for (ULONG ul = 0; ul < btree_indices->Size(); ul++)
	{
		IMDId *pmdidIndex = (*btree_indices)[ul];
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);

		// Check if index is applicable and get Scan direction
		EIndexScanDirection scan_direction =
			GetScanDirection(pmdindex, popScAggFunc, agg_func_type);

		// build IndexGet expression
		CExpression *pexprIndexGet = CXformUtils::PexprLogicalIndexGet(
			mp, md_accessor, pexprGetNotNull, popAgg->UlOpId(), pdrgpexpr,
			pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel, false,
			scan_direction);

		if (pexprIndexGet != nullptr)
		{
			// Compute the required OrderSpec for first index key
			COrderSpec *pos = CXformUtils::ComputeOrderSpecForIndexKey(
				mp, pmdindex, scan_direction, agg_colref, 0);

			// build Limit expression
			CExpression *pexprLimit = CUtils::BuildLimitExprWithOrderSpec(
				mp, pexprIndexGet, pos, 0, 1);

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
//		This function performs below checks to determine if this transform
//		applies for a given pattern and if it does, returns applicable btree
//		indices array, if not returns nullptr
//		 1. Is the aggregate on min or max and is performed on a column.
//		 2. If the relation has any btree indices that have leading index key as
//		    the aggregate column
//---------------------------------------------------------------------------
IMdIdArray *
CXformMinMax2IndexGet::IsMinMaxAggOnColumn(
	CMemoryPool *mp, const IMDType *agg_func_type, CExpression *pexprAggFunc,
	CColRefArray *output_col_array, CMDAccessor *md_accessor,
	const IMDRelation *pmdrel, ULONG ulIndices)
{
	CScalarAggFunc *popScAggFunc =
		CScalarAggFunc::PopConvert(pexprAggFunc->Pop());

	// Check if aggregate function is either min() or max()
	if (!popScAggFunc->IsMinMax(agg_func_type))
	{
		return nullptr;
	}

	const CColRef *agg_colref = CCastUtils::PcrExtractFromScIdOrCastScId(
		(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexArgs])[0]);

	// Check if min/max aggregation performed on a column or cast
	// of column. This optimization isn't necessary for min/max on constants.
	if (nullptr == agg_colref)
	{
		return nullptr;
	}

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
