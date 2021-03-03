//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CTVFTest.h
//
//	@doc:
//		Test for optimizing queries with TVF
//---------------------------------------------------------------------------
#ifndef GPOPT_CTVFTest_H
#define GPOPT_CTVFTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CTVFTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CTVFTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulTVFTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunTests();

};	// class CTVFTest
}  // namespace gpopt

#endif	// !GPOPT_CTVFTest_H

// EOF
