//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXform.h
//
//	@doc:
//		Base class for all transformations: substitution, exploration,
//		and implementation
//---------------------------------------------------------------------------
#ifndef GPOPT_CXform_H
#define GPOPT_CXform_H

#include "gpos/base.h"
#include "gpos/common/CEnumSet.h"
#include "gpos/common/CEnumSetIter.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CPhysical.h"
#include "gpopt/xforms/CXform.h"
#include "gpopt/xforms/CXformContext.h"
#include "gpopt/xforms/CXformResult.h"
#include "naucrates/traceflags/traceflags.h"

// Macro for enabling and disabling xforms
#define GPOPT_DISABLE_XFORM_TF(x) EopttraceDisableXformBase + x
#define GPOPT_ENABLE_XFORM(x) GPOS_UNSET_TRACE(GPOPT_DISABLE_XFORM_TF(x))
#define GPOPT_DISABLE_XFORM(x) GPOS_SET_TRACE(GPOPT_DISABLE_XFORM_TF(x))
#define GPOPT_FENABLED_XFORM(x) !GPOS_FTRACE(GPOPT_DISABLE_XFORM_TF(x))
#define GPOPT_FDISABLED_XFORM(x) GPOS_FTRACE(GPOPT_DISABLE_XFORM_TF(x))


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXform
//
//	@doc:
//		base class for all transformations
//
//---------------------------------------------------------------------------
class CXform : public CRefCount, public DbgPrintMixin<CXform>
{
private:
	// pattern
	CExpression *m_pexpr;

public:
	CXform(CXform &) = delete;

	// identification
	//
	// IMPORTANT: when adding new Xform Ids, please add them near
	// the end of the enum (before ExfInvalid). Xform Ids are sometimes
	// referenced using their location in the array (e.g. when disabling
	// xforms using traceflags), so shifting these ids may result in
	// accidentally disabling the wrong xform

