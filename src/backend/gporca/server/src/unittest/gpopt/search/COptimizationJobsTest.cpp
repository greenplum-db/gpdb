//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		COptimizationJobsTest.cpp
//
//	@doc:
//		Test for optimization jobs
//---------------------------------------------------------------------------
#include "gpos/error/CAutoTrace.h"

#include "gpopt/engine/CEngine.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/search/CGroupProxy.h"
#include "gpopt/search/CJobFactory.h"
#include "gpopt/search/CJobGroupOptimization.h"
#include "gpopt/search/CJobGroupExpressionOptimization.h"
#include "gpopt/search/CJobGroupExploration.h"
#include "gpopt/search/CJobGroupExpressionExploration.h"
#include "gpopt/search/CJobGroupImplementation.h"
#include "gpopt/search/CJobGroupExpressionImplementation.h"
#include "gpopt/search/CJobTransformation.h"
#include "gpopt/search/CScheduler.h"
#include "gpopt/search/CSchedulerContext.h"
#include "gpopt/xforms/CXformFactory.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/search/COptimizationJobsTest.h"


//---------------------------------------------------------------------------
//	@function:
//		COptimizationJobsTest::EresUnittest
//
//	@doc:
//		Unittest for optimization jobs
//
//---------------------------------------------------------------------------
GPOS_RESULT
COptimizationJobsTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(COptimizationJobsTest::EresUnittest_StateMachine),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationJobsTest::EresUnittest_StateMachine
//
//	@doc:
//		Test of optimization jobs stat machine
//
//---------------------------------------------------------------------------
GPOS_RESULT
COptimizationJobsTest::EresUnittest_StateMachine()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	{
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

		CGroup *pgroup = eng.PgroupRoot();
		pqc->Prpp()->AddRef();
		COptimizationContext *poc = GPOS_NEW(mp) COptimizationContext
							(
							mp,
							pgroup,
							pqc->Prpp(),
							GPOS_NEW(mp) CReqdPropRelational(GPOS_NEW(mp) CColRefSet(mp)),
							GPOS_NEW(mp) IStatisticsArray(mp),
							0 // ulSearchStageIndex
							);

		// optimize query
		CJobFactory jf(mp, 1000 /*ulJobs*/);
		CScheduler sched(mp, 1000 /*ulJobs*/);
		CSchedulerContext sc;
		sc.Init(mp, &jf, &sched, &eng);
		CJob *pj = jf.PjCreate(CJob::EjtGroupOptimization);
		CJobGroupOptimization *pjgo = CJobGroupOptimization::PjConvert(pj);
		pjgo->Init(pgroup, NULL /*pgexprOrigin*/, poc);
		sched.Add(pjgo, NULL /*pjParent*/);
		CScheduler::Run(&sc);

#ifdef GPOS_DEBUG
		{
			CAutoTrace at(mp);
			at.Os() << std::endl << "GROUP OPTIMIZATION:" << std::endl;
			(void) pjgo->OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) pjgo->OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("GroupOptimizationJob"));

			CJobGroupOptimization::EState *pestate = NULL;
			ULONG size = 0;
			pjgo->Unreachable(mp, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupOptimization::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		CGroupExpression *pgexprLogical = NULL;
		CGroupExpression *pgexprPhysical = NULL;
		{
			CGroupProxy gp(pgroup);
			pgexprLogical = gp.PgexprNextLogical(NULL /*pgexpr*/);
			GPOS_ASSERT(NULL != pgexprLogical);

			pgexprPhysical = gp.PgexprSkipLogical(NULL /*pgexpr*/);
			GPOS_ASSERT(NULL != pgexprPhysical);
		}

		{
			CAutoTrace at(mp);
			CJobGroupImplementation jgi;
			jgi.Init(pgroup);
			at.Os() << std::endl << "GROUP IMPLEMENTATION:" << std::endl;
			(void) jgi.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgi.OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("GroupImplementationJob"));

			CJobGroupImplementation::EState *pestate = NULL;
			ULONG size = 0;
			jgi.Unreachable(mp, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupImplementation::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(mp);
			CJobGroupExploration jge;
			jge.Init(pgroup);
			at.Os() << std::endl << "GROUP EXPLORATION:" << std::endl;
			(void) jge.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jge.OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("GroupExplorationJob"));

			CJobGroupExploration::EState *pestate = NULL;
			ULONG size = 0;
			jge.Unreachable(mp, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExploration::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(mp);
			CJobGroupExpressionOptimization jgeo;
			jgeo.Init(pgexprPhysical, poc, 0 /*ulOptReq*/);
			at.Os() << std::endl << "GROUP EXPRESSION OPTIMIZATION:" << std::endl;
			(void) jgeo.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgeo.OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("GroupExpressionOptimizationJob"));

			CJobGroupExpressionOptimization::EState *pestate = NULL;
			ULONG size = 0;
			jgeo.Unreachable(mp, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExpressionOptimization::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(mp);
			CJobGroupExpressionImplementation jgei;
			jgei.Init(pgexprLogical);
			at.Os() << std::endl << "GROUP EXPRESSION IMPLEMENTATION:" << std::endl;
			(void) jgei.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgei.OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("GroupExpressionImplementationJob"));

			CJobGroupExpressionImplementation::EState *pestate = NULL;
			ULONG size = 0;
			jgei.Unreachable(mp, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExpressionImplementation::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(mp);
			CJobGroupExpressionExploration jgee;
			jgee.Init(pgexprLogical);
			at.Os() << std::endl << "GROUP EXPRESSION EXPLORATION:" << std::endl;
			(void) jgee.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgee.OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("GroupExpressionExplorationJob"));

			CJobGroupExpressionExploration::EState *pestate = NULL;
			ULONG size = 0;
			jgee.Unreachable(mp, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExpressionExploration::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(mp);
			CXformSet *xform_set = CLogical::PopConvert(pgexprLogical->Pop())->PxfsCandidates(mp);

			CXformSetIter xsi(*(xform_set));
			while (xsi.Advance())
			{
				CXform *pxform = CXformFactory::Pxff()->Pxf(xsi.TBit());
				CJobTransformation jt;
				jt.Init(pgexprLogical, pxform);
				at.Os() << std::endl << "GROUP EXPRESSION TRANSFORMATION:" << std::endl;
				(void) jt.OsPrint(at.Os());

				// dumping state graph
				at.Os() << std::endl;
				(void) jt.OsDiagramToGraphviz(mp, at.Os(), GPOS_WSZ_LIT("TransformationJob"));

				CJobTransformation::EState *pestate = NULL;
				ULONG size = 0;
				jt.Unreachable(mp, &pestate, &size);
				GPOS_ASSERT(size == 1 && pestate[0] == CJobTransformation::estInitialized);

				GPOS_DELETE_ARRAY(pestate);
			}

			xform_set->Release();
		}
#endif // GPOS_DEBUG

		pexpr->Release();
		poc->Release();
		GPOS_DELETE(pqc);
	}

	return GPOS_OK;
}


// EOF
