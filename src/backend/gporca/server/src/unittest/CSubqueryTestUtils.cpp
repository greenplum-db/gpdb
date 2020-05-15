//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSubqueryTestUtils.cpp
//
//	@doc:
//		Implementation of test utility functions
//---------------------------------------------------------------------------

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/ops.h"

#include "unittest/gpopt/CSubqueryTestUtils.h"

#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDTypeBool.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::GenerateGetExpressions
//
//	@doc:
//		Helper for generating a pair of randomized Get expressions
//
//---------------------------------------------------------------------------
void
CSubqueryTestUtils::GenerateGetExpressions
	(
	CMemoryPool *mp,
	CExpression **ppexprOuter,
	CExpression **ppexprInner
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != ppexprOuter);
	GPOS_ASSERT(NULL != ppexprInner);

	// outer expression
	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescCreate(mp, 3 /*num_cols*/, pmdidR, CName(&strNameR));
	*ppexprOuter = CTestUtils::PexprLogicalGet(mp, ptabdescR, &strNameR);

	// inner expression
	CWStringConst strNameS(GPOS_WSZ_LIT("Rel2"));
	CMDIdGPDB *pmdidS = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID2, 1, 1);
	CTableDescriptor *ptabdescS = CTestUtils::PtabdescCreate(mp, 3 /*num_cols*/, pmdidS, CName(&strNameS));
	*ppexprInner = CTestUtils::PexprLogicalGet(mp, ptabdescS, &strNameS);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprJoinWithAggSubquery
//
//	@doc:
//		Generate randomized join expression with a subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprJoinWithAggSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprLeft = NULL;
	CExpression *pexprRight = NULL;
	GenerateGetExpressions(mp, &pexprLeft, &pexprRight);

	CExpression *pexprInner = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSelect = PexprSelectWithAggSubquery(mp, pexprLeft, pexprInner, fCorrelated);

	(*pexprSelect)[0]->AddRef();
	(*pexprSelect)[1]->AddRef();

	CExpression *pexpr = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalInnerJoin(mp),
											  (*pexprSelect)[0],
											  pexprRight,
											  (*pexprSelect)[1]);

	pexprSelect->Release();

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubquery
//
//	@doc:
//		Generate a Select expression with a subquery equality predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubquery
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	// get any column
	CColRefSet *pcrs = pexprOuter->DeriveOutputColumns();
	CColRef *pcrLeft =  pcrs->PcrAny();

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);

	// generate equality predicate
	CExpression *pexprPredicate = CUtils::PexprScalarEqCmp(mp, pcrLeft, pexprSubq);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
//
//	@doc:
//		Generate a Select expression with a subquery equality predicate
//		involving constant
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);

	CExpression *pexprConst = CUtils::PexprScalarConstInt8(mp, 0 /*val*/);

	// generate equality predicate
	CExpression *pexprPredicate = CUtils::PexprScalarEqCmp(mp, pexprConst, pexprSubq);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAggSubquery
//
//	@doc:
//		Generate a Project expression with a subquery equality predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAggSubquery
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);

	// generate a computed column
	CScalarSubquery *popSubquery = CScalarSubquery::PopConvert(pexprSubq->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(popSubquery->MdidType());
	CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, popSubquery->TypeModifier());

	// generate a scalar project list
	CExpression *pexprPrjElem = CUtils::PexprScalarProjectElement(mp, pcrComputed, pexprSubq);
	CExpression *pexprPrjList =
		GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprPrjElem);

	return CUtils::PexprLogicalProject(mp, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubquery
//
//	@doc:
//		Generate randomized Select expression with a subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubquery
	(
	CMemoryPool *mp,
	 BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprSelectWithAggSubquery(mp, pexprOuter, pexprInner, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
//
//	@doc:
//		Generate randomized Select expression with a subquery predicate
//		involving constant
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprSelectWithAggSubqueryConstComparison(mp, pexprOuter, pexprInner, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAggSubquery
//
//	@doc:
//		Generate randomized Project expression with a subquery in project
//		element
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAggSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprProjectWithAggSubquery(mp, pexprOuter, pexprInner, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubqueryOverJoin
//
//	@doc:
//		Generate a random select expression with a subquery over join predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubqueryOverJoin
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	// generate a pair of get expressions
	CExpression *pexprR = NULL;
	CExpression *pexprS = NULL;
	GenerateGetExpressions(mp, &pexprR, &pexprS);

	// generate outer expression
	CWStringConst strNameT(GPOS_WSZ_LIT("Rel3"));

	CMDIdGPDB *pmdidT = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID3, 1, 1);
	CTableDescriptor *ptabdescT = CTestUtils::PtabdescCreate(mp, 3 /*num_cols*/, pmdidT, CName(&strNameT));
	CExpression *pexprT = CTestUtils::PexprLogicalGet(mp, ptabdescT, &strNameT);
	CColRef *pcrInner = pexprR->DeriveOutputColumns()->PcrAny();
	CColRef *pcrOuter = pexprT->DeriveOutputColumns()->PcrAny();

	CExpression *pexprPred = NULL;
	if (fCorrelated)
	{
		// generate correlation predicate
		pexprPred = CUtils::PexprScalarEqCmp(mp, pcrInner, pcrOuter);
	}
	else
	{
		pexprPred = CUtils::PexprScalarConstBool(mp, true /*value*/);
	}

	// generate N-Ary join
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexpr->Append(pexprR);
	pdrgpexpr->Append(pexprS);
	pdrgpexpr->Append(pexprPred);
	CExpression *pexprJoin = CTestUtils::PexprLogicalNAryJoin(mp, pdrgpexpr);

	CExpression *pexprSubq =
		GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubquery(mp, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprJoin);

	CExpression *pexprPredOuter = CUtils::PexprScalarEqCmp(mp, pcrOuter, pexprSubq);
	return CUtils::PexprLogicalSelect(mp, pexprT, pexprPredOuter);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAnySubquery
//
//	@doc:
//		Generate randomized Select expression with Any subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAnySubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantified(mp, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAnySubqueryOverWindow
//
//	@doc:
//		Generate randomized Select expression with Any subquery predicate
//		over window operation
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAnySubqueryOverWindow
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantifiedOverWindow(mp, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryQuantifiedOverWindow
//
//	@doc:
//		Generate randomized Select expression with quantified subquery
//		predicate over window operations
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryQuantifiedOverWindow
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprInner = CTestUtils::PexprOneWindowFunction(mp);
	CExpression *pexprSubqueryQuantified = PexprSubqueryQuantified(mp, op_id, pexprOuter, pexprInner, fCorrelated);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprSubqueryQuantified);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAllSubquery
//
//	@doc:
//		Generate randomized Select expression with All subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAllSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantified(mp, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAllSubqueryOverWindow
//
//	@doc:
//		Generate randomized Select expression with All subquery predicate
//		over window operation
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAllSubqueryOverWindow
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantifiedOverWindow(mp, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAnyAggSubquery
//
//	@doc:
//		Generate randomized Select expression with Any subquery whose inner
//		expression is a GbAgg
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAnyAggSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithQuantifiedAggSubquery(mp, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAllAggSubquery
//
//	@doc:
//		Generate randomized Select expression with All subquery whose inner
//		expression is a GbAgg
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAllAggSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithQuantifiedAggSubquery(mp, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAnySubquery
//
//	@doc:
//		Generate randomized Project expression with Any subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAnySubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprProjectWithSubqueryQuantified(mp, pexprOuter, pexprInner, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAllSubquery
//
//	@doc:
//		Generate randomized Project expression with All subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAllSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprProjectWithSubqueryQuantified(mp, pexprOuter, pexprInner, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueriesInDifferentContexts
//
//	@doc:
//		Generate a randomized expression with subqueries in both value
//		and filter contexts
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueriesInDifferentContexts
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);
	CExpression *pexprSelect = PexprSelectWithAggSubquery(mp, pexprOuter, pexprInner, fCorrelated);

	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	return PexprProjectWithSubqueryQuantified(mp, pexprSelect, pexprGet, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueriesInNullTestContext
//
//	@doc:
//		Generate a randomized expression expression with subquery in null
//		test context
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueriesInNullTestContext
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);

	// generate Is Not Null predicate
	CExpression *pexprPredicate = CUtils::PexprIsNotNull(mp, pexprSubq);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithExistsSubquery
//
//	@doc:
//		Generate randomized Select expression with Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithExistsSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryExistential(mp, COperator::EopScalarSubqueryExists, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNotExistsSubquery
//
//	@doc:
//		Generate randomized Select expression with Not Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNotExistsSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryExistential(mp, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryDisjuncts
//
//	@doc:
//		Generate randomized select expression with subquery predicates in
//		an OR tree
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryDisjuncts
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprSelectWithSubqueryBoolOp(mp, pexprOuter, pexprInner, fCorrelated, CScalarBoolOp::EboolopOr);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithTrimmableExists
//
//	@doc:
//		Generate randomized Select expression with trimmable Exists
//		subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithTrimmableExists
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithTrimmableExistentialSubquery(mp, COperator::EopScalarSubqueryExists, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithTrimmableNotExists
//
//	@doc:
//		Generate randomized Select expression with trimmable Not Exists
//		subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithTrimmableNotExists
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithTrimmableExistentialSubquery(mp, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithExistsSubquery
//
//	@doc:
//		Generate randomized Project expression with Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithExistsSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprProjectWithSubqueryExistential(mp, COperator::EopScalarSubqueryExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithNotExistsSubquery
//
//	@doc:
//		Generate randomized Project expression with Not Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithNotExistsSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprProjectWithSubqueryExistential(mp, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedCmpSubquery
//
//	@doc:
//		Generate randomized Select expression with nested comparisons
//		involving subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedCmpSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprSelectWithSubquery = PexprSelectWithAggSubquery(mp, fCorrelated);

	CExpression *pexprLogical = (*pexprSelectWithSubquery)[0];
	CExpression *pexprSubqueryPred = (*pexprSelectWithSubquery)[1];

	// generate a parent equality predicate
	pexprSubqueryPred->AddRef();
	CExpression *pexprPredicate1 =
		CUtils::PexprScalarEqCmp(mp, CUtils::PexprScalarConstBool(mp, true /*value*/), pexprSubqueryPred);

	// add another nesting level
	CExpression *pexprPredicate =
		CUtils::PexprScalarEqCmp(mp, CUtils::PexprScalarConstBool(mp, true /*value*/), pexprPredicate1);

	pexprLogical->AddRef();
	pexprSelectWithSubquery->Release();

	return CUtils::PexprLogicalSelect(mp, pexprLogical, pexprPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithCmpSubqueries
//
//	@doc:
//		Generate randomized Select expression with comparison between
//		two subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithCmpSubqueries
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	// generate a scalar subquery
	CExpression *pexprScalarSubquery1 = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);

	// generate get expression
	CWStringConst strNameT(GPOS_WSZ_LIT("Rel3"));

	CMDIdGPDB *pmdidT = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID3, 1, 1);
	CTableDescriptor *ptabdescT = CTestUtils::PtabdescCreate(mp, 3 /*num_cols*/, pmdidT, CName(&strNameT));
	CExpression *pexprT = CTestUtils::PexprLogicalGet(mp, ptabdescT, &strNameT);

	// generate another scalar subquery
	CExpression *pexprScalarSubquery2 = PexprSubqueryAgg(mp, pexprOuter, pexprT, fCorrelated);

	// generate equality predicate between both subqueries
	CExpression *pexprPredicate =
		CUtils::PexprScalarEqCmp(mp, pexprScalarSubquery1, pexprScalarSubquery2);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedSubquery
//
//	@doc:
//		Generate randomized Select expression with nested subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprInner = PexprSelectWithAggSubquery(mp, fCorrelated);
	CColRef *pcrInner = pexprInner->DeriveOutputColumns()->PcrAny();

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp);
	CColRef *pcrOuter = pexprOuter->DeriveOutputColumns()->PcrAny();

	CExpression *pexprSubq = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubquery(mp, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprInner);

	CExpression *pexprPred = CUtils::PexprScalarEqCmp(mp, pcrOuter, pexprSubq);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedQuantifiedSubqueries
//
//	@doc:
//		Generate a random select expression with nested quantified subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedQuantifiedSubqueries
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CWStringConst strName1(GPOS_WSZ_LIT("Rel1"));
	CWStringConst strAlias1(GPOS_WSZ_LIT("Rel1Alias"));
	CExpression *pexprOuter1 = CTestUtils::PexprLogicalGetNullable(mp, GPOPT_TEST_REL_OID1, &strName1, &strAlias1);

	CWStringConst strName2(GPOS_WSZ_LIT("Rel2"));
	CWStringConst strAlias2(GPOS_WSZ_LIT("Rel2Alias"));
	CExpression *pexprOuter2 = CTestUtils::PexprLogicalGetNullable(mp, GPOPT_TEST_REL_OID2, &strName2, &strAlias2);

	CWStringConst strName3(GPOS_WSZ_LIT("Rel3"));
	CWStringConst strAlias3(GPOS_WSZ_LIT("Rel3Alias"));
	CExpression *pexprOuter3 = CTestUtils::PexprLogicalGetNullable(mp, GPOPT_TEST_REL_OID3, &strName3, &strAlias3);

	CWStringConst strName4(GPOS_WSZ_LIT("Rel4"));
	CWStringConst strAlias4(GPOS_WSZ_LIT("Rel4Alias"));
	CExpression *pexprInner = CTestUtils::PexprLogicalGetNullable(mp, GPOPT_TEST_REL_OID4, &strName4, &strAlias4);

	CExpression *pexprSubqueryQuantified1 = PexprSubqueryQuantified(mp, op_id, pexprOuter3, pexprInner, fCorrelated);
	CExpression *pexprSelect1 = CUtils::PexprLogicalSelect(mp, pexprOuter3, pexprSubqueryQuantified1);
	CExpression *pexprSubqueryQuantified2 = PexprSubqueryQuantified(mp, op_id, pexprOuter2, pexprSelect1, fCorrelated);
	CExpression *pexprSelect2 = CUtils::PexprLogicalSelect(mp, pexprOuter2, pexprSubqueryQuantified2);
	CExpression *pexprSubqueryQuantified3 = PexprSubqueryQuantified(mp, op_id, pexprOuter1, pexprSelect2, fCorrelated);

	return CUtils::PexprLogicalSelect(mp, pexprOuter1, pexprSubqueryQuantified3);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedAnySubqueries
//
//	@doc:
//		Generate a random select expression with nested Any subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedAnySubqueries
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithNestedQuantifiedSubqueries(mp, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedAllSubqueries
//
//	@doc:
//		Generate a random select expression with nested All subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedAllSubqueries
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprSelectWithNestedQuantifiedSubqueries(mp, COperator::EopScalarSubqueryAll, fCorrelated);
}



//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWith2LevelsCorrSubquery
//
//	@doc:
//		Generate randomized select expression with 2-levels correlated subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWith2LevelsCorrSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexpr = PexprSelectWithNestedSubquery(mp, fCorrelated);
	if (fCorrelated)
	{
		// add a 2-level correlation
		CExpression *pexprOuterSubq = (*(*pexpr)[1])[1];
		CExpression *pexprInnerSubq = (*(*(*pexprOuterSubq)[0])[1])[1];
		CExpression *pexprInnerSelect = (*(*pexprInnerSubq)[0])[0];
		CExpressionArray *pdrgpexpr = (*pexprInnerSelect)[1]->PdrgPexpr();

		CColRef *pcrOuter = pexpr->DeriveOutputColumns()->PcrAny();
		CColRef *pcrInner = pexprInnerSelect->DeriveOutputColumns()->PcrAny();
		CExpression *pexprPred = CUtils::PexprScalarEqCmp(mp, pcrOuter, pcrInner);
		pdrgpexpr->Append(pexprPred);
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryConjuncts
//
//	@doc:
//		Generate randomized select expression with subquery predicates in
//		an AND tree
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryConjuncts
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprSelectWithSubqueryBoolOp(mp, pexprOuter, pexprInner, fCorrelated, CScalarBoolOp::EboolopAnd);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueries
//
//	@doc:
//		Generate randomized project expression with multiple subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueries
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);

	return PexprProjectWithSubqueries(mp, pexprOuter, pexprInner, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubquery
//
//	@doc:
//		Helper for generating a randomized Select expression with correlated
//		predicates to be used for building subquery examples:
//
//			SELECT inner_column
//			FROM inner_expression
//			WHERE inner_column = 5 [AND outer_column = inner_column]
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubquery
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	// get a random column from inner expression
	CColRef *pcrInner = pexprInner->DeriveOutputColumns()->PcrAny();

	// generate a non-correlated predicate to be added to inner expression
	CExpression *pexprNonCorrelated = CUtils::PexprScalarEqCmp(mp, pcrInner, CUtils::PexprScalarConstInt4(mp, 5 /*val*/));

	// predicate for the inner expression
	CExpression *pexprPred = NULL;
	if (fCorrelated)
	{
		// get a random column from outer expression
		CColRef *pcrOuter = pexprOuter->DeriveOutputColumns()->PcrAny();

		// generate correlated predicate
		CExpression *pexprCorrelated = CUtils::PexprScalarEqCmp(mp, pcrOuter, pcrInner);

		// generate AND expression of correlated and non-correlated predicates
		CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
		pdrgpexpr->Append(pexprCorrelated);
		pdrgpexpr->Append(pexprNonCorrelated);
		pexprPred = CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopAnd, pdrgpexpr);
	}
	else
	{
		pexprPred = pexprNonCorrelated;
	}

	// generate a select on top of inner expression
	return CUtils::PexprLogicalSelect(mp, pexprInner, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryQuantified
//
//	@doc:
//		Generate a quantified subquery expression
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryQuantified
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id);
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CExpression *pexprSelect = PexprSubquery(mp, pexprOuter, pexprInner, fCorrelated);

	// get random columns from inner expression
	CColRefSet *pcrs = pexprInner->DeriveOutputColumns();
	const CColRef *pcrInner = pcrs->PcrAny();

	// get random columns from outer expression
	pcrs = pexprOuter->DeriveOutputColumns();
	const CColRef *pcrOuter = pcrs->PcrAny();

	// return a quantified subquery expression
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		const CWStringConst *str = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("="));
		return GPOS_NEW(mp) CExpression
			(
			mp,
			GPOS_NEW(mp) CScalarSubqueryAny(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
			pexprSelect,
			CUtils::PexprScalarIdent(mp, pcrOuter)
			);
	}

	const CWStringConst *str = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("<>"));
	return GPOS_NEW(mp) CExpression
			(
			mp,
			GPOS_NEW(mp) CScalarSubqueryAll(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_NEQ_OP), str, pcrInner),
			pexprSelect,
			CUtils::PexprScalarIdent(mp, pcrOuter)
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable quantified subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableSubquery
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id ||
				COperator::EopScalarSubqueryExists == op_id ||
				COperator::EopScalarSubqueryNotExists == op_id);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1 /*version_major*/, 1 /*version_minor*/);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescPlain(mp, 3 /*num_cols*/, pmdidR, CName(&strNameR));
	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp, ptabdescR, &strNameR);

	// generate quantified subquery predicate
	CExpression *pexprInner = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSubquery = NULL;
	switch (op_id)
	{
		case COperator::EopScalarSubqueryAny:
		case COperator::EopScalarSubqueryAll:
			pexprSubquery = PexprSubqueryQuantified(mp, op_id, pexprOuter, pexprInner, fCorrelated);
			break;

		case COperator::EopScalarSubqueryExists:
		case COperator::EopScalarSubqueryNotExists:
			pexprSubquery = PexprSubqueryExistential(mp, op_id, pexprOuter, pexprInner, fCorrelated);
			break;

		default:
			GPOS_ASSERT(!"Invalid subquery type");
	}

	// generate a regular predicate
	CColRef *pcrOuter = pexprOuter->DeriveOutputColumns()->PcrAny();
	CExpression *pexprPred = CUtils::PexprScalarEqCmp(mp, pcrOuter, CUtils::PexprScalarConstInt4(mp, 5 /*val*/));

	// generate OR expression of  predicates
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexpr->Append(pexprSubquery);
	pdrgpexpr->Append(pexprPred);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, CPredicateUtils::PexprDisjunction(mp, pdrgpexpr));
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableAnySubquery
//
//	@doc:
//		Generate an expression with undecorrelatable ANY subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableAnySubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(mp, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableAllSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable ALL subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableAllSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(mp, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableExistsSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable Exists subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableExistsSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(mp, COperator::EopScalarSubqueryExists, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableNotExistsSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable Not Exists subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableNotExistsSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(mp, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableScalarSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable Scalar subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableScalarSubquery
	(
	CMemoryPool *mp,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprInner = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSelect = PexprSubquery(mp, pexprOuter, pexprInner, fCorrelated);

	// get a random column from inner expression
	CColRefSet *pcrs = pexprInner->DeriveOutputColumns();
	CColRef *pcrInner =  pcrs->PcrAny();

	CExpression *pexprSubquery = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubquery(mp, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprSelect);

	CColRef *pcrOuter = pexprOuter->DeriveOutputColumns()->PcrAny();
	CExpression *pexprPred = CUtils::PexprScalarEqCmp(mp, pcrOuter, pexprSubquery);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryExistential
//
//	@doc:
//		Generate an EXISTS/NOT EXISTS subquery expression
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryExistential
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id ||
				COperator::EopScalarSubqueryNotExists == op_id);
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CExpression *pexprSelect = PexprSubquery(mp, pexprOuter, pexprInner, fCorrelated);

	// return a quantified subquery expression
	if (COperator::EopScalarSubqueryExists == op_id)
	{
		return GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubqueryExists(mp), pexprSelect);
	}

	return GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubqueryNotExists(mp), pexprSelect);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryAgg
//
//	@doc:
//		Generate a randomized ScalarSubquery aggregate expression for
//		the following query:
//
//			SELECT sum(inner_column)
//			FROM inner_expression
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryAgg
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CExpression *pexprSelect = PexprSubquery(mp, pexprOuter, pexprInner, fCorrelated);

	// get a random column from inner expression
	CColRefSet *pcrs = pexprInner->DeriveOutputColumns();
	CColRef *pcrInner =  pcrs->PcrAny();

	// generate a SUM expression
	CExpression *pexprProjElem = CTestUtils::PexprPrjElemWithSum(mp, pcrInner);
	CColRef *pcrComputed = CScalarProjectElement::PopConvert(pexprProjElem->Pop())->Pcr();

	// add SUM expression to a project list
	CExpression *pexprProjList = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprProjElem);

	// generate empty grouping columns list
	CColRefArray *colref_array = GPOS_NEW(mp) CColRefArray(mp);

	// generate a group by on top of select expression
	CExpression *pexprLogicalGbAgg = CUtils::PexprLogicalGbAggGlobal(mp, colref_array, pexprSelect, pexprProjList);

	// return a subquery expression on top of group by
	return GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubquery(mp, pcrComputed, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprLogicalGbAgg);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryBoolOp
//
//	@doc:
//		Generate a Select expression with a BoolOp (AND/OR) predicate tree involving
//		subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryBoolOp
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated,
	CScalarBoolOp::EBoolOperator eboolop
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	GPOS_ASSERT(CScalarBoolOp::EboolopAnd == eboolop || CScalarBoolOp::EboolopOr == eboolop);

	// get any two columns
	CColRefSet *pcrs = pexprOuter->DeriveOutputColumns();
	CColRef *pcrLeft = pcrs->PcrAny();

	// generate agg subquery
	CExpression *pexprAggSubquery = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);

	// generate equality predicate involving a subquery
	CExpression *pexprPred1 = CUtils::PexprScalarEqCmp(mp, pcrLeft, pexprAggSubquery);

	// generate a regular predicate
	CExpression *pexprPred2 = CUtils::PexprScalarEqCmp(mp, pcrLeft, CUtils::PexprScalarConstInt4(mp, 5 /*val*/));

	// generate ALL subquery
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSubqueryAll = PexprSubqueryQuantified(mp, COperator::EopScalarSubqueryAll, pexprOuter, pexprGet, fCorrelated);

	// generate EXISTS subquery
	CExpression *pexprGet2 = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSubqueryExists = PexprSubqueryExistential(mp, COperator::EopScalarSubqueryExists, pexprOuter, pexprGet2, fCorrelated);

	// generate AND expression of all predicates
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexpr->Append(pexprPred1);
	pdrgpexpr->Append(pexprPred2);
	pdrgpexpr->Append(pexprSubqueryExists);
	pdrgpexpr->Append(pexprSubqueryAll);

	CExpression *pexprPred = CUtils::PexprScalarBoolOp(mp, eboolop, pdrgpexpr);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueries
