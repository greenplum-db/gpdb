//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMessageTest.h
//
//	@doc:
//		Test for CMessage
//---------------------------------------------------------------------------
#ifndef GPOS_CMessageTest_H
#define GPOS_CMessageTest_H

#include "gpos/types.h"
#include "gpos/assert.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CMessageTest
//
//	@doc:
//		Static unit tests for messages
//
//---------------------------------------------------------------------------
class CMessageTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_BasicWrapper();
	static GPOS_RESULT EresUnittest_Basic(const void *, ...);
};
}  // namespace gpos

#endif	// !GPOS_CMessageTest_H

// EOF
