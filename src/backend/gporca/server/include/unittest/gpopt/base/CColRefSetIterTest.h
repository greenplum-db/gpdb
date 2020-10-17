//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColRefSetIterTest.h
//
//	@doc:
//		Tests for CColRefSetIter
//---------------------------------------------------------------------------
#ifndef GPOS_CColRefSetIterTest_H
#define GPOS_CColRefSetIterTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CColRefSetIterTest
//
//	@doc:
//		Static unit tests for col ref set
//
//---------------------------------------------------------------------------
class CColRefSetIterTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class CColRefSetIterTest
}  // namespace gpopt

#endif	// !GPOS_CColRefSetIterTest_H


// EOF
