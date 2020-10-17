//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CEnumSetTest.cpp
//
//	@doc:
//      Test for CEnumSet/CEnumSetIter
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/types.h"

#include "gpos/common/CEnumSet.h"
#include "gpos/common/CEnumSetIter.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CEnumSetTest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CEnumSetTest::EresUnittest
//
//	@doc:
//		Unittest for enum sets
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEnumSetTest::EresUnittest()
{
	CUnittest rgut[] = {GPOS_UNITTEST_FUNC(CEnumSetTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CEnumSetTest::EresUnittest_Basics
//
//	@doc:
//		Testing ctors/dtor, accessors, iterator
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEnumSetTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	typedef CEnumSet<eTest, eTestSentinel> CETestSet;
	typedef CEnumSetIter<eTest, eTestSentinel> CETestIter;

	CETestSet *enum_set = GPOS_NEW(mp) CETestSet(mp);

	(void) enum_set->ExchangeSet(eTestOne);
	(void) enum_set->ExchangeSet(eTestTwo);

	GPOS_ASSERT(enum_set->ExchangeClear(eTestTwo));
	GPOS_ASSERT(!enum_set->ExchangeSet(eTestTwo));

	CETestIter type_info(*enum_set);
	while (type_info.Advance())
	{
		GPOS_ASSERT((BOOL) type_info);
		GPOS_ASSERT(eTestSentinel > type_info.TBit());
		GPOS_ASSERT(enum_set->Get(type_info.TBit()));
	}

	enum_set->Release();

	return GPOS_OK;
}

// EOF
