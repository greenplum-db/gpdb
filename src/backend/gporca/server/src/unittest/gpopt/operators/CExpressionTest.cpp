//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CExpressionTest.cpp
//
//	@doc:
//		Test for CExpression
//---------------------------------------------------------------------------
#include "gpos/error/CAutoTrace.h"
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/base/CDatumInt8GPDB.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDScalarOp.h"

#include "gpopt/base/CCTEReq.h"
#include "gpopt/base/CDrvdPropCtxtPlan.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/mdcache/CAutoMDAccessor.h"
#include "gpopt/operators/ops.h"
#include "gpopt/operators/CLogicalDynamicGetBase.h"

#include "unittest/base.h"
#include "unittest/gpopt/operators/CExpressionTest.h"
#include "unittest/gpopt/CTestUtils.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest
//
//	@doc:
//		Create required properties which are empty, except for required column set, given by 'pcrs'.
// 		Caller takes ownership of returned pointer.
//
//---------------------------------------------------------------------------
CReqdPropPlan *
CExpressionTest::PrppCreateRequiredProperties(CMemoryPool *mp, CColRefSet *pcrs)
{
	COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);
	CDistributionSpec *pds = GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtNone, CRewindabilitySpec::EmhtNoMotion);
	CEnfdOrder *peo = GPOS_NEW(mp) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);
	CEnfdDistribution *ped = GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmSatisfy);
	CEnfdRewindability *per = GPOS_NEW(mp) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);
	CCTEReq *pcter = GPOS_NEW(mp) CCTEReq(mp);
	return GPOS_NEW(mp) CReqdPropPlan(pcrs, peo, ped, per, pcter);
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest
//
//	@doc:
//		Creates a logical GroupBy and a Get as its child. The columns in the
//		Get follow the format wszColNameFormat.
//		Caller takes ownership of returned pointer.
//
//---------------------------------------------------------------------------
CExpression *
CExpressionTest::PexprCreateGbyWithColumnFormat(CMemoryPool *mp, const WCHAR *wszColNameFormat)
{
	CWStringConst strRelName(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *rel_mdid = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdesc = CTestUtils::PtabdescPlainWithColNameFormat(
			mp, 3, rel_mdid, wszColNameFormat, CName(&strRelName), false);
	CWStringConst strRelAlias(GPOS_WSZ_LIT("Rel1"));
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp, ptabdesc, &strRelAlias);
	return CTestUtils::PexprLogicalGbAggWithInput(mp, pexprGet);
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_SimpleOps),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_Union),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_BitmapGet),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_Const),
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_ComparisonTypes),
#endif // GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_FValidPlan),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_FValidPlan_InvalidOrder),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_FValidPlan_InvalidDistribution),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_FValidPlan_InvalidRewindability),
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_FValidPlan_InvalidCTEs),

#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CExpressionTest::EresUnittest_FValidPlanError),
#endif // GPOS_DEBUG
		GPOS_UNITTEST_FUNC(EresUnittest_ReqdCols),
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC_ASSERT(CExpressionTest::EresUnittest_InvalidSetOp),
#endif // GPOS_DEBUG
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_SimpleOps
//
//	@doc:
//		Basic tree builder test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_SimpleOps()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	typedef CExpression *(*Pfpexpr)(CMemoryPool*);

	Pfpexpr rgpf[] =
		{
		CTestUtils::PexprLogicalGet,
		CTestUtils::PexprLogicalExternalGet,
		CTestUtils::PexprLogicalGetPartitioned,
		CTestUtils::PexprLogicalSelect,
		CTestUtils::PexprLogicalSelectCmpToConst,
		CTestUtils::PexprLogicalSelectArrayCmp,
		CTestUtils::PexprLogicalSelectPartitioned,
		CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftSemiJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftAntiSemiJoin>,
		CTestUtils::PexprLogicalJoin<CLogicalLeftAntiSemiJoinNotIn>,
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
		CTestUtils::PexprLogicalAssert,
		PexprComplexJoinTree
		};

#ifdef GPOS_DEBUG
	// misestimation risk for the roots of the plans in rgpf
	ULONG rgulRisk[] =
		{ 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 2, 1, 1, 1, 1, 1, 1, 6 };
