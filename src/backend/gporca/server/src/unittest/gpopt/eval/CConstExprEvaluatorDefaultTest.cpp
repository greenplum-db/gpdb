//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorDefaultTest.cpp
//
//	@doc:
//		Unit tests for CConstExprEvaluatorDefault
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/eval/CConstExprEvaluatorDefaultTest.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/operators/ops.h"

#include "naucrates/md/CMDProviderMemory.h"

using namespace gpos;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorDefaultTest::EresUnittest
//
//	@doc:
//		Executes all unit tests for CConstExprEvaluatorDefault
//
//---------------------------------------------------------------------------
GPOS_RESULT
CConstExprEvaluatorDefaultTest::EresUnittest()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CConstExprEvaluatorDefault *pceevaldefault =
		GPOS_NEW(mp) CConstExprEvaluatorDefault();
	GPOS_ASSERT(!pceevaldefault->FCanEvalExpressions());

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// Test evaluation of an integer constant
	{
		ULONG ulVal = 123456;
		CExpression *pexprUl = CUtils::PexprScalarConstInt4(mp, ulVal);
#ifdef GPOS_DEBUG
		CExpression *pexprUlResult = pceevaldefault->PexprEval(pexprUl);
		CScalarConst *pscalarconstUl = CScalarConst::PopConvert(pexprUl->Pop());
		CScalarConst *pscalarconstUlResult =
			CScalarConst::PopConvert(pexprUlResult->Pop());
		GPOS_ASSERT(pscalarconstUl->Matches(pscalarconstUlResult));
		pexprUlResult->Release();
#endif	// GPOS_DEBUG
		pexprUl->Release();
	}

	// Test evaluation of a null test expression
	{
		ULONG ulVal = 123456;
		CExpression *pexprUl = CUtils::PexprScalarConstInt4(mp, ulVal);
		CExpression *pexprIsNull = CUtils::PexprIsNull(mp, pexprUl);
#ifdef GPOS_DEBUG
		CExpression *pexprResult = pceevaldefault->PexprEval(pexprIsNull);
		CScalarNullTest *pscalarnulltest =
			CScalarNullTest::PopConvert(pexprIsNull->Pop());
		GPOS_ASSERT(pscalarnulltest->Matches(pexprResult->Pop()));
		pexprResult->Release();
#endif	// GPOS_DEBUG
		pexprIsNull->Release();
	}
	pceevaldefault->Release();

	return GPOS_OK;
}

// EOF
