//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMDProviderTest.h
//
//	@doc:
//		Tests the metadata provider.
//---------------------------------------------------------------------------


#ifndef GPOPT_CMDProviderTest_H
#define GPOPT_CMDProviderTest_H

#include "gpos/base.h"
#include "naucrates/md/IMDProvider.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMDProviderTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------

class CMDProviderTest
{
private:
	// test lookup of MD objects with given MD provider
	static void TestMDLookup(CMemoryPool *mp, IMDProvider *pmdp);

public:
	// file for the file-based MD provider
	static const CHAR *file_name;

	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Stats();
	static GPOS_RESULT EresUnittest_Negative();


};	// class CMDProviderTest
}  // namespace gpdxl

#endif	// !GPOPT_CMDProviderTest_H

// EOF
