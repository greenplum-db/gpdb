//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware Inc.
//
//	@filename:
//		CGroupTest.cpp
//
//	@doc:
//		Test for CGroup
//---------------------------------------------------------------------------
#include "unittest/gpopt/base/CGroupTest.h"

#include "gpos/error/CAutoTrace.h"
#include "gpos/task/CAutoTraceFlag.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/operators/CLogicalNAryJoin.h"
#include "gpopt/search/CGroup.h"
#include "gpopt/search/CGroupProxy.h"
#include "gpopt/search/CMemo.h"

#include "unittest/base.h"
#include "unittest/gpopt/CSubqueryTestUtils.h"

//---------------------------------------------------------------------------
//	@function:
//		CGroupTest::EresUnittest
//
//	@doc:
//		Unittest for cgroup
//
//---------------------------------------------------------------------------
GPOS_RESULT
CGroupTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_FResetStatsOnCGroupWithDuplicateGroup),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


static CExpression *
InsertGroupIntoMemo(CMemo *pmemo, CMemoryPool *mp)
{
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	pexprGet->Pop()->AddRef();
	CGroupExpression *pgexpr = GPOS_NEW(mp)
		CGroupExpression(mp, pexprGet->Pop(), GPOS_NEW(mp) CGroupArray(mp),
						 CXform::ExfInvalid /*exfidOrigin*/,
						 NULL /*pgexprOrigin*/, false /*fIntermediate*/);

	pmemo->PgroupInsert(NULL, pexprGet, pgexpr);

	return pexprGet;
}


GPOS_RESULT
CGroupTest::EresUnittest_FResetStatsOnCGroupWithDuplicateGroup()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	CMemo *pmemo = GPOS_NEW(mp) CMemo(mp);

	CExpression *pexprGet1 = InsertGroupIntoMemo(pmemo, mp);
	CExpression *pexprGet2 = InsertGroupIntoMemo(pmemo, mp);

	CGroupExpression *pgexprFirst = NULL;
	{
		CGroupProxy gp(pmemo->Pgroup(0));
		pgexprFirst = gp.PgexprFirst();
	}

	// mark group (1) as duplicates of group (0)
	pmemo->MarkDuplicates(pmemo->Pgroup(0), pmemo->Pgroup(1));

	CExpressionHandle exprhdl(mp);
	exprhdl.Attach(pgexprFirst);
	exprhdl.DeriveStats(mp, mp, NULL, NULL);

	// After deriving stats on group 0, we should now have identical stats
	// reference for group (0) and group (1)
	// Actually, the stats is ONLY derived of group (1) and not of group (0),
	// because group (1) is the duplicate group
	if (pmemo->Pgroup(0)->Pstats() != pmemo->Pgroup(1)->Pstats())
	{
		// stat references should be identical for duplicate groups
		GPOS_DELETE(pmemo);
		pexprGet1->Release();
		pexprGet2->Release();
		return GPOS_FAILED;
	}

	pmemo->Pgroup(0)->FResetStats();

	// By resetting stats on group (0), we really are resetting the stats on
	// group (1). group (0) stats is never set in the first place.
	if (pmemo->Pgroup(0)->Pstats() != NULL ||
		pmemo->Pgroup(1)->Pstats() != NULL)
	{
		GPOS_DELETE(pmemo);
		pexprGet1->Release();
		pexprGet2->Release();
		return GPOS_FAILED;
	}

	GPOS_DELETE(pmemo);
	pexprGet1->Release();
	pexprGet2->Release();
	return GPOS_OK;
}

// EOF
