//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CDirectDispatchTest.h
//
//	@doc:
//		Test for direct dispatch
//---------------------------------------------------------------------------
#ifndef GPOPT_CDirectDispatchTest_H
#define GPOPT_CDirectDispatchTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDirectDispatchTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CDirectDispatchTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulDirectDispatchCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunTests();

};	// class CDirectDispatchTest
}  // namespace gpopt

#endif	// !GPOPT_CDirectDispatchTest_H

// EOF