//
//	@doc:
//		Generate a Project expression with multiple subqueries in project list
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueries
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate an array of project elements holding subquery expressions
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);

	CColRef *pcrComputed = NULL;
	CExpression *pexprPrjElem = NULL;
	CExpression *pexprGet = NULL;

	const IMDTypeBool *pmdtypebool = md_accessor->PtMDType<IMDTypeBool>();

	// generate agg subquery
	CExpression *pexprAggSubquery = PexprSubqueryAgg(mp, pexprOuter, pexprInner, fCorrelated);
	const CColRef *colref = CScalarSubquery::PopConvert(pexprAggSubquery->Pop())->Pcr();
	pcrComputed = col_factory->PcrCreate(colref->RetrieveType(), colref->TypeModifier());
	pexprPrjElem = CUtils::PexprScalarProjectElement(mp, pcrComputed, pexprAggSubquery);
	pdrgpexpr->Append(pexprPrjElem);

	// generate ALL subquery
	pexprGet = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSubqueryAll = PexprSubqueryQuantified(mp, COperator::EopScalarSubqueryAll, pexprOuter, pexprGet, fCorrelated);
	pcrComputed = col_factory->PcrCreate(pmdtypebool, default_type_modifier);
	pexprPrjElem = CUtils::PexprScalarProjectElement(mp, pcrComputed, pexprSubqueryAll);
	pdrgpexpr->Append(pexprPrjElem);

	// generate existential subquery
	pexprGet =CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSubqueryExists = PexprSubqueryExistential(mp, COperator::EopScalarSubqueryExists, pexprOuter, pexprGet, fCorrelated);
	pcrComputed = col_factory->PcrCreate(pmdtypebool, default_type_modifier);
	pexprPrjElem = CUtils::PexprScalarProjectElement(mp, pcrComputed, pexprSubqueryExists);
	pdrgpexpr->Append(pexprPrjElem);

	CExpression *pexprPrjList = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp), pdrgpexpr);

	return CUtils::PexprLogicalProject(mp, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryQuantified
//
//	@doc:
//		Generate randomized Select expression with quantified subquery
//		predicate
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryQuantified
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);
	CExpression *pexprSubqueryQuantified = PexprSubqueryQuantified(mp, op_id, pexprOuter, pexprInner, fCorrelated);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprSubqueryQuantified);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithQuantifiedAggSubquery
