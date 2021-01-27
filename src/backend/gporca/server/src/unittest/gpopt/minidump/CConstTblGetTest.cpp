//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CConstTblGetTest.h"

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"


using namespace gpopt;

ULONG CConstTblGetTest::m_ulTestCounter = 0;  // start from first test

// minidump files
const CHAR *rgszCTGMdpFiles[] = {
	"../data/dxl/minidump/ConstTblGetUnderSubqWithOuterRef.mdp",
	"../data/dxl/minidump/ConstTblGetUnderSubqWithNoOuterRef.mdp",
	"../data/dxl/minidump/ConstTblGetUnderSubqUnderProjectNoOuterRef.mdp",
	"../data/dxl/minidump/ConstTblGetUnderSubqUnderProjectWithOuterRef.mdp",
	"../data/dxl/minidump/CTG-Filter.mdp",
	"../data/dxl/minidump/CTG-Join.mdp",
	"../data/dxl/minidump/Sequence-With-Universal-Outer.mdp",
	"../data/dxl/minidump/UseDistributionSatisfactionForUniversalInnerChild.mdp",
	"../data/dxl/minidump/Join_OuterChild_DistUniversal.mdp",
};


GPOS_RESULT
CConstTblGetTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

// Run all Minidump-based tests with plan matching
GPOS_RESULT
CConstTblGetTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(rgszCTGMdpFiles, &m_ulTestCounter,
											 GPOS_ARRAY_SIZE(rgszCTGMdpFiles));
}

// EOF
