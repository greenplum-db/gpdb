//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CRangeTest.cpp
//
//	@doc:
//		Test for ranges
//---------------------------------------------------------------------------
#include "unittest/base.h"
#include "unittest/gpopt/base/CRangeTest.h"

#include "gpos/error/CAutoTrace.h"

#include "naucrates/base/CDatumInt2GPDB.h"
#include "naucrates/base/CDatumInt4GPDB.h"
#include "naucrates/base/CDatumInt8GPDB.h"

#include "gpopt/base/CDefaultComparator.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::CreateInt2Datum
//
//	@doc:
//		Creates an int2 datum.
//
//---------------------------------------------------------------------------
IDatum *
CRangeTest::CreateInt2Datum
	(
	gpos::CMemoryPool *mp,
	INT i
	)
{
	return GPOS_NEW(mp) gpnaucrates::CDatumInt2GPDB(CTestUtils::m_sysidDefault, (SINT) i);
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::CreateInt4Datum
//
//	@doc:
//		Creates an int4 datum.
//
//---------------------------------------------------------------------------
IDatum *
CRangeTest::CreateInt4Datum
	(
	gpos::CMemoryPool *mp,
	INT i
	)
{
	return GPOS_NEW(mp) gpnaucrates::CDatumInt4GPDB(CTestUtils::m_sysidDefault, i);
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::CreateInt2Datum
//
//	@doc:
//		Creates an int8 datum.
//
//---------------------------------------------------------------------------
IDatum *
CRangeTest::CreateInt8Datum
	(
	gpos::CMemoryPool *mp,
	INT li
	)
{
	return GPOS_NEW(mp) gpnaucrates::CDatumInt8GPDB(CTestUtils::m_sysidDefault, (LINT) li);
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::EresUnittest
//
//	@doc:
//		Unittest for ranges
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRangeTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CRangeTest::EresUnittest_CRangeInt2),
		GPOS_UNITTEST_FUNC(CRangeTest::EresUnittest_CRangeInt4),
		GPOS_UNITTEST_FUNC(CRangeTest::EresUnittest_CRangeInt8),
		GPOS_UNITTEST_FUNC(CRangeTest::EresUnittest_CRangeFromScalar),
		};

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL, /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::EresUnittest_CRangeInt2
//
//	@doc:
//		Int2 range tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRangeTest::EresUnittest_CRangeInt2()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	IMDTypeInt2 *pmdtypeint2 = (IMDTypeInt2 *) mda.PtMDType<IMDTypeInt2>(CTestUtils::m_sysidDefault);
	IMDId *mdid = pmdtypeint2->MDId();

	return EresInitAndCheckRanges(mp, mdid, &CreateInt2Datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::EresUnittest_CRangeInt4
//
//	@doc:
//		Int4 range tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRangeTest::EresUnittest_CRangeInt4()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	IMDTypeInt4 *pmdtypeint4 = (IMDTypeInt4 *) mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
	IMDId *mdid = pmdtypeint4->MDId();

	return EresInitAndCheckRanges(mp, mdid, &CreateInt4Datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::EresUnittest_CRangeInt8
//
//	@doc:
//		Int8 range tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRangeTest::EresUnittest_CRangeInt8()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	IMDTypeInt8 *pmdtypeint8 = (IMDTypeInt8 *) mda.PtMDType<IMDTypeInt8>(CTestUtils::m_sysidDefault);
	IMDId *mdid = pmdtypeint8->MDId();

	return EresInitAndCheckRanges(mp, mdid, &CreateInt8Datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::EresUnittest_CRangeFromScalar
//
//	@doc:
//		Range From Scalar Expression test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRangeTest::EresUnittest_CRangeFromScalar()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	CColRefSet *pcrs = pexprGet->DeriveOutputColumns();
	CColRef *colref =  pcrs->PcrAny();

	CDatumInt4GPDB *pdatumint4 = GPOS_NEW(mp) CDatumInt4GPDB(CTestUtils::m_sysidDefault, 10 /*val*/);

	IMDType::ECmpType rgecmpt[] =
			{
			IMDType::EcmptEq,
			IMDType::EcmptL,
			IMDType::EcmptLEq,
			IMDType::EcmptG,
			IMDType::EcmptGEq,
			};

	CConstExprEvaluatorDefault *pceeval = GPOS_NEW(mp) CConstExprEvaluatorDefault();
	CDefaultComparator comp(pceeval);
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgecmpt); ul++)
	{
		pdatumint4->AddRef();
		CRange *prange = GPOS_NEW(mp) CRange
									(
									&comp,
									rgecmpt[ul],
									pdatumint4
									);

		PrintRange(mp, colref, prange);
		prange->Release();
	}

	pceeval->Release();
	pdatumint4->Release();
	pexprGet->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::EresInitAndCheckRanges
//
//	@doc:
//		Create and test ranges
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRangeTest::EresInitAndCheckRanges
	(
	CMemoryPool *mp,
	IMDId *mdid,
	PfPdatum pf
	)
{
	CConstExprEvaluatorDefault *pceeval = GPOS_NEW(mp) CConstExprEvaluatorDefault();
	CDefaultComparator comp(pceeval);

	// generate ranges
	mdid->AddRef();
	CRange *prange1 = GPOS_NEW(mp) CRange
								(
								mdid,
								&comp,
								(*pf)(mp, 10),
								CRange::EriIncluded,
								NULL,
								CRange::EriExcluded
								); // [10, inf)

	mdid->AddRef();
	CRange *prange2 = GPOS_NEW(mp) CRange
								(
								mdid,
								&comp,
								NULL,
								CRange::EriExcluded,
								(*pf)(mp, 20),
								CRange::EriIncluded
								); // (-inf, 20]

	mdid->AddRef();
	CRange *prange3 = GPOS_NEW(mp) CRange
								(
								mdid,
								&comp,
								(*pf)(mp, -20),
								CRange::EriExcluded,
								(*pf)(mp, 0),
								CRange::EriIncluded
								); // (-20, 0]

	mdid->AddRef();
	CRange *prange4 = GPOS_NEW(mp) CRange
								(
								mdid,
								&comp,
								(*pf)(mp, -10),
								CRange::EriIncluded,
								(*pf)(mp, 10),
								CRange::EriExcluded
								); // [-10, 10)

	mdid->AddRef();
	CRange *prange5 = GPOS_NEW(mp) CRange
								(
								mdid,
								&comp,
								(*pf)(mp, 0),
								CRange::EriIncluded,
								(*pf)(mp, 0),
								CRange::EriIncluded
								); // [0, 0]

	TestRangeRelationship(mp, prange1, prange2, prange3, prange4, prange5);

	prange1->Release();
	prange2->Release();
	prange3->Release();
	prange4->Release();
	prange5->Release();
	pceeval->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::TestRangeRelationship
//
//	@doc:
//		Test relationship between ranges
//
//---------------------------------------------------------------------------
void
CRangeTest::TestRangeRelationship
	(
	CMemoryPool *mp,
	CRange *prange1,
	CRange *prange2,
	CRange *prange3,
	CRange *prange4,
	CRange *prange5
	)
{
	GPOS_ASSERT_MSG(!prange4->FDisjointLeft(prange5), "[-10, 10) does not end before [0, 0]");
	GPOS_ASSERT_MSG(prange4->FDisjointLeft(prange1), "[-10, 10) ends before [10, inf)");

	GPOS_ASSERT_MSG(!prange1->Contains(prange2), "[10, inf) does not contain (-inf, 20]");
	GPOS_ASSERT_MSG(prange2->Contains(prange4), "(-inf, 20] contains [-10, 10)");
	GPOS_ASSERT_MSG(prange3->Contains(prange5), "(-20, 0] contains [0, 0]");

	GPOS_ASSERT_MSG(!prange3->FOverlapsLeft(prange2), "(-20, 0] does not overlap beginning (-inf, 20]");
	GPOS_ASSERT_MSG(prange2->FOverlapsLeft(prange1), "(-inf, 20] overlaps beginning [10, inf)");
	GPOS_ASSERT_MSG(prange3->FOverlapsLeft(prange4), "(-20, 0] overlaps beginning [-10, 10)");

	GPOS_ASSERT_MSG(!prange2->FOverlapsRight(prange3), "(-inf, 20] does not overlap end (-20,0]");
	GPOS_ASSERT_MSG(prange1->FOverlapsRight(prange2), "[10, inf) overlaps end (-inf, 20)");
	GPOS_ASSERT_MSG(prange4->FOverlapsRight(prange3), "[-10, 10) overlaps end (-20, 0]");

	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	CColRefSet *pcrs = pexprGet->DeriveOutputColumns();
	CColRef *colref =  pcrs->PcrAny();

	PrintRange(mp, colref, prange1);
	PrintRange(mp, colref, prange2);
	PrintRange(mp, colref, prange3);
	PrintRange(mp, colref, prange4);
	PrintRange(mp, colref, prange5);

	pexprGet->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CRangeTest::PrintRange
//
//	@doc:
//		Test relationship between ranges
//
//---------------------------------------------------------------------------
void
CRangeTest::PrintRange
	(
	CMemoryPool *mp,
	CColRef *colref,
	CRange *prange
	)
{
	CExpression *pexpr = prange->PexprScalar(mp, colref);

	// debug print
	CAutoTrace at(mp);
	at.Os() << std::endl;
	at.Os() << "RANGE: " << *prange << std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
	pexpr->Release();
}

// EOF
