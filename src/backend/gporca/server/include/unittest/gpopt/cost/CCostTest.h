//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CCostTest.h
//
//	@doc:
//		Basic tests for costing
//---------------------------------------------------------------------------
#ifndef GPOPT_CCostTest_H
#define GPOPT_CCostTest_H

#include "gpos/base.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CCostTest
//
//	@doc:
//		Unittests for costing
//
//---------------------------------------------------------------------------
class CCostTest
{
private:
	// test cost model parameters
	static void TestParams(CMemoryPool *mp, BOOL fCalibrated);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Arithmetic();
	static GPOS_RESULT EresUnittest_Bool();
	static GPOS_RESULT EresUnittest_Params();
	static GPOS_RESULT EresUnittest_Parsing();
	static GPOS_RESULT EresUnittest_ParsingWithException();
	static GPOS_RESULT EresUnittest_SetParams();

};	// class CCostTest
}  // namespace gpopt

#endif	// !GPOPT_CCostTest_H

// EOF
