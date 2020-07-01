#
# Makefile for optimizer
#
# src/backend/gporca/libgpopt/src/xforms/Makefile
#

subdir = src/backend/gporca/libgpopt/src/xforms
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

OBJS        = CDecorrelator.o \
              CJoinOrder.o \
              CJoinOrderDP.o \
              CJoinOrderDPv2.o \
              CJoinOrderGreedy.o \
              CJoinOrderMinCard.o \
              CSubqueryHandler.o \
              CXform.o \
              CXformCTEAnchor2Sequence.o \
              CXformCTEAnchor2TrivialSelect.o \
              CXformCollapseGbAgg.o \
              CXformCollapseProject.o \
              CXformDelete2DML.o \
              CXformDifference2LeftAntiSemiJoin.o \
              CXformDifferenceAll2LeftAntiSemiJoin.o \
              CXformDynamicGet2DynamicTableScan.o \
              CXformDynamicIndexGet2DynamicIndexScan.o \
              CXformEagerAgg.o \
              CXformExpandFullOuterJoin.o \
              CXformExpandNAryJoin.o \
              CXformExpandNAryJoinDP.o \
              CXformExpandNAryJoinDPv2.o \
              CXformExpandNAryJoinGreedy.o \
              CXformExpandNAryJoinMinCard.o \
              CXformExploration.o \
              CXformExternalGet2ExternalScan.o \
              CXformFactory.o \
              CXformGbAgg2Apply.o \
              CXformGbAgg2HashAgg.o \
              CXformGbAgg2ScalarAgg.o \
              CXformGbAgg2StreamAgg.o \
              CXformGbAggDedup2HashAggDedup.o \
              CXformGbAggDedup2StreamAggDedup.o \
              CXformGbAggWithMDQA2Join.o \
              CXformGet2TableScan.o \
              CXformImplementAssert.o \
              CXformImplementBitmapTableGet.o \
              CXformImplementCTEConsumer.o \
              CXformImplementCTEProducer.o \
              CXformImplementConstTableGet.o \
              CXformImplementDML.o \
              CXformImplementDynamicBitmapTableGet.o \
              CXformImplementFullOuterMergeJoin.o \
              CXformImplementLimit.o \
              CXformImplementPartitionSelector.o \
              CXformImplementRowTrigger.o \
              CXformImplementSequence.o \
              CXformImplementSequenceProject.o \
              CXformImplementSplit.o \
              CXformImplementTVF.o \
              CXformImplementTVFNoArgs.o \
              CXformImplementUnionAll.o \
              CXformImplementation.o \
              CXformIndexGet2IndexScan.o \
              CXformIndexGet2IndexOnlyScan.o \
              CXformInlineCTEConsumer.o \
              CXformInlineCTEConsumerUnderSelect.o \
              CXformInnerApply2InnerJoin.o \
              CXformInnerApply2InnerJoinNoCorrelations.o \
              CXformInnerApplyWithOuterKey2InnerJoin.o \
              CXformInnerJoin2HashJoin.o \
              CXformInnerJoin2NLJoin.o \
              CXformInsert2DML.o \
              CXformIntersect2Join.o \
              CXformIntersectAll2LeftSemiJoin.o \
              CXformJoin2IndexApply.o \
              CXformJoinAssociativity.o \
              CXformJoinCommutativity.o \
              CXformLeftAntiSemiApply2LeftAntiSemiJoin.o \
              CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations.o \
              CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotIn.o \
              CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations.o \
              CXformLeftAntiSemiJoin2CrossProduct.o \
              CXformLeftAntiSemiJoin2HashJoin.o \
              CXformLeftAntiSemiJoin2NLJoin.o \
              CXformLeftAntiSemiJoinNotIn2HashJoinNotIn.o \
              CXformLeftAntiSemiJoinNotIn2NLJoinNotIn.o \
              CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin.o \
              CXformLeftOuterApply2LeftOuterJoin.o \
              CXformLeftOuterApply2LeftOuterJoinNoCorrelations.o \
              CXformLeftOuterJoin2HashJoin.o \
              CXformLeftOuterJoin2NLJoin.o \
              CXformLeftSemiApply2LeftSemiJoin.o \
              CXformLeftSemiApply2LeftSemiJoinNoCorrelations.o \
              CXformLeftSemiApplyWithExternalCorrs2InnerJoin.o \
              CXformLeftSemiJoin2CrossProduct.o \
              CXformLeftSemiJoin2HashJoin.o \
              CXformLeftSemiJoin2InnerJoin.o \
              CXformLeftSemiJoin2InnerJoinUnderGb.o \
              CXformLeftSemiJoin2NLJoin.o \
              CXformMaxOneRow2Assert.o \
              CXformProject2Apply.o \
              CXformProject2ComputeScalar.o \
              CXformPushDownLeftOuterJoin.o \
              CXformPushGbBelowJoin.o \
              CXformPushGbDedupBelowJoin.o \
              CXformPushGbWithHavingBelowJoin.o \
              CXformRemoveSubqDistinct.o \
              CXformResult.o \
              CXformSelect2Apply.o \
              CXformSelect2BitmapBoolOp.o \
              CXformSelect2DynamicIndexGet.o \
              CXformSelect2Filter.o \
              CXformSelect2IndexGet.o \
              CXformSelect2PartialDynamicIndexGet.o \
              CXformSequenceProject2Apply.o \
              CXformSimplifyGbAgg.o \
              CXformSimplifyLeftOuterJoin.o \
              CXformSimplifySubquery.o \
              CXformSplitDQA.o \
              CXformSplitGbAgg.o \
              CXformSplitGbAggDedup.o \
              CXformSplitLimit.o \
              CXformSubqJoin2Apply.o \
              CXformSubqueryUnnest.o \
              CXformUnion2UnionAll.o \
              CXformUnnestTVF.o \
              CXformUpdate2DML.o \
              CXformUtils.o \
              CxformSelect2DynamicBitmapBoolOp.o

include $(top_srcdir)/src/backend/common.mk

