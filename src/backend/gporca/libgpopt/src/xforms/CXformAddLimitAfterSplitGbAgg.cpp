//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformAddLimitAfterSplitGbAgg.cpp
//
//	@doc:
//		Implementation of the transform that adds a local limit in between
//   	splitted local and global aggregate containing distinct.
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformAddLimitAfterSplitGbAgg.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/search/CGroupProxy.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;

CXformAddLimitAfterSplitGbAgg::CXformAddLimitAfterSplitGbAgg(CMemoryPool *mp)
	:  // pattern
	  CXformExploration(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalLimit(mp),  // Outer limit operator
		  GPOS_NEW(mp) CExpression(
			  mp,
			  GPOS_NEW(mp) CLogicalLimit(
				  mp),	// Inner limit(Local Limit after splitting)
			  GPOS_NEW(mp) CExpression(
				  mp,
				  GPOS_NEW(mp)
					  CLogicalGbAgg(mp),  // GbAgg, first child of local limit
				  GPOS_NEW(mp) CExpression(
					  mp,
					  GPOS_NEW(mp) CLogicalGbAgg(
						  mp),	// Inner Aggregate(Local GbAgg after splitting)
					  GPOS_NEW(mp)
						  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),
					  GPOS_NEW(mp)
						  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))),
				  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(
						  mp)),	 // Second child of inner limit, Limit offset
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(
						  mp))),  // Third child of inner limit, Number of rows
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CPatternLeaf(
					  mp)),	 // Second child of outer limit, Limit offset
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CPatternLeaf(
					  mp))	// Third child of outer limit, Number of rows
		  ))
{
}

CXform::EXformPromise
CXformAddLimitAfterSplitGbAgg::Exfp(CExpressionHandle &exprhdl) const
{
	CLogicalLimit *popLimit = CLogicalLimit::PopConvert(exprhdl.Pop());
	// Transform is not applicable if limit operator doesn't have rows or if the
	// operator is not global.
	if (!popLimit->FGlobal() || !popLimit->FHasCount())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


// Applying this transform converts below

// Input Pattern
//+--CLogicalLimit <empty> global
//   |--CLogicalLimit <empty> local
//		|--CLogicalGbAgg( Global )
//		|--|--CLogicalGbAgg( Local )
//		|  +--CScalarProjectList
//	 |--CScalarConst (0)
//	 +--CScalarConst (3)

// Transformed Pattern
//+--CLogicalLimit <empty> global
//   |--CLogicalLimit <empty> local
//		|--CLogicalGbAgg( Global )
//			|--CLogicalLimit( Local )
//				|--CLogicalGbAgg( Local )
//		    |--CScalarConst (0)
//	        +--CScalarConst (3)
//	    +--CScalarProjectList
//	|--CScalarConst (0)
//  +--CScalarConst (3)
void
CXformAddLimitAfterSplitGbAgg::Transform(CXformContext *pxfctxt,
										 CXformResult *pxfres,
										 CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(nullptr != pxfres);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));


	CMemoryPool *mp = pxfctxt->Pmp();
	// extract components
	CLogicalLimit *popOuterLimit = CLogicalLimit::PopConvert(pexpr->Pop());
	CExpression *pexprInnerLimit = (*pexpr)[0];
	CLogicalLimit *popInnerLimit =
		CLogicalLimit::PopConvert(pexprInnerLimit->Pop());
	if (!popInnerLimit->FGlobal())
	{
		CExpression *pexpOuterGbAgg = (*pexprInnerLimit)[0];
		COperator *outerGbAggOp = pexpOuterGbAgg->Pop();
		CLogicalGbAgg *popOuterAgg = CLogicalGbAgg::PopConvert(outerGbAggOp);
		// Check for global/local status of outer gb agg.
		if (popOuterAgg->FGlobal())
		{
			CExpression *pexprGbAggProjectionList = (*pexpOuterGbAgg)[1];
			// If Gb Aggregate does not have any other aggregate functions but Distinct
			if (pexprGbAggProjectionList->Arity() == 0)
			{
				// Take first child of global Gb Agg
				CExpression *pexprInnerGbAgg = (*pexpOuterGbAgg)[0];
				CLogicalGbAgg *popInnerGbAgg =
					CLogicalGbAgg::PopConvert(pexprInnerGbAgg->Pop());
				if (!popInnerGbAgg->FGlobal())
				{
					// Add limit on top of it with scalar start and scalar rows.
					CExpression *pexprScalarStart = (*pexpr)[1];
					CExpression *pexprScalarRows = (*pexpr)[2];
					COrderSpec *pos = popOuterLimit->Pos();
					pexprInnerGbAgg->AddRef();
					// assemble local limit operator
					CExpression *pexprLimitLocal =
						PexprLimit(mp, pexprInnerGbAgg, pexprScalarStart,
								   pexprScalarRows, pos,
								   false,  // fGlobal
								   popOuterLimit->FHasCount(),
								   popOuterLimit->IsTopLimitUnderDMLorCTAS());

					outerGbAggOp->AddRef();
					pexprGbAggProjectionList->AddRef();

					// Create new first global Logical Aggregate
					CExpression *pexprNewFirstAgg = GPOS_NEW(mp)
						CExpression(mp, outerGbAggOp, pexprLimitLocal,
									pexprGbAggProjectionList);

					// Create local limit wrapper on top of newly created global aggregate
					CExpression *pexprNewExprLocal =
						PexprLimit(mp, pexprNewFirstAgg, pexprScalarStart,
								   pexprScalarRows, pos,
								   false,  // fGlobal
								   popOuterLimit->FHasCount(),
								   popOuterLimit->IsTopLimitUnderDMLorCTAS());

					// Create final global limit wrapper on top of local limit created above
					CExpression *pexprNewExpr =
						PexprLimit(mp, pexprNewExprLocal, pexprScalarStart,
								   pexprScalarRows, pos,
								   true,  // fGlobal
								   popOuterLimit->FHasCount(),
								   popOuterLimit->IsTopLimitUnderDMLorCTAS());

					pxfres->Add(pexprNewExpr);
				}
			}
		}
	}
}


// Function to assemble limit expressions
CExpression *
CXformAddLimitAfterSplitGbAgg::PexprLimit(
	CMemoryPool *mp, CExpression *pexprRelational,
	CExpression *pexprScalarStart, CExpression *pexprScalarRows,
	COrderSpec *pos, BOOL fGlobal, BOOL fHasCount, BOOL fTopLimitUnderDML)
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