#endif // GPOS_DEBUG

	for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgpf); i++)
	{
		// install opt context in TLS
		CAutoOptCtxt aoc
						(
						mp,
						&mda,
						NULL,  /* pceeval */
						CTestUtils::GetCostModel(mp)
						);

		// generate simple expression
		CExpression *pexpr = rgpf[i](mp);

		CLogicalGet *popGet = dynamic_cast<CLogicalGet *>(pexpr->Pop());
		CLogicalDynamicGetBase *popDynGet = dynamic_cast<CLogicalDynamicGetBase *>(pexpr->Pop());
		CColRefArray *colrefs = NULL;

		if (NULL != popGet)
		{
			colrefs = popGet->PdrgpcrOutput();
		}
		else if (NULL != popDynGet)
		{
			colrefs = popDynGet->PdrgpcrOutput();
		}

		if (NULL != colrefs)
		{
			for (ULONG ul = 0; ul < colrefs->Size(); ul++)
			{
				(*colrefs)[ul]->MarkAsUsed();
			}
		}

		// self-match
		GPOS_ASSERT(pexpr->FMatchDebug(pexpr));

		// debug print
		CWStringDynamic str(mp, GPOS_WSZ_LIT("\n"));
		COstreamString oss(&str);

		oss << "EXPR:" << std::endl << *pexpr << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

#ifdef GPOS_DEBUG

		oss << std::endl << "DERIVED PROPS:" << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();
		pexpr->DbgPrint();

		// copy expression
		CColRef *pcrOld = pexpr->DeriveOutputColumns()->PcrAny();
		CColRef *new_colref = COptCtxt::PoctxtFromTLS()->Pcf()->PcrCreate(pcrOld);
		UlongToColRefMap *colref_mapping = GPOS_NEW(mp) UlongToColRefMap(mp);

		BOOL result = colref_mapping->Insert(GPOS_NEW(mp) ULONG(pcrOld->Id()), new_colref);
		GPOS_ASSERT(result);
		CExpression *pexprCopy = pexpr->PexprCopyWithRemappedColumns(mp, colref_mapping, true /*must_exist*/);
		colref_mapping->Release();
		oss << std::endl << "COPIED EXPRESSION (AFTER MAPPING " << *pcrOld << " TO " << *new_colref << "):" << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();
		pexprCopy->DbgPrint();
		pexprCopy->Release();

		// derive stats on expression
		CReqdPropRelational *prprel = GPOS_NEW(mp) CReqdPropRelational(GPOS_NEW(mp) CColRefSet(mp));
		IStatisticsArray *stats_ctxt = GPOS_NEW(mp) IStatisticsArray(mp);
		IStatistics *stats = pexpr->PstatsDerive(prprel, stats_ctxt);
		GPOS_ASSERT(NULL != stats);

		oss << "Expected risk: " << rgulRisk[i] << std::endl;
		oss << std::endl << "STATS:" << *stats << std::endl;
		GPOS_TRACE(str.GetBuffer());

		GPOS_ASSERT(rgulRisk[i] == stats->StatsEstimationRisk());

		prprel->Release();
		stats_ctxt->Release();
#endif // GPOS_DEBUG

		// cleanup
		pexpr->Release();
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//		CExpressionTest::EresUnittest_Union
//
//	@doc:
//		Basic tree builder test w/ Unions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_Union()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	// build union tree of depth 2
	CExpression *pexpr = CTestUtils::PexprLogicalUnion(mp, 2);

	// debug print
	CWStringDynamic str(mp);
	COstreamString oss(&str);
	pexpr->OsPrint(oss);

	GPOS_TRACE(str.GetBuffer());

	// derive properties on expression
	(void) pexpr->PdpDerive();

#ifdef GPOS_DEBUG
	CReqdPropRelational *prprel = GPOS_NEW(mp) CReqdPropRelational(GPOS_NEW(mp) CColRefSet(mp));
	IStatisticsArray *stats_ctxt = GPOS_NEW(mp) IStatisticsArray(mp);
	IStatistics *stats = pexpr->PstatsDerive(prprel, stats_ctxt);
	GPOS_ASSERT(NULL != stats);

	// We expect a risk of 3 because every Union increments the risk.
	GPOS_ASSERT(3 == stats->StatsEstimationRisk());
	stats_ctxt->Release();
	prprel->Release();
#endif // GPOS_DEBUG

	// cleanup
	pexpr->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//		CExpressionTest::EresUnittest_BitmapScan
//
//	@doc:
//		Basic tree builder test with bitmap index and table get.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_BitmapGet()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	CWStringConst strRelName(GPOS_WSZ_LIT("MyTable"));
	CWStringConst strRelAlias(GPOS_WSZ_LIT("T"));
	CMDIdGPDB *rel_mdid = GPOS_NEW(mp) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1);
	const WCHAR *wszColNameFormat = GPOS_WSZ_LIT("column_%04d");
	CTableDescriptor *ptabdesc = CTestUtils::PtabdescPlainWithColNameFormat(
			mp, 3, rel_mdid, wszColNameFormat, CName(&strRelName), false);

	// get the index associated with the table
	const IMDRelation *pmdrel = mda.RetrieveRel(ptabdesc->MDId());
	GPOS_ASSERT(0 < pmdrel->IndexCount());

	// create an index descriptor
	IMDId *pmdidIndex = pmdrel->IndexMDidAt(0 /*pos*/);
	const IMDIndex *pmdindex = mda.RetrieveIndex(pmdidIndex);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	const ULONG num_cols = pmdrel->ColumnCount();
	GPOS_ASSERT(2 < num_cols);

	// create an index on the first column
	const IMDColumn *pmdcol = pmdrel->GetMdCol(0);
	const IMDType *pmdtype = mda.RetrieveType(pmdcol->MdidType());
	CColRef *pcrFirst = col_factory->PcrCreate(pmdtype, pmdcol->TypeModifier());

	CExpression *pexprIndexCond = CUtils::PexprScalarEqCmp
								(
								mp,
								pcrFirst,
								CUtils::PexprScalarConstInt4(mp, 20 /*val*/)
								);

	CMDIdGPDB *mdid = GPOS_NEW(mp) CMDIdGPDB(CMDIdGPDB::m_mdid_unknown);
	CIndexDescriptor *pindexdesc = CIndexDescriptor::Pindexdesc(mp, ptabdesc, pmdindex);
	CExpression *pexprBitmapIndex =
			GPOS_NEW(mp) CExpression
					(
					mp,
					GPOS_NEW(mp) CScalarBitmapIndexProbe(mp, pindexdesc, mdid),
					pexprIndexCond
					);

	CColRefArray *pdrgpcrTable = GPOS_NEW(mp) CColRefArray(mp);
	for (ULONG ul = 0; ul < num_cols; ++ul)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		const IMDType *pmdtype = mda.RetrieveType(pmdcol->MdidType());
		CColRef *colref = col_factory->PcrCreate(pmdtype, pmdcol->TypeModifier());
		pdrgpcrTable->Append(colref);
	}

	CExpression *pexprTableCond = CUtils::PexprScalarEqCmp
								(
								mp,
								pcrFirst,
								CUtils::PexprScalarConstInt4(mp, 20 /*val*/)
								);

	CExpression *pexprBitmapTableGet =
			GPOS_NEW(mp) CExpression
					(
					mp,
					GPOS_NEW(mp) CLogicalBitmapTableGet
							(
							mp,
							ptabdesc,
							gpos::ulong_max, // pgexprOrigin
							GPOS_NEW(mp) CName(mp, CName(&strRelAlias)),
							pdrgpcrTable
							),
					pexprTableCond,
					pexprBitmapIndex
					);

	// debug print
	CWStringDynamic str(mp);
	COstreamString oss(&str);
	pexprBitmapTableGet->OsPrint(oss);

	CWStringConst strExpectedDebugPrint(GPOS_WSZ_LIT(
		"+--CLogicalBitmapTableGet , Table Name: (\"MyTable\"), Columns: [\"ColRef_0001\" (1), \"ColRef_0002\" (2), \"ColRef_0003\" (3)]\n"
		"   |--CScalarCmp (=)\n"
		"   |  |--CScalarIdent \"ColRef_0000\" (0)\n"
		"   |  +--CScalarConst (20)\n"
		"   +--CScalarBitmapIndexProbe   Bitmap Index Name: (\"T_a\")\n"
		"      +--CScalarCmp (=)\n"
		"         |--CScalarIdent \"ColRef_0000\" (0)\n"
		"         +--CScalarConst (20)\n"));

	GPOS_ASSERT(str.Equals(&strExpectedDebugPrint));

	// derive properties on expression
	(void) pexprBitmapTableGet->PdpDerive();

	// test matching of bitmap index probe expressions
	CMDIdGPDB *pmdid2 = GPOS_NEW(mp) CMDIdGPDB(CMDIdGPDB::m_mdid_unknown);
	CIndexDescriptor *pindexdesc2 = CIndexDescriptor::Pindexdesc(mp, ptabdesc, pmdindex);
	CExpression *pexprIndexCond2 = CUtils::PexprScalarEqCmp
								(
								mp,
								pcrFirst,
								CUtils::PexprScalarConstInt4(mp, 20 /*val*/)
								);
	CExpression *pexprBitmapIndex2 =
			GPOS_NEW(mp) CExpression
					(
					mp,
					GPOS_NEW(mp) CScalarBitmapIndexProbe(mp, pindexdesc2, pmdid2),
					pexprIndexCond2
					);
	CWStringDynamic strIndex2(mp);
	COstreamString ossIndex2(&strIndex2);
	pexprBitmapIndex2->OsPrint(ossIndex2);
	CWStringConst strExpectedDebugPrintIndex2(GPOS_WSZ_LIT(
	// clang-format off
		"+--CScalarBitmapIndexProbe   Bitmap Index Name: (\"T_a\")\n"
		"   +--CScalarCmp (=)\n"
		"      |--CScalarIdent \"ColRef_0000\" (0)\n"
		"      +--CScalarConst (20)\n"
	// clang-format on
														   ));

	GPOS_ASSERT(strIndex2.Equals(&strExpectedDebugPrintIndex2));
	GPOS_ASSERT(pexprBitmapIndex2->Matches(pexprBitmapIndex));

	mdid->AddRef();
	pexprBitmapIndex->AddRef();
	pexprBitmapIndex2->AddRef();
	CExpression *pexprBitmapBoolOp1 =
			GPOS_NEW(mp) CExpression
					(
					mp,
					GPOS_NEW(mp) CScalarBitmapBoolOp(mp, CScalarBitmapBoolOp::EbitmapboolAnd, mdid),
					pexprBitmapIndex,
					pexprBitmapIndex2
					);

	mdid->AddRef();
	pexprBitmapIndex->AddRef();
	CExpression *pexprBitmapBoolOp2 =
			GPOS_NEW(mp) CExpression
					(
					mp,
					GPOS_NEW(mp) CScalarBitmapBoolOp(mp, CScalarBitmapBoolOp::EbitmapboolAnd, mdid),
					pexprBitmapIndex,
					pexprBitmapIndex2
					);
	GPOS_ASSERT(pexprBitmapBoolOp2->Matches(pexprBitmapBoolOp1));

	// cleanup
	pexprBitmapBoolOp2->Release();
	pexprBitmapBoolOp1->Release();
	pexprBitmapTableGet->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_Const
