//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Inc.
//
//	@filename:
//		CBucketTest.h
//
//	@doc:
//		Test for CBucketTest
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CBucketTest_H
#define GPNAUCRATES_CBucketTest_H

#include "naucrates/statistics/CBucket.h"

namespace gpnaucrates
{

	//---------------------------------------------------------------------------
	//	@class:
	//		CBucketTest
	//
	//	@doc:
	//		Static unit testing operations on histogram buckets
	//
	//---------------------------------------------------------------------------
	class CBucketTest
	{
		private:

		// test case for intersection of buckets
		struct SBucketsIntersectTestElem
		{
			// lower bound of bucket 1
			INT m_iLb1;

			// upper bound of bucket 1
			INT m_iUb1;

			// is lower bound of bucket 1 closed
			BOOL m_fLb1Closed;

			// is upper bound of bucket 1 closed
			BOOL m_fUb1Closed;

			// lower bound of bucket 2
			INT m_iLb2;

			// upper bound of bucket 2
			INT m_iUb2;

			// is lower bound of bucket 2 closed
			BOOL m_fLb2Closed;

			// is upper bound of bucket 2 closed
			BOOL m_fUb2Closed;

			// result of the bucket intersect test
			BOOL fIntersect;

			// lower bound of output bucket
			INT m_iLbOutput;

			// upper bound of output bucket
			INT m_iUbOutput;

			// is lower bound of output bucket closed
			BOOL m_fLbOutputClosed;

			// is upper bound of output bucket closed
			BOOL m_fUbOutputClosed;

		}; // SBucketsIntersectTestElem

			// do the bucket boundaries match
			static
			BOOL FMatchBucketBoundary(CBucket *bucket1, CBucket *bucket2);

		public:

			// unittests
			static
			GPOS_RESULT EresUnittest();

			// bucket basic tests
			static
			GPOS_RESULT EresUnittest_CBucketInt4();

			static
			GPOS_RESULT EresUnittest_CBucketBool();

			// bucket intersect
			static
			GPOS_RESULT EresUnittest_CBucketIntersect();

			// bucket scaling tests
			static
			GPOS_RESULT EresUnittest_CBucketScale();

			// bucket difference tests
			static
			GPOS_RESULT EresUnittest_CBucketDifference();

			static
			GPOS_RESULT EresUnittest_CBucketMergeCommutativityUnion();

			static
			GPOS_RESULT EresUnittest_CBucketMergeCommutativitySameLowerBounds();

			static
			GPOS_RESULT EresUnittest_CBucketMergeCommutativitySameUpperBounds();

			static
			GPOS_RESULT EresUnittest_CBucketMergeCommutativityUnionAll();

	}; // class CBucketTest
}

#endif // !GPNAUCRATES_CBucketTest_H


// EOF
