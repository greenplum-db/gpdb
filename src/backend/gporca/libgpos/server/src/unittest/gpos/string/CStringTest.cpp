//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CStringTest.cpp
//
//	@doc:
//		Tests for CStringStatic
//---------------------------------------------------------------------------

#include "unittest/gpos/string/CStringTest.h"

#include "gpos/base.h"
#include "gpos/string/CStringStatic.h"
#include "gpos/test/CUnittest.h"


using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CStringTest::EresUnittest
//
//	@doc:
//		Driver for unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStringTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CStringTest::EresUnittest_Equals),
		GPOS_UNITTEST_FUNC(CStringTest::EresUnittest_Append),
		GPOS_UNITTEST_FUNC(CStringTest::EresUnittest_AppendFormat),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CStringTest::EresUnittest_Append
//
//	@doc:
//		Test appending of strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStringTest::EresUnittest_Append()
{
	CHAR buffer1[16];
	CHAR buffer2[8];
	CHAR buffer3[8];

	CStringStatic ss1(buffer1, GPOS_ARRAY_SIZE(buffer1), "123");
	CStringStatic ss2(buffer2, GPOS_ARRAY_SIZE(buffer2), "456");
	CStringStatic ss3(buffer3, GPOS_ARRAY_SIZE(buffer3));

	ss1.Append(&ss2);

	GPOS_ASSERT(ss1.Equals("123456"));

	// append an empty string
	ss1.Append(&ss3);

	// string should be the same as before
	GPOS_ASSERT(ss1.Equals("123456"));

	// append to an empty string
	ss3.Append(&ss1);

	GPOS_ASSERT(ss3.Equals("123456"));

	// check truncation
	ss3.Append(&ss2);
	GPOS_ASSERT(ss3.Equals("1234564"));

	// test wide character string
	ss1.Reset();
	ss1.AppendConvert(GPOS_WSZ_LIT("Wide \x1111 character string"));
	GPOS_ASSERT(ss1.Equals("Wide . characte"));

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CStringTest::EresUnittest_AppendFormat
//
//	@doc:
//		Test formatting strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStringTest::EresUnittest_AppendFormat()
{
	CHAR buffer1[16];
	CHAR buffer2[12];

	CStringStatic ss1(buffer1, GPOS_ARRAY_SIZE(buffer1), "Hello");
	CStringStatic ss2(buffer2, GPOS_ARRAY_SIZE(buffer2), "Hello");

	ss1.AppendFormat(" world %d", 123);
	ss2.AppendFormat(" world %d", 123);

	GPOS_ASSERT(ss1.Equals("Hello world 123"));
	GPOS_ASSERT(ss2.Equals("Hello world"));

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CStringTest::EresUnittest_Equals
//
//	@doc:
//		Test checking for equality of strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStringTest::EresUnittest_Equals()
{
#ifdef GPOS_DEBUG
	// static strings
	CHAR buffer1[8];
	CHAR buffer2[8];
	CHAR buffer3[8];

	CStringStatic ss1(buffer1, GPOS_ARRAY_SIZE(buffer1), "123");
	CStringStatic ss2(buffer2, GPOS_ARRAY_SIZE(buffer2), "123");
	CStringStatic ss3(buffer3, GPOS_ARRAY_SIZE(buffer3), "12");

	GPOS_ASSERT(ss1.Equals(ss2.Buffer()));
	GPOS_ASSERT(!ss1.Equals(ss3.Buffer()));
	GPOS_ASSERT(!ss3.Equals(ss1.Buffer()));
#endif	// #ifdef GPOS_DEBUG

	return GPOS_OK;
}

// EOF
