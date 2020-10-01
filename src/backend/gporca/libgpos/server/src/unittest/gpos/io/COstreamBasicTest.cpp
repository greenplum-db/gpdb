//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		COstreamBasicTest.cpp
//
//	@doc:
//		Tests for COstreamBasic
//---------------------------------------------------------------------------

#include "gpos/assert.h"
#include "gpos/error/CMessage.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/io/COstreamBasicTest.h"


using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		COstreamBasicTest::EresUnittest
//
//	@doc:
//		Function for raising assert exceptions; again, encapsulated in a function
//		to facilitate debugging
//
//---------------------------------------------------------------------------
GPOS_RESULT
COstreamBasicTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(COstreamBasicTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(COstreamBasicTest::EresUnittest_Strings),
		GPOS_UNITTEST_FUNC(COstreamBasicTest::EresUnittest_Numbers),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		COstreamBasicTest::EresUnittest_Basic
//
//	@doc:
//		test for non-string/non-number types;
//
//---------------------------------------------------------------------------
GPOS_RESULT
COstreamBasicTest::EresUnittest_Basic()
{
	// define basic stream over wide char out stream
	COstreamBasic osb(&std::wcout);

	// non-string, non-number types
	WCHAR wc = 'W';
	CHAR c = 'C';

	osb << c << wc << std::endl;
	osb << (void *) &wc << std::endl;

	return GPOS_OK;
}



//---------------------------------------------------------------------------
//	@function:
//		COstreamBasicTest::EresUnittest_Strings
//
//	@doc:
//		test for string types
//
//---------------------------------------------------------------------------
GPOS_RESULT
COstreamBasicTest::EresUnittest_Strings()
{
	// define basic stream over wide char out stream
	COstreamBasic ostream(&std::wcout);

	// all non-string, non-number types
	WCHAR wsz[] = GPOS_WSZ_LIT("test string for wide chars");
	CHAR sz[] = "test string for regular chars";

	ostream << wsz << std::endl;
	ostream << sz << std::endl;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		COstreamBasicTest::EresUnittest_Numbers
//
//	@doc:
//		test for numbers types
//
//---------------------------------------------------------------------------
GPOS_RESULT
COstreamBasicTest::EresUnittest_Numbers()
{
	// define basic stream over wide char out stream
	COstreamBasic osb(&std::wcout);

	const INT i = 0xdeadbeef;

	const ULONG ul = gpos::ulong_max;
	const ULLONG ull = gpos::ullong_max;
	const DOUBLE d = 1.23456789e20;

	// numbers in dec and hex formats
	osb << "int: " << COstream::EsmDec << i << COstream::EsmHex << " - 0x" << i
		<< std::endl;
	osb << "ulong: " << COstream::EsmDec << ul << COstream::EsmHex << " - 0x"
		<< ul << std::endl;
	osb << "ullong: " << COstream::EsmDec << ull << COstream::EsmHex << " - 0x"
		<< ull << std::endl;
	osb << "double: " << d << std::endl;

	return GPOS_OK;
}

// EOF
