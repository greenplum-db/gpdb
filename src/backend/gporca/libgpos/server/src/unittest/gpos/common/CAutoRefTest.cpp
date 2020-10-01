//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CAutoRefTest.cpp
//
//	@doc:
//		Tests for CAutoRef
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpos/common/CAutoRef.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CAutoRefTest.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CAutoRef::EresUnittest
//
//	@doc:
//		Unittest for reference counted auto pointers
//
//---------------------------------------------------------------------------
GPOS_RESULT
CAutoRefTest::EresUnittest()
{
	CUnittest rgut[] = {GPOS_UNITTEST_FUNC(CAutoRefTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CAutoRefTest::EresUnittest_Basics
//
//	@doc:
//		Various basic operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CAutoRefTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// assignment
	CAutoRef<CElem> aelem;
	CElem *pelem = GPOS_NEW(mp) CElem(0);
	aelem = pelem;

	GPOS_ASSERT(aelem->m_ul == pelem->m_ul);
	GPOS_ASSERT(&aelem->m_ul == &pelem->m_ul);

#ifdef GPOS_DEBUG
	CElem *pelem2 = &(*pelem);
	GPOS_ASSERT(pelem2 == pelem);
#endif	// GPOS_DEBUG

	// hand reference over to other auto ref count
	CAutoRef<CElem> aelem2;
	aelem2 = aelem.Reset();

	// c'tor
	CAutoRef<CElem> aelem3(GPOS_NEW(mp) CElem(10));
	GPOS_ASSERT(aelem3->m_ul == ULONG(10));

	return GPOS_OK;
}

// EOF
