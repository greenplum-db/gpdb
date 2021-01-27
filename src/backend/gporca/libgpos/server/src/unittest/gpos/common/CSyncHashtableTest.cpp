//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CSyncHashtableTest.cpp
//
//	@doc:
//      Tests for CSyncHashtableTest; spliced out into a separate
//		class CSyncHashtableTest to avoid template parameter confusion for the compiler
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CSyncHashtableTest.h"

#include "gpos/base.h"
#include "gpos/common/CBitVector.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

#define GPOS_SHT_SMALL_BUCKETS 5
#define GPOS_SHT_BIG_BUCKETS 100
#define GPOS_SHT_ELEMENTS 10
#define GPOS_SHT_LOOKUPS 500
#define GPOS_SHT_INITIAL_ELEMENTS (1 + GPOS_SHT_ELEMENTS / 2)
#define GPOS_SHT_ELEMENT_DUPLICATES 5
#define GPOS_SHT_THREADS 15


// invalid key
const ULONG CSyncHashtableTest::SElem::m_ulInvalid = gpos::ulong_max;

// invalid element
const CSyncHashtableTest::SElem CSyncHashtableTest::SElem::m_elemInvalid(
	gpos::ulong_max, gpos::ulong_max);

//---------------------------------------------------------------------------
//	@function:
//		CSyncHashtableTest::EresUnittest
//
//	@doc:
//		Unittest for sync'd hashtable
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncHashtableTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CSyncHashtableTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CSyncHashtableTest::EresUnittest_Accessor),
		GPOS_UNITTEST_FUNC(CSyncHashtableTest::EresUnittest_ComplexEquality),
		GPOS_UNITTEST_FUNC(CSyncHashtableTest::EresUnittest_SameKeyIteration),
		GPOS_UNITTEST_FUNC(
			CSyncHashtableTest::EresUnittest_NonConcurrentIteration)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CSyncHashtableTest::EresUnittest_Basics
//
//	@doc:
//		Various list operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncHashtableTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, GPOS_SHT_ELEMENTS);
	CSyncHashtable<SElem, ULONG> sht;
	sht.Init(mp, GPOS_SHT_SMALL_BUCKETS, GPOS_OFFSET(SElem, m_link),
			 GPOS_OFFSET(SElem, m_ulKey), &(SElem::m_ulInvalid),
			 SElem::HashValue, SElem::FEqualKeys);

	for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
	{
		rgelem[i] = SElem(i, i);
		sht.Insert(&rgelem[i]);
	}

	GPOS_DELETE_ARRAY(rgelem);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSyncHashtableTest::EresUnittest_Accessor
//
//	@doc:
//		Various hashtable operations via accessor class
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncHashtableTest::EresUnittest_Accessor()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, GPOS_SHT_ELEMENTS);

	CSyncHashtable<SElem, ULONG> rgsht[2];

	rgsht[0].Init(mp, GPOS_SHT_SMALL_BUCKETS, GPOS_OFFSET(SElem, m_link),
				  GPOS_OFFSET(SElem, m_ulKey), &(SElem::m_ulInvalid),
				  SElem::HashValue, SElem::FEqualKeys);

	rgsht[1].Init(mp, GPOS_SHT_BIG_BUCKETS, GPOS_OFFSET(SElem, m_link),
				  GPOS_OFFSET(SElem, m_ulKey), &(SElem::m_ulInvalid),
				  SElem::HashValue, SElem::FEqualKeys);

	for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
	{
		rgelem[i] = SElem(i, i);

		// distribute elements over both hashtables
		rgsht[i % 2].Insert(&rgelem[i]);
	}

	for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
	{
		SElem *pelem = &rgelem[i];
		ULONG ulKey = pelem->m_ulKey;

		CSyncHashtableAccessByKey<SElem, ULONG> shtacc0(rgsht[0], ulKey);

		CSyncHashtableAccessByKey<SElem, ULONG> shtacc1(rgsht[1], ulKey);

		if (NULL == shtacc0.Find())
		{
			// must be in the other hashtable
			GPOS_ASSERT(pelem == shtacc1.Find());

			// move to other hashtable
			shtacc1.Remove(pelem);
			shtacc0.Insert(pelem);
		}
	}

	// check that all elements have been moved over to the first hashtable
	for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
	{
		SElem *pelem = &rgelem[i];
		ULONG ulKey = pelem->m_ulKey;

		CSyncHashtableAccessByKey<SElem, ULONG> shtacc0(rgsht[0], ulKey);

		CSyncHashtableAccessByKey<SElem, ULONG> shtacc1(rgsht[1], ulKey);

		GPOS_ASSERT(NULL == shtacc1.Find());
		GPOS_ASSERT(pelem == shtacc0.Find());
	}

	GPOS_DELETE_ARRAY(rgelem);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSyncHashtableTest::EresUnittest_ComplexEquality
