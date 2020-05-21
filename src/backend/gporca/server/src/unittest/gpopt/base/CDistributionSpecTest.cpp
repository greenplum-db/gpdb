//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecTest.cpp
//
//	@doc:
//		Tests for distribution specification
//---------------------------------------------------------------------------

#include "gpos/error/CAutoTrace.h"

#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/CMDProviderMemory.h"

#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/CDistributionSpecUniversal.h"
	
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColumnFactory.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/operators/CExpressionHandle.h"


#include "unittest/base.h"
#include "unittest/gpopt/base/CDistributionSpecTest.h"
#include "unittest/gpopt/CTestUtils.h"


const CHAR *szMDFileName = "../data/dxl/metadata/md.xml";

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest
//
//	@doc:
//		Unittest for distribution spec classes
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CDistributionSpecTest::EresUnittest_Any),
		GPOS_UNITTEST_FUNC(CDistributionSpecTest::EresUnittest_Singleton),
		GPOS_UNITTEST_FUNC(CDistributionSpecTest::EresUnittest_Random),
		GPOS_UNITTEST_FUNC(CDistributionSpecTest::EresUnittest_Replicated),
		GPOS_UNITTEST_FUNC(CDistributionSpecTest::EresUnittest_Universal),
		GPOS_UNITTEST_FUNC(CDistributionSpecTest::EresUnittest_Hashed),
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC_ASSERT(CDistributionSpecTest::EresUnittest_NegativeAny),
		GPOS_UNITTEST_FUNC_ASSERT(CDistributionSpecTest::EresUnittest_NegativeUniversal),
		GPOS_UNITTEST_FUNC_ASSERT(CDistributionSpecTest::EresUnittest_NegativeRandom),
