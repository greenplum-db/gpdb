//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMessageTableTest.cpp
//
//	@doc:
//		Tests for CMessageTable
//---------------------------------------------------------------------------

#include "unittest/gpos/error/CMessageTableTest.h"

#include "gpos/assert.h"
#include "gpos/error/CMessageTable.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"


using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CMessageTableTest::EresUnittest
//
//	@doc:
//		Function for raising assert exceptions; again, encapsulated in a function
//		to facilitate debugging
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMessageTableTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CMessageTableTest::EresUnittest_Basic),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageTableTest::EresUnittest_Basic
//
//	@doc:
//		Create message table and insert all standard messages;
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMessageTableTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CMessageTable *pmt =
		GPOS_NEW(mp) CMessageTable(mp, GPOS_MSGTAB_SIZE, ElocEnUS_Utf8);

	// insert all system messages
	for (ULONG ul = 0; ul < CException::ExmiSentinel; ul++)
	{
		CMessage *pmsg = CMessage::GetMessage(ul);
		if (CException::m_invalid_exception != pmsg->m_exception)
		{
			pmt->AddMessage(pmsg);

#ifdef GPOS_DEBUG
			CMessage *pmsgLookedup = pmt->LookupMessage(pmsg->m_exception);
			GPOS_ASSERT(pmsg == pmsgLookedup && "Lookup failed");
#endif	// GPOS_DEBUG
		}
	}

	GPOS_DELETE(pmt);

	return GPOS_OK;
}

// EOF
