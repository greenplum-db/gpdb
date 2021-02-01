//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorExprToDXLTest.cpp
//
//	@doc:
//		Test for translating CExpressions into DXL
//---------------------------------------------------------------------------
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/exception.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"



ULONG CTranslatorExprToDXLTest::m_ulTestCounter = 0;  // start from first test


const CTestUtils::STestCase rgtc[] = {
	{"../data/dxl/expressiontests/TableScanQuery.xml",
	 "../data/dxl/expressiontests/TableScanPlan.xml"},
	{"../data/dxl/expressiontests/TableScanWithFilterQuery.xml",
	 "../data/dxl/expressiontests/TableScanWithFilterPlan.xml"},
	{"../data/dxl/expressiontests/FilterQuery.xml",
	 "../data/dxl/expressiontests/FilterPlan.xml"},
	{"../data/dxl/expressiontests/FilterOnTableWithIndexQuery.xml",
	 "../data/dxl/expressiontests/FilterOnTableWithIndexPlan.xml"},
	{"../data/dxl/expressiontests/ConstQuery.xml",
	 "../data/dxl/expressiontests/ConstPlan.xml"},
	{"../data/dxl/expressiontests/FilterLogOpQuery.xml",
	 "../data/dxl/expressiontests/FilterLogOpPlan.xml"},
	{"../data/dxl/expressiontests/FuncExprQuery.xml",
	 "../data/dxl/expressiontests/FuncExprPlan.xml"},
	{"../data/dxl/expressiontests/GroupByQuery.xml",
	 "../data/dxl/expressiontests/GroupByPlan.xml"},
	{"../data/dxl/expressiontests/GroupByNoAggQuery.xml",
	 "../data/dxl/expressiontests/GroupByNoAggPlan.xml"},
	{"../data/dxl/expressiontests/AggNoGroupByQuery.xml",
	 "../data/dxl/expressiontests/AggNoGroupByPlan.xml"},
	{"../data/dxl/expressiontests/ScalarOpAddQuery.xml",
	 "../data/dxl/expressiontests/ScalarOpAddPlan.xml"},
	{"../data/dxl/expressiontests/ScalarDistFromQuery.xml",
	 "../data/dxl/expressiontests/ScalarDistFromPlan.xml"},
	{"../data/dxl/expressiontests/ScalarNullTestQuery.xml",
	 "../data/dxl/expressiontests/ScalarNullTestPlan.xml"},
	{"../data/dxl/expressiontests/LeftOuterJoinHJQuery.xml",
	 "../data/dxl/expressiontests/LeftOuterJoinHJPlan.xml"},
	{"../data/dxl/expressiontests/LeftOuterJoinNLQuery.xml",
	 "../data/dxl/expressiontests/LeftOuterJoinNLPlan.xml"},
	{"../data/dxl/expressiontests/InnerJoinQuery.xml",
	 "../data/dxl/expressiontests/InnerJoinPlan.xml"},
	{"../data/dxl/expressiontests/NAryJoinQuery.xml",
	 "../data/dxl/expressiontests/NAryJoinPlan.xml"},
	{"../data/dxl/expressiontests/ScalarIfQuery.xml",
	 "../data/dxl/expressiontests/ScalarIfPlan.xml"},
	{"../data/dxl/expressiontests/ScalarCastQuery.xml",
	 "../data/dxl/expressiontests/ScalarCastPlan.xml"},
	{"../data/dxl/expressiontests/ProjectQuery.xml",
	 "../data/dxl/expressiontests/ProjectPlan.xml"},
	{"../data/dxl/expressiontests/HashJoinQuery.xml",
	 "../data/dxl/expressiontests/HashJoinPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetCountStarQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetCountStarPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetRangeSelectQuery1.xml",
	 "../data/dxl/expressiontests/DynamicGetRangeSelectPlan1.xml"},
	{"../data/dxl/expressiontests/DynamicGetRangeSelectQuery2.xml",
	 "../data/dxl/expressiontests/DynamicGetRangeSelectPlan2.xml"},
	{"../data/dxl/expressiontests/DynamicGetPointQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetPointPlan.xml"},
	{"../data/dxl/expressiontests/ArrayQuery.xml",
	 "../data/dxl/expressiontests/ArrayPlan.xml"},
	{"../data/dxl/expressiontests/ArrayCmpQuery.xml",
	 "../data/dxl/expressiontests/ArrayCmpPlan.xml"},
	{"../data/dxl/expressiontests/CorrelatedSubqueryQuery.xml",
	 "../data/dxl/expressiontests/CorrelatedSubqueryPlan.xml"},
	{"../data/dxl/expressiontests/CorrelatedSubqueryFilterQuery.xml",
	 "../data/dxl/expressiontests/CorrelatedSubqueryFilterPlan.xml"},
	{"../data/dxl/expressiontests/CorrelatedSubqueryResultQuery.xml",
	 "../data/dxl/expressiontests/CorrelatedSubqueryResultPlan.xml"},
	{"../data/dxl/expressiontests/CorrelatedSubqueryBooleanQuery.xml",
	 "../data/dxl/expressiontests/CorrelatedSubqueryBooleanPlan.xml"},
	{"../data/dxl/expressiontests/CorrelatedSubqueryOuterQuery.xml",
	 "../data/dxl/expressiontests/CorrelatedSubqueryOuterPlan.xml"},
#ifdef GPOS_DEBUG
	{"../data/dxl/expressiontests/SortQuery.xml",
	 "../data/dxl/expressiontests/SortPlan.xml"},
	{"../data/dxl/expressiontests/DoubleSortQuery.xml",
	 "../data/dxl/expressiontests/DoubleSortPlan.xml"},
	{"../data/dxl/expressiontests/RedundantSortQuery.xml",
	 "../data/dxl/expressiontests/RedundantSortPlan.xml"},
#endif
	{"../data/dxl/expressiontests/ScalarSubqueryQuery.xml",
	 "../data/dxl/expressiontests/ScalarSubqueryPlan.xml"},
	{"../data/dxl/expressiontests/ScalarSubqueryAnyAllQuery.xml",
	 "../data/dxl/expressiontests/ScalarSubqueryAnyAllPlan.xml"},
	{"../data/dxl/expressiontests/ScalarSubqueryExistentialQuery.xml",
	 "../data/dxl/expressiontests/ScalarSubqueryExistentialPlan.xml"},
	{"../data/dxl/expressiontests/GatherQuery.xml",
	 "../data/dxl/expressiontests/GatherPlan.xml"},
	{"../data/dxl/expressiontests/GatherMergeQuery.xml",
	 "../data/dxl/expressiontests/GatherMergePlan.xml"},
	{"../data/dxl/expressiontests/BroadcastQuery.xml",
	 "../data/dxl/expressiontests/BroadcastPlan.xml"},
	{"../data/dxl/expressiontests/HashDistributeQuery.xml",
	 "../data/dxl/expressiontests/HashDistributePlan.xml"},
	{"../data/dxl/expressiontests/SpoolQuery.xml",
	 "../data/dxl/expressiontests/SpoolPlan.xml"},
	{"../data/dxl/expressiontests/ConstTableGetQuery.xml",
	 "../data/dxl/expressiontests/ConstTableGetPlan.xml"},
	{"../data/dxl/expressiontests/NLLSJoinQuery.xml",
	 "../data/dxl/expressiontests/NLLSJoinPlan.xml"},
	{"../data/dxl/expressiontests/NLLASJoinQuery.xml",
	 "../data/dxl/expressiontests/NLLASJoinPlan.xml"},
	{"../data/dxl/expressiontests/TableValuedFunctionQuery.xml",
	 "../data/dxl/expressiontests/TableValuedFunctionPlan.xml"},
	{"../data/dxl/expressiontests/TableValuedFunctionJoinQuery.xml",
	 "../data/dxl/expressiontests/TableValuedFunctionJoinPlan.xml"},
	{"../data/dxl/expressiontests/UnionAllQuery.xml",
	 "../data/dxl/expressiontests/UnionAllPlan.xml"},
	{"../data/dxl/expressiontests/UnionAllRemappedQuery.xml",
	 "../data/dxl/expressiontests/UnionAllRemappedPlan.xml"},
	{"../data/dxl/expressiontests/UnionQuery.xml",
	 "../data/dxl/expressiontests/UnionPlan.xml"},
	{"../data/dxl/expressiontests/IntersectAllQuery.xml",
	 "../data/dxl/expressiontests/IntersectAllPlan.xml"},
	{"../data/dxl/expressiontests/IntersectQuery.xml",
	 "../data/dxl/expressiontests/IntersectPlan.xml"},
	{"../data/dxl/expressiontests/DifferenceQuery.xml",
	 "../data/dxl/expressiontests/DifferencePlan.xml"},
	{"../data/dxl/expressiontests/WindowQuery.xml",
	 "../data/dxl/expressiontests/WindowPlan.xml"},
	{"../data/dxl/expressiontests/WindowQueryEmptyPartitionBy.xml",
	 "../data/dxl/expressiontests/WindowPlanEmptyPartitionBy.xml"},
	{"../data/dxl/expressiontests/WindowWithFrameQuery.xml",
	 "../data/dxl/expressiontests/WindowWithFramePlan.xml"},
	{"../data/dxl/expressiontests/MultipleWindowFuncQuery.xml",
	 "../data/dxl/expressiontests/MultipleWindowFuncPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetHashJoinPartKeyQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetHashJoinPartKeyPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetHashJoinOtherKeyQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetHashJoinOtherKeyPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetUnionAllOuterJoinQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetUnionAllOuterJoinPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetNLJoinPartKeyQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetNLJoinPartKeyPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetNLJoinOtherKeyQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetNLJoinOtherKeyPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetBooleanQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetBooleanPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetBooleanNotQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetBooleanNotPlan.xml"},
	{"../data/dxl/expressiontests/DynamicGetMultiJoinQuery.xml",
	 "../data/dxl/expressiontests/DynamicGetMultiJoinPlan.xml"},
	{"../data/dxl/expressiontests/CoalesceQuery.xml",
	 "../data/dxl/expressiontests/CoalescePlan.xml"},
	{"../data/dxl/expressiontests/ScalarSwitchQuery.xml",
	 "../data/dxl/expressiontests/ScalarSwitchPlan.xml"},
	{"../data/dxl/expressiontests/ScalarCaseTestQuery.xml",
	 "../data/dxl/expressiontests/ScalarCaseTestPlan.xml"},
	{"../data/dxl/expressiontests/NullIfQuery.xml",
	 "../data/dxl/expressiontests/NullIfPlan.xml"},
	{"../data/dxl/expressiontests/RightOuterJoinQuery.xml",
	 "../data/dxl/expressiontests/RightOuterJoinPlan.xml"},
	{"../data/dxl/expressiontests/ContradictionQuery.xml",
	 "../data/dxl/expressiontests/ContradictionPlan.xml"},
	{"../data/dxl/expressiontests/WindowWithNoLeadingEdgeQuery.xml",
	 "../data/dxl/expressiontests/WindowWithNoLeadingEdgePlan.xml"},
	{"../data/dxl/expressiontests/VolatileFuncQuery.xml",
	 "../data/dxl/expressiontests/VolatileFuncPlan.xml"},
	{"../data/dxl/expressiontests/VolatileTVFQuery.xml",
	 "../data/dxl/expressiontests/VolatileTVFPlan.xml"},
	{"../data/dxl/expressiontests/VolatileNLJoinQuery.xml",
	 "../data/dxl/expressiontests/VolatileNLJoinPlan.xml"},
	{"../data/dxl/expressiontests/VolatileHashJoinQuery.xml",
	 "../data/dxl/expressiontests/VolatileHashJoinPlan.xml"},
	{"../data/dxl/expressiontests/VolatileCSQQuery.xml",
	 "../data/dxl/expressiontests/VolatileCSQPlan.xml"},
	{"../data/dxl/expressiontests/VolatileWithPartTableQuery.xml",
	 "../data/dxl/expressiontests/VolatileWithPartTablePlan.xml"},
	{"../data/dxl/expressiontests/InsertPartitionedQuery.xml",
	 "../data/dxl/expressiontests/InsertPartitionedPlan.xml"},
};

