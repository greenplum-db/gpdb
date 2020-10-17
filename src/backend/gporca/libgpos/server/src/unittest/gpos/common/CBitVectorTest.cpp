//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CBitVectorTest.cpp
//
//	@doc:
//		Tests for CBitVector
//---------------------------------------------------------------------------


#include "gpos/base.h"

#include "gpos/common/clibwrapper.h"
#include "gpos/common/CBitVector.h"
#include "gpos/common/CAutoRg.h"
#include "gpos/common/CRandom.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CBitVectorTest.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CBitVector::EresUnittest
//
//	@doc:
//		Unittest for bit vectors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitVectorTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CBitVectorTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CBitVectorTest::EresUnittest_SetOps),
		GPOS_UNITTEST_FUNC(CBitVectorTest::EresUnittest_Cursor),
		GPOS_UNITTEST_FUNC(CBitVectorTest::EresUnittest_Random)
#ifdef GPOS_DEBUG
			,
		GPOS_UNITTEST_FUNC_ASSERT(CBitVectorTest::EresUnittest_OutOfBounds)
#endif	// GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CBitVectorTest::EresUnittest_Basics
//
//	@doc:
//		Various basic operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitVectorTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG cSize = 129;

	CBitVector bv(mp, cSize);
	GPOS_ASSERT(bv.IsEmpty());

	for (ULONG i = 0; i < cSize; i++)
	{
		BOOL fSet = bv.ExchangeSet(i);
		if (fSet)
		{
			return GPOS_FAILED;
		}
		GPOS_ASSERT(bv.Get(i));

		CBitVector bvCopy(mp, bv);
		for (ULONG j = 0; j <= i; j++)
		{
			BOOL fSetAlt = bvCopy.Get(j);
			GPOS_ASSERT(fSetAlt);

			if (true != fSetAlt)
			{
				return GPOS_FAILED;
			}

			// clear and check
			bvCopy.ExchangeClear(j);
			fSetAlt = bvCopy.Get(j);
			GPOS_ASSERT(!fSetAlt);
		}

		GPOS_ASSERT(bvCopy.CountSetBits() == 0);
	}

	GPOS_ASSERT(bv.CountSetBits() == cSize);

	return GPOS_OK;
}



//---------------------------------------------------------------------------
//	@function:
//		CBitVectorTest::EresUnittest_SetOps
//
//	@doc:
//		Set operation tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitVectorTest::EresUnittest_SetOps()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG cSize = 129;
	CBitVector bvEmpty(mp, cSize);

	CBitVector bvEven(mp, cSize);
	for (ULONG i = 0; i < cSize; i += 2)
	{
		bvEven.ExchangeSet(i);
	}
	GPOS_ASSERT(bvEven.ContainsAll(&bvEmpty));

	CBitVector bvOdd(mp, cSize);
	for (ULONG i = 1; i < cSize; i += 2)
	{
		bvOdd.ExchangeSet(i);
	}
	GPOS_ASSERT(bvOdd.ContainsAll(&bvEmpty));
	GPOS_ASSERT(bvOdd.IsDisjoint(&bvEven));

	GPOS_ASSERT(!bvEven.ContainsAll(&bvOdd));
	GPOS_ASSERT(!bvOdd.ContainsAll(&bvEven));

	CBitVector bv(mp, bvOdd);

	bv.Or(&bvEven);
	bv.And(&bvOdd);
	GPOS_ASSERT(bv.Equals(&bvOdd));

	bv.Or(&bvEven);
	bv.And(&bvEven);
	GPOS_ASSERT(bv.Equals(&bvEven));

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CBitVectorTest::EresUnittest_Cursor
//
//	@doc:
//		Unittest for cursoring
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitVectorTest::EresUnittest_Cursor()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CBitVector bv(mp, 129);
	for (ULONG i = 1; i < 20; i++)
	{
		bv.ExchangeSet(i * 3);
	}

	ULONG ulCursor = 0;
	bv.GetNextSetBit(0, ulCursor);
	while (bv.GetNextSetBit(ulCursor + 1, ulCursor))
	{
		GPOS_ASSERT(ulCursor == ((ulCursor / 3) * 3));
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CBitVectorTest::EresUnittest_Random
//
//	@doc:
//		Test with random bit vectors to avoid patterns
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitVectorTest::EresUnittest_Random()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// set up control vector
	ULONG cTotal = 10000;
	CHAR *rg = GPOS_NEW_ARRAY(mp, CHAR, cTotal);

	CRandom rand;

	clib::Memset(rg, 0, cTotal);

	// set random chars in the control vector
	for (ULONG i = 0; i < cTotal * 0.2; i++)
	{
		ULONG index = rand.Next() % (cTotal - 1);
		GPOS_ASSERT(index < cTotal);
		rg[index] = 1;
	}

	ULONG cElements = 0;
	CBitVector bv(mp, cTotal);
	for (ULONG i = 0; i < cTotal; i++)
	{
		if (1 == rg[i])
		{
			bv.ExchangeSet(i);
			cElements++;
		}
	}

	GPOS_ASSERT(cElements == bv.CountSetBits());

	ULONG ulCursor = 0;
	while (bv.GetNextSetBit(ulCursor + 1, ulCursor))
	{
		GPOS_ASSERT(1 == rg[ulCursor]);
		cElements--;
	}

	GPOS_ASSERT(0 == cElements);
	GPOS_DELETE_ARRAY(rg);

	return GPOS_OK;
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CBitVectorTest::EresUnittest_OutOfBounds
//
//	@doc:
//		Unittest for OOB access
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitVectorTest::EresUnittest_OutOfBounds()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CBitVector bv(mp, 129);

	// this must assert
	bv.ExchangeSet(130);

	return GPOS_FAILED;
}

#endif	// GPOS_DEBUG

// EOF
