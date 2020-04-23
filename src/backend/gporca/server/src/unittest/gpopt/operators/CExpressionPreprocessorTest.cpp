//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CExpressionPreprocessorTest.cpp
//
//	@doc:
//		Test for expression preprocessing
//---------------------------------------------------------------------------
#include <string.h>

#include "gpos/error/CAutoTrace.h"
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/common/CAutoRef.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/mdcache/CAutoMDAccessor.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CScalarProjectElement.h"
#include "gpopt/operators/CLogicalNAryJoin.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftOuterJoin.h"
#include "gpopt/operators/CExpressionUtils.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/operators/CExpressionPreprocessorTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest
//
//	@doc:
//		Unittest for predicate utilities
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest()
{

	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(EresUnittest_UnnestSubqueries),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcess),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessWindowFunc),
		GPOS_UNITTEST_FUNC(EresUnittest_InferPredsOnLOJ),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessWindowFuncWithLOJ),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessWindowFuncWithOuterRefs),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessWindowFuncWithDistinctAggs),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessNestedScalarSubqueries),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessOuterJoin),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessOuterJoinMinidumps),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessOrPrefilters),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessOrPrefiltersPartialPush),
		GPOS_UNITTEST_FUNC(EresUnittest_CollapseInnerJoin),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessConvert2InPredicate),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessConvertArrayWithEquals),
		GPOS_UNITTEST_FUNC(EresUnittest_PreProcessConvert2InPredicateDeepExpressionTree)
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasSubqueryAll
//
//	@doc:
//		Check if a given expression has an ALL subquery
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasSubqueryAll
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopScalarSubqueryAll,
	};

	return CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasSubqueryAny
//
//	@doc:
//		Check if a given expression has an ANY subquery
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasSubqueryAny
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopScalarSubqueryAny,
	};

	return CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasSubqueryExists
//
//	@doc:
//		Check if a given expression has a subquery exists
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasSubqueryExists
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopScalarSubqueryExists,
	};

	return CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasSubqueryNotExists
//
//	@doc:
//		Check if a given expression has a subquery not exists
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasSubqueryNotExists
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopScalarSubqueryNotExists,
	};

	return CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasNoOuterJoin
//
//	@doc:
//		Check if a given expression has no Outer Join nodes
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasNoOuterJoin
	(
	CExpression *pexpr
	)
{
	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopLogicalLeftOuterJoin,
	};

	return !CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::HasOuterRefs
//
//	@doc:
//		Check if a given expression has outer references in any node
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::HasOuterRefs
	(
	CExpression *pexpr
	)
{
	COperator *pop = pexpr->Pop();
	BOOL fHasOuterRefs = (pop->FLogical() && CUtils::HasOuterRefs(pexpr));
	if (fHasOuterRefs)
	{
		return true;
	}

	// recursively process children
	const ULONG arity = pexpr->Arity();
	fHasOuterRefs = false;
	for (ULONG ul = 0; !fHasOuterRefs && ul < arity; ul++)
	{
		fHasOuterRefs = HasOuterRefs((*pexpr)[ul]);
	}
	return fHasOuterRefs;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasSeqPrj
//
//	@doc:
//		Check if a given expression has Sequence Project nodes
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasSeqPrj
	(
	CExpression *pexpr
	)
{
	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopLogicalSequenceProject,
	};

	return CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::FHasIDF
//
//	@doc:
//		Check if a given expression has IS DISTINCT FROM nodes
//
//---------------------------------------------------------------------------
BOOL
CExpressionPreprocessorTest::FHasIDF
	(
	CExpression *pexpr
	)
{
	COperator::EOperatorId rgeopid[] =
	{
		COperator::EopScalarIsDistinctFrom,
	};

	return CUtils::FHasOp(pexpr, rgeopid, GPOS_ARRAY_SIZE(rgeopid));
}

#endif // GPOD_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcess
//
//	@doc:
//		Test of logical expression preprocessing
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcess()
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
		CTestUtils::PexprLogicalSelectWithConstAnySubquery,
		CTestUtils::PexprLogicalSelectWithConstAllSubquery,
		CTestUtils::PexprLogicalSelectWithNestedAnd,
		CTestUtils::PexprLogicalSelectWithNestedOr,
		CTestUtils::PexprLogicalSelectWithEvenNestedNot,
		CTestUtils::PexprLogicalSelectWithOddNestedNot,
		CTestUtils::PexprLogicalSelectWithNestedAndOrNot,
		CTestUtils::PexprLogicalSelectOnOuterJoin,
		CTestUtils::PexprNAryJoinOnLeftOuterJoin,
		};

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

		// generate expression
		CExpression *pexpr = rgpf[i](mp);

		CWStringDynamic str(mp);
		COstreamString oss(&str);

		oss	<< std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

 		CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexpr);
		oss	<< std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

		pexprPreprocessed->Release();
		pexpr->Release();
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFunc
//
//	@doc:
//		Test preprocessing of window functions with unpushable predicates
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFunc()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

	// generate a Select with a null-filtering predicate on top of Outer Join,
	// pre-processing should transform Outer Join to Inner Join
	CExpression *pexprSelectOnOuterJoin = CTestUtils::PexprLogicalSelectOnOuterJoin(mp);

	OID row_number_oid = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetWindowOids()->OidRowNumber();

	// add a window function with a predicate on top of the Outer Join expression
	CExpression *pexprWindow = CTestUtils::PexprLogicalSequenceProject(mp, row_number_oid, pexprSelectOnOuterJoin);
	CExpression *pexpr = CTestUtils::PexprLogicalSelect(mp, pexprWindow);

	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
	GPOS_TRACE(str.GetBuffer());
	str.Reset();

 	CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexpr);
	oss << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
	GPOS_TRACE(str.GetBuffer());

	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed) && "unexpected outer join");

	str.Reset();

	pexprPreprocessed->Release();
	pexpr->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PexprJoinHelper
//
//	@doc:
//		Helper function for testing cascaded inner/outer joins
//
//---------------------------------------------------------------------------
CExpression *
CExpressionPreprocessorTest::PexprJoinHelper
	(
	CMemoryPool *mp,
	CExpression *pexprLOJ,
	BOOL fCascadedLOJ,
	BOOL fIntermediateInnerjoin
	)
{
	CExpression *pexprBottomJoin = pexprLOJ;
	CExpression *pexprResult = pexprBottomJoin;

	if (fIntermediateInnerjoin)
	{
		CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
		CColRef *pcrLeft = (*pexprBottomJoin)[0]->DeriveOutputColumns()->PcrAny();
		CColRef *pcrRight = pexprGet->DeriveOutputColumns()->PcrAny();
		CExpression *pexprEquality = CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight);

		pexprBottomJoin = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalInnerJoin(mp), pexprBottomJoin, pexprGet, pexprEquality);
		pexprResult = pexprBottomJoin;
	}

	if (fCascadedLOJ)
	{
		// generate cascaded LOJ expression
		CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
		CColRef *pcrLeft = pexprBottomJoin->DeriveOutputColumns()->PcrAny();
		CColRef *pcrRight = pexprGet->DeriveOutputColumns()->PcrAny();
		CExpression *pexprEquality = CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight);

		pexprResult = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalLeftOuterJoin(mp), pexprBottomJoin, pexprGet, pexprEquality);
	}

	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PexprWindowFuncWithLOJHelper
//
//	@doc:
//		Helper function for testing window functions with outer join
//
//---------------------------------------------------------------------------
CExpression *
CExpressionPreprocessorTest::PexprWindowFuncWithLOJHelper
	(
	CMemoryPool *mp,
	CExpression *pexprLOJ,
	CColRef *pcrPartitionBy,
	BOOL fAddWindowFunction,
	BOOL fOuterChildPred,
	BOOL fCascadedLOJ,
	BOOL fPredBelowWindow
	)
{
	// add window function on top of join expression
	CColRefArray *pdrgpcrPartitionBy = GPOS_NEW(mp) CColRefArray(mp);
	pdrgpcrPartitionBy->Append(pcrPartitionBy);

	// add Select node on top of window function
	CExpression *pexprPred = CUtils::PexprScalarEqCmp(mp, pcrPartitionBy, CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
	if (!fOuterChildPred && fCascadedLOJ)
	{
		// add another predicate on inner child of top LOJ
		CColRef *pcrInner = (*pexprLOJ)[1]->DeriveOutputColumns()->PcrAny();
		if (fAddWindowFunction)
		{
			pdrgpcrPartitionBy->Append(pcrInner);
		}
		CExpression *pexprPred2 = CUtils::PexprScalarEqCmp(mp, pcrInner, CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
		CExpression *pexprConjunction = CPredicateUtils::PexprConjunction(mp, pexprPred, pexprPred2);
		pexprPred->Release();
		pexprPred2->Release();
		pexprPred = pexprConjunction;
	}

	if (fAddWindowFunction)
	{
		if (fPredBelowWindow)
		{
			pexprPred->AddRef();
			pexprLOJ = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPred);
		}

		CExpression *pexprPartitionedWinFunc = CXformUtils::PexprWindowWithRowNumber(mp, pexprLOJ, pdrgpcrPartitionBy);
		pexprLOJ->Release();
		pdrgpcrPartitionBy->Release();

		return CUtils::PexprLogicalSelect(mp, pexprPartitionedWinFunc, pexprPred);
	}

	pdrgpcrPartitionBy->Release();
	return 	CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PreprocessOuterJoin
//
//	@doc:
//		Helper for preprocessing outer join by rewriting as inner join
//
//---------------------------------------------------------------------------
void
CExpressionPreprocessorTest::PreprocessOuterJoin
	(
	const CHAR *szFilePath,
	BOOL
#ifdef GPOS_DEBUG
	 fAllowOuterJoin
#endif // GPOS_DEBUG
	)
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// set up MD providers
	CMDProviderMemory *pmdp = GPOS_NEW(mp) CMDProviderMemory(mp, szFilePath);
	GPOS_CHECK_ABORT;

	{
		CAutoMDAccessor amda(mp, pmdp,  CTestUtils::m_sysidDefault);
		CAutoOptCtxt aoc(mp, amda.Pmda(), NULL,  /* pceeval */ CTestUtils::GetCostModel(mp));

		// read query expression
		CExpression *pexpr = CTestUtils::PexprReadQuery(mp, szFilePath);
		GPOS_ASSERT(!FHasNoOuterJoin(pexpr) && "expected outer join");

		CWStringDynamic str(mp);
		COstreamString oss(&str);

		oss << std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

		CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexpr);
		oss << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
		GPOS_TRACE(str.GetBuffer());

#ifdef GPOS_DEBUG
		if (fAllowOuterJoin)
		{
			GPOS_ASSERT(!FHasNoOuterJoin(pexprPreprocessed) && "expected outer join");
		}
		else
		{
			GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed) && "unexpected outer join");
		}