// minidump files
const CHAR *rgszMinidumpFileNames[] = {
	"../data/dxl/minidump/PartTbl-DTS.mdp",
	"../data/dxl/minidump/PartTbl-DTSEq.mdp",
	"../data/dxl/minidump/PartTbl-DTSLessThan.mdp",
	"../data/dxl/minidump/PartTbl-HJ1.mdp",
	"../data/dxl/minidump/PartTbl-HJ2.mdp",
	//		"../data/dxl/minidump/PartTbl-NLJ.mdp",
};

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorExprToDXLTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CTranslatorExprToDXLTest::EresUnittest_RunTests),
		GPOS_UNITTEST_FUNC(
			CTranslatorExprToDXLTest::EresUnittest_RunMinidumpTests),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLTest::EresUnittest_RunTests
//
//	@doc:
//		Run all the Expr -> DXL translation tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorExprToDXLTest::EresUnittest_RunTests()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	const ULONG ulTests = GPOS_ARRAY_SIZE(rgtc);
	for (ULONG ul = m_ulTestCounter; ul < ulTests; ul++)
	{
		GPOS_RESULT eres = CTestUtils::EresTranslate(mp, rgtc[ul].szInputFile,
													 rgtc[ul].szOutputFile,
													 true /*fIgnoreMismatch*/);
		m_ulTestCounter++;

		if (GPOS_OK != eres)
		{
			return eres;
		}
	}
	// reset test counter
	m_ulTestCounter = 0;

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLTest::EresUnittest_RunMinidumpTests
//
//	@doc:
//		Run all Minidump-based tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorExprToDXLTest::EresUnittest_RunMinidumpTests()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();
	const ULONG ulTests = GPOS_ARRAY_SIZE(rgszMinidumpFileNames);

	return CTestUtils::EresRunMinidumps(mp, rgszMinidumpFileNames, ulTests,
										&m_ulTestCounter,
										1,		// ulSessionId
										1,		// ulCmdId
										false,	//fMatchPlans
										false	// fTestSpacePruning
	);
}

// EOF
