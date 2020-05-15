//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPredicateUtilsTest.cpp
//
//	@doc:
//		Test for predicate utilities
//---------------------------------------------------------------------------
#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CExpressionPreprocessor.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/operators/CPredicateUtilsTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/md/CMDIdGPDB.h"


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest
//
//	@doc:
//		Unittest for predicate utilities
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest()
{

	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_Conjunctions),
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_Disjunctions),
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_PlainEqualities),
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_Implication),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_Conjunctions
//
//	@doc:
//		Test extraction and construction of conjuncts
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_Conjunctions()
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

	// build conjunction
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	const ULONG ulConjs = 3;
	for (ULONG ul = 0; ul < ulConjs; ul++)
	{
		pdrgpexpr->Append(CUtils::PexprScalarConstBool(mp, true /*fValue*/));
	}
	CExpression *pexprConjunction = CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopAnd, pdrgpexpr);
	
	// break into conjuncts
	CExpressionArray *pdrgpexprExtract = CPredicateUtils::PdrgpexprConjuncts(mp, pexprConjunction);
	GPOS_ASSERT(pdrgpexprExtract->Size() == ulConjs);
	
	// collapse into single conjunct
	CExpression *pexpr = CPredicateUtils::PexprConjunction(mp, pdrgpexprExtract);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarConstTrue(pexpr));
	pexpr->Release();
	
	// collapse empty input array to conjunct
	CExpression *pexprSingleton = CPredicateUtils::PexprConjunction(mp, NULL /*pdrgpexpr*/);
	GPOS_ASSERT(NULL != pexprSingleton);
	pexprSingleton->Release();

	pexprConjunction->Release();

	// conjunction on scalar comparisons
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	CColRefSet *pcrs = pexprGet->DeriveOutputColumns();
	CColRef *pcr1 = pcrs->PcrAny();
	CColRef *pcr2 = pcrs->PcrFirst();
	CExpression *pexprCmp1 = CUtils::PexprScalarCmp(mp, pcr1, pcr2, IMDType::EcmptEq);
	CExpression *pexprCmp2 = CUtils::PexprScalarCmp(mp, pcr1, CUtils::PexprScalarConstInt4(mp, 1 /*val*/), IMDType::EcmptEq);

	CExpression *pexprConj = CPredicateUtils::PexprConjunction(mp, pexprCmp1, pexprCmp2);
	pdrgpexprExtract = CPredicateUtils::PdrgpexprConjuncts(mp, pexprConj);
	GPOS_ASSERT(2 == pdrgpexprExtract->Size());
	pdrgpexprExtract->Release();

	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprConj->Release();
	pexprGet->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_Disjunctions
//
//	@doc:
//		Test extraction and construction of disjuncts
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_Disjunctions()
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

	// build disjunction
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	const ULONG ulDisjs = 3;
	for (ULONG ul = 0; ul < ulDisjs; ul++)
	{
		pdrgpexpr->Append(CUtils::PexprScalarConstBool(mp, false /*fValue*/));
	}
	CExpression *pexprDisjunction = CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopOr, pdrgpexpr);

	// break into disjuncts
	CExpressionArray *pdrgpexprExtract = CPredicateUtils::PdrgpexprDisjuncts(mp, pexprDisjunction);
	GPOS_ASSERT(pdrgpexprExtract->Size() == ulDisjs);

	// collapse into single disjunct
	CExpression *pexpr = CPredicateUtils::PexprDisjunction(mp, pdrgpexprExtract);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarConstFalse(pexpr));
	pexpr->Release();

	// collapse empty input array to disjunct
	CExpression *pexprSingleton = CPredicateUtils::PexprDisjunction(mp, NULL /*pdrgpexpr*/);
	GPOS_ASSERT(NULL != pexprSingleton);
	pexprSingleton->Release();

	pexprDisjunction->Release();

	// disjunction on scalar comparisons
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	CColRefSet *pcrs = pexprGet->DeriveOutputColumns();
	CColRefSetIter crsi(*pcrs);