#endif // GPOS_DEBUG
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_Any
//
//	@doc:
//		Test for "any" distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_Any()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// any distribution
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);
	
	GPOS_ASSERT(pdsany->FSatisfies(pdsany));
	GPOS_ASSERT(pdsany->Matches(pdsany));

	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << *pdsany << std::endl;
	
	pdsany->Release();
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_Random
//
//	@doc:
//		Test for forced random distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_Random()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));
	COptCtxt *poptctxt = COptCtxt::PoctxtFromTLS();

	// basic tests with random distribution
	poptctxt->MarkDMLQuery(true /*fDMLQuery*/);
	CDistributionSpecRandom *pdsRandomDuplicateSensitive = GPOS_NEW(mp) CDistributionSpecRandom();

	poptctxt->MarkDMLQuery(false /*fDMLQuery*/);
	CDistributionSpecRandom *pdsRandomNonDuplicateSensitive = GPOS_NEW(mp) CDistributionSpecRandom();
	
	GPOS_ASSERT(pdsRandomDuplicateSensitive->FSatisfies(pdsRandomDuplicateSensitive));
	GPOS_ASSERT(pdsRandomDuplicateSensitive->Matches(pdsRandomDuplicateSensitive));
	GPOS_ASSERT(pdsRandomDuplicateSensitive->FSatisfies(pdsRandomNonDuplicateSensitive));
	GPOS_ASSERT(!pdsRandomNonDuplicateSensitive->FSatisfies(pdsRandomDuplicateSensitive));

	// random and universal
	CDistributionSpecUniversal *pdsUniversal =	GPOS_NEW(mp) CDistributionSpecUniversal();
	GPOS_ASSERT(pdsUniversal->FSatisfies(pdsRandomNonDuplicateSensitive));
	GPOS_ASSERT(!pdsUniversal->FSatisfies(pdsRandomDuplicateSensitive));
	
	// random and any
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);
	
	GPOS_ASSERT(!pdsany->FSatisfies(pdsRandomDuplicateSensitive));
	GPOS_ASSERT(pdsRandomDuplicateSensitive->FSatisfies(pdsany));	
	
	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << *pdsRandomDuplicateSensitive << std::endl;

	pdsRandomDuplicateSensitive->Release();
	pdsRandomNonDuplicateSensitive->Release();
	pdsUniversal->Release();
	pdsany->Release();
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_Replicated
//
//	@doc:
//		Test for replicated distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_Replicated()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	// basic tests with replicated distributions
	CDistributionSpecReplicated *pdsreplicated = GPOS_NEW(mp) CDistributionSpecReplicated();
	
	GPOS_ASSERT(pdsreplicated->FSatisfies(pdsreplicated));
	GPOS_ASSERT(pdsreplicated->Matches(pdsreplicated));
	
	// replicated and random
	CDistributionSpecRandom *pdsrandom = GPOS_NEW(mp) CDistributionSpecRandom();
	
	GPOS_ASSERT(!pdsrandom->FSatisfies(pdsreplicated));
	GPOS_ASSERT(pdsreplicated->FSatisfies(pdsrandom));
	
	// replicated and any 
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);
	
	GPOS_ASSERT(!pdsany->FSatisfies(pdsreplicated));
	GPOS_ASSERT(pdsreplicated->FSatisfies(pdsany));	
	
	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << *pdsreplicated << std::endl;

	pdsreplicated->Release();
	pdsrandom->Release();
	pdsany->Release();
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_Singleton
//
//	@doc:
//		Test for singleton distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_Singleton()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	// basic tests with singleton distributions
	CDistributionSpecSingleton *pdssSegment =
			GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstSegment);
	CDistributionSpecSingleton *pdssMaster =
			GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	
	GPOS_ASSERT(pdssMaster->FSatisfies(pdssMaster));
	GPOS_ASSERT(pdssMaster->Matches(pdssMaster));
	
	GPOS_ASSERT(pdssSegment->FSatisfies(pdssSegment));
	GPOS_ASSERT(pdssSegment->Matches(pdssSegment));

	GPOS_ASSERT(!pdssMaster->FSatisfies(pdssSegment) &&
				!pdssSegment->FSatisfies(pdssMaster));

	// singleton and replicated
	CDistributionSpecReplicated *pdsreplicated = GPOS_NEW(mp) CDistributionSpecReplicated();
	
	GPOS_ASSERT(pdsreplicated->FSatisfies(pdssSegment));
	GPOS_ASSERT(!pdsreplicated->FSatisfies(pdssMaster));
	
	GPOS_ASSERT(!pdssSegment->FSatisfies(pdsreplicated));
	
	// singleton and random
	CDistributionSpecRandom *pdsrandom = GPOS_NEW(mp) CDistributionSpecRandom();
	
	GPOS_ASSERT(!pdsrandom->FSatisfies(pdssSegment));
	GPOS_ASSERT(!pdsrandom->FSatisfies(pdssMaster));
	GPOS_ASSERT(!pdssSegment->FSatisfies(pdsrandom));
	GPOS_ASSERT(!pdssMaster->FSatisfies(pdsrandom));
	
	// singleton and any 
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);
	
	GPOS_ASSERT(!pdsany->FSatisfies(pdssSegment));
	GPOS_ASSERT(pdssSegment->FSatisfies(pdsany));

	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << *pdssMaster << std::endl;
	at.Os() << *pdssSegment << std::endl;
	
	pdssMaster->Release();
	pdssSegment->Release();
	pdsreplicated->Release();
	pdsrandom->Release();
	pdsany->Release();
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_Universal
//
//	@doc:
//		Test for universal distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_Universal()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	// basic tests with universal distributions
	CDistributionSpecUniversal *pdsuniversal = GPOS_NEW(mp) CDistributionSpecUniversal();

	GPOS_ASSERT(pdsuniversal->FSatisfies(pdsuniversal));
	GPOS_ASSERT(pdsuniversal->Matches(pdsuniversal));

	// universal and singleton
	CDistributionSpecSingleton *pdssSegment =
			GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstSegment);

	GPOS_ASSERT(pdsuniversal->FSatisfies(pdssSegment));

	// universal and replicated
	CDistributionSpecReplicated *pdsreplicated = GPOS_NEW(mp) CDistributionSpecReplicated();

	GPOS_ASSERT(pdsuniversal->FSatisfies(pdsreplicated));

	// universal and random
	CDistributionSpecRandom *pdsrandom = GPOS_NEW(mp) CDistributionSpecRandom();

	GPOS_ASSERT(pdsuniversal->FSatisfies(pdsrandom));

	// universal and any
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);

	GPOS_ASSERT(pdsuniversal->FSatisfies(pdsany));

	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << *pdsuniversal << std::endl;

	pdsuniversal->Release();
	pdssSegment->Release();
	pdsreplicated->Release();
	pdsrandom->Release();
	pdsany->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_Hashed
