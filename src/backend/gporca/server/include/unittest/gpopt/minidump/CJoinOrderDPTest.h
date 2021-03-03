//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates
//
//	@filename:
//		CJoinOrderDPTest.h
//
//	@doc:
//		Testing guc for disabling dynamic join order algorithm
//---------------------------------------------------------------------------
#ifndef GPOPT_CJoinOrderDPTest_H
#define GPOPT_CJoinOrderDPTest_H

#include "gpos/base.h"

namespace gpopt
{
class CJoinOrderDPTest
{
public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();
};	// class CJoinOrderDPTest
}  // namespace gpopt

#endif	// !GPOPT_CJoinOrderDPTest_H

// EOF
