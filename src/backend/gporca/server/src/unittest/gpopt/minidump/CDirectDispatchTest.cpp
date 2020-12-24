//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDirectDispatchTest.cpp
//
//	@doc:
//		Test for direct dispatch
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"

#include "unittest/gpopt/minidump/CDirectDispatchTest.h"

using namespace gpopt;

ULONG CDirectDispatchTest::m_ulDirectDispatchCounter =
	0;	// start from first test

// minidump files
const CHAR *rgszDirectDispatchFileNames[] = {
	"../data/dxl/minidump/DirectDispatch-SingleCol.mdp",
	"../data/dxl/minidump/DirectDispatch-GpSegmentId.mdp",
	"../data/dxl/minidump/DirectDispatch-SingleCol-Disjunction.mdp",
	"../data/dxl/minidump/DirectDispatch-SingleCol-Disjunction-IsNull.mdp",
	"../data/dxl/minidump/DirectDispatch-SingleCol-Disjunction-Negative.mdp",
	"../data/dxl/minidump/DirectDispatch-MultiCol.mdp",
	"../data/dxl/minidump/DirectDispatch-MultiCol-Disjunction.mdp",
	"../data/dxl/minidump/DirectDispatch-MultiCol-Negative.mdp",
	"../data/dxl/minidump/DirectDispatch-IndexScan.mdp",
	"../data/dxl/minidump/DirectDispatch-DynamicIndexScan.mdp",
	"../data/dxl/minidump/InsertDirectedDispatchNullValue.mdp",
};

//---------------------------------------------------------------------------
//	@function:
//		CDirectDispatchTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDirectDispatchTest::EresUnittest()
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
//		CDirectDispatchTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDirectDispatchTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(
		rgszDirectDispatchFileNames, &m_ulDirectDispatchCounter,
		GPOS_ARRAY_SIZE(rgszDirectDispatchFileNames));
}

// EOF
