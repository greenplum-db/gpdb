//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal, Inc.
//
//	@filename:
//		CJoinCardinalityTest.cpp
//
//	@doc:
//		Test for join cardinality estimation
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/error/CAutoTrace.h"
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/statistics/CStatisticsUtils.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/dxl/statistics/CJoinCardinalityTest.h"
#include "unittest/gpopt/CTestUtils.h"

// unittest for join cardinality estimation
GPOS_RESULT
CJoinCardinalityTest::EresUnittest()
{
	// tests that use shared optimization context
	CUnittest rgutSharedOptCtxt[] =
		{
		GPOS_UNITTEST_FUNC(CJoinCardinalityTest::EresUnittest_Join),
		GPOS_UNITTEST_FUNC(CJoinCardinalityTest::EresUnittest_JoinNDVRemain),
		};

	// run tests with shared optimization context first
	GPOS_RESULT eres = GPOS_FAILED;

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL /* pceeval */,
					CTestUtils::GetCostModel(mp)
					);

	eres = CUnittest::EresExecute(rgutSharedOptCtxt, GPOS_ARRAY_SIZE(rgutSharedOptCtxt));

	return eres;
}

//	test join cardinality estimation over histograms with NDVRemain information
GPOS_RESULT
CJoinCardinalityTest::EresUnittest_JoinNDVRemain()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	SHistogramTestCase rghisttc[] =
	{
		{0, 0, false, 0}, // empty histogram
		{10, 100, false, 0},  // distinct values only in buckets
		{0, 0, false, 1000},   // distinct values only in NDVRemain
		{5, 100, false, 500} // distinct values spread in both buckets and NDVRemain
	};

	UlongToHistogramMap *col_histogram_mapping = GPOS_NEW(mp) UlongToHistogramMap(mp);

	const ULONG ulHist = GPOS_ARRAY_SIZE(rghisttc);
	for (ULONG ul1 = 0; ul1 < ulHist; ul1++)
	{
		SHistogramTestCase elem = rghisttc[ul1];

		ULONG num_of_buckets = elem.m_num_of_buckets;
		CDouble dNDVPerBucket = elem.m_dNDVPerBucket;
		BOOL fNullFreq = elem.m_fNullFreq;
		CDouble num_NDV_remain = elem.m_dNDVRemain;

		CHistogram *histogram = CCardinalityTestUtils::PhistInt4Remain(mp, num_of_buckets, dNDVPerBucket, fNullFreq, num_NDV_remain);
#ifdef GPOS_DEBUG
			BOOL result =
#endif // GPOS_DEBUG
		col_histogram_mapping->Insert(GPOS_NEW(mp) ULONG(ul1), histogram);
		GPOS_ASSERT(result);
	}

	SStatsJoinNDVRemainTestCase rgjoinndvrtc[] =
	{
		// cases where we are joining with an empty histogram
	    // first two columns refer to the histogram entries that are joining
		{0, 0, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},
		{0, 1, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},
		{0, 2, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},
		{0, 3, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},

		{1, 0, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},
		{2, 0, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},
		{3, 0, 0, CDouble(0.0), CDouble(0.0), CDouble(0.0)},

		// cases where one or more input histogram has only buckets and no remaining NDV information
		{1, 1, 10, CDouble(1000.00), CDouble(0.0), CDouble(0.0)},
		{1, 3, 5, CDouble(500.00), CDouble(500.0), CDouble(0.333333)},
		{3, 1, 5, CDouble(500.00), CDouble(500.0), CDouble(0.333333)},

		// cases where for one or more input histogram has only remaining NDV information and no buckets
		{1, 2, 0, CDouble(0.0), CDouble(1000.0), CDouble(1.0)},
		{2, 1, 0, CDouble(0.0), CDouble(1000.0), CDouble(1.0)},
		{2, 2, 0, CDouble(0.0), CDouble(1000.0), CDouble(1.0)},
		{2, 3, 0, CDouble(0.0), CDouble(1000.0), CDouble(1.0)},
		{3, 2, 0, CDouble(0.0), CDouble(1000.0), CDouble(1.0)},

		// cases where both buckets and NDV remain information available for both inputs
		{3, 3, 5, CDouble(500.0), CDouble(500.0), CDouble(0.5)},
	};

	GPOS_RESULT eres = GPOS_OK;
	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgjoinndvrtc);
	for (ULONG ul2 = 0; ul2 < ulTestCases && (GPOS_FAILED != eres); ul2++)
	{
		SStatsJoinNDVRemainTestCase elem = rgjoinndvrtc[ul2];
		ULONG colid1 = elem.m_ulCol1;
		ULONG colid2 = elem.m_ulCol2;
		CHistogram *histogram1 = col_histogram_mapping->Find(&colid1);
		CHistogram *histogram2 = col_histogram_mapping->Find(&colid2);

		CHistogram *join_histogram = histogram1->MakeJoinHistogram(CStatsPred::EstatscmptEq, histogram2);

		{
			CAutoTrace at(mp);
			at.Os() <<  std::endl << "Input Histogram 1" <<  std::endl;
			histogram1->OsPrint(at.Os());
			at.Os() << "Input Histogram 2" <<  std::endl;
			histogram2->OsPrint(at.Os());
			at.Os() << "Join Histogram" <<  std::endl;
			join_histogram->OsPrint(at.Os());

			join_histogram->NormalizeHistogram();

			at.Os() <<  std::endl << "Normalized Join Histogram" <<  std::endl;
			join_histogram->OsPrint(at.Os());
		}

		ULONG ulBucketsJoin = elem.m_ulBucketsJoin;
		CDouble dNDVBucketsJoin = elem.m_dNDVBucketsJoin;
		CDouble dNDVRemainJoin = elem.m_dNDVRemainJoin;
		CDouble dFreqRemainJoin = elem.m_dFreqRemainJoin;

		CDouble dDiffNDVJoin(fabs((dNDVBucketsJoin - CStatisticsUtils::GetNumDistinct(join_histogram->GetBuckets())).Get()));
		CDouble dDiffNDVRemainJoin(fabs((dNDVRemainJoin - join_histogram->GetDistinctRemain()).Get()));
		CDouble dDiffFreqRemainJoin(fabs((dFreqRemainJoin - join_histogram->GetFreqRemain()).Get()));

		if (join_histogram->GetNumBuckets() != ulBucketsJoin || (dDiffNDVJoin > CStatistics::Epsilon)
			|| (dDiffNDVRemainJoin > CStatistics::Epsilon) || (dDiffFreqRemainJoin > CStatistics::Epsilon))
		{
			eres = GPOS_FAILED;
		}

		GPOS_DELETE(join_histogram);
	}
	// clean up
	col_histogram_mapping->Release();

	return eres;
}

