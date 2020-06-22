//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Inc.
//
//	@filename:
//		CHistogramTest.h
//
//	@doc:
//		Testing operations on points used to define histogram buckets
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CHistogramTest_H
#define GPNAUCRATES_CHistogramTest_H

#include "gpos/base.h"
#include "naucrates/statistics/CHistogram.h"

namespace gpnaucrates
{

	//---------------------------------------------------------------------------
	//	@class:
	//		CHistogramTest
	//
	//	@doc:
	//		Static unit tests for point
	//
	//---------------------------------------------------------------------------
	class CHistogramTest
	{
		private:
			// generate int histogram having tuples not covered by buckets,
			// including null fraction and nDistinctRemain
			static
			CHistogram* PhistExampleInt4Remain(CMemoryPool *mp);

		public:

			// unittests
			static
			GPOS_RESULT EresUnittest();

			// histogram basic tests
			static
			GPOS_RESULT EresUnittest_CHistogramValid();

			static
			GPOS_RESULT EresUnittest_CHistogramInt4();

			static
			GPOS_RESULT EresUnittest_CHistogramBool();

			// skew basic tests
			static
			GPOS_RESULT EresUnittest_Skew();

			// merge basic tests
			static
			GPOS_RESULT EresUnittest_MergeUnion();

	}; // class CHistogramTest
}

#endif // !GPNAUCRATES_CHistogramTest_H


// EOF