#endif // GPOS_DEBUG

		str.Reset();

		pexprPreprocessed->Release();
		pexpr->Release();
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessOuterJoinMinidumps
//
//	@doc:
//		Test preprocessing of outer-joins ini minidumps by rewriting as inner-joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessOuterJoinMinidumps()
{
	// tests where OuterJoin must be converted to InnerJoin
	const CHAR *rgszOuterJoinPositiveTests[] =
	{
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q1.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q3.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q5.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q7.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q9.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q11.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q13.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q15.xml",
	};

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszOuterJoinPositiveTests); ul++)
	{
		const CHAR *szFilePath = rgszOuterJoinPositiveTests[ul];
		PreprocessOuterJoin(szFilePath, false /*fAllowOuterJoin*/);
	}

	// tests where OuterJoin must NOT be converted to InnerJoin
	const CHAR *rgszOuterJoinNegativeTests[] =
	{
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q2.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q4.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q6.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q8.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q10.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q12.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q14.xml",
		"../data/dxl/expressiontests/LOJ-TO-InnerJoin-Q16.xml",
	};

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszOuterJoinNegativeTests); ul++)
	{
		const CHAR *szFilePath = rgszOuterJoinNegativeTests[ul];
		PreprocessOuterJoin(szFilePath, true /*fAllowOuterJoin*/);
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresCompareExpressions
//
//	@doc:
//		Helper function for comparing expressions resulting from preprocessing
//		window functions with outer join
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresCompareExpressions
	(
	CMemoryPool *mp,
	CWStringDynamic *rgstr[],
	ULONG size
	)
{
	// check equality of processed expressions with/without the duplicate Select below Window
	for (ULONG ul = 0; ul < size; ul+=2)
	{
		CWStringDynamic *pstrFst = rgstr[ul];
		CWStringDynamic *pstrSnd = rgstr[ul + 1];
		BOOL fEqual = pstrFst->Equals(pstrSnd);

		if (!fEqual)
		{
			CAutoTrace at(mp);
			at.Os() << std::endl << "EXPECTED EQUAL EXPRESSIONS:";
			at.Os() << std::endl << "EXPR1:" << std::endl << pstrFst->GetBuffer() << std::endl;
			at.Os() << std::endl << "EXPR2:" << std::endl << pstrSnd->GetBuffer() << std::endl;
		}
		GPOS_ASSERT(fEqual && "expected equal expressions");

		GPOS_DELETE(pstrFst);
		GPOS_DELETE(pstrSnd);

		if (!fEqual)
		{
			return GPOS_FAILED;
		}
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//
//	@function:
//		CExpressionPreprocessorTest::EresTestLOJ
//
//	@doc:
//		Test case generator for outer joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresTestLOJ
	(
	BOOL fAddWindowFunction // if true, a window function is added on top of outer join test cases
	)
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);


	// test for Select(SequenceProject(OuterJoin)) where Select's predicate is either:
	// 	(1) a predicate on LOJ's outer child that can be pushed through to the leaves, or
	// 	(2) a predicate on LOJ's inner child that can be used to turn outer join to inner join
	//
	// the flag fIntermediateInnerjoin controls adding InnerJoin to the expression
	// the flag fCascadedLOJ controls adding two cascaded LOJ's
	// the flag fOuterChildPred controls where the predicate columns are coming from with respect to LOJ children
	// the flag fPredBelowWindow controls duplicating the predicate below window function

	// the input expression looks like the following:
	//
	// clang-format off
	//		+--CLogicalSelect
	//		   |--CLogicalSequenceProject (HASHED: [ +--CScalarIdent "column_0000" (3) , nulls colocated ], [], [])  /* (added if fAddWindowFunction is TRUE) */
	//		   |  |--CLogicalSelect								/* (added if fPredBelowWindow is TRUE) */
	//		   |  |  |--CLogicalLeftOuterJoin
	//		   |  |  |  |--CLogicalInnerJoin					/* (added if fIntermediateInnerjoin is TRUE) */
	//		   |  |  |  |  |--CLogicalLeftOuterJoin				/* (added if fCascadedLOJ is TRUE) */
	//		   |  |  |  |  |  |--CLogicalGet "BaseTableAlias" ("BaseTable"), Columns: ["column_0000" (3), "column_0001" (4), "column_0002" (5)] Key sets: {[0]}
	//		   |  |  |  |  |  |--CLogicalGet "BaseTableAlias" ("BaseTable"), Columns: ["column_0000" (0), "column_0001" (1), "column_0002" (2)] Key sets: {[0]}
	//		   |  |  |  |  |  +--CScalarCmp (=)
	//		   |  |  |  |  |     |--CScalarIdent "column_0000" (3)
	//		   |  |  |  |  |     +--CScalarIdent "column_0000" (0)
	//		   |  |  |  |  |--CLogicalGet "BaseTableAlias" ("BaseTable"), Columns: ["column_0000" (6), "column_0001" (7), "column_0002" (8)] Key sets: {[0]}
	//		   |  |  |  |  +--CScalarCmp (=)
	//		   |  |  |  |     |--CScalarIdent "column_0000" (3)
	//		   |  |  |  |     +--CScalarIdent "column_0000" (6)
	//		   |  |  |  |--CLogicalGet "BaseTableAlias" ("BaseTable"), Columns: ["column_0000" (9), "column_0001" (10), "column_0002" (11)] Key sets: {[0]}
	//		   |  |  |  +--CScalarCmp (=)
	//		   |  |  |     |--CScalarIdent "column_0000" (0)
	//		   |  |  |     +--CScalarIdent "column_0000" (9)
	//		   |  |  +--CScalarCmp (=)
	//		   |  |     |--CScalarIdent "column_0000" (3)
	//		   |  |     +--CScalarConst (1)
	//		   |  +--CScalarProjectList
	//		   |     +--CScalarProjectElement "row_number" (12)
	//		   |        +--CScalarWindowFunc (row_number , Distinct: false)
	//		   +--CScalarCmp (=)
	//			  |--CScalarIdent "column_0000" (3)				/* (column is generated from LOJ's outer child if fOuterChildPred is TRUE) */
	//			  +--CScalarConst (1)
	// clang-format on


	// we generate 16 test cases using 4 nested loops that consider all possible values of the 4 flags

	// array to store string representation of all preprocessed expressions
	CWStringDynamic *rgstrResult[16];
	ULONG ulTestCases = 0;
	BOOL fIntermediateInnerjoin = false;
	for (ULONG ulInnerJoinCases = 0; ulInnerJoinCases < 2; ulInnerJoinCases++)
	{
		fIntermediateInnerjoin = !fIntermediateInnerjoin;

		BOOL fCascadedLOJ = false;
		for (ULONG ulLOJCases = 0; ulLOJCases < 2; ulLOJCases++)
		{
			fCascadedLOJ = !fCascadedLOJ;

			BOOL fOuterChildPred = false;
			for (ULONG ulPredCases = 0; ulPredCases < 2; ulPredCases++)
			{
				fOuterChildPred = !fOuterChildPred;

				BOOL fPredBelowWindow = false;
				for (ULONG ulPredBelowWindowCases = 0; ulPredBelowWindowCases < 2; ulPredBelowWindowCases++)
				{
					CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/,  CTestUtils::GetCostModel(mp));

					fPredBelowWindow = fAddWindowFunction && !fPredBelowWindow;

					CExpression *pexprLOJ = CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>(mp);
					CColRef *colref = NULL;
					if (fOuterChildPred)
					{
						colref = (*pexprLOJ)[0]->DeriveOutputColumns()->PcrAny();
					}
					else
					{
						colref = (*pexprLOJ)[1]->DeriveOutputColumns()->PcrAny();
					}

					pexprLOJ = PexprJoinHelper(mp, pexprLOJ, fCascadedLOJ, fIntermediateInnerjoin);

					CExpression *pexprSelect = PexprWindowFuncWithLOJHelper(mp, pexprLOJ, colref, fAddWindowFunction, fOuterChildPred, fCascadedLOJ, fPredBelowWindow);

					CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect);

					{
						CAutoTrace at(mp);
						at.Os() << std::endl << "WindowFunction: "<< fAddWindowFunction << ", IntermediateInnerjoin: " << fIntermediateInnerjoin << ", CascadedLOJ: " << fCascadedLOJ << ", OuterChildPred: " << fOuterChildPred << ", PredBelowWindow: " << fPredBelowWindow;
						at.Os() << std::endl << "EXPR:" << std::endl << *pexprSelect << std::endl;
						at.Os() << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
					}

#ifdef GPOS_DEBUG
					if (fOuterChildPred)
					{
						GPOS_ASSERT(!FHasNoOuterJoin(pexprPreprocessed) && "expected outer join");
					}
					else
					{
						GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed) && "unexpected outer join");
					}
#endif // GPOS_DEBUG

					// store string representation of preprocessed expression
					CWStringDynamic *str = GPOS_NEW(mp) CWStringDynamic(mp);
					COstreamString oss(str);
					oss << *pexprPreprocessed;
					rgstrResult[ulTestCases] = str;
					ulTestCases++;

					pexprSelect->Release();
					pexprPreprocessed->Release();
				}
			}
		}
	}

	return EresCompareExpressions(mp, rgstrResult, ulTestCases);
}


//---------------------------------------------------------------------------
//
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_InferPredsOnLOJ
//
//	@doc:
//		Test of inferring predicates on outer join
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_InferPredsOnLOJ()
{
	return EresTestLOJ(false /*fAddWindowFunction*/);
}

