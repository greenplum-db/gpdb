//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CBitSetIterTest.h
//
//	@doc:
//		Test for CBitSetIter
//---------------------------------------------------------------------------
#ifndef GPOS_CBitSetIterTest_H
#define GPOS_CBitSetIterTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CBitSetIterTest
//
//	@doc:
//		Static unit tests for bit set
//
//---------------------------------------------------------------------------
class CBitSetIterTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_Uninitialized();
	static GPOS_RESULT EresUnittest_Overrun();
#endif	// GPOS_DEBUG

};	// class CBitSetIterTest
}  // namespace gpos

#endif	// !GPOS_CBitSetIterTest_H

// EOF
