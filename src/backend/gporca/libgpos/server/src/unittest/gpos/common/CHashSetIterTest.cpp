//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc

#include "unittest/gpos/common/CHashSetIterTest.h"

#include "gpos/base.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/common/CHashSetIter.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

// Unittest for basic hash set iterator
GPOS_RESULT
CHashSetIterTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CHashSetIterTest::EresUnittest_Basic),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

// Basic iterator test
GPOS_RESULT
CHashSetIterTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// test data
	ULONG rgul[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	const ULONG ulCnt = GPOS_ARRAY_SIZE(rgul);

	typedef CHashSet<ULONG, HashValue<ULONG>, gpos::Equals<ULONG>,
					 CleanupNULL<ULONG> >
		UlongHashSet;

	typedef CHashSetIter<ULONG, HashValue<ULONG>, gpos::Equals<ULONG>,
						 CleanupNULL<ULONG> >
		UlongHashSetIter;

	// using N - 2 slots guarantees collisions
	UlongHashSet *ps = GPOS_NEW(mp) UlongHashSet(mp, ulCnt - 2);

#ifdef GPOS_DEBUG

	// iteration over empty map
	UlongHashSetIter siEmpty(ps);
	GPOS_ASSERT(!siEmpty.Advance());

#endif	// GPOS_DEBUG

	typedef CDynamicPtrArray<const ULONG, CleanupNULL> ULongPtrArray;
	CAutoRef<ULongPtrArray> pdrgpulValues(GPOS_NEW(mp) ULongPtrArray(mp));
	// load map and iterate over it after each step
	for (ULONG ul = 0; ul < ulCnt; ++ul)
	{
		(void) ps->Insert(&rgul[ul]);
		pdrgpulValues->Append(&rgul[ul]);

		CAutoRef<ULongPtrArray> pdrgpulIterValues(GPOS_NEW(mp)
													  ULongPtrArray(mp));

		// iterate over full set
		UlongHashSetIter si(ps);
		while (si.Advance())
		{
			pdrgpulIterValues->Append(si.Get());
		}

		pdrgpulIterValues->Sort();

		GPOS_ASSERT(pdrgpulValues->Equals(pdrgpulIterValues.Value()));
	}

	ps->Release();

	return GPOS_OK;
}


// EOF