//---------------------------------------------------------------------------
//
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFuncWithLOJ
//
//	@doc:
//		Test preprocessing of outer join with window functions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFuncWithLOJ()
{
	return EresTestLOJ(true /*fAddWindowFunction*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PreprocessWinFuncWithOuterRefs
//
//	@doc:
//		Helper for preprocessing window functions with outer references
//
//---------------------------------------------------------------------------
void
CExpressionPreprocessorTest::PreprocessWinFuncWithOuterRefs
	(
	const CHAR *szFilePath,
	BOOL
#ifdef GPOS_DEBUG
		fAllowWinFuncOuterRefs
#endif // GPOS_DEBUG
	)
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();


	// reset metadata cache
	CMDCache::Reset();

	// set up MD providers
	CMDProviderMemory *pmdp = GPOS_NEW(mp) CMDProviderMemory(mp, szFilePath);
	GPOS_CHECK_ABORT;

	{
		CAutoMDAccessor amda(mp, pmdp,  CTestUtils::m_sysidDefault);
		CAutoOptCtxt aoc(mp, amda.Pmda(), NULL,  /* pceeval */ CTestUtils::GetCostModel(mp));

		// read query expression
		CExpression *pexpr = CTestUtils::PexprReadQuery(mp, szFilePath);
		GPOS_ASSERT(HasOuterRefs(pexpr) && "expected outer references");

		CWStringDynamic str(mp);
		COstreamString oss(&str);

		oss << std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
		GPOS_TRACE(str.GetBuffer());
		str.Reset();

		CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexpr);
		oss << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
		GPOS_TRACE(str.GetBuffer());

#ifdef GPOS_DEBUG
		if (fAllowWinFuncOuterRefs)
		{
			GPOS_ASSERT(HasOuterRefs(pexprPreprocessed) && "expected outer references");
		}
		else
		{
			GPOS_ASSERT(!HasOuterRefs(pexprPreprocessed) && "unexpected outer references");
		}
#endif // GPOS_DEBUG

		str.Reset();

		pexprPreprocessed->Release();
		pexpr->Release();
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFuncWithOuterRefs
//
//	@doc:
//		Test preprocessing of window functions when no outer references
//		are expected
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFuncWithOuterRefs()
{
	const CHAR *rgszTestsNoOuterRefs[] =
	{
		"../data/dxl/expressiontests/WinFunc-OuterRef-Partition-Query.xml",
		"../data/dxl/expressiontests/WinFunc-OuterRef-Partition-Order-Query.xml",
	};

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszTestsNoOuterRefs); ul++)
	{
		const CHAR *szFilePath = rgszTestsNoOuterRefs[ul];
		PreprocessWinFuncWithOuterRefs(szFilePath, false /*fAllowWinFuncOuterRefs*/);
	}

	const CHAR *rgszTestsOuterRefs[] =
	{
		"../data/dxl/expressiontests/WinFunc-OuterRef-Partition-Order-Frames-Query.xml",
	};

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszTestsOuterRefs); ul++)
	{
		const CHAR *szFilePath = rgszTestsOuterRefs[ul];
		PreprocessWinFuncWithOuterRefs(szFilePath, true /*fAllowWinFuncOuterRefs*/);
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PreprocessWinFuncWithDistinctAggs
//
//	@doc:
//		Helper for preprocessing window functions with distinct aggs
//
//---------------------------------------------------------------------------
void
CExpressionPreprocessorTest::PreprocessWinFuncWithDistinctAggs
	(
	CMemoryPool *mp,
	const CHAR *szFilePath,
	BOOL
#ifdef GPOS_DEBUG
		fAllowSeqPrj
#endif // GPOS_DEBUG
	,
	BOOL
#ifdef GPOS_DEBUG
		fAllowIDF
#endif // GPOS_DEBUG

	)
{
	// read query expression
	CExpression *pexpr = CTestUtils::PexprReadQuery(mp, szFilePath);
	GPOS_ASSERT(FHasSeqPrj(pexpr) && "expected sequence project");

	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
	GPOS_TRACE(str.GetBuffer());
	str.Reset();

	CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexpr);
	oss << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
	GPOS_TRACE(str.GetBuffer());

#ifdef GPOS_DEBUG
	if (fAllowSeqPrj)
	{
		GPOS_ASSERT(FHasSeqPrj(pexprPreprocessed) && "expected sequence project");
	}
	else
	{
		GPOS_ASSERT(!FHasSeqPrj(pexprPreprocessed) && "unexpected sequence project");
	}

	if (fAllowIDF)
	{
		GPOS_ASSERT(FHasIDF(pexprPreprocessed) && "expected (is distinct from)");
	}
	else
	{
		GPOS_ASSERT(!FHasIDF(pexprPreprocessed) && "unexpected (is distinct from)");
	}

#endif // GPOS_DEBUG

	str.Reset();

	pexprPreprocessed->Release();
	pexpr->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFuncWithDistinctAggs
//
//	@doc:
//		Test preprocessing of window functions with distinct aggregates
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessWindowFuncWithDistinctAggs()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// tests where preprocessing removes SeqPrj nodes
	const CHAR *rgszTestsDistinctAggsRemoveWindow[] =
	{
		"../data/dxl/expressiontests/WinFunc-Single-DQA-Query.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-2.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-3.xml",
	};

	// tests where preprocessing removes SeqPrj nodes and adds join with INDF condition
	const CHAR *rgszTestsDistinctAggsRemoveWindowINDF[] =
	{
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-PartitionBy-SameColumn.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-PartitionBy-SameColumn-2.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-PartitionBy-DifferentColumn.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-PartitionBy-DifferentColumn-2.xml",
	};

	// tests where preprocessing does not remove SeqPrj nodes
	const CHAR *rgszTestsDistinctAggsDoNotRemoveWindow[] =
	{
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-2.xml",
	};

	// tests where preprocessing does not remove SeqPrj nodes and add join with INDF condition
	const CHAR *rgszTestsDistinctAggsDoNotRemoveWindowINDF[] =
	{
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-PartitionBy-SameColumn.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-PartitionBy-SameColumn-2.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-OrderBy-PartitionBy-SameColumn.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-OrderBy-PartitionBy-SameColumn-2.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-Distinct-Different-Columns.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-Distinct-ParitionBy-Different-Columns.xml",
		"../data/dxl/expressiontests/WinFunc-Multiple-DQA-Query-RowNumber-Multiple-ParitionBy-Columns.xml",
	};

	// path to metadata file of the previous tests
	const CHAR *szMDFilePath = "../data/dxl/expressiontests/WinFunc-Tests-MD.xml";

	// reset metadata cache
	CMDCache::Reset();

	// set up MD providers
	CMDProviderMemory *pmdp = GPOS_NEW(mp) CMDProviderMemory(mp, szMDFilePath);

	GPOS_CHECK_ABORT;

	{
		CAutoMDAccessor amda(mp, pmdp,  CTestUtils::m_sysidDefault);
		CAutoOptCtxt aoc(mp, amda.Pmda(), NULL,  /* pceeval */ CTestUtils::GetCostModel(mp));

		for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszTestsDistinctAggsRemoveWindow); ul++)
		{
			PreprocessWinFuncWithDistinctAggs(mp, rgszTestsDistinctAggsRemoveWindow[ul], false /* fAllowSeqPrj */, false /* fAllowIDF */);
		}

		for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszTestsDistinctAggsRemoveWindowINDF); ul++)
		{
			PreprocessWinFuncWithDistinctAggs(mp, rgszTestsDistinctAggsRemoveWindowINDF[ul], false /* fAllowSeqPrj */, true /* fAllowIDF */);
		}

		for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszTestsDistinctAggsDoNotRemoveWindow); ul++)
		{
			PreprocessWinFuncWithDistinctAggs(mp, rgszTestsDistinctAggsDoNotRemoveWindow[ul], true /* fAllowSeqPrj */, false /* fAllowIDF */);
		}

		for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszTestsDistinctAggsDoNotRemoveWindowINDF); ul++)
		{
			PreprocessWinFuncWithDistinctAggs(mp, rgszTestsDistinctAggsDoNotRemoveWindowINDF[ul], true /* fAllowSeqPrj */, true /* fAllowIDF */);
		}
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::UlScalarSubqs
//
//	@doc:
//		Count number of scalar subqueries
//
//---------------------------------------------------------------------------
ULONG
CExpressionPreprocessorTest::UlScalarSubqs
	(
	CExpression *pexpr
	)
{
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != pexpr);

	ULONG ulSubqs = 0;
	COperator *pop = pexpr->Pop();
	if (COperator::EopScalarSubquery == pop->Eopid())
	{
		ulSubqs = 1;
	}

	// recursively process children
	const ULONG arity = pexpr->Arity();
	ULONG ulChildSubqs = 0;
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ulChildSubqs += UlScalarSubqs((*pexpr)[ul]);
	}

	return ulSubqs + ulChildSubqs;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_UnnestSubqueries
