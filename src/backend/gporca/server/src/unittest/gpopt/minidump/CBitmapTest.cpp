//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CBitmapTest.cpp
//
//	@doc:
//		Test for optimizing queries that can use a bitmap index
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"

#include "unittest/gpopt/minidump/CBitmapTest.h"

using namespace gpopt;

ULONG CBitmapTest::m_ulBitmapTestCounter = 0;  // start from first test

// minidump files
const CHAR *rgszBitmapFileNames[] = {
	"../data/dxl/minidump/BitmapIndexScan.mdp",
	"../data/dxl/minidump/BitmapIndexScanCost.mdp",
	"../data/dxl/minidump/DynamicBitmapIndexScan.mdp",
	"../data/dxl/minidump/BitmapTableScan-AO.mdp",
	"../data/dxl/minidump/BitmapTableScan-Basic.mdp",
	"../data/dxl/minidump/BitmapTableScan-ColumnOnRightSide.mdp",
	"../data/dxl/minidump/BitmapTableScan-AndCondition.mdp",
	"../data/dxl/minidump/BitmapIndexProbeMergeFilters.mdp",
	"../data/dxl/minidump/DynamicBitmapTableScan-Basic.mdp",
	"../data/dxl/minidump/DynamicBitmapTableScan-Heterogeneous.mdp",
	"../data/dxl/minidump/DynamicBitmapTableScan-UUID.mdp",
	"../data/dxl/minidump/BitmapBoolOr.mdp",
	"../data/dxl/minidump/BitmapBoolOr-BoolColumn.mdp",
	"../data/dxl/minidump/BitmapBoolAnd.mdp",
	"../data/dxl/minidump/BitmapBoolOp-DeepTree.mdp",
	"../data/dxl/minidump/BitmapBoolOp-DeepTree2.mdp",
	"../data/dxl/minidump/BitmapBoolOp-DeepTree3.mdp",
	"../data/dxl/minidump/DynamicBitmapBoolOp.mdp",
	"../data/dxl/minidump/BitmapIndexScan-WithUnsupportedOperatorFilter.mdp",
	"../data/dxl/minidump/BitmapTableScan-AO-Btree.mdp",
	"../data/dxl/minidump/BitmapIndexApply-Basic-SelfJoin.mdp",
	"../data/dxl/minidump/BitmapIndexApply-Basic-TwoTables.mdp",
	"../data/dxl/minidump/BitmapIndexApply-Complex-Condition.mdp",
	"../data/dxl/minidump/BitmapIndexApply-PartTable.mdp",
	"../data/dxl/minidump/BitmapIndexApply-InnerSelect-Basic.mdp",
	"../data/dxl/minidump/BitmapIndexApply-InnerSelect-PartTable.mdp",
	"../data/dxl/minidump/PredicateWithConjunctsAndDisjuncts.mdp",
	"../data/dxl/minidump/PredicateWithConjunctsOfDisjuncts.mdp",
	"../data/dxl/minidump/PredicateWithLongConjunction.mdp",
	"../data/dxl/minidump/MultipleIndexPredicate.mdp",
	"../data/dxl/minidump/BitmapIndexScanChooseIndex.mdp",
};

//---------------------------------------------------------------------------
//	@function:
//		CBitmapTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitmapTest::EresUnittest()
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
//		CBitmapTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitmapTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(
		rgszBitmapFileNames, &m_ulBitmapTestCounter,
		GPOS_ARRAY_SIZE(rgszBitmapFileNames));
}

// EOF
