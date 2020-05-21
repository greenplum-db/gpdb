//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CFunctionalDependencyTest.cpp
//
//	@doc:
//		Tests for functional dependencies
//---------------------------------------------------------------------------
#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/CFunctionalDependency.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"

#include "unittest/base.h"
#include "unittest/gpopt/base/CFunctionalDependencyTest.h"
#include "unittest/gpopt/CTestUtils.h"


//---------------------------------------------------------------------------
//	@function:
//		CFunctionalDependencyTest::EresUnittest
//
//	@doc:
//		Unittest for functional dependencies
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFunctionalDependencyTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CFunctionalDependencyTest::EresUnittest_Basics)
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CFunctionalDependencyTest::EresUnittest_Basics
//
//	@doc:
//		Basic test for functional dependencies
//
//---------------------------------------------------------------------------
GPOS_RESULT
CFunctionalDependencyTest::EresUnittest_Basics()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
				(
				mp,
				&mda,
				NULL, /* pceeval */
				CTestUtils::GetCostModel(mp)
				);

	// get column factory from optimizer context object
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	CWStringConst strName(GPOS_WSZ_LIT("Test Column"));
	CName name(&strName);

	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>();

	const ULONG num_cols = 3;
	CColRefSet *pcrsLeft = GPOS_NEW(mp) CColRefSet(mp);
	CColRefSet *pcrsRight = GPOS_NEW(mp) CColRefSet(mp);
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *colref = col_factory->PcrCreate(pmdtypeint4, default_type_modifier, name);
		pcrsLeft->Include(colref);

		colref = col_factory->PcrCreate(pmdtypeint4, default_type_modifier, name);
		pcrsRight->Include(colref);
	}

	pcrsLeft->AddRef();
	pcrsRight->AddRef();
	CFunctionalDependency *pfdFst = GPOS_NEW(mp) CFunctionalDependency(pcrsLeft, pcrsRight);

	pcrsLeft->AddRef();
	pcrsRight->AddRef();
	CFunctionalDependency *pfdSnd = GPOS_NEW(mp) CFunctionalDependency(pcrsLeft, pcrsRight);

	GPOS_ASSERT(pfdFst->Equals(pfdSnd));
	GPOS_ASSERT(pfdFst->HashValue() == pfdSnd->HashValue());

	 CFunctionalDependencyArray *pdrgpfd = GPOS_NEW(mp) CFunctionalDependencyArray(mp);
	 pfdFst->AddRef();
	 pdrgpfd->Append(pfdFst);
	 pfdSnd->AddRef();
	 pdrgpfd->Append(pfdSnd);
	 GPOS_ASSERT(CFunctionalDependency::Equals(pdrgpfd, pdrgpfd));

	 CColRefArray *colref_array = CFunctionalDependency::PdrgpcrKeys(mp, pdrgpfd);
	 CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	 pcrs->Include(colref_array);
	 CColRefSet *pcrsKeys = CFunctionalDependency::PcrsKeys(mp, pdrgpfd);

	 GPOS_ASSERT(pcrsLeft->Equals(pcrs));
	 GPOS_ASSERT(pcrsKeys->Equals(pcrs));

	CAutoTrace at(mp);
	at.Os()
		<< "FD1:" << *pfdFst << std::endl
		<< "FD2:" << *pfdSnd << std::endl;

	pfdFst->Release();
	pfdSnd->Release();
	pcrsLeft->Release();
	pcrsRight->Release();
	pdrgpfd->Release();
	colref_array->Release();
	pcrs->Release();
	pcrsKeys->Release();

	return GPOS_OK;
}


// EOF
