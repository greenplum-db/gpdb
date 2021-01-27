//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSyncListTest.cpp
//
//	@doc:
//		Tests for CSyncList
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CSyncListTest.h"

#include "gpos/base.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/task/CWorkerPoolManager.h"
#include "gpos/test/CUnittest.h"

#define GPOS_SLIST_SIZE 10

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CSyncListTest::EresUnittest
//
//	@doc:
//		Unittest for sync list
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncListTest::EresUnittest()
{
	CUnittest rgut[] = {GPOS_UNITTEST_FUNC(CSyncListTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CSyncListTest::EresUnittest_Basics
//
//	@doc:
//		Various sync list operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncListTest::EresUnittest_Basics()
{
	CSyncList<SElem> list;
	list.Init(GPOS_OFFSET(SElem, m_link));

	SElem rgelem[GPOS_SLIST_SIZE];

	// insert all elements
	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgelem); i++)
	{
		list.Push(&rgelem[i]);

		GPOS_ASSERT(GPOS_OK == list.Find(&rgelem[i]));
	}

#ifdef GPOS_DEBUG
	// scope for auto trace
	{
		CAutoMemoryPool amp;
		CMemoryPool *mp = amp.Pmp();

		CAutoTrace trace(mp);
		IOstream &os(trace.Os());

		os << GPOS_WSZ_LIT("Sync list contents:") << std::endl;
		list.OsPrint(os);
	}
#endif	// GPOS_DEBUG

	// pop elements until empty
	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgelem); i++)
	{
#ifdef GPOS_DEBUG
		SElem *pe =
#endif	// GPOS_DEBUG
			list.Pop();

		GPOS_ASSERT(pe == &rgelem[GPOS_ARRAY_SIZE(rgelem) - i - 1]);
	}
	GPOS_ASSERT(NULL == list.Pop());

	// insert all elements in reverse order
	for (ULONG i = GPOS_ARRAY_SIZE(rgelem); i > 0; i--)
	{
		list.Push(&rgelem[i - 1]);

		GPOS_ASSERT(GPOS_OK == list.Find(&rgelem[i - 1]));
	}

	// pop elements until empty
	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgelem); i++)
	{
#ifdef GPOS_DEBUG
		SElem *pe =
#endif	// GPOS_DEBUG
			list.Pop();

		GPOS_ASSERT(pe == &rgelem[i]);
	}
	GPOS_ASSERT(NULL == list.Pop());

	return GPOS_OK;
}


// EOF
