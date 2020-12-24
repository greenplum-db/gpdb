//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CCardinalityTestUtils.cpp
//
//	@doc:
//		Utility functions used in the testing cardinality estimation
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include "gpos/io/COstreamString.h"

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatisticsUtils.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLDatumGeneric.h"
#include "naucrates/dxl/operators/CDXLDatumStatsDoubleMappable.h"
#include "naucrates/dxl/operators/CDXLDatumStatsLintMappable.h"

#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/gpopt/CTestUtils.h"


// create a bucket with closed integer bounds
CBucket *
CCardinalityTestUtils::PbucketIntegerClosedLowerBound(CMemoryPool *mp,
													  INT iLower, INT iUpper,
													  CDouble frequency,
													  CDouble distinct)
{
	CPoint *ppLower = CTestUtils::PpointInt4(mp, iLower);
	CPoint *ppUpper = CTestUtils::PpointInt4(mp, iUpper);

	BOOL is_upper_closed = false;
	if (ppLower->Equals(ppUpper))
	{
		is_upper_closed = true;
	}

	return GPOS_NEW(mp) CBucket(ppLower, ppUpper, true /* is_lower_closed */,
								is_upper_closed, frequency, distinct);
}

// create an integer bucket with the provider upper/lower bound, frequency and NDV information
CBucket *
CCardinalityTestUtils::PbucketInteger(CMemoryPool *mp, INT iLower, INT iUpper,
									  BOOL is_lower_closed,
									  BOOL is_upper_closed, CDouble frequency,
									  CDouble distinct)
{
	CPoint *ppLower = CTestUtils::PpointInt4(mp, iLower);
	CPoint *ppUpper = CTestUtils::PpointInt4(mp, iUpper);

	return GPOS_NEW(mp) CBucket(ppLower, ppUpper, is_lower_closed,
								is_upper_closed, frequency, distinct);
}

// create a singleton bucket containing a boolean value
CBucket *
CCardinalityTestUtils::PbucketSingletonBoolVal(CMemoryPool *mp, BOOL fValue,
											   CDouble frequency)
{
	CPoint *ppLower = CTestUtils::PpointBool(mp, fValue);

	// lower bound is also upper bound
	ppLower->AddRef();
	return GPOS_NEW(mp) CBucket(ppLower, ppLower, true /* fClosedUpper */,
								true /* fClosedUpper */, frequency, 1.0);
}

// helper function to generate integer histogram based on the NDV and bucket information provided
CHistogram *
CCardinalityTestUtils::PhistInt4Remain(CMemoryPool *mp, ULONG num_of_buckets,
									   CDouble dNDVPerBucket, BOOL fNullFreq,
									   CDouble num_NDV_remain)
{
	// generate histogram of the form [0, 100), [100, 200), [200, 300) ...
	CBucketArray *histogram_buckets = GPOS_NEW(mp) CBucketArray(mp);
	for (ULONG idx = 0; idx < num_of_buckets; idx++)
	{
		INT iLower = INT(idx * 100);
		INT iUpper = INT((idx + 1) * 100);
		CDouble frequency(0.1);
		CDouble distinct = dNDVPerBucket;
		CBucket *bucket = PbucketIntegerClosedLowerBound(mp, iLower, iUpper,
														 frequency, distinct);
		histogram_buckets->Append(bucket);
	}

	CDouble freq = CStatisticsUtils::GetFrequency(histogram_buckets);
	CDouble null_freq(0.0);
	if (fNullFreq && 1 > freq)
	{
		null_freq = 0.1;
		freq = freq + null_freq;
	}

	CDouble freq_remaining = (1 - freq);
	if (freq_remaining < CStatistics::Epsilon ||
		num_NDV_remain < CStatistics::Epsilon)
	{
		freq_remaining = CDouble(0.0);
	}

	return GPOS_NEW(mp) CHistogram(mp, histogram_buckets, true, null_freq,
								   num_NDV_remain, freq_remaining);
}

