//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CWStringTest.cpp
//
//	@doc:
//		Tests for CWStringBase, CWString, and CWStringConst
//---------------------------------------------------------------------------

#include "unittest/gpos/string/CWStringTest.h"

#include <locale.h>

#include "gpos/base.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/string/CWStringConst.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/string/CWStringStatic.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest
//
//	@doc:
//		Driver for unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_Initialize),
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_Equals),
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_Append),
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_AppendFormat),
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_Copy),
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_AppendEscape),
		GPOS_UNITTEST_FUNC(CWStringTest::EresUnittest_AppendFormatLarge),
#ifndef GPOS_Darwin
		GPOS_UNITTEST_FUNC(
			CWStringTest::EresUnittest_AppendFormatInvalidLocale),
#endif
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_Append
//
//	@doc:
//		Test appending of strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_Append()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstr1 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("123"));
	CWStringDynamic *pstr2 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("456"));
	CWStringDynamic *pstr3 = GPOS_NEW(mp) CWStringDynamic(mp);

	WCHAR buffer1[8];
	WCHAR buffer2[8];
	WCHAR buffer3[8];

	CWStringStatic ss1(buffer1, GPOS_ARRAY_SIZE(buffer1), GPOS_WSZ_LIT("123"));
	CWStringStatic ss2(buffer2, GPOS_ARRAY_SIZE(buffer2), GPOS_WSZ_LIT("456"));
	CWStringStatic ss3(buffer3, GPOS_ARRAY_SIZE(buffer3));

	pstr1->Append(pstr2);
	ss1.Append(&ss2);

#ifdef GPOS_DEBUG
	CWStringConst cstr1(GPOS_WSZ_LIT("123456"));
	CWStringConst cstr2(GPOS_WSZ_LIT("1234564"));
#endif

	GPOS_ASSERT(pstr1->Equals(&cstr1));
	GPOS_ASSERT(ss1.Equals(&cstr1));

	// append an empty string
	pstr1->Append(pstr3);
	ss1.Append(&ss3);

	// string should be the same as before
	GPOS_ASSERT(pstr1->Equals(&cstr1));
	GPOS_ASSERT(ss1.Equals(&cstr1));

	// append to an empty string
	pstr3->Append(pstr1);
	ss3.Append(&ss1);

	GPOS_ASSERT(pstr3->Equals(pstr1));
	GPOS_ASSERT(ss3.Equals(&ss1));

	// check truncation
	ss3.Append(&ss2);
	GPOS_ASSERT(ss3.Equals(&cstr2));

	// cleanup
	GPOS_DELETE(pstr1);
	GPOS_DELETE(pstr2);
	GPOS_DELETE(pstr3);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_AppendFormat
//
//	@doc:
//		Test formatting strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_AppendFormat()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstr1 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("Hello"));

	WCHAR buffer1[16];
	WCHAR buffer2[12];

	CWStringStatic ss1(buffer1, GPOS_ARRAY_SIZE(buffer1),
					   GPOS_WSZ_LIT("Hello"));
	CWStringStatic ss2(buffer2, GPOS_ARRAY_SIZE(buffer2),
					   GPOS_WSZ_LIT("Hello"));

	pstr1->AppendFormat(GPOS_WSZ_LIT(" world %d"), 123);
	ss1.AppendFormat(GPOS_WSZ_LIT(" world %d"), 123);
	ss2.AppendFormat(GPOS_WSZ_LIT(" world %d"), 123);

	CWStringConst cstr1(GPOS_WSZ_LIT("Hello world 123"));
#ifdef GPOS_DEBUG
	CWStringConst cstr2(GPOS_WSZ_LIT("Hello world"));
#endif

	GPOS_ASSERT(pstr1->Equals(&cstr1));
	GPOS_ASSERT(ss1.Equals(&cstr1));
	GPOS_ASSERT(ss2.Equals(&cstr2));

	GPOS_RESULT eres = GPOS_OK;

	// cleanup
	GPOS_DELETE(pstr1);

	return eres;
}



