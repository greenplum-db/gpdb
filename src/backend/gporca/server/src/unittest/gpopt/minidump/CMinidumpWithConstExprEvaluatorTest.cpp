//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CMinidumpWithConstExprEvaluatorTest.cpp
//
//	@doc:
//		Tests minidumps with constant expression evaluator turned on
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpos/task/CAutoTraceFlag.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/exception.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/engine/CCTEConfig.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/CConstExprEvaluatorForDates.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/minidump/CMinidumpWithConstExprEvaluatorTest.h"

using namespace gpopt;
using namespace gpos;

// start from first test that uses constant expression evaluator
ULONG CMinidumpWithConstExprEvaluatorTest::m_ulTestCounter = 0;

// minidump files we run with constant expression evaluator on
const CHAR *rgszConstExprEvaluatorOnFileNames[] = {
	"../data/dxl/minidump/DynamicIndexScan-Homogenous-EnabledDateConstraint.mdp",
	"../data/dxl/minidump/DynamicIndexScan-Heterogenous-EnabledDateConstraint.mdp",
	"../data/dxl/minidump/RemoveImpliedPredOnBCCPredicates.mdp"};


//---------------------------------------------------------------------------
//	@function:
//		CMinidumpWithConstExprEvaluatorTest::EresUnittest
//
//	@doc:
//		Runs all unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMinidumpWithConstExprEvaluatorTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(
			CMinidumpWithConstExprEvaluatorTest::
				EresUnittest_RunMinidumpTestsWithConstExprEvaluatorOn),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CMinidumpWithConstExprEvaluatorTest::EresUnittest_RunMinidumpTestsWithConstExprEvaluatorOn
//
//	@doc:
//		Run tests with constant expression evaluation enabled
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMinidumpWithConstExprEvaluatorTest::
	EresUnittest_RunMinidumpTestsWithConstExprEvaluatorOn()
{
	CAutoTraceFlag atf(EopttraceEnableConstantExpressionEvaluation,
					   true /*value*/);

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	IConstExprEvaluator *pceeval = GPOS_NEW(mp) CConstExprEvaluatorForDates(mp);

	BOOL fMatchPlans = true;

	// enable plan enumeration only if we match plans
	CAutoTraceFlag atf1(EopttraceEnumeratePlans, fMatchPlans);

	// enable stats derivation for DPE
	CAutoTraceFlag atf2(EopttraceDeriveStatsForDPE, true /*value*/);

	const ULONG ulTests = GPOS_ARRAY_SIZE(rgszConstExprEvaluatorOnFileNames);

	GPOS_RESULT eres = CTestUtils::EresRunMinidumps(
		mp, rgszConstExprEvaluatorOnFileNames, ulTests, &m_ulTestCounter,
		1,	// ulSessionId
		1,	// ulCmdId
		fMatchPlans,
		false,	// fTestSpacePruning
		NULL,	// szMDFilePath
		pceeval);
	pceeval->Release();

	return eres;
}

// EOF
