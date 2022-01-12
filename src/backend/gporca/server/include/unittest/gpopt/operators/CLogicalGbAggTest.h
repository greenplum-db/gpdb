//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware, Inc.
//
//	@filename:
//		CLogicalGbAggTest.h
//
//	@doc:
//		Tests for logical group by aggregate operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalGbAggTest_H
#define GPOPT_CLogicalGbAggTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalGbAggTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CLogicalGbAggTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_PxfsCandidates();

};	// class CLogicalGbAggTest
}  // namespace gpopt

#endif	// !GPOPT_CLogicalGbAggTest_H

// EOF