// This tests the behavior of AppendFormat for unicode characters when the locale settings of the system are incompatible
// AppendFormat uses a C library function, vswprintf(), whose behavior is platform specific.
// vswprintf() returns with error on non-Darwin platforms when the locale is incompatible with unicode string.
// Hence do not run this test on Darwin platform.
#ifndef GPOS_Darwin
GPOS_RESULT
CWStringTest::EresUnittest_AppendFormatInvalidLocale()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	CHAR *oldLocale = setlocale(LC_CTYPE, NULL);
	CWStringDynamic *pstr1 = GPOS_NEW(mp) CWStringDynamic(mp);

	GPOS_RESULT eres = GPOS_OK;

	setlocale(LC_CTYPE, "C");
	GPOS_TRY
	{
		pstr1->AppendFormat(GPOS_WSZ_LIT("%s"), (CHAR *) "ÃË", 123);

		eres = GPOS_FAILED;
	}
	GPOS_CATCH_EX(ex)
	{
		GPOS_ASSERT(GPOS_MATCH_EX(ex, CException::ExmaSystem,
								  CException::ExmiIllegalByteSequence));

		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	// cleanup
	setlocale(LC_CTYPE, oldLocale);
	GPOS_DELETE(pstr1);

	return eres;
}
#endif

//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_AppendFormatLarge
//
//	@doc:
//		Test formatting large strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_AppendFormatLarge()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstr1 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("Hello"));
	CWStringConst cstr1(GPOS_WSZ_LIT(" World "));

	const ULONG ulLengthInit = pstr1->Length();
	const ULONG ulLengthAppend = cstr1.Length();
	const ULONG ulIters = 1000;
	const ULONG ulExpected = ulLengthInit + ulLengthAppend * ulIters;

	for (ULONG ul = 0; ul < ulIters; ul++)
	{
		pstr1->AppendFormat(GPOS_WSZ_LIT("%ls"), cstr1.GetBuffer());
	}

	{
		CAutoTrace at(mp);
		at.Os() << std::endl << "Formatted string size:" << pstr1->Length();
		at.Os() << std::endl
				<< "Expected string size:" << ulExpected << std::endl;
	}

	GPOS_ASSERT(pstr1->Length() == ulExpected);

	// cleanup
	GPOS_DELETE(pstr1);

	// append small string
	CWStringDynamic *pstr2 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("Hello"));
	pstr2->AppendCharArray(" World");
	GPOS_TRACE(pstr2->GetBuffer());

	// cleanup
	GPOS_DELETE(pstr2);

	// append large string
	CWStringDynamic *pstr3 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("Hello "));
#ifdef GPOS_DEBUG
	ULONG ulStartLength = pstr3->Length();
#endif	// GPOS_DEBUG
	const ULONG ulAppendLength = 50000;
	CHAR *sz = GPOS_NEW_ARRAY(mp, CHAR, ulAppendLength + 1);
	for (ULONG ul = 0; ul < ulAppendLength; ul++)
	{
		sz[ul] = 'W';
	}
	sz[ulAppendLength] = '\0';

	// append a large string
	pstr3->AppendCharArray(sz);
	GPOS_ASSERT(ulAppendLength + ulStartLength == pstr3->Length());
	GPOS_DELETE_ARRAY(sz);

	// do another append of a small string
	pstr3->AppendCharArray(" World");

	pstr3->AppendWideCharArray(GPOS_WSZ_LIT(" WIDE WORLD"));
	GPOS_TRACE(pstr3->GetBuffer());
	GPOS_TRACE(GPOS_WSZ_LIT("\n"));

	// cleanup
	GPOS_DELETE(pstr3);

	WCHAR w_str[25];
	CWStringStatic *pstr4 = GPOS_NEW(mp)
		CWStringStatic(w_str, GPOS_ARRAY_SIZE(w_str), GPOS_WSZ_LIT("Hello"));
	pstr4->AppendCharArray(" World");
	pstr4->AppendWideCharArray(GPOS_WSZ_LIT(" WIDE WORLD"));

	// another append should be truncated since we will overflow string
	pstr4->AppendWideCharArray(L" 1234567");
	GPOS_TRACE(pstr4->GetBuffer());
	GPOS_TRACE(GPOS_WSZ_LIT("\n"));

	GPOS_ASSERT(pstr4->GetBuffer()[pstr4->Length() - 1] == L'1');

	// cleanup
	GPOS_DELETE(pstr4);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_Initialize
