//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CCTETest.h
//
//	@doc:
//		Test for optimizing queries with CTEs
//---------------------------------------------------------------------------
#ifndef GPOPT_CCTETest_H
#define GPOPT_CCTETest_H

#include "gpos/base.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CCTETest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CCTETest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulCTETestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CCTETest
}  // namespace gpopt

#endif	// !GPOPT_CCTETest_H

// EOF
