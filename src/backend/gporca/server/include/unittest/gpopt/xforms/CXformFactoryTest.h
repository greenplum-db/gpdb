//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformFactoryTest.h
//
//	@doc:
//		Unittests for management of global xform set
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformFactoryTest_H
#define GPOPT_CXformFactoryTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformFactoryTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CXformFactoryTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CXformFactoryTest

}  // namespace gpopt


#endif	// !GPOPT_CXformFactoryTest_H

// EOF
