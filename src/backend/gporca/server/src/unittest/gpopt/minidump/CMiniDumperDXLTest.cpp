//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperDXLTest.cpp
//
//	@doc:
//		Test for DXL-based minidumps
//---------------------------------------------------------------------------
#include "unittest/gpopt/minidump/CMiniDumperDXLTest.h"

#include <fstream>

#include "gpos/io/COstreamFile.h"
#include "gpos/io/COstreamString.h"
#include "gpos/task/CAutoTraceFlag.h"

#include "gpopt/base/CQueryContext.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/minidump/CDXLMinidump.h"
#include "gpopt/minidump/CMiniDumperDXL.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/minidump/CSerializableMDAccessor.h"
#include "gpopt/minidump/CSerializableOptimizerConfig.h"
#include "gpopt/minidump/CSerializablePlan.h"
#include "gpopt/minidump/CSerializableQuery.h"
#include "gpopt/minidump/CSerializableStackTrace.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"
#include "naucrates/base/CQueryToDXLResult.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"

static const CHAR *szQueryFile = "../data/dxl/minidump/Query.xml";

//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXLTest::EresUnittest
//
//	@doc:
//		Unittest for DXL minidumps
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperDXLTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CMiniDumperDXLTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CMiniDumperDXLTest::EresUnittest_Load),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}