	enum EXformId
	{
		ExfProject2ComputeScalar = 0,
		ExfExpandNAryJoin,
		ExfExpandNAryJoinMinCard,
		ExfExpandNAryJoinDP,
		ExfGet2TableScan,
		ExfIndexGet2IndexScan,
		ExfDynamicGet2DynamicTableScan,
		ExfDynamicIndexGet2DynamicIndexScan,
		ExfImplementSequence,
		ExfImplementConstTableGet,
		ExfUnnestTVF,
		ExfImplementTVF,
		ExfImplementTVFNoArgs,
		ExfSelect2Filter,
		ExfSelect2IndexGet,
		ExfSelect2DynamicIndexGet,
		ExfSelect2PartialDynamicIndexGet____removed,
		ExfSimplifySelectWithSubquery,
		ExfSimplifyProjectWithSubquery,
		ExfSelect2Apply,
		ExfProject2Apply,
		ExfGbAgg2Apply,
		ExfSubqJoin2Apply,
		ExfSubqNAryJoin2Apply,
		ExfInnerJoin2IndexGetApply____removed,
		ExfInnerJoin2DynamicIndexGetApply____removed,
		ExfInnerApplyWithOuterKey2InnerJoin,
		ExfInnerJoin2NLJoin____removed,
		ExfImplementIndexApply,
		ExfInnerJoin2HashJoin____removed,
		ExfInnerApply2InnerJoin,
		ExfInnerApply2InnerJoinNoCorrelations,
		ExfImplementInnerCorrelatedApply,
		ExfLeftOuterApply2LeftOuterJoin,
		ExfLeftOuterApply2LeftOuterJoinNoCorrelations,
		ExfImplementLeftOuterCorrelatedApply,
		ExfLeftSemiApply2LeftSemiJoin,
		ExfLeftSemiApplyWithExternalCorrs2InnerJoin,
		ExfLeftSemiApply2LeftSemiJoinNoCorrelations,
		ExfLeftAntiSemiApply2LeftAntiSemiJoin,
		ExfLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations,
		ExfLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotIn,
		ExfLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations,
		ExfPushDownLeftOuterJoin,
		ExfSimplifyLeftOuterJoin,
		ExfLeftOuterJoin2NLJoin,
		ExfLeftOuterJoin2HashJoin,
		ExfLeftSemiJoin2NLJoin,
		ExfLeftSemiJoin2HashJoin,
		ExfLeftAntiSemiJoin2CrossProduct,
		ExfLeftAntiSemiJoinNotIn2CrossProduct,
		ExfLeftAntiSemiJoin2NLJoin,
		ExfLeftAntiSemiJoinNotIn2NLJoinNotIn,
		ExfLeftAntiSemiJoin2HashJoin,
		ExfLeftAntiSemiJoinNotIn2HashJoinNotIn,
		ExfGbAgg2HashAgg,
		ExfGbAgg2StreamAgg,
		ExfGbAgg2ScalarAgg,
		ExfGbAggDedup2HashAggDedup,
		ExfGbAggDedup2StreamAggDedup,
		ExfImplementLimit,
		ExfIntersectAll2LeftSemiJoin,
		ExfIntersect2Join,
		ExfDifference2LeftAntiSemiJoin,
		ExfDifferenceAll2LeftAntiSemiJoin,
		ExfUnion2UnionAll,
		ExfImplementUnionAll,
		ExfInsert2DML,
		ExfDelete2DML,
		ExfUpdate2DML,
		ExfImplementDML,
		ExfImplementRowTrigger____removed,
		ExfImplementSplit,
		ExfJoinCommutativity,
		ExfJoinAssociativity,
		ExfSemiJoinSemiJoinSwap,
		ExfSemiJoinAntiSemiJoinSwap,
		ExfSemiJoinAntiSemiJoinNotInSwap,
		ExfSemiJoinInnerJoinSwap,
		ExfAntiSemiJoinAntiSemiJoinSwap,
		ExfAntiSemiJoinAntiSemiJoinNotInSwap,
		ExfAntiSemiJoinSemiJoinSwap,
		ExfAntiSemiJoinInnerJoinSwap,
		ExfAntiSemiJoinNotInAntiSemiJoinSwap,
		ExfAntiSemiJoinNotInAntiSemiJoinNotInSwap,
		ExfAntiSemiJoinNotInSemiJoinSwap,
		ExfAntiSemiJoinNotInInnerJoinSwap,
		ExfInnerJoinSemiJoinSwap,
		ExfInnerJoinAntiSemiJoinSwap,
		ExfInnerJoinAntiSemiJoinNotInSwap,
		ExfLeftSemiJoin2InnerJoin,
		ExfLeftSemiJoin2InnerJoinUnderGb,
		ExfLeftSemiJoin2CrossProduct,
		ExfSplitLimit,
		ExfSimplifyGbAgg,
		ExfCollapseGbAgg,
		ExfPushGbBelowJoin,
		ExfPushGbDedupBelowJoin,
		ExfPushGbWithHavingBelowJoin,
		ExfPushGbBelowUnion,
		ExfPushGbBelowUnionAll,
		ExfSplitGbAgg,
		ExfSplitGbAggDedup,
		ExfSplitDQA,
		ExfSequenceProject2Apply,
		ExfImplementSequenceProject,
		ExfImplementAssert,
		ExfCTEAnchor2Sequence,
		ExfCTEAnchor2TrivialSelect,
		ExfInlineCTEConsumer,
		ExfInlineCTEConsumerUnderSelect,
		ExfImplementCTEProducer,
		ExfImplementCTEConsumer,
		ExfExpandFullOuterJoin,
		ExfForeignGet2ForeignScan,
		ExfSelect2BitmapBoolOp,
		ExfSelect2DynamicBitmapBoolOp,
		ExfImplementBitmapTableGet,
		ExfImplementDynamicBitmapTableGet,
		ExfInnerJoin2PartialDynamicIndexGetApply____removed,
		ExfLeftOuter2InnerUnionAllLeftAntiSemiJoin,
		ExfImplementLeftSemiCorrelatedApply,
		ExfImplementLeftSemiCorrelatedApplyIn,
		ExfImplementLeftAntiSemiCorrelatedApply,
		ExfImplementLeftAntiSemiCorrelatedApplyNotIn,
		ExfLeftSemiApplyIn2LeftSemiJoin,
		ExfLeftSemiApplyInWithExternalCorrs2InnerJoin,
		ExfLeftSemiApplyIn2LeftSemiJoinNoCorrelations,
		ExfInnerJoin2BitmapIndexGetApply____removed,
		ExfImplementPartitionSelector,
		ExfMaxOneRow2Assert,
		ExfInnerJoinWithInnerSelect2IndexGetApply____removed,
		ExfInnerJoinWithInnerSelect2DynamicIndexGetApply____removed,
		ExfInnerJoinWithInnerSelect2PartialDynamicIndexGetApply____removed,
		ExfInnerJoin2DynamicBitmapIndexGetApply____removed,
		ExfInnerJoinWithInnerSelect2BitmapIndexGetApply____removed,
		ExfInnerJoinWithInnerSelect2DynamicBitmapIndexGetApply____removed,
		ExfGbAggWithMDQA2Join,
		ExfCollapseProject,
		ExfRemoveSubqDistinct,
		ExfLeftOuterJoin2BitmapIndexGetApply____removed,
		ExfLeftOuterJoin2IndexGetApply____removed,
		ExfLeftOuterJoinWithInnerSelect2BitmapIndexGetApply____removed,
		ExfLeftOuterJoinWithInnerSelect2IndexGetApply____removed,
		ExfExpandNAryJoinGreedy,
		ExfEagerAgg,
		ExfExpandNAryJoinDPv2,
		ExfImplementFullOuterMergeJoin,
		ExfLeftOuterJoin2DynamicBitmapIndexGetApply____removed,
		ExfLeftOuterJoin2DynamicIndexGetApply____removed,
		ExfLeftOuterJoinWithInnerSelect2DynamicBitmapIndexGetApply____removed,
		ExfLeftOuterJoinWithInnerSelect2DynamicIndexGetApply____removed,
		ExfIndexGet2IndexOnlyScan,
		ExfJoin2BitmapIndexGetApply,
		ExfJoin2IndexGetApply,
		ExfMultiExternalGet2MultiExternalScan____removed,
		ExfExpandDynamicGetWithExternalPartitions____removed,
		ExfLeftJoin2RightJoin,
		ExfRightOuterJoin2HashJoin,
		ExfImplementInnerJoin,
		ExfDynamicForeignGet2DynamicForeignScan,
		ExfExpandDynamicGetWithForeignPartitions,
		ExfInvalid,
		ExfSentinel = ExfInvalid
	};

