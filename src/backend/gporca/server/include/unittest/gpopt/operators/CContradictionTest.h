//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CContradictionTest.h
//
//	@doc:
//		Test for contradiction detection
//---------------------------------------------------------------------------
#ifndef GPOPT_CContradictionTest_H
#define GPOPT_CContradictionTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CContradictionTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CContradictionTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Constraint();

};	// class CContradictionTest
}  // namespace gpopt

#endif	// !GPOPT_CContradictionTest_H

// EOF