//
//	@doc:
//		Test preprocessing of nested scalar subqueries that can be eliminated
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_UnnestSubqueries()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc(mp, &mda, NULL,  /* pceeval */ CTestUtils::GetCostModel(mp));

	SUnnestSubqueriesTestCase rgunnesttc[] =
		{
			{CTestUtils::PexprScalarCmpIdentToConstant, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopSentinel, false},
			{CTestUtils::PexprScalarCmpIdentToConstant, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopSentinel, false},

			{CTestUtils::PexprScalarCmpIdentToConstant, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopSentinel,  true},
			{CTestUtils::PexprScalarCmpIdentToConstant, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopSentinel, true},

			{CTestUtils::PexprExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryNotExists, false},
			{CTestUtils::PexprExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryNotExists, false},

			{CTestUtils::PexprNotExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryExists, false},
			{CTestUtils::PexprNotExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryExists, false},

			{CTestUtils::PexprExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryExists, true},
			{CTestUtils::PexprExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryExists, true},

			{CTestUtils::PexprNotExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryNotExists, true},
			{CTestUtils::PexprNotExistsSubquery, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryNotExists, true},

			{CTestUtils::PexpSubqueryAny, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryAny, false},
			{CTestUtils::PexpSubqueryAny, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryAny, false},

			{CTestUtils::PexpSubqueryAll, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryAll, false},
			{CTestUtils::PexpSubqueryAll, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryAll, false},

			{CTestUtils::PexpSubqueryAny, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryAny, true},
			{CTestUtils::PexpSubqueryAny, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryAny, true},

			{CTestUtils::PexpSubqueryAll, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopOr, COperator::EopScalarSubqueryAll, true},
			{CTestUtils::PexpSubqueryAll, CTestUtils::PexprScalarCmpIdentToConstant, CScalarBoolOp::EboolopAnd, COperator::EopScalarSubqueryAll, true},
		};

	GPOS_RESULT eres = GPOS_OK;

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgunnesttc);
	for (ULONG ul = 0; ul < ulTestCases && (GPOS_OK == eres); ul++)
	{
		SUnnestSubqueriesTestCase elem = rgunnesttc[ul];

		// generate the logical get
		CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);

		// generate the children of AND/OR predicate
		FnPexprUnnestTestCase *pfFst = elem.m_pfFst;
		FnPexprUnnestTestCase *pfSnd = elem.m_pfSnd;

		GPOS_ASSERT(NULL != pfFst);
		GPOS_ASSERT(NULL != pfSnd);
		CExpression *pexprPredFst = pfFst(mp, pexprGet);
		CExpression *pexprPredSnd = pfSnd(mp, pexprGet);

		BOOL fNegateChildren = elem.m_fNegateChildren;
		CExpressionArray *pdrgpexprAndOr = GPOS_NEW(mp) CExpressionArray(mp);

		if (fNegateChildren)
		{
			CExpressionArray *pdrgpexprFst = GPOS_NEW(mp) CExpressionArray(mp);
			pdrgpexprFst->Append(pexprPredFst);
			pdrgpexprAndOr->Append(CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopNot , pdrgpexprFst));

			CExpressionArray *pdrgpexprSnd = GPOS_NEW(mp) CExpressionArray(mp);
			pdrgpexprSnd->Append(pexprPredSnd);
			pdrgpexprAndOr->Append(CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopNot , pdrgpexprSnd));
		}
		else
		{
			pdrgpexprAndOr->Append(pexprPredFst);
			pdrgpexprAndOr->Append(pexprPredSnd);
		}

		CScalarBoolOp::EBoolOperator eboolop = elem.m_eboolop;
		CExpression *pexprAndOr = CUtils::PexprScalarBoolOp(mp, eboolop , pdrgpexprAndOr);

		CExpressionArray *pdrgpexprNot = GPOS_NEW(mp) CExpressionArray(mp);
		pdrgpexprNot->Append(pexprAndOr);

		CExpression *pexpr = GPOS_NEW(mp) CExpression
								(
								mp,
								GPOS_NEW(mp) CLogicalSelect(mp),
								pexprGet,
								CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopNot , pdrgpexprNot)
								);

		CExpression *pexprProcessed = CExpressionUtils::PexprUnnest(mp, pexpr);

		{
			CAutoTrace at(mp);
			at.Os() << std::endl << "EXPR:" << std::endl << *pexpr << std::endl;
			at.Os() << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprProcessed << std::endl;
		}

		const CExpression *pexprFirst = CTestUtils::PexprFirst(pexprProcessed, COperator::EopScalarBoolOp);
		if (NULL == pexprFirst || (eboolop == CScalarBoolOp::PopConvert(pexprFirst->Pop())->Eboolop()))
		{
			eres = GPOS_FAILED;
		}

		// operator that should be present after unnesting
		eres = EresCheckSubqueryType(pexprProcessed, elem.m_eopidPresent);

		// clean up
		pexpr->Release();
		pexprProcessed->Release();
	}

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresCheckExistsSubqueryType
//
//	@doc:
//		Check the type of the existential subquery
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresCheckExistsSubqueryType
	(
	CExpression *pexpr,
	COperator::EOperatorId eopidPresent
	)
{
	if (COperator::EopScalarSubqueryNotExists == eopidPresent && (FHasSubqueryExists(pexpr) || !FHasSubqueryNotExists(pexpr)))
	{
		return GPOS_FAILED;
	}

	if (COperator::EopScalarSubqueryExists == eopidPresent && (FHasSubqueryNotExists(pexpr) || !FHasSubqueryExists(pexpr)))
	{
		return GPOS_FAILED;
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresCheckQuantifiedSubqueryType
//
//	@doc:
//		Check the type of the quantified subquery
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresCheckQuantifiedSubqueryType
	(
	CExpression *pexpr,
	COperator::EOperatorId eopidPresent
	)
{
	if (COperator::EopScalarSubqueryAny == eopidPresent && !FHasSubqueryAny(pexpr))
	{
		return GPOS_FAILED;
	}

	if (COperator::EopScalarSubqueryAll == eopidPresent && !FHasSubqueryAll(pexpr))
	{
		return GPOS_FAILED;
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresCheckSubqueryType
//
//	@doc:
//		Check the type of the subquery
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresCheckSubqueryType
	(
	CExpression *pexpr,
	COperator::EOperatorId eopidPresent
	)
{
	GPOS_ASSERT(NULL != pexpr);

	if (COperator::EopSentinel == eopidPresent)
	{
		// no checks needed

		return GPOS_OK;
	}

	if (GPOS_OK == EresCheckExistsSubqueryType(pexpr, eopidPresent))
	{
		return GPOS_OK;
	}

	if (GPOS_OK == EresCheckQuantifiedSubqueryType(pexpr, eopidPresent))
	{
		return GPOS_OK;
	}

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessNestedScalarSubqueries
//
//	@doc:
//		Test preprocessing of nested scalar subqueries that can be eliminated
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessNestedScalarSubqueries()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc
					(
					mp,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(mp)
					);

	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	const CColRef *pcrInner = pexprGet->DeriveOutputColumns()->PcrAny();
	CExpression *pexprSubqInner = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubquery(mp, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprGet);
	CExpression *pexprCTG1 = CUtils::PexprLogicalCTGDummy(mp);
	CExpression *pexprPrj1 = CUtils::PexprAddProjection(mp, pexprCTG1, pexprSubqInner);

	const CColRef *pcrComputed = CScalarProjectElement::PopConvert((*(*pexprPrj1)[1])[0]->Pop())->Pcr();
	CExpression *pexprSubqOuter = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubquery(mp, pcrComputed, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprPrj1);
	CExpression *pexprCTG2 = CUtils::PexprLogicalCTGDummy(mp);
	CExpression *pexprPrj2 = CUtils::PexprAddProjection(mp, pexprCTG2, pexprSubqOuter);

	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << std::endl << "EXPR:" << std::endl << *pexprPrj2 << std::endl;
	GPOS_TRACE(str.GetBuffer());
	str.Reset();

 	CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexprPrj2);
	oss << std::endl << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
	GPOS_TRACE(str.GetBuffer());

	GPOS_ASSERT(1 == UlScalarSubqs(pexprPreprocessed) &&
			"expecting ONE scalar subquery in preprocessed expression");

	pexprPreprocessed->Release();
	pexprPrj2->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessOuterJoin
//
//	@doc:
//		Test of preprocessing outer-joins by rewriting as inner-joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessOuterJoin()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	CExpression *pexprLOJ = CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>(mp);
	CColRefSet *pcrsInner = (*pexprLOJ)[1]->DeriveOutputColumns();

	// test case 1: generate a single comparison predicate between an inner column and const
	CColRef *pcrInner = pcrsInner->PcrAny();
	CExpression *pexprPredicate1 = CUtils::PexprScalarEqCmp(mp, pcrInner, CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
	CExpression *pexprSelect1 = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPredicate1);

 	CExpression *pexprPreprocessed1 = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect1);
 	{
 		CAutoTrace at(mp);
 		at.Os() << "EXPR:" << std::endl << *pexprSelect1 << std::endl;
 		at.Os() << "No outer joins are expected after preprocessing:" << std::endl;
 		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed1 << std::endl;
 	}
	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed1) && "unexpected outer join");


 	// test case 2: generate a conjunction of predicates involving inner columns
 	CColRefSet *outer_refs = (*pexprLOJ)[0]->DeriveOutputColumns();
	CColRef *pcrOuter = outer_refs->PcrAny();
	CExpression *pexprCmp1 = CUtils::PexprScalarEqCmp(mp, pcrInner, pcrOuter);
	CExpression *pexprCmp2 = CUtils::PexprScalarEqCmp(mp, pcrInner, CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
	CExpression *pexprPredicate2 = CPredicateUtils::PexprConjunction(mp, pexprCmp1, pexprCmp2);
	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprLOJ->AddRef();
	CExpression *pexprSelect2 = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPredicate2);
	CExpression *pexprPreprocessed2 = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect2);
 	{
 		CAutoTrace at(mp);
 		at.Os() << "EXPR:" << std::endl << *pexprSelect2 << std::endl;
 		at.Os() << "No outer joins are expected after preprocessing:" << std::endl;
 		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed2 << std::endl;
 	}
	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed2) && "unexpected outer join");


	// test case 3: generate a disjunction of predicates involving inner columns
	pexprCmp1 = CUtils::PexprScalarEqCmp(mp, pcrInner, pcrOuter);
	pexprCmp2 = CUtils::PexprScalarEqCmp(mp, pcrInner,  CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
	CExpression *pexprPredicate3 = CPredicateUtils::PexprDisjunction(mp, pexprCmp1, pexprCmp2);
	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprLOJ->AddRef();
	CExpression *pexprSelect3 = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPredicate3);

	CExpression *pexprPreprocessed3 = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect3);
 	{
 		CAutoTrace at(mp);
 		at.Os() << "EXPR:" << std::endl << *pexprSelect3 << std::endl;
 		at.Os() << "No outer joins are expected after preprocessing:" << std::endl;
 		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed3 << std::endl;
 	}
	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed3) && "unexpected outer join");

	// test case 4: generate a null-rejecting conjunction since it involves one null-rejecting conjunct
	pexprCmp1 = CUtils::PexprScalarEqCmp(mp, pcrInner, pcrOuter);
	pexprCmp2 = CUtils::PexprScalarEqCmp(mp, pcrOuter, CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
	CExpression *pexprPredicate4 = CPredicateUtils::PexprConjunction(mp, pexprCmp1, pexprCmp2);
	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprLOJ->AddRef();
	CExpression *pexprSelect4 = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPredicate4);

	CExpression *pexprPreprocessed4 = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect4);
 	{
 		CAutoTrace at(mp);
 		at.Os() << "EXPR:" << std::endl << *pexprSelect4 << std::endl;
 		at.Os() << "No outer joins are expected after preprocessing:" << std::endl;
 		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed4 << std::endl;
 	}
	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed4) && "unexpected outer join");


	// test case 5: generate a null-passing disjunction since it involves a predicate on outer columns
	pexprCmp1 = CUtils::PexprScalarEqCmp(mp, pcrInner, pcrOuter);
	pexprCmp2 = CUtils::PexprScalarEqCmp(mp, pcrOuter, CUtils::PexprScalarConstInt4(mp, 1 /*val*/));
	CExpression *pexprPredicate5 = CPredicateUtils::PexprDisjunction(mp, pexprCmp1, pexprCmp2);
	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprLOJ->AddRef();
	CExpression *pexprSelect5 = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPredicate5);

	CExpression *pexprPreprocessed5 = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect5);
 	{
 		CAutoTrace at(mp);
 		at.Os() << "EXPR:" << std::endl << *pexprSelect5 << std::endl;
 		at.Os() << "Outer joins are expected after preprocessing:" << std::endl;
 		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed5 << std::endl;
 	}
	GPOS_ASSERT(!FHasNoOuterJoin(pexprPreprocessed5) && "expected outer join");

	// test case 6: generate a negated null-passing disjunction
	pexprPredicate5->AddRef();
	CExpression *pexprPredicate6 = CUtils::PexprNegate(mp, pexprPredicate5);
	pexprLOJ->AddRef();
	CExpression *pexprSelect6 = CUtils::PexprLogicalSelect(mp, pexprLOJ, pexprPredicate6);

	CExpression *pexprPreprocessed6 = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect6);
 	{
 		CAutoTrace at(mp);
 		at.Os() << "EXPR:" << std::endl << *pexprSelect6 << std::endl;
 		at.Os() << "No outer joins are expected after preprocessing:" << std::endl;
 		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed6 << std::endl;
 	}
	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed6) && "unexpected outer join");

 	pexprSelect1->Release();
 	pexprPreprocessed1->Release();
 	pexprSelect2->Release();
 	pexprPreprocessed2->Release();
	pexprSelect3->Release();
 	pexprPreprocessed3->Release();
	pexprSelect4->Release();
 	pexprPreprocessed4->Release();
	pexprSelect5->Release();
 	pexprPreprocessed5->Release();
	pexprSelect6->Release();
 	pexprPreprocessed6->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PexprCreateConjunction
