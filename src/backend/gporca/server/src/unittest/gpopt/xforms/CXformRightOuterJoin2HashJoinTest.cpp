//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformRightOuterJoin2HashJoinTest.cpp
//
//	@doc:
//		Test for right outer join to hash join transform
//---------------------------------------------------------------------------
#include "unittest/gpopt/xforms/CXformRightOuterJoin2HashJoinTest.h"

#include "gpopt/base/CQueryContext.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/xforms/CXformRightOuterJoin2HashJoin.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"


//---------------------------------------------------------------------------
//	@function:
//		CXformRightOuterJoin2HashJoinTest::EresUnittest
//
//	@doc:
//		Unittest for predicate utilities
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformRightOuterJoin2HashJoinTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(
			CXformRightOuterJoin2HashJoinTest::EresUnittest_Transform),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CXformRightOuterJoin2HashJoinTest::EresUnittest_Transform
//
//	@doc:
//		Driver for transform tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformRightOuterJoin2HashJoinTest::EresUnittest_Transform()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);
	CAutoOptCtxt aoc(mp, &mda, nullptr, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	CXformContext *context = GPOS_NEW(mp) CXformContext(mp);
	CXformResult *result = GPOS_NEW(mp) CXformResult(mp);

	CXformRightOuterJoin2HashJoin *xform =
		GPOS_NEW(mp) CXformRightOuterJoin2HashJoin(mp);

	CExpression *pLogicalRightOuterJoin = CTestUtils::PexprRightOuterJoin(mp);

	// xform should work even if child stats aren't derived yet.
	GPOS_ASSERT(nullptr == (*pLogicalRightOuterJoin)[0]->Pstats());
	GPOS_ASSERT(nullptr == (*pLogicalRightOuterJoin)[1]->Pstats());

	(void) xform->Transform(context, result, pLogicalRightOuterJoin);

	pLogicalRightOuterJoin->Release();
	xform->Release();
	context->Release();
	result->Release();

	return GPOS_OK;
}

// EOF
