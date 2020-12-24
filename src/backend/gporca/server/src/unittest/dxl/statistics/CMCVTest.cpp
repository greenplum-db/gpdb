//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CHistogramTest.cpp
//
//	@doc:
//		Testing merging most common values (MCV) and histograms
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatisticsUtils.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/dxl/statistics/CMCVTest.h"
#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;

// DXL files
const CHAR *szMCVSortExpectedFileName =
	"../data/dxl/statistics/MCV-Sort-Output.xml";


// unittest for statistics objects
GPOS_RESULT
CMCVTest::EresUnittest()
{
	// tests that use shared optimization context
	CUnittest rgutSharedOptCtxt[] = {
		GPOS_UNITTEST_FUNC(CMCVTest::EresUnittest_SortInt4MCVs),
		GPOS_UNITTEST_FUNC(CMCVTest::EresUnittest_MergeHistMCV),
	};

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /* pceeval */,
					 CTestUtils::GetCostModel(mp));

	return CUnittest::EresExecute(rgutSharedOptCtxt,
								  GPOS_ARRAY_SIZE(rgutSharedOptCtxt));
}

// test sorting of Int4 MCVs and their associated frequencies
GPOS_RESULT
CMCVTest::EresUnittest_SortInt4MCVs()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	CMDIdGPDB *mdid = GPOS_NEW(mp) CMDIdGPDB(CMDIdGPDB::m_mdid_int4);
	const IMDType *pmdtype = md_accessor->RetrieveType(mdid);

	// create three integer MCVs
	CPoint *point1 = CTestUtils::PpointInt4(mp, 5);
	CPoint *point2 = CTestUtils::PpointInt4(mp, 1);
	CPoint *point3 = CTestUtils::PpointInt4(mp, 10);
	IDatumArray *pdrgpdatumMCV = GPOS_NEW(mp) IDatumArray(mp);
	IDatum *datum1 = point1->GetDatum();
	IDatum *datum2 = point2->GetDatum();
	IDatum *datum3 = point3->GetDatum();
	datum1->AddRef();
	datum2->AddRef();
	datum3->AddRef();
	pdrgpdatumMCV->Append(datum1);
	pdrgpdatumMCV->Append(datum2);
	pdrgpdatumMCV->Append(datum3);

	// create corresponding frequencies
	CDoubleArray *pdrgpdFreq = GPOS_NEW(mp) CDoubleArray(mp);
	// in GPDB, MCVs are stored in descending frequencies
	pdrgpdFreq->Append(GPOS_NEW(mp) CDouble(0.4));
	pdrgpdFreq->Append(GPOS_NEW(mp) CDouble(0.2));
	pdrgpdFreq->Append(GPOS_NEW(mp) CDouble(0.1));

	// exercise MCV sorting function
	CHistogram *phistMCV = CStatisticsUtils::TransformMCVToHist(
		mp, pmdtype, pdrgpdatumMCV, pdrgpdFreq, pdrgpdatumMCV->Size());

	// create hash map from colid -> histogram
	UlongToHistogramMap *col_histogram_mapping =
		GPOS_NEW(mp) UlongToHistogramMap(mp);

	// generate int histogram for column 1
	col_histogram_mapping->Insert(GPOS_NEW(mp) ULONG(1), phistMCV);

	// column width for int4
	UlongToDoubleMap *colid_width_mapping = GPOS_NEW(mp) UlongToDoubleMap(mp);
	colid_width_mapping->Insert(GPOS_NEW(mp) ULONG(1),
								GPOS_NEW(mp) CDouble(4.0));

	CStatistics *stats =
		GPOS_NEW(mp) CStatistics(mp, col_histogram_mapping, colid_width_mapping,
								 1000.0 /* rows */, false /* is_empty */
		);

	// put stats object in an array in order to serialize
	CStatisticsArray *pdrgpstats = GPOS_NEW(mp) CStatisticsArray(mp);
	pdrgpstats->Append(stats);

	// serialize stats object
	CWStringDynamic *pstrOutput =
		CDXLUtils::SerializeStatistics(mp, md_accessor, pdrgpstats, true, true);
	GPOS_TRACE(pstrOutput->GetBuffer());

	// get expected output
	CWStringDynamic str(mp);
	COstreamString oss(&str);
	CHAR *szDXLExpected = CDXLUtils::Read(mp, szMCVSortExpectedFileName);
	CWStringDynamic dstrExpected(mp);
	dstrExpected.AppendFormat(GPOS_WSZ_LIT("%s"), szDXLExpected);

	GPOS_RESULT eres =
		CTestUtils::EresCompare(oss, pstrOutput, &dstrExpected,
								false  // mismatch will not be ignored
		);

	// cleanup
	GPOS_DELETE(pstrOutput);
	GPOS_DELETE_ARRAY(szDXLExpected);
	pdrgpdatumMCV->Release();
	pdrgpdFreq->Release();
	pdrgpstats->Release();
	point1->Release();
	point2->Release();
	point3->Release();
	mdid->Release();

	return eres;
}