//
//	@doc:
//		Create a conjunction of comparisons using the given columns.
//
//---------------------------------------------------------------------------
CExpression *
CExpressionPreprocessorTest::PexprCreateConjunction
	(
	CMemoryPool *mp,
	CColRefArray *colref_array
	)
{
	GPOS_ASSERT(NULL != colref_array);

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	ULONG length = colref_array->Size();
	for (ULONG ul = 0; ul < length; ++ul)
	{
		CExpression *pexprComparison= CUtils::PexprScalarEqCmp
										(
										mp,
										(*colref_array)[ul],
										CUtils::PexprScalarConstInt4(mp, ul)
										);
		pdrgpexpr->Append(pexprComparison);
	}

	return CPredicateUtils::PexprConjunction(mp, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessPrefilters
//
//	@doc:
//		Test extraction of prefilters out of disjunctive expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessOrPrefilters()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();
	CAutoTraceFlag atf(EopttraceArrayConstraints, true /*value*/);

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));
	CExpression *pexprJoin = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp);

	CColRefSet *pcrsInner = (*pexprJoin)[1]->DeriveOutputColumns();
	CColRefArray *pdrgpcrInner = pcrsInner->Pdrgpcr(mp);
	GPOS_ASSERT(pdrgpcrInner != NULL);
	GPOS_ASSERT(3 <= pdrgpcrInner->Size());

	CColRefSet *outer_refs = (*pexprJoin)[0]->DeriveOutputColumns();
	CColRefArray *pdrgpcrOuter = outer_refs->Pdrgpcr(mp);
	GPOS_ASSERT(pdrgpcrOuter != NULL);
	GPOS_ASSERT(3 <= pdrgpcrOuter->Size());

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);

	// every disjunct has one or two comparisons on various outer columns and one comparison on
	// the first inner column, so we expect to have prefilters both on the outer and the inner tables
	CColRefArray *pdrgpcrDisjunct = GPOS_NEW(mp) CColRefArray(mp);
	CColRef *pcr0_0 = (*pdrgpcrOuter)[0];
	pdrgpcrDisjunct->Append(pcr0_0);
	CColRef *pcr1_0 = (*pdrgpcrInner)[0];
	pdrgpcrDisjunct->Append(pcr1_0);
	pdrgpexpr->Append(PexprCreateConjunction(mp, pdrgpcrDisjunct));
	pdrgpcrDisjunct->Release();

	pdrgpcrDisjunct = GPOS_NEW(mp) CColRefArray(mp);
	CColRef *pcr0_1 = (*pdrgpcrOuter)[1];
	pdrgpcrDisjunct->Append(pcr0_1);
	CColRef *pcr0_2 = (*pdrgpcrOuter)[2];
	pdrgpcrDisjunct->Append(pcr0_2);
	pdrgpcrDisjunct->Append(pcr1_0);
	pdrgpexpr->Append(PexprCreateConjunction(mp, pdrgpcrDisjunct));
	pdrgpcrDisjunct->Release();

	pdrgpcrDisjunct = GPOS_NEW(mp) CColRefArray(mp);
	pdrgpcrDisjunct->Append(pcr0_2);
	pdrgpcrDisjunct->Append(pcr1_0);
	pdrgpexpr->Append(PexprCreateConjunction(mp, pdrgpcrDisjunct));
	pdrgpcrDisjunct->Release();

	pdrgpcrInner->Release();
	pdrgpcrOuter->Release();

	CExpression *pexprPredicate = CPredicateUtils::PexprDisjunction(mp, pdrgpexpr);
	CExpression *pexprSelect = CUtils::PexprLogicalSelect(mp, pexprJoin, pexprPredicate);
 	CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect);

 	CWStringDynamic strSelect(mp);
 	COstreamString oss(&strSelect);
 	pexprSelect->OsPrint(oss);
	CWStringConst strExpectedDebugPrintForSelect(GPOS_WSZ_LIT(
	// clang-format off
			"+--CLogicalSelect\n"
			"   |--CLogicalInnerJoin\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (3), \"column_0001\" (4), \"column_0002\" (5)] Key sets: {[0]}\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (0), \"column_0001\" (1), \"column_0002\" (2)] Key sets: {[0]}\n"
			"   |  +--CScalarCmp (=)\n"
			"   |     |--CScalarIdent \"column_0000\" (3)\n"
			"   |     +--CScalarIdent \"column_0000\" (0)\n"
			"   +--CScalarBoolOp (EboolopOr)\n"
			"      |--CScalarBoolOp (EboolopAnd)\n"
			"      |  |--CScalarCmp (=)\n"
			"      |  |  |--CScalarIdent \"column_0000\" (3)\n"
			"      |  |  +--CScalarConst (0)\n"
			"      |  +--CScalarCmp (=)\n"
			"      |     |--CScalarIdent \"column_0000\" (0)\n"
			"      |     +--CScalarConst (1)\n"
			"      |--CScalarBoolOp (EboolopAnd)\n"
			"      |  |--CScalarCmp (=)\n"
			"      |  |  |--CScalarIdent \"column_0001\" (4)\n"
			"      |  |  +--CScalarConst (0)\n"
			"      |  |--CScalarCmp (=)\n"
			"      |  |  |--CScalarIdent \"column_0002\" (5)\n"
			"      |  |  +--CScalarConst (1)\n"
			"      |  +--CScalarCmp (=)\n"
			"      |     |--CScalarIdent \"column_0000\" (0)\n"
			"      |     +--CScalarConst (2)\n"
			"      +--CScalarBoolOp (EboolopAnd)\n"
			"         |--CScalarCmp (=)\n"
			"         |  |--CScalarIdent \"column_0002\" (5)\n"
			"         |  +--CScalarConst (0)\n"
			"         +--CScalarCmp (=)\n"
			"            |--CScalarIdent \"column_0000\" (0)\n"
			"            +--CScalarConst (1)\n"
	// clang-format on
															  ));

	GPOS_ASSERT(strSelect.Equals(&strExpectedDebugPrintForSelect));

	BOOL fEqual = strSelect.Equals(&strExpectedDebugPrintForSelect);
	if (!fEqual)
	{
		CAutoTrace at(mp);
		at.Os() << std::endl << "RETURNED EXPRESSION:" << std::endl << strSelect.GetBuffer();
		at.Os() << std::endl << "EXPECTED EXPRESSION:" << std::endl << strExpectedDebugPrintForSelect.GetBuffer();

		return GPOS_FAILED;
	}

 	CWStringDynamic strPreprocessed(mp);
 	COstreamString ossPreprocessed(&strPreprocessed);
 	pexprPreprocessed->OsPrint(ossPreprocessed);
	CWStringConst strExpectedDebugPrintForPreprocessed(GPOS_WSZ_LIT(
	// clang-format off
			"+--CLogicalNAryJoin\n"
			"   |--CLogicalSelect\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (3), \"column_0001\" (4), \"column_0002\" (5)] Key sets: {[0]}\n"
			"   |  +--CScalarBoolOp (EboolopAnd)\n"
			"   |     |--CScalarArrayCmp Any (=)\n"
			"   |     |  |--CScalarIdent \"column_0000\" (3)\n"
			"   |     |  +--CScalarArray: {eleMDId: (23,1.0), arrayMDId: (1007,1.0)}\n"
			"   |     |     |--CScalarConst (1)\n"
			"   |     |     +--CScalarConst (2)\n"
			"   |     +--CScalarBoolOp (EboolopOr)\n"
			"   |        |--CScalarCmp (=)\n"
			"   |        |  |--CScalarIdent \"column_0000\" (3)\n"
			"   |        |  +--CScalarConst (0)\n"
			"   |        |--CScalarBoolOp (EboolopAnd)\n"
			"   |        |  |--CScalarCmp (=)\n"
			"   |        |  |  |--CScalarIdent \"column_0001\" (4)\n"
			"   |        |  |  +--CScalarConst (0)\n"
			"   |        |  +--CScalarCmp (=)\n"
			"   |        |     |--CScalarIdent \"column_0002\" (5)\n"
			"   |        |     +--CScalarConst (1)\n"
			"   |        +--CScalarCmp (=)\n"
			"   |           |--CScalarIdent \"column_0002\" (5)\n"
			"   |           +--CScalarConst (0)\n"
			"   |--CLogicalSelect\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (0), \"column_0001\" (1), \"column_0002\" (2)] Key sets: {[0]}\n"
			"   |  +--CScalarBoolOp (EboolopAnd)\n"
			"   |     |--CScalarBoolOp (EboolopOr)\n"
			"   |     |  |--CScalarCmp (=)\n"
			"   |     |  |  |--CScalarIdent \"column_0000\" (0)\n"
			"   |     |  |  +--CScalarConst (1)\n"
			"   |     |  +--CScalarCmp (=)\n"
			"   |     |     |--CScalarIdent \"column_0000\" (0)\n"
			"   |     |     +--CScalarConst (2)\n"
			"   |     +--CScalarArrayCmp Any (=)\n"
			"   |        |--CScalarIdent \"column_0000\" (0)\n"
			"   |        +--CScalarArray: {eleMDId: (23,1.0), arrayMDId: (1007,1.0)}\n"
			"   |           |--CScalarConst (1)\n"
			"   |           +--CScalarConst (2)\n"
			"   +--CScalarBoolOp (EboolopAnd)\n"
			"      |--CScalarCmp (=)\n"
			"      |  |--CScalarIdent \"column_0000\" (3)\n"
			"      |  +--CScalarIdent \"column_0000\" (0)\n"
			"      +--CScalarBoolOp (EboolopOr)\n"
			"         |--CScalarBoolOp (EboolopAnd)\n"
			"         |  |--CScalarCmp (=)\n"
			"         |  |  |--CScalarIdent \"column_0000\" (3)\n"
			"         |  |  +--CScalarConst (0)\n"
			"         |  +--CScalarCmp (=)\n"
			"         |     |--CScalarIdent \"column_0000\" (0)\n"
			"         |     +--CScalarConst (1)\n"
			"         |--CScalarBoolOp (EboolopAnd)\n"
			"         |  |--CScalarCmp (=)\n"
			"         |  |  |--CScalarIdent \"column_0001\" (4)\n"
			"         |  |  +--CScalarConst (0)\n"
			"         |  |--CScalarCmp (=)\n"
			"         |  |  |--CScalarIdent \"column_0002\" (5)\n"
			"         |  |  +--CScalarConst (1)\n"
			"         |  +--CScalarCmp (=)\n"
			"         |     |--CScalarIdent \"column_0000\" (0)\n"
			"         |     +--CScalarConst (2)\n"
			"         +--CScalarBoolOp (EboolopAnd)\n"
			"            |--CScalarCmp (=)\n"
			"            |  |--CScalarIdent \"column_0002\" (5)\n"
			"            |  +--CScalarConst (0)\n"
			"            +--CScalarCmp (=)\n"
			"               |--CScalarIdent \"column_0000\" (0)\n"
			"               +--CScalarConst (1)\n"
	// clang-format on
																	));

	pexprSelect->Release();
	pexprPreprocessed->Release();

	fEqual = strPreprocessed.Equals(&strExpectedDebugPrintForPreprocessed);
	if (!fEqual)
	{
		CAutoTrace at(mp);
		at.Os() << std::endl << "RETURNED EXPRESSION:" << std::endl << strPreprocessed.GetBuffer();
		at.Os() << std::endl << "EXPECTED EXPRESSION:" << std::endl << strExpectedDebugPrintForPreprocessed.GetBuffer();

		return GPOS_FAILED;
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_PreProcessPrefiltersPartialPush
//
//	@doc:
//		Test extraction of prefilters out of disjunctive expressions where
//		not all conditions can be pushed.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessOrPrefiltersPartialPush()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));
	CExpression *pexprJoin = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(mp);

	CColRefSet *pcrsInner = (*pexprJoin)[1]->DeriveOutputColumns();
	CColRefArray *pdrgpcrInner = pcrsInner->Pdrgpcr(mp);
	GPOS_ASSERT(NULL != pdrgpcrInner);
	GPOS_ASSERT(3 <= pdrgpcrInner->Size());

	CColRefSet *outer_refs = (*pexprJoin)[0]->DeriveOutputColumns();
	CColRefArray *pdrgpcrOuter = outer_refs->Pdrgpcr(mp);
	GPOS_ASSERT(NULL != pdrgpcrOuter);
	GPOS_ASSERT(3 <= pdrgpcrOuter->Size());

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);

	// first disjunct has conditions on both tables that can be pushed
	CColRefArray *pdrgpcrDisjunct = GPOS_NEW(mp) CColRefArray(mp);
	CColRef *pcr0_0 = (*pdrgpcrOuter)[0];
	pdrgpcrDisjunct->Append(pcr0_0);
	CColRef *pcr1_0 = (*pdrgpcrInner)[0];
	pdrgpcrDisjunct->Append(pcr1_0);
	pdrgpexpr->Append(PexprCreateConjunction(mp, pdrgpcrDisjunct));
	pdrgpcrDisjunct->Release();

	// second disjunct has only a condition on the inner branch
	pdrgpcrDisjunct = GPOS_NEW(mp) CColRefArray(mp);
	CColRef *pcr1_2 = (*pdrgpcrInner)[2];
	pdrgpcrDisjunct->Append(pcr1_2);
	pdrgpexpr->Append(PexprCreateConjunction(mp, pdrgpcrDisjunct));
	pdrgpcrDisjunct->Release();

	pdrgpcrInner->Release();
	pdrgpcrOuter->Release();

	CExpression *pexprPredicate = CPredicateUtils::PexprDisjunction(mp, pdrgpexpr);
	CExpression *pexprSelect = CUtils::PexprLogicalSelect(mp, pexprJoin, pexprPredicate);
 	CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect);

 	CWStringDynamic strSelect(mp);
 	COstreamString oss(&strSelect);
 	pexprSelect->OsPrint(oss);
	CWStringConst strExpectedDebugPrintForSelect(GPOS_WSZ_LIT(
	// clang-format on
			"+--CLogicalSelect\n"
			"   |--CLogicalInnerJoin\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (3), \"column_0001\" (4), \"column_0002\" (5)] Key sets: {[0]}\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (0), \"column_0001\" (1), \"column_0002\" (2)] Key sets: {[0]}\n"
			"   |  +--CScalarCmp (=)\n"
			"   |     |--CScalarIdent \"column_0000\" (3)\n"
			"   |     +--CScalarIdent \"column_0000\" (0)\n"
			"   +--CScalarBoolOp (EboolopOr)\n"
			"      |--CScalarBoolOp (EboolopAnd)\n"
			"      |  |--CScalarCmp (=)\n"
			"      |  |  |--CScalarIdent \"column_0000\" (3)\n"
			"      |  |  +--CScalarConst (0)\n"
			"      |  +--CScalarCmp (=)\n"
			"      |     |--CScalarIdent \"column_0000\" (0)\n"
			"      |     +--CScalarConst (1)\n"
		    "      +--CScalarCmp (=)\n"
		    "         |--CScalarIdent \"column_0002\" (2)\n"
		    "         +--CScalarConst (0)\n"
	// clang-format on
															  ));

	GPOS_ASSERT(strSelect.Equals(&strExpectedDebugPrintForSelect));

 	CWStringDynamic strPreprocessed(mp);
 	COstreamString ossPreprocessed(&strPreprocessed);
 	pexprPreprocessed->OsPrint(ossPreprocessed);
	CWStringConst strExpectedDebugPrintForPreprocessed(GPOS_WSZ_LIT(
	// clang-format off
			"+--CLogicalNAryJoin\n"
			"   |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (3), \"column_0001\" (4), \"column_0002\" (5)] Key sets: {[0]}\n"
			"   |--CLogicalSelect\n"
			"   |  |--CLogicalGet \"BaseTableAlias\" (\"BaseTable\"), Columns: [\"column_0000\" (0), \"column_0001\" (1), \"column_0002\" (2)] Key sets: {[0]}\n"
			"   |  +--CScalarBoolOp (EboolopOr)\n"
			"   |     |--CScalarCmp (=)\n"
			"   |     |  |--CScalarIdent \"column_0000\" (0)\n"
			"   |     |  +--CScalarConst (1)\n"
			"   |     +--CScalarCmp (=)\n"
			"   |        |--CScalarIdent \"column_0002\" (2)\n"
			"   |        +--CScalarConst (0)\n"
			"   +--CScalarBoolOp (EboolopAnd)\n"
			"      |--CScalarCmp (=)\n"
			"      |  |--CScalarIdent \"column_0000\" (3)\n"
			"      |  +--CScalarIdent \"column_0000\" (0)\n"
			"      +--CScalarBoolOp (EboolopOr)\n"
			"         |--CScalarBoolOp (EboolopAnd)\n"
			"         |  |--CScalarCmp (=)\n"
			"         |  |  |--CScalarIdent \"column_0000\" (3)\n"
			"         |  |  +--CScalarConst (0)\n"
			"         |  +--CScalarCmp (=)\n"
			"         |     |--CScalarIdent \"column_0000\" (0)\n"
			"         |     +--CScalarConst (1)\n"
			"         +--CScalarCmp (=)\n"
			"            |--CScalarIdent \"column_0002\" (2)\n"
			"            +--CScalarConst (0)\n"));
	// clang-format on


 	pexprSelect->Release();
 	pexprPreprocessed->Release();
	BOOL fEqual = strExpectedDebugPrintForPreprocessed.Equals(&strPreprocessed);
	if (!fEqual)
	{
		CAutoTrace at(mp);
		at.Os() << std::endl << "RETURNED EXPRESSION:" << std::endl << strPreprocessed.GetBuffer();
		at.Os() << std::endl << "EXPECTED EXPRESSION:" << std::endl << strExpectedDebugPrintForPreprocessed.GetBuffer();

		return GPOS_FAILED;
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_CollapseInnerJoinHelper
//
//	@doc:
//		Helper function for testing collapse of Inner Joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_CollapseInnerJoinHelper
	(
	CMemoryPool *mp,
	COperator *popJoin,
	CExpression *rgpexpr[],
	CDrvdPropRelational *rgpdprel[]
	)
{
	GPOS_ASSERT(NULL != popJoin);

	// (1) generate two nested outer joins
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	rgpexpr[0]->AddRef();
	pdrgpexpr->Append(rgpexpr[0]);
	rgpexpr[1]->AddRef();
	pdrgpexpr->Append(rgpexpr[1]);
	CTestUtils::EqualityPredicate(mp, rgpdprel[0]->GetOutputColumns(), rgpdprel[1]->GetOutputColumns(), pdrgpexpr);
	CExpression *pexprLOJ1 = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalLeftOuterJoin(mp), pdrgpexpr);

	pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	rgpexpr[2]->AddRef();
	pdrgpexpr->Append(rgpexpr[2]);
	pdrgpexpr->Append(pexprLOJ1);
	CTestUtils::EqualityPredicate(mp, rgpdprel[0]->GetOutputColumns(), rgpdprel[2]->GetOutputColumns(), pdrgpexpr);
	CExpression *pexprLOJ2 = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalLeftOuterJoin(mp), pdrgpexpr);

	// (2) add Inner/NAry Join on top of outer join
	pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexpr->Append(pexprLOJ2);
	rgpexpr[3]->AddRef();
	pdrgpexpr->Append(rgpexpr[3]);
	CTestUtils::EqualityPredicate(mp, rgpdprel[0]->GetOutputColumns(), rgpdprel[3]->GetOutputColumns(), pdrgpexpr);
	popJoin->AddRef();
	CExpression *pexprJoin1 = GPOS_NEW(mp) CExpression(mp, popJoin, pdrgpexpr);

	// (3) add another Inner/NAry Join on top of Inner/NAry Join
	pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexpr->Append(pexprJoin1);
	rgpexpr[4]->AddRef();
	pdrgpexpr->Append(rgpexpr[4]);
	CTestUtils::EqualityPredicate(mp, rgpdprel[0]->GetOutputColumns(), rgpdprel[4]->GetOutputColumns(), pdrgpexpr);
	popJoin->AddRef();
	CExpression *pexprJoin2 = GPOS_NEW(mp) CExpression(mp, popJoin, pdrgpexpr);

	// (4) add another Inner/NAry Join on top of Inner/NAry Join
	pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexpr->Append(pexprJoin2);
	rgpexpr[5]->AddRef();
	pdrgpexpr->Append(rgpexpr[5]);
	CTestUtils::EqualityPredicate(mp, rgpdprel[0]->GetOutputColumns(), rgpdprel[5]->GetOutputColumns(), pdrgpexpr);
	popJoin->AddRef();
	CExpression *pexprJoin3 = GPOS_NEW(mp) CExpression(mp, popJoin, pdrgpexpr);

	// (5) create Select with predicate that can turn all outer joins into inner joins,
	// add the Select on top of the top Inner/NAry Join
	CExpression *pexprCmpLOJInner = CUtils::PexprScalarEqCmp(mp, rgpdprel[1]->GetOutputColumns()->PcrFirst(), CUtils::PexprScalarConstInt4(mp, 1 /*value*/));
	CExpression *pexprSelect = CUtils::PexprSafeSelect(mp, pexprJoin3, pexprCmpLOJInner);
	CExpression *pexprPreprocessed = CExpressionPreprocessor::PexprPreprocess(mp, pexprSelect);
	{
		CAutoTrace at(mp);
		at.Os() <<  std::endl << "EXPR:" << std::endl << *pexprSelect << std::endl;
		at.Os() << "No outer joins are expected after preprocessing:" << std::endl;
		at.Os() << "PREPROCESSED EXPR:" << std::endl << *pexprPreprocessed << std::endl;
	}
	GPOS_ASSERT(FHasNoOuterJoin(pexprPreprocessed) && "unexpected outer join");

	// assert the structure of resulting expression,
	// root must be NAryJoin operator,
	// selection predicate must be pushed below the NaryJoin,
	// no other deep join trees remain below the root NAryJoin
	GPOS_ASSERT(COperator::EopLogicalNAryJoin == pexprPreprocessed->Pop()->Eopid() &&
			"root operator is expected to be NAryJoin");

