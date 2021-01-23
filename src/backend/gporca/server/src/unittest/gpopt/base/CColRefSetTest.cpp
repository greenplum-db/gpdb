//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColRefSetTest.cpp
//
//	@doc:
//		Tests for CColRefSet
//---------------------------------------------------------------------------
#include "unittest/gpopt/base/CColRefSetTest.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColumnFactory.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/mdcache/CMDCache.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/IMDTypeInt4.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"


//---------------------------------------------------------------------------
//	@function:
//		CColRefSetTest::EresUnittest
//
//	@doc:
//		Unittest for bit vectors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColRefSetTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CColRefSetTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CColRefSetTest::EresUnittest_Basics
//
//	@doc:
//		Very basic tests; setops tested in context of CBitSet already
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColRefSetTest::EresUnittest_Basics()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// get column factory from optimizer context object
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	CWStringConst strName(GPOS_WSZ_LIT("Test Column"));
	CName name(&strName);

	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>();

	ULONG num_cols = 10;
	for (ULONG i = 0; i < num_cols; i++)
	{
		CColRef *colref =
			col_factory->PcrCreate(pmdtypeint4, default_type_modifier, name);
		pcrs->Include(colref);

		GPOS_ASSERT(pcrs->FMember(colref));
	}

	GPOS_ASSERT(pcrs->Size() == num_cols);

	CColRefSet *pcrsTwo = GPOS_NEW(mp) CColRefSet(mp, *pcrs);
	GPOS_ASSERT(pcrsTwo->Size() == num_cols);

	pcrsTwo->Release();
	pcrs->Release();

	return GPOS_OK;
}


// EOF
