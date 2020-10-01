//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		COrderSpecTest.h
//
//	@doc:
//		Test for order spec
//---------------------------------------------------------------------------
#ifndef GPOS_COrderSpecTest_H
#define GPOS_COrderSpecTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		COrderSpecTest
//
//	@doc:
//		Static unit tests for order specs
//
//---------------------------------------------------------------------------
class COrderSpecTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class COrderSpecTest
}  // namespace gpopt

#endif	// !GPOS_COrderSpecTest_H


// EOF