#ifdef GPOS_DEBUG
	BOOL fAdvance =
#endif
	crsi.Advance();
	GPOS_ASSERT(fAdvance);
	CColRef *pcr1 = crsi.Pcr();

#ifdef GPOS_DEBUG
	fAdvance =
#endif
	crsi.Advance();
	GPOS_ASSERT(fAdvance);
	CColRef *pcr2 = crsi.Pcr();

#ifdef GPOS_DEBUG
	fAdvance =
#endif
	crsi.Advance();
	GPOS_ASSERT(fAdvance);
	CColRef *pcr3 = crsi.Pcr();

	CExpression *pexprCmp1 = CUtils::PexprScalarCmp(mp, pcr1, pcr2, IMDType::EcmptEq);
	CExpression *pexprCmp2 = CUtils::PexprScalarCmp(mp, pcr1, CUtils::PexprScalarConstInt4(mp, 1 /*val*/), IMDType::EcmptEq);

	{
		CExpression *pexprDisj = CPredicateUtils::PexprDisjunction(mp, pexprCmp1, pexprCmp2);
		pdrgpexprExtract = CPredicateUtils::PdrgpexprDisjuncts(mp, pexprDisj);
		GPOS_ASSERT(2 == pdrgpexprExtract->Size());
		pdrgpexprExtract->Release();
		pexprDisj->Release();
	}


	{
		CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
		CExpression *pexprCmp3 = CUtils::PexprScalarCmp(mp, pcr2, pcr1, IMDType::EcmptG);
		CExpression *pexprCmp4 = CUtils::PexprScalarCmp(mp, CUtils::PexprScalarConstInt4(mp, 200 /*val*/), pcr3, IMDType::EcmptL);
		pexprCmp1->AddRef();
		pexprCmp2->AddRef();

		pdrgpexpr->Append(pexprCmp3);
		pdrgpexpr->Append(pexprCmp4);
		pdrgpexpr->Append(pexprCmp1);
		pdrgpexpr->Append(pexprCmp2);

		CExpression *pexprDisj = CPredicateUtils::PexprDisjunction(mp, pdrgpexpr);
		pdrgpexprExtract = CPredicateUtils::PdrgpexprDisjuncts(mp, pexprDisj);
		GPOS_ASSERT(4 == pdrgpexprExtract->Size());
		pdrgpexprExtract->Release();
		pexprDisj->Release();
	}

	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprGet->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_PlainEqualities
//
//	@doc:
//		Test the extraction of equality predicates between scalar identifiers
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_PlainEqualities()
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

	CExpression *pexprLeft = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprRight = CTestUtils::PexprLogicalGet(mp);

	CExpressionArray *pdrgpexprOriginal = GPOS_NEW(mp) CExpressionArray(mp);

	CColRefSet *pcrsLeft = pexprLeft->DeriveOutputColumns();
	CColRefSet *pcrsRight = pexprRight->DeriveOutputColumns();

	CColRef *pcrLeft = pcrsLeft->PcrAny();
	CColRef *pcrRight = pcrsRight->PcrAny();

	// generate an equality predicate between two column reference
	CExpression *pexprScIdentEquality =
		CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight);

	pexprScIdentEquality->AddRef();
	pdrgpexprOriginal->Append(pexprScIdentEquality);

	// generate a non-equality predicate between two column reference
	CExpression *pexprScIdentInequality =
		CUtils::PexprScalarCmp(mp, pcrLeft, pcrRight, CWStringConst(GPOS_WSZ_LIT("<")), GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_LT_OP));

	pexprScIdentInequality->AddRef();
	pdrgpexprOriginal->Append(pexprScIdentInequality);

	// generate an equality predicate between a column reference and a constant value
	CExpression *pexprScalarConstInt4 = CUtils::PexprScalarConstInt4(mp, 10 /*fValue*/);
	CExpression *pexprScIdentConstEquality = CUtils::PexprScalarEqCmp(mp, pexprScalarConstInt4, pcrRight);

	pdrgpexprOriginal->Append(pexprScIdentConstEquality);

	GPOS_ASSERT(3 == pdrgpexprOriginal->Size());

	CExpressionArray *pdrgpexprResult = CPredicateUtils::PdrgpexprPlainEqualities(mp, pdrgpexprOriginal);

	GPOS_ASSERT(1 == pdrgpexprResult->Size());

	// clean up
	pdrgpexprOriginal->Release();
	pdrgpexprResult->Release();
	pexprLeft->Release();
	pexprRight->Release();
	pexprScIdentEquality->Release();
	pexprScIdentInequality->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_Implication
