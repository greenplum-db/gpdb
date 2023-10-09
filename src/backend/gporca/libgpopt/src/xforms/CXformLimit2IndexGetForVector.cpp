//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLimit2IndexGetForVector.cpp
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalIndexGet if order by columns
//		match any of the index prefix
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformLimit2IndexGetForVector.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/operators/CLogicalProject.h"
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
/*CXformLimit2IndexGet::CXformLimit2IndexGet
(CMemoryPool *mp)
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
}*/
CXformLimit2IndexGetForVector ::CXformLimit2IndexGetForVector(CMemoryPool *mp)
	:  // pattern
	  CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalLimit(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CLogicalProject(mp),
				  GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CLogicalGet(
							  mp)),	 // relational child     - Project child1
				  GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CPatternTree(
							  mp))	// scalar project list - Project child2
				  ),				//LIMIT CHILD 1
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(
						  mp)),	 // scalar child for offset  //LIMIT CHILD 2
			  GPOS_NEW(mp) CExpression(
				  mp,
				  GPOS_NEW(mp) CPatternLeaf(
					  mp))	// scalar child for number of rows //LIMIT CHILD 3
			  ))
{
}


//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGetForVector::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLimit2IndexGetForVector ::Exfp(CExpressionHandle &exprhdl) const
{
	if (exprhdl.DeriveHasSubquery(1))
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGetForVector::Transform
	//
	//	@doc:
	//		Actual transformation
	//
	//---------------------------------------------------------------------------
	void
	CXformLimit2IndexGetForVector ::Transform(CXformContext *pxfctxt,
											  CXformResult *pxfres,
											  CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();

	CLogicalLimit *popLimit = CLogicalLimit::PopConvert(pexpr->Pop());

	// 1. extract components
	CExpression *pexprLogicalProject = (*pexpr)[0];	 // Logical Project
	CExpression *pexprScalarOffset = (*pexpr)[1];	 // Scalar Offset
	CExpression *pexprScalarRows = (*pexpr)[2];		 // Scalar Rows


	CLogicalProject *popProject =
		CLogicalProject::PopConvert(pexprLogicalProject->Pop());

	// 2. Transforming Logical Get to Index Get.
	CExpression *pexprGet = (*pexprLogicalProject)[0];
	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprGet->Pop());

	// get the indices count of this relation
	const ULONG ulIndices = popGet->Ptabdesc()->IndexCount();

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

	// 2.1 Collecting index conditions
	CExpression *pexprScalarProjList = (*pexprLogicalProject)[1];

	// Check if query has condition on one column only.
	if (pexprScalarProjList->Arity() != 1)
	{
		return;
	}

	CExpression *pexprProjElem = (*pexprScalarProjList)[0];
	CExpression *pexprScalarOp = (*pexprProjElem)[0];

	// Add a condition on the operator ID, it should be L2 distace only
	// else return ??
	pexprScalarOp->AddRef();
	CExpressionArray *pdrgpexprConds = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexprConds->Append(pexprScalarOp);
	GPOS_ASSERT(pdrgpexprConds->Size() > 0);

	// 2.2 Collect used colrefs
	CColRefSet *pcrsScalarExpr = pexprScalarOp->DeriveUsedColumns();


	// 2.3 Other info for Index Get


	// Iterate through all the table indices and generate 'IndexGet'
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(popGet->Ptabdesc()->MDId());

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);

		// The index access method doesnot support backward scan, thus
		// the scan direction is fixed to forward scan only.
		EIndexScanDirection scan_direction = EForwardScan;

		// build IndexGet expression
		CExpression *pexprIndexGet = CXformUtils::PexprLogicalIndexGet(
			mp, md_accessor, pexprGet, popProject->UlOpId(), pdrgpexprConds,
			pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel, true,
			scan_direction);

		if (pexprIndexGet != nullptr)
		{
			popProject->AddRef();
			pexprScalarProjList->AddRef();

			CExpression *pexprLogicalProjectFinal = GPOS_NEW(mp)
				CExpression(mp, popProject, pexprIndexGet, pexprScalarProjList);

			pexprScalarOffset->AddRef();
			pexprScalarRows->AddRef();
			pos->AddRef();
			CExpression *pexprLimit = GPOS_NEW(mp) CExpression(
				mp,
				GPOS_NEW(mp) CLogicalLimit(
					mp, pos, popLimit->FGlobal(), popLimit->FHasCount(),
					popLimit->IsTopLimitUnderDMLorCTAS()),
				pexprLogicalProjectFinal, pexprScalarOffset, pexprScalarRows);

			pxfres->Add(pexprLimit);
		}
	}

	pdrgpexprConds->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLimit2IndexGetForVector::GetScanDirection
	//
	//	@doc:
	//		Function to validate if index is applicable and determine Index Scan
	//		direction, given OrderSpec and index columns. This function checks if
	//	        1. ORDER BY columns are prefix of the index columns
	//	        2. Sort and Nulls Direction of ORDER BY columns is either equal or
	//	           reversed to the index columns
	//---------------------------------------------------------------------------
EIndexScanDirection
CXformLimit2IndexGetForVector ::GetScanDirection(
	COrderSpec *pos, CColRefArray *pdrgpcrIndexColumns,
	const IMDIndex *pmdindex)
{
	// Ordered IndexScan is only applicable for BTree index
	if (pmdindex->IndexType() != IMDIndex::EmdindBtree)
	{
		return EisdSentinel;
	}
	if (pdrgpcrIndexColumns->Size() < pos->UlSortColumns())
	{
		return EisdSentinel;
	}

	EIndexScanDirection finalDirection = EisdSentinel;

	for (ULONG i = 0; i < pos->UlSortColumns(); i++)
	{
		// ORDER BY columns must match with leading index columns
		const CColRef *colref = pos->Pcr(i);
		if (!CColRef::Equals(colref, (*pdrgpcrIndexColumns)[i]))
		{
			return EisdSentinel;
		}

		// 1st bit represents sort direction, 1 for DESC.
		// 2nd bit represents nulls direction, 1 for NULLS FIRST.
		// track required order's sort, nulls direction
		ULONG reqOrder = 0;
		// track index key's sort, nulls direction
		ULONG indexOrder = 0;
		IMDId *greater_than_mdid =
			colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptG);
		if (greater_than_mdid->Equals(pos->GetMdIdSortOp(i)))
		{
			// If order spec's sort mdid is DESC
			reqOrder |= 1 << 0;
		}
		if (pos->Ent(i) == COrderSpec::EntFirst)
		{
			// If order spec's nulls direction is FIRST
			reqOrder |= 1 << 1;
		}

		if (pmdindex->KeySortDirectionAt(i) == SORT_DESC)
		{
			// If index key's sort direction is DESC
			indexOrder |= 1 << 0;
		}
		if (pmdindex->KeyNullsDirectionAt(i) == COrderSpec::EntFirst)
		{
			// If index key's nulls direction is FIRST
			indexOrder |= 1 << 1;
		}

		EIndexScanDirection direction;
		if (reqOrder == indexOrder)
		{
			// Choose ForwardScan if index order and required order matches
			direction = EForwardScan;
		}
		else if ((reqOrder ^ indexOrder) == 3)
		{
			// Choose ForwardScan if index order and required order are reversed
			direction = EBackwardScan;
		}
		else
		{
			return EisdSentinel;
		}

		if (i == 0)
		{
			// first column's scan direction decides the overall direction
			finalDirection = direction;
		}
		else if (finalDirection != direction)
		{
			return EisdSentinel;
		}
	}

	return finalDirection;
}

// EOF
