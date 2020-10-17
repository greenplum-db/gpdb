//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CNameTest.h
//
//	@doc:
//      Test for CName
//---------------------------------------------------------------------------
#ifndef GPOPT_CNameTest_H
#define GPOPT_CNameTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CNameTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CNameTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Ownership();

};	// class CNameTest
}  // namespace gpopt

#endif	// !GPOPT_CNameTest_H

// EOF
