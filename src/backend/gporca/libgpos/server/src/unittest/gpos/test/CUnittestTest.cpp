//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Software, Inc.
//
//	@filename:
//		CUnittestTest.cpp
//
//	@doc:
//		Test for CUnittest with subtests
//---------------------------------------------------------------------------

#include "unittest/gpos/test/CUnittestTest.h"

#include "gpos/base.h"
#include "gpos/types.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@function:
//		CUnittestTest::EresSubtest
//
//	@doc:
//		Driver for trivial subtest.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CUnittestTest::EresSubtest(ULONG ulSubtest)
{
	if (ulSubtest * 1 == ulSubtest)
	{
		return GPOS_OK;
	}
	else
	{
		return GPOS_FAILED;
	}
}

}  // namespace gpos

// EOF
