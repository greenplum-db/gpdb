//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CBitSetIterTest.cpp
//
//	@doc:
//		Test of bitset iterator
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CBitSetIterTest.h"

#include "gpos/base.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/common/CBitSetIter.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CBitSetIter::EresUnittest
//
//	@doc:
//		Unittest for bit vectors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetIterTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CBitSetIterTest::EresUnittest_Basics),

#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC_ASSERT(CBitSetIterTest::EresUnittest_Uninitialized),
		GPOS_UNITTEST_FUNC_ASSERT(CBitSetIterTest::EresUnittest_Overrun)
#endif	// GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CBitSetIterTest::EresUnittest_Basics
//
//	@doc:
//		Testing ctors/dtor
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetIterTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 32;
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, vector_size);

	ULONG cInserts = 10;
	for (ULONG i = 0; i < cInserts; i++)
	{
		// forces addition of new link
		pbs->ExchangeSet(i * vector_size);
	}

	ULONG cCount = 0;
	CBitSetIter bsi(*pbs);
	while (bsi.Advance())
	{
		GPOS_ASSERT(bsi.Bit() == (bsi.Bit() / vector_size) * vector_size);
		GPOS_ASSERT((BOOL) bsi);

		cCount++;
	}
	GPOS_ASSERT(cInserts == cCount);

	GPOS_ASSERT(!((BOOL) bsi));

	pbs->Release();

	return GPOS_OK;
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CBitSetIterTest::EresUnittest_Uninitialized
//
//	@doc:
//		Test for uninitialized access
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetIterTest::EresUnittest_Uninitialized()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 32;

	CAutoRef<CBitSet> a_pbs;
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, vector_size);
	a_pbs = pbs;

	CBitSetIter bsi(*pbs);

	// this throws
	bsi.Bit();

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CBitSetIterTest::EresUnittest_Overrun
//
//	@doc:
//		Test for calling Advance on exhausted iter
//
//---------------------------------------------------------------------------
GPOS_RESULT
CBitSetIterTest::EresUnittest_Overrun()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG vector_size = 32;

	CAutoRef<CBitSet> a_pbs;
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp, vector_size);
	a_pbs = pbs;

	CBitSetIter bsi(*pbs);

	while (bsi.Advance())
	{
	}

	// this throws
	bsi.Advance();

	return GPOS_FAILED;
}

#endif	// GPOS_DEBUG

// EOF
