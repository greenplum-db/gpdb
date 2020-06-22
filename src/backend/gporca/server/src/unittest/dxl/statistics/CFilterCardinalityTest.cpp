//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CFilterCardinalityTest.cpp
//
//	@doc:
//		Test for filter cardinality estimation
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/dxl/statistics/CStatisticsTest.h"
#include "unittest/dxl/statistics/CFilterCardinalityTest.h"

#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;

// DXL files
const CHAR *
szInputDXLFileName = "../data/dxl/statistics/Basic-Statistics-Input.xml";
const CHAR *
szOutputDXLFileName = "../data/dxl/statistics/Basic-Statistics-Output.xml";

const CTestUtils::STestCase rgtcStatistics[] =
{
	{"../data/dxl/statistics/Numeric-Input.xml", "../data/dxl/statistics/Numeric-Output-LT-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input.xml", "../data/dxl/statistics/Numeric-Output-LTE-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input.xml", "../data/dxl/statistics/Numeric-Output-E-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input.xml", "../data/dxl/statistics/Numeric-Output-GT-MaxBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input.xml", "../data/dxl/statistics/Numeric-Output-GTE-MaxBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input.xml", "../data/dxl/statistics/Numeric-Output-E-MaxBoundary.xml"},

	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-LT-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-LTE-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-E-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-GT-MinBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-GTE-MinBoundary.xml"},

	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-LT-MaxBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-LTE-MaxBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-GT-MaxBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-GTE-MaxBoundary.xml"},
	{"../data/dxl/statistics/Numeric-Input2.xml", "../data/dxl/statistics/Numeric-Output-2-E-MaxBoundary.xml"},
};

// unittest for statistics objects
GPOS_RESULT
CFilterCardinalityTest::EresUnittest()
{
	// tests that use shared optimization context
	CUnittest rgutSharedOptCtxt[] =
		{
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsBasicsFromDXLNumeric),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsFilter),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsFilterArrayCmpAny),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsFilterConj),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsFilterDisj),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsNestedPred),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsBasicsFromDXL),
		GPOS_UNITTEST_FUNC(CFilterCardinalityTest::EresUnittest_CStatisticsAccumulateCard)
		};

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /* pceeval */, CTestUtils::GetCostModel(mp));

	return CUnittest::EresExecute(rgutSharedOptCtxt, GPOS_ARRAY_SIZE(rgutSharedOptCtxt));
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatistics
	(
	SStatsFilterSTestCase rgstatsdisjtc[],
	ULONG ulTestCases
	)
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic str(mp);
	COstreamString oss(&str);

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	for (ULONG ul = 0; ul < ulTestCases; ul++)
	{
		SStatsFilterSTestCase elem = rgstatsdisjtc[ul];

		// read input/output DXL file
		CHAR *szDXLInput = CDXLUtils::Read(mp, elem.m_szInputFile);
		CHAR *szDXLOutput = CDXLUtils::Read(mp, elem.m_szOutputFile);

		GPOS_CHECK_ABORT;

		// parse the statistics objects
		CDXLStatsDerivedRelationArray *dxl_derived_rel_stats_array = CDXLUtils::ParseDXLToStatsDerivedRelArray(mp, szDXLInput, NULL);
		CStatisticsArray *pdrgpstatBefore = CDXLUtils::ParseDXLToOptimizerStatisticObjArray(mp, md_accessor, dxl_derived_rel_stats_array);
		dxl_derived_rel_stats_array->Release();
		GPOS_ASSERT(NULL != pdrgpstatBefore);

		GPOS_CHECK_ABORT;

		// generate the disjunctive predicate
		FnPstatspredDisj *pf = elem.m_pf;
		GPOS_ASSERT(NULL != pf);
		CStatsPred *disjunctive_pred_stats = pf(mp);

		GPOS_RESULT eres = EresUnittest_CStatisticsCompare
									(
									mp,
									md_accessor,
									pdrgpstatBefore,
									disjunctive_pred_stats,
									szDXLOutput
									);

		// clean up
		pdrgpstatBefore->Release();
		disjunctive_pred_stats->Release();
		GPOS_DELETE_ARRAY(szDXLInput);
		GPOS_DELETE_ARRAY(szDXLOutput);

		if (GPOS_FAILED == eres)
		{
			return eres;
		}
	}

	return GPOS_OK;
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsFilter()
{
	SStatsFilterSTestCase rgstatstc[] =
	{
		{"../data/dxl/statistics/Select-Statistics-Input-Null-Bucket.xml", "../data/dxl/statistics/Select-Statistics-Output-Null-Bucket.xml", PstatspredNullableCols},
		{"../data/dxl/statistics/Select-Statistics-Input-Null-Bucket.xml", "../data/dxl/statistics/Select-Statistics-Output-Null-Constant.xml", PstatspredWithNullConstant},
		{"../data/dxl/statistics/Select-Statistics-Input-Null-Bucket.xml", "../data/dxl/statistics/Select-Statistics-Output-NotNull-Constant.xml", PstatspredNotNull},
	};

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatstc);

	return EresUnittest_CStatistics(rgstatstc, ulTestCases);
}

// create a filter on a column with null values
CStatsPred *
CFilterCardinalityTest::PstatspredNullableCols
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptLEq, CTestUtils::PpointInt4(mp, 1)));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred);
}