#ifdef GPOS_DEBUG
	const ULONG arity = pexprPreprocessed->Arity();
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CExpression *pexprChild = (*pexprPreprocessed)[ul];
		GPOS_ASSERT(1 == CDrvdPropRelational::GetRelationalProperties(pexprChild->PdpDerive())->GetJoinDepth() &&
				"unexpected deep join tree below NAryJoin");

		COperator::EOperatorId op_id = pexprChild->Pop()->Eopid();
		GPOS_ASSERT((COperator::EopLogicalGet == op_id || COperator::EopLogicalSelect == op_id) &&
					"child operator is expected to be either Get or Select");
		GPOS_ASSERT_IMP(COperator::EopLogicalSelect == op_id, COperator::EopLogicalGet == (*pexprChild)[0]->Pop()->Eopid() &&
				"expected Select operator to be directly on top of Get operator");
	}
#endif // GPOS_DEBUG

	// cleanup
	pexprSelect->Release();
	pexprPreprocessed->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::EresUnittest_CollapseInnerJoin
//
//	@doc:
//		Test collapsing of Inner Joins
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_CollapseInnerJoin()
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
		GPOS_WSZ_LIT("Rel6"),

	};

	// array of relation IDs
	ULONG rgulRel[] =
	{
		GPOPT_TEST_REL_OID1,
		GPOPT_TEST_REL_OID2,
		GPOPT_TEST_REL_OID3,
		GPOPT_TEST_REL_OID4,
		GPOPT_TEST_REL_OID5,
		GPOPT_TEST_REL_OID6,
	};

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);
	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	CExpression *rgpexpr[GPOS_ARRAY_SIZE(rgscRel)];
	CDrvdPropRelational *rgpdprel[GPOS_ARRAY_SIZE(rgscRel)];
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgscRel); ul++)
	{
		rgpexpr[ul] = CTestUtils::PexprLogicalGet(mp, &rgscRel[ul], &rgscRel[ul], rgulRel[ul]);
		rgpdprel[ul] = CDrvdPropRelational::GetRelationalProperties(rgpexpr[ul]->PdpDerive());
	}

	// the following expression is used as input,
	// we also generate another variant with CLogicalInnerJoin instead of CLogicalNAryJoin

	//	+--CLogicalSelect
	//	   |--CLogicalNAryJoin
	//	   |  |--CLogicalNAryJoin
	//	   |  |  |--CLogicalNAryJoin
	//	   |  |  |  |--CLogicalLeftOuterJoin
	//	   |  |  |  |  |--CLogicalGet "Rel3" ("Rel3"), Columns: ["column_0000" (6), "column_0001" (7), "column_0002" (8)] Key sets: {[0]}
	//	   |  |  |  |  |--CLogicalLeftOuterJoin
	//	   |  |  |  |  |  |--CLogicalGet "Rel1" ("Rel1"), Columns: ["column_0000" (0), "column_0001" (1), "column_0002" (2)] Key sets: {[0]}
	//	   |  |  |  |  |  |--CLogicalGet "Rel2" ("Rel2"), Columns: ["column_0000" (3), "column_0001" (4), "column_0002" (5)] Key sets: {[0]}
	//	   |  |  |  |  |  +--CScalarCmp (=)
	//	   |  |  |  |  |     |--CScalarIdent "column_0000" (0)
	//	   |  |  |  |  |     +--CScalarIdent "column_0000" (3)
	//	   |  |  |  |  +--CScalarCmp (=)
	//	   |  |  |  |     |--CScalarIdent "column_0000" (0)
	//	   |  |  |  |     +--CScalarIdent "column_0000" (6)
	//	   |  |  |  |--CLogicalGet "Rel4" ("Rel4"), Columns: ["column_0000" (9), "column_0001" (10), "column_0002" (11)] Key sets: {[0]}
	//	   |  |  |  +--CScalarCmp (=)
	//	   |  |  |     |--CScalarIdent "column_0000" (0)
	//	   |  |  |     +--CScalarIdent "column_0000" (9)
	//	   |  |  |--CLogicalGet "Rel5" ("Rel5"), Columns: ["column_0000" (12), "column_0001" (13), "column_0002" (14)] Key sets: {[0]}
	//	   |  |  +--CScalarCmp (=)
	//	   |  |     |--CScalarIdent "column_0000" (0)
	//	   |  |     +--CScalarIdent "column_0000" (12)
	//	   |  |--CLogicalGet "Rel6" ("Rel6"), Columns: ["column_0000" (15), "column_0001" (16), "column_0002" (17)] Key sets: {[0]}
	//	   |  +--CScalarCmp (=)
	//	   |     |--CScalarIdent "column_0000" (0)
	//	   |     +--CScalarIdent "column_0000" (15)
	//	   +--CScalarCmp (=)
	//	      |--CScalarIdent "column_0000" (3)
	//	      +--CScalarConst (1)

	GPOS_RESULT eres = GPOS_OK;
	for (ULONG ulInput = 0; eres == GPOS_OK && ulInput < 2; ulInput++)
	{
		COperator *popJoin = NULL;
		if (0 == ulInput)
		{
			popJoin = GPOS_NEW(mp) CLogicalNAryJoin(mp);
		}
		else
		{
			popJoin = GPOS_NEW(mp) CLogicalInnerJoin(mp);
		}

		eres = EresUnittest_CollapseInnerJoinHelper(mp, popJoin, rgpexpr, rgpdprel);
		popJoin->Release();
	}

	// cleanup input expressions
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgscRel); ul++)
	{
		rgpexpr[ul]->Release();
	}

	return GPOS_OK;
}