//
//	@doc:
//		Test of scalar constant expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_Const()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	BOOL value = true;
	CExpression *pexprTrue = CUtils::PexprScalarConstBool(mp, value);

	value = false;
	CExpression *pexprFalse = CUtils::PexprScalarConstBool(mp, value);

	ULONG ulVal = 123456;
	CExpression *pexprUl = CUtils::PexprScalarConstInt4(mp, ulVal);
	CExpression *pexprUl2nd = CUtils::PexprScalarConstInt4(mp, ulVal);

	ulVal = 1;
	CExpression *pexprUlOne = CUtils::PexprScalarConstInt4(mp, ulVal);

	CWStringDynamic str(mp);
	COstreamString oss(&str);
	oss << std::endl;
	pexprTrue->OsPrint(oss);
	pexprFalse->OsPrint(oss);
	pexprUl->OsPrint(oss);
	pexprUl2nd->OsPrint(oss);
	pexprUlOne->OsPrint(oss);

#ifdef GPOS_DEBUG
	CScalarConst *pscalarconstTrue = CScalarConst::PopConvert(pexprTrue->Pop());
	CScalarConst *pscalarconstFalse = CScalarConst::PopConvert(pexprFalse->Pop());
	CScalarConst *pscalarconstUl = CScalarConst::PopConvert(pexprUl->Pop());
	CScalarConst *pscalarconstUl2nd = CScalarConst::PopConvert(pexprUl2nd->Pop());
	CScalarConst *pscalarconstUlOne = CScalarConst::PopConvert(pexprUlOne->Pop());
