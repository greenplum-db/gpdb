//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CExternalTableTest.cpp
//
//	@doc:
//		Test for external tables
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CExternalTableTest.h"

#include "gpos/task/CAutoTraceFlag.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/exception.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLPhysicalNLJoin.h"
#include "naucrates/exception.h"

#include "unittest/base.h"
#include "unittest/gpopt/CConstExprEvaluatorForDates.h"
#include "unittest/gpopt/CTestUtils.h"

using namespace gpdxl;

ULONG CExternalTableTest::m_ulTestCounter = 0;	// start from first test

// minidump files
const CHAR *rgszExternalTableFileNames[] = {
	"../data/dxl/minidump/ExternalTable1.mdp",
	"../data/dxl/minidump/ExternalTable2.mdp",
	"../data/dxl/minidump/ExternalTable3.mdp",
	"../data/dxl/minidump/ExternalTable4.mdp",
	"../data/dxl/minidump/ExternalTableWithFilter.mdp",
	"../data/dxl/minidump/CTAS-with-randomly-distributed-external-table.mdp",
	"../data/dxl/minidump/CTAS-with-hashed-distributed-external-table.mdp",
	"../data/dxl/minidump/AggonExternalTableNoMotion.mdp"};


//---------------------------------------------------------------------------
//	@function:
//		CExternalTableTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExternalTableTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CExternalTableTest::EresUnittest_RunMinidumpTests),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CExternalTableTest::EresUnittest_RunMinidumpTests
//
//	@doc:
//		Run all Minidump-based tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExternalTableTest::EresUnittest_RunMinidumpTests()
{
	return CTestUtils::EresUnittest_RunTests(
		rgszExternalTableFileNames, &m_ulTestCounter,
		GPOS_ARRAY_SIZE(rgszExternalTableFileNames));
}

// EOF