// helper function to generate an example int histogram
CHistogram *
CCardinalityTestUtils::PhistExampleInt4(CMemoryPool *mp)
{
	// generate histogram of the form [0, 10), [10, 20), [20, 30) ... [80, 90)
	CBucketArray *histogram_buckets = GPOS_NEW(mp) CBucketArray(mp);
	for (ULONG idx = 0; idx < 9; idx++)
	{
		INT iLower = INT(idx * 10);
		INT iUpper = iLower + INT(10);
		CDouble frequency(0.1);
		CDouble distinct(4.0);
		CBucket *bucket = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(
			mp, iLower, iUpper, frequency, distinct);
		histogram_buckets->Append(bucket);
	}

	// add an additional singleton bucket [100, 100]
	histogram_buckets->Append(
		CCardinalityTestUtils::PbucketIntegerClosedLowerBound(mp, 100, 100, 0.1,
															  1.0));

	return GPOS_NEW(mp) CHistogram(mp, histogram_buckets);
}

// helper function to generates example bool histogram
CHistogram *
CCardinalityTestUtils::PhistExampleBool(CMemoryPool *mp)
{
	CBucketArray *histogram_buckets = GPOS_NEW(mp) CBucketArray(mp);
	CBucket *pbucketFalse =
		CCardinalityTestUtils::PbucketSingletonBoolVal(mp, false, 0.1);
	CBucket *pbucketTrue =
		CCardinalityTestUtils::PbucketSingletonBoolVal(mp, true, 0.2);
	histogram_buckets->Append(pbucketFalse);
	histogram_buckets->Append(pbucketTrue);
	return GPOS_NEW(mp) CHistogram(mp, histogram_buckets);
}

// helper function to generate a point from an encoded value of specific datatype
CPoint *
CCardinalityTestUtils::PpointGeneric(CMemoryPool *mp, OID oid,
									 CWStringDynamic *pstrEncodedValue,
									 LINT value)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	IMDId *mdid = GPOS_NEW(mp) CMDIdGPDB(oid);
	IDatum *datum = CTestUtils::CreateGenericDatum(mp, md_accessor, mdid,
												   pstrEncodedValue, value);
	CPoint *point = GPOS_NEW(mp) CPoint(datum);

	return point;
}

// helper function to generate a point of numeric datatype
CPoint *
CCardinalityTestUtils::PpointNumeric(CMemoryPool *mp,
									 CWStringDynamic *pstrEncodedValue,
									 CDouble value)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	CMDIdGPDB *mdid = GPOS_NEW(mp) CMDIdGPDB(CMDIdGPDB::m_mdid_numeric);
	const IMDType *pmdtype = md_accessor->RetrieveType(mdid);

	ULONG ulbaSize = 0;
	BYTE *data =
		CDXLUtils::DecodeByteArrayFromString(mp, pstrEncodedValue, &ulbaSize);

	CDXLDatumStatsDoubleMappable *dxl_datum = GPOS_NEW(mp)
		CDXLDatumStatsDoubleMappable(mp, mdid, default_type_modifier,
									 false /*is_const_null*/, data, ulbaSize,
									 value);

	IDatum *datum = pmdtype->GetDatumForDXLDatum(mp, dxl_datum);
	CPoint *point = GPOS_NEW(mp) CPoint(datum);
	dxl_datum->Release();

	return point;
}

// helper function to generate a point from an encoded value of specific datatype
CPoint *
CCardinalityTestUtils::PpointDouble(CMemoryPool *mp, OID oid, CDouble value)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	IMDId *mdid = GPOS_NEW(mp) CMDIdGPDB(oid);
	IDatum *datum = CTestUtils::CreateDoubleDatum(mp, md_accessor, mdid, value);
	CPoint *point = GPOS_NEW(mp) CPoint(datum);

	return point;
}

// helper function to print the bucket object
void
CCardinalityTestUtils::PrintBucket(CMemoryPool *mp, const char *pcPrefix,
								   const CBucket *bucket)
{
	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << pcPrefix << " = ";
	bucket->OsPrint(oss);
	oss << std::endl;
	GPOS_TRACE(str.GetBuffer());
}

// helper function to print histogram object
void
CCardinalityTestUtils::PrintHist(CMemoryPool *mp, const char *pcPrefix,
								 const CHistogram *histogram)
{
	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << pcPrefix << " = ";
	histogram->OsPrint(oss);
	oss << std::endl;
	GPOS_TRACE(str.GetBuffer());
}

// helper function to print the statistics object
void
CCardinalityTestUtils::PrintStats(CMemoryPool *mp, const CStatistics *stats)
{
	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << "Statistics = ";
	stats->OsPrint(oss);
	oss << std::endl;
	GPOS_TRACE(str.GetBuffer());
}


// EOF