//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXLTest::EresUnittest_Basic
//
//	@doc:
//		Test minidumps in case of an exception
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperDXLTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic minidumpstr(mp);
	COstreamString oss(&minidumpstr);
	CMiniDumperDXL mdrs(mp);
	mdrs.Init(&oss);

	CHAR file_name[GPOS_FILE_NAME_BUF_SIZE];

	GPOS_TRY
	{
		CSerializableStackTrace serStackTrace;

		// read the dxl document
		CHAR *szQueryDXL = CDXLUtils::Read(mp, szQueryFile);

		// parse the DXL query tree from the given DXL document
		CQueryToDXLResult *ptroutput =
			CDXLUtils::ParseQueryToQueryDXLTree(mp, szQueryDXL, NULL);
		GPOS_CHECK_ABORT;

		CSerializableQuery serQuery(mp, ptroutput->CreateDXLNode(),
									ptroutput->GetOutputColumnsDXLArray(),
									ptroutput->GetCTEProducerDXLArray());

		// setup a file-based provider
		CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
		pmdp->AddRef();

		// we need to use an auto pointer for the cache here to ensure
		// deleting memory of cached objects when we throw
		CAutoP<CMDAccessor::MDCache> apcache;
		apcache = CCacheFactory::CreateCache<gpopt::IMDCacheObject *,
											 gpopt::CMDKey *>(
			true,  // fUnique
			0 /* unlimited cache quota */, CMDKey::UlHashMDKey,
			CMDKey::FEqualMDKey);

		CMDAccessor::MDCache *pcache = apcache.Value();

		CMDAccessor mda(mp, pcache, CTestUtils::m_sysidDefault, pmdp);

		CSerializableMDAccessor serMDA(&mda);

		CAutoTraceFlag atfPrintQuery(EopttracePrintQuery, true);
		CAutoTraceFlag atfPrintPlan(EopttracePrintPlan, true);
		CAutoTraceFlag atfTest(EtraceTest, true);

		COptimizerConfig *optimizer_config = GPOS_NEW(mp) COptimizerConfig(
			CEnumeratorConfig::GetEnumeratorCfg(mp, 0 /*plan_id*/),
			CStatisticsConfig::PstatsconfDefault(mp),
			CCTEConfig::PcteconfDefault(mp), ICostModel::PcmDefault(mp),
			CHint::PhintDefault(mp), CWindowOids::GetWindowOids(mp));

		// setup opt ctx
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		// translate DXL Tree -> Expr Tree
		CTranslatorDXLToExpr *pdxltr =
			GPOS_NEW(mp) CTranslatorDXLToExpr(mp, &mda);
		CExpression *pexprTranslated = pdxltr->PexprTranslateQuery(
			ptroutput->CreateDXLNode(), ptroutput->GetOutputColumnsDXLArray(),
			ptroutput->GetCTEProducerDXLArray());

		gpdxl::ULongPtrArray *pdrgul = pdxltr->PdrgpulOutputColRefs();
		gpmd::CMDNameArray *pdrgpmdname = pdxltr->Pdrgpmdname();

		ULONG ulSegments = GPOPT_TEST_SEGMENTS;
		CQueryContext *pqc = CQueryContext::PqcGenerate(
			mp, pexprTranslated, pdrgul, pdrgpmdname, true /*fDeriveStats*/);

		// optimize logical expression tree into physical expression tree.

		CEngine eng(mp);

		CSerializableOptimizerConfig serOptConfig(mp, optimizer_config);

		eng.Init(pqc, NULL /*search_stage_array*/);
		eng.Optimize();

		CExpression *pexprPlan = eng.PexprExtractPlan();
		(void) pexprPlan->PrppCompute(mp, pqc->Prpp());

		// translate plan into DXL
		IntPtrArray *pdrgpiSegments = GPOS_NEW(mp) IntPtrArray(mp);


		GPOS_ASSERT(0 < ulSegments);

		for (ULONG ul = 0; ul < ulSegments; ul++)
		{
			pdrgpiSegments->Append(GPOS_NEW(mp) INT(ul));
		}

		CTranslatorExprToDXL ptrexprtodxl(mp, &mda, pdrgpiSegments);
		CDXLNode *pdxlnPlan = ptrexprtodxl.PdxlnTranslate(
			pexprPlan, pqc->PdrgPcr(), pqc->Pdrgpmdname());
		GPOS_ASSERT(NULL != pdxlnPlan);

		CSerializablePlan serPlan(
			mp, pdxlnPlan, optimizer_config->GetEnumeratorCfg()->GetPlanId(),
			optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize());
		GPOS_CHECK_ABORT;

		// simulate an exception
		GPOS_OOM_CHECK(NULL);
	}
	GPOS_CATCH_EX(ex)
	{
		// unless we're simulating faults, the exception must be OOM
		GPOS_ASSERT_IMP(!GPOS_FTRACE(EtraceSimulateAbort) &&
							!GPOS_FTRACE(EtraceSimulateIOError),
						CException::ExmaSystem == ex.Major() &&
							CException::ExmiOOM == ex.Minor());

		mdrs.Finalize();

		GPOS_RESET_EX;

		CWStringDynamic str(mp);
		COstreamString oss(&str);
		oss << std::endl << "Minidump" << std::endl;
		oss << minidumpstr.GetBuffer();
		oss << std::endl;

		// dump the same to a temp file
		ULONG ulSessionId = 1;
		ULONG ulCommandId = 1;

		CMinidumperUtils::GenerateMinidumpFileName(
			file_name, GPOS_FILE_NAME_BUF_SIZE, ulSessionId, ulCommandId,
			NULL /*szMinidumpFileName*/);

		std::wofstream osMinidump(file_name);
		osMinidump << minidumpstr.GetBuffer();

		oss << "Minidump file: " << file_name << std::endl;

		GPOS_TRACE(str.GetBuffer());
	}
	GPOS_CATCH_END;

	// TODO:  - Feb 11, 2013; enable after fixing problems with serializing
	// XML special characters (OPT-2996)
	//	// try to load minidump file
	//	CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(mp, file_name);
	//	GPOS_ASSERT(NULL != pdxlmd);
	//	delete pdxlmd;


	// delete temp file
	ioutils::Unlink(file_name);

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXLTest::EresUnittest_Load
//
//	@doc:
//		Load a minidump file
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperDXLTest::EresUnittest_Load()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	const CHAR *rgszMinidumps[] = {
		"../data/dxl/minidump/Minidump.xml",
	};
	ULONG ulTestCounter = 0;

	GPOS_RESULT eres = CTestUtils::EresRunMinidumps(mp, rgszMinidumps,
													1,	// ulTests
													&ulTestCounter,
													1,		// ulSessionId
													1,		// ulCmdId
													false,	// fMatchPlans
													false	// fTestSpacePruning
	);
	return eres;
}
// EOF
