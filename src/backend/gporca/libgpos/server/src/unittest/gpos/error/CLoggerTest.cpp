//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CLoggerTest
//
//	@doc:
//		Unit test for logger classes.
//---------------------------------------------------------------------------

#include "unittest/gpos/error/CLoggerTest.h"

#include "gpos/error/CLoggerStream.h"
#include "gpos/error/CLoggerSyslog.h"
#include "gpos/error/ILogger.h"
#include "gpos/io/ioutils.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CLoggerTest::EresUnittest
//
//	@doc:
//		Driver for unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CLoggerTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CLoggerTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CLoggerTest::EresUnittest_LoggerSyslog),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CLoggerTest::EresUnittest_Basic
//
//	@doc:
//		Basic test for logging
//
//---------------------------------------------------------------------------
GPOS_RESULT
CLoggerTest::EresUnittest_Basic()
{
	// log trace messages
	GPOS_TRACE(GPOS_WSZ_LIT("Log trace message as built string"));
	GPOS_TRACE_FORMAT("Log trace message as %s %s", "formatted", "string");

	{
		// disable Abort simulation;
		// simulation leads to self-deadlock while trying to log injection
		CAutoTraceFlag atfSet(EtraceSimulateAbort, false);

		// log warning message
		GPOS_WARNING(CException::ExmaSystem, CException::ExmiDummyWarning,
					 "Foo");
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CLoggerTest::EresUnittest_LoggerSyslog
//
//	@doc:
//		Log to file
//
//---------------------------------------------------------------------------
GPOS_RESULT
CLoggerTest::EresUnittest_LoggerSyslog()
{
	GPOS_SYSLOG_ALERT("This is test message 1 from GPOS to syslog");
	GPOS_SYSLOG_ALERT("This is test message 2 from GPOS to syslog");
	GPOS_SYSLOG_ALERT("This is test message 3 from GPOS to syslog");

	return GPOS_OK;
}


// EOF