//	join buckets tests
GPOS_RESULT
CJoinCardinalityTest::EresUnittest_Join()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	SStatsJoinSTestCase rgstatsjointc[] =
	{
		{"../data/dxl/statistics/Join-Statistics-Input.xml", "../data/dxl/statistics/Join-Statistics-Output.xml", false, PdrgpstatspredjoinMultiplePredicates},
		{"../data/dxl/statistics/Join-Statistics-Input-Null-Bucket.xml", "../data/dxl/statistics/Join-Statistics-Output-Null-Bucket.xml", false, PdrgpstatspredjoinNullableCols},
		{"../data/dxl/statistics/LOJ-Input.xml", "../data/dxl/statistics/LOJ-Output.xml", true, PdrgpstatspredjoinNullableCols},
		{"../data/dxl/statistics/Join-Statistics-Input-Only-Nulls.xml", "../data/dxl/statistics/Join-Statistics-Output-Only-Nulls.xml", false, PdrgpstatspredjoinNullableCols},
		{"../data/dxl/statistics/Join-Statistics-Input-Only-Nulls.xml", "../data/dxl/statistics/Join-Statistics-Output-LOJ-Only-Nulls.xml", true, PdrgpstatspredjoinNullableCols},
	    {"../data/dxl/statistics/Join-Statistics-DDistinct-Input.xml", "../data/dxl/statistics/Join-Statistics-DDistinct-Output.xml", false, PdrgpstatspredjoinSingleJoinPredicate},
	};

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	const IMDTypeInt4 *pmdtypeint4 = COptCtxt::PoctxtFromTLS()->Pmda()->PtMDType<IMDTypeInt4>();

	ULongPtrArray *cols = GPOS_NEW(mp) ULongPtrArray(mp);
	cols->Append(GPOS_NEW(mp) ULONG(0));
	cols->Append(GPOS_NEW(mp) ULONG(1));
	cols->Append(GPOS_NEW(mp) ULONG(2));
	cols->Append(GPOS_NEW(mp) ULONG(8));
	cols->Append(GPOS_NEW(mp) ULONG(16));
	cols->Append(GPOS_NEW(mp) ULONG(31));
	cols->Append(GPOS_NEW(mp) ULONG(32));
	cols->Append(GPOS_NEW(mp) ULONG(53));
	cols->Append(GPOS_NEW(mp) ULONG(54));

	for (ULONG ul  = 0; ul < cols->Size(); ul++)
	{
		ULONG id = *((*cols)[ul]);
		if (NULL == col_factory->LookupColRef(id))
		{
			// for this test the col name doesn't matter
			CWStringConst str(GPOS_WSZ_LIT("col"));
			// create column references for grouping columns
			(void) col_factory->PcrCreate
			(
			 pmdtypeint4,
			 default_type_modifier,
			 NULL,
			 ul /* attno */,
			 false /*IsNullable*/,
			 id,
			 CName(&str),
			 0
			 );
		}
	}
	cols->Release();

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatsjointc);
	for (ULONG ul = 0; ul < ulTestCases; ul++)
	{
		SStatsJoinSTestCase elem = rgstatsjointc[ul];

		// read input/output DXL file
		CHAR *szDXLInput = CDXLUtils::Read(mp, elem.m_szInputFile);
		CHAR *szDXLOutput = CDXLUtils::Read(mp, elem.m_szOutputFile);
		BOOL left_outer_join = elem.m_fLeftOuterJoin;

		GPOS_CHECK_ABORT;

		// parse the input statistics objects
		CDXLStatsDerivedRelationArray *dxl_derived_rel_stats_array = CDXLUtils::ParseDXLToStatsDerivedRelArray(mp, szDXLInput, NULL);
		CStatisticsArray *pdrgpstatBefore = CDXLUtils::ParseDXLToOptimizerStatisticObjArray(mp, md_accessor, dxl_derived_rel_stats_array);
		dxl_derived_rel_stats_array->Release();

		GPOS_ASSERT(NULL != pdrgpstatBefore);
		GPOS_ASSERT(2 == pdrgpstatBefore->Size());
		CStatistics *pstats1 = (*pdrgpstatBefore)[0];
		CStatistics *pstats2 = (*pdrgpstatBefore)[1];

		GPOS_CHECK_ABORT;

		// generate the join conditions
		FnPdrgpstatjoin *pf = elem.m_pf;
		GPOS_ASSERT(NULL != pf);
		CStatsPredJoinArray *join_preds_stats = pf(mp);

		// calculate the output stats
		CStatistics *pstatsOutput = NULL;
		if (left_outer_join)
		{
			pstatsOutput = pstats1->CalcLOJoinStats(mp, pstats2, join_preds_stats);
		}
		else
		{
			pstatsOutput = pstats1->CalcInnerJoinStats(mp, pstats2, join_preds_stats);
		}
		GPOS_ASSERT(NULL != pstatsOutput);

		CStatisticsArray *pdrgpstatOutput = GPOS_NEW(mp) CStatisticsArray(mp);
		pdrgpstatOutput->Append(pstatsOutput);

		// serialize and compare against expected stats
		CWStringDynamic *pstrOutput = CDXLUtils::SerializeStatistics
													(
													mp,
													md_accessor,
													pdrgpstatOutput,
													true /*serialize_header_footer*/,
													true /*indentation*/
													);
		CWStringDynamic dstrExpected(mp);
		dstrExpected.AppendFormat(GPOS_WSZ_LIT("%s"), szDXLOutput);

		GPOS_RESULT eres = GPOS_OK;
		CWStringDynamic str(mp);
		COstreamString oss(&str);

		// compare the two dxls
		if (!pstrOutput->Equals(&dstrExpected))
		{
			oss << "Output does not match expected DXL document" << std::endl;
			oss << "Actual: " << std::endl;
			oss << pstrOutput->GetBuffer() << std::endl;
			oss << "Expected: " << std::endl;
			oss << dstrExpected.GetBuffer() << std::endl;
			GPOS_TRACE(str.GetBuffer());

			eres = GPOS_FAILED;
		}

		// clean up
		pdrgpstatBefore->Release();
		pdrgpstatOutput->Release();
		join_preds_stats->Release();

		GPOS_DELETE_ARRAY(szDXLInput);
		GPOS_DELETE_ARRAY(szDXLOutput);
		GPOS_DELETE(pstrOutput);

		if (GPOS_FAILED == eres)
		{
			return eres;
		}
	}

	return GPOS_OK;
}