#endif // GPOS_DEBUG

	GPOS_ASSERT(pscalarconstUl->HashValue() == pscalarconstUl2nd->HashValue());
	GPOS_ASSERT(!pscalarconstTrue->Matches(pscalarconstFalse));
	GPOS_ASSERT(!pscalarconstTrue->Matches(pscalarconstUlOne));

	pexprTrue->Release();
	pexprFalse->Release();
	pexprUl->Release();
	pexprUl2nd->Release();
	pexprUlOne->Release();

	GPOS_TRACE(str.GetBuffer());

	return GPOS_OK;
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_ComparisonTypes
//
//	@doc:
//		Test of scalar comparison types
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_ComparisonTypes()
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

	const IMDType *pmdtype = mda.PtMDType<IMDTypeInt4>();

	GPOS_ASSERT(IMDType::EcmptEq == CUtils::ParseCmpType(pmdtype->GetMdidForCmpType(IMDType::EcmptEq)));
	GPOS_ASSERT(IMDType::EcmptL == CUtils::ParseCmpType(pmdtype->GetMdidForCmpType(IMDType::EcmptL)));
	GPOS_ASSERT(IMDType::EcmptG == CUtils::ParseCmpType(pmdtype->GetMdidForCmpType(IMDType::EcmptG)));

	const IMDScalarOp *pmdscopEq = mda.RetrieveScOp(pmdtype->GetMdidForCmpType(IMDType::EcmptEq));
	const IMDScalarOp *pmdscopLT = mda.RetrieveScOp(pmdtype->GetMdidForCmpType(IMDType::EcmptL));
	const IMDScalarOp *pmdscopGT = mda.RetrieveScOp(pmdtype->GetMdidForCmpType(IMDType::EcmptG));

	GPOS_ASSERT(IMDType::EcmptNEq == CUtils::ParseCmpType(pmdscopEq->GetInverseOpMdid()));
	GPOS_ASSERT(IMDType::EcmptLEq == CUtils::ParseCmpType(pmdscopGT->GetInverseOpMdid()));
	GPOS_ASSERT(IMDType::EcmptGEq == CUtils::ParseCmpType(pmdscopLT->GetInverseOpMdid()));

	return GPOS_OK;
}
#endif // GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::SetupPlanForFValidPlanTest
//
//	@doc:
//		Helper function for the FValidPlan tests
//
//---------------------------------------------------------------------------
void CExpressionTest::SetupPlanForFValidPlanTest
	(
	CMemoryPool *mp,
	CExpression **ppexprGby,
	CColRefSet **ppcrs,
	CExpression **ppexprPlan,
	CReqdPropPlan **pprpp
	)
{
	*ppexprGby = PexprCreateGbyWithColumnFormat(mp, GPOS_WSZ_LIT("Test Column%d"));

	// Create a column requirement using the first output column of the group by.
	CColRefSet *pcrsGby = (*ppexprGby)->DeriveOutputColumns();
	*ppcrs = GPOS_NEW(mp) CColRefSet(mp);
	(*ppcrs)->Include(pcrsGby->PcrFirst());

	*pprpp = PrppCreateRequiredProperties(mp, *ppcrs);
	CExpressionHandle exprhdl(mp);
	exprhdl.Attach(*ppexprGby);
	exprhdl.InitReqdProps(*pprpp);

	// Optimize the logical plan under default required properties, which are always satisfied.
	CEngine eng(mp);
	CAutoP<CQueryContext> pqc;
	pqc = CTestUtils::PqcGenerate(mp, *ppexprGby);
	eng.Init(pqc.Value(), NULL /*search_stage_array*/);
	eng.Optimize();
	*ppexprPlan = eng.PexprExtractPlan();
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_FValidPlan
//
//	@doc:
//		Test for CExpression::FValidPlan
// 		Test now just very basic cases. More complex cases are covered by minidump tests
// 		in CEnumeratorTest.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_FValidPlan()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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
	const IMDType *pmdtype = mda.PtMDType<IMDTypeInt4>();

	// Create a group-by with a get child. Properties required contain one of the columns in the group by.
	// Test that the plan is valid.
	{
		CExpression *pexprGby = NULL;
		CColRefSet *pcrs = NULL;
		CExpression *pexprPlan = NULL;
		CReqdPropPlan *prpp = NULL;

		SetupPlanForFValidPlanTest(mp, &pexprGby, &pcrs, &pexprPlan, &prpp);
		CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);

		// Test that prpp is actually satisfied.
		GPOS_ASSERT(pexprPlan->FValidPlan(prpp, pdpctxtplan));
		pdpctxtplan->Release();
		pexprPlan->Release();
		prpp->Release();
		pexprGby->Release();
	}
	// Create a group-by with a get child. Properties required contain one column that doesn't exist.
	// Test that the plan is NOT valid.
	{
		CExpression *pexprGby = PexprCreateGbyWithColumnFormat(mp, GPOS_WSZ_LIT("Test Column%d"));

		(void) pexprGby->PdpDerive();
		CColRefSet *pcrsInvalid = GPOS_NEW(mp) CColRefSet(mp);
		CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

		// Creating the column reference with the column factory ensures that it's a brand new column.
		CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, default_type_modifier);
		pcrsInvalid->Include(pcrComputed);

		CReqdPropPlan *prpp = PrppCreateRequiredProperties(mp, pcrsInvalid);
		CExpressionHandle exprhdl(mp);
		exprhdl.Attach(pexprGby);
		exprhdl.InitReqdProps(prpp);

		// Optimize the logical plan, but under default required properties, which are always satisfied.
		CEngine eng(mp);
		CAutoP<CQueryContext> pqc;
		pqc = CTestUtils::PqcGenerate(mp, pexprGby);
		eng.Init(pqc.Value(), NULL /*search_stage_array*/);
		eng.Optimize();
		CExpression *pexprPlan = eng.PexprExtractPlan();

		CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);

		// Test that prpp is actually unsatisfied.
		GPOS_ASSERT(!pexprPlan->FValidPlan(prpp, pdpctxtplan));
		pdpctxtplan->Release();
		pexprPlan->Release();
		prpp->Release();
		pexprGby->Release();
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_FValidPlan_InvalidOrder
//
//	@doc:
//		Test for CExpression::FValidPlan with incompatible order properties
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_FValidPlan_InvalidOrder()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	CExpression *pexprGby = NULL;
	CColRefSet *pcrs = NULL;
	CExpression *pexprPlan = NULL;
	CReqdPropPlan *prpp = NULL;

	SetupPlanForFValidPlanTest(mp, &pexprGby, &pcrs, &pexprPlan, &prpp);
	CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);

	// Create similar requirements, but
	// add an order requirement using a couple of output columns of a Get
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	CColRefSet *pcrsGet = pexprGet->DeriveOutputColumns();
	CColRefSet *pcrsGetCopy = GPOS_NEW(mp) CColRefSet(mp, *pcrsGet);

	CColRefArray *pdrgpcrGet = pcrsGetCopy->Pdrgpcr(mp);
	GPOS_ASSERT(2 <= pdrgpcrGet->Size());

	COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);
	const IMDType *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>();
	IMDId *pmdidInt4LT = pmdtypeint4->GetMdidForCmpType(IMDType::EcmptL);
	pmdidInt4LT->AddRef();
	pos->Append(pmdidInt4LT, (*pdrgpcrGet)[1], COrderSpec::EntFirst);
	CEnfdOrder *peo = GPOS_NEW(mp) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);

	CDistributionSpec *pds = GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtNone, CRewindabilitySpec::EmhtNoMotion);
	CEnfdDistribution *ped = GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);
	CEnfdRewindability *per = GPOS_NEW(mp) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);
	CCTEReq *pcter = GPOS_NEW(mp) CCTEReq(mp);
	CReqdPropPlan *prppIncompatibleOrder = GPOS_NEW(mp) CReqdPropPlan(pcrsGetCopy, peo, ped, per, pcter);

	CPartInfo *ppartinfo = GPOS_NEW(mp) CPartInfo(mp);
	prppIncompatibleOrder->InitReqdPartitionPropagation(mp, ppartinfo);

	// Test that the plan is not valid.
	GPOS_ASSERT(!pexprPlan->FValidPlan(prppIncompatibleOrder, pdpctxtplan));
	pdpctxtplan->Release();
	prppIncompatibleOrder->Release();
	pdrgpcrGet->Release();
	ppartinfo->Release();
	prpp->Release();
	pexprGet->Release();
	pexprPlan->Release();
	pexprGby->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_FValidPlan_InvalidDistribution
