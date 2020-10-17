//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CLoggerTest.h
//
//	@doc:
//      Unit test for logger classes.
//---------------------------------------------------------------------------
#ifndef GPOS_CLoggerTest_H
#define GPOS_CLoggerTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CLoggerTest
//
//	@doc:
//		Unittests for log functionality
//
//---------------------------------------------------------------------------
class CLoggerTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_LoggerSyslog();

};	// CLoggerTest
}  // namespace gpos

#endif	// !GPOS_CLoggerTest_H

// EOF
