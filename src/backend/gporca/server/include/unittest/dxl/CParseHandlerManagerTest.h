//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerManagerTest.h
//
//	@doc:
//		Tests parse handler manager, responsible for stacking up parse handlers
//		during DXL parsing.
//---------------------------------------------------------------------------


#ifndef GPOPT_CParseHandlerManagerTest_H
#define GPOPT_CParseHandlerManagerTest_H

#include "gpos/base.h"


namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerManagerTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------

class CParseHandlerManagerTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CParseHandlerManagerTest
}  // namespace gpdxl

#endif	// GPOPT_CParseHandlerManagerTest_H

// EOF
