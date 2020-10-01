//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CDMLTest.h
//
//	@doc:
//		Test for optimizing DML queries
//---------------------------------------------------------------------------
#ifndef GPOPT_CDMLTest_H
#define GPOPT_CDMLTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDMLTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CDMLTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulDMLTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunTests();

};	// class CDMLTest
}  // namespace gpopt

#endif	// !GPOPT_CDMLTest_H

// EOF