// create a point filter where the constant is null
CStatsPred *
CFilterCardinalityTest::PstatspredWithNullConstant
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq,  CTestUtils::PpointInt4NullVal(mp)));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred);
}

// create an 'is not null' point filter
CStatsPred *
CFilterCardinalityTest::PstatspredNotNull
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptNEq,  CTestUtils::PpointInt4NullVal(mp)));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred);
}

// testing ArryCmpAny predicates
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsFilterArrayCmpAny()
{
	SStatsFilterSTestCase rgstatsdisjtc[] =
	{
		{
			"../data/dxl/statistics/ArrayCmpAny-Input-1.xml",
			"../data/dxl/statistics/ArrayCmpAny-Output-1.xml",
			PstatspredArrayCmpAnySimple
		},
		{
			"../data/dxl/statistics/ArrayCmpAny-Input-1.xml",
			"../data/dxl/statistics/ArrayCmpAny-Output-1.xml",
			PstatspredArrayCmpAnyDuplicate
		}
	};

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatsdisjtc);

	return EresUnittest_CStatistics(rgstatsdisjtc, ulTestCases);
}

// create a 'col IN (...)' filter without duplicates
CStatsPred *
CFilterCardinalityTest::PstatspredArrayCmpAnySimple
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	CPointArray *arr = GPOS_NEW(mp) CPointArray(mp);
	arr->Append(CTestUtils::PpointInt4(mp, 1));
	arr->Append(CTestUtils::PpointInt4(mp, 2));
	arr->Append(CTestUtils::PpointInt4(mp, 15));

	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredArrayCmp(1, CStatsPred::EstatscmptEq, arr));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred);
}

// create a 'col IN (...)' filter with duplicates (unsorted)
CStatsPred *
CFilterCardinalityTest::PstatspredArrayCmpAnyDuplicate
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	CPointArray *arr = GPOS_NEW(mp) CPointArray(mp);
	arr->Append(CTestUtils::PpointInt4(mp, 1));
	arr->Append(CTestUtils::PpointInt4(mp, 1));
	arr->Append(CTestUtils::PpointInt4(mp, 15));
	arr->Append(CTestUtils::PpointInt4(mp, 2));
	arr->Append(CTestUtils::PpointInt4(mp, 1));
	arr->Append(CTestUtils::PpointInt4(mp, 1));
	arr->Append(CTestUtils::PpointInt4(mp, 2));
	arr->Append(CTestUtils::PpointInt4(mp, 15));
	arr->Append(CTestUtils::PpointInt4(mp, 15));
	arr->Append(CTestUtils::PpointInt4(mp, 15));

	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredArrayCmp(1, CStatsPred::EstatscmptEq, arr));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred);
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsFilterDisj()
{
	SStatsFilterSTestCase rgstatsdisjtc[] =
	{
		{"../data/dxl/statistics/Disj-Input-1.xml", "../data/dxl/statistics/Disj-Output-1.xml", PstatspredDisj1},
		{"../data/dxl/statistics/Disj-Input-1.xml", "../data/dxl/statistics/Disj-Output-1.xml", PstatspredDisj2},
		{"../data/dxl/statistics/Disj-Input-1.xml", "../data/dxl/statistics/Disj-Output-1.xml", PstatspredDisj3},
		{"../data/dxl/statistics/Disj-Input-2.xml", "../data/dxl/statistics/Disj-Output-2-1.xml", PstatspredDisj4},
		{"../data/dxl/statistics/Disj-Input-2.xml", "../data/dxl/statistics/Disj-Output-2-2.xml", PstatspredDisj5},
		{"../data/dxl/statistics/Disj-Input-2.xml", "../data/dxl/statistics/Disj-Output-2-3.xml", PstatspredDisj6},
		{"../data/dxl/statistics/Disj-Input-2.xml", "../data/dxl/statistics/Disj-Output-2-4.xml", PstatspredDisj7},
		{"../data/dxl/statistics/NestedPred-Input-10.xml", "../data/dxl/statistics/Disj-Output-8.xml", PstatspredDisj8},
		{"../data/dxl/statistics/Disj-Input-2.xml", "../data/dxl/statistics/Disj-Output-2-5.xml", PstatspredDisj9},
	};

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatsdisjtc);

	return EresUnittest_CStatistics(rgstatsdisjtc, ulTestCases);
}

