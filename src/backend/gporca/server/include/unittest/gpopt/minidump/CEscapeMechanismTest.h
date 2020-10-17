//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal, Inc.
//
//	@filename:
//		CEscapeMechanismTest.h
//
//	@doc:
//		Test for optimizing queries for exploring fewer alternatives and
//		run optimization process faster
//---------------------------------------------------------------------------
#ifndef GPOPT_CEscapeMechanismTest_H
#define GPOPT_CEscapeMechanismTest_H

#include "gpos/base.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CEscapeMechanismTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CEscapeMechanismTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulEscapeMechanismTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CEscapeMechanismTest
}  // namespace gpopt

#endif	// !GPOPT_CEscapeMechanismTest_H

// EOF
