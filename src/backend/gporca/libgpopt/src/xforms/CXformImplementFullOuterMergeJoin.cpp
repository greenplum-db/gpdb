//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 VMware, Inc. or its affiliates.

#include "gpopt/xforms/CXformImplementFullOuterMergeJoin.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalFullOuterJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalFullMergeJoin.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


// ctor
CXformImplementFullOuterMergeJoin::CXformImplementFullOuterMergeJoin(
	CMemoryPool *mp)
	: CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalFullOuterJoin(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // outer child
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // inner child
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternTree(mp))  // scalar child
			  ))
{
}

CXform::EXformPromise
CXformImplementFullOuterMergeJoin::Exfp(CExpressionHandle &exprhdl) const
{
	if (exprhdl.DeriveHasSubquery(2))
	{
		return CXform::ExfpNone;
	}
	return CXform::ExfpHigh;
}

void
CXformImplementFullOuterMergeJoin::Transform(CXformContext *pxfctxt,
											 CXformResult *pxfres,
											 CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CXformUtils::ImplementMergeJoin<CPhysicalFullMergeJoin>(pxfctxt, pxfres,
															pexpr);
}

// EOF
