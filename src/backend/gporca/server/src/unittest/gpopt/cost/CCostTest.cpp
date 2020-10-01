//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CCostTest.cpp
//
//	@doc:
//		Tests for basic operations on cost
//---------------------------------------------------------------------------
#include "gpos/error/CAutoTrace.h"
#include "gpos/task/CAutoTraceFlag.h"

#include "gpopt/cost/CCost.h"
#include "gpopt/cost/ICostModelParams.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/operators/CLogicalInnerJoin.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/parser/CParseHandlerDXL.h"

#include "unittest/base.h"
#include "unittest/gpopt/cost/CCostTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "gpdbcost/CCostModelGPDB.h"
#include "gpdbcost/CCostModelGPDBLegacy.h"

//---------------------------------------------------------------------------
//	@function:
//		CCostTest::EresUnittest
//
//	@doc:
//		Driver for unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CCostTest::EresUnittest_Bool),
		GPOS_UNITTEST_FUNC(CCostTest::EresUnittest_Arithmetic),
		GPOS_UNITTEST_FUNC(CCostTest::EresUnittest_Params),
		GPOS_UNITTEST_FUNC(CCostTest::EresUnittest_Parsing),
		GPOS_UNITTEST_FUNC(EresUnittest_SetParams),

		// TODO: : re-enable test after resolving exception throwing problem on OSX
		// GPOS_UNITTEST_FUNC_THROW(CCostTest::EresUnittest_ParsingWithException, gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CCostTest::EresUnittest_Arithmetic
//
//	@doc:
//		Test arithmetic operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest_Arithmetic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CCost cost1(2.5);
	CCost cost2(3.0);
	CCost cost3(5.49);
	CCost cost4(5.51);
	CCost cost5(7.49);
	CCost cost6(7.51);

	CCost costAdd(cost1 + cost2);
	CCost costMultiply(cost1 * cost2);

	GPOS_ASSERT(costAdd > cost3);
	GPOS_ASSERT(costAdd < cost4);

	GPOS_ASSERT(costMultiply > cost5);
	GPOS_ASSERT(costMultiply < cost6);

	CAutoTrace at(mp);
	IOstream &os(at.Os());

	os << "Arithmetic operations: " << std::endl
	   << cost1 << " + " << cost2 << " = " << costAdd << std::endl
	   << cost1 << " * " << cost2 << " = " << costMultiply << std::endl;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostTest::EresUnittest_Bool
