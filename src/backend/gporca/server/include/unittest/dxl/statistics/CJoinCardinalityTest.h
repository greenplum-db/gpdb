//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CJoinCardinalityTest.h
//
//	@doc:
//		Test for join cardinality estimation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CJoinCardinalityTest_H
#define GPNAUCRATES_CJoinCardinalityTest_H

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatsPredDisj.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		CJoinCardinalityTest
//
//	@doc:
//		Static unit tests for join cardinality estimation
//
//---------------------------------------------------------------------------
class CJoinCardinalityTest
{
	// shorthand for functions for generating the join predicates
	typedef CStatsPredJoinArray *(FnPdrgpstatjoin)(CMemoryPool *mp);

private:
	// test case for join evaluation
	struct SStatsJoinSTestCase
	{
		// input stats dxl file
		const CHAR *m_szInputFile;

		// output stats dxl file
		const CHAR *m_szOutputFile;

		// is the join a left outer join
		BOOL m_fLeftOuterJoin;

		// join predicate generation function pointer
		FnPdrgpstatjoin *m_pf;
	};	// SStatsJoinSTestCase

	// test case for join evaluation with NDVRemain
	struct SStatsJoinNDVRemainTestCase
	{
		// column identifier for the first histogram
		ULONG m_ulCol1;

		// column identifier for the second histogram
		ULONG m_ulCol2;

		// number of buckets in the output
		ULONG m_ulBucketsJoin;

		// cumulative number of distinct values in the buckets of the join histogram
		CDouble m_dNDVBucketsJoin;

		// NDV remain of the join histogram
		CDouble m_dNDVRemainJoin;

		// frequency of the NDV remain in the join histogram
		CDouble m_dFreqRemainJoin;
	};	// SStatsJoinNDVRemainTestCase

	// int4 histogram test cases
	struct SHistogramTestCase
	{
		// number of buckets in the histogram
		ULONG m_num_of_buckets;

		// number of distinct values per bucket
		CDouble m_dNDVPerBucket;

		// percentage of tuples that are null
		BOOL m_fNullFreq;

		// number of remain distinct values
		CDouble m_dNDVRemain;

	};	// SHistogramTestCase

	// helper method to generate a single join predicate
	static CStatsPredJoinArray *PdrgpstatspredjoinSingleJoinPredicate(
		CMemoryPool *mp);

	// helper method to generate generate multiple join predicates
	static CStatsPredJoinArray *PdrgpstatspredjoinMultiplePredicates(
		CMemoryPool *mp);

	// helper method to generate join predicate over columns that contain null values
	static CStatsPredJoinArray *PdrgpstatspredjoinNullableCols(CMemoryPool *mp);

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	// test join cardinality estimation over histograms with NDVRemain information
	static GPOS_RESULT EresUnittest_JoinNDVRemain();

	// join buckets tests
	static GPOS_RESULT EresUnittest_Join();

};	// class CJoinCardinalityTest
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CJoinCardinalityTest_H


// EOF
