//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008, 2009 Greenplum, Inc.
//
//	@filename:
//		CColumnFactoryTest.cpp
//
//	@doc:
//		Test for CColumnFactory
//---------------------------------------------------------------------------

#include "unittest/gpopt/base/CColumnFactoryTest.h"

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/base/CColumnFactory.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/IMDTypeInt4.h"

#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CColumnFactoryTest::EresUnittest
//
//	@doc:
//		Unittest for column factory
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColumnFactoryTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CColumnFactoryTest::EresUnittest_Basic)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CColumnFactoryTest::EresUnittest_Basic
//
//	@doc:
//		Basic array allocation test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColumnFactoryTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>();

	CColumnFactory cf;

	// typed colref
	CColRef *pcrOne = cf.PcrCreate(pmdtypeint4, default_type_modifier);
	GPOS_ASSERT(pcrOne == cf.LookupColRef(pcrOne->m_id));
	cf.Destroy(pcrOne);

	// typed/named colref
	CWStringConst strName(GPOS_WSZ_LIT("C_CustKey"));
	CColRef *pcrTwo =
		cf.PcrCreate(pmdtypeint4, default_type_modifier, CName(&strName));
	GPOS_ASSERT(pcrTwo == cf.LookupColRef(pcrTwo->m_id));

	// clone previous colref
	CColRef *pcrThree = cf.PcrCreate(pcrTwo);
	GPOS_ASSERT(pcrThree != cf.LookupColRef(pcrTwo->m_id));
	GPOS_ASSERT(!pcrThree->Name().Equals(pcrTwo->Name()));
	cf.Destroy(pcrThree);

	cf.Destroy(pcrTwo);

	return GPOS_OK;
}


// EOF