// create an or filter (no duplicate)
CStatsPred *
CFilterCardinalityTest::PstatspredDisj1
	(
	CMemoryPool *mp
	)
{
	// predicate col_1 in (13, 25, 47, 49);
	INT rgiVal[] = {13, 25, 47, 49};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create an or filter (one duplicate constant)
CStatsPred *
CFilterCardinalityTest::PstatspredDisj2
	(
	CMemoryPool *mp
	)
{

	// predicate col_1 in (13, 13, 25, 47, 49);
	INT rgiVal[] = {13, 13, 25, 47, 49};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

//	create an or filter (multiple duplicate constants)
CStatsPred *
CFilterCardinalityTest::PstatspredDisj3
	(
	CMemoryPool *mp
	)
{
	// predicate col_1 in (13, 25, 47, 47, 47, 49, 13);
	INT rgiVal[] = {13, 25, 47, 47, 47, 49, 13};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create an or filter
CStatsPred *
CFilterCardinalityTest::PstatspredDisj4
	(
	CMemoryPool *mp
	)
{
	// the predicate is (x <= 5 or x <= 10 or x <= 13) (domain [0 -- 20])
	INT rgiVal[] = {5, 10, 13};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptLEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

//	create an or filter (multiple LEQ)
CStatsPred *
CFilterCardinalityTest::PstatspredDisj5
	(
	CMemoryPool *mp
	)
{
	// the predicate is (x >= 5 or x >= 13) (domain [0 -- 20])
	INT rgiVal[] = {5, 13};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptGEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

//	create an or filter
CStatsPred *
CFilterCardinalityTest::PstatspredDisj6
	(
	CMemoryPool *mp
	)
{
	// the predicate is (x > 10 or x < 5) (domain [0 -- 20])
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptG, CTestUtils::PpointInt4(mp, 10)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptL, CTestUtils::PpointInt4(mp, 5)));

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create an or filter
CStatsPred *
CFilterCardinalityTest::PstatspredDisj7
	(
	CMemoryPool *mp
	)
{
	// the predicate is (x <= 15 or x >= 5 or x > = 10) (domain [0 -- 20])
	INT rgiVal[] = {5, 10};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptGEq, rgiVal, ulVals);
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptLEq, CTestUtils::PpointInt4(mp, 15)));

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create disjunctive predicate on same columns
CStatsPred *
CFilterCardinalityTest::PstatspredDisj8
	(
	CMemoryPool *mp
	)
{
	// predicate is b = 2001 OR b == 2002
	INT rgiVal[] = {2001, 2002};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 61, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

//	create an or filter (multiple LEQ)
CStatsPred *
CFilterCardinalityTest::PstatspredDisj9
(
 CMemoryPool *mp
 )
{
	// the predicate is (x <= 3 or x <= 10) (domain [0 -- 20])
	INT rgiVal[] = {3, 10};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 1, CStatsPred::EstatscmptLEq, rgiVal, ulVals);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsFilterConj()
{
	SStatsFilterSTestCase rgstatsdisjtc[] =
	{
			{"../data/dxl/statistics/NestedPred-Input-9.xml", "../data/dxl/statistics/NestedPred-Output-9.xml", PstatspredConj},
	};

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatsdisjtc);

	return EresUnittest_CStatistics(rgstatsdisjtc, ulTestCases);
}

// create conjunctive predicate
CStatsPred *
CFilterCardinalityTest::PstatspredConj
	(
	CMemoryPool *mp
	)
{
	CWStringDynamic *pstrW = GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("AAAABXc="));

	// predicate is a == 'w' AND b == 2001 AND c > 0
	CStatsPredPtrArry *pdrgpstatspredConj3 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(594, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrW, 160621100)));
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(592, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2001)));
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(593, CStatsPred::EstatscmptG, CTestUtils::PpointInt4(mp, 0)));

	GPOS_DELETE(pstrW);

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj3);
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsNestedPred()
{
	SStatsFilterSTestCase rgstatsdisjtc[] =
	{
	    {"../data/dxl/statistics/NestedPred-Input-1.xml", "../data/dxl/statistics/NestedPred-Output-1.xml", PstatspredNestedPredDiffCol1},
		{"../data/dxl/statistics/NestedPred-Input-1.xml", "../data/dxl/statistics/NestedPred-Output-1.xml", PstatspredNestedPredDiffCol2},
		{"../data/dxl/statistics/NestedPred-Input-2.xml", "../data/dxl/statistics/NestedPred-Output-2.xml", PstatspredNestedPredCommonCol1},
		{"../data/dxl/statistics/NestedPred-Input-1.xml", "../data/dxl/statistics/NestedPred-Output-3.xml", PstatspredNestedSharedCol},
		{"../data/dxl/statistics/NestedPred-Input-3.xml", "../data/dxl/statistics/NestedPred-Output-4.xml", PstatspredDisjOverConjSameCol1},
		{"../data/dxl/statistics/NestedPred-Input-3.xml", "../data/dxl/statistics/NestedPred-Input-3.xml", PstatspredDisjOverConjSameCol2},
		{"../data/dxl/statistics/NestedPred-Input-1.xml", "../data/dxl/statistics/NestedPred-Output-5.xml", PstatspredDisjOverConjDifferentCol1},
		{"../data/dxl/statistics/NestedPred-Input-1.xml", "../data/dxl/statistics/NestedPred-Output-6.xml", PstatspredDisjOverConjMultipleIdenticalCols},
		{"../data/dxl/statistics/NestedPred-Input-2.xml", "../data/dxl/statistics/NestedPred-Output-7.xml", PstatspredNestedPredCommonCol2},
		{"../data/dxl/statistics/NestedPred-Input-8.xml", "../data/dxl/statistics/NestedPred-Output-8.xml", PstatspredDisjOverConjSameCol3},
		{"../data/dxl/statistics/NestedPred-Input-10.xml", "../data/dxl/statistics/NestedPred-Output-10.xml", PstatspredDisjOverConjSameCol4},
	};

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatsdisjtc);

	return EresUnittest_CStatistics(rgstatsdisjtc, ulTestCases);
}

