//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CHashMapIterTest.h
//
//	@doc:
//		Test for CHashMapIter
//---------------------------------------------------------------------------
#ifndef GPOS_CHashMapIterTest_H
#define GPOS_CHashMapIterTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CHashMapIterTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CHashMapIterTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CHashMapIterTest
}  // namespace gpos

#endif	// !GPOS_CHashMapIterTest_H

// EOF
