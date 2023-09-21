//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMessageRepository.cpp
//
//	@doc:
//		Singleton to keep error messages;
//---------------------------------------------------------------------------

#include "gpos/error/CMessageRepository.h"

#include "gpos/common/CSyncHashtableAccessByKey.h"
#include "gpos/utils.h"


using namespace gpos;
//---------------------------------------------------------------------------
// static singleton
//---------------------------------------------------------------------------
CMessageRepository *CMessageRepository::m_repository = nullptr;

//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::CMessageRepository
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CMessageRepository::CMessageRepository(CMemoryPool *mp) : m_mp(mp)
{
	GPOS_ASSERT(nullptr != mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::LookupMessage
//
//	@doc:
//		Lookup a message by a given CException/Local combination
//
//---------------------------------------------------------------------------
CMessage *
CMessageRepository::LookupMessage(CException exc, ELocale locale)
{
	GPOS_ASSERT(exc != CException::m_invalid_exception &&
				"Cannot lookup invalid exception message");

	if (exc != CException::m_invalid_exception)
	{
		CMessage *msg = nullptr;
		ELocale search_locale = locale;

		for (ULONG i = 0; i < 2; i++)
		{
			// try to locate locale-specific message table
			TMTAccessor tmta(m_hash_table, search_locale);
			CMessageTable *mt = tmta.Find();

			if (nullptr != mt)
			{
				// try to locate specific message
				msg = mt->LookupMessage(exc);
				if (nullptr != msg)
				{
					return msg;
				}
			}

			// retry with en-US locale
			search_locale = ElocEnUS_Utf8;
		}
	}

	return CMessage::GetMessage(CException::ExmiInvalid);
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::Init
//
//	@doc:
//		Initialize global instance of message repository
//
//---------------------------------------------------------------------------
void
CMessageRepository::Init()
{
	GPOS_ASSERT(nullptr == m_repository);

	CMemoryPool *mp = CMemoryPoolManager::CreateMemoryPool();

	m_repository = GPOS_NEW(mp) CMessageRepository(mp);
	m_repository->InitDirectory(mp);
	m_repository->LoadStandardMessages();
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::GetMessageRepository
//
//	@doc:
//		Retrieve singleton
//
//---------------------------------------------------------------------------
CMessageRepository *
CMessageRepository::GetMessageRepository()
{
	GPOS_ASSERT(nullptr != m_repository);
	return m_repository;
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::Shutdown
//
//	@doc:
//		Reclaim memory -- no specific clean up operation
//		Does not reclaim memory of messages; that remains the loader's
//		responsibility
//
//---------------------------------------------------------------------------
void
CMessageRepository::Shutdown()
{
	GPOS_ASSERT(nullptr != m_repository);
	CMemoryPoolManager::Destroy(m_repository->m_mp);
	m_repository = nullptr;
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::InitDirectory
//
//	@doc:
//		Install table-of-tables directory
//
//---------------------------------------------------------------------------
void
CMessageRepository::InitDirectory(CMemoryPool *mp)
{
	m_hash_table.Init(mp, 128, GPOS_OFFSET(CMessageTable, m_link),
					  GPOS_OFFSET(CMessageTable, m_locale),
					  &(CMessageTable::m_invalid_locale),
					  CMessageTable::HashValue, CMessageTable::Equals);
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::AddMessage
//
//	@doc:
//		Add individual message to a locale-specific message table; create
//		table on demand as needed
//
//---------------------------------------------------------------------------
void
CMessageRepository::AddMessage(ELocale locale, CMessage *msg)
{
	// retry logic: (1) attempt to insert first (frequent code path)
	// or (2) create message table after failure and retry (infreq code path)
	for (ULONG i = 0; i < 2; i++)
	{
		// scope for accessor lock
		{
			TMTAccessor tmta(m_hash_table, locale);
			CMessageTable *mt = tmta.Find();

			if (nullptr != mt)
			{
				mt->AddMessage(msg);
				return;
			}
		}

		// create message table for this locale on demand
		AddMessageTable(locale);
	}

	GPOS_ASSERT(!"Adding message table on demand failed");
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::AddMessageTable
//
//	@doc:
//		Add new locale table
//
//---------------------------------------------------------------------------
void
CMessageRepository::AddMessageTable(ELocale locale)
{
	CMessageTable *new_mt =
		GPOS_NEW(m_mp) CMessageTable(m_mp, GPOS_MSGTAB_SIZE, locale);

	{
		TMTAccessor tmta(m_hash_table, locale);
		CMessageTable *mt = tmta.Find();

		if (nullptr == mt)
		{
			tmta.Insert(new_mt);
			return;
		}
	}

	GPOS_DELETE(new_mt);
}


//---------------------------------------------------------------------------
//	@function:
//		CMessageRepository::LoadStandardMessages
//
//	@doc:
//		Insert standard messages for enUS locale;
//
//---------------------------------------------------------------------------
void
CMessageRepository::LoadStandardMessages()
{
	for (ULONG ul = 0; ul < CException::ExmiSentinel; ul++)
	{
		CMessage *msg = CMessage::GetMessage(ul);
		if (CException::m_invalid_exception != msg->m_exception)
		{
			AddMessage(ElocEnUS_Utf8, msg);
		}
	}
}

// EOF
