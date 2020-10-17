//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CErrorHandlerTest.h
//
//	@doc:
//		Test for CMessage
//---------------------------------------------------------------------------
#ifndef GPOS_CErrorHandlerTest_H
#define GPOS_CErrorHandlerTest_H

#include "gpos/types.h"
#include "gpos/assert.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CErrorHandlerTest
//
//	@doc:
//		Static unit tests for error handler base class
//
//---------------------------------------------------------------------------
class CErrorHandlerTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_BadRethrow();
	static GPOS_RESULT EresUnittest_BadReset();
	static GPOS_RESULT EresUnittest_Unhandled();
#endif	// GPOS_DEBUG
};
}  // namespace gpos

#endif	// !GPOS_CErrorHandlerTest_H

// EOF
