//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CBitmapTest.h
//
//	@doc:
//		Test for optimizing queries that can use a bitmap index
//---------------------------------------------------------------------------
#ifndef GPOPT_CBitmapTest_H
#define GPOPT_CBitmapTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CBitmapTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CBitmapTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulBitmapTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunTests();

};	// class CBitmapTest
}  // namespace gpopt

#endif	// !GPOPT_CBitmapTest_H

// EOF