//
//	@doc:
//		Test comparison operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest_Bool()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CCost cost1(2.5);
	CCost cost2(3.5);

	GPOS_ASSERT(cost1 < cost2);
	GPOS_ASSERT(cost2 > cost1);

	CAutoTrace at(mp);
	IOstream &os(at.Os());

	os << "Boolean operations: " << std::endl
	   << cost1 << " < " << cost2 << " = " << (cost1 < cost2) << std::endl
	   << cost2 << " > " << cost1 << " = " << (cost2 > cost1) << std::endl;

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CCostTest::TestParams
//
//	@doc:
//		Test cost model parameters
//
//---------------------------------------------------------------------------
void
CCostTest::TestParams(CMemoryPool *mp, BOOL fCalibrated)
{
	CAutoTrace at(mp);
	IOstream &os(at.Os());

	ICostModelParams *pcp = NULL;
	CDouble dSeqIOBandwidth(0.0);
	CDouble dRandomIOBandwidth(0.0);
	CDouble dTupProcBandwidth(0.0);
	CDouble dNetBandwidth(0.0);
	CDouble dSegments(0.0);
	CDouble dNLJFactor(0.0);
	CDouble dHashFactor(0.0);
	CDouble dDefaultCost(0.0);

	if (fCalibrated)
	{
		pcp = ((CCostModelGPDB *) COptCtxt::PoctxtFromTLS()->GetCostModel())
				  ->GetCostModelParams();

		dSeqIOBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDB::EcpSeqIOBandwidth)->Get();
		dRandomIOBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDB::EcpRandomIOBandwidth)->Get();
		dTupProcBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDB::EcpTupProcBandwidth)->Get();
		dNetBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDB::EcpNetBandwidth)->Get();
		dSegments = pcp->PcpLookup(CCostModelParamsGPDB::EcpSegments)->Get();
		dNLJFactor = pcp->PcpLookup(CCostModelParamsGPDB::EcpNLJFactor)->Get();
		dHashFactor =
			pcp->PcpLookup(CCostModelParamsGPDB::EcpHashFactor)->Get();
		dDefaultCost =
			pcp->PcpLookup(CCostModelParamsGPDB::EcpDefaultCost)->Get();
	}
	else
	{
		pcp =
			((CCostModelGPDBLegacy *) COptCtxt::PoctxtFromTLS()->GetCostModel())
				->GetCostModelParams();

		dSeqIOBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpSeqIOBandwidth)
				->Get();
		dRandomIOBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpRandomIOBandwidth)
				->Get();
		dTupProcBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpTupProcBandwidth)
				->Get();
		dNetBandwidth =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpNetBandwidth)->Get();
		dSegments =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpSegments)->Get();
		dNLJFactor =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpNLJFactor)->Get();
		dHashFactor =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpHashFactor)->Get();
		dDefaultCost =
			pcp->PcpLookup(CCostModelParamsGPDBLegacy::EcpDefaultCost)->Get();
	}

	os << std::endl << "Lookup cost model params by id: " << std::endl;
	os << "Seq I/O bandwidth: " << dSeqIOBandwidth << std::endl;
	os << "Random I/O bandwidth: " << dRandomIOBandwidth << std::endl;
	os << "Tuple proc bandwidth: " << dTupProcBandwidth << std::endl;
	os << "Network bandwidth: " << dNetBandwidth << std::endl;
	os << "Segments: " << dSegments << std::endl;
	os << "NLJ Factor: " << dNLJFactor << std::endl;
	os << "Hash Factor: " << dHashFactor << std::endl;
	os << "Default Cost: " << dDefaultCost << std::endl;

	CDouble dSeqIOBandwidth1 = pcp->PcpLookup("SeqIOBandwidth")->Get();
	CDouble dRandomIOBandwidth1 = pcp->PcpLookup("RandomIOBandwidth")->Get();
	CDouble dTupProcBandwidth1 = pcp->PcpLookup("TupProcBandwidth")->Get();
	CDouble dNetBandwidth1 = pcp->PcpLookup("NetworkBandwidth")->Get();
	CDouble dSegments1 = pcp->PcpLookup("Segments")->Get();
	CDouble dNLJFactor1 = pcp->PcpLookup("NLJFactor")->Get();
	CDouble dHashFactor1 = pcp->PcpLookup("HashFactor")->Get();
	CDouble dDefaultCost1 = pcp->PcpLookup("DefaultCost")->Get();

	os << std::endl << "Lookup cost model params by name: " << std::endl;
	os << "Seq I/O bandwidth: " << dSeqIOBandwidth1 << std::endl;
	os << "Random I/O bandwidth: " << dRandomIOBandwidth1 << std::endl;
	os << "Tuple proc bandwidth: " << dTupProcBandwidth1 << std::endl;
	os << "Network bandwidth: " << dNetBandwidth1 << std::endl;
	os << "Segments: " << dSegments1 << std::endl;
	os << "NLJ Factor: " << dNLJFactor1 << std::endl;
	os << "Hash Factor: " << dHashFactor1 << std::endl;
	os << "Default Cost: " << dDefaultCost1 << std::endl;

	GPOS_ASSERT(dSeqIOBandwidth == dSeqIOBandwidth1);
	GPOS_ASSERT(dRandomIOBandwidth == dRandomIOBandwidth1);
	GPOS_ASSERT(dTupProcBandwidth == dTupProcBandwidth1);
	GPOS_ASSERT(dNetBandwidth == dNetBandwidth1);
	GPOS_ASSERT(dSegments == dSegments1);
	GPOS_ASSERT(dNLJFactor == dNLJFactor1);
	GPOS_ASSERT(dHashFactor == dHashFactor1);
	GPOS_ASSERT(dDefaultCost == dDefaultCost1);
}


