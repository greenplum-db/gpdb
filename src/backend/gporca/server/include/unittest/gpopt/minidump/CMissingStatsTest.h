//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CMissingStatsTest.h
//
//	@doc:
//		Test for ensuring the expected number of missing stats during optimization
//		is correct.
//---------------------------------------------------------------------------
#ifndef GPOPT_CMissingStatsTest_H
#define GPOPT_CMissingStatsTest_H

#include "gpos/base.h"

#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CMissingStatsTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CMissingStatsTest
{
	struct SMissingStatsTestCase
	{
		// input stats dxl file
		const CHAR *m_szInputFile;

		// expected number of columns with missing statistics
		ULONG m_ulExpectedMissingStats;

	};	// SMissingStatsTestCase

private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulMissingStatsTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CMissingStatsTest
}  // namespace gpopt

#endif	// !GPOPT_CMissingStatsTest_H

// EOF
