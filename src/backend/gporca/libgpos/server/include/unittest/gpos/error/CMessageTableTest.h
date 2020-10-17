//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMessageTableTest.h
//
//	@doc:
//		Test for CMessageTable
//---------------------------------------------------------------------------
#ifndef GPOS_CMessageTableTest_H
#define GPOS_CMessageTableTest_H

#include "gpos/types.h"
#include "gpos/assert.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CMessageTableTest
//
//	@doc:
//		Static unit tests for message table
//
//---------------------------------------------------------------------------
class CMessageTableTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
};
}  // namespace gpos

#endif	// !GPOS_CMessageTableTest_H

// EOF
