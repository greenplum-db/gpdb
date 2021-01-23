//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMessageRepositoryTest.cpp
//
//	@doc:
//		Tests for CMessageTable
//---------------------------------------------------------------------------

#include "unittest/gpos/error/CMessageRepositoryTest.h"

#include "gpos/assert.h"
#include "gpos/base.h"
#include "gpos/error/CMessageRepository.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"


using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CMessageRepositoryTest::EresUnittest
//
//	@doc:
//		unit test driver
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMessageRepositoryTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CMessageRepositoryTest::EresUnittest_Basic),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepositoryTest::EresUnittest_Basic
//
//	@doc:
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMessageRepositoryTest::EresUnittest_Basic()
{
#ifdef GPOS_DEBUG
	// lookup OOM message
	CMessage *pmsg = CMessageRepository::GetMessageRepository()->LookupMessage(
		CException(CException::ExmaSystem, CException::ExmiOOM), ElocEnUS_Utf8);

	GPOS_ASSERT(GPOS_MATCH_EX(pmsg->m_exception, CException::ExmaSystem,
							  CException::ExmiOOM));

	GPOS_ASSERT(pmsg == CMessage::GetMessage(CException::ExmiOOM));

	// attempt looking up OOM message in German -- should return enUS OOM message;
	pmsg = CMessageRepository::GetMessageRepository()->LookupMessage(
		CException(CException::ExmaSystem, CException::ExmiOOM), ElocGeDE_Utf8);

	GPOS_ASSERT(GPOS_MATCH_EX(pmsg->m_exception, CException::ExmaSystem,
							  CException::ExmiOOM));

	GPOS_ASSERT(pmsg == CMessage::GetMessage(CException::ExmiOOM));

	GPOS_TRY
	{
		// attempt looking up message with invalid exception code
		pmsg = CMessageRepository::GetMessageRepository()->LookupMessage(
			CException(CException::ExmaSystem, 1234567), ElocEnUS_Utf8);
	}
	GPOS_CATCH_EX(exc)
	{
		GPOS_ASSERT(
			GPOS_MATCH_EX(exc, CException::ExmaSystem, CException::ExmiAssert));
		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

#endif	// GPOS_DEBUG

	return GPOS_OK;
}

// EOF