	// promise levels;
	// used for prioritizing xforms as well as bypassing inapplicable xforms
	enum EXformPromise
	{
		ExfpNone,	 // xform must not be used as it fails a precondition
		ExfpLow,	 // xform has low priority
		ExfpMedium,	 // xform has medium priority
		ExfpHigh	 // xform has high priority
	};

	// ctor
	explicit CXform(CExpression *pexpr);

	// dtor
	~CXform() override;

	// ident accessors
	virtual EXformId Exfid() const = 0;

	// return a string for xform name
	virtual const CHAR *SzId() const = 0;

	// the following functions check xform type

	// is xform substitution?
	virtual BOOL
	FSubstitution() const
	{
		return false;
	}

	// is xform exploration?
	virtual BOOL
	FExploration() const
	{
		return false;
	}

	// is xform implementation?
	virtual BOOL
	FImplementation() const
	{
		return false;
	}

	// actual transformation
	virtual void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
						   CExpression *pexpr) const = 0;

	// accessor
	CExpression *
	PexprPattern() const
	{
		return m_pexpr;
	}

	// check compatibility with another xform
	virtual BOOL FCompatible(CXform::EXformId)
	{
		return true;
	}

	// compute xform promise for a given expression handle
	virtual EXformPromise Exfp(CExpressionHandle &exprhdl) const = 0;

	// print
	IOstream &OsPrint(IOstream &os) const;

#ifdef GPOS_DEBUG

	// verify pattern against given expression
	BOOL FCheckPattern(CExpression *pexpr) const;

	// verify xform promise on the given expression
	static BOOL FPromising(CMemoryPool *mp, const CXform *pxform,
						   CExpression *pexpr);

#endif	// GPOS_DEBUG

	// equality function over xform ids
	static BOOL FEqualIds(const CHAR *szIdOne, const CHAR *szIdTwo);

	// returns a set containing all xforms related to nl join
	// caller takes ownership of the returned set
	static CBitSet *PbsNLJoinXforms(CMemoryPool *mp);

	// returns a set containing all xforms related to index join
	// caller takes ownership of the returned set
	static CBitSet *PbsIndexJoinXforms(CMemoryPool *mp);

	// returns a set containing all xforms related to bitmap indexes
	// caller takes ownership of the returned set
	static CBitSet *PbsBitmapIndexXforms(CMemoryPool *mp);

	// returns a set containing all xforms that generate a plan with a hash join
	// caller takes ownership of the returned set
	static CBitSet *PbsHashJoinXforms(CMemoryPool *mp);

	// returns a set containing xforms to use only the join order as available
	// in the query
	static CBitSet *PbsJoinOrderInQueryXforms(CMemoryPool *mp);

	// returns a set containing xforms to use combination of greedy xforms
	// for join order
	static CBitSet *PbsJoinOrderOnGreedyXforms(CMemoryPool *mp);

	// returns a set containing xforms to use for exhaustive join order
	static CBitSet *PbsJoinOrderOnExhaustiveXforms(CMemoryPool *mp);

	// returns a set containing xforms to use for exhaustive2 join order
	static CBitSet *PbsJoinOrderOnExhaustive2Xforms(CMemoryPool *mp);

	// return true if xform should be applied only once.
	// for expression of type CPatternTree, in deep trees, the number
	// of expressions generated for group expression can be significantly
	// large causing the Xform to be applied many times. This can lead to
	// significantly long planning time, so such Xform should only be applied once
	virtual BOOL IsApplyOnce();

};	// class CXform

// shorthand for printing
inline IOstream &
operator<<(IOstream &os, CXform &xform)
{
	return xform.OsPrint(os);
}

// shorthands for enum sets and iterators of xform ids
using CXformSet = CEnumSet<CXform::EXformId, CXform::ExfSentinel>;
using CXformSetIter = CEnumSetIter<CXform::EXformId, CXform::ExfSentinel>;
}  // namespace gpopt


#endif	// !GPOPT_CXform_H

// EOF