//
//	@doc:
//		Test string construction
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_Initialize()
{
#ifdef GPOS_DEBUG  // run this test in debug mode only
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstr1 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("123"));

	CWStringConst cstr1(GPOS_WSZ_LIT("123"));
	GPOS_ASSERT(pstr1->Equals(&cstr1));

	// empty string initialization
	CWStringDynamic *pstr2 = GPOS_NEW(mp) CWStringDynamic(mp);
	WCHAR buffer[16];
	CWStringStatic ss(buffer, GPOS_ARRAY_SIZE(buffer));

	CWStringConst cstr2(GPOS_WSZ_LIT(""));
	GPOS_ASSERT(pstr2->Equals(&cstr2));
	GPOS_ASSERT(ss.Equals(&cstr2));
	GPOS_ASSERT(0 == pstr2->Length());
	GPOS_ASSERT(0 == ss.Length());

	// constant string initialization
	CWStringConst *pcstr1 = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("123"));
	GPOS_ASSERT(pcstr1->Equals(&cstr1));

	// cleanup
	GPOS_DELETE(pstr1);
	GPOS_DELETE(pstr2);
	GPOS_DELETE(pcstr1);

#endif	// #ifdef GPOS_DEBUG
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_Equals
//
//	@doc:
//		Test checking for equality of strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_Equals()
{
#ifdef GPOS_DEBUG
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// dynamic strings
	CWStringDynamic *str1 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("123"));
	CWStringDynamic *str2 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("123"));
	CWStringDynamic *str3 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("12"));

	GPOS_ASSERT(str1->Equals(str2));
	GPOS_ASSERT(!str1->Equals(str3));
	GPOS_ASSERT(!str3->Equals(str1));

	// static strings
	WCHAR buffer1[8];
	WCHAR buffer2[8];
	WCHAR buffer3[8];

	CWStringStatic ss1(buffer1, GPOS_ARRAY_SIZE(buffer1), GPOS_WSZ_LIT("123"));
	CWStringStatic ss2(buffer2, GPOS_ARRAY_SIZE(buffer2), GPOS_WSZ_LIT("123"));
	CWStringStatic ss3(buffer3, GPOS_ARRAY_SIZE(buffer3), GPOS_WSZ_LIT("12"));

	GPOS_ASSERT(ss1.Equals(&ss2));
	GPOS_ASSERT(!ss1.Equals(&ss3));
	GPOS_ASSERT(!ss3.Equals(&ss1));

	// Const strings
	CWStringConst *cstr1 = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("123"));
	CWStringConst *cstr2 = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("12"));
	GPOS_ASSERT(!cstr1->Equals(cstr2));
	GPOS_ASSERT(cstr1->Equals(str1));

	// cleanup
	GPOS_DELETE(str1);
	GPOS_DELETE(str2);
	GPOS_DELETE(str3);
	GPOS_DELETE(cstr1);
	GPOS_DELETE(cstr2);

#endif	// #ifdef GPOS_DEBUG

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_Copy
//
//	@doc:
//		Test deep copying of strings
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_Copy()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstr1 =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("Hello"));

	CWStringConst *pcstr1 = pstr1->Copy(mp);

	GPOS_ASSERT(pstr1->Equals(pcstr1));

	// character buffers should be different
	GPOS_ASSERT(pstr1->GetBuffer() != pcstr1->GetBuffer());

	// cleanup
	GPOS_DELETE(pstr1);

	GPOS_ASSERT(NULL != pcstr1->GetBuffer());
	GPOS_DELETE(pcstr1);

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CWStringTest::EresUnittest_AppendEscape
//
//	@doc:
//		Test replacing character with string
//
//---------------------------------------------------------------------------
GPOS_RESULT
CWStringTest::EresUnittest_AppendEscape()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	const WCHAR *w_str = GPOS_WSZ_LIT("Helloe ");
	CWStringConst cstr(w_str);

	CWStringDynamic strd(mp, w_str);

	WCHAR buffer1[32];
	WCHAR buffer2[9];

	CWStringStatic strs1(buffer1, GPOS_ARRAY_SIZE(buffer1), w_str);
	CWStringStatic strs2(buffer2, GPOS_ARRAY_SIZE(buffer2), w_str);

	GPOS_ASSERT(1 == strd.Find('e'));
	GPOS_ASSERT(1 == strs1.Find('e'));
	GPOS_ASSERT(1 == strs2.Find('e'));

	GPOS_ASSERT(2 == strd.Find('l'));
	GPOS_ASSERT(6 == strs1.Find(' '));
	GPOS_ASSERT(-1 == strs2.Find('a'));

	strd.Reset();
	strs1.Reset();
	strs2.Reset();

	strd.AppendEscape(&cstr, 'e', GPOS_WSZ_LIT("yyy"));
	strs1.AppendEscape(&cstr, 'e', GPOS_WSZ_LIT("yyy"));
	strs2.AppendEscape(&cstr, 'e', GPOS_WSZ_LIT("yyy"));

