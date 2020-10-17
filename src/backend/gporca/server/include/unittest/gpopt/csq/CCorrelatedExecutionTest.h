//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CCorrelatedExecutionTest.h
//
//	@doc:
//		Test for correlated subqueries
//---------------------------------------------------------------------------
#ifndef GPOPT_CCorrelatedExecutionTest_H
#define GPOPT_CCorrelatedExecutionTest_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/base/CColRef.h"

#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/operators/CExpression.h"

// forward declarations
namespace gpdxl
{
typedef CDynamicPtrArray<INT, CleanupDelete> IntPtrArray;
}

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CCorrelatedExecutionTest
//
//	@doc:
//		Tests for converting Apply expressions into NL expressions
//
//---------------------------------------------------------------------------
class CCorrelatedExecutionTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_RunAllPositiveTests();

};	// class CCorrelatedExecutionTest
}  // namespace gpopt


#endif	// !GPOPT_CCorrelatedExecutionTest_H

// EOF
