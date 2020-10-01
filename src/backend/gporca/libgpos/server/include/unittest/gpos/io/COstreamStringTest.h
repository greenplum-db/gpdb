//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		COstreamStringTest.h
//
//	@doc:
//		Test for COstreamString
//---------------------------------------------------------------------------
#ifndef GPOS_COstreamStringTest_H
#define GPOS_COstreamStringTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		COstreamStringTest
//
//	@doc:
//		Static unit tests for messages
//
//---------------------------------------------------------------------------
class COstreamStringTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_EndlAssert();
#endif
};
}  // namespace gpos

#endif	// !GPOS_COstreamStringTest_H

// EOF
