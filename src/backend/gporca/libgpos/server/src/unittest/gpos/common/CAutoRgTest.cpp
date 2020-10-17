//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CAutoRgTest.cpp
//
//	@doc:
//		Tests for CAutoRg
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpos/common/CAutoRg.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CAutoRgTest.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CAutoRg::EresUnittest
//
//	@doc:
//		Unittest for bit vectors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CAutoRgTest::EresUnittest()
{
	CUnittest rgut[] = {GPOS_UNITTEST_FUNC(CAutoRgTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CAutoRgTest::EresUnittest_Basics
//
//	@doc:
//		Various basic operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CAutoRgTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoRg<CHAR> asz;
	CHAR *sz = GPOS_NEW_ARRAY(mp, CHAR, 1234);
	asz = sz;

	CAutoRg<CHAR> asz2;
	CAutoRg<CHAR> asz3;
	CHAR *sz2 = GPOS_NEW_ARRAY(mp, CHAR, 1234);

	asz2 = sz2;
	asz3 = asz2;

#ifdef GPOS_DEBUG
	CHAR ch = asz3[0];
	GPOS_ASSERT(ch == sz2[0]);
#endif	// GPOS_DEBUG

	asz2 = NULL;
	GPOS_DELETE_ARRAY(asz3.RgtReset());

	// ctor
	CAutoRg<CHAR> asz4(GPOS_NEW_ARRAY(mp, CHAR, 1234));

	return GPOS_OK;
}

// EOF
