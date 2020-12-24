//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CCollapseProjectTest.h
//
//	@doc:
//		Test for optimizing queries with multiple project nodes
//---------------------------------------------------------------------------
#ifndef GPOPT_CCollapseProjectTest_H
#define GPOPT_CCollapseProjectTest_H

#include "gpos/base.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CCollapseProjectTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CCollapseProjectTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulCollapseProjectTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CCollapseProjectTest
}  // namespace gpopt

#endif	// !GPOPT_CCollapseProjectTest_H

// EOF