//
//	@doc:
//		Generate randomized Select expression with quantified subquery
//		whose inner expression is an aggregate
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithQuantifiedAggSubquery
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprSubq = PexprSubqueryAgg(mp, pexprOuter, CTestUtils::PexprLogicalGet(mp), fCorrelated);
	CExpression *pexprGb = (*pexprSubq)[0];
	pexprGb->AddRef();
	pexprSubq->Release();

	CColRef *pcrInner = pexprGb->DeriveOutputColumns()->PcrAny();
	CColRef *pcrOuter = pexprOuter->DeriveOutputColumns()->PcrAny();
	CExpression *pexprSubqueryQuantified = NULL;
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		const CWStringConst *str = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("="));
		pexprSubqueryQuantified = GPOS_NEW(mp) CExpression
			(
			mp,
			GPOS_NEW(mp) CScalarSubqueryAny(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
			pexprGb,
			CUtils::PexprScalarIdent(mp, pcrOuter)
			);
	}

	const CWStringConst *str = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("<>"));
	pexprSubqueryQuantified = GPOS_NEW(mp) CExpression
			(
			mp,
			GPOS_NEW(mp) CScalarSubqueryAll(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_NEQ_OP), str, pcrInner),
			pexprGb,
			CUtils::PexprScalarIdent(mp, pcrOuter)
			);


	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprSubqueryQuantified);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueryQuantified
//
//	@doc:
//		Generate a randomized Project expression with a quantified subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueryQuantified
	(
	CMemoryPool *mp,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprSubqueryQuantified = PexprSubqueryQuantified(mp, op_id, pexprOuter, pexprInner, fCorrelated);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate a computed column
	CScalarSubqueryQuantified *pop = CScalarSubqueryQuantified::PopConvert(pexprSubqueryQuantified->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(pop->MdidType());
	CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, pop->TypeModifier());

	// generate a scalar project list
	CExpression *pexprPrjElem =  CUtils::PexprScalarProjectElement(mp, pcrComputed, pexprSubqueryQuantified);
	CExpression *pexprPrjList = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprPrjElem);

	return CUtils::PexprLogicalProject(mp, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryExistential
//
//	@doc:
//		Generate randomized Select expression with existential subquery
//		predicate
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryExistential
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id || COperator::EopScalarSubqueryNotExists == op_id);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);
	CExpression *pexprSubqueryExistential = PexprSubqueryExistential(mp, op_id, pexprOuter, pexprInner, fCorrelated);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprSubqueryExistential);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithTrimmableExistentialSubquery
//
//	@doc:
//		Generate randomized Select expression with existential subquery
//		predicate that can be trimmed
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithTrimmableExistentialSubquery
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL // fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id || COperator::EopScalarSubqueryNotExists == op_id);

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp);
	CExpression *pexprInner = CTestUtils::PexprLogicalGbAggWithSum(mp);

	// remove grouping columns
	(*pexprInner)[0]->AddRef();
	(*pexprInner)[1]->AddRef();
	CExpression *pexprGbAgg =
		CUtils::PexprLogicalGbAggGlobal(mp, GPOS_NEW(mp) CColRefArray(mp), (*pexprInner)[0], (*pexprInner)[1]);
	pexprInner->Release();

	// create existential subquery
	CExpression *pexprSubqueryExistential = NULL;
	if (COperator::EopScalarSubqueryExists == op_id)
	{
		pexprSubqueryExistential = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubqueryExists(mp), pexprGbAgg);
	}
	else
	{
		pexprSubqueryExistential = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarSubqueryNotExists(mp), pexprGbAgg);
	}

	// generate a regular predicate
	CColRefSet *pcrs = pexprOuter->DeriveOutputColumns();
	CExpression *pexprEqPred = CUtils::PexprScalarEqCmp(mp, pcrs->PcrAny(), CUtils::PexprScalarConstInt4(mp, 5 /*val*/));

	CExpression *pexprConjunction =
		CPredicateUtils::PexprConjunction(mp, pexprSubqueryExistential, pexprEqPred);
	pexprEqPred->Release();
	pexprSubqueryExistential->Release();

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprConjunction);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueryExistential
//
//	@doc:
//		Generate randomized Project expression with existential subquery
//
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueryExistential
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id || COperator::EopScalarSubqueryNotExists == op_id);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(mp, &pexprOuter, &pexprInner);
	CExpression *pexprSubqueryExistential = PexprSubqueryExistential(mp, op_id, pexprOuter, pexprInner, fCorrelated);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate a computed column
	CScalarSubqueryExistential *pop = CScalarSubqueryExistential::PopConvert(pexprSubqueryExistential->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(pop->MdidType());
	CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, pop->TypeModifier());

	// generate a scalar project list
	CExpression *pexprPrjElem = CUtils::PexprScalarProjectElement(mp, pcrComputed, pexprSubqueryExistential);
	CExpression *pexprPrjList = GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprPrjElem);

	return CUtils::PexprLogicalProject(mp, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryWithConstTableGet
//
//	@doc:
//		Generate Select expression with Any subquery predicate over a const
//		table get
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryWithConstTableGet
	(
	CMemoryPool *mp,
	COperator::EOperatorId op_id
	)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescCreate(mp, 3 /*num_cols*/, pmdidR, CName(&strNameR));

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp, ptabdescR, &strNameR);
	CExpression *pexprConstTableGet = CTestUtils::PexprConstTableGet(mp, 3 /* ulElements */);

	// get random columns from inner expression
	CColRefSet *pcrs = pexprConstTableGet->DeriveOutputColumns();
	const CColRef *pcrInner = pcrs->PcrAny();

	// get random columns from outer expression
	pcrs = pexprOuter->DeriveOutputColumns();
	const CColRef *pcrOuter = pcrs->PcrAny();

	const CWStringConst *str = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("="));

	CExpression *pexprSubquery = NULL;
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		// construct ANY subquery expression
		pexprSubquery = GPOS_NEW(mp) CExpression
									(
									mp,
									GPOS_NEW(mp) CScalarSubqueryAny(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(mp, pcrOuter)
									);
	}
	else
	{
		// construct ALL subquery expression
		pexprSubquery = GPOS_NEW(mp) CExpression
									(
									mp,
									GPOS_NEW(mp) CScalarSubqueryAll(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(mp, pcrOuter)
									);

	}

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprSubquery);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryWithDisjunction
//
//	@doc:
//		Generate Select expression with a disjunction of two Any subqueries over
//		const table get
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryWithDisjunction
	(
	CMemoryPool *mp
	)
{
	GPOS_ASSERT(NULL != mp);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescCreate(mp, 3 /*num_cols*/, pmdidR, CName(&strNameR));

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(mp, ptabdescR, &strNameR);

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);

	for (int i=0; i<2; i++)
	{
		CExpression *pexprConstTableGet = CTestUtils::PexprConstTableGet(mp, 3 /* ulElements */);
		// get random columns from inner expression
		CColRefSet *pcrs = pexprConstTableGet->DeriveOutputColumns();
		const CColRef *pcrInner = pcrs->PcrAny();

		// get random columns from outer expression
		pcrs = pexprOuter->DeriveOutputColumns();
		const CColRef *pcrOuter = pcrs->PcrAny();

		const CWStringConst *str = GPOS_NEW(mp) CWStringConst(GPOS_WSZ_LIT("="));

		CExpression *pexprSubquery = GPOS_NEW(mp) CExpression
										(
										mp,
										GPOS_NEW(mp) CScalarSubqueryAny(mp, GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
										pexprConstTableGet,
										CUtils::PexprScalarIdent(mp, pcrOuter)
										);
		pdrgpexpr->Append(pexprSubquery);
	}



	// generate a disjunction of the subquery with itself
	CExpression *pexprBoolOp = CUtils::PexprScalarBoolOp(mp, CScalarBoolOp::EboolopOr, pdrgpexpr);

	return CUtils::PexprLogicalSelect(mp, pexprOuter, pexprBoolOp);
}

// EOF