//
//	@doc:
//		Test where key is the entire object rather than a member;
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncHashtableTest::EresUnittest_ComplexEquality()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, GPOS_SHT_ELEMENTS);

	CSyncHashtable<SElem, SElem> sht;
	sht.Init(mp, GPOS_SHT_SMALL_BUCKETS, GPOS_OFFSET(SElem, m_link),
			 0 /*cKeyOffset*/, &(SElem::m_elemInvalid), SElem::HashValue,
			 SElem::Equals);

	for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
	{
		rgelem[i] = SElem(GPOS_SHT_ELEMENTS + i, i);

		sht.Insert(&rgelem[i]);
	}

	for (ULONG j = 0; j < GPOS_SHT_ELEMENTS; j++)
	{
		SElem elem(GPOS_SHT_ELEMENTS + j, j);
		CSyncHashtableAccessByKey<SElem, SElem> shtacc(sht, elem);

#ifdef GPOS_DEBUG
		SElem *pelem = shtacc.Find();
		GPOS_ASSERT(NULL != pelem && pelem != &elem);
		GPOS_ASSERT(pelem->Id() == GPOS_SHT_ELEMENTS + j);
#endif	// GPOS_DEBUG
	}

	GPOS_DELETE_ARRAY(rgelem);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSyncHashtableTest::EresUnittest_SameKeyIteration
//
//	@doc:
//		Test iteration over elements with the same key
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncHashtableTest::EresUnittest_SameKeyIteration()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	const ULONG size = GPOS_SHT_ELEMENTS * GPOS_SHT_ELEMENT_DUPLICATES;
	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, size);

	SElemHashtable sht;

	sht.Init(mp, GPOS_SHT_SMALL_BUCKETS, GPOS_OFFSET(SElem, m_link),
			 GPOS_OFFSET(SElem, m_ulKey), &(SElem::m_ulInvalid),
			 SElem::HashValue, SElem::FEqualKeys);

	// insert a mix of elements with duplicate keys
	for (ULONG j = 0; j < GPOS_SHT_ELEMENT_DUPLICATES; j++)
	{
		for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
		{
			ULONG ulIndex = GPOS_SHT_ELEMENTS * j + i;
			rgelem[ulIndex] = SElem(ulIndex, i);
			sht.Insert(&rgelem[ulIndex]);
		}
	}

	// iterate over elements with the same key
	for (ULONG ulKey = 0; ulKey < GPOS_SHT_ELEMENTS; ulKey++)
	{
		SElemHashtableAccessor shtacc(sht, ulKey);

		ULONG count = 0;
		SElem *pelem = shtacc.Find();
		while (NULL != pelem)
		{
			count++;
			pelem = shtacc.Next(pelem);
		}
		GPOS_ASSERT(count == GPOS_SHT_ELEMENT_DUPLICATES);
	}

	GPOS_DELETE_ARRAY(rgelem);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSyncHashtableTest::EresUnittest_NonConcurrentIteration
//
//	@doc:
//		Test iteration by a single client over all hash table elements
//
//---------------------------------------------------------------------------
GPOS_RESULT
CSyncHashtableTest::EresUnittest_NonConcurrentIteration()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	SElem *rgelem = GPOS_NEW_ARRAY(mp, SElem, GPOS_SHT_ELEMENTS);

	SElemHashtable sht;

	sht.Init(mp, GPOS_SHT_SMALL_BUCKETS, GPOS_OFFSET(SElem, m_link),
			 GPOS_OFFSET(SElem, m_ulKey), &(SElem::m_ulInvalid),
			 SElem::HashValue, SElem::FEqualKeys);


	// iterate over empty hash table
	SElemHashtableIter shtitEmpty(sht);
	GPOS_ASSERT(!shtitEmpty.Advance() &&
				"Iterator advanced in an empty hash table");


	// insert elements
	for (ULONG i = 0; i < GPOS_SHT_ELEMENTS; i++)
	{
		rgelem[i] = SElem(i, i);
		sht.Insert(&rgelem[i]);
	}

	// iteration with no concurrency - each access must
	// produce a unique, valid and not NULL element
	SElemHashtableIter shtit(sht);
	ULONG count = 0;

#ifdef GPOS_DEBUG
	// maintain a flag for visiting each element
	CBitVector bv(mp, GPOS_SHT_ELEMENTS);
#endif	// GPOS_DEBUG

	while (shtit.Advance())
	{
		SElemHashtableIterAccessor htitacc(shtit);

#ifdef GPOS_DEBUG
		SElem *pelem =
#endif	// GPOS_DEBUG
			htitacc.Value();

		GPOS_ASSERT(NULL != pelem);

		GPOS_ASSERT(SElem::IsValid(pelem->m_ulKey));

		// check if element has been visited before
		GPOS_ASSERT(!bv.ExchangeSet(pelem->Id()) &&
					"Iterator returned duplicates");

		count++;
	}

	GPOS_ASSERT(count == GPOS_SHT_ELEMENTS);

	GPOS_DELETE_ARRAY(rgelem);

	return GPOS_OK;
}

// EOF
