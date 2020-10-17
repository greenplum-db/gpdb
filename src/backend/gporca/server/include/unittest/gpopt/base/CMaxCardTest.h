//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2012 EMC Corp.
//
//	@filename:
//		CMaxCardTest.h
//
//	@doc:
//		Test for max card functionality
//---------------------------------------------------------------------------
#ifndef GPOS_CMaxCardTest_H
#define GPOS_CMaxCardTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMaxCardTest
//
//	@doc:
//		Static unit tests for max card computation
//
//---------------------------------------------------------------------------
class CMaxCardTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();
	static GPOS_RESULT EresUnittest_RunMinidumpTests();

};	// class CMaxCardTest
}  // namespace gpopt

#endif	// !GPOS_CMaxCardTest_H


// EOF
