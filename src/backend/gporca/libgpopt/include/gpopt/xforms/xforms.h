//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		xforms.h
//
//	@doc:
//		Collective include for all xform related headers
//---------------------------------------------------------------------------
#ifndef GPOPT_xforms_H
#define GPOPT_xforms_H

#include "gpopt/xforms/CXform.h"
#include "gpopt/xforms/CXformAddLimitAfterSplitGbAgg.h"
#include "gpopt/xforms/CXformAntiSemiJoinAntiSemiJoinNotInSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinAntiSemiJoinSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinInnerJoinSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinNotInAntiSemiJoinSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinNotInInnerJoinSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinNotInSemiJoinSwap.h"
#include "gpopt/xforms/CXformAntiSemiJoinSemiJoinSwap.h"
#include "gpopt/xforms/CXformCTEAnchor2Sequence.h"
#include "gpopt/xforms/CXformCTEAnchor2TrivialSelect.h"
#include "gpopt/xforms/CXformCollapseGbAgg.h"
#include "gpopt/xforms/CXformCollapseProject.h"
#include "gpopt/xforms/CXformContext.h"
#include "gpopt/xforms/CXformDelete2DML.h"
#include "gpopt/xforms/CXformDifference2LeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformDifferenceAll2LeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformDynamicGet2DynamicTableScan.h"
#include "gpopt/xforms/CXformDynamicIndexGet2DynamicIndexScan.h"
#include "gpopt/xforms/CXformEagerAgg.h"
#include "gpopt/xforms/CXformExpandFullOuterJoin.h"
#include "gpopt/xforms/CXformExpandNAryJoin.h"
#include "gpopt/xforms/CXformExpandNAryJoinDP.h"
#include "gpopt/xforms/CXformExpandNAryJoinDPv2.h"
#include "gpopt/xforms/CXformExpandNAryJoinGreedy.h"
#include "gpopt/xforms/CXformExpandNAryJoinMinCard.h"
#include "gpopt/xforms/CXformExternalGet2ExternalScan.h"
#include "gpopt/xforms/CXformFactory.h"
#include "gpopt/xforms/CXformGbAgg2Apply.h"
#include "gpopt/xforms/CXformGbAgg2HashAgg.h"
#include "gpopt/xforms/CXformGbAgg2ScalarAgg.h"
#include "gpopt/xforms/CXformGbAgg2StreamAgg.h"
#include "gpopt/xforms/CXformGbAggDedup2HashAggDedup.h"
#include "gpopt/xforms/CXformGbAggDedup2StreamAggDedup.h"
#include "gpopt/xforms/CXformGbAggWithMDQA2Join.h"
#include "gpopt/xforms/CXformGet2TableScan.h"
#include "gpopt/xforms/CXformImplementAssert.h"
#include "gpopt/xforms/CXformImplementBitmapTableGet.h"
#include "gpopt/xforms/CXformImplementCTEConsumer.h"
#include "gpopt/xforms/CXformImplementCTEProducer.h"
#include "gpopt/xforms/CXformImplementConstTableGet.h"
#include "gpopt/xforms/CXformImplementDML.h"
#include "gpopt/xforms/CXformImplementDynamicBitmapTableGet.h"
#include "gpopt/xforms/CXformImplementFullOuterMergeJoin.h"
#include "gpopt/xforms/CXformImplementIndexApply.h"
#include "gpopt/xforms/CXformImplementInnerCorrelatedApply.h"
#include "gpopt/xforms/CXformImplementInnerJoin.h"
#include "gpopt/xforms/CXformImplementLeftAntiSemiCorrelatedApply.h"
#include "gpopt/xforms/CXformImplementLeftAntiSemiCorrelatedApplyNotIn.h"
#include "gpopt/xforms/CXformImplementLeftOuterCorrelatedApply.h"
#include "gpopt/xforms/CXformImplementLeftSemiCorrelatedApply.h"
#include "gpopt/xforms/CXformImplementLeftSemiCorrelatedApplyIn.h"
#include "gpopt/xforms/CXformImplementLimit.h"
#include "gpopt/xforms/CXformImplementPartitionSelector.h"
#include "gpopt/xforms/CXformImplementRowTrigger.h"
#include "gpopt/xforms/CXformImplementSequence.h"
#include "gpopt/xforms/CXformImplementSequenceProject.h"
#include "gpopt/xforms/CXformImplementSplit.h"
#include "gpopt/xforms/CXformImplementTVF.h"
#include "gpopt/xforms/CXformImplementTVFNoArgs.h"
#include "gpopt/xforms/CXformImplementUnionAll.h"
#include "gpopt/xforms/CXformIndexGet2IndexOnlyScan.h"
#include "gpopt/xforms/CXformIndexGet2IndexScan.h"
#include "gpopt/xforms/CXformInlineCTEConsumer.h"
#include "gpopt/xforms/CXformInlineCTEConsumerUnderSelect.h"
#include "gpopt/xforms/CXformInnerApply2InnerJoin.h"
#include "gpopt/xforms/CXformInnerApply2InnerJoinNoCorrelations.h"
#include "gpopt/xforms/CXformInnerApplyWithOuterKey2InnerJoin.h"
#include "gpopt/xforms/CXformInnerJoin2HashJoin.h"
#include "gpopt/xforms/CXformInnerJoin2NLJoin.h"
#include "gpopt/xforms/CXformInnerJoinAntiSemiJoinNotInSwap.h"
#include "gpopt/xforms/CXformInnerJoinAntiSemiJoinSwap.h"
#include "gpopt/xforms/CXformInnerJoinSemiJoinSwap.h"
#include "gpopt/xforms/CXformInsert2DML.h"
#include "gpopt/xforms/CXformIntersect2Join.h"
#include "gpopt/xforms/CXformIntersectAll2LeftSemiJoin.h"
#include "gpopt/xforms/CXformJoin2BitmapIndexGetApply.h"
#include "gpopt/xforms/CXformJoin2IndexGetApply.h"
#include "gpopt/xforms/CXformJoinAssociativity.h"
#include "gpopt/xforms/CXformJoinCommutativity.h"
#include "gpopt/xforms/CXformJoinSwap.h"
#include "gpopt/xforms/CXformLeftAntiSemiApply2LeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations.h"
#include "gpopt/xforms/CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotIn.h"
#include "gpopt/xforms/CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoin2CrossProduct.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoin2HashJoin.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoin2NLJoin.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoinNotIn2CrossProduct.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoinNotIn2HashJoinNotIn.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoinNotIn2NLJoinNotIn.h"
#include "gpopt/xforms/CXformLeftJoin2RightJoin.h"
#include "gpopt/xforms/CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformLeftOuterApply2LeftOuterJoin.h"
#include "gpopt/xforms/CXformLeftOuterApply2LeftOuterJoinNoCorrelations.h"
#include "gpopt/xforms/CXformLeftOuterJoin2HashJoin.h"
#include "gpopt/xforms/CXformLeftOuterJoin2NLJoin.h"
#include "gpopt/xforms/CXformLeftSemiApply2LeftSemiJoin.h"
#include "gpopt/xforms/CXformLeftSemiApply2LeftSemiJoinNoCorrelations.h"
#include "gpopt/xforms/CXformLeftSemiApplyIn2LeftSemiJoin.h"
#include "gpopt/xforms/CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations.h"
#include "gpopt/xforms/CXformLeftSemiApplyInWithExternalCorrs2InnerJoin.h"
#include "gpopt/xforms/CXformLeftSemiApplyWithExternalCorrs2InnerJoin.h"
#include "gpopt/xforms/CXformLeftSemiJoin2CrossProduct.h"
#include "gpopt/xforms/CXformLeftSemiJoin2HashJoin.h"
#include "gpopt/xforms/CXformLeftSemiJoin2InnerJoin.h"
#include "gpopt/xforms/CXformLeftSemiJoin2InnerJoinUnderGb.h"
#include "gpopt/xforms/CXformLeftSemiJoin2NLJoin.h"
#include "gpopt/xforms/CXformMaxOneRow2Assert.h"
#include "gpopt/xforms/CXformProject2Apply.h"
#include "gpopt/xforms/CXformProject2ComputeScalar.h"
#include "gpopt/xforms/CXformPushDownLeftOuterJoin.h"
#include "gpopt/xforms/CXformPushGbBelowJoin.h"
#include "gpopt/xforms/CXformPushGbBelowUnion.h"
#include "gpopt/xforms/CXformPushGbBelowUnionAll.h"
#include "gpopt/xforms/CXformPushGbDedupBelowJoin.h"
#include "gpopt/xforms/CXformPushGbWithHavingBelowJoin.h"
#include "gpopt/xforms/CXformRemoveSubqDistinct.h"
#include "gpopt/xforms/CXformResult.h"
#include "gpopt/xforms/CXformRightOuterJoin2HashJoin.h"
#include "gpopt/xforms/CXformSelect2Apply.h"
#include "gpopt/xforms/CXformSelect2BitmapBoolOp.h"
#include "gpopt/xforms/CXformSelect2DynamicBitmapBoolOp.h"
#include "gpopt/xforms/CXformSelect2DynamicIndexGet.h"
#include "gpopt/xforms/CXformSelect2Filter.h"
#include "gpopt/xforms/CXformSelect2IndexGet.h"
#include "gpopt/xforms/CXformSemiJoinAntiSemiJoinNotInSwap.h"
#include "gpopt/xforms/CXformSemiJoinAntiSemiJoinSwap.h"
#include "gpopt/xforms/CXformSemiJoinInnerJoinSwap.h"
#include "gpopt/xforms/CXformSemiJoinSemiJoinSwap.h"
#include "gpopt/xforms/CXformSequenceProject2Apply.h"
#include "gpopt/xforms/CXformSimplifyGbAgg.h"
#include "gpopt/xforms/CXformSimplifyLeftOuterJoin.h"
#include "gpopt/xforms/CXformSimplifyProjectWithSubquery.h"
#include "gpopt/xforms/CXformSimplifySelectWithSubquery.h"
#include "gpopt/xforms/CXformSplitDQA.h"
#include "gpopt/xforms/CXformSplitGbAgg.h"
#include "gpopt/xforms/CXformSplitGbAggDedup.h"
#include "gpopt/xforms/CXformSplitLimit.h"
#include "gpopt/xforms/CXformSubqJoin2Apply.h"
#include "gpopt/xforms/CXformSubqNAryJoin2Apply.h"
#include "gpopt/xforms/CXformUnion2UnionAll.h"
#include "gpopt/xforms/CXformUnnestTVF.h"
#include "gpopt/xforms/CXformUpdate2DML.h"
#include "gpopt/xforms/CXformUtils.h"

#endif	// !GPOPT_xforms_H

// EOF
