//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CHashMapTest.h
//
//	@doc:
//		Test for CHashMap
//---------------------------------------------------------------------------
#ifndef GPOS_CHashMapTest_H
#define GPOS_CHashMapTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CHashMapTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CHashMapTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Ownership();

};	// class CHashMapTest
}  // namespace gpos

#endif	// !GPOS_CHashMapTest_H

// EOF
