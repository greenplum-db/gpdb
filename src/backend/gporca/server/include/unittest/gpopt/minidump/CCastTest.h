//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//---------------------------------------------------------------------------
#ifndef GPOPT_CCastTest_H
#define GPOPT_CCastTest_H

#include "gpos/base.h"

namespace gpopt
{
class CCastTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulTestCounter;

public:
	// unittests
	static gpos::GPOS_RESULT EresUnittest();

	static gpos::GPOS_RESULT EresUnittest_RunTests();

};	// class CCastTest
}  // namespace gpopt

#endif	// !GPOPT_CCastTest_H

// EOF
