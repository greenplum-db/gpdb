//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColRefSetTest.h
//
//	@doc:
//	    Test for CColRefSet
//---------------------------------------------------------------------------
#ifndef GPOS_CColRefSetTest_H
#define GPOS_CColRefSetTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CColRefSetTest
//
//	@doc:
//		Static unit tests for column reference set
//
//---------------------------------------------------------------------------
class CColRefSetTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class CColRefSetTest
}  // namespace gpopt

#endif	// !GPOS_CColRefSetTest_H


// EOF
