//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CAggTest.cpp
//
//	@doc:
//		Test for optimizing queries with aggregates
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CAggTest.h"
#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"


using namespace gpopt;

ULONG CAggTest::m_ulAggTestCounter = 0;  // start from first test

// minidump files
const CHAR *rgszAggFileNames[] =
{
	"../data/dxl/minidump/AggWithVolatileFunc.mdp",
	"../data/dxl/minidump/VolatileFunctionsBelowScalarAgg.mdp",
	"../data/dxl/minidump/EagerAggMax.mdp",
	"../data/dxl/minidump/EagerAggMaxWithNestedLoop.mdp",
	"../data/dxl/minidump/EagerAggEmptyInput.mdp",
	"../data/dxl/minidump/EagerAggMinMax.mdp",
	"../data/dxl/minidump/EagerAggExpression.mdp",
	"../data/dxl/minidump/EagerAggUnsupportedAgg.mdp",
	"../data/dxl/minidump/EagerAggGroupColumnInJoin.mdp",
	"../data/dxl/minidump/EagerAggSubquery.mdp",
	"../data/dxl/minidump/DQA-KeepOuterReference.mdp",
	"../data/dxl/minidump/ScalarSubqueryCountStarInJoin.mdp",
	"../data/dxl/minidump/ScalarCorrelatedSubqueryCountStar.mdp",
	"../data/dxl/minidump/ScalarSubqueryCountStar.mdp",
	"../data/dxl/minidump/DQA-SplitScalarWithAggAndGuc.mdp",
	"../data/dxl/minidump/DQA-SplitScalarOnDistCol.mdp",
	"../data/dxl/minidump/ScalarDQAWithNonScalarAgg.mdp",
	"../data/dxl/minidump/DQA-SplitScalarWithGuc.mdp",
	"../data/dxl/minidump/DQA-SplitScalar.mdp",
	"../data/dxl/minidump/Agg-NonSplittable.mdp",
	"../data/dxl/minidump/SortOverStreamAgg.mdp",
	"../data/dxl/minidump/NoHashAggWithoutPrelimFunc.mdp",
	"../data/dxl/minidump/AggWithSubqArgs.mdp",
	"../data/dxl/minidump/Agg-Limit.mdp",
	"../data/dxl/minidump/GroupByEmptySetNoAgg.mdp",
	"../data/dxl/minidump/CollapseGb-With-Agg-Funcs.mdp",
	"../data/dxl/minidump/CollapseGb-Without-Agg-Funcs.mdp",
	"../data/dxl/minidump/CollapseGb-SingleColumn.mdp",
	"../data/dxl/minidump/CollapseGb-MultipleColumn.mdp",
	"../data/dxl/minidump/CollapseGb-Nested.mdp",
	"../data/dxl/minidump/ThreeStageAgg.mdp",
	"../data/dxl/minidump/ThreeStageAgg-GbandDistinctOnDistrCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-GbMultipleCol-DistinctOnDistrCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-DistinctOnSameNonDistrCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-DistinctOnComputedCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-DistinctOnDistrCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-ScalarAgg-DistinctDistrCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-ScalarAgg-DistinctNonDistrCol.mdp",
	"../data/dxl/minidump/ThreeStageAgg-ScalarAgg-DistinctComputedCol.mdp",
	"../data/dxl/minidump/CannotPullGrpColAboveAgg.mdp",
	"../data/dxl/minidump/DQA-1-RegularAgg.mdp",
	"../data/dxl/minidump/DQA-2-RegularAgg.mdp",
	"../data/dxl/minidump/MDQA-SameDQAColumn.mdp",
	"../data/dxl/minidump/MDQAs1.mdp",
	"../data/dxl/minidump/MDQAs-Grouping.mdp",
	"../data/dxl/minidump/MDQAs-Grouping-OrderBy.mdp",
	"../data/dxl/minidump/MDQAs-Union.mdp",
	"../data/dxl/minidump/DistinctAgg-NonSplittable.mdp",
	"../data/dxl/minidump/RollupNoAgg.mdp",
	"../data/dxl/minidump/GroupingSets.mdp",
#ifndef GPOS_DEBUG
	"../data/dxl/minidump/CapGbCardToSelectCard.mdp",
	"../data/dxl/minidump/GroupingOnSameTblCol-1.mdp",
	"../data/dxl/minidump/GroupingOnSameTblCol-2.mdp",
#endif
	"../data/dxl/minidump/PushGbBelowJoin-NegativeCase.mdp",
	"../data/dxl/minidump/Gb-on-keys.mdp",
	"../data/dxl/minidump/ComputedGroupByCol.mdp",
	"../data/dxl/minidump/GroupByOuterRef.mdp",
	"../data/dxl/minidump/DuplicateGrpCol.mdp",
	"../data/dxl/minidump/CountAny.mdp",
	"../data/dxl/minidump/CountStar.mdp",
	"../data/dxl/minidump/ProjectCountStar.mdp",
	"../data/dxl/minidump/ProjectOutsideCountStar.mdp",
	"../data/dxl/minidump/NestedProjectCountStarWithOuterRefs.mdp",
	"../data/dxl/minidump/AggregateWithSkew.mdp",
};


//---------------------------------------------------------------------------
//	@function:
//		CAggTest::EresUnittest
// @doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CAggTest::EresUnittest()
{

	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
		};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CAggTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CAggTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests
						(
						rgszAggFileNames,
						&m_ulAggTestCounter,
						GPOS_ARRAY_SIZE(rgszAggFileNames)
						);
}

// EOF