//
//	@doc:
//		Test for CExpression::FValidPlan with incompatible distribution properties
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_FValidPlan_InvalidDistribution()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	CExpression *pexprGby = NULL;
	CColRefSet *pcrs = NULL;
	CExpression *pexprPlan = NULL;
	CReqdPropPlan *prpp = NULL;

	SetupPlanForFValidPlanTest(mp, &pexprGby, &pcrs, &pexprPlan, &prpp);
	CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);
	COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);
	CDistributionSpec *pds = GPOS_NEW(mp) CDistributionSpecRandom();
	CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtNone, CRewindabilitySpec::EmhtNoMotion);
	CEnfdOrder *peo = GPOS_NEW(mp) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);
	CEnfdDistribution *ped = GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);
	CEnfdRewindability *per = GPOS_NEW(mp) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);
	CCTEReq *pcter = GPOS_NEW(mp) CCTEReq(mp);
	CColRefSet *pcrsCopy = GPOS_NEW(mp) CColRefSet(mp, *pcrs);
	CReqdPropPlan *prppIncompatibleDistribution = GPOS_NEW(mp) CReqdPropPlan(pcrsCopy, peo, ped, per, pcter);
	CPartInfo *ppartinfo = GPOS_NEW(mp) CPartInfo(mp);
	prppIncompatibleDistribution->InitReqdPartitionPropagation(mp, ppartinfo);

	// Test that the plan is not valid.
	GPOS_ASSERT(!pexprPlan->FValidPlan(prppIncompatibleDistribution, pdpctxtplan));
	pdpctxtplan->Release();
	pexprPlan->Release();
	prppIncompatibleDistribution->Release();
	prpp->Release();
	pexprGby->Release();
	ppartinfo->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_FValidPlan_InvalidRewindability
