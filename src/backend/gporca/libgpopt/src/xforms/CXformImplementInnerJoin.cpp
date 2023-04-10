//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformImplementInnerJoin.cpp
//
//	@doc:
//		Transform inner join to inner Hash Join or Inner Nested Loop Join
//		(if a Hash Join is not possible)
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformImplementInnerJoin.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalInnerHashJoin.h"
#include "gpopt/operators/CPhysicalInnerNLJoin.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementInnerJoin::CXformImplementInnerJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformImplementInnerJoin::CXformImplementInnerJoin(CMemoryPool *mp)
	:  // pattern
	  CXformImplementation(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalInnerJoin(mp),
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
//		CXformImplementInnerJoin::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformImplementInnerJoin::Exfp(CExpressionHandle &exprhdl) const
{
	return CXformUtils::ExfpLogicalJoin2PhysicalJoin(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementInnerJoin::Transform
//
//	@doc:
//		actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementInnerJoin::Transform(CXformContext *pxfctxt,
									CXformResult *pxfres,
									CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	GPOS_ASSERT(pxfres->Size() == 0);

	if (!GPOS_FTRACE(EopttraceDisableInnerHashJoin))
	{
		CXformUtils::ImplementHashJoin<CPhysicalInnerHashJoin>(pxfctxt, pxfres,
															   pexpr);
	}

	if ((GPOS_FTRACE(EopttraceForceComprehensiveJoinImplementation) ||
		 pxfres->Size() == 0) &&
		!GPOS_FTRACE(EopttraceDisableInnerNLJ))
	{
		CXformUtils::ImplementNLJoin<CPhysicalInnerNLJoin>(pxfctxt, pxfres,
														   pexpr);
	}
}

// EOF