//	helper method to generate a single join predicate
CStatsPredJoinArray *
CJoinCardinalityTest::PdrgpstatspredjoinSingleJoinPredicate
	(
	CMemoryPool *mp
	)
{
	CStatsPredJoinArray *join_preds_stats = GPOS_NEW(mp) CStatsPredJoinArray(mp);
	join_preds_stats->Append(GPOS_NEW(mp) CStatsPredJoin(0, CStatsPred::EstatscmptEq, 8));

	return join_preds_stats;
}

//	helper method to generate generate multiple join predicates
CStatsPredJoinArray *
CJoinCardinalityTest::PdrgpstatspredjoinMultiplePredicates
	(
	CMemoryPool *mp
	)
{
	CStatsPredJoinArray *join_preds_stats = GPOS_NEW(mp) CStatsPredJoinArray(mp);
	join_preds_stats->Append(GPOS_NEW(mp) CStatsPredJoin(16, CStatsPred::EstatscmptEq, 32));
	join_preds_stats->Append(GPOS_NEW(mp) CStatsPredJoin(0, CStatsPred::EstatscmptEq, 31));
	join_preds_stats->Append(GPOS_NEW(mp) CStatsPredJoin(54, CStatsPred::EstatscmptEq, 32));
	join_preds_stats->Append(GPOS_NEW(mp) CStatsPredJoin(53, CStatsPred::EstatscmptEq, 31));

	return join_preds_stats;
}

// helper method to generate join predicate over columns that contain null values
CStatsPredJoinArray *
CJoinCardinalityTest::PdrgpstatspredjoinNullableCols
	(
	CMemoryPool *mp
	)
{
	CStatsPredJoinArray *join_preds_stats = GPOS_NEW(mp) CStatsPredJoinArray(mp);
	join_preds_stats->Append(GPOS_NEW(mp) CStatsPredJoin(1, CStatsPred::EstatscmptEq, 2));

	return join_preds_stats;
}

// EOF