//		Create nested AND and OR predicates where the AND and OR predicates
//		are on different columns
CStatsPred *
CFilterCardinalityTest::PstatspredNestedPredDiffCol1
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 <> 3
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptNEq, CTestUtils::PpointInt4(mp, 3)));

	// predicate col_2 in (15, 20, 22, 24, 31, 39, 42, 46);
	INT rgiVal[] = {15, 20, 22, 24, 31, 39, 42, 46};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 2, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	CStatsPredDisj *disjunctive_pred_stats = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
	pdrgpstatspredConj->Append(disjunctive_pred_stats);

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);
}

// create nested AND and OR predicates where the AND and OR predicates
// are on different columns. note: the order of the predicates in
// reversed as in PstatspredNestedPredDiffCol1
CStatsPred *
CFilterCardinalityTest::PstatspredNestedPredDiffCol2
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_2 in (15, 20, 22, 24, 31, 39, 42, 46);
	INT rgiVal[] = {15, 20, 22, 24, 31, 39, 42, 46};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 2, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	CStatsPredDisj *disjunctive_pred_stats = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
	pdrgpstatspredConj->Append(disjunctive_pred_stats);

	// predicate col_1 <> 3
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptNEq, CTestUtils::PpointInt4(mp, 3)));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);
}

// create nested AND and OR predicates where the AND and OR predicates
// are on the same columns
CStatsPred *
CFilterCardinalityTest::PstatspredNestedPredCommonCol1
	(
	CMemoryPool *mp
	)
{
	// predicate is col_2 in (39, 31, 24, 22, 46, 20, 42, 15) AND col_2 == 2
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_2 in (15, 20, 22, 24, 31, 39, 42, 46);
	INT rgiVal[] = {15, 20, 22, 24, 31, 39, 42, 46};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 2, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	CStatsPredDisj *disjunctive_pred_stats = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
	pdrgpstatspredConj->Append(disjunctive_pred_stats);

	// predicate col_2 == 2
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2)));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);
}

// create nested AND and OR predicates where the AND and OR predicates
// are on the same columns
CStatsPred *
CFilterCardinalityTest::PstatspredNestedPredCommonCol2
	(
	CMemoryPool *mp
	)
{
	// predicate is col_2 in (2, 39, 31, 24, 22, 46, 20, 42, 15) AND col_2 == 2
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// IN predicate: col_2 in (2, 39, 31, 24, 22, 46, 20, 42, 15);
	INT rgiVal[] = {2, 15, 20, 22, 24, 31, 39, 42, 46};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 2, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	CStatsPredDisj *disjunctive_pred_stats = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
	pdrgpstatspredConj->Append(disjunctive_pred_stats);

	// predicate col_2 == 2
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2)));

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);
}

// create nested AND and OR predicates where the AND and OR predicates
// share common columns
CStatsPred *
CFilterCardinalityTest::PstatspredNestedSharedCol
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 <> 3
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptNEq, CTestUtils::PpointInt4(mp, 3)));

	// predicate col_2 in (15, 20, 22, 24, 31, 39, 42, 46) OR (col_1 == 4));

	INT rgiVal[] = {15, 20, 22, 24, 31, 39, 42, 46};
	const ULONG ulVals = GPOS_ARRAY_SIZE(rgiVal);
	CStatsPredPtrArry *pdrgpstatspredDisj = PdrgpstatspredInteger(mp, 2, CStatsPred::EstatscmptEq, rgiVal, ulVals);

	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 4)));

	CStatsPredDisj *disjunctive_pred_stats = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
	pdrgpstatspredConj->Append(disjunctive_pred_stats);

	return GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);
}

// create nested AND and OR predicates where the AND and OR predicates share common columns
CStatsPred *
CFilterCardinalityTest::PstatspredDisjOverConjSameCol1
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 = 3 AND col_1 >=3
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 3)));
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptGEq, CTestUtils::PpointInt4(mp, 3)));

	CStatsPredConj *conjunctive_pred_stats = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);

	// predicate (col_1 = 1);
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 1)));
	pdrgpstatspredDisj->Append(conjunctive_pred_stats);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create nested AND and OR predicates where the AND and OR predicates share common columns
CStatsPred *
CFilterCardinalityTest::PstatspredDisjOverConjSameCol2
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 <= 5 AND col_1 >=1
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptLEq, CTestUtils::PpointInt4(mp, 5)));
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptGEq, CTestUtils::PpointInt4(mp, 1)));

	CStatsPredConj *conjunctive_pred_stats = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);

	// predicate (col_1 = 1);
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 1)));
	pdrgpstatspredDisj->Append(conjunctive_pred_stats);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create disjunctive predicate over conjunctions on same columns