//
//	@doc:
//		Test for hashed distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_Hashed()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);
	
	// install opt context
	CAutoOptCtxt aoc
				(
				mp,
				&mda,
				NULL,  /* pceeval */
				CTestUtils::GetCostModel(mp)				
				);

	// get column factory from optimizer context object
	COptCtxt *poptctxt = COptCtxt::PoctxtFromTLS();
	CColumnFactory *col_factory = poptctxt->Pcf();
		
	CWStringConst strA(GPOS_WSZ_LIT("A"));
	CWStringConst strB(GPOS_WSZ_LIT("B"));
	
	CName nameA(&strA);
	CName nameB(&strB);
		
	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);

	CColRef *pcrA = col_factory->PcrCreate(pmdtypeint4, default_type_modifier, nameA);
	CColRef *pcrB = col_factory->PcrCreate(pmdtypeint4, default_type_modifier, nameB);
	
	CExpression *pexprScalarA = CUtils::PexprScalarIdent(mp, pcrA);
	CExpression *pexprScalarB = CUtils::PexprScalarIdent(mp, pcrB);

	CExpressionArray *prgpexpr1 = GPOS_NEW(mp) CExpressionArray(mp);
	prgpexpr1->Append(pexprScalarA);
	prgpexpr1->Append(pexprScalarB);
	
	CExpressionArray *prgpexpr2 = GPOS_NEW(mp) CExpressionArray(mp);
	pexprScalarA->AddRef();
	prgpexpr2->Append(pexprScalarA);

	// basic hash distribution tests

	// HD{A,B}, nulls colocated, duplicate insensitive
	poptctxt->MarkDMLQuery(false /*fDMLQuery*/);
	CDistributionSpecHashed *pdshashed1 = GPOS_NEW(mp) CDistributionSpecHashed(prgpexpr1, true /* fNullsCollocated */);
	GPOS_ASSERT(pdshashed1->FSatisfies(pdshashed1));
	GPOS_ASSERT(pdshashed1->Matches(pdshashed1));
	
	// HD{A}, nulls colocated, duplicate sensitive
	poptctxt->MarkDMLQuery(true /*fDMLQuery*/);
	CDistributionSpecHashed *pdshashed2 = GPOS_NEW(mp) CDistributionSpecHashed(prgpexpr2, true /* fNullsCollocated */);
	GPOS_ASSERT(pdshashed2->FSatisfies(pdshashed2));
	GPOS_ASSERT(pdshashed2->Matches(pdshashed2));
		
	// HD{A}, nulls not colocated, duplicate sensitive
	prgpexpr2->AddRef();
	CDistributionSpecHashed *pdshashed3 = GPOS_NEW(mp) CDistributionSpecHashed(prgpexpr2, false /* fNullsCollocated */);
	GPOS_ASSERT(pdshashed3->FSatisfies(pdshashed3));
	GPOS_ASSERT(pdshashed3->Matches(pdshashed3));

	// ({A,B}, true, true) does not satisfy ({A}, true, false)
	GPOS_ASSERT(!pdshashed1->FMatchSubset(pdshashed2));
	GPOS_ASSERT(!pdshashed1->FSatisfies(pdshashed2));
	
	// ({A}, true) satisfies ({A, B}, true)
	GPOS_ASSERT(pdshashed2->FMatchSubset(pdshashed1));
	GPOS_ASSERT(pdshashed2->FSatisfies(pdshashed1));
	
	// ({A}, true) satisfies ({A}, false)
	GPOS_ASSERT(pdshashed2->FMatchSubset(pdshashed3));
	GPOS_ASSERT(pdshashed2->FSatisfies(pdshashed3));

	// ({A}, false) does not satisfy ({A}, true)
	GPOS_ASSERT(!pdshashed1->FMatchSubset(pdshashed3));
	GPOS_ASSERT(!pdshashed1->FSatisfies(pdshashed3));
	
	// hashed and universal
	CDistributionSpecUniversal *pdsuniversal =
			GPOS_NEW(mp) CDistributionSpecUniversal();

	GPOS_ASSERT(pdsuniversal->FSatisfies(pdshashed1));

	// hashed and singleton
	CDistributionSpecSingleton *pdssSegment =
			GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstSegment);
	
	GPOS_ASSERT(!pdshashed1->FSatisfies(pdssSegment));
	GPOS_ASSERT(!pdssSegment->Matches(pdshashed1));

	// hashed and replicated
	CDistributionSpecReplicated *pdsreplicated = GPOS_NEW(mp) CDistributionSpecReplicated();
	
	GPOS_ASSERT(pdsreplicated->FSatisfies(pdshashed1));
	GPOS_ASSERT(!pdshashed1->FSatisfies(pdsreplicated));
		
	// hashed and random
	CDistributionSpecRandom *pdsrandom = GPOS_NEW(mp) CDistributionSpecRandom();
	
	GPOS_ASSERT(!pdsrandom->FSatisfies(pdshashed1));
	GPOS_ASSERT(!pdshashed1->FSatisfies(pdsrandom));
	
	// hashed and any 
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);
	
	GPOS_ASSERT(!pdsany->FSatisfies(pdshashed1));
	GPOS_ASSERT(pdshashed1->FSatisfies(pdsany));
	
	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << *pdshashed1 << std::endl;
	at.Os() << *pdshashed2 << std::endl;
	at.Os() << *pdshashed3 << std::endl;

	pdshashed1->Release();
	pdshashed2->Release();
	pdshashed3->Release();

	pdssSegment->Release();
	pdsreplicated->Release();
	pdsrandom->Release();
	pdsany->Release();
	pdsuniversal->Release();
	
	return GPOS_OK;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_NegativeAny
