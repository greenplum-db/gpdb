//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CMultilevelPartitionTest.h
//
//	@doc:
//		Test for optimizing queries on multilevel partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CMultilevelPartitionTest_H
#define GPOPT_CMultilevelPartitionTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMultilevelPartitionTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CMultilevelPartitionTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulMLPTTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunTests();

};	// class CMultilevelPartitionTest
}  // namespace gpopt

#endif	// !GPOPT_CMultilevelPartitionTest_H

// EOF