//
//	@doc:
//		Test for CExpression::FValidPlan with incompatible rewindability properties
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_FValidPlan_InvalidRewindability()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	CExpression *pexprGby = NULL;
	CColRefSet *pcrs = NULL;
	CExpression *pexprPlan = NULL;
	CReqdPropPlan *prpp = NULL;

	SetupPlanForFValidPlanTest(mp, &pexprGby, &pcrs, &pexprPlan, &prpp);
	CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);

	COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);
	CDistributionSpec *pds = GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtRewindable, CRewindabilitySpec::EmhtNoMotion);
	CEnfdOrder *peo = GPOS_NEW(mp) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);
	CEnfdDistribution *ped = GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);
	CEnfdRewindability *per = GPOS_NEW(mp) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);
	CCTEReq *pcter = GPOS_NEW(mp) CCTEReq(mp);
	CColRefSet *pcrsCopy = GPOS_NEW(mp) CColRefSet(mp, *pcrs);
	CReqdPropPlan *prppIncompatibleRewindability = GPOS_NEW(mp) CReqdPropPlan(pcrsCopy, peo, ped, per, pcter);
	CPartInfo *ppartinfo = GPOS_NEW(mp) CPartInfo(mp);
	prppIncompatibleRewindability->InitReqdPartitionPropagation(mp, ppartinfo);

	// Test that the plan is not valid.
	GPOS_ASSERT(!pexprPlan->FValidPlan(prppIncompatibleRewindability, pdpctxtplan));
	pdpctxtplan->Release();
	pexprPlan->Release();
	prppIncompatibleRewindability->Release();
	prpp->Release();
	pexprGby->Release();
	ppartinfo->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_FValidPlan_InvalidCTEs
//
//	@doc:
//		Test for CExpression::FValidPlan with incompatible CTE properties
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_FValidPlan_InvalidCTEs()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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

	CExpression *pexprGby = NULL;
	CColRefSet *pcrs = NULL;
	CExpression *pexprPlan = NULL;
	CReqdPropPlan *prpp = NULL;

	SetupPlanForFValidPlanTest(mp, &pexprGby, &pcrs, &pexprPlan, &prpp);
	CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);

	COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);
	CDistributionSpec *pds = GPOS_NEW(mp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(CRewindabilitySpec::ErtNone, CRewindabilitySpec::EmhtNoMotion);
	CEnfdOrder *peo = GPOS_NEW(mp) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);
	CEnfdDistribution *ped = GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);
	CEnfdRewindability *per = GPOS_NEW(mp) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);
	CCTEReq *pcter = GPOS_NEW(mp) CCTEReq(mp);
	ULONG ulCTEId = 0;

	CExpression *pexprProducer = CTestUtils::PexprLogicalCTEProducerOverSelect(mp, ulCTEId);
	CDrvdPropPlan *pdpplan = CDrvdPropPlan::Pdpplan(pexprPlan->PdpDerive());
	pdpplan->AddRef();
	pcter->Insert(ulCTEId, CCTEMap::EctProducer /*ect*/, true /*fRequired*/, pdpplan);
	CColRefSet *pcrsCopy = GPOS_NEW(mp) CColRefSet(mp, *pcrs);
	CReqdPropPlan *prppIncompatibleCTE = GPOS_NEW(mp) CReqdPropPlan(pcrsCopy, peo, ped, per, pcter);
	CPartInfo *ppartinfo = GPOS_NEW(mp) CPartInfo(mp);
	prppIncompatibleCTE->InitReqdPartitionPropagation(mp, ppartinfo);

	// Test that the plan is not valid.
	GPOS_ASSERT(!pexprPlan->FValidPlan(prppIncompatibleCTE, pdpctxtplan));
	pdpctxtplan->Release();
	pexprPlan->Release();
	prppIncompatibleCTE->Release();
	prpp->Release();
	pexprGby->Release();
	ppartinfo->Release();
	pexprProducer->Release();

	return GPOS_OK;
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_FValidPlanError
//
//	@doc:
//		Tests that CExpression::FValidPlan fails with an assert exception in debug mode
//		for bad input.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_FValidPlanError()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

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
	const IMDType *pmdtype = mda.PtMDType<IMDTypeInt4>();

	GPOS_RESULT eres = GPOS_OK;
	// Test that in debug mode GPOS_ASSERT fails for non-physical expressions.
	{
		CColRefSet *pcrsInvalid = GPOS_NEW(mp) CColRefSet(mp);
		CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
		CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, default_type_modifier);
		pcrsInvalid->Include(pcrComputed);

		CReqdPropPlan *prpp = PrppCreateRequiredProperties(mp, pcrsInvalid);
		IDatum *datum = GPOS_NEW(mp) gpnaucrates::CDatumInt8GPDB(CTestUtils::m_sysidDefault,
				1 /*val*/, false /*is_null*/);
		CExpression *pexpr = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarConst(mp, datum));

		CDrvdPropCtxtPlan *pdpctxtplan = GPOS_NEW(mp) CDrvdPropCtxtPlan(mp);
		GPOS_TRY
		{
			// FValidPlan should fail for expressions which are not physical.
			if (!pexpr->FValidPlan(prpp, pdpctxtplan))
			{
				eres = GPOS_FAILED;
			}
			pdpctxtplan->Release();
		}
		GPOS_CATCH_EX(ex)
		{
			pdpctxtplan->Release();
			if (!GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiAssert))
			{
				GPOS_RETHROW(ex);
			}
			else
			{
				GPOS_RESET_EX;
			}
		}
		GPOS_CATCH_END;

		pexpr->Release();
		prpp->Release();
	}

	return eres;
}
#endif // GPOS_DEBUG



