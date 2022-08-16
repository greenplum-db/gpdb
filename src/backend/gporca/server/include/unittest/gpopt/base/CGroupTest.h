//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware Inc.
//
//	@filename:
//		CGroupTest.h
//
//	@doc:
//		Test for CGroup
//---------------------------------------------------------------------------
#ifndef GPOPT_CGroupTest_H
#define GPOPT_CGroupTest_H


#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/operators/CExpression.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CGroupTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CGroupTest
{
public:
	// main driver
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_FResetStatsOnCGroupWithDuplicateGroup();

};	// class CGroupTest
}  // namespace gpopt

#endif	// !GPOPT_CGroupTest_H


// EOF
