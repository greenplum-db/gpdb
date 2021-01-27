//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CFSimulatorTest.cpp
//
//	@doc:
//		Tests for
//---------------------------------------------------------------------------

#include "unittest/gpos/error/CFSimulatorTest.h"

#include "gpos/base.h"
#include "gpos/error/CFSimulator.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoSuspendAbort.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

#ifdef GPOS_FPSIMULATOR

//---------------------------------------------------------------------------
//	@function:
//		CFSimulator::EresUnittest
//
//	@doc:
//		Driver for unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CFSimulatorTest::EresUnittest_BasicTracking)};

	// ignore this test for FP simulation and time slicing check
	if (CFSimulator::FSimulation())
	{
		return GPOS_OK;
	}

	// set test flag in this scope
	CAutoTraceFlag atf(EtraceTest, true);

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CFSimulatorTest::EresUnittest_BasicTracking
//
//	@doc:
//		Register a single occurrance
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTest::EresUnittest_BasicTracking()
{
	BOOL fThrown = false;
	static ULONG ul = 0;
	if (10 == ul)
	{
		return GPOS_OK;
	}

	GPOS_TRY
	{
		GPOS_SIMULATE_FAILURE(EtraceTest, CException::ExmaSystem,
							  CException::ExmiOOM);
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM))
		{
			// suspend CFA
			CAutoSuspendAbort asa;

			GPOS_TRACE_FORMAT("%d: Caught expected exception.", ul);

			fThrown = true;
		}

		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	++ul;
	EresUnittest_BasicTracking();

	if (fThrown)
	{
		return GPOS_OK;
	}

	return GPOS_FAILED;
}

#endif	// GPOS_FPSIMULATOR

// EOF
