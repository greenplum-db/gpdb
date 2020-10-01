//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLUtilsTest.h
//
//	@doc:
//		Tests the DXL utility functions
//---------------------------------------------------------------------------


#ifndef GPOPT_CDXLUtilsTest_H
#define GPOPT_CDXLUtilsTest_H

#include "gpos/base.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLUtilsTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------

class CDXLUtilsTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_SerializeQuery();
	static GPOS_RESULT EresUnittest_SerializePlan();
	static GPOS_RESULT EresUnittest_Encoding();

};	// class CDXLUtilsTest
}  // namespace gpdxl

#endif	// !GPOPT_CDXLUtilsTest_H

// EOF