// Tests that a expression with nested OR statements will convert them into
// an array IN statement. The statement we are testing looks is equivalent to
// +-LogicalGet
// 	+-ScalarBoolOp (Or)
// 		+-ScalarBoolCmp
// 			+-ScalarId
// 			+-ScalarConst
// 		+-ScalarBoolCmp
// 			+-ScalarId
// 			+-ScalarConst
// 		+-ScalarCmp
// 			+-ScalarId
// 			+-ScalarId
// and should convert to
// +-LogicalGet
// 	+-ScalarArrayCmp
// 		+-ScalarId
// 		+-ScalarArray
// 			+-ScalarConst
// 			+-ScalarConst
// 	+-ScalarCmp
// 		+-ScalarId
// 		+-ScalarId
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessConvert2InPredicate()
{
	CAutoTraceFlag atf(EopttraceArrayConstraints, true /*value*/);

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	CAutoRef<CExpression> apexprGet(CTestUtils::PexprLogicalGet(mp)); // useful for colref
	COperator *popGet = apexprGet->Pop();
	popGet->AddRef();

	// Create a disjunct, add as a child
	CColRef *pcrLeft = apexprGet->DeriveOutputColumns()->PcrAny();
	CScalarBoolOp *pscboolop = GPOS_NEW(mp) CScalarBoolOp(mp, CScalarBoolOp::EboolopOr);
	CExpression *pexprDisjunct =
			GPOS_NEW(mp) CExpression(
									mp,
									pscboolop,
									CUtils::PexprScalarEqCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 1 /*val*/)),
									CUtils::PexprScalarEqCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 2 /*val*/)),
									CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrLeft)
									);

	CAutoRef<CExpression> apexprGetWithChildren(GPOS_NEW(mp) CExpression(mp, popGet, pexprDisjunct));

	GPOS_ASSERT(3 == CUtils::UlCountOperator(apexprGetWithChildren.Value(), COperator::EopScalarCmp));

	CAutoRef<CExpression> apexprConvert(CExpressionPreprocessor::PexprConvert2In(mp, apexprGetWithChildren.Value()));

	GPOS_ASSERT(1 == CUtils::UlCountOperator(apexprConvert.Value(), COperator::EopScalarArrayCmp));
	GPOS_ASSERT(1 == CUtils::UlCountOperator(apexprConvert.Value(), COperator::EopScalarCmp));
	// the OR node should not be removed because there should be an array expression and
	// a scalar identity comparison
	GPOS_ASSERT(1 == CUtils::UlCountOperator(apexprConvert.Value(), COperator::EopScalarBoolOp));

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest::PexprCreateConvertableArray
//
//	@doc:
//		If fCreateInStatement is true then create an array expression like
//			A IN (1,2,3,4,5) OR A = 6 OR A = 7
//		If fCreateInStatement is set to false, create the NOT IN version like
//			A NOT IN (1,2,3,4,5) AND A <> 6 AND A <> 7
//
//---------------------------------------------------------------------------
CExpression *
CExpressionPreprocessorTest::PexprCreateConvertableArray
	(
	CMemoryPool *mp,
	BOOL fCreateInStatement
	)
{
	CScalarArrayCmp::EArrCmpType earrcmp = CScalarArrayCmp::EarrcmpAny;
	IMDType::ECmpType ecmptype = IMDType::EcmptEq;
	CScalarBoolOp::EBoolOperator eboolop = CScalarBoolOp::EboolopOr;
	if (!fCreateInStatement)
	{
		earrcmp = CScalarArrayCmp::EarrcmpAll;
		ecmptype = IMDType::EcmptNEq;
		eboolop = CScalarBoolOp::EboolopAnd;
	}
	CExpression *pexpr(CTestUtils::PexprLogicalSelectArrayCmp(mp, earrcmp, ecmptype));
	// get a ref to the comparison column
	CColRef *pcrLeft = pexpr->DeriveOutputColumns()->PcrAny();

	// remove the array child and then make an OR node with two equality comparisons
	CExpression *pexprArrayComp = (*pexpr->PdrgPexpr())[1];
	GPOS_ASSERT(CUtils::FScalarArrayCmp(pexprArrayComp));

	CExpressionArray *pdrgexprDisjChildren = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgexprDisjChildren->Append(pexprArrayComp);
	pdrgexprDisjChildren->Append(CUtils::PexprScalarCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 6 /*val*/), ecmptype));
	pdrgexprDisjChildren->Append(CUtils::PexprScalarCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 7 /*val*/), ecmptype));

	CScalarBoolOp *pscboolop = GPOS_NEW(mp) CScalarBoolOp(mp, eboolop);
	CExpression *pexprDisjConj = GPOS_NEW(mp) CExpression(mp, pscboolop, pdrgexprDisjChildren);
	pexprArrayComp->AddRef(); // needed for Replace()
	pexpr->PdrgPexpr()->Replace(1, pexprDisjConj);

	GPOS_ASSERT(2 == CUtils::UlCountOperator(pexpr, COperator::EopScalarCmp));
	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest
