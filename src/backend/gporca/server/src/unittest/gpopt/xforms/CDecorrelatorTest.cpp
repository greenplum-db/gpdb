//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDecorrelatorTest.cpp
//
//	@doc:
//		Test for decorrelation
//---------------------------------------------------------------------------
#include "unittest/gpopt/xforms/CDecorrelatorTest.h"

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/CQueryContext.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CDecorrelator.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelatorTest::EresUnittest
//
//	@doc:
//		Unittest for predicate utilities
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDecorrelatorTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CDecorrelatorTest::EresUnittest_Decorrelate),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelatorTest::EresUnittest_Decorrelate
//
//	@doc:
//		Driver for decorrelation tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDecorrelatorTest::EresUnittest_Decorrelate()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// test cases
	typedef CExpression *(*Pfpexpr)(CMemoryPool *);
	Pfpexpr rgpf[] = {CTestUtils::PexprLogicalGbAggCorrelated,
					  CTestUtils::PexprLogicalSelectCorrelated,
					  CTestUtils::PexprLogicalJoinCorrelated,
					  CTestUtils::PexprLogicalProjectGbAggCorrelated};

	for (ULONG ulCase = 0; ulCase < GPOS_ARRAY_SIZE(rgpf); ulCase++)
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		// generate expression
		CExpression *pexpr = rgpf[ulCase](mp);

		CWStringDynamic str(mp);
		COstreamString oss(&str);
		oss << std::endl << "INPUT:" << std::endl << *pexpr << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

		CExpression *pexprResult = NULL;
		CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
		CColRefSet *outerRefs = pexpr->DeriveOuterReferences();
#ifdef GPOS_DEBUG
		BOOL fSuccess =
#endif	// GPOS_DEBUG
			CDecorrelator::FProcess(mp, pexpr, false /*fEqualityOnly*/,
									&pexprResult, pdrgpexpr, outerRefs);
		GPOS_ASSERT(fSuccess);

		// convert residuals into one single conjunct
		CExpression *pexprResidual =
			CPredicateUtils::PexprConjunction(mp, pdrgpexpr);

		oss << std::endl
			<< "RESIDUAL RELATIONAL:" << std::endl
			<< *pexprResult << std::endl;
		oss << std::endl
			<< "RESIDUAL SCALAR:" << std::endl
			<< *pexprResidual << std::endl;

		GPOS_TRACE(str.GetBuffer());
		str.Reset();

		pexprResult->Release();
		pexprResidual->Release();
		pexpr->Release();
	}

	return GPOS_OK;
}

// EOF
