//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CAggTest.h
//
//	@doc:
//		Test for optimizing queries with aggregates
//---------------------------------------------------------------------------
#ifndef GPOPT_CAggTest_H
#define GPOPT_CAggTest_H

#include "gpos/base.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CAggTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CAggTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulAggTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CAggTest
}  // namespace gpopt

#endif	// !GPOPT_CAggTest_H

// EOF
