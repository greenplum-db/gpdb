//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CCTETest.cpp
//
//	@doc:
//		Test for optimizing queries with CTEs
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CCTETest.h"
#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"


using namespace gpopt;

ULONG CCTETest::m_ulCTETestCounter = 0;  // start from first test

// minidump files
const CHAR *rgszCTEFileNames[] =
	{
	// TODO:  - 03/20/2014: the plan in this test keeps changing between runs
	// re-enable test after this issue is fixed
	//		"../data/dxl/minidump/CTE-1.mdp",
	// 		"../data/dxl/minidump/CTE-2.mdp",
	"../data/dxl/minidump/CTE-3.mdp",
	"../data/dxl/minidump/CTE-4.mdp",
	"../data/dxl/minidump/CTE-5.mdp",
	"../data/dxl/minidump/CTE-6.mdp",
	"../data/dxl/minidump/CTE-7.mdp",
	"../data/dxl/minidump/CTE-8.mdp",
	"../data/dxl/minidump/CTE-9.mdp",
	"../data/dxl/minidump/CTE-10.mdp",
	"../data/dxl/minidump/CTE-11.mdp",
	"../data/dxl/minidump/CTE-with-random-filter.mdp",
	"../data/dxl/minidump/CTE-volatile.mdp",
	"../data/dxl/minidump/CTE-PartTbl.mdp",
	"../data/dxl/minidump/CTE-Preds1.mdp",
	"../data/dxl/minidump/CTE-Preds2.mdp",
	"../data/dxl/minidump/CTE-Join-Redistribute-Producer.mdp",
	"../data/dxl/minidump/CTE-SetOp.mdp",
	"../data/dxl/minidump/cte-duplicate-columns-1.mdp",
	"../data/dxl/minidump/cte-duplicate-columns-2.mdp",
	"../data/dxl/minidump/cte-duplicate-columns-3.mdp",
	"../data/dxl/minidump/cte-duplicate-columns-4.mdp",
	"../data/dxl/minidump/CTEWithVolatileFunction.mdp",
	"../data/dxl/minidump/WinFunc-Redistribute-Sort-CTE-Producer.mdp",
	"../data/dxl/minidump/Select-Over-CTEAnchor.mdp",
	"../data/dxl/minidump/CTEinlining.mdp",
	"../data/dxl/minidump/CTE-ValuesScan-ProjList.mdp",
	"../data/dxl/minidump/CTEWithMergedGroup.mdp",
	"../data/dxl/minidump/CTEMergeGroupsCircularDeriveStats.mdp"
	};


//---------------------------------------------------------------------------
//	@function:
//		CCTETest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCTETest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
		};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CCTETest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCTETest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests
						(
						rgszCTEFileNames,
						&m_ulCTETestCounter,
						GPOS_ARRAY_SIZE(rgszCTEFileNames)
						);
}

// EOF
