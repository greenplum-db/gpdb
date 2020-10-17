//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CBindingTest.h
//
//	@doc:
//		Test for checking bindings extracted for an expression
//---------------------------------------------------------------------------
#ifndef GPOPT_CBindingTest_H
#define GPOPT_CBindingTest_H

#include "gpos/base.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CBindingTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CBindingTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CBindingTest
}  // namespace gpopt

#endif	// !GPOPT_CBindingTest_H

// EOF