CStatsPred *
CFilterCardinalityTest::PstatspredDisjOverConjSameCol3
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	CWStringDynamic *pstrS = GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("AAAABXM="));
	CWStringDynamic *pstrW = GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("AAAABXc="));

	// predicate is a == 's' AND b == 2001
	CStatsPredPtrArry *pdrgpstatspredConj1 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(142, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrS, 160588332)));
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(113, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2001)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj1));

	// predicate is a == 's' AND b == 2002
	CStatsPredPtrArry *pdrgpstatspredConj2 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj2->Append(GPOS_NEW(mp) CStatsPredPoint(142, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrS, 160588332)));
	pdrgpstatspredConj2->Append(GPOS_NEW(mp) CStatsPredPoint(113, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2002)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj2));

	// predicate is a == 'w' AND b == 2001
	CStatsPredPtrArry *pdrgpstatspredConj3 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(142, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrW, 160621100)));
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(113, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2001)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj3));

	// predicate is a == 'w' AND b == 2002
	CStatsPredPtrArry *pdrgpstatspredConj4 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj4->Append(GPOS_NEW(mp) CStatsPredPoint(142, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrW, 160621100)));
	pdrgpstatspredConj4->Append(GPOS_NEW(mp) CStatsPredPoint(113, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2002)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj4));

	GPOS_DELETE(pstrS);
	GPOS_DELETE(pstrW);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create disjunctive predicate over conjunctions on same columns
CStatsPred *
CFilterCardinalityTest::PstatspredDisjOverConjSameCol4
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	CWStringDynamic *pstrS = GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("AAAABXM="));
	CWStringDynamic *pstrW = GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("AAAABXc="));

	// predicate is a == 's' AND b == 2001 AND c > 0
	CStatsPredPtrArry *pdrgpstatspredConj1 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(91, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrS, 160588332)));
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(61, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2001)));
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(90, CStatsPred::EstatscmptG, CTestUtils::PpointInt4(mp, 0)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj1));

	// predicate is a == 's' AND b == 2002
	CStatsPredPtrArry *pdrgpstatspredConj2 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj2->Append(GPOS_NEW(mp) CStatsPredPoint(91, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrS, 160588332)));
	pdrgpstatspredConj2->Append(GPOS_NEW(mp) CStatsPredPoint(61, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2002)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj2));

	// predicate is a == 'w' AND b == 2001 AND c > 0
	CStatsPredPtrArry *pdrgpstatspredConj3 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(91, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrW, 160621100)));
	pdrgpstatspredConj3->Append(GPOS_NEW(mp) CStatsPredPoint(61, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2001)));
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(90, CStatsPred::EstatscmptG, CTestUtils::PpointInt4(mp, 0)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj3));

	// predicate is a == 'w' AND b == 2002
	CStatsPredPtrArry *pdrgpstatspredConj4 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredConj4->Append(GPOS_NEW(mp) CStatsPredPoint(91, CStatsPred::EstatscmptEq, CCardinalityTestUtils::PpointGeneric(mp, GPDB_TEXT, pstrW, 160621100)));
	pdrgpstatspredConj4->Append(GPOS_NEW(mp) CStatsPredPoint(61, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2002)));
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj4));

	GPOS_DELETE(pstrS);
	GPOS_DELETE(pstrW);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create nested AND and OR predicates where the AND and OR predicates share common columns
CStatsPred *
CFilterCardinalityTest::PstatspredDisjOverConjDifferentCol1
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 = 3 AND col_2 >=3
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 3)));
	pdrgpstatspredConj->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptGEq, CTestUtils::PpointInt4(mp, 3)));

	CStatsPredConj *conjunctive_pred_stats = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj);

	// predicate (col_1 = 1);
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspredDisj->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 1)));
	pdrgpstatspredDisj->Append(conjunctive_pred_stats);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// create nested AND and OR predicates where the AND and OR predicates
