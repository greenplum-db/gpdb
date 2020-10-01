//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CWStringTest.h
//
//	@doc:
//		Tests for the CWStringBase, CWStringConst, CWString classes
//---------------------------------------------------------------------------
#ifndef GPOS_CWStringTest_H
#define GPOS_CWStringTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CWStringTest
//
//	@doc:
//		Unittests for strings
//
//---------------------------------------------------------------------------
class CWStringTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Initialize();
	static GPOS_RESULT EresUnittest_Equals();
	static GPOS_RESULT EresUnittest_Append();
	static GPOS_RESULT EresUnittest_AppendFormat();
	static GPOS_RESULT EresUnittest_Copy();
	static GPOS_RESULT EresUnittest_AppendEscape();
	static GPOS_RESULT EresUnittest_AppendFormatLarge();
#ifndef GPOS_Darwin
	static GPOS_RESULT EresUnittest_AppendFormatInvalidLocale();
#endif
};	// class CWStringTest
}  // namespace gpos

#endif	// !GPOS_CWStringTest_H

// EOF
