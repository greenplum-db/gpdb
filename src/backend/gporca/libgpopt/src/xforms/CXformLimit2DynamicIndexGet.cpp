//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLimit2DynamicIndexGet.cpp
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalDynamicIndexGet if order by
//		columns match any of the index that has partition columns as its prefix
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformLimit2DynamicIndexGet.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGet.h"
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
//		CXformLimit2DynamicIndexGet::CXformLimit2DynamicIndexGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformLimit2DynamicIndexGet::CXformLimit2DynamicIndexGet(CMemoryPool *mp)
	:  // pattern
	  CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalLimit(mp),
			  GPOS_NEW(mp) CExpression(
				  mp,
				  GPOS_NEW(mp) CLogicalDynamicGet(mp)),  // relational child
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
//		CXformLimit2DynamicIndexGet::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLimit2DynamicIndexGet::Exfp(CExpressionHandle &exprhdl) const
{
	if (exprhdl.DeriveHasSubquery(1))
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2DynamicIndexGet::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformLimit2DynamicIndexGet::Transform(CXformContext *pxfctxt, CXformResult *pxfres,
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

	CLogicalDynamicGet *popDynGet = CLogicalDynamicGet::PopConvert(pexprRelational->Pop());
	// get the indices count of this relation
	const ULONG ulIndices = popDynGet->Ptabdesc()->IndexCount();
	// Ignore xform if relation doesn't have any indices
	if (0 == ulIndices)
	{
		return;
	}

	COrderSpec *pos = popLimit->Pos();
	// Ignore xform if query only specifies limit but no order expressions
	if (0 == pos->UlSortColumns())
	{
		return;
	}

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(popDynGet->Ptabdesc()->MDId());

	CHAR part_type = pmdrel->PartTypeAtLevel(0);
	// TODO: Currently ORCA doesn't support DynamicIndexScans on List, Hash Partitions
	// Ignore xform if table is list or hash partitioned
	if (part_type != gpmd::IMDRelation::ErelpartitionRange) {
		return;
	}

	ULONG partitionKeysCount = pmdrel->PartColumnCount();
	// TODO: Currently ORCA doesn't support Composite partitioning keys
	// Ignore xform if table has more than one partition key
	if (partitionKeysCount > 1 || pos->UlSortColumns() > 1) {
		return;
	}

	CExpression *boolConstExpr = nullptr;
	boolConstExpr = CUtils::PexprScalarConstBool(mp, true);

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	boolConstExpr->AddRef();
	pdrgpexpr->Append(boolConstExpr);

	popDynGet->AddRef();
	CExpression *pexprUpdtdRltn =
		GPOS_NEW(mp) CExpression(mp, popDynGet, boolConstExpr);

	CColRefSet *pcrsScalarExpr = popLimit->PcrsLocalUsed();

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);
		if (FIndexApplicableForOrderBy(mp, pos, pmdrel, pmdindex, popDynGet))
		{
			// get Scan direction
			EIndexScanDirection scan_direction =
				CXformUtils::GetIndexScanDirection(pos, pmdindex);
			// build IndexGet expression
			CExpression *pexprIndexGet = CXformUtils::PexprLogicalIndexGet(
				mp, md_accessor, pexprUpdtdRltn, popLimit->UlOpId(), pdrgpexpr,
				pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel, true,
				scan_direction);

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
//		columns. This function checks if
//	        1. ORDER BY columns are prefix of the index that has partition columns in its prefix
//	        2. Sort and Nulls Direction of ORDER BY columns is either equal or commutative to the index columns
//     Currently ORCA only supports DynamicIndexScans on partition columns.
//---------------------------------------------------------------------------
BOOL
CXformLimit2DynamicIndexGet::FIndexApplicableForOrderBy(
	CMemoryPool *mp, COrderSpec *pos, const IMDRelation *pmdrel,
	const IMDIndex *pmdindex, CLogicalDynamicGet *popDynGet)
{
	// Ordered IndexScan is only applicable for BTree index
	if (pmdindex->IndexType() != IMDIndex::EmdindBtree)
	{
		return false;
	}

	// get order by columns size
	ULONG totalOrderByCols = pos->UlSortColumns();
	CColRefArray *pdrgpcrOutput = popDynGet->PdrgpcrOutput();
	// get columns in the index
	CColRefArray *pdrgpcrIndexColumns = CXformUtils::PdrgpcrIndexKeys(
		mp, popDynGet->PdrgpcrOutput(), pmdindex, pmdrel);

	if (pdrgpcrIndexColumns->Size() < totalOrderByCols)
	{
		return false;
	}
	BOOL indexApplicable = true;
	// BitVectors to maintain required and derived sort, null directions.
	CBitVector *req_sort_direction =
		GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);
	CBitVector *derived_sort_direction =
		GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);
	CBitVector *req_nulls_direction =
		GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);
	CBitVector *derived_nulls_direction =
		GPOS_NEW(mp) CBitVector(mp, totalOrderByCols);

	const ULongPtrArray *part_col_indices = popDynGet->Ptabdesc()->PdrgpulPart();
	for (ULONG i = 0; i < totalOrderByCols; i++)
	{
		const CColRef *orderby_col = pos->Pcr(i);
		CColRef *index_col = (*pdrgpcrIndexColumns)[i];
		CColRef *part_col = (*pdrgpcrOutput)[*(*part_col_indices)[i]];

		if (!CColRef::Equals(part_col, index_col) || !CColRef::Equals(index_col, orderby_col)) {
			indexApplicable = false;
			break;
		}

		IMDId *greater_than_mdid =
			orderby_col->RetrieveType()->GetMdidForCmpType(IMDType::EcmptG);
		if (greater_than_mdid->Equals(pos->GetMdIdSortOp(i)))
		{
			// If order spec's sort mdid is DESC set req_sort_direction for the key
			req_sort_direction->ExchangeSet(i);
		}
		if (pmdindex->KeySortDirectionAt(i) == SORT_DESC)
		{
			// If index key's sort direction is DESC set derived_sort_direction for the key
			derived_sort_direction->ExchangeSet(i);
		}
		if (pos->Ent(i) == COrderSpec::EntFirst)
		{
			// If order spec's nulls direction is FIRST set req_nulls_direction for the key
			req_nulls_direction->ExchangeSet(i);
		}
		if (pmdindex->KeyNullsDirectionAt(i) == NULLS_FIRST)
		{
			// If index key's nulls direction is FIRST set derived_nulls_direction for the key
			derived_nulls_direction->ExchangeSet(i);
		}

		// If the derived, required sort directions and nulls directions are not equal or not commutative, then the index is not applicable.
		if (!(req_sort_direction->Equals(derived_sort_direction) &&
			  req_nulls_direction->Equals(derived_nulls_direction)) &&
			!(CXformUtils::FIndicesCommutative(req_sort_direction, derived_sort_direction,
								  i) &&
			  CXformUtils::FIndicesCommutative(req_nulls_direction, derived_nulls_direction,
								  i)))
		{
			indexApplicable = false;
			break;
		}

	}

	pdrgpcrIndexColumns->Release();
	GPOS_DELETE(req_sort_direction);
	GPOS_DELETE(derived_sort_direction);
	GPOS_DELETE(req_nulls_direction);
	GPOS_DELETE(derived_nulls_direction);
	return indexApplicable;
}

// EOF
