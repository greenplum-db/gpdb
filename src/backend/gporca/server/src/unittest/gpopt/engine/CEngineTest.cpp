//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2011 EMC Corp.
//
//	@filename:
//		CEngineTest.cpp
//
//	@doc:
//		Test for CEngine
//---------------------------------------------------------------------------
#include "gpos/error/CAutoTrace.h"
#include "gpos/task/CAutoTraceFlag.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/search/CGroup.h"
#include "gpopt/search/CGroupProxy.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/operators/ops.h"

#include "unittest/base.h"
#include "unittest/gpopt/engine/CEngineTest.h"
#include "unittest/gpopt/CSubqueryTestUtils.h"

ULONG CEngineTest::m_ulTestCounter = 0; // start from first test
ULONG CEngineTest::m_ulTestCounterSubq = 0; // start from first test

//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest
//
//	@doc:
//		Unittest for engine
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest()
{
	CUnittest rgut[] =
	{
		GPOS_UNITTEST_FUNC(EresUnittest_Basic),
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemo),
		GPOS_UNITTEST_FUNC(EresUnittest_AppendStats),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoWithSubqueries),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoWithGrouping),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoWithTVF),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoWithPartitioning),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoWithWindowing),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoLargeJoins),
		GPOS_UNITTEST_FUNC(EresUnittest_BuildMemoWithCTE),
#endif // GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_Basic
//
//	@doc:
//		Basic test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTraceFlag atf1(EopttracePrintOptimizationStatistics, true);
	CAutoTraceFlag atf2(EopttracePrintMemoAfterExploration, true);
	CAutoTraceFlag atf3(EopttracePrintMemoAfterImplementation, true);
	CAutoTraceFlag atf4(EopttracePrintMemoAfterOptimization, true);
	CAutoTraceFlag atf6(EopttracePrintXform, true);
	CAutoTraceFlag atf7(EopttracePrintGroupProperties, true);
	CAutoTraceFlag atf8(EopttracePrintOptimizationContext, true);
	CAutoTraceFlag atf9(EopttracePrintXformPattern, true);

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

	CEngine eng(mp);

	// generate  join expression
	CExpression *pexpr = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp);

	// generate query context
	CQueryContext *pqc = CTestUtils::PqcGenerate(mp, pexpr);

	// Initialize engine
	eng.Init(pqc, NULL /*search_stage_array*/);

	// optimize query
	eng.Optimize();

	// extract plan
	CExpression *pexprPlan = eng.PexprExtractPlan();
	GPOS_ASSERT(NULL != pexprPlan);

	// clean up
	pexpr->Release();
	pexprPlan->Release();
	GPOS_DELETE(pqc);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresOptimize
