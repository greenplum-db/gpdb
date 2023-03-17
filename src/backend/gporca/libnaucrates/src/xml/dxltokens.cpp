//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		dxltokens.cpp
//
//	@doc:
//		DXL token constants in the ORCA CWStringConst and Xerces XMLCh format.
//		Constants are initialized in the CDXLTokens::Init function
//		invoked during initialization of the library (init.cpp),
//		and destroyed in CDXLTokens::Terminate when the library is unloaded
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/xml/dxltokens.h"

#include "naucrates/dxl/xml/CDXLMemoryManager.h"

using namespace gpdxl;

// static member initialization
CDXLTokens::SStrMapElem *CDXLTokens::m_pstrmap = nullptr;

CDXLTokens::SXMLStrMapElem *CDXLTokens::m_pxmlszmap = nullptr;

CMemoryPool *CDXLTokens::m_mp = nullptr;

CDXLMemoryManager *CDXLTokens::m_dxl_memory_manager = nullptr;


//---------------------------------------------------------------------------
//	@function:
//		CDXLTokens::Init
//
//	@doc:
//		Initialize the constants representing the DXL tokens.
//
//---------------------------------------------------------------------------
void
CDXLTokens::Init(CMemoryPool *mp)
{
	GPOS_ASSERT(nullptr != mp);
	GPOS_ASSERT(nullptr == m_dxl_memory_manager);
	GPOS_ASSERT(nullptr == m_mp);

	m_mp = mp;

	m_dxl_memory_manager = GPOS_NEW(m_mp) CDXLMemoryManager(m_mp);

	SWszMapElem rgStrMap[] = {
		{EdxltokenDXLMessage, GPOS_WSZ_LIT("DXLMessage")},
		{EdxltokenComment, GPOS_WSZ_LIT("Comment")},
		{EdxltokenPlan, GPOS_WSZ_LIT("Plan")},
		{EdxltokenPlanId, GPOS_WSZ_LIT("Id")},
		{EdxltokenPlanSpaceSize, GPOS_WSZ_LIT("SpaceSize")},
		{EdxltokenSamplePlans, GPOS_WSZ_LIT("SamplePlans")},
		{EdxltokenSamplePlan, GPOS_WSZ_LIT("SamplePlan")},
		{EdxltokenCostDistr, GPOS_WSZ_LIT("CostDistribution")},
		{EdxltokenRelativeCost, GPOS_WSZ_LIT("RelativeCost")},
		{EdxltokenX, GPOS_WSZ_LIT("X")},
		{EdxltokenY, GPOS_WSZ_LIT("Y")},

		{EdxltokenOptimizerConfig, GPOS_WSZ_LIT("OptimizerConfig")},
		{EdxltokenEnumeratorConfig, GPOS_WSZ_LIT("EnumeratorConfig")},
		{EdxltokenStatisticsConfig, GPOS_WSZ_LIT("StatisticsConfig")},
		{EdxltokenDampingFactorFilter, GPOS_WSZ_LIT("DampingFactorFilter")},
		{EdxltokenDampingFactorJoin, GPOS_WSZ_LIT("DampingFactorJoin")},
		{EdxltokenDampingFactorGroupBy, GPOS_WSZ_LIT("DampingFactorGroupBy")},
		{EdxltokenMaxStatsBuckets, GPOS_WSZ_LIT("MaxStatsBuckets")},
		{EdxltokenCTEConfig, GPOS_WSZ_LIT("CTEConfig")},
		{EdxltokenCTEInliningCutoff, GPOS_WSZ_LIT("CTEInliningCutoff")},
		{EdxltokenCostModelConfig, GPOS_WSZ_LIT("CostModelConfig")},
		{EdxltokenCostModelType, GPOS_WSZ_LIT("CostModelType")},
		{EdxltokenSegmentsForCosting, GPOS_WSZ_LIT("SegmentsForCosting")},
		{EdxltokenHint, GPOS_WSZ_LIT("Hint")},
		{EdxltokenJoinArityForAssociativityCommutativity,
		 GPOS_WSZ_LIT("JoinArityForAssociativityCommutativity")},
		{EdxltokenArrayExpansionThreshold,
		 GPOS_WSZ_LIT("ArrayExpansionThreshold")},
		{EdxltokenJoinOrderDPThreshold,
		 GPOS_WSZ_LIT("JoinOrderDynamicProgThreshold")},
		{EdxltokenBroadcastThreshold, GPOS_WSZ_LIT("BroadcastThreshold")},
		{EdxltokenEnforceConstraintsOnDML,
		 GPOS_WSZ_LIT("EnforceConstraintsOnDML")},
		{EdxltokenPushGroupByBelowSetopThreshold,
		 GPOS_WSZ_LIT("PushGroupByBelowSetopThreshold")},
		{EdxltokenXformBindThreshold, GPOS_WSZ_LIT("XformBindThreshold")},
		{EdxltokenSkewFactor, GPOS_WSZ_LIT("SkewFactor")},
		{EdxltokenWindowOids, GPOS_WSZ_LIT("WindowOids")},
		{EdxltokenOidRowNumber, GPOS_WSZ_LIT("RowNumber")},
		{EdxltokenOidRank, GPOS_WSZ_LIT("Rank")},

		{EdxltokenPlanSamples, GPOS_WSZ_LIT("PlanSamples")},

		{EdxltokenMetadata, GPOS_WSZ_LIT("Metadata")},
		{EdxltokenTraceFlags, GPOS_WSZ_LIT("TraceFlags")},
		{EdxltokenMDRequest, GPOS_WSZ_LIT("MDRequest")},

		{EdxltokenSysids, GPOS_WSZ_LIT("SystemIds")},
		{EdxltokenSysid, GPOS_WSZ_LIT("SystemId")},

		{EdxltokenThread, GPOS_WSZ_LIT("Thread")},

		{EdxltokenPhysical, GPOS_WSZ_LIT("OpPhysical")},

		{EdxltokenPhysicalTableScan, GPOS_WSZ_LIT("TableScan")},
		{EdxltokenPhysicalBitmapTableScan, GPOS_WSZ_LIT("BitmapTableScan")},
		{EdxltokenPhysicalDynamicBitmapTableScan,
		 GPOS_WSZ_LIT("DynamicBitmapTableScan")},
		{EdxltokenPhysicalForeignScan, GPOS_WSZ_LIT("ForeignScan")},
		{EdxltokenPhysicalIndexScan, GPOS_WSZ_LIT("IndexScan")},
		{EdxltokenPhysicalIndexOnlyScan, GPOS_WSZ_LIT("IndexOnlyScan")},
		{EdxltokenScalarBitmapIndexProbe, GPOS_WSZ_LIT("BitmapIndexProbe")},
		{EdxltokenPhysicalHashJoin, GPOS_WSZ_LIT("HashJoin")},
		{EdxltokenPhysicalNLJoin, GPOS_WSZ_LIT("NestedLoopJoin")},
		{EdxltokenPhysicalNLJoinIndex, GPOS_WSZ_LIT("IndexNestedLoopJoin")},
		{EdxltokenPhysicalMergeJoin, GPOS_WSZ_LIT("MergeJoin")},
		{EdxltokenPhysicalGatherMotion, GPOS_WSZ_LIT("GatherMotion")},
		{EdxltokenPhysicalBroadcastMotion, GPOS_WSZ_LIT("BroadcastMotion")},
		{EdxltokenPhysicalRedistributeMotion,
		 GPOS_WSZ_LIT("RedistributeMotion")},
		{EdxltokenPhysicalRoutedDistributeMotion,
		 GPOS_WSZ_LIT("RoutedDistributeMotion")},
		{EdxltokenPhysicalRandomMotion, GPOS_WSZ_LIT("RandomMotion")},
		{EdxltokenPhysicalLimit, GPOS_WSZ_LIT("Limit")},
		{EdxltokenPhysicalSort, GPOS_WSZ_LIT("Sort")},
		{EdxltokenPhysicalAggregate, GPOS_WSZ_LIT("Aggregate")},
		{EdxltokenPhysicalSubqueryScan, GPOS_WSZ_LIT("SubqueryScan")},
		{EdxltokenPhysicalResult, GPOS_WSZ_LIT("Result")},
		{EdxltokenPhysicalValuesScan, GPOS_WSZ_LIT("Values")},
		{EdxltokenPhysicalAppend, GPOS_WSZ_LIT("Append")},
		{EdxltokenPhysicalMaterialize, GPOS_WSZ_LIT("Materialize")},
		{EdxltokenPhysicalDynamicForeignScan,
		 GPOS_WSZ_LIT("DynamicForeignScan")},
		{EdxltokenPhysicalSequence, GPOS_WSZ_LIT("Sequence")},
		{EdxltokenPhysicalDynamicTableScan, GPOS_WSZ_LIT("DynamicTableScan")},
		{EdxltokenPhysicalDynamicIndexScan, GPOS_WSZ_LIT("DynamicIndexScan")},
		{EdxltokenPhysicalTVF, GPOS_WSZ_LIT("TableValuedFunction")},
		{EdxltokenPhysicalWindow, GPOS_WSZ_LIT("Window")},
		{EdxltokenPhysicalDMLInsert, GPOS_WSZ_LIT("DMLInsert")},
		{EdxltokenPhysicalDMLDelete, GPOS_WSZ_LIT("DMLDelete")},
		{EdxltokenPhysicalDMLUpdate, GPOS_WSZ_LIT("DMLUpdate")},
		{EdxltokenDirectDispatchInfo, GPOS_WSZ_LIT("DirectDispatchInfo")},
		{EdxltokenDirectDispatchIsRaw, GPOS_WSZ_LIT("IsRaw")},
		{EdxltokenDirectDispatchKeyValue, GPOS_WSZ_LIT("KeyValue")},

		{EdxltokenPhysicalPartitionSelector, GPOS_WSZ_LIT("PartitionSelector")},
		{EdxltokenPhysicalPartitionSelectorId, GPOS_WSZ_LIT("SelectorId")},
		{EdxltokenPhysicalPartitionSelectorScanId, GPOS_WSZ_LIT("ScanId")},
		{EdxltokenPhysicalSplit, GPOS_WSZ_LIT("Split")},
		{EdxltokenPhysicalAssert, GPOS_WSZ_LIT("Assert")},
		{EdxltokenPhysicalCTEProducer, GPOS_WSZ_LIT("CTEProducer")},
		{EdxltokenPhysicalCTEConsumer, GPOS_WSZ_LIT("CTEConsumer")},

		{EdxltokenErrorCode, GPOS_WSZ_LIT("ErrorCode")},
		{EdxltokenErrorMessage, GPOS_WSZ_LIT("ErrorMessage")},

		{EdxltokenOnCommitAction, GPOS_WSZ_LIT("OnCommitAction")},
		{EdxltokenOnCommitNOOP, GPOS_WSZ_LIT("NOOP")},
		{EdxltokenOnCommitPreserve, GPOS_WSZ_LIT("PreserveRows")},
		{EdxltokenOnCommitDelete, GPOS_WSZ_LIT("DeleteRows")},
		{EdxltokenOnCommitDrop, GPOS_WSZ_LIT("Drop")},

		{EdxltokenDuplicateSensitive, GPOS_WSZ_LIT("DuplicateSensitive")},

		{EdxltokenPartIndexId, GPOS_WSZ_LIT("PartIndexId")},
		{EdxltokenPartIndexIdPrintable, GPOS_WSZ_LIT("PrintablePartIndexId")},
		{EdxltokenSegmentIdCol, GPOS_WSZ_LIT("SegmentIdCol")},

		{EdxltokenScalar, GPOS_WSZ_LIT("Scalar")},
		{EdxltokenScalarExpr, GPOS_WSZ_LIT("ScalarExpr")},

		{EdxltokenScalarProjList, GPOS_WSZ_LIT("ProjList")},
		{EdxltokenScalarFilter, GPOS_WSZ_LIT("Filter")},
		{EdxltokenScalarAggref, GPOS_WSZ_LIT("AggFunc")},
		{EdxltokenScalarWindowref, GPOS_WSZ_LIT("WindowFunc")},
		{EdxltokenScalarArrayComp, GPOS_WSZ_LIT("ArrayComp")},
		{EdxltokenScalarBoolTestIsTrue, GPOS_WSZ_LIT("IsTrue")},
		{EdxltokenScalarBoolTestIsNotTrue, GPOS_WSZ_LIT("IsNotTrue")},
		{EdxltokenScalarBoolTestIsFalse, GPOS_WSZ_LIT("IsFalse")},
		{EdxltokenScalarBoolTestIsNotFalse, GPOS_WSZ_LIT("IsNotFalse")},
		{EdxltokenScalarBoolTestIsUnknown, GPOS_WSZ_LIT("IsUnknown")},
		{EdxltokenScalarBoolTestIsNotUnknown, GPOS_WSZ_LIT("IsNotUnknown")},
		{EdxltokenScalarBoolAnd, GPOS_WSZ_LIT("And")},
		{EdxltokenScalarBoolOr, GPOS_WSZ_LIT("Or")},
		{EdxltokenScalarBoolNot, GPOS_WSZ_LIT("Not")},
		{EdxltokenScalarMin, GPOS_WSZ_LIT("Minimum")},
		{EdxltokenScalarMax, GPOS_WSZ_LIT("Maximum")},
		{EdxltokenScalarCoalesce, GPOS_WSZ_LIT("Coalesce")},
		{EdxltokenScalarComp, GPOS_WSZ_LIT("Comparison")},
		{EdxltokenScalarConstValue, GPOS_WSZ_LIT("ConstValue")},
		{EdxltokenScalarDistinctComp, GPOS_WSZ_LIT("IsDistinctFrom")},
		{EdxltokenScalarFuncExpr, GPOS_WSZ_LIT("FuncExpr")},
		{EdxltokenScalarIsNull, GPOS_WSZ_LIT("IsNull")},
		{EdxltokenScalarIsNotNull, GPOS_WSZ_LIT("IsNotNull")},
		{EdxltokenScalarNullIf, GPOS_WSZ_LIT("NullIf")},
		{EdxltokenScalarHashCondList, GPOS_WSZ_LIT("HashCondList")},
		{EdxltokenScalarMergeCondList, GPOS_WSZ_LIT("MergeCondList")},
		{EdxltokenScalarHashExprList, GPOS_WSZ_LIT("HashExprList")},
		{EdxltokenScalarHashExpr, GPOS_WSZ_LIT("HashExpr")},
		{EdxltokenScalarIdent, GPOS_WSZ_LIT("Ident")},
		{EdxltokenScalarIfStmt, GPOS_WSZ_LIT("If")},
		{EdxltokenScalarSwitch, GPOS_WSZ_LIT("Switch")},
		{EdxltokenScalarSwitchCase, GPOS_WSZ_LIT("SwitchCase")},
		{EdxltokenScalarCaseTest, GPOS_WSZ_LIT("CaseTest")},
		{EdxltokenScalarSubPlan, GPOS_WSZ_LIT("SubPlan")},
		{EdxltokenScalarJoinFilter, GPOS_WSZ_LIT("JoinFilter")},
		{EdxltokenScalarRecheckCondFilter, GPOS_WSZ_LIT("RecheckCond")},
		{EdxltokenScalarLimitCount, GPOS_WSZ_LIT("LimitCount")},
		{EdxltokenScalarLimitOffset, GPOS_WSZ_LIT("LimitOffset")},
		{EdxltokenScalarOneTimeFilter, GPOS_WSZ_LIT("OneTimeFilter")},
		{EdxltokenScalarOpExpr, GPOS_WSZ_LIT("OpExpr")},
		{EdxltokenScalarProjElem, GPOS_WSZ_LIT("ProjElem")},
		{EdxltokenScalarCast, GPOS_WSZ_LIT("Cast")},
		{EdxltokenScalarCoerceToDomain, GPOS_WSZ_LIT("CoerceToDomain")},
		{EdxltokenScalarCoerceViaIO, GPOS_WSZ_LIT("CoerceViaIO")},
		{EdxltokenScalarArrayCoerceExpr, GPOS_WSZ_LIT("ArrayCoerceExpr")},
		{EdxltokenScalarSortCol, GPOS_WSZ_LIT("SortingColumn")},
		{EdxltokenScalarSortColList, GPOS_WSZ_LIT("SortingColumnList")},
		{EdxltokenScalarGroupingColList, GPOS_WSZ_LIT("GroupingColumns")},
		{EdxltokenScalarSortGroupClause, GPOS_WSZ_LIT("SortGroupClause")},

		{EdxltokenScalarBitmapAnd, GPOS_WSZ_LIT("BitmapAnd")},
		{EdxltokenScalarBitmapOr, GPOS_WSZ_LIT("BitmapOr")},

		{EdxltokenScalarArray, GPOS_WSZ_LIT("Array")},
		{EdxltokenScalarArrayRef, GPOS_WSZ_LIT("ArrayRef")},
		{EdxltokenScalarArrayRefIndexList, GPOS_WSZ_LIT("ArrayIndexList")},
		{EdxltokenScalarArrayRefIndexListBound, GPOS_WSZ_LIT("Bound")},
		{EdxltokenScalarArrayRefIndexListLower, GPOS_WSZ_LIT("Lower")},
		{EdxltokenScalarArrayRefIndexListUpper, GPOS_WSZ_LIT("Upper")},
		{EdxltokenScalarArrayRefExpr, GPOS_WSZ_LIT("RefExpr")},
		{EdxltokenScalarArrayRefAssignExpr, GPOS_WSZ_LIT("AssignExpr")},

		{EdxltokenScalarAssertConstraintList,
		 GPOS_WSZ_LIT("AssertConstraintList")},
		{EdxltokenScalarAssertConstraint, GPOS_WSZ_LIT("AssertConstraint")},

		{EdxltokenScalarSubquery, GPOS_WSZ_LIT("ScalarSubquery")},
		{EdxltokenScalarSubqueryAny, GPOS_WSZ_LIT("SubqueryAny")},
		{EdxltokenScalarSubqueryAll, GPOS_WSZ_LIT("SubqueryAll")},
		{EdxltokenScalarSubqueryExists, GPOS_WSZ_LIT("SubqueryExists")},
		{EdxltokenScalarSubqueryNotExists, GPOS_WSZ_LIT("SubqueryNotExists")},

		{EdxltokenScalarDMLAction, GPOS_WSZ_LIT("DMLAction")},
		{EdxltokenScalarOpList, GPOS_WSZ_LIT("ScalarOpList")},

		{EdxltokenScalarSubPlanType, GPOS_WSZ_LIT("SubPlanType")},
		{EdxltokenScalarSubPlanTypeScalar, GPOS_WSZ_LIT("ScalarSubPlan")},
		{EdxltokenScalarSubPlanTypeExists, GPOS_WSZ_LIT("ExistsSubPlan")},
		{EdxltokenScalarSubPlanTypeNotExists, GPOS_WSZ_LIT("NotExistsSubPlan")},
		{EdxltokenScalarSubPlanTypeAny, GPOS_WSZ_LIT("AnySubPlan")},
		{EdxltokenScalarSubPlanTypeAll, GPOS_WSZ_LIT("AllSubPlan")},

		{EdxltokenPartLevelEqFilterList, GPOS_WSZ_LIT("PartEqFilters")},
		{EdxltokenPartLevelFilterList, GPOS_WSZ_LIT("PartFilters")},
		{EdxltokenPartLevel, GPOS_WSZ_LIT("Level")},
		{EdxltokenScalarPartDefault, GPOS_WSZ_LIT("DefaultPart")},
		{EdxltokenScalarResidualFilter, GPOS_WSZ_LIT("ResidualFilter")},
		{EdxltokenScalarPartFilterExpr, GPOS_WSZ_LIT("PartFilterExpr")},

		{EdxltokenScalarSubPlanParamList, GPOS_WSZ_LIT("ParamList")},
		{EdxltokenScalarSubPlanParam, GPOS_WSZ_LIT("Param")},
		{EdxltokenScalarSubPlanTestExpr, GPOS_WSZ_LIT("TestExpr")},
		{EdxltokenScalarValuesList, GPOS_WSZ_LIT("ValuesList")},

		{EdxltokenValue, GPOS_WSZ_LIT("Value")},
		{EdxltokenTypeId, GPOS_WSZ_LIT("TypeMdid")},
		{EdxltokenTypeIds, GPOS_WSZ_LIT("TypeMdids")},

		{EdxltokenConstTuple, GPOS_WSZ_LIT("ConstTuple")},
		{EdxltokenDatum, GPOS_WSZ_LIT("Datum")},

		{EdxltokenTypeMod, GPOS_WSZ_LIT("TypeModifier")},
		{EdxltokenCoercionForm, GPOS_WSZ_LIT("CoercionForm")},
		{EdxltokenLocation, GPOS_WSZ_LIT("Location")},
		{EdxltokenElementFunc, GPOS_WSZ_LIT("ElementFunc")},
		{EdxltokenIsExplicit, GPOS_WSZ_LIT("IsExplicit")},

		{EdxltokenJoinType, GPOS_WSZ_LIT("JoinType")},
		{EdxltokenJoinInner, GPOS_WSZ_LIT("Inner")},
		{EdxltokenJoinLeft, GPOS_WSZ_LIT("Left")},
		{EdxltokenJoinFull, GPOS_WSZ_LIT("Full")},
		{EdxltokenJoinRight, GPOS_WSZ_LIT("Right")},
		{EdxltokenJoinIn, GPOS_WSZ_LIT("In")},
		{EdxltokenJoinLeftAntiSemiJoin, GPOS_WSZ_LIT("LeftAntiSemiJoin")},
		{EdxltokenJoinLeftAntiSemiJoinNotIn,
		 GPOS_WSZ_LIT("LeftAntiSemiJoinNotIn")},

		{EdxltokenMergeJoinUniqueOuter, GPOS_WSZ_LIT("UniqueOuter")},

		{EdxltokenWindowLeadingBoundary, GPOS_WSZ_LIT("LeadingBoundary")},
		{EdxltokenWindowTrailingBoundary, GPOS_WSZ_LIT("TrailingBoundary")},
		{EdxltokenWindowBoundaryUnboundedPreceding,
		 GPOS_WSZ_LIT("UnboundedPreceding")},
		{EdxltokenWindowBoundaryBoundedPreceding,
		 GPOS_WSZ_LIT("BoundedPreceding")},
		{EdxltokenWindowBoundaryCurrentRow, GPOS_WSZ_LIT("CurrentRow")},
		{EdxltokenWindowBoundaryUnboundedFollowing,
		 GPOS_WSZ_LIT("UnboundedFollowing")},
		{EdxltokenWindowBoundaryBoundedFollowing,
		 GPOS_WSZ_LIT("BoundedFollowing")},
		{EdxltokenWindowBoundaryDelayedBoundedPreceding,
		 GPOS_WSZ_LIT("DelayedBoundedPreceding")},
		{EdxltokenWindowBoundaryDelayedBoundedFollowing,
		 GPOS_WSZ_LIT("DelayedBoundedFollowing")},

		{EdxltokenWindowFrameSpec, GPOS_WSZ_LIT("FrameSpec")},
		{EdxltokenScalarWindowFrameLeadingEdge, GPOS_WSZ_LIT("LeadingEdge")},
		{EdxltokenScalarWindowFrameTrailingEdge, GPOS_WSZ_LIT("TrailingEdge")},
		{EdxltokenWindowFSRow, GPOS_WSZ_LIT("Row")},
		{EdxltokenWindowFSRange, GPOS_WSZ_LIT("Range")},
		{EdxltokenWindowFSGroups, GPOS_WSZ_LIT("Groups")},

		{EdxltokenWindowStartInRangeOid, GPOS_WSZ_LIT("StartInRange")},
		{EdxltokenWindowEndInRangeOid, GPOS_WSZ_LIT("EndInRange")},
		{EdxltokenWindowInRangeColl, GPOS_WSZ_LIT("InRangeColl")},
		{EdxltokenWindowInRangeAsc, GPOS_WSZ_LIT("InRangeAsc")},
		{EdxltokenWindowInRangeNullsFirst, GPOS_WSZ_LIT("InRangeNullsFirst")},

		{EdxltokenWindowExclusionStrategy, GPOS_WSZ_LIT("ExclusionStrategy")},
		{EdxltokenWindowESNone, GPOS_WSZ_LIT("None")},
		{EdxltokenWindowESNulls, GPOS_WSZ_LIT("Nulls")},
		{EdxltokenWindowESCurrentRow, GPOS_WSZ_LIT("CurrentRow")},
		{EdxltokenWindowESGroup, GPOS_WSZ_LIT("Group")},
		{EdxltokenWindowESTies, GPOS_WSZ_LIT("Ties")},

		{EdxltokenWindowFrame, GPOS_WSZ_LIT("WindowFrame")},
		{EdxltokenWindowKeyList, GPOS_WSZ_LIT("WindowKeyList")},
		{EdxltokenWindowKey, GPOS_WSZ_LIT("WindowKey")},

		{EdxltokenWindowSpecList, GPOS_WSZ_LIT("WindowSpecList")},
		{EdxltokenWindowSpec, GPOS_WSZ_LIT("WindowSpec")},

		{EdxltokenWindowrefOid, GPOS_WSZ_LIT("Mdid")},
		{EdxltokenWindowrefDistinct, GPOS_WSZ_LIT("Distinct")},
		{EdxltokenWindowrefStarArg, GPOS_WSZ_LIT("WindowStarArg")},
		{EdxltokenWindowrefSimpleAgg, GPOS_WSZ_LIT("WindowSimpleAgg")},
		{EdxltokenWindowrefStrategy, GPOS_WSZ_LIT("WindowStrategy")},
		{EdxltokenWindowrefStageImmediate, GPOS_WSZ_LIT("Immediate")},
		{EdxltokenWindowrefStagePreliminary, GPOS_WSZ_LIT("Preliminary")},
		{EdxltokenWindowrefStageRowKey, GPOS_WSZ_LIT("RowKey")},
		{EdxltokenWindowrefWinSpecPos, GPOS_WSZ_LIT("WinSpecPos")},

		{EdxltokenAggStrategy, GPOS_WSZ_LIT("AggregationStrategy")},
		{EdxltokenAggStrategyPlain, GPOS_WSZ_LIT("Plain")},
		{EdxltokenAggStrategySorted, GPOS_WSZ_LIT("Sorted")},
		{EdxltokenAggStrategyHashed, GPOS_WSZ_LIT("Hashed")},

		{EdxltokenAggStreamSafe, GPOS_WSZ_LIT("StreamSafe")},

		{EdxltokenAggrefOid, GPOS_WSZ_LIT("AggMdid")},
		{EdxltokenAggrefDistinct, GPOS_WSZ_LIT("AggDistinct")},
		{EdxltokenAggrefArgTypes, GPOS_WSZ_LIT("AggArgTypes")},
		{EdxltokenAggrefKind, GPOS_WSZ_LIT("AggKind")},
		{EdxltokenAggrefStage, GPOS_WSZ_LIT("AggStage")},
		{EdxltokenAggrefLookups, GPOS_WSZ_LIT("AggLookups")},
		{EdxltokenAggrefStageNormal, GPOS_WSZ_LIT("Normal")},
		{EdxltokenAggrefStagePartial, GPOS_WSZ_LIT("Partial")},
		{EdxltokenAggrefStageIntermediate, GPOS_WSZ_LIT("Intermediate")},
		{EdxltokenAggrefStageFinal, GPOS_WSZ_LIT("Final")},
		{EdxltokenAggrefKindNormal, GPOS_WSZ_LIT("n")},
		{EdxltokenAggrefKindOrderedSet, GPOS_WSZ_LIT("o")},
		{EdxltokenAggrefKindHypothetical, GPOS_WSZ_LIT("h")},

		{EdxltokenArrayType, GPOS_WSZ_LIT("ArrayType")},
		{EdxltokenArrayElementType, GPOS_WSZ_LIT("ElementType")},
		{EdxltokenArrayMultiDim, GPOS_WSZ_LIT("MultiDimensional")},

		{EdxltokenTableDescr, GPOS_WSZ_LIT("TableDescriptor")},
		{EdxltokenProperties, GPOS_WSZ_LIT("Properties")},
		{EdxltokenOutputCols, GPOS_WSZ_LIT("OutputColumns")},
		{EdxltokenCost, GPOS_WSZ_LIT("Cost")},
		{EdxltokenStartupCost, GPOS_WSZ_LIT("StartupCost")},
		{EdxltokenTotalCost, GPOS_WSZ_LIT("TotalCost")},
		{EdxltokenRows, GPOS_WSZ_LIT("Rows")},
		{EdxltokenWidth, GPOS_WSZ_LIT("Width")},
		{EdxltokenRelPages, GPOS_WSZ_LIT("RelPages")},
		{EdxltokenRelAllVisible, GPOS_WSZ_LIT("RelAllVisible")},
		{EdxltokenTableName, GPOS_WSZ_LIT("TableName")},
		{EdxltokenDerivedTableName, GPOS_WSZ_LIT("DerivedTableName")},
		{EdxltokenExecuteAsUser, GPOS_WSZ_LIT("ExecuteAsUser")},

		{EdxltokenCTASOptions, GPOS_WSZ_LIT("CTASOptions")},
		{EdxltokenCTASOption, GPOS_WSZ_LIT("CTASOption")},

		{EdxltokenColDescr, GPOS_WSZ_LIT("Column")},
		{EdxltokenColRef, GPOS_WSZ_LIT("ColRef")},

		{EdxltokenColumns, GPOS_WSZ_LIT("Columns")},
		{EdxltokenColumn, GPOS_WSZ_LIT("Column")},
		{EdxltokenColName, GPOS_WSZ_LIT("ColName")},
		{EdxltokenTagColType, GPOS_WSZ_LIT("ColType")},
		{EdxltokenColId, GPOS_WSZ_LIT("ColId")},
		{EdxltokenAttno, GPOS_WSZ_LIT("Attno")},
		{EdxltokenColDropped, GPOS_WSZ_LIT("IsDropped")},
		{EdxltokenColWidth, GPOS_WSZ_LIT("ColWidth")},
		{EdxltokenColNullFreq, GPOS_WSZ_LIT("NullFreq")},
		{EdxltokenColNdvRemain, GPOS_WSZ_LIT("NdvRemain")},
		{EdxltokenColFreqRemain, GPOS_WSZ_LIT("FreqRemain")},
		{EdxltokenColStatsMissing, GPOS_WSZ_LIT("ColStatsMissing")},

		{EdxltokenCtidColName, GPOS_WSZ_LIT("ctid")},
		{EdxltokenOidColName, GPOS_WSZ_LIT("oid")},
		{EdxltokenXminColName, GPOS_WSZ_LIT("xmin")},
		{EdxltokenCminColName, GPOS_WSZ_LIT("cmin")},
		{EdxltokenXmaxColName, GPOS_WSZ_LIT("xmax")},
		{EdxltokenCmaxColName, GPOS_WSZ_LIT("cmax")},
		{EdxltokenTableOidColName, GPOS_WSZ_LIT("tableoid")},
		{EdxltokenGpSegmentIdColName, GPOS_WSZ_LIT("gp_segment_id")},

		{EdxltokenActionColId, GPOS_WSZ_LIT("ActionCol")},
		{EdxltokenCtidColId, GPOS_WSZ_LIT("CtidCol")},
		{EdxltokenGpSegmentIdColId, GPOS_WSZ_LIT("SegmentIdCol")},
		{EdxltokenTupleOidColId, GPOS_WSZ_LIT("TupleOidCol")},
		{EdxltokenSplitUpdate, GPOS_WSZ_LIT("IsSplitUpdate")},

		{EdxltokenInputSegments, GPOS_WSZ_LIT("InputSegments")},
		{EdxltokenOutputSegments, GPOS_WSZ_LIT("OutputSegments")},
		{EdxltokenSegment, GPOS_WSZ_LIT("Segment")},
		{EdxltokenSegId, GPOS_WSZ_LIT("SegmentId")},

		{EdxltokenGroupingCols, GPOS_WSZ_LIT("GroupingColumns")},
		{EdxltokenGroupingCol, GPOS_WSZ_LIT("GroupingColumn")},

		{EdxltokenParamKind, GPOS_WSZ_LIT("ParamType")},

		{EdxltokenAppendIsTarget, GPOS_WSZ_LIT("IsTarget")},
		{EdxltokenAppendIsZapped, GPOS_WSZ_LIT("IsZapped")},
		{EdxltokenSelectorIds, GPOS_WSZ_LIT("SelectorIds")},

		{EdxltokenOpNo, GPOS_WSZ_LIT("OperatorMdid")},
		{EdxltokenOpName, GPOS_WSZ_LIT("OperatorName")},
		{EdxltokenOpType, GPOS_WSZ_LIT("OperatorType")},
		{EdxltokenOpTypeAny, GPOS_WSZ_LIT("Any")},
		{EdxltokenOpTypeAll, GPOS_WSZ_LIT("All")},

		{EdxltokenTypeName, GPOS_WSZ_LIT("TypeName")},
		{EdxltokenUnknown, GPOS_WSZ_LIT("Unknown")},

		{EdxltokenFuncId, GPOS_WSZ_LIT("FuncId")},
		{EdxltokenFuncRetSet, GPOS_WSZ_LIT("FuncRetSet")},

		{EdxltokenAlias, GPOS_WSZ_LIT("Alias")},

		{EdxltokenSortOpId, GPOS_WSZ_LIT("SortOperatorMdid")},
		{EdxltokenSortOpName, GPOS_WSZ_LIT("SortOperatorName")},
		{EdxltokenSortDiscardDuplicates, GPOS_WSZ_LIT("SortDiscardDuplicates")},
		{EdxltokenSortNullsFirst, GPOS_WSZ_LIT("SortNullsFirst")},

		{EdxltokenMaterializeEager, GPOS_WSZ_LIT("Eager")},

		{EdxltokenSpoolId, GPOS_WSZ_LIT("SpoolId")},
		{EdxltokenSpoolType, GPOS_WSZ_LIT("SpoolType")},
		{EdxltokenSpoolMaterialize, GPOS_WSZ_LIT("Materialize")},
		{EdxltokenSpoolSort, GPOS_WSZ_LIT("Sort")},
		{EdxltokenSpoolMultiSlice, GPOS_WSZ_LIT("MultiSliceSpool")},
		{EdxltokenExecutorSliceId, GPOS_WSZ_LIT("ExecutorSliceId")},
		{EdxltokenConsumerSliceCount, GPOS_WSZ_LIT("NumberOfConsumersSlices")},

		{EdxltokenComparisonOp, GPOS_WSZ_LIT("ComparisonOperator")},

		{EdxltokenXMLDocHeader,
		 GPOS_WSZ_LIT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")},
		{EdxltokenNamespaceAttr, GPOS_WSZ_LIT("xmlns")},
		{EdxltokenNamespacePrefix, GPOS_WSZ_LIT("dxl")},
		{EdxltokenNamespaceURI,
		 GPOS_WSZ_LIT("http://greenplum.com/dxl/2010/12/")},

		{EdxltokenBracketOpenTag, GPOS_WSZ_LIT("<")},
		{EdxltokenBracketCloseTag, GPOS_WSZ_LIT(">")},
		{EdxltokenBracketOpenEndTag, GPOS_WSZ_LIT("</")},
		{EdxltokenBracketCloseSingletonTag, GPOS_WSZ_LIT("/>")},
		{EdxltokenColon, GPOS_WSZ_LIT(":")},
		{EdxltokenSemicolon, GPOS_WSZ_LIT(";")},
		{EdxltokenComma, GPOS_WSZ_LIT(",")},
		{EdxltokenDot, GPOS_WSZ_LIT(".")},
		{EdxltokenDotSemicolon, GPOS_WSZ_LIT(".;")},
		{EdxltokenSpace, GPOS_WSZ_LIT(" ")},
		{EdxltokenQuote, GPOS_WSZ_LIT("\"")},
		{EdxltokenEq, GPOS_WSZ_LIT("=")},
		{EdxltokenIndent, GPOS_WSZ_LIT("  ")},

		{EdxltokenTrue, GPOS_WSZ_LIT("true")},
		{EdxltokenFalse, GPOS_WSZ_LIT("false")},

		{EdxltokenRelation, GPOS_WSZ_LIT("Relation")},
		{EdxltokenRelationCTAS, GPOS_WSZ_LIT("CTASRelation")},
		{EdxltokenName, GPOS_WSZ_LIT("Name")},
		{EdxltokenSchema, GPOS_WSZ_LIT("Schema")},
		{EdxltokenTablespace, GPOS_WSZ_LIT("Tablespace")},
		{EdxltokenOid, GPOS_WSZ_LIT("Oid")},
		{EdxltokenKind, GPOS_WSZ_LIT("Kind")},
		{EdxltokenVersion, GPOS_WSZ_LIT("Version")},
		{EdxltokenMdid, GPOS_WSZ_LIT("Mdid")},
		{EdxltokenLockMode, GPOS_WSZ_LIT("LockMode")},
		{EdxltokenMDTypeRequest, GPOS_WSZ_LIT("TypeRequest")},
		{EdxltokenTypeInfo, GPOS_WSZ_LIT("TypeInfo")},
		{EdxltokenFuncInfo, GPOS_WSZ_LIT("FuncInfo")},
		{EdxltokenRelationMdid, GPOS_WSZ_LIT("RelationMdid")},
		{EdxltokenRelationStats, GPOS_WSZ_LIT("RelationStatistics")},
		{EdxltokenColumnStats, GPOS_WSZ_LIT("ColumnStatistics")},
		{EdxltokenColumnStatsBucket, GPOS_WSZ_LIT("StatsBucket")},
		{EdxltokenRelationExtendedStats,
		 GPOS_WSZ_LIT("RelationExtendedStatistics")},
		{EdxltokenExtendedStatsInfo, GPOS_WSZ_LIT("ExtendedStatisticsInfo")},
		{EdxltokenExtendedStats, GPOS_WSZ_LIT("ExtendedStatistics")},
		{EdxltokenMVDependencyList, GPOS_WSZ_LIT("MVDependencyList")},
		{EdxltokenMVDependency, GPOS_WSZ_LIT("MVDependency")},
		{EdxltokenMVNDistinctList, GPOS_WSZ_LIT("MVNDistinctList")},
		{EdxltokenMVNDistinct, GPOS_WSZ_LIT("MVNDistinct")},
		{EdxltokenDegree, GPOS_WSZ_LIT("Degree")},
		{EdxltokenFrom, GPOS_WSZ_LIT("From")},
		{EdxltokenTo, GPOS_WSZ_LIT("To")},
		{EdxltokenEmptyRelation, GPOS_WSZ_LIT("EmptyRelation")},

		{EdxltokenIsNull, GPOS_WSZ_LIT("IsNull")},
		{EdxltokenLintValue, GPOS_WSZ_LIT("LintValue")},
		{EdxltokenDoubleValue, GPOS_WSZ_LIT("DoubleValue")},
		{EdxltokenAssignedQueryIdForTargetRel,
		 GPOS_WSZ_LIT("AssignedQueryIdForTargetRel")},

		{EdxltokenRelTemporary, GPOS_WSZ_LIT("IsTemporary")},

		{EdxltokenEntireRow, GPOS_WSZ_LIT("*")},

		{EdxltokenRelStorageType, GPOS_WSZ_LIT("StorageType")},
		{EdxltokenRelStorageHeap, GPOS_WSZ_LIT("Heap")},
		{EdxltokenRelStorageAppendOnlyCols,
		 GPOS_WSZ_LIT("AppendOnly, Column-oriented")},
		{EdxltokenRelStorageAppendOnlyRows,
		 GPOS_WSZ_LIT("AppendOnly, Row-oriented")},
		{EdxltokenRelStorageMixedPartitioned, GPOS_WSZ_LIT("MixedPartitioned")},
		{EdxltokenRelStorageForeign, GPOS_WSZ_LIT("Foreign")},
		{EdxltokenRelStorageCompositeType, GPOS_WSZ_LIT("Composite")},

		{EdxltokenRelDistrPolicy, GPOS_WSZ_LIT("DistributionPolicy")},
		{EdxltokenRelDistrMasterOnly, GPOS_WSZ_LIT("MasterOnly")},
		{EdxltokenRelDistrHash, GPOS_WSZ_LIT("Hash")},
		{EdxltokenRelDistrRandom, GPOS_WSZ_LIT("Random")},
		{EdxltokenRelDistrReplicated, GPOS_WSZ_LIT("Replicated")},
		{EdxltokenConvertHashToRandom, GPOS_WSZ_LIT("ConvertHashToRandom")},

		{EdxltokenRelDistrOpfamilies, GPOS_WSZ_LIT("DistrOpfamilies")},
		{EdxltokenRelDistrOpfamily, GPOS_WSZ_LIT("DistrOpfamily")},

		{EdxltokenRelDistrOpclasses, GPOS_WSZ_LIT("DistrOpclasses")},
		{EdxltokenRelDistrOpclass, GPOS_WSZ_LIT("DistrOpclass")},

		{EdxltokenRelForeignServer, GPOS_WSZ_LIT("ForeignServer")},
		{EdxltokenForeignServerOid, GPOS_WSZ_LIT("ForeignServerOid")},
		{EdxltokenKeys, GPOS_WSZ_LIT("Keys")},
		{EdxltokenDistrColumns, GPOS_WSZ_LIT("DistributionColumns")},

		{EdxltokenPartKeys, GPOS_WSZ_LIT("PartitionColumns")},
		{EdxltokenPartTypes, GPOS_WSZ_LIT("PartitionTypes")},

		{EdxltokenTypeInt4, GPOS_WSZ_LIT("Int4")},
		{EdxltokenTypeBool, GPOS_WSZ_LIT("Bool")},

		{EdxltokenMetadataIdList, GPOS_WSZ_LIT("MetadataIdList")},
		{EdxltokenMetadataColumns, GPOS_WSZ_LIT("MetadataColumns")},
		{EdxltokenMetadataColumn, GPOS_WSZ_LIT("MetadataColumn")},

		{EdxltokenColumnNullable, GPOS_WSZ_LIT("Nullable")},
		{EdxltokenColumnDefaultValue, GPOS_WSZ_LIT("DefaultValue")},

		{EdxltokenIndex, GPOS_WSZ_LIT("Index")},

		{EdxltokenIndexInfoList, GPOS_WSZ_LIT("IndexInfoList")},
		{EdxltokenIndexInfo, GPOS_WSZ_LIT("IndexInfo")},

		{EdxltokenIndexKeyCols, GPOS_WSZ_LIT("KeyColumns")},
		{EdxltokenIndexIncludedCols, GPOS_WSZ_LIT("IncludedColumns")},
		{EdxltokenIndexClustered, GPOS_WSZ_LIT("IsClustered")},
		{EdxltokenIndexPartial, GPOS_WSZ_LIT("IsPartial")},
		{EdxltokenIndexType, GPOS_WSZ_LIT("IndexType")},
		{EdxltokenIndexTypeBtree, GPOS_WSZ_LIT("B-tree")},
		{EdxltokenIndexTypeBitmap, GPOS_WSZ_LIT("Bitmap")},
		{EdxltokenIndexTypeGist, GPOS_WSZ_LIT("Gist")},
		{EdxltokenIndexTypeGin, GPOS_WSZ_LIT("Gin")},
		{EdxltokenIndexTypeBrin, GPOS_WSZ_LIT("Brin")},
		{EdxltokenIndexTypeHash, GPOS_WSZ_LIT("Hash")},
		{EdxltokenIndexItemType, GPOS_WSZ_LIT("IndexItemType")},

		{EdxltokenOpfamily, GPOS_WSZ_LIT("Opfamily")},
		{EdxltokenOpfamilies, GPOS_WSZ_LIT("Opfamilies")},

		{EdxltokenPartitions, GPOS_WSZ_LIT("Partitions")},
		{EdxltokenPartition, GPOS_WSZ_LIT("Partition")},

		{EdxltokenConstraints, GPOS_WSZ_LIT("Constraints")},
		{EdxltokenConstraint, GPOS_WSZ_LIT("Constraint")},

		{EdxltokenCheckConstraints, GPOS_WSZ_LIT("CheckConstraints")},
		{EdxltokenCheckConstraint, GPOS_WSZ_LIT("CheckConstraint")},

		{EdxltokenPartConstraint, GPOS_WSZ_LIT("PartConstraint")},
		{EdxltokenDefaultPartition, GPOS_WSZ_LIT("DefaultPartition")},
		{EdxltokenPartConstraintUnbounded, GPOS_WSZ_LIT("Unbounded")},

		{EdxltokenMDType, GPOS_WSZ_LIT("Type")},
		{EdxltokenMDTypeRedistributable, GPOS_WSZ_LIT("IsRedistributable")},
		{EdxltokenMDTypeHashable, GPOS_WSZ_LIT("IsHashable")},
		{EdxltokenMDTypeMergeJoinable, GPOS_WSZ_LIT("IsMergeJoinable")},
		{EdxltokenMDTypeComposite, GPOS_WSZ_LIT("IsComposite")},
		{EdxltokenMDTypeIsTextRelated, GPOS_WSZ_LIT("IsTextRelated")},
		{EdxltokenMDTypeFixedLength, GPOS_WSZ_LIT("IsFixedLength")},
		{EdxltokenMDTypeLength, GPOS_WSZ_LIT("Length")},
		{EdxltokenMDTypeByValue, GPOS_WSZ_LIT("PassByValue")},
		{EdxltokenMDTypeDistrOpfamily, GPOS_WSZ_LIT("DistrOpfamily")},
		{EdxltokenMDTypeLegacyDistrOpfamily,
		 GPOS_WSZ_LIT("LegacyDistrOpfamily")},
		{EdxltokenMDTypeEqOp, GPOS_WSZ_LIT("EqualityOp")},
		{EdxltokenMDTypeNEqOp, GPOS_WSZ_LIT("InequalityOp")},
		{EdxltokenMDTypeLTOp, GPOS_WSZ_LIT("LessThanOp")},
		{EdxltokenMDTypeLEqOp, GPOS_WSZ_LIT("LessThanEqualsOp")},
		{EdxltokenMDTypeGTOp, GPOS_WSZ_LIT("GreaterThanOp")},
		{EdxltokenMDTypeGEqOp, GPOS_WSZ_LIT("GreaterThanEqualsOp")},
		{EdxltokenMDTypeCompOp, GPOS_WSZ_LIT("ComparisonOp")},
		{EdxltokenMDTypeHashOp, GPOS_WSZ_LIT("HashOp")},
		{EdxltokenMDTypeRelid, GPOS_WSZ_LIT("BaseRelationMdid")},
		{EdxltokenMDTypeArray, GPOS_WSZ_LIT("ArrayType")},

		{EdxltokenMDTypeAggMin, GPOS_WSZ_LIT("MinAgg")},
		{EdxltokenMDTypeAggMax, GPOS_WSZ_LIT("MaxAgg")},
		{EdxltokenMDTypeAggAvg, GPOS_WSZ_LIT("AvgAgg")},
		{EdxltokenMDTypeAggSum, GPOS_WSZ_LIT("SumAgg")},
		{EdxltokenMDTypeAggCount, GPOS_WSZ_LIT("CountAgg")},

		{EdxltokenGPDBScalarOp, GPOS_WSZ_LIT("GPDBScalarOp")},
		{EdxltokenGPDBScalarOpLeftTypeId, GPOS_WSZ_LIT("LeftType")},
		{EdxltokenGPDBScalarOpRightTypeId, GPOS_WSZ_LIT("RightType")},
		{EdxltokenGPDBScalarOpResultTypeId, GPOS_WSZ_LIT("ResultType")},
		{EdxltokenGPDBScalarOpFuncId, GPOS_WSZ_LIT("OpFunc")},
		{EdxltokenGPDBScalarOpCommOpId, GPOS_WSZ_LIT("Commutator")},
		{EdxltokenGPDBScalarOpInverseOpId, GPOS_WSZ_LIT("InverseOp")},
		{EdxltokenGPDBScalarOpLTOpId, GPOS_WSZ_LIT("LessThanMergeOp")},
		{EdxltokenGPDBScalarOpGTOpId, GPOS_WSZ_LIT("GreaterThanMergeOp")},
		{EdxltokenGPDBScalarOpCmpType, GPOS_WSZ_LIT("ComparisonType")},
		{EdxltokenGPDBScalarOpHashOpfamily, GPOS_WSZ_LIT("HashOpfamily")},
		{EdxltokenGPDBScalarOpLegacyHashOpfamily,
		 GPOS_WSZ_LIT("LegacyHashOpfamily")},

		{EdxltokenCmpEq, GPOS_WSZ_LIT("Eq")},
		{EdxltokenCmpNeq, GPOS_WSZ_LIT("NEq")},
		{EdxltokenCmpLt, GPOS_WSZ_LIT("LT")},
		{EdxltokenCmpLeq, GPOS_WSZ_LIT("LEq")},
		{EdxltokenCmpGt, GPOS_WSZ_LIT("GT")},
		{EdxltokenCmpGeq, GPOS_WSZ_LIT("GEq")},
		{EdxltokenCmpIDF, GPOS_WSZ_LIT("IDF")},
		{EdxltokenCmpOther, GPOS_WSZ_LIT("Other")},

		{EdxltokenReturnsNullOnNullInput,
		 GPOS_WSZ_LIT("ReturnsNullOnNullInput")},
		{EdxltokenIsNDVPreserving, GPOS_WSZ_LIT("IsNDVPreserving")},

		{EdxltokenGPDBFunc, GPOS_WSZ_LIT("GPDBFunc")},
		{EdxltokenGPDBFuncStability, GPOS_WSZ_LIT("Stability")},
		{EdxltokenGPDBFuncStable, GPOS_WSZ_LIT("Stable")},
		{EdxltokenGPDBFuncImmutable, GPOS_WSZ_LIT("Immutable")},
		{EdxltokenGPDBFuncVolatile, GPOS_WSZ_LIT("Volatile")},
		{EdxltokenGPDBFuncDataAccess, GPOS_WSZ_LIT("DataAccess")},
		{EdxltokenGPDBFuncNoSQL, GPOS_WSZ_LIT("NoSQL")},
		{EdxltokenGPDBFuncContainsSQL, GPOS_WSZ_LIT("ContainsSQL")},
		{EdxltokenGPDBFuncReadsSQLData, GPOS_WSZ_LIT("ReadsSQLData")},
		{EdxltokenGPDBFuncModifiesSQLData, GPOS_WSZ_LIT("ModifiesSQLData")},
		{EdxltokenGPDBFuncResultTypeId, GPOS_WSZ_LIT("ResultType")},
		{EdxltokenGPDBFuncReturnsSet, GPOS_WSZ_LIT("ReturnsSet")},
		{EdxltokenGPDBFuncStrict, GPOS_WSZ_LIT("IsStrict")},
		{EdxltokenGPDBFuncNDVPreserving, GPOS_WSZ_LIT("IsNDVPreserving")},
		{EdxltokenGPDBFuncIsAllowedForPS, GPOS_WSZ_LIT("IsAllowedForPS")},

		{EdxltokenGPDBAgg, GPOS_WSZ_LIT("GPDBAgg")},
		{EdxltokenGPDBIsAggOrdered, GPOS_WSZ_LIT("IsOrdered")},
		{EdxltokenGPDBIsAggRepSafe, GPOS_WSZ_LIT("IsRepSafe")},
		{EdxltokenGPDBAggResultTypeId, GPOS_WSZ_LIT("ResultType")},
		{EdxltokenGPDBAggIntermediateResultTypeId,
		 GPOS_WSZ_LIT("IntermediateResultType")},
		{EdxltokenGPDBAggSplittable, GPOS_WSZ_LIT("IsSplittable")},
		{EdxltokenGPDBAggHashAggCapable, GPOS_WSZ_LIT("HashAggCapable")},

		{EdxltokenGPDBCast, GPOS_WSZ_LIT("MDCast")},
		{EdxltokenGPDBCastBinaryCoercible, GPOS_WSZ_LIT("BinaryCoercible")},
		{EdxltokenGPDBCastSrcType, GPOS_WSZ_LIT("SourceTypeId")},
		{EdxltokenGPDBCastDestType, GPOS_WSZ_LIT("DestinationTypeId")},
		{EdxltokenGPDBCastFuncId, GPOS_WSZ_LIT("CastFuncId")},
		{EdxltokenGPDBCastCoercePathType, GPOS_WSZ_LIT("CoercePathType")},
		{EdxltokenGPDBArrayCoerceCast, GPOS_WSZ_LIT("ArrayCoerceCast")},

		{EdxltokenGPDBMDScCmp, GPOS_WSZ_LIT("MDScalarComparison")},

		{EdxltokenQuery, GPOS_WSZ_LIT("Query")},
		{EdxltokenQueryOutput, GPOS_WSZ_LIT("OutputColumns")},
		{EdxltokenLogical, GPOS_WSZ_LIT("LogicalOp")},
		{EdxltokenLogicalGet, GPOS_WSZ_LIT("LogicalGet")},
		{EdxltokenLogicalForeignGet, GPOS_WSZ_LIT("LogicalForeignGet")},
		{EdxltokenLogicalProject, GPOS_WSZ_LIT("LogicalProject")},
		{EdxltokenLogicalSelect, GPOS_WSZ_LIT("LogicalSelect")},
		{EdxltokenLogicalJoin, GPOS_WSZ_LIT("LogicalJoin")},
		{EdxltokenLogicalCTEProducer, GPOS_WSZ_LIT("LogicalCTEProducer")},
		{EdxltokenLogicalCTEConsumer, GPOS_WSZ_LIT("LogicalCTEConsumer")},
		{EdxltokenCTEList, GPOS_WSZ_LIT("CTEList")},
		{EdxltokenLogicalCTEAnchor, GPOS_WSZ_LIT("LogicalCTEAnchor")},
		{EdxltokenLogicalLimit, GPOS_WSZ_LIT("LogicalLimit")},
		{EdxltokenLogicalConstTable, GPOS_WSZ_LIT("LogicalConstTable")},
		{EdxltokenLogicalGrpBy, GPOS_WSZ_LIT("LogicalGroupBy")},
		{EdxltokenLogicalSetOperation, GPOS_WSZ_LIT("LogicalSetOperation")},
		{EdxltokenLogicalTVF, GPOS_WSZ_LIT("LogicalTVF")},
		{EdxltokenLogicalWindow, GPOS_WSZ_LIT("LogicalWindow")},

		{EdxltokenLogicalInsert, GPOS_WSZ_LIT("LogicalInsert")},
		{EdxltokenLogicalDelete, GPOS_WSZ_LIT("LogicalDelete")},
		{EdxltokenLogicalUpdate, GPOS_WSZ_LIT("LogicalUpdate")},
		{EdxltokenLogicalCTAS, GPOS_WSZ_LIT("LogicalCTAS")},
		{EdxltokenPhysicalCTAS, GPOS_WSZ_LIT("PhysicalCTAS")},

		{EdxltokenInsertCols, GPOS_WSZ_LIT("InsertColumns")},
		{EdxltokenDeleteCols, GPOS_WSZ_LIT("DeleteColumns")},
		{EdxltokenNewCols, GPOS_WSZ_LIT("NewColumns")},
		{EdxltokenOldCols, GPOS_WSZ_LIT("OldColumns")},

		{EdxltokenCTEId, GPOS_WSZ_LIT("CTEId")},

		{EdxltokenInputCols, GPOS_WSZ_LIT("InputColumns")},
		{EdxltokenCastAcrossInputs, GPOS_WSZ_LIT("CastAcrossInputs")},

		{EdxltokenLogicalUnion, GPOS_WSZ_LIT("Union")},
		{EdxltokenLogicalUnionAll, GPOS_WSZ_LIT("UnionAll")},
		{EdxltokenLogicalIntersect, GPOS_WSZ_LIT("Intersect")},
		{EdxltokenLogicalIntersectAll, GPOS_WSZ_LIT("IntersectAll")},
		{EdxltokenLogicalDifference, GPOS_WSZ_LIT("Difference")},
		{EdxltokenLogicalDifferenceAll, GPOS_WSZ_LIT("DifferenceAll")},

		{EdxltokenIndexDescr, GPOS_WSZ_LIT("IndexDescriptor")},
		{EdxltokenIndexName, GPOS_WSZ_LIT("IndexName")},
		{EdxltokenScalarIndexCondList, GPOS_WSZ_LIT("IndexCondList")},
		{EdxltokenIndexScanDirection, GPOS_WSZ_LIT("IndexScanDirection")},
		{EdxltokenIndexScanDirectionForward, GPOS_WSZ_LIT("Forward")},
		{EdxltokenIndexScanDirectionBackward, GPOS_WSZ_LIT("Backward")},
		{EdxltokenIndexScanDirectionNoMovement, GPOS_WSZ_LIT("NoMovement")},

		{EdxltokenStackTrace, GPOS_WSZ_LIT("Stacktrace")},

		{EdxltokenStatistics, GPOS_WSZ_LIT("Statistics")},
		{EdxltokenStatsBaseRelation, GPOS_WSZ_LIT("BaseRelationStats")},
		{EdxltokenStatsDerivedRelation, GPOS_WSZ_LIT("DerivedRelationStats")},
		{EdxltokenStatsDerivedColumn, GPOS_WSZ_LIT("DerivedColumnStats")},
		{EdxltokenStatsBucketLowerBound, GPOS_WSZ_LIT("LowerBound")},
		{EdxltokenStatsBucketUpperBound, GPOS_WSZ_LIT("UpperBound")},
		{EdxltokenStatsFrequency, GPOS_WSZ_LIT("Frequency")},
		{EdxltokenStatsDistinct, GPOS_WSZ_LIT("DistinctValues")},
		{EdxltokenStatsBoundClosed, GPOS_WSZ_LIT("Closed")},

		{EdxltokenSearchStrategy, GPOS_WSZ_LIT("SearchStrategy")},
		{EdxltokenSearchStage, GPOS_WSZ_LIT("SearchStage")},
		{EdxltokenXform, GPOS_WSZ_LIT("Xform")},
		{EdxltokenTimeThreshold, GPOS_WSZ_LIT("TimeThreshold")},
		{EdxltokenCostThreshold, GPOS_WSZ_LIT("CostThreshold")},

		{EdxltokenCostParams, GPOS_WSZ_LIT("CostParams")},
		{EdxltokenCostParam, GPOS_WSZ_LIT("CostParam")},
		{EdxltokenCostParamLowerBound, GPOS_WSZ_LIT("LowerBound")},
		{EdxltokenCostParamUpperBound, GPOS_WSZ_LIT("UpperBound")},

		{EdxltokenTopLimitUnderDML, GPOS_WSZ_LIT("TopLimitUnderDML")},

		{EdxltokenCtasOptionType, GPOS_WSZ_LIT("CtasOptionType")},
		{EdxltokenVarTypeModList, GPOS_WSZ_LIT("VarTypeModList")},
		{EdxltokenNLJIndexParamList, GPOS_WSZ_LIT("NLJIndexParamList")},
		{EdxltokenNLJIndexParam, GPOS_WSZ_LIT("NLJIndexParam")},
		{EdxltokenNLJIndexOuterRefAsParam, GPOS_WSZ_LIT("OuterRefAsParam")},
	};

	m_pstrmap = GPOS_NEW_ARRAY(m_mp, SStrMapElem, EdxltokenSentinel);
	m_pxmlszmap = GPOS_NEW_ARRAY(m_mp, SXMLStrMapElem, EdxltokenSentinel);

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgStrMap); ul++)
	{
		SWszMapElem mapelem = rgStrMap[ul];

		m_pstrmap[mapelem.m_edxlt].m_pstr =
			GPOS_NEW(m_mp) CWStringConst(m_mp, mapelem.m_wsz);
		m_pxmlszmap[mapelem.m_edxlt].m_xmlsz = XmlstrFromWsz(mapelem.m_wsz);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTokens::Terminate
//
//	@doc:
//		Releases the DXL token constants
//
//---------------------------------------------------------------------------
void
CDXLTokens::Terminate()
{
	GPOS_DELETE_ARRAY(m_pstrmap);
	GPOS_DELETE_ARRAY(m_pxmlszmap);
	GPOS_DELETE(m_dxl_memory_manager);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTokens::GetDXLTokenStr
//
//	@doc:
//		Returns the token with the given token id in CWStringConst format
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLTokens::GetDXLTokenStr(Edxltoken token_type)
{
	GPOS_ASSERT(nullptr != m_pstrmap && "Token map not initialized yet");

	const CWStringConst *str = m_pstrmap[token_type].m_pstr;
	GPOS_ASSERT(nullptr != str);

	return str;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTokens::XmlstrToken
//
//	@doc:
//		Returns the token with the given token id in XMLCh* format
//
//---------------------------------------------------------------------------
const XMLCh *
CDXLTokens::XmlstrToken(Edxltoken token_type)
{
	GPOS_ASSERT(nullptr != m_pxmlszmap && "Token map not initialized yet");

	const XMLCh *xml_val = m_pxmlszmap[token_type].m_xmlsz;
	GPOS_ASSERT(nullptr != xml_val);

	return xml_val;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTokens::XmlstrFromWsz
//
//	@doc:
//		Creates an XMLCh* string from the specified wide character array in the
//		provided memory pool.
//
//---------------------------------------------------------------------------
XMLCh *
CDXLTokens::XmlstrFromWsz(const WCHAR *wsz)
{
	ULONG length = GPOS_WSZ_LENGTH(wsz);
	CHAR *sz = GPOS_NEW_ARRAY(m_mp, CHAR, 1 + length);

	LINT iLen GPOS_ASSERTS_ONLY =
		clib::Wcstombs(sz, const_cast<WCHAR *>(wsz), 1 + length);

	GPOS_ASSERT(0 <= iLen);
	XMLCh *pxmlsz = XMLString::transcode(sz, m_dxl_memory_manager);
	GPOS_DELETE_ARRAY(sz);
	return pxmlsz;
}

// EOF
