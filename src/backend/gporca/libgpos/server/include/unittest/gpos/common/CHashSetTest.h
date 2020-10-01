//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CHashSetTest.h
//
//	@doc:
//		Test for CHashSet
//
//	@owner:
//		solimm1
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPOS_CHashSetTest_H
#define GPOS_CHashSetTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CHashSetTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CHashSetTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Ownership();

};	// class CHashSetTest
}  // namespace gpos

#endif	// !GPOS_CHashSetTest_H

// EOF
