//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CTVFTest.cpp
//
//	@doc:
//		Test for optimizing queries with TVF
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"

#include "unittest/gpopt/minidump/CTVFTest.h"

using namespace gpopt;

ULONG CTVFTest::m_ulTVFTestCounter = 0;	 // start from first test

// minidump files
const CHAR *rgszTVFFileNames[] = {
	"../data/dxl/minidump/TVF.mdp",
	"../data/dxl/minidump/TVFAnyelement.mdp",
	"../data/dxl/minidump/TVFVolatileJoin.mdp",
	"../data/dxl/minidump/TVFRandom.mdp",
	"../data/dxl/minidump/TVFGenerateSeries.mdp",
	"../data/dxl/minidump/TVF-With-Deep-Subq-Args.mdp",
	"../data/dxl/minidump/TVFCorrelatedExecution.mdp",
	"../data/dxl/minidump/CSQ-VolatileTVF.mdp",
};

//---------------------------------------------------------------------------
//	@function:
//		CTVFTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTVFTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CTVFTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTVFTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(rgszTVFFileNames,
											 &m_ulTVFTestCounter,
											 GPOS_ARRAY_SIZE(rgszTVFFileNames));
}

// EOF
