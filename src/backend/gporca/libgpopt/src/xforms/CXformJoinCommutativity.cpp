//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformJoinCommutativity.cpp
//
//	@doc:
//		Implementation of commutativity transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformJoinCommutativity.h"

#include "gpopt/operators/CLogicalFullOuterJoin.h"
#include "gpopt/operators/CLogicalInnerJoin.h"

using namespace gpopt;



//---------------------------------------------------------------------------
//	@function:
//		CXformJoinCommutativity::FCompatible
//
//	@doc:
//		Compatibility function for join commutativity
//
//---------------------------------------------------------------------------
BOOL
CXformJoinCommutativity::FCompatible(CXform::EXformId exfid)
{
	BOOL fCompatible = true;

	switch (exfid)
	{
		case CXform::ExfInnerJoinCommutativity:
		case CXform::ExfFullJoinCommutativity:
			fCompatible = false;
			break;
		default:
			fCompatible = true;
	}

	return fCompatible;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformJoinCommutativity::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformJoinCommutativity::Transform(CXformContext *pxfctxt, CXformResult *pxfres,
								   CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();

	// extract components
	CExpression *pexprLeft = (*pexpr)[0];
	CExpression *pexprRight = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	// addref children
	pexprLeft->AddRef();
	pexprRight->AddRef();
	pexprScalar->AddRef();

	CExpression *pexprAlt = nullptr;
	// assemble transformed expression
	if (COperator::EopLogicalInnerJoin == pexpr->Pop()->Eopid())
	{
		pexprAlt = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(
			mp, pexprRight, pexprLeft, pexprScalar);
	}
	else
	{
		GPOS_ASSERT(COperator::EopLogicalFullOuterJoin ==
					pexpr->Pop()->Eopid());
		pexprAlt = CUtils::PexprLogicalJoin<CLogicalFullOuterJoin>(
			mp, pexprRight, pexprLeft, pexprScalar);
	}

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF
