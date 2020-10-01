//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CListTest.cpp
//
//	@doc:
//		Tests for CList
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CList.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CListTest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CListTest::EresUnittest
//
//	@doc:
//		Unittest for lists
//
//---------------------------------------------------------------------------
GPOS_RESULT
CListTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CListTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CListTest::EresUnittest_Navigate),
		GPOS_UNITTEST_FUNC(CListTest::EresUnittest_Cursor),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CListTest::EresUnittest_Basics
//
//	@doc:
//		Various list operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CListTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CList<SElem> listFwd;
	listFwd.Init(GPOS_OFFSET(SElem, m_linkFwd));

	CList<SElem> listBwd;
	listBwd.Init(GPOS_OFFSET(SElem, m_linkBwd));

	ULONG cSize = 10;
	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, cSize);

	GPOS_ASSERT(0 == listFwd.Size());
	GPOS_ASSERT(0 == listBwd.Size());

	// insert all elements
	for (ULONG i = 0; i < cSize; i++)
	{
		GPOS_ASSERT(i == listFwd.Size());
		GPOS_ASSERT(i == listBwd.Size());

		listFwd.Prepend(&rgelem[i]);
		listBwd.Append(&rgelem[i]);
	}

	GPOS_ASSERT(cSize == listFwd.Size());
	GPOS_ASSERT(cSize == listBwd.Size());

	// remove first/last element until empty
	for (ULONG i = 0; i < cSize; i++)
	{
		GPOS_ASSERT(cSize - i == listFwd.Size());
		GPOS_ASSERT(&rgelem[i] == listFwd.Last());
		listFwd.Remove(listFwd.Last());

		// make sure it's still in the other list
		GPOS_ASSERT(GPOS_OK == listBwd.Find(&rgelem[i]));
	}
	GPOS_ASSERT(NULL == listFwd.First());
	GPOS_ASSERT(0 == listFwd.Size());

	// insert all elements in reverse order,
	// i.e. list is in same order as array
	for (ULONG i = cSize; i > 0; i--)
	{
		GPOS_ASSERT(cSize - i == listFwd.Size());
		listFwd.Prepend(&rgelem[i - 1]);
	}
	GPOS_ASSERT(cSize == listFwd.Size());

	for (ULONG i = 0; i < cSize; i++)
	{
		listFwd.Remove(&rgelem[(cSize / 2 + i) % cSize]);
	}
	GPOS_ASSERT(NULL == listFwd.First());
	GPOS_ASSERT(NULL == listFwd.Last());
	GPOS_ASSERT(0 == listFwd.Size());

	GPOS_DELETE_ARRAY(rgelem);
	return GPOS_OK;
}



//---------------------------------------------------------------------------
//	@function:
//		CListTest::EresUnittest_Navigate
//
//	@doc:
//		Various navigation operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CListTest::EresUnittest_Navigate()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CList<SElem> listFwd;
	listFwd.Init(GPOS_OFFSET(SElem, m_linkFwd));

	CList<SElem> listBwd;
	listBwd.Init(GPOS_OFFSET(SElem, m_linkBwd));

	ULONG cSize = 10;
	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, cSize);

	// insert all elements in reverse order,
	// i.e. list is in same order as array
	for (ULONG i = 0; i < cSize; i++)
	{
		listBwd.Prepend(&rgelem[i]);
		listFwd.Append(&rgelem[i]);
	}

	// use getnext to walk list
	SElem *pelem = listFwd.First();
	for (ULONG i = 0; i < cSize; i++)
	{
		GPOS_ASSERT(pelem == &rgelem[i]);
		pelem = listFwd.Next(pelem);
	}
	GPOS_ASSERT(NULL == pelem);

	// go to end of list -- then traverse backward
	pelem = listFwd.First();
	while (pelem && listFwd.Next(pelem))
	{
		pelem = listFwd.Next(pelem);
	}
	GPOS_ASSERT(listFwd.Last() == pelem);

	for (ULONG i = cSize; i > 0; i--)
	{
		GPOS_ASSERT(pelem == &rgelem[i - 1]);
		pelem = listFwd.Prev(pelem);
	}
	GPOS_ASSERT(NULL == pelem);

	GPOS_DELETE_ARRAY(rgelem);
	return GPOS_OK;
}



//---------------------------------------------------------------------------
//	@function:
//		CListTest::EresUnittest_Cursor
//
//	@doc:
//		Various cursor-based inserts
//
//---------------------------------------------------------------------------
GPOS_RESULT
CListTest::EresUnittest_Cursor()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CList<SElem> list;
	list.Init(GPOS_OFFSET(SElem, m_linkFwd));

	ULONG cSize = 5;
	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, cSize);

	list.Append(&rgelem[0]);

	list.Prepend(&rgelem[1], list.First());
	list.Append(&rgelem[2], list.Last());

	GPOS_ASSERT(&rgelem[1] == list.First());
	GPOS_ASSERT(&rgelem[2] == list.Last());

	list.Prepend(&rgelem[3], list.Last());
	list.Append(&rgelem[4], list.First());

	GPOS_ASSERT(&rgelem[1] == list.First());
	GPOS_ASSERT(&rgelem[2] == list.Last());

	GPOS_DELETE_ARRAY(rgelem);
	return GPOS_OK;
}

// EOF