//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresCheckCachedReqdCols
//
//	@doc:
//		Helper for checking cached required columns
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresCheckCachedReqdCols
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	CReqdPropPlan *prppInput
	)
{
	if (pexpr->Pop()->FScalar())
	{
		// scalar operators have no required columns
		return GPOS_OK;
	}

	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != prppInput);

	CExpressionHandle exprhdl(mp);
	exprhdl.Attach(pexpr);

	// init required properties of expression
	exprhdl.InitReqdProps(prppInput);

	// create array of child derived properties
	CDrvdPropArray *pdrgpdp = GPOS_NEW(mp) CDrvdPropArray(mp);

	GPOS_RESULT eres = GPOS_OK;
	const ULONG arity =  pexpr->Arity();
	for (ULONG ul = 0; GPOS_OK == eres && ul < arity; ul++)
	{
		CExpression *pexprChild = (*pexpr)[ul];
		if (pexprChild->Pop()->FScalar())
		{
			continue;
		}
		GPOS_ASSERT(NULL != pexprChild->Prpp());

		// extract cached required columns of the n-th child
		CColRefSet *pcrsChildReqd = pexprChild->Prpp()->PcrsRequired();

		// compute required columns of the n-th child
		exprhdl.ComputeChildReqdCols(ul, pdrgpdp);

		// check if cached columns pointer is the same as computed columns pointer,
		// if this is not the case, then we have re-computed the same set of columns and test should fail
		if (pcrsChildReqd != exprhdl.Prpp(ul)->PcrsRequired())
		{
			CAutoTrace at(mp);
			at.Os() << "\nExpression: \n" << *pexprChild;
			at.Os() << "\nCached cols: " << pcrsChildReqd << " : " << *pcrsChildReqd;
			at.Os() << "\nComputed cols: " <<  exprhdl.Prpp(ul)->PcrsRequired() << " : " << *exprhdl.Prpp(ul)->PcrsRequired();

			eres = GPOS_FAILED;
			continue;
		}

		// call the function recursively for child expression
		GPOS_RESULT eres = EresCheckCachedReqdCols(mp, pexprChild, exprhdl.Prpp(ul));
		if (GPOS_FAILED == eres)
		{
			eres = GPOS_FAILED;
			continue;
		}

		// add plan props of current child to derived props array
		CDrvdProp *pdp = pexprChild->PdpDerive();
		pdp->AddRef();
		pdrgpdp->Append(pdp);
	}

	pdrgpdp->Release();

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresComputeReqdCols
//
//	@doc:
//		Helper for testing required column computation
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresComputeReqdCols
	(
	const CHAR *szFilePath
	)
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// set up MD providers
	CMDProviderMemory *pmdp = GPOS_NEW(mp) CMDProviderMemory(mp, szFilePath);
	GPOS_CHECK_ABORT;

	GPOS_RESULT eres = GPOS_FAILED;
	{
		CAutoMDAccessor amda(mp, pmdp,  CTestUtils::m_sysidDefault);
		CAutoOptCtxt aoc(mp, amda.Pmda(), NULL,  /* pceeval */ CTestUtils::GetCostModel(mp));

		// read query expression
		CExpression *pexpr = CTestUtils::PexprReadQuery(mp, szFilePath);

		// optimize query
		CEngine eng(mp);
		CQueryContext *pqc = CTestUtils::PqcGenerate(mp, pexpr);
		eng.Init(pqc, NULL /*search_stage_array*/);
		eng.Optimize();

		// extract plan and decorate it with required columns
		CExpression *pexprPlan = eng.PexprExtractPlan();
		(void) pexprPlan->PrppCompute(mp, pqc->Prpp());

		// attempt computing required columns again --
		// we make sure that we reuse cached required columns at each operator
		eres = EresCheckCachedReqdCols(mp, pexprPlan, pqc->Prpp());

		pexpr->Release();
		pexprPlan->Release();
		GPOS_DELETE(pqc);
	}

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_ReqdCols
//
//	@doc:
//		Test for required columns computation
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_ReqdCols()
{
	const CHAR *rgszTests[] =
	{
		"../data/dxl/tpch/q1.mdp",
		"../data/dxl/tpch/q3.mdp",
		"../data/dxl/expressiontests/WinFunc-OuterRef-Partition-Query.xml",
		"../data/dxl/expressiontests/WinFunc-OuterRef-Partition-Order-Query.xml",
	};

	GPOS_RESULT eres = GPOS_OK;
	for (ULONG ul = 0; GPOS_OK == eres && ul < GPOS_ARRAY_SIZE(rgszTests); ul++)
	{
		const CHAR *szFilePath = rgszTests[ul];
		eres = EresComputeReqdCols(szFilePath);
	}

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::EresUnittest_InvalidSetOp
//
//	@doc:
//		Test for invalid SetOp expression,
//		SetOp is expected to have no outer references in input columns,
//		when an outer reference needs to be fed to SetOp as input, we must
//		project it first and feed the projected column into SetOp
//
//		For example, this is an invalid SetOp expression since it consumes
//		an outer reference from inner child
//
//			+--CLogicalUnion [Output: "col0" (0)], [Input: ("col0" (0)), ("col6" (6))]
//			   |--CLogicalGet ("T1"), Columns: ["col0" (0)]
//			   +--CLogicalGet ("T2"), Columns: ["col3" (3)]
//
//
//		the valid expression should looks like this:
//
//			+--CLogicalUnion [Output: "col0" (0)], [Input: ("col0" (0)), ("col7" (7))]
//			   |--CLogicalGet ("T1"), Columns: ["col0" (0)]
//			   +--CLogicalProject
//			       |--CLogicalGet ("T2"), Columns: ["col3" (3)]
//			       +--CScalarProjectList
//			       		+--CScalarProjectElement  "col7" (7)
//			       		      +--CScalarIdent "col6" (6)
//
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionTest::EresUnittest_InvalidSetOp()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	{
		CAutoOptCtxt aoc(mp, &mda, NULL /* pceeval */, CTestUtils::GetCostModel(mp));

		// create two different Get expressions
		CWStringConst strName1(GPOS_WSZ_LIT("T1"));
		CMDIdGPDB *pmdid1 = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
		CTableDescriptor *ptabdesc1 = CTestUtils::PtabdescCreate(mp, 3, pmdid1, CName(&strName1));
		CWStringConst strAlias1(GPOS_WSZ_LIT("T1Alias"));
		CExpression *pexprGet1 = CTestUtils::PexprLogicalGet(mp, ptabdesc1, &strAlias1);
		CColRefSet *pcrsOutput1 = pexprGet1->DeriveOutputColumns();

		CWStringConst strName2(GPOS_WSZ_LIT("T2"));
		CMDIdGPDB *pmdid2 = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID2, 1, 1);
		CTableDescriptor *ptabdesc2 = CTestUtils::PtabdescCreate(mp, 3, pmdid2, CName(&strName2));
		CWStringConst strAlias2(GPOS_WSZ_LIT("T2Alias"));
		CExpression *pexprGet2 = CTestUtils::PexprLogicalGet(mp, ptabdesc2, &strAlias2);

		// create output columns of SetOp from output columns of first Get expression
		CColRefArray *pdrgpcrOutput = GPOS_NEW(mp) CColRefArray(mp);
		pdrgpcrOutput->Append(pcrsOutput1->PcrFirst());

		// create input columns of SetOp while including an outer reference in inner child
		CColRef2dArray *pdrgpdrgpcrInput = GPOS_NEW(mp) CColRef2dArray(mp);

		CColRefArray *pdrgpcr1 = GPOS_NEW(mp) CColRefArray(mp);
		pdrgpcr1->Append(pcrsOutput1->PcrFirst());
		pdrgpdrgpcrInput->Append(pdrgpcr1);

		CColRefArray *pdrgpcr2 = GPOS_NEW(mp) CColRefArray(mp);
		CColRef *pcrOuterRef = COptCtxt::PoctxtFromTLS()->Pcf()->PcrCreate(pcrsOutput1->PcrFirst());
		pdrgpcr2->Append(pcrOuterRef);
		pdrgpdrgpcrInput->Append(pdrgpcr2);

		// create invalid SetOp expression
		CLogicalUnion *pop = GPOS_NEW(mp) CLogicalUnion(mp, pdrgpcrOutput, pdrgpdrgpcrInput);
		CExpression *pexprSetOp = GPOS_NEW(mp) CExpression(mp, pop, pexprGet1, pexprGet2);

		{
			CAutoTrace at(mp);
			at.Os() << "\nInvalid SetOp Expression: \n" << *pexprSetOp;
		}

		// deriving relational properties must fail
		(void) pexprSetOp->PdpDerive();

		pexprSetOp->Release();
	}

	return GPOS_FAILED;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionTest::PexprComplexJoinTree