// test merging MCVs and histogram
GPOS_RESULT
CMCVTest::EresUnittest_MergeHistMCV()
{
	SMergeTestElem rgMergeTestElem[] = {
		{"../data/dxl/statistics/Merge-Input-MCV-Int.xml",
		 "../data/dxl/statistics/Merge-Input-Histogram-Int.xml",
		 "../data/dxl/statistics/Merge-Output-Int.xml"},

		{"../data/dxl/statistics/Merge-Input-MCV-Numeric.xml",
		 "../data/dxl/statistics/Merge-Input-Histogram-Numeric.xml",
		 "../data/dxl/statistics/Merge-Output-Numeric.xml"}};

	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG length = GPOS_ARRAY_SIZE(rgMergeTestElem);
	for (ULONG ul = 0; ul < length; ul++)
	{
		// read input MCVs DXL file
		CHAR *szDXLInputMCV =
			CDXLUtils::Read(mp, rgMergeTestElem[ul].szInputMCVFile);
		// read input histogram DXL file
		CHAR *szDXLInputHist =
			CDXLUtils::Read(mp, rgMergeTestElem[ul].szInputHistFile);

		GPOS_CHECK_ABORT;

		CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

		// parse the stats objects
		CDXLStatsDerivedRelationArray *pdrgpdxlstatsderrelMCV =
			CDXLUtils::ParseDXLToStatsDerivedRelArray(mp, szDXLInputMCV, NULL);
		CDXLStatsDerivedRelationArray *pdrgpdxlstatsderrelHist =
			CDXLUtils::ParseDXLToStatsDerivedRelArray(mp, szDXLInputHist, NULL);

		GPOS_CHECK_ABORT;

		CDXLStatsDerivedRelation *pdxlstatsderrelMCV =
			(*pdrgpdxlstatsderrelMCV)[0];
		const CDXLStatsDerivedColumnArray *pdrgpdxlstatsdercolMCV =
			pdxlstatsderrelMCV->GetDXLStatsDerivedColArray();
		CDXLStatsDerivedColumn *pdxlstatsdercolMCV =
			(*pdrgpdxlstatsdercolMCV)[0];
		CBucketArray *pdrgppbucketMCV = CDXLUtils::ParseDXLToBucketsArray(
			mp, md_accessor, pdxlstatsdercolMCV);
		CHistogram *phistMCV = GPOS_NEW(mp) CHistogram(mp, pdrgppbucketMCV);

		CDXLStatsDerivedRelation *pdxlstatsderrelHist =
			(*pdrgpdxlstatsderrelHist)[0];
		const CDXLStatsDerivedColumnArray *pdrgpdxlstatsdercolHist =
			pdxlstatsderrelHist->GetDXLStatsDerivedColArray();
		CDXLStatsDerivedColumn *pdxlstatsdercolHist =
			(*pdrgpdxlstatsdercolHist)[0];
		CBucketArray *pdrgppbucketHist = CDXLUtils::ParseDXLToBucketsArray(
			mp, md_accessor, pdxlstatsdercolHist);
		CHistogram *phistHist = GPOS_NEW(mp) CHistogram(mp, pdrgppbucketHist);

		GPOS_CHECK_ABORT;

		// exercise the merge
		CHistogram *phistMerged =
			CStatisticsUtils::MergeMCVHist(mp, phistMCV, phistHist);

		// create hash map from colid -> histogram
		UlongToHistogramMap *col_histogram_mapping =
			GPOS_NEW(mp) UlongToHistogramMap(mp);

		// generate int histogram for column 1
		ULONG colid = pdxlstatsdercolMCV->GetColId();
		col_histogram_mapping->Insert(GPOS_NEW(mp) ULONG(colid), phistMerged);

		// column width for int4
		UlongToDoubleMap *colid_width_mapping =
			GPOS_NEW(mp) UlongToDoubleMap(mp);
		CDouble width = pdxlstatsdercolMCV->Width();
		colid_width_mapping->Insert(GPOS_NEW(mp) ULONG(colid),
									GPOS_NEW(mp) CDouble(width));

		CStatistics *stats = GPOS_NEW(mp) CStatistics(
			mp, col_histogram_mapping, colid_width_mapping,
			pdxlstatsderrelMCV->Rows(), pdxlstatsderrelMCV->IsEmpty());

		// put stats object in an array in order to serialize
		CStatisticsArray *pdrgpstats = GPOS_NEW(mp) CStatisticsArray(mp);
		pdrgpstats->Append(stats);

		// serialize stats object
		CWStringDynamic *pstrOutput = CDXLUtils::SerializeStatistics(
			mp, md_accessor, pdrgpstats, true, true);
		GPOS_TRACE(pstrOutput->GetBuffer());

		// get expected output
		CWStringDynamic str(mp);
		COstreamString oss(&str);
		CHAR *szDXLExpected =
			CDXLUtils::Read(mp, rgMergeTestElem[ul].szMergedFile);
		CWStringDynamic dstrExpected(mp);
		dstrExpected.AppendFormat(GPOS_WSZ_LIT("%s"), szDXLExpected);

		GPOS_RESULT eres =
			CTestUtils::EresCompare(oss, pstrOutput, &dstrExpected,
									false  // mismatch will not be ignored
			);

		// cleanup
		GPOS_DELETE_ARRAY(szDXLInputMCV);
		GPOS_DELETE_ARRAY(szDXLInputHist);
		GPOS_DELETE_ARRAY(szDXLExpected);
		GPOS_DELETE(pstrOutput);
		pdrgpdxlstatsderrelMCV->Release();
		pdrgpdxlstatsderrelHist->Release();
		GPOS_DELETE(phistMCV);
		GPOS_DELETE(phistHist);
		pdrgpstats->Release();

		if (GPOS_OK != eres)
		{
			return eres;
		}
	}

	return GPOS_OK;
}

// EOF
