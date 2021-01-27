//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CHashMapTest.cpp
//
//	@doc:
//		Test for CHashMap
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CHashMapTest.h"

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CHashMapTest::EresUnittest
//
//	@doc:
//		Unittest for basic hash map
//
//---------------------------------------------------------------------------
GPOS_RESULT
CHashMapTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CHashMapTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CHashMapTest::EresUnittest_Ownership),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CHashMapTest::EresUnittest_Basic
//
//	@doc:
//		Basic insertion/lookup for hash table
//
//---------------------------------------------------------------------------
GPOS_RESULT
CHashMapTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// test with CHAR array
	ULONG_PTR rgul[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	CHAR rgsz[][5] = {"abc",  "def", "ghi", "qwe", "wer",
					  "wert", "dfg", "xcv", "zxc"};

	GPOS_ASSERT(GPOS_ARRAY_SIZE(rgul) == GPOS_ARRAY_SIZE(rgsz));
	const ULONG ulCnt = GPOS_ARRAY_SIZE(rgul);

	typedef CHashMap<ULONG_PTR, CHAR, HashPtr<ULONG_PTR>,
					 gpos::Equals<ULONG_PTR>, CleanupNULL<ULONG_PTR>,
					 CleanupNULL<CHAR> >
		UlongPtrToCharMap;

	UlongPtrToCharMap *phm = GPOS_NEW(mp) UlongPtrToCharMap(mp, 128);
	for (ULONG i = 0; i < ulCnt; ++i)
	{
#ifdef GPOS_DEBUG
		BOOL fSuccess =
#endif	// GPOS_DEBUG
			phm->Insert(&rgul[i], (CHAR *) rgsz[i]);
		GPOS_ASSERT(fSuccess);

		for (ULONG j = 0; j <= i; ++j)
		{
			GPOS_ASSERT(rgsz[j] == phm->Find(&rgul[j]));
		}
	}
	GPOS_ASSERT(ulCnt == phm->Size());

	// test replacing entry values of existing keys
	CHAR rgszNew[][10] = {"abc_",  "def_", "ghi_", "qwe_", "wer_",
						  "wert_", "dfg_", "xcv_", "zxc_"};
	for (ULONG i = 0; i < ulCnt; ++i)
	{
#ifdef GPOS_DEBUG
		BOOL fSuccess =
#endif	// GPOS_DEBUG
			phm->Replace(&rgul[i], rgszNew[i]);
		GPOS_ASSERT(fSuccess);

#ifdef GPOS_DEBUG
		fSuccess =
#endif	// GPOS_DEBUG
			phm->Replace(&rgul[i], rgsz[i]);
		GPOS_ASSERT(fSuccess);
	}
	GPOS_ASSERT(ulCnt == phm->Size());

	// test replacing entry value of a non-existing key
	ULONG_PTR ulp = 0;
#ifdef GPOS_DEBUG
	BOOL fSuccess =
#endif	// GPOS_DEBUG
		phm->Replace(&ulp, rgsz[0]);
	GPOS_ASSERT(!fSuccess);

	phm->Release();

	// test replacing values and triggering their release
	typedef CHashMap<ULONG, ULONG, HashValue<ULONG>, gpos::Equals<ULONG>,
					 CleanupDelete<ULONG>, CleanupDelete<ULONG> >
		UlongToUlongMap;
	UlongToUlongMap *phm2 = GPOS_NEW(mp) UlongToUlongMap(mp, 128);

	ULONG *pulKey = GPOS_NEW(mp) ULONG(1);
	ULONG *pulVal1 = GPOS_NEW(mp) ULONG(2);
	ULONG *pulVal2 = GPOS_NEW(mp) ULONG(3);

#ifdef GPOS_DEBUG
	fSuccess =
#endif	// GPOS_DEBUG
		phm2->Insert(pulKey, pulVal1);
	GPOS_ASSERT(fSuccess);

#ifdef GPOS_DEBUG
	ULONG *pulVal = phm2->Find(pulKey);
	GPOS_ASSERT(*pulVal == 2);

	fSuccess =
#endif	// GPOS_DEBUG
		phm2->Replace(pulKey, pulVal2);
	GPOS_ASSERT(fSuccess);

#ifdef GPOS_DEBUG
	pulVal = phm2->Find(pulKey);
	GPOS_ASSERT(*pulVal == 3);
#endif	// GPOS_DEBUG

	phm2->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CHashMapTest::EresUnittest_Ownership
//
//	@doc:
//		Basic hash map test with ownership
//
//---------------------------------------------------------------------------
GPOS_RESULT
CHashMapTest::EresUnittest_Ownership()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG ulCnt = 256;

	typedef CHashMap<ULONG_PTR, CHAR, HashPtr<ULONG_PTR>,
					 gpos::Equals<ULONG_PTR>, CleanupDelete<ULONG_PTR>,
					 CleanupDeleteArray<CHAR> >
		UlongPtrToCharMap;

	UlongPtrToCharMap *phm = GPOS_NEW(mp) UlongPtrToCharMap(mp, 32);
	for (ULONG i = 0; i < ulCnt; ++i)
	{
		ULONG_PTR *pulp = GPOS_NEW(mp) ULONG_PTR(i);
		CHAR *sz = GPOS_NEW_ARRAY(mp, CHAR, 3);

#ifdef GPOS_DEBUG
		BOOL fSuccess =
#endif	// GPOS_DEBUG
			phm->Insert(pulp, sz);

		GPOS_ASSERT(fSuccess);
		GPOS_ASSERT(sz == phm->Find(pulp));

		// can't insert existing keys
		GPOS_ASSERT(!phm->Insert(pulp, sz));
	}

	phm->Release();

	return GPOS_OK;
}

// EOF
