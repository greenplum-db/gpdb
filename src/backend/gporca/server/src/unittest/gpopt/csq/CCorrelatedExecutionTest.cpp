//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CCorrelatedExecutionTest.cpp
//
//	@doc:
//		Test for correlated subqueries
//---------------------------------------------------------------------------
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/exception.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/ops.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/csq/CCorrelatedExecutionTest.h"

ULONG CCorrelatedExecutionTest::m_ulTestCounter = 0;  // start from first test

// files for testing of different queries with correlated subqueries
const CHAR *rgszPositiveTests[] = {
	"../data/dxl/csq_tests/dxl-q1.xml",	 "../data/dxl/csq_tests/dxl-q2.xml",
	"../data/dxl/csq_tests/dxl-q3.xml",	 "../data/dxl/csq_tests/dxl-q4.xml",
	"../data/dxl/csq_tests/dxl-q5.xml",	 "../data/dxl/csq_tests/dxl-q6.xml",
	"../data/dxl/csq_tests/dxl-q7.xml",	 "../data/dxl/csq_tests/dxl-q8.xml",
	"../data/dxl/csq_tests/dxl-q9.xml",	 "../data/dxl/csq_tests/dxl-q10.xml",
	"../data/dxl/csq_tests/dxl-q11.xml", "../data/dxl/csq_tests/dxl-q12.xml",
	"../data/dxl/csq_tests/dxl-q13.xml", "../data/dxl/csq_tests/dxl-q14.xml",
	"../data/dxl/csq_tests/dxl-q15.xml", "../data/dxl/csq_tests/dxl-q16.xml",
};

//---------------------------------------------------------------------------
//	@function:
//		CCorrelatedExecutionTest::EresUnittest
//
//	@doc:
//		Unittest for converting correlated Apply expressions to NL joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCorrelatedExecutionTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(
			CCorrelatedExecutionTest::EresUnittest_RunAllPositiveTests),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CCorrelatedExecutionTest::EresUnittest_RunAllPositiveTests
//
//	@doc:
//		Run all tests that are expected to pass without any exceptions
//		for parsing DXL documents into DXL trees.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCorrelatedExecutionTest::EresUnittest_RunAllPositiveTests()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// loop over all test files
	for (ULONG ul = m_ulTestCounter; ul < GPOS_ARRAY_SIZE(rgszPositiveTests);
		 ul++)
	{
		// TODO:  06/15/2012; enable plan matching
		GPOS_RESULT eres = CTestUtils::EresTranslate(mp, rgszPositiveTests[ul],
													 NULL /* plan file */,
													 true /*fIgnoreMismatch*/
		);

		m_ulTestCounter++;

		if (GPOS_OK != eres)
		{
			return eres;
		}
	}

	// reset test counter
	m_ulTestCounter = 0;

	return GPOS_OK;
}

// EOF
