//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterJoin.cpp
//
//	@doc:
//		Implementation of left outer join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalLeftOuterJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterJoin::CLogicalLeftOuterJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalLeftOuterJoin::CLogicalLeftOuterJoin(CMemoryPool *mp,
											 CXform::EXformId origin_xform)
	: CLogicalJoin(mp, origin_xform)
{
	GPOS_ASSERT(nullptr != mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterJoin::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalLeftOuterJoin::DeriveMaxCard(CMemoryPool *,	 // mp
									 CExpressionHandle &exprhdl) const
{
	CMaxCard maxCard = exprhdl.DeriveMaxCard(0);
	CMaxCard maxCardInner = exprhdl.DeriveMaxCard(1);

	// if the inner has a max card of 0, that will not make the LOJ's
	// max card go to 0
	if (0 < maxCardInner.Ull())
	{
		maxCard *= maxCardInner;
	}

	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, maxCard);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterJoin::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftOuterJoin::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfPushDownLeftOuterJoin);
	(void) xform_set->ExchangeSet(CXform::ExfSimplifyLeftOuterJoin);
	(void) xform_set->ExchangeSet(CXform::ExfLeftOuterJoin2NLJoin);
	(void) xform_set->ExchangeSet(CXform::ExfLeftOuterJoin2HashJoin);
	(void) xform_set->ExchangeSet(
		CXform::ExfLeftOuter2InnerUnionAllLeftAntiSemiJoin);
	(void) xform_set->ExchangeSet(CXform::ExfJoin2BitmapIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfJoin2IndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfLeftJoin2RightJoin);
	(void) xform_set->ExchangeSet(CXform::ExfPushJoinBelowLeftUnionAll);
	(void) xform_set->ExchangeSet(CXform::ExfPushJoinBelowRightUnionAll);

	return xform_set;
}



// EOF
