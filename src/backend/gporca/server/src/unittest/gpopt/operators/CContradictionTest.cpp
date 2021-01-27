//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CContradictionTest.cpp
//
//	@doc:
//		Tests for contradiction detection
//---------------------------------------------------------------------------
#include "unittest/gpopt/operators/CContradictionTest.h"

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/ops.h"
#include "naucrates/md/IMDScalarOp.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"



//---------------------------------------------------------------------------
//	@function:
//		CContradictionTest::EresUnittest
//
//	@doc:
//		Unittest for contradiction detection
//
//---------------------------------------------------------------------------
GPOS_RESULT
CContradictionTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CContradictionTest::EresUnittest_Constraint),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CContradictionTest::EresUnittest_Constraint
//
//	@doc:
//		Tests for constraint property derivation and constraint push down
//
//---------------------------------------------------------------------------
GPOS_RESULT
CContradictionTest::EresUnittest_Constraint()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);


	typedef CExpression *(*Pfpexpr)(CMemoryPool *);

	Pfpexpr rgpf[] = {
		CTestUtils::PexprLogicalApplyWithOuterRef<CLogicalInnerApply>,
		CTestUtils::PexprLogicalApply<CLogicalLeftSemiApply>,
		CTestUtils::PexprLogicalApply<CLogicalLeftAntiSemiApply>,
		CTestUtils::PexprLogicalApplyWithOuterRef<CLogicalLeftOuterApply>,
		CTestUtils::PexprLogicalGet,
		CTestUtils::PexprLogicalGetPartitioned,
		CTestUtils::PexprLogicalSelect,
		CTestUtils::PexprLogicalSelectCmpToConst,
		CTestUtils::PexprLogicalSelectPartitioned,
		CTestUtils::PexprLogicalSelectWithContradiction,
		CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftSemiJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftAntiSemiJoin>,
		CTestUtils::PexprLogicalGbAgg,
		CTestUtils::PexprLogicalGbAggOverJoin,
		CTestUtils::PexprLogicalGbAggWithSum,
		CTestUtils::PexprLogicalLimit,
		CTestUtils::PexprLogicalNAryJoin,
		CTestUtils::PexprLogicalProject,
		CTestUtils::PexprConstTableGet5,
		CTestUtils::PexprLogicalDynamicGet,
		CTestUtils::PexprLogicalSequence,
		CTestUtils::PexprLogicalTVFTwoArgs,
	};

	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgpf); i++)
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		// generate simple expression
		CExpression *pexpr = rgpf[i](mp);

		// self-match
		GPOS_ASSERT(pexpr->FMatchDebug(pexpr));

		// debug print
		CWStringDynamic str(mp, GPOS_WSZ_LIT("\n"));
		COstreamString oss(&str);

		oss << "EXPR:" << std::endl << *pexpr << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();


#ifdef GPOS_DEBUG
		// derive properties on expression
		(void) pexpr->PdpDerive();

		oss << std::endl << "DERIVED PROPS:" << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();
		pexpr->DbgPrint();
#endif	// GPOS_DEBUG

		CExpression *pexprPreprocessed =
			CExpressionPreprocessor::PexprPreprocess(mp, pexpr);
		oss << std::endl
			<< "PREPROCESSED EXPR:" << std::endl
			<< *pexprPreprocessed << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

		// cleanup
		pexprPreprocessed->Release();
		pexpr->Release();
	}

	return GPOS_OK;
}

// EOF
