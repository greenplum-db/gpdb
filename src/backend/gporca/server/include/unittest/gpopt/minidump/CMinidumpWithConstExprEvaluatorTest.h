//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CMinidumpWithConstExprEvaluatorTest.h
//
//	@doc:
//		Tests minidumps with constant expression evaluator turned on
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CMinidumpWithConstExprEvaluatorTest_H
#define GPOPT_CMinidumpWithConstExprEvaluatorTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMinidumpWithConstExprEvaluatorTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CMinidumpWithConstExprEvaluatorTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunMinidumpTestsWithConstExprEvaluatorOn();

};	// class CMinidumpWithConstExprEvaluatorTest
}  // namespace gpopt

#endif	// !GPOPT_CMinidumpWithConstExprEvaluatorTest_H

// EOF
