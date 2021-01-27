//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CFSimulatorTestExt.cpp
//
//	@doc:
//		Extended FS tests
//---------------------------------------------------------------------------

#include "gpos/test/CFSimulatorTestExt.h"

#include "gpos/base.h"
#include "gpos/common/CMainArgs.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/error/CErrorHandlerStandard.h"
#include "gpos/error/CFSimulator.h"
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
CFSimulatorTestExt::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CFSimulatorTestExt::EresUnittest_OOM),
		GPOS_UNITTEST_FUNC(CFSimulatorTestExt::EresUnittest_Abort),
		GPOS_UNITTEST_FUNC(CFSimulatorTestExt::EresUnittest_IOError),
		GPOS_UNITTEST_FUNC(CFSimulatorTestExt::EresUnittest_NetError),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CFSimulatorTestExt::EresUnittest_OOM
//
//	@doc:
//		Simulate an OOM failure
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTestExt::EresUnittest_OOM()
{
	// enable OOM simulation
	CAutoTraceFlag atfSet(EtraceSimulateOOM, true);

	// run simulation
	return EresUnittest_SimulateException(CException::ExmaSystem,
										  CException::ExmiOOM);
}


//---------------------------------------------------------------------------
//	@function:
//		CFSimulatorTestExt::EresUnittest_Abort
//
//	@doc:
//		Simulate an Abort request
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTestExt::EresUnittest_Abort()
{
	// enable Abort simulation
	CAutoTraceFlag atfSet(EtraceSimulateAbort, true);

	// run simulation
	return EresUnittest_SimulateException(CException::ExmaSystem,
										  CException::ExmiAbort);
}


//---------------------------------------------------------------------------
//	@function:
//		CFSimulatorTestExt::EresUnittest_IOError
//
//	@doc:
//		Simulate an I/O error
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTestExt::EresUnittest_IOError()
{
	// enable I/O error simulation
	CAutoTraceFlag atfSet(EtraceSimulateIOError, true);

	// run simulation
	return EresUnittest_SimulateException(CException::ExmaSystem,
										  CException::ExmiIOError);
}


//---------------------------------------------------------------------------
//	@function:
//		CFSimulatorTestExt::EresUnittest_NetError
//
//	@doc:
//		Simulate an networking error
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTestExt::EresUnittest_NetError()
{
	// enable networking error simulation
	CAutoTraceFlag atfSet(EtraceSimulateNetError, true);

	// run simulation
	return EresUnittest_SimulateException(CException::ExmaSystem,
										  CException::ExmiNetError);
}


//---------------------------------------------------------------------------
//	@function:
//		CFSimulatorTestExt::EresUnittest_SimulateException
//
//	@doc:
//		Simulate exceptions of given type
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFSimulatorTestExt::EresUnittest_SimulateException(ULONG major, ULONG minor)
{
	// assemble -u option
	const CHAR *rgsz[] = {"", "-u"};

	while (true)
	{
		GPOS_TRY
		{
			CMainArgs ma(GPOS_ARRAY_SIZE(rgsz), rgsz, "u");
			CUnittest::Driver(&ma);

			// executed all tests w/o exception
			return GPOS_OK;
		}
		GPOS_CATCH_EX(ex)
		{
			GPOS_RESET_EX;

			// retry every time we hit an OOM, else bail
			if (!GPOS_MATCH_EX(ex, major, minor))
			{
				return GPOS_FAILED;
			}
		}
		GPOS_CATCH_END;
	}
}


#endif	// GPOS_FPSIMULATOR

// EOF
