//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperDXLTest.h
//
//	@doc:
//		Test for DXL-based minidumoers
//---------------------------------------------------------------------------
#ifndef GPOPT_CMiniDumperDXLTest_H
#define GPOPT_CMiniDumperDXLTest_H

#include "gpos/base.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMiniDumperDXLTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CMiniDumperDXLTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Load();

};	// class CMiniDumperDXLTest
}  // namespace gpopt

#endif	// !GPOPT_CMiniDumperDXLTest_H

// EOF
