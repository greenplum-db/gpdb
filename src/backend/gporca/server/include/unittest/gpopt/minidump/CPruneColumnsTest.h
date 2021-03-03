//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPruneColumnsTest.h
//
//	@doc:
//		Test for optimizing queries where intermediate columns are pruned
//---------------------------------------------------------------------------
#ifndef GPOPT_CPruneColumnsTest_H
#define GPOPT_CPruneColumnsTest_H

#include "gpos/base.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPruneColumnsTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CPruneColumnsTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulPruneColumnsTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CPruneColumnsTest
}  // namespace gpopt

#endif	// !GPOPT_CPruneColumnsTest_H

// EOF
