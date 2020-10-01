//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperTest.cpp
//
//	@doc:
//		Tests for minidump handler
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/string/CWStringStatic.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/task/CAutoSuspendAbort.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/error/CMiniDumperTest.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::EresUnittest
//
//	@doc:
//		Function for testing minidump handler
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CMiniDumperTest::EresUnittest_Basic)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::EresUnittest_Basic
//
//	@doc:
//		Basic test for minidump handler
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CMiniDumperStream mdrs(mp);

	CWStringDynamic wstrMinidump(mp);
	COstreamString oss(&wstrMinidump);
	mdrs.Init(&oss);

	GPOS_TRY
	{
		(void) PvRaise(NULL);
	}
	GPOS_CATCH_EX(ex)
	{
		mdrs.Finalize();

		GPOS_RESET_EX;

		GPOS_TRACE(wstrMinidump.GetBuffer());
	}
	GPOS_CATCH_END;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::PvRaise
//
//	@doc:
//		Function raising an exception
//
//---------------------------------------------------------------------------
void *
CMiniDumperTest::PvRaise(void *	 // pv
)
{
	// register stack serializer with error context
	CSerializableStack ss;

	clib::USleep(1000);

	// raise exception to trigger minidump
	GPOS_OOM_CHECK(NULL);

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CMiniDumperStream::CMiniDumperStream
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMiniDumperTest::CMiniDumperStream::CMiniDumperStream(CMemoryPool *mp)
	: CMiniDumper(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CMiniDumperStream::~CMiniDumperStream
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMiniDumperTest::CMiniDumperStream::~CMiniDumperStream()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CMiniDumperStream::UlSerializeHeader
//
//	@doc:
//		Serialize minidump header
//
//---------------------------------------------------------------------------
void
CMiniDumperTest::CMiniDumperStream::SerializeHeader()
{
	*m_oos << "\n<MINIDUMP_TEST>\n";
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CMiniDumperStream::UlSerializeFooter
//
//	@doc:
//		Serialize minidump footer
//
//---------------------------------------------------------------------------
void
CMiniDumperTest::CMiniDumperStream::SerializeFooter()
{
	*m_oos << "</MINIDUMP_TEST>\n";
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CMiniDumperStream::SerializeEntryHeader
//
//	@doc:
//		Serialize entry header
//
//---------------------------------------------------------------------------
void
CMiniDumperTest::CMiniDumperStream::SerializeEntryHeader()
{
	WCHAR wszBuffer[GPOS_MINIDUMP_BUF_SIZE];
	CWStringStatic wstr(wszBuffer, GPOS_ARRAY_SIZE(wszBuffer));
	wstr.AppendFormat(GPOS_WSZ_LIT("<THREAD ID=%d>\n"), 0);

	*m_oos << wstr.GetBuffer();
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CMiniDumperStream::SerializeEntryFooter
//
//	@doc:
//		Serialize entry footer
//
//---------------------------------------------------------------------------
void
CMiniDumperTest::CMiniDumperStream::SerializeEntryFooter()
{
	*m_oos << "</THREAD>\n";
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CSerializableStack::CSerializableStack
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMiniDumperTest::CSerializableStack::CSerializableStack() : CSerializable()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CSerializableStack::~CSerializableStack
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------

CMiniDumperTest::CSerializableStack::~CSerializableStack()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperTest::CSerializableStack::Serialize
//
//	@doc:
//		Serialize object to passed stream
//
//---------------------------------------------------------------------------
void
CMiniDumperTest::CSerializableStack::Serialize(COstream &oos)
{
	WCHAR wszStackBuffer[GPOS_MINIDUMP_BUF_SIZE];
	CWStringStatic wstr(wszStackBuffer, GPOS_ARRAY_SIZE(wszStackBuffer));

	wstr.AppendFormat(GPOS_WSZ_LIT("<STACK_TRACE>\n"));

	CErrorContext *perrctxt = CTask::Self()->ConvertErrCtxt();
	perrctxt->GetStackDescriptor()->AppendTrace(&wstr);

	wstr.AppendFormat(GPOS_WSZ_LIT("</STACK_TRACE>\n"));

	oos << wstr.GetBuffer();
}


// EOF