//
//	@doc:
//		Negative test for ANY distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_NegativeAny()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// cannot add enforcers for ANY distribution
	CDistributionSpecAny *pdsany = GPOS_NEW(mp) CDistributionSpecAny(COperator::EopSentinel);
	CExpressionHandle *pexprhdl = GPOS_NEW(mp) CExpressionHandle(mp);
	pdsany->AppendEnforcers(NULL /*mp*/, *pexprhdl, NULL /*prpp*/, NULL /*pdrgpexpr*/, NULL /*pexpr*/);
	pdsany->Release();
	GPOS_DELETE(pexprhdl);

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_NegativeUniversal
//
//	@doc:
//		Negative test for universal distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_NegativeUniversal()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// cannot add enforcers for Universal distribution
	CDistributionSpecUniversal *pdsuniversal = GPOS_NEW(mp) CDistributionSpecUniversal();
	CExpressionHandle *pexprhdl = GPOS_NEW(mp) CExpressionHandle(mp);

	pdsuniversal->AppendEnforcers(NULL /*mp*/, *pexprhdl, NULL /*prpp*/, NULL /*pdrgpexpr*/, NULL /*pexpr*/);
	pdsuniversal->Release();
	GPOS_DELETE(pexprhdl);

	return GPOS_FAILED;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecTest::EresUnittest_NegativeRandom
//
//	@doc:
//		Negative test for random distribution spec
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDistributionSpecTest::EresUnittest_NegativeRandom()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	// cannot add enforcers for Random distribution
	CDistributionSpecRandom *pdsrandom = GPOS_NEW(mp) CDistributionSpecRandom();
	CExpressionHandle *pexprhdl = GPOS_NEW(mp) CExpressionHandle(mp);

	pdsrandom->AppendEnforcers(NULL /*mp*/, *pexprhdl, NULL /*prpp*/, NULL /*pdrgpexpr*/, NULL /*pexpr*/);
	pdsrandom->Release();
	GPOS_DELETE(pexprhdl);
	
	return GPOS_FAILED;
}
#endif // GPOS_DEBUG

// EOF