//
//	@doc:
//		Test removal of implied predicates
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_Implication()
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

	// generate a two cascaded joins
	CWStringConst strName1(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdid1 = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdesc1 = CTestUtils::PtabdescCreate(mp, 3, pmdid1, CName(&strName1));
	CWStringConst strAlias1(GPOS_WSZ_LIT("Rel1"));
	CExpression *pexprRel1 = CTestUtils::PexprLogicalGet(mp, ptabdesc1, &strAlias1);

	CWStringConst strName2(GPOS_WSZ_LIT("Rel2"));
	CMDIdGPDB *pmdid2 = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID2, 1, 1);
	CTableDescriptor *ptabdesc2 = CTestUtils::PtabdescCreate(mp, 3, pmdid2, CName(&strName2));
	CWStringConst strAlias2(GPOS_WSZ_LIT("Rel2"));
	CExpression *pexprRel2 = CTestUtils::PexprLogicalGet(mp, ptabdesc2, &strAlias2);

	CWStringConst strName3(GPOS_WSZ_LIT("Rel3"));
	CMDIdGPDB *pmdid3 = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID3, 1, 1);
	CTableDescriptor *ptabdesc3 = CTestUtils::PtabdescCreate(mp, 3, pmdid3, CName(&strName3));
	CWStringConst strAlias3(GPOS_WSZ_LIT("Rel3"));
	CExpression *pexprRel3 = CTestUtils::PexprLogicalGet(mp, ptabdesc3, &strAlias3);

	CExpression *pexprJoin1 = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp, pexprRel1, pexprRel2);
	CExpression *pexprJoin2 = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp, pexprJoin1, pexprRel3);

	{
		CAutoTrace at(mp);
		at.Os() << "Original expression:" << std::endl << *pexprJoin2 <<std::endl;
	}

	// imply new predicates by deriving constraints
	CExpression *pexprConstraints = CExpressionPreprocessor::PexprAddPredicatesFromConstraints(mp, pexprJoin2);

	{
		CAutoTrace at(mp);
		at.Os() << "Expression with implied predicates:" << std::endl << *pexprConstraints <<std::endl;;
	}

	// minimize join predicates by removing implied conjuncts
	CExpressionHandle exprhdl(mp);
	exprhdl.Attach(pexprConstraints);
	CExpression *pexprMinimizedPred = CPredicateUtils::PexprRemoveImpliedConjuncts(mp, (*pexprConstraints)[2], exprhdl);

	{
		CAutoTrace at(mp);
		at.Os() << "Minimized join predicate:" << std::endl << *pexprMinimizedPred <<std::endl;
	}

	CExpressionArray *pdrgpexprOriginalConjuncts = CPredicateUtils::PdrgpexprConjuncts(mp,  (*pexprConstraints)[2]);
	CExpressionArray *pdrgpexprNewConjuncts = CPredicateUtils::PdrgpexprConjuncts(mp, pexprMinimizedPred);

	GPOS_ASSERT(pdrgpexprNewConjuncts->Size() < pdrgpexprOriginalConjuncts->Size());

	// clean up
	pdrgpexprOriginalConjuncts->Release();
	pdrgpexprNewConjuncts->Release();
	pexprJoin2->Release();
	pexprConstraints->Release();
	pexprMinimizedPred->Release();

	return GPOS_OK;
}
// EOF
