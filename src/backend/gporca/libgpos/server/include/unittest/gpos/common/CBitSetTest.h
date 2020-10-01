//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CBitSetTest.h
//
//	@doc:
//		Test for CBitSet
//---------------------------------------------------------------------------
#ifndef GPOS_CBitSetTest_H
#define GPOS_CBitSetTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CBitSetTest
//
//	@doc:
//		Static unit tests for bit set
//
//---------------------------------------------------------------------------
class CBitSetTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();
	static GPOS_RESULT EresUnittest_Removal();
	static GPOS_RESULT EresUnittest_SetOps();
	static GPOS_RESULT EresUnittest_Performance();

};	// class CBitSetTest
}  // namespace gpos

#endif	// !GPOS_CBitSetTest_H

// EOF