//
//	@doc:
// 		Return an expression with several joins
//
//---------------------------------------------------------------------------
CExpression *CExpressionTest::PexprComplexJoinTree
	(
	CMemoryPool *mp
	)
{
	// The plan will have this shape
	//
	//	+--CLogicalInnerJoin
	//	   |--CLogicalUnion
	//	   |  |--CLogicalLeftOuterJoin
	//	   |  |  |--CLogicalInnerJoin
	//	   |  |  |  |--CLogicalGet
	//	   |  |  |  +--CLogicalSelect
	//	   |  |  +--CLogicalInnerJoin
	//	   |  |     |--CLogicalGet
	//	   |  |     +--CLogicalDynamicGet
	//	   |  +--CLogicalLeftOuterJoin
	//	   |     |--CLogicalGet
	//	   |     +--CLogicalGet
	//	   +--CLogicalInnerJoin
	//	      |--CLogicalGet
	//	      +--CLogicalSelect

	CExpression *pexprInnerJoin1 = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>
									(
									mp,
									CTestUtils::PexprLogicalGet(mp),
									CTestUtils::PexprLogicalSelect(mp)
									);
	CExpression *pexprJoinIndex = CTestUtils::PexprLogicalJoinWithPartitionedAndIndexedInnerChild(mp);
	CExpression *pexprLeftJoin =
			CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>(mp, pexprInnerJoin1, pexprJoinIndex);

	CExpression *pexprOuterJoin = CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>(mp);
	CExpression *pexprInnerJoin4 =
			CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp, pexprLeftJoin, pexprOuterJoin);

	CExpression *pexprInnerJoin5 = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>
									(
									mp,
									CTestUtils::PexprLogicalGet(mp),
									CTestUtils::PexprLogicalSelect(mp)
									);
	CExpression *pexprTopJoin =
			CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp, pexprInnerJoin4, pexprInnerJoin5);

	return pexprTopJoin;
}

// EOF