//---------------------------------------------------------------------------
//	@function:
//		CCostTest::EresUnittest_Params
//
//	@doc:
//		Cost model parameters
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest_Params()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		TestParams(mp, false /*fCalibrated*/);
	}

	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 GPOS_NEW(mp) CCostModelGPDB(mp, GPOPT_TEST_SEGMENTS));

		TestParams(mp, true /*fCalibrated*/);
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostTest::EresUnittest_Parsing
//
//	@doc:
//		Test parsing cost params from external file
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest_Parsing()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();
	CParseHandlerDXL *pphDXL = CDXLUtils::GetParseHandlerForDXLFile(
		mp, "../data/dxl/cost/cost0.xml", NULL);
	ICostModelParams *pcp = pphDXL->GetCostModelParams();

	{
		CAutoTrace at(mp);
		at.Os() << " Parsed cost params: " << std::endl;
		pcp->OsPrint(at.Os());
	}
	GPOS_DELETE(pphDXL);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CSearchStrategyTest::EresUnittest_ParsingWithException
//
//	@doc:
//		Test exception handling when parsing cost params
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest_ParsingWithException()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();
	CParseHandlerDXL *pphDXL = CDXLUtils::GetParseHandlerForDXLFile(
		mp, "../data/dxl/cost/wrong-cost.xml", NULL);
	GPOS_DELETE(pphDXL);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostTest::EresUnittest_SetParams
//
//	@doc:
//		Test of setting cost model params
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCostTest::EresUnittest_SetParams()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	ICostModel *pcm = GPOS_NEW(mp) CCostModelGPDB(mp, GPOPT_TEST_SEGMENTS);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */ pcm);

	// generate in-equality join expression
	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp);
	const CColRef *pcrOuter = pexprOuter->DeriveOutputColumns()->PcrAny();
	CExpression *pexprInner = CTestUtils::PexprLogicalGet(mp);
	const CColRef *pcrInner = pexprInner->DeriveOutputColumns()->PcrAny();
	CExpression *pexprPred =
		CUtils::PexprScalarCmp(mp, pcrOuter, pcrInner, IMDType::EcmptNEq);
	CExpression *pexpr = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(
		mp, pexprOuter, pexprInner, pexprPred);

	// optimize in-equality join based on default cost model params
	CExpression *pexprPlan1 = NULL;
	{
		CEngine eng(mp);

		// generate query context
		CQueryContext *pqc = CTestUtils::PqcGenerate(mp, pexpr);

		// Initialize engine
		eng.Init(pqc, NULL /*search_stage_array*/);

		// optimize query
		eng.Optimize();

		// extract plan
		pexprPlan1 = eng.PexprExtractPlan();
		GPOS_ASSERT(NULL != pexprPlan1);

		GPOS_DELETE(pqc);
	}

	// change NLJ cost factor
	ICostModelParams::SCostParam *pcp = pcm->GetCostModelParams()->PcpLookup(
		CCostModelParamsGPDB::EcpNLJFactor);
	CDouble dNLJFactor = CDouble(2.0);
	CDouble dVal = pcp->Get() * dNLJFactor;
	pcm->GetCostModelParams()->SetParam(pcp->Id(), dVal, dVal - 0.5,
										dVal + 0.5);

	// optimize again after updating NLJ cost factor
	CExpression *pexprPlan2 = NULL;
	{
		CEngine eng(mp);

		// generate query context
		CQueryContext *pqc = CTestUtils::PqcGenerate(mp, pexpr);

		// Initialize engine
		eng.Init(pqc, NULL /*search_stage_array*/);

		// optimize query
		eng.Optimize();

		// extract plan
		pexprPlan2 = eng.PexprExtractPlan();
		GPOS_ASSERT(NULL != pexprPlan2);

		GPOS_DELETE(pqc);
	}

	{
		CAutoTrace at(mp);
		at.Os() << "\nPLAN1: \n" << *pexprPlan1;
		at.Os() << "\nNLJ Cost1: " << (*pexprPlan1)[0]->Cost();
		at.Os() << "\n\nPLAN2: \n" << *pexprPlan2;
		at.Os() << "\nNLJ Cost2: " << (*pexprPlan2)[0]->Cost();
	}
	GPOS_ASSERT(
		(*pexprPlan2)[0]->Cost() >= (*pexprPlan1)[0]->Cost() * dNLJFactor &&
		"expected NLJ cost in PLAN2 to be larger than NLJ cost in PLAN1");

	// clean up
	pexpr->Release();
	pexprPlan1->Release();
	pexprPlan2->Release();

	return GPOS_OK;
}

// EOF