CStatsPred *
CFilterCardinalityTest::PstatspredDisjOverConjMultipleIdenticalCols
	(
	CMemoryPool *mp
	)
{
	CStatsPredPtrArry *pdrgpstatspredConj1 = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 = 1 AND col_2 = 1
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 1)));
	pdrgpstatspredConj1->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 1)));

	CStatsPredConj *pstatspredConj1 = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj1);

	CStatsPredPtrArry *pdrgpstatspredConj2 = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	// predicate col_1 = 2 AND col_2 = 2
	pdrgpstatspredConj2->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2)));
	pdrgpstatspredConj2->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 2)));

	CStatsPredConj *pstatspredConj2 = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspredConj2);
	CStatsPredPtrArry *pdrgpstatspredDisj = GPOS_NEW(mp) CStatsPredPtrArry(mp);

	pdrgpstatspredDisj->Append(pstatspredConj1);
	pdrgpstatspredDisj->Append(pstatspredConj2);

	return GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspredDisj);
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsBasicsFromDXLNumeric()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	SStatsCmpValElem rgStatsCmpValElem[] =
	{
		{CStatsPred::EstatscmptL,    GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},
		{CStatsPred::EstatscmptLEq,  GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},
		{CStatsPred::EstatscmptEq,   GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},

		{CStatsPred::EstatscmptG,    GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
		{CStatsPred::EstatscmptGEq,  GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
		{CStatsPred::EstatscmptEq,   GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},

		{CStatsPred::EstatscmptL,    GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},
		{CStatsPred::EstatscmptLEq,  GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},
		{CStatsPred::EstatscmptEq,   GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},
		{CStatsPred::EstatscmptG,    GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},
		{CStatsPred::EstatscmptGEq,  GPOS_WSZ_LIT("AAAACgAAAgABAA=="), CDouble(1.0)},

		{CStatsPred::EstatscmptL,    GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
		{CStatsPred::EstatscmptLEq,  GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
		{CStatsPred::EstatscmptG,    GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
		{CStatsPred::EstatscmptGEq,  GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
		{CStatsPred::EstatscmptEq,   GPOS_WSZ_LIT("AAAACgAAAgAyAA=="), CDouble(50.0)},
	};

	const ULONG length = GPOS_ARRAY_SIZE(rgStatsCmpValElem);
	GPOS_ASSERT(length == GPOS_ARRAY_SIZE(rgtcStatistics));
	for (ULONG ul = 0; ul < length; ul++)
	{
		// read input DXL file
		CHAR *szDXLInput = CDXLUtils::Read(mp, rgtcStatistics[ul].szInputFile);
		// read output DXL file
		CHAR *szDXLOutput = CDXLUtils::Read(mp, rgtcStatistics[ul].szOutputFile);

		GPOS_CHECK_ABORT;

		CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
		// parse the statistics objects
		CDXLStatsDerivedRelationArray *dxl_derived_rel_stats_array = CDXLUtils::ParseDXLToStatsDerivedRelArray(mp, szDXLInput, NULL);
		CStatisticsArray *pdrgpstatBefore = CDXLUtils::ParseDXLToOptimizerStatisticObjArray(mp, md_accessor, dxl_derived_rel_stats_array);
		dxl_derived_rel_stats_array->Release();

		GPOS_ASSERT(NULL != pdrgpstatBefore);

		GPOS_CHECK_ABORT;

		SStatsCmpValElem statsCmpValElem = rgStatsCmpValElem[ul];

		CStatsPredPtrArry *pdrgpstatspred = PdrgppredfilterNumeric(mp, 1 /*colid*/, statsCmpValElem);
		CStatsPredConj *pred_stats = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred);
		GPOS_RESULT eres = EresUnittest_CStatisticsCompare
							(
							mp,
							md_accessor,
							pdrgpstatBefore,
							pred_stats,
							szDXLOutput,
							true /*fApplyTwice*/
							);

		// clean up
		pdrgpstatBefore->Release();
		pred_stats->Release();
		GPOS_DELETE_ARRAY(szDXLInput);
		GPOS_DELETE_ARRAY(szDXLOutput);

		if (GPOS_OK != eres)
		{
			return eres;
		}
	}

	return GPOS_OK;
}

// generate an array of filter given a column identifier, comparison type,
// and array of integer point
CStatsPredPtrArry *
CFilterCardinalityTest::PdrgpstatspredInteger
	(
	CMemoryPool *mp,
	ULONG colid,
	CStatsPred::EStatsCmpType stats_cmp_type,
	INT *piVals,
	ULONG ulVals
	)
{
	GPOS_ASSERT(0 < ulVals);

	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	for (ULONG ul = 0; ul < ulVals; ul++)
	{
		pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(colid, stats_cmp_type, CTestUtils::PpointInt4(mp, piVals[ul])));
	}

	return pdrgpstatspred;
}

// generate a numeric filter on the column specified and the literal value
CStatsPredPtrArry *
CFilterCardinalityTest::PdrgppredfilterNumeric
	(
	CMemoryPool *mp,
	ULONG colid,
	SStatsCmpValElem statsCmpValElem
	)
{
	// create a filter
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	CWStringDynamic *pstrNumeric = GPOS_NEW(mp) CWStringDynamic(mp, statsCmpValElem.m_wsz);
	CStatsPredPoint *pred_stats = GPOS_NEW(mp) CStatsPredPoint
													(
													colid,
													statsCmpValElem.m_stats_cmp_type,
													CCardinalityTestUtils::PpointNumeric(mp, pstrNumeric, statsCmpValElem.m_value)
													);
	pdrgpstatspred->Append(pred_stats);
	GPOS_DELETE(pstrNumeric);

	return pdrgpstatspred;
}

// reads a DXL document, generates the statistics object, performs a
// filter operation on it, serializes it into a DXL document and
// compares the generated DXL document with the expected DXL document.
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsBasicsFromDXL()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic str(mp);
	COstreamString oss(&str);

	// read input DXL file
	CHAR *szDXLInput = CDXLUtils::Read(mp, szInputDXLFileName);
	// read output DXL file
	CHAR *szDXLOutput = CDXLUtils::Read(mp, szOutputDXLFileName);

	GPOS_CHECK_ABORT;

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// parse the statistics objects
	CDXLStatsDerivedRelationArray *dxl_derived_rel_stats_array = CDXLUtils::ParseDXLToStatsDerivedRelArray(mp, szDXLInput, NULL);
	CStatisticsArray *pdrgpstatsBefore = CDXLUtils::ParseDXLToOptimizerStatisticObjArray
									(
									mp,
									md_accessor,
									dxl_derived_rel_stats_array
									);
	dxl_derived_rel_stats_array->Release();
	GPOS_ASSERT(NULL != pdrgpstatsBefore);

	GPOS_CHECK_ABORT;

	// create a filter
	CStatsPredConj *pred_stats = GPOS_NEW(mp) CStatsPredConj(CStatisticsTest::Pdrgpstatspred2(mp));
	GPOS_RESULT eres = EresUnittest_CStatisticsCompare
							(
							mp,
							md_accessor,
							pdrgpstatsBefore,
							pred_stats,
							szDXLOutput
							);

	// clean up
	pdrgpstatsBefore->Release();
	pred_stats->Release();
	GPOS_DELETE_ARRAY(szDXLInput);
	GPOS_DELETE_ARRAY(szDXLOutput);

	return eres;
}

// performs a filter operation on it, serializes it into a DXL document
// and compares the generated DXL document with the expected DXL document
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsCompare
	(
	CMemoryPool *mp,
	CMDAccessor *md_accessor,
	CStatisticsArray *pdrgpstatBefore,
	CStatsPred *pred_stats,
	const CHAR *szDXLOutput,
	BOOL fApplyTwice
	)
{
	CWStringDynamic str(mp);
	COstreamString oss(&str);

	CStatistics *input_stats = (* pdrgpstatBefore)[0];

	GPOS_TRACE(GPOS_WSZ_LIT("Statistics before"));
	CCardinalityTestUtils::PrintStats(mp, input_stats);

	CStatistics *pstatsOutput = CFilterStatsProcessor::MakeStatsFilter(mp, input_stats, pred_stats, true /* do_cap_NDVs */);

	GPOS_TRACE(GPOS_WSZ_LIT("Statistics after"));
	CCardinalityTestUtils::PrintStats(mp, pstatsOutput);

	// output array of stats objects
	CStatisticsArray *pdrgpstatOutput = GPOS_NEW(mp) CStatisticsArray(mp);
	pdrgpstatOutput->Append(pstatsOutput);

	oss << "Serializing Input Statistics Objects (Before Filter)" << std::endl;
	CWStringDynamic *pstrInput = CDXLUtils::SerializeStatistics
												(
												mp,
												md_accessor,
												pdrgpstatBefore,
												true /*serialize_header_footer*/,
												true /*indentation*/
												);
	GPOS_TRACE(pstrInput->GetBuffer());
	GPOS_DELETE(pstrInput);

	oss << "Serializing Output Statistics Objects (After Filter)" << std::endl;
	CWStringDynamic *pstrOutput = CDXLUtils::SerializeStatistics
												(
												mp,
												md_accessor,
												pdrgpstatOutput,
												true /*serialize_header_footer*/,
												true /*indentation*/
												);
	GPOS_TRACE(pstrOutput->GetBuffer());

	CWStringDynamic dstrExpected(mp);
	dstrExpected.AppendFormat(GPOS_WSZ_LIT("%s"), szDXLOutput);

	GPOS_RESULT eres = CTestUtils::EresCompare
							(
							oss,
							pstrOutput,
							&dstrExpected,
							false /* ignore mismatch */
							);

	if (fApplyTwice && GPOS_OK == eres)
	{
		CStatistics *pstatsOutput2 = CFilterStatsProcessor::MakeStatsFilter(mp, pstatsOutput, pred_stats, true /* do_cap_NDVs */);
		pstatsOutput2->Rows();
		GPOS_TRACE(GPOS_WSZ_LIT("Statistics after another filter"));
		CCardinalityTestUtils::PrintStats(mp, pstatsOutput2);

		// output array of stats objects
		CStatisticsArray *pdrgpstatOutput2 = GPOS_NEW(mp) CStatisticsArray(mp);
		pdrgpstatOutput2->Append(pstatsOutput2);

		CWStringDynamic *pstrOutput2 = CDXLUtils::SerializeStatistics
													(
													mp,
													md_accessor,
													pdrgpstatOutput2,
													true /*serialize_header_footer*/,
													true /*indentation*/
													);
		eres = CTestUtils::EresCompare
					(
					oss,
					pstrOutput2,
					&dstrExpected,
					false /* ignore mismatch */
					);

		pdrgpstatOutput2->Release();
		GPOS_DELETE(pstrOutput2);
	}

	pdrgpstatOutput->Release();
	GPOS_DELETE(pstrOutput);

	return eres;
}

// test for accumulating cardinality in disjunctive and conjunctive predicates
GPOS_RESULT
CFilterCardinalityTest::EresUnittest_CStatisticsAccumulateCard()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// create hash map from colid -> histogram
	UlongToHistogramMap *col_histogram_mapping = GPOS_NEW(mp) UlongToHistogramMap(mp);

	// array capturing columns for which width information is available
	UlongToDoubleMap *colid_width_mapping = GPOS_NEW(mp) UlongToDoubleMap(mp);

	const ULONG num_cols = 3;
	for (ULONG ul = 0; ul < num_cols; ul ++)
	{
		// generate histogram of the form [0, 10), [10, 20), [20, 30), [80, 90), [100,100]
		col_histogram_mapping->Insert(GPOS_NEW(mp) ULONG(ul), CCardinalityTestUtils::PhistExampleInt4(mp));

		// width for int
		colid_width_mapping->Insert(GPOS_NEW(mp) ULONG(ul), GPOS_NEW(mp) CDouble(4.0));
	}

	CStatistics *stats = GPOS_NEW(mp) CStatistics
									(
									mp,
									col_histogram_mapping,
									colid_width_mapping,
									CDouble(1000.0) /* rows */,
									false /* is_empty() */
									);
	GPOS_TRACE(GPOS_WSZ_LIT("\nOriginal Stats:\n"));
	CCardinalityTestUtils::PrintStats(mp, stats);

	// (1)
	// create disjunctive filter
	CStatsPredPtrArry *pdrgpstatspred = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(0, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 5)));
	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));
	pdrgpstatspred->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));
	CStatsPredDisj *disjunctive_pred_stats = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspred);

	// apply filter and print resulting stats
	CStatistics *pstats1 = CFilterStatsProcessor::MakeStatsFilter(mp, stats, disjunctive_pred_stats, true /* do_cap_NDVs */);
	CDouble num_rows1 = pstats1->Rows();
	GPOS_TRACE(GPOS_WSZ_LIT("\n\nStats after disjunctive filter [Col0=5 OR Col1=200 OR Col2=200]:\n"));
	CCardinalityTestUtils::PrintStats(mp, pstats1);

	disjunctive_pred_stats->Release();

	// (2)
	// create point filter
	CStatsPredPtrArry *pdrgpstatspred1 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspred1->Append(GPOS_NEW(mp) CStatsPredPoint(0, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 5)));
	CStatsPredConj *pstatspredConj1 = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred1);

	// apply filter and print resulting stats
	CStatistics *pstats2 = CFilterStatsProcessor::MakeStatsFilter(mp, stats, pstatspredConj1, true /* do_cap_NDVs */);
	CDouble num_rows2 = pstats2->Rows();
	GPOS_TRACE(GPOS_WSZ_LIT("\n\nStats after point filter [Col0=5]:\n"));
	CCardinalityTestUtils::PrintStats(mp, pstats2);

	pstatspredConj1->Release();

	GPOS_RTL_ASSERT(num_rows1 - num_rows2 < 10 && "Disjunctive filter and point filter have very different row estimates");

	// (3)
	// create conjunctive filter
	CStatsPredPtrArry *pdrgpstatspred2 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspred2->Append(GPOS_NEW(mp) CStatsPredPoint(0, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 5)));
	pdrgpstatspred2->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));
	pdrgpstatspred2->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));

	CStatsPredConj *pstatspredConj2 = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred2);

	// apply filter and print resulting stats
	CStatistics *pstats3 = CFilterStatsProcessor::MakeStatsFilter(mp, stats, pstatspredConj2, true /* do_cap_NDVs */);
	CDouble dRows3 = pstats3->Rows();
	GPOS_TRACE(GPOS_WSZ_LIT("\n\nStats after conjunctive filter [Col0=5 AND Col1=200 AND Col2=200]:\n"));
	CCardinalityTestUtils::PrintStats(mp, pstats3);

	pstatspredConj2->Release();
	GPOS_RTL_ASSERT(dRows3 < num_rows2  && "Conjunctive filter passes more rows than than point filter");

	// (4)
	// create selective disjunctive filter that pass no rows
	CStatsPredPtrArry *pdrgpstatspred3 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspred3->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));
	pdrgpstatspred3->Append(GPOS_NEW(mp) CStatsPredPoint(2, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));
	CStatsPredDisj *pstatspredDisj1 = GPOS_NEW(mp) CStatsPredDisj(pdrgpstatspred3);

	// apply filter and print resulting stats
	CStatistics *pstats4 = CFilterStatsProcessor::MakeStatsFilter(mp, stats, pstatspredDisj1, true /* do_cap_NDVs */);
	CDouble dRows4 = pstats4->Rows();
	GPOS_TRACE(GPOS_WSZ_LIT("\n\nStats after disjunctive filter [Col1=200 OR Col2=200]:\n"));
	CCardinalityTestUtils::PrintStats(mp, pstats4);

	pstatspredDisj1->Release();

	GPOS_RTL_ASSERT(dRows4 < num_rows2  && "Selective disjunctive filter passes more rows than than point filter");

	// (5)
	// create selective conjunctive filter that pass no rows
	CStatsPredPtrArry *pdrgpstatspred4 = GPOS_NEW(mp) CStatsPredPtrArry(mp);
	pdrgpstatspred4->Append(GPOS_NEW(mp) CStatsPredPoint(0, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 5)));
	pdrgpstatspred4->Append(GPOS_NEW(mp) CStatsPredPoint(1, CStatsPred::EstatscmptEq, CTestUtils::PpointInt4(mp, 200)));
	CStatsPredConj *pstatspredConj3 = GPOS_NEW(mp) CStatsPredConj(pdrgpstatspred4);

	// apply filter and print resulting stats
	CStatistics *pstats5 = CFilterStatsProcessor::MakeStatsFilter(mp, stats, pstatspredConj3, true /* do_cap_NDVs */);
	CDouble dRows5 = pstats5->Rows();
	GPOS_TRACE(GPOS_WSZ_LIT("\n\nStats after conjunctive filter [Col0=5 AND Col1=200]:\n"));
	CCardinalityTestUtils::PrintStats(mp, pstats5);

	pstatspredConj3->Release();

	GPOS_RTL_ASSERT(dRows5 < num_rows2  && "Selective conjunctive filter passes more rows than than point filter");

	// clean up
	stats->Release();
	pstats1->Release();
	pstats2->Release();
	pstats3->Release();
	pstats4->Release();
	pstats5->Release();

	return GPOS_OK;
}

// EOF
