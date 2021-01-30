//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CXformRightOuterJoin2HashJoin.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformRightOuterJoin2HashJoin.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalRightOuterJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalRightOuterHashJoin.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/xforms/CXformUtils.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformRightOuterJoin2HashJoin::CXformRightOuterJoin2HashJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformRightOuterJoin2HashJoin::CXformRightOuterJoin2HashJoin(CMemoryPool *mp)
	:  // pattern
	  CXformImplementation(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalRightOuterJoin(mp),
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // left child
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // right child
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))  // predicate
		  ))
{
}


//---------------------------------------------------------------------------
//	@function:
//		CXformRightOuterJoin2HashJoin::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformRightOuterJoin2HashJoin::Exfp(CExpressionHandle &exprhdl) const
{
	return CXformUtils::ExfpLogicalJoin2PhysicalJoin(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformRightOuterJoin2HashJoin::Transform
//
//	@doc:
//		actual transformation
//
//---------------------------------------------------------------------------
void
CXformRightOuterJoin2HashJoin::Transform(CXformContext *pxfctxt,
										 CXformResult *pxfres,
										 CExpression *pexpr) const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	// If the inner row estimate is an arbitary factor larger than the outer, don't generate a ROJ alternative.
	// Although the ROJ may still be better due to partition selection, which isn't considered below,
	// we're more cautious so we don't increase optimization time too much and that we account for
	// poor cardinality estimation. This is admittedly conservative, but mis-estimating the hash side
	// of a ROJ can cause spilling.
	CDouble outerRows = (*pexpr)[0]->Pstats()->Rows();
	CDouble outerWidth = (*pexpr)[0]->Pstats()->Width();
	CDouble innerRows = (*pexpr)[1]->Pstats()->Rows();
	CDouble innerWidth = (*pexpr)[1]->Pstats()->Width();

	CDouble confidenceFactor = 2 * (*pexpr)[1]->DeriveJoinDepth();
	if (innerRows * innerWidth * confidenceFactor > outerRows * outerWidth)
	{
		return;
	}
	CXformUtils::ImplementHashJoin<CPhysicalRightOuterHashJoin>(pxfctxt, pxfres,
																pexpr);
}


// EOF
