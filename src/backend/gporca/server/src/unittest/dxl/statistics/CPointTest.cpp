//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPointTest.cpp
//
//	@doc:
//		Tests for CPoint
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/statistics/CPoint.h"

#include "unittest/base.h"
#include "unittest/dxl/statistics/CPointTest.h"
#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;

// unittest for statistics objects
GPOS_RESULT
CPointTest::EresUnittest()
{
	// tests that use shared optimization context
	CUnittest rgutSharedOptCtxt[] = {
		GPOS_UNITTEST_FUNC(CPointTest::EresUnittest_CPointInt4),
		GPOS_UNITTEST_FUNC(CPointTest::EresUnittest_CPointBool),
	};

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /* pceeval */,
					 CTestUtils::GetCostModel(mp));

	return CUnittest::EresExecute(rgutSharedOptCtxt,
								  GPOS_ARRAY_SIZE(rgutSharedOptCtxt));
}

// basic int4 point tests;
GPOS_RESULT
CPointTest::EresUnittest_CPointInt4()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// generate integer points
	CPoint *point1 = CTestUtils::PpointInt4(mp, 1);
	CPoint *point2 = CTestUtils::PpointInt4(mp, 2);

	GPOS_RTL_ASSERT_MSG(point1->Equals(point1), "1 == 1");
	GPOS_RTL_ASSERT_MSG(point1->IsLessThan(point2), "1 < 2");
	GPOS_RTL_ASSERT_MSG(point2->IsGreaterThan(point1), "2 > 1");
	GPOS_RTL_ASSERT_MSG(point1->IsLessThanOrEqual(point2), "1 <= 2");
	GPOS_RTL_ASSERT_MSG(point2->IsGreaterThanOrEqual(point2), "2 >= 2");

	CDouble dDistance = point2->Distance(point1);

	// should be 1.0
	GPOS_RTL_ASSERT_MSG(0.99 < dDistance && dDistance < 1.01,
						"incorrect distance calculation");

	point1->Release();
	point2->Release();

	return GPOS_OK;
}

// basic bool point tests;
GPOS_RESULT
CPointTest::EresUnittest_CPointBool()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// generate boolean points
	CPoint *point1 = CTestUtils::PpointBool(mp, true);
	CPoint *point2 = CTestUtils::PpointBool(mp, false);

	// true == true
	GPOS_RTL_ASSERT_MSG(point1->Equals(point1), "true must be equal to true");

	// true != false
	GPOS_RTL_ASSERT_MSG(point1->IsNotEqual(point2),
						"true must not be equal to false");

	point1->Release();
	point2->Release();

	return GPOS_OK;
}

// EOF
