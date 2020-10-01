//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CStringTest.h
//
//	@doc:
//		Tests for the CStringStatic class
//---------------------------------------------------------------------------
#ifndef GPOS_CStringTest_H
#define GPOS_CStringTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CStringTest
//
//	@doc:
//		Unittests for strings
//
//---------------------------------------------------------------------------
class CStringTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Equals();
	static GPOS_RESULT EresUnittest_Append();
	static GPOS_RESULT EresUnittest_AppendFormat();
};	// class CStringTest
}  // namespace gpos

#endif	// !GPOS_CStringTest_H

// EOF
