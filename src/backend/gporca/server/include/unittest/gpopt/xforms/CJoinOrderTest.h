//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CJoinOrderTest.h
//
//	@doc:
//		Tests for join ordering
//---------------------------------------------------------------------------
#ifndef GPOPT_CJoinOrderTest_H
#define GPOPT_CJoinOrderTest_H

#include "gpos/base.h"

#include "gpopt/operators/CExpression.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CJoinOrderTest
//
//	@doc:
//		Tests for join ordering
//
//---------------------------------------------------------------------------
class CJoinOrderTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_ExpandMinCard();
	static GPOS_RESULT EresUnittest_RunTests();

};	// class CJoinOrderTest
}  // namespace gpopt


#endif	// !GPOPT_CJoinOrderTest_H

// EOF
