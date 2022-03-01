//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware, Inc.
//
//	@filename:
//		CLogicalGbAggTest.cpp
//
//	@doc:
//		Tests for logical group by aggregate operator
//---------------------------------------------------------------------------
#include "unittest/gpopt/operators/CLogicalGbAggTest.h"

#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/COperator.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"



GPOS_RESULT
CLogicalGbAggTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CLogicalGbAggTest::EresUnittest_PxfsCandidates),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGbAggTest::EresUnittest_PxfsCandidates
//
//	@doc:
//		Test set of generated xform candidates
//
//---------------------------------------------------------------------------
GPOS_RESULT
CLogicalGbAggTest::EresUnittest_PxfsCandidates()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	BOOL test_passed = true;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CLogicalGbAgg *pGlobalGbAgg = GPOS_NEW(mp) CLogicalGbAgg(
		mp, GPOS_NEW(mp) CColRefArray(mp), COperator::EgbaggtypeGlobal);

	CXformSet *xfset = pGlobalGbAgg->PxfsCandidates(mp);

	// allow xform split agg on "global" aggregate
	GPOS_ASSERT(xfset->Get(CXform::ExfSplitGbAgg));
	if (!xfset->Get(CXform::ExfSplitGbAgg))
	{
		test_passed = false;
	}
	xfset->Release();

	CLogicalGbAgg *pLocalGbAgg = GPOS_NEW(mp) CLogicalGbAgg(
		mp, GPOS_NEW(mp) CColRefArray(mp), COperator::EgbaggtypeLocal);

	// don't allow xform split agg on "local" aggregate
	xfset = pLocalGbAgg->PxfsCandidates(mp);
	GPOS_ASSERT(!xfset->Get(CXform::ExfSplitGbAgg));
	if (xfset->Get(CXform::ExfSplitGbAgg))
	{
		test_passed = false;
	}
	xfset->Release();

	CLogicalGbAgg *pIntermediateGbAgg = GPOS_NEW(mp) CLogicalGbAgg(
		mp, GPOS_NEW(mp) CColRefArray(mp), COperator::EgbaggtypeIntermediate,
		false /* fGeneratesDuplicates */,
		GPOS_NEW(mp) CColRefArray(mp) /*pdrgpcrArgDQA*/);

	// don't allow xform split agg on "intermediate" aggregate
	xfset = pIntermediateGbAgg->PxfsCandidates(mp);
	GPOS_ASSERT(!xfset->Get(CXform::ExfSplitGbAgg));
	if (xfset->Get(CXform::ExfSplitGbAgg))
	{
		test_passed = false;
	}
	xfset->Release();

	pLocalGbAgg->Release();
	pGlobalGbAgg->Release();
	pIntermediateGbAgg->Release();

	return test_passed ? GPOS_OK : GPOS_FAILED;
}

// EOF
