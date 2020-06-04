	//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftSemiJoin.cpp
//
//	@doc:
//		Implementation of left semi join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"

#include "naucrates/statistics/CStatsPredUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::CLogicalLeftSemiJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalLeftSemiJoin::CLogicalLeftSemiJoin
	(
	CMemoryPool *mp
	)
	:
	CLogicalJoin(mp)
{
	GPOS_ASSERT(NULL != mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftSemiJoin::PxfsCandidates
	(
	CMemoryPool *mp
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfSemiJoinSemiJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfSemiJoinAntiSemiJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfSemiJoinAntiSemiJoinNotInSwap);
	(void) xform_set->ExchangeSet(CXform::ExfSemiJoinInnerJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfLeftSemiJoin2InnerJoin);
	(void) xform_set->ExchangeSet(CXform::ExfLeftSemiJoin2InnerJoinUnderGb);
	(void) xform_set->ExchangeSet(CXform::ExfLeftSemiJoin2CrossProduct);
	(void) xform_set->ExchangeSet(CXform::ExfLeftSemiJoin2NLJoin);
	(void) xform_set->ExchangeSet(CXform::ExfLeftSemiJoin2HashJoin);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalLeftSemiJoin::DeriveOutputColumns
	(
	CMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(3 == exprhdl.Arity());

	return PcrsDeriveOutputPassThru(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalLeftSemiJoin::DeriveKeyCollection
	(
	CMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalLeftSemiJoin::DeriveMaxCard
	(
	CMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, exprhdl.DeriveMaxCard(0));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalLeftSemiJoin::PstatsDerive
	(
	CMemoryPool *mp,
	CStatsPredJoinArray *join_preds_stats,
	IStatistics *outer_stats,
	IStatistics *inner_side_stats
	)
{
	return outer_stats->CalcLSJoinStats(mp, inner_side_stats, join_preds_stats);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiJoin::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalLeftSemiJoin::PstatsDerive
	(
	CMemoryPool *mp,
	CExpressionHandle &exprhdl,
	IStatisticsArray * // not used
	)
	const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);
	IStatistics *outer_stats = exprhdl.Pstats(0);
	IStatistics *inner_side_stats = exprhdl.Pstats(1);
	CStatsPredJoinArray *join_preds_stats = CStatsPredUtils::ExtractJoinStatsFromExprHandle(mp, exprhdl, true/*semi-join*/);
	IStatistics *pstatsSemiJoin = PstatsDerive(mp, join_preds_stats, outer_stats, inner_side_stats);

	join_preds_stats->Release();

	return pstatsSemiJoin;
}
// EOF
