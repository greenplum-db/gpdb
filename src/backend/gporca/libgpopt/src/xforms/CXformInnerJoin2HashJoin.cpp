//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformInnerJoin2HashJoin.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformInnerJoin2HashJoin.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalInnerHashJoin.h"
#include "gpopt/operators/CPhysicalInnerNLJoin.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformInnerJoin2HashJoin::CXformInnerJoin2HashJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformInnerJoin2HashJoin::CXformInnerJoin2HashJoin(CMemoryPool *mp)
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
//		CXformInnerJoin2HashJoin::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformInnerJoin2HashJoin::Exfp(CExpressionHandle &) const
{
	return CXform::ExfpNone;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformInnerJoin2HashJoin::Transform
//
//	@doc:
//		actual transformation
//		Deprecated in favor of CXformImplementInnerJoin.
//
//---------------------------------------------------------------------------
void
CXformInnerJoin2HashJoin::Transform(CXformContext *, CXformResult *,
									CExpression *) const
{
}

// EOF