//				::EresUnittest_PreProcessConvertArrayWithEquals
//
//	@doc:
//		Test that an array expression like A IN (1,2,3,4,5) OR A = 6 OR A = 7
//		converts to A IN (1,2,3,4,5,6,7). Also test the NOT AND NEq variant
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessConvertArrayWithEquals()
{
	CAutoTraceFlag atf(EopttraceArrayConstraints, true /*value*/);

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	// test the IN OR Eq variant
	CAutoRef<CExpression> apexprInConvertable(PexprCreateConvertableArray(mp, true));
	CAutoRef<CExpression> apexprInConverted(CExpressionPreprocessor::PexprConvert2In(mp, apexprInConvertable.Value()));

	GPOS_RTL_ASSERT(0 == CUtils::UlCountOperator(apexprInConverted.Value(), COperator::EopScalarCmp));
	GPOS_RTL_ASSERT(7 == CUtils::UlCountOperator(apexprInConverted.Value(), COperator::EopScalarConst));
	GPOS_RTL_ASSERT(1 == CUtils::UlCountOperator(apexprInConverted.Value(), COperator::EopScalarArrayCmp));

	CExpression *pexprArrayInCmp = CTestUtils::PexprFindFirstExpressionWithOpId(apexprInConverted.Value(), COperator::EopScalarArrayCmp);
	GPOS_ASSERT(NULL != pexprArrayInCmp);
	CScalarArrayCmp *popCmpInArray = CScalarArrayCmp::PopConvert(pexprArrayInCmp->Pop());
	GPOS_RTL_ASSERT(CScalarArrayCmp::EarrcmpAny == popCmpInArray->Earrcmpt());

	// test the NOT IN OR NEq variant
	CAutoRef<CExpression> apexprNotInConvertable(PexprCreateConvertableArray(mp, false));
	CAutoRef<CExpression> apexprNotInConverted(CExpressionPreprocessor::PexprConvert2In(mp, apexprNotInConvertable.Value()));

	GPOS_RTL_ASSERT(0 == CUtils::UlCountOperator(apexprNotInConverted.Value(), COperator::EopScalarCmp));
	GPOS_RTL_ASSERT(7 == CUtils::UlCountOperator(apexprNotInConverted.Value(), COperator::EopScalarConst));
	GPOS_RTL_ASSERT(1 == CUtils::UlCountOperator(apexprNotInConverted.Value(), COperator::EopScalarArrayCmp));

	CExpression *pexprArrayCmpNotIn = CTestUtils::PexprFindFirstExpressionWithOpId(apexprNotInConverted.Value(), COperator::EopScalarArrayCmp);
	GPOS_ASSERT(NULL != pexprArrayCmpNotIn);
	CScalarArrayCmp *popCmpNotInArray = CScalarArrayCmp::PopConvert(pexprArrayCmpNotIn->Pop());
	GPOS_RTL_ASSERT(CScalarArrayCmp::EarrcmpAll == popCmpNotInArray->Earrcmpt());

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionPreprocessorTest
//				::EresUnittest_PreProcessConvert2InPredicateDeepExpressionTree
//
//	@doc:
//		Test of preprocessing with a whole expression tree. The expression tree
//		looks like this predicate (x = 1 OR x = 2 OR (x = y AND (y = 3 OR y = 4)))
//		which should be converted to (x in (1,2) OR (x = y AND y IN (3,4)))
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExpressionPreprocessorTest::EresUnittest_PreProcessConvert2InPredicateDeepExpressionTree()
{
	CAutoTraceFlag atf(EopttraceArrayConstraints, true /*value*/);

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// reset metadata cache
	CMDCache::Reset();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	CAutoOptCtxt aoc(mp, &mda, NULL /*pceeval*/, CTestUtils::GetCostModel(mp));

	CAutoRef<CExpression> apexprGet(CTestUtils::PexprLogicalGet(mp));
	COperator *popGet = apexprGet->Pop();
	popGet->AddRef();

	// get a column ref from the outermost Get expression
	CAutoRef<CColRefArray> apdrgpcr(apexprGet->DeriveOutputColumns()->Pdrgpcr(mp));
	GPOS_ASSERT(1 < apdrgpcr->Size());
	CColRef *pcrLeft = (*apdrgpcr)[0];
	CColRef *pcrRight = (*apdrgpcr)[1];

	// inner most OR
	CScalarBoolOp *pscboolopOrInner = GPOS_NEW(mp) CScalarBoolOp(mp, CScalarBoolOp::EboolopOr);
	CExpression *pexprDisjunctInner =
			GPOS_NEW(mp) CExpression(
									mp,
									pscboolopOrInner,
									CUtils::PexprScalarEqCmp(mp, pcrRight, CUtils::PexprScalarConstInt4(mp, 3 /*val*/)),
									CUtils::PexprScalarEqCmp(mp, pcrRight, CUtils::PexprScalarConstInt4(mp, 4 /*val*/))
									);
	// middle and expression
	CScalarBoolOp *pscboolopAnd = GPOS_NEW(mp) CScalarBoolOp(mp, CScalarBoolOp::EboolopAnd);
	CExpression *pexprConjunct =
			GPOS_NEW(mp) CExpression(
									mp,
									pscboolopAnd,
									pexprDisjunctInner,
									CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight)
									);
	// outer most OR
	CScalarBoolOp *pscboolopOr = GPOS_NEW(mp) CScalarBoolOp(mp, CScalarBoolOp::EboolopOr);
	CExpression *pexprDisjunct =
			GPOS_NEW(mp) CExpression(
									mp,
									pscboolopOr,
									CUtils::PexprScalarEqCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 1)),
									CUtils::PexprScalarEqCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 2)),
									pexprConjunct
									);

	CAutoRef<CExpression> apexprGetWithChildren(GPOS_NEW(mp) CExpression(mp, popGet, pexprDisjunct));

	GPOS_ASSERT(5 == CUtils::UlCountOperator(apexprGetWithChildren.Value(), COperator::EopScalarCmp));

	CAutoRef<CExpression> apexprConvert(CExpressionPreprocessor::PexprConvert2In(mp, apexprGetWithChildren.Value()));

	GPOS_ASSERT(2 == CUtils::UlCountOperator(apexprConvert.Value(), COperator::EopScalarArrayCmp));
	GPOS_ASSERT(1 == CUtils::UlCountOperator(apexprConvert.Value(), COperator::EopScalarCmp));

	return GPOS_OK;
}

// EOF

