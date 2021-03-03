//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluationDefaultTest.h
//
//	@doc:
//		Unit tests for CConstExprEvaluatorDefault
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CConstExprEvaluatorDefaultTest_H
#define GPOPT_CConstExprEvaluatorDefaultTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CConstExprEvaluatorDefaultTest
//
//	@doc:
//		Unit tests for CConstExprEvaluatorDefault
//
//---------------------------------------------------------------------------
class CConstExprEvaluatorDefaultTest
{
public:
	// run unittests
	static GPOS_RESULT EresUnittest();
};
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorDefaultTest_H

// EOF
