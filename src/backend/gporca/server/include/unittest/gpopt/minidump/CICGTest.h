//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CICGTest.h
//
//	@doc:
//		Test for installcheck-good bugs
//---------------------------------------------------------------------------
#ifndef GPOPT_CICGTest_H
#define GPOPT_CICGTest_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLNode.h"

namespace gpopt
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CICGTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CICGTest
{
private:
	// function pointer type for checking predicates over DXL fragments
	typedef BOOL(FnDXLOpPredicate)(CDXLOperator *);

	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

	// counter used to mark last successful unsupported test
	static ULONG m_ulUnsupportedTestCounter;

	// counter used to mark last successful negative IndexApply test
	static ULONG m_ulNegativeIndexApplyTestCounter;

	// counter to mark last successful test for has joins versus index joins
	static ULONG m_ulTestCounterPreferHashJoinToIndexJoin;

	// counter to mark last successful test for index joins versus hash joins
	static ULONG m_ulTestCounterPreferIndexJoinToHashJoin;

	// counter to mark last successful test without additional traceflag
	static ULONG m_ulTestCounterNoAdditionTraceFlag;

	// check if all the operators in the given dxl fragment satisfy the given predicate
	static BOOL FDXLOpSatisfiesPredicate(CDXLNode *pdxl, FnDXLOpPredicate fdop);

	// check if the given dxl fragment does not contains Index Join
	static BOOL FIsNotIndexJoin(CDXLOperator *dxl_op);

	// check that the given dxl fragment does not contain an Index Join
	static BOOL FHasNoIndexJoin(CDXLNode *pdxl);

	// check that the given dxl fragment contains an Index Join
	static BOOL
	FHasIndexJoin(CDXLNode *pdxl)
	{
		return !FHasNoIndexJoin(pdxl);
	}

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunMinidumpTests();

	static GPOS_RESULT EresUnittest_RunUnsupportedMinidumpTests();

	static GPOS_RESULT EresUnittest_NegativeIndexApplyTests();

	// test that hash join is preferred versus index join when estimation risk is high
	static GPOS_RESULT
	EresUnittest_PreferHashJoinVersusIndexJoinWhenRiskIsHigh();

	static GPOS_RESULT EresUnittest_RunTestsWithoutAdditionalTraceFlags();

};	// class CICGTest
}  // namespace gpopt

#endif	// !GPOPT_CICGTest_H

// EOF
