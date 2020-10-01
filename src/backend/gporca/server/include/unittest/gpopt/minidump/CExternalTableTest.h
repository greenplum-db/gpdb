//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CExternalTableTest.h
//
//	@doc:
//		Test for external tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CExternalTableTest_H
#define GPOPT_CExternalTableTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CExternalTableTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CExternalTableTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunMinidumpTests();
};	// class CExternalTableTest
}  // namespace gpopt

#endif	// !GPOPT_CExternalTableTest_H

// EOF