//
//	@doc:
//		Helper for optimizing deep join trees;
//		generate an array of join expressions for the given relation, if the
//		bit corresponding to an expression is set, the optimizer is invoked
//		for that expression
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresOptimize
	(
	FnOptimize *pfopt,
	CWStringConst *str, // array of relation names
	ULONG *pul,// array of relation OIDs
	ULONG ulRels, // number of array entries
	CBitSet *pbs // if a bit is set, the corresponding join expression will be optimized
	)
{
	GPOS_ASSERT(NULL != pfopt);
	GPOS_ASSERT(NULL != str);
	GPOS_ASSERT(NULL != pul);
	GPOS_ASSERT(NULL != pbs);

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);
	// scope for optimization context
	{
		CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL, /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

		// generate cross product expressions
		CExpressionJoinsArray *pdrgpexprCrossProducts = CTestUtils::PdrgpexprJoins(mp, str, pul, ulRels, true /*fCrossProduct*/);

		// generate join expressions
		CExpressionJoinsArray *pdrgpexpr = CTestUtils::PdrgpexprJoins(mp, str, pul, ulRels, false  /*fCrossProduct*/);

		// build memo for each expression
		for (ULONG ul = m_ulTestCounter; ul < ulRels; ul++)
		{
			if (pbs->Get(ul))
			{
				pfopt(mp, (*pdrgpexprCrossProducts)[ul], NULL /*search_stage_array*/);
				GPOS_CHECK_ABORT;

				pfopt(mp, (*pdrgpexpr)[ul], NULL /*search_stage_array*/);
				GPOS_CHECK_ABORT;

				m_ulTestCounter++;
			}
		}

		// reset counter
		m_ulTestCounter = 0;

		(*pdrgpexprCrossProducts)[ulRels - 1]->Release();
		CRefCount::SafeRelease(pdrgpexprCrossProducts);

		(*pdrgpexpr)[ulRels - 1]->Release();
		CRefCount::SafeRelease(pdrgpexpr);
	}

	return GPOS_OK;
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemo
//
//	@doc:
//		Test of building memo from different input expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemo()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// array of relation names
	CWStringConst rgscRel[] =
	{
		GPOS_WSZ_LIT("Rel1"),
		GPOS_WSZ_LIT("Rel2"),
		GPOS_WSZ_LIT("Rel3"),
		GPOS_WSZ_LIT("Rel4"),
		GPOS_WSZ_LIT("Rel5"),
	};

	// array of relation IDs
	ULONG rgulRel[] =
	{
		GPOPT_TEST_REL_OID1,
		GPOPT_TEST_REL_OID2,
		GPOPT_TEST_REL_OID3,
		GPOPT_TEST_REL_OID4,
		GPOPT_TEST_REL_OID5,
	};

	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp);
	const ULONG ulRels = GPOS_ARRAY_SIZE(rgscRel);
	for (ULONG ul = 0; ul < ulRels; ul++)
	{
		(void) pbs->ExchangeSet(ul);
	}

	GPOS_RESULT eres = EresOptimize(BuildMemoRecursive, rgscRel, rgulRel, ulRels, pbs);
	pbs->Release();

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_AppendStats
//
//	@doc:
//		Test of appending stats during optimization
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_AppendStats()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTraceFlag atf(EopttracePrintGroupProperties, true);

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

	CEngine eng(mp);

	// generate  join expression
	CExpression *pexpr = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp);

	// generate query context
	CQueryContext *pqc = CTestUtils::PqcGenerate(mp, pexpr);

	// Initialize engine
	eng.Init(pqc, NULL /*search_stage_array*/);

	CGroupExpression *pgexpr = NULL;
	{
		CGroupProxy gp(eng.PgroupRoot());
		pgexpr = gp.PgexprFirst();
	}

	// derive stats with empty requirements
	{
		CAutoTrace at(mp);

		CExpressionHandle exprhdl(mp);
		exprhdl.Attach(pgexpr);
		exprhdl.DeriveStats(mp, mp, NULL /*prprel*/, NULL /*stats_ctxt*/);
		at.Os() << std::endl << "MEMO AFTER FIRST STATS DERIVATION:" << std::endl;
	}
	eng.Trace();

	// create a non-empty set of output columns as requirements for stats derivation
	ULONG ulIndex = 0;
	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	CColRefSetIter crsi(*pexpr->DeriveOutputColumns());
	while (crsi.Advance() && ulIndex < 3)
	{
		CColRef *colref = crsi.Pcr();
		if (ulIndex == 1 || ulIndex == 2)
		{
			pcrs->Include(colref);
		}
		ulIndex++;
	}

	CReqdPropRelational *prprel = GPOS_NEW(mp) CReqdPropRelational(pcrs);

	// derive stats with non-empty requirements
	// missing stats should be appended to the already derived ones
	{
		CAutoTrace at(mp);

		CExpressionHandle exprhdl(mp);
		exprhdl.Attach(pgexpr);
		exprhdl.DeriveStats(mp, mp, prprel, NULL /*stats_ctxt*/);
		at.Os() << std::endl << "MEMO AFTER SECOND STATS DERIVATION:" << std::endl;
	}
	eng.Trace();

	pexpr->Release();
	prprel->Release();
	GPOS_DELETE(pqc);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoLargeJoins
//
//	@doc:
//		Test of building memo for a large number of joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoLargeJoins()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// array of relation names
	CWStringConst rgscRel[] =
	{
		GPOS_WSZ_LIT("Rel1"),
		GPOS_WSZ_LIT("Rel2"),
		GPOS_WSZ_LIT("Rel3"),
		GPOS_WSZ_LIT("Rel4"),
		GPOS_WSZ_LIT("Rel5"),
	};

	// array of relation IDs
	ULONG rgulRel[] =
	{
		GPOPT_TEST_REL_OID1,
		GPOPT_TEST_REL_OID2,
		GPOPT_TEST_REL_OID3,
		GPOPT_TEST_REL_OID4,
		GPOPT_TEST_REL_OID5,
	};

	// only optimize the last join expression
	CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp);
	const ULONG ulRels = GPOS_ARRAY_SIZE(rgscRel);
	(void) pbs->ExchangeSet(ulRels - 1);

	GPOS_RESULT eres = EresOptimize(BuildMemoRecursive, rgscRel, rgulRel, ulRels, pbs);
	pbs->Release();

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::BuildMemoRecursive
//
//	@doc:
//		Build memo recursively from a given input expression
//
//---------------------------------------------------------------------------
void
CEngineTest::BuildMemoRecursive
	(
	CMemoryPool *mp,
	CExpression *pexprInput,
	CSearchStageArray *search_stage_array
	)
{
	CQueryContext *pqc = CTestUtils::PqcGenerate(mp, pexprInput);

	CAutoTrace at(mp);
	IOstream &os(at.Os());

	os << std::endl << std::endl;
	os << "QUERY CONTEXT:" <<std::endl;
	(void) pqc->OsPrint(os);

	// enable space pruning
	CAutoTraceFlag atf(EopttraceEnableSpacePruning, true /*value*/);

	CEngine eng(mp);
	eng.Init(pqc, search_stage_array);
	eng.PrintRoot();
	GPOS_CHECK_ABORT;

	eng.RecursiveOptimize();
	GPOS_CHECK_ABORT;

	CExpression *pexprPlan = eng.PexprExtractPlan();
	GPOS_ASSERT(NULL != pexprPlan);
	
	(void) pexprPlan->PrppCompute(mp, pqc->Prpp());

	os << std::endl << std::endl;
	os << "OUTPUT PLAN:" <<std::endl;
	(void) pexprPlan->OsPrint(os);
	os << std::endl << std::endl;

	eng.PrintOptCtxts();
	pexprPlan->Release();
	GPOS_DELETE(pqc);

	GPOS_CHECK_ABORT;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresTestEngine
//
//	@doc:
//		Helper for testing engine using an array of expression generators
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresTestEngine
	(
	Pfpexpr rgpf[],
	ULONG size
	)
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	for (ULONG ul = m_ulTestCounter; ul < size; ul++)
	{
		// install opt context in TLS
		CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

		CExpression *pexpr = rgpf[ul](mp);
		BuildMemoRecursive(mp, pexpr, NULL /*search_stage_array*/);
		pexpr->Release();

		m_ulTestCounter++;
	}

	// reset counter
	m_ulTestCounter = 0;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoWithSubqueries
//
//	@doc:
//		Test of building memo for expressions with subqueries
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoWithSubqueries()
{
	typedef CExpression *(*Pfpexpr)(CMemoryPool*, BOOL);
	Pfpexpr rgpf[] =
		{
		CSubqueryTestUtils::PexprSelectWithAllAggSubquery,
		CSubqueryTestUtils::PexprSelectWithAggSubquery,
		CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison,
		CSubqueryTestUtils::PexprProjectWithAggSubquery,
		CSubqueryTestUtils::PexprSelectWithAnySubquery,
		CSubqueryTestUtils::PexprProjectWithAnySubquery,
		CSubqueryTestUtils::PexprSelectWithAllSubquery,
		CSubqueryTestUtils::PexprProjectWithAllSubquery,
		CSubqueryTestUtils::PexprSelectWithExistsSubquery,
		CSubqueryTestUtils::PexprProjectWithExistsSubquery,
		CSubqueryTestUtils::PexprSelectWithNotExistsSubquery,
		CSubqueryTestUtils::PexprProjectWithNotExistsSubquery,
		CSubqueryTestUtils::PexprSelectWithNestedCmpSubquery,
		CSubqueryTestUtils::PexprSelectWithCmpSubqueries,
		CSubqueryTestUtils::PexprSelectWithSubqueryConjuncts,
		CSubqueryTestUtils::PexprProjectWithSubqueries,
		CSubqueryTestUtils::PexprSelectWithAggSubqueryOverJoin,
		CSubqueryTestUtils::PexprSelectWithNestedSubquery,
		CSubqueryTestUtils::PexprSelectWithNestedAnySubqueries,
		CSubqueryTestUtils::PexprSelectWithNestedAllSubqueries,
		CSubqueryTestUtils::PexprSelectWith2LevelsCorrSubquery,
		CSubqueryTestUtils::PexprSubqueriesInNullTestContext,
		CSubqueryTestUtils::PexprSubqueriesInDifferentContexts,
		CSubqueryTestUtils::PexprSelectWithTrimmableExists,
		CSubqueryTestUtils::PexprSelectWithTrimmableNotExists,
		CSubqueryTestUtils::PexprSelectWithSubqueryDisjuncts,
		CSubqueryTestUtils::PexprUndecorrelatableAnySubquery,
		CSubqueryTestUtils::PexprUndecorrelatableAllSubquery,
		CSubqueryTestUtils::PexprUndecorrelatableExistsSubquery,
		CSubqueryTestUtils::PexprUndecorrelatableNotExistsSubquery,
		CSubqueryTestUtils::PexprUndecorrelatableScalarSubquery,
		CSubqueryTestUtils::PexprSelectWithAnySubqueryOverWindow,
		CSubqueryTestUtils::PexprSelectWithAllSubqueryOverWindow,
		};

		BOOL fCorrelated = true;

		// we get two expressions using each generator
		const ULONG size = 2 * GPOS_ARRAY_SIZE(rgpf);
		for (ULONG ul = m_ulTestCounterSubq; ul < size; ul++)
		{
			CAutoMemoryPool amp;
			CMemoryPool *mp = amp.Pmp();

			// setup a file-based provider
			CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
			pmdp->AddRef();
			CMDAccessor mda(mp, CMDCache::Pcache());
			mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

			{
				// install opt context in TLS
				CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

				ULONG ulIndex = ul / 2;
				CExpression *pexpr = rgpf[ulIndex](mp, fCorrelated);
				BuildMemoRecursive(mp, pexpr, NULL /*search_stage_array*/);
				pexpr->Release();
			}

			fCorrelated = !fCorrelated;
			m_ulTestCounterSubq++;
		}

		m_ulTestCounterSubq = 0;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoWithTVF
//
//	@doc:
//		Test of building memo for expressions with table-valued functions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoWithTVF()
{
	Pfpexpr rgpf[] =
		{
		CTestUtils::PexprLogicalTVFTwoArgs,
		CTestUtils::PexprLogicalTVFThreeArgs,
		CTestUtils::PexprLogicalTVFNoArgs,
		};

	return EresTestEngine(rgpf, GPOS_ARRAY_SIZE(rgpf));
}

//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoWithGrouping
//
//	@doc:
//		Test of building memo for expressions with grouping
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoWithGrouping()
{
	Pfpexpr rgpf[] =
		{
		CTestUtils::PexprLogicalGbAgg,
		CTestUtils::PexprLogicalGbAggOverJoin,
		CTestUtils::PexprLogicalGbAggWithSum,
		CTestUtils::PexprLogicalNAryJoin
		};

	return EresTestEngine(rgpf, GPOS_ARRAY_SIZE(rgpf));
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoWithPartitioning
//
//	@doc:
//		Test of building memo for expressions with partitioning
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoWithPartitioning()
{
	Pfpexpr rgpf[] =
		{
		CTestUtils::PexprLogicalDynamicGet,
		CTestUtils::PexprLogicalSelectWithEqPredicateOverDynamicGet,
		CTestUtils::PexprLogicalSelectWithLTPredicateOverDynamicGet,
//		CTestUtils::PexprJoinPartitionedOuter<CLogicalInnerJoin>,
//		CTestUtils::Pexpr3WayJoinPartitioned,
//		CTestUtils::Pexpr4WayJoinPartitioned
		};

	return EresTestEngine(rgpf, GPOS_ARRAY_SIZE(rgpf));
}


//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoWithWindowing
//
//	@doc:
//		Test of building memo for expressions with windowing operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoWithWindowing()
{
	Pfpexpr rgpf[] =
		{
		CTestUtils::PexprOneWindowFunction,
		CTestUtils::PexprTwoWindowFunctions,
		};

	return EresTestEngine(rgpf, GPOS_ARRAY_SIZE(rgpf));
}

//---------------------------------------------------------------------------
//	@function:
//		CEngineTest::EresUnittest_BuildMemoWithCTE
//
//	@doc:
//		Test of building memo for expressions with CTEs
//
//---------------------------------------------------------------------------
GPOS_RESULT
CEngineTest::EresUnittest_BuildMemoWithCTE()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

	CExpression *pexprCTE = CTestUtils::PexprCTETree(mp);
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);

	CColRefSet *pcrsLeft = pexprCTE->DeriveOutputColumns();
	CColRef *pcrLeft =  pcrsLeft->PcrAny();

	CColRefSet *pcrsRight = pexprGet->DeriveOutputColumns();
	CColRef *pcrRight =  pcrsRight->PcrAny();

	CExpression *pexprScalar = CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight);

	CExpression *pexpr = GPOS_NEW(mp) CExpression
									(
									mp,
									GPOS_NEW(mp) CLogicalInnerJoin(mp),
									pexprCTE,
									pexprGet,
									pexprScalar
									);

	BuildMemoRecursive(mp, pexpr, NULL /*search_stage_array*/);
	pexpr->Release();

	return GPOS_OK;
}

#endif // GPOS_DEBUG

// EOF

