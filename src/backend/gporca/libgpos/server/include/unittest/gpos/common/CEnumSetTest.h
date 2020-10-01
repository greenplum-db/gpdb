//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CEnumSetTest.h
//
//	@doc:
//		Test for enum sets
//---------------------------------------------------------------------------
#ifndef GPOS_CEnumSetTest_H
#define GPOS_CEnumSetTest_H

#include "gpos/types.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CEnumSetTest
//
//	@doc:
//		Static unit tests for enum set
//
//---------------------------------------------------------------------------
class CEnumSetTest
{
public:
	enum eTest
	{
		eTestOne,
		eTestTwo,

		eTestSentinel
	};

	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class CEnumSetTest
}  // namespace gpos

#endif	// !GPOS_CEnumSetTest_H

// EOF