#ifdef GPOS_DEBUG
	CWStringConst cstr1(GPOS_WSZ_LIT("Hyyylloyyy "));
	CWStringConst cstr2(GPOS_WSZ_LIT("Hyyylloy"));
#endif	// GPOS_DEBUG

	GPOS_ASSERT(strd.Equals(&cstr1));
	GPOS_ASSERT(strs1.Equals(&cstr1));
	GPOS_ASSERT(strs2.Equals(&cstr2));

	strd.AppendEscape(&cstr, 'a', GPOS_WSZ_LIT("yyy"));
	strs1.AppendEscape(&cstr, 'a', GPOS_WSZ_LIT("yyy"));
	strs2.AppendEscape(&cstr, 'a', GPOS_WSZ_LIT("yyy"));

#ifdef GPOS_DEBUG
	CWStringConst cstr3(GPOS_WSZ_LIT("Hyyylloyyy Helloe "));
#endif	// GPOS_DEBUG

	// should be the same
	GPOS_ASSERT(strd.Equals(&cstr3));
	GPOS_ASSERT(strs1.Equals(&cstr3));
	GPOS_ASSERT(strs2.Equals(&cstr2));

	// check escaped characters
	const WCHAR *wszEscape1 = GPOS_WSZ_LIT("   \\\" ");
	const WCHAR *wszEscape2 = GPOS_WSZ_LIT("   \\\\\" ");
	const WCHAR *wszEscape3 = GPOS_WSZ_LIT("   \\\\\\\" ");
	const WCHAR *wszEscape4 = GPOS_WSZ_LIT("\\\\\\\" ");
	const WCHAR *wszEscape5 = GPOS_WSZ_LIT("\\\\\\\\\" ");
	const WCHAR *wszEscape6 = GPOS_WSZ_LIT("\\\\\\\\\\\" ");

	CWStringConst cstrEscape1(wszEscape1);
	CWStringConst cstrEscape2(wszEscape2);
	CWStringConst cstrEscape3(wszEscape3);
	CWStringConst cstrEscape4(wszEscape4);
	CWStringConst cstrEscape5(wszEscape5);
	CWStringConst cstrEscape6(wszEscape6);

	strd.Reset();
	strs1.Reset();
	strd.AppendEscape(&cstrEscape1, '"', GPOS_WSZ_LIT("\\\""));
	strs1.AppendEscape(&cstrEscape1, '"', GPOS_WSZ_LIT("\\\""));

	// escape character is skipped
	GPOS_ASSERT(strd.Equals(&cstrEscape1));
	GPOS_ASSERT(strs1.Equals(&cstrEscape1));

	strd.Reset();
	strs1.Reset();
	strd.AppendEscape(&cstrEscape2, '"', GPOS_WSZ_LIT("\\\""));
	strs1.AppendEscape(&cstrEscape2, '"', GPOS_WSZ_LIT("\\\""));

	// escape character is added
	GPOS_ASSERT(strd.Equals(&cstrEscape3));
	GPOS_ASSERT(strs1.Equals(&cstrEscape3));

	strd.Reset();
	strs1.Reset();
	strd.AppendEscape(&cstrEscape4, '"', GPOS_WSZ_LIT("\\\""));
	strs1.AppendEscape(&cstrEscape4, '"', GPOS_WSZ_LIT("\\\""));

	// escape character is skipped
	GPOS_ASSERT(strd.Equals(&cstrEscape4));
	GPOS_ASSERT(strs1.Equals(&cstrEscape4));

	strd.Reset();
	strs1.Reset();
	strd.AppendEscape(&cstrEscape5, '"', GPOS_WSZ_LIT("\\\""));
	strs1.AppendEscape(&cstrEscape5, '"', GPOS_WSZ_LIT("\\\""));

	// escape character is added
	GPOS_ASSERT(strd.Equals(&cstrEscape6));
	GPOS_ASSERT(strs1.Equals(&cstrEscape6));

	return GPOS_OK;
}

// EOF
