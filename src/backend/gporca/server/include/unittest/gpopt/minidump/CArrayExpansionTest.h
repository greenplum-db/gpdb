//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates
//
//	@filename:
//		CArrayExpansionTest.h
//
//	@doc:
//		Test for Array expansion in WHERE clause
//
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CArrayExpansionTest_H
#define GPOPT_CArrayExpansionTest_H

#include "gpos/base.h"

namespace gpopt
{
class CArrayExpansionTest
{
public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();
};	// class CArrayExpansionTest
}  // namespace gpopt

#endif	// !GPOPT_CArrayExpansionTest_H

// EOF
