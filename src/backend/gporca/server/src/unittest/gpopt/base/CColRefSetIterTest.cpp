//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColRefSetIterTest.cpp
//
//	@doc:
//		Test of ColRefSet iterator
//---------------------------------------------------------------------------

#include "gpopt/base/CColumnFactory.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/base/CColRefSetIterTest.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"

#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/CMDProviderMemory.h"

//---------------------------------------------------------------------------
//	@function:
//		CColRefSetIterTest::EresUnittest
//
//	@doc:
//		Unittest for bit vectors
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColRefSetIterTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CColRefSetIterTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CColRefSetIterTest::EresUnittest_Basics
//
//	@doc:
//		Testing ctors/dtor; and colref decoding;
//		Other functionality already tested in vanilla CBitSetIter;
//
//---------------------------------------------------------------------------
GPOS_RESULT
CColRefSetIterTest::EresUnittest_Basics()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /* pceeval */,
					 CTestUtils::GetCostModel(mp));

	// get column factory from optimizer context object
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	CWStringConst strName(GPOS_WSZ_LIT("Test Column"));
	CName name(&strName);

	// create a int4 datum
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

	ULONG count = 0;
	CColRefSetIter crsi(*pcrs);
	while (crsi.Advance())
	{
		GPOS_ASSERT((BOOL) crsi);

		CColRef *colref = crsi.Pcr();
		GPOS_ASSERT(colref->Name().Equals(name));

		// to avoid unused variable warnings
		(void) colref->Id();

		count++;
	}

	GPOS_ASSERT(num_cols == count);
	GPOS_ASSERT(!((BOOL) crsi));

	pcrs->Release();

	return GPOS_OK;
}

// EOF
