//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CBitSetTest.cpp
//
//	@doc:
//      Test for CBitSet
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpos/common/CBitSet.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CBitSetTest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CBitSet::EresUnittest
//
//	@doc:
//		Unittest for bit vectors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CBitSetTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CBitSetTest::EresUnittest_Removal),
		GPOS_UNITTEST_FUNC(CBitSetTest::EresUnittest_SetOps),
		GPOS_UNITTEST_FUNC(CBitSetTest::EresUnittest_Performance)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CBitSetTest::EresUnittest_Basics
//
//	@doc:
//		Testing ctors/dtor
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 32;
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, vector_size);

	ULONG cInserts = 10;
	for (ULONG i = 0; i < cInserts; i += 2)
	{
		// forces addition of new link
		pbs->ExchangeSet(i * vector_size);
	}
	GPOS_ASSERT(cInserts / 2 == pbs->Size());

	for (ULONG i = 1; i < cInserts; i += 2)
	{
		// new link between existing links
		pbs->ExchangeSet(i * vector_size);
	}
	GPOS_ASSERT(cInserts == pbs->Size());

	CBitSet *pbsCopy = GPOS_NEW(mp) CBitSet(mp, *pbs);
	GPOS_ASSERT(pbsCopy->Equals(pbs));

	// delete old bitset to make sure we're not accidentally
	// using any of its memory
	pbs->Release();

	for (ULONG i = 0; i < cInserts; i++)
	{
		GPOS_ASSERT(pbsCopy->Get(i * vector_size));
	}

	CWStringDynamic str(mp);
	COstreamString os(&str);

	os << *pbsCopy << std::endl;
	GPOS_TRACE(str.GetBuffer());

	pbsCopy->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CBitSetTest::EresUnittest_Removal
//
//	@doc:
//		Cleanup test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetTest::EresUnittest_Removal()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 32;
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, vector_size);
	CBitSet *pbsEmpty = GPOS_NEW(mp) CBitSet(mp, vector_size);

	GPOS_ASSERT(pbs->Equals(pbsEmpty));
	GPOS_ASSERT(pbsEmpty->Equals(pbs));

	ULONG cInserts = 10;
	for (ULONG i = 0; i < cInserts; i++)
	{
		pbs->ExchangeSet(i * vector_size);

		GPOS_ASSERT(i + 1 == pbs->Size());
	}

	for (ULONG i = 0; i < cInserts; i++)
	{
		// cleans up empty links
		pbs->ExchangeClear(i * vector_size);

		GPOS_ASSERT(cInserts - i - 1 == pbs->Size());
	}

	GPOS_ASSERT(pbs->Equals(pbsEmpty));
	GPOS_ASSERT(pbsEmpty->Equals(pbs));

	pbs->Release();
	pbsEmpty->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CBitSetTest::EresUnittest_SetOps
//
//	@doc:
//		Test for set operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetTest::EresUnittest_SetOps()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 32;
	ULONG cInserts = 10;

	CBitSet *pbs1 = GPOS_NEW(mp) CBitSet(mp, vector_size);
	for (ULONG i = 0; i < cInserts; i += 2)
	{
		pbs1->ExchangeSet(i * vector_size);
	}

	CBitSet *pbs2 = GPOS_NEW(mp) CBitSet(mp, vector_size);
	for (ULONG i = 1; i < cInserts; i += 2)
	{
		pbs2->ExchangeSet(i * vector_size);
	}
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, vector_size);

	pbs->Union(pbs1);
	GPOS_ASSERT(pbs->Equals(pbs1));

	pbs->Intersection(pbs1);
	GPOS_ASSERT(pbs->Equals(pbs1));
	GPOS_ASSERT(pbs->Equals(pbs));
	GPOS_ASSERT(pbs1->Equals(pbs1));

	pbs->Union(pbs2);
	GPOS_ASSERT(!pbs->Equals(pbs1) && !pbs->Equals(pbs2));
	GPOS_ASSERT(pbs->ContainsAll(pbs1) && pbs->ContainsAll(pbs2));

	pbs->Difference(pbs2);
	GPOS_ASSERT(pbs->Equals(pbs1));

	pbs1->Release();

	pbs->Union(pbs2);
	pbs->Intersection(pbs2);
	GPOS_ASSERT(pbs->Equals(pbs2));
	GPOS_ASSERT(pbs->ContainsAll(pbs2));

	GPOS_ASSERT(pbs->Size() == pbs2->Size());

	pbs2->Release();

	pbs->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CBitSetTest::EresUnittest_Performance
//
//	@doc:
//		Simple perf test -- simulates xform candidate sets
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetTest::EresUnittest_Performance()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 512;
	CBitSet *pbsBase = GPOS_NEW(mp) CBitSet(mp, vector_size);
	for (ULONG i = 0; i < vector_size; i++)
	{
		(void) pbsBase->ExchangeSet(i);
	}

	CBitSet *pbsTest = GPOS_NEW(mp) CBitSet(mp, vector_size);
	for (ULONG j = 0; j < 100000; j++)
	{
		ULONG cRandomBits = 16;
		for (ULONG i = 0; i < cRandomBits;
			 i += ((vector_size - 1) / cRandomBits))
		{
			(void) pbsTest->ExchangeSet(i);
		}

		pbsTest->Intersection(pbsBase);
	}

	pbsTest->Release();
	pbsBase->Release();

	return GPOS_OK;
}

// EOF
