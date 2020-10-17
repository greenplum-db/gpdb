//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software
//
//	@filename:
//		CWindowTest.h
//
//	@doc:
//		Test for Window functions
//---------------------------------------------------------------------------
#ifndef GPOPT_CWindowTest_H
#define GPOPT_CWindowTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

class CWindowTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
};	// class CWindowTest
}  // namespace gpopt

#endif	// !GPOPT_CWindowTest_H

// EOF
