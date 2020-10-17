//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CStackTest.h
//
//	@doc:
//		Test for CStack
//---------------------------------------------------------------------------
#ifndef GPOS_CStackTest_H
#define GPOS_CStackTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CStackTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CStackTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_PushPop();

#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_Pop();
#endif

	// destructor function for char's
	static void DestroyChar(char *);

};	// class CStackTest
}  // namespace gpos

#endif	// !GPOS_CStackTest_H

// EOF
