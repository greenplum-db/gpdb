//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal, Inc.
//
//	@filename:
//		CPruneColumnsTest.cpp
//
//	@doc:
//		Test for optimizing queries where intermediate columns are pruned
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/minidump/CPruneColumnsTest.h"


using namespace gpopt;

ULONG CPruneColumnsTest::m_ulPruneColumnsTestCounter =
	0;	// start from first test

// minidump files
const CHAR *rgszPruneColumnsFileNames[] = {
	"../data/dxl/minidump/RemoveUnusedProjElementsInGbAgg.mdp",
	"../data/dxl/minidump/RemoveUnusedProjElements.mdp",
	"../data/dxl/minidump/CPruneColumnsTest/PruneIntermediateUnusedColumns.mdp",  // prune all unused columns
	"../data/dxl/minidump/CPruneColumnsTest/AggTopOfSingleSetRetFuncs.mdp",	 // no pruning done
	"../data/dxl/minidump/CPruneColumnsTest/AggTopOfSetRetFuncsAndUnusedScalar.mdp",  // partial pruning
	"../data/dxl/minidump/CPruneColumnsTest/AggTopOfMultipleSetRetFuncs.mdp",  // no pruning done
	"../data/dxl/minidump/CPruneColumnsTest/AggTopOfMultipleSetRetFuncsAndUnusedScalar.mdp",  // partial prune
	"../data/dxl/minidump/CPruneColumnsTest/AggTopOfSetRefFuncsOnTopTbl.mdp",  // no pruning done
	"../data/dxl/minidump/CPruneColumnsTest/AllColsUsed.mdp",  // no pruning done
	"../data/dxl/minidump/CPruneColumnsTest/UsedSetRetFuncAndUnusedScalarFunc.mdp",	 // partial pruning
	"../data/dxl/minidump/CPruneColumnsTest/UnusedSetRetFuncAndUsedScalarFunc.mdp",	 // partial pruning ---> BUG
	"../data/dxl/minidump/CPruneColumnsTest/MultiLevelSubqueryWithSetRetFuncs.mdp",	 // expect error in optimizer and planner: "Expected no more than one row to be returned by expression"
	"../data/dxl/minidump/CPruneColumnsTest/MultiLevelSubqueryWithSetRetFuncsAndScalarFuncs.mdp",  // if pruning subquery is handled then we should prune some columns
};


//---------------------------------------------------------------------------
//	@function:
//		CPruneColumnsTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPruneColumnsTest::EresUnittest()
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
//		CPruneColumnsTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPruneColumnsTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(
		rgszPruneColumnsFileNames, &m_ulPruneColumnsTestCounter,
		GPOS_ARRAY_SIZE(rgszPruneColumnsFileNames));
}

// EOF
