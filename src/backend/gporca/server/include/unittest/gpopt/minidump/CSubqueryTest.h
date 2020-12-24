//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CSubqueryTest.h
//
//	@doc:
//		Test for subquery optimization
//---------------------------------------------------------------------------
#ifndef GPOPT_CSubqueryTest_H
#define GPOPT_CSubqueryTest_H

#include "gpos/base.h"

namespace gpopt
{
class CSubqueryTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulSubQueryTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CSubqueryTest
}  // namespace gpopt

#endif	// !GPOPT_CSubqueryTest_H

// EOF
