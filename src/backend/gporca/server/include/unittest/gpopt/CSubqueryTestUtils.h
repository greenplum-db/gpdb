//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSubqueryTestUtils.h
//
//	@doc:
//		Optimizer test utility functions for tests requiring subquery
//		expressions
//---------------------------------------------------------------------------
#ifndef GPOPT_CSubqueryTestUtils_H
#define GPOPT_CSubqueryTestUtils_H

#include "gpos/base.h"
#include "unittest/gpopt/CTestUtils.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CSubqueryTestUtils
//
//	@doc:
//		Test utility functions for tests requiring subquery expressions
//
//---------------------------------------------------------------------------
class CSubqueryTestUtils
{
public:
	//-------------------------------------------------------------------
	// Helpers for generating expressions
	//-------------------------------------------------------------------

	// helper to generate a pair of random Get expressions
	static void GenerateGetExpressions(CMemoryPool *mp,
									   CExpression **ppexprOuter,
									   CExpression **ppexprInner);

	// generate a random join expression with a subquery predicate
	static CExpression *PexprJoinWithAggSubquery(CMemoryPool *mp,
												 BOOL fCorrelated);

	// generate a select  expression with subquery equality predicate
	static CExpression *PexprSelectWithAggSubquery(CMemoryPool *mp,
												   CExpression *pexprOuter,
												   CExpression *pexprInner,
												   BOOL fCorrelated);

	// generate a Select expression with a subquery equality predicate involving constant
	static CExpression *PexprSelectWithAggSubqueryConstComparison(
		CMemoryPool *mp, CExpression *pexprOuter, CExpression *pexprInner,
		BOOL fCorrelated);

	// generate a project  expression with subquery equality predicate
	static CExpression *PexprProjectWithAggSubquery(CMemoryPool *mp,
													CExpression *pexprOuter,
													CExpression *pexprInner,
													BOOL fCorrelated);

	// generate a random select expression with a subquery predicate
	static CExpression *PexprSelectWithAggSubquery(CMemoryPool *mp,
												   BOOL fCorrelated);

	// generate a random select expression with a subquery predicate involving constant
	static CExpression *PexprSelectWithAggSubqueryConstComparison(
		CMemoryPool *mp, BOOL fCorrelated);

	// generate a random project expression with a subquery in project element
	static CExpression *PexprProjectWithAggSubquery(CMemoryPool *mp,
													BOOL fCorrelated);

	// generate a random select expression with a subquery over join predicate
	static CExpression *PexprSelectWithAggSubqueryOverJoin(CMemoryPool *mp,
														   BOOL fCorrelated);

	// generate a random select expression with ANY subquery predicate
	static CExpression *PexprSelectWithAnySubquery(CMemoryPool *mp,
												   BOOL fCorrelated);

	// generate a random select expression with ANY subquery predicate over window operation
	static CExpression *PexprSelectWithAnySubqueryOverWindow(CMemoryPool *mp,
															 BOOL fCorrelated);

	// generate a random select expression with Any subquery whose inner expression is a GbAgg
	static CExpression *PexprSelectWithAnyAggSubquery(CMemoryPool *mp,
													  BOOL fCorrelated);

	// generate a random project expression with ANY subquery
	static CExpression *PexprProjectWithAnySubquery(CMemoryPool *mp,
													BOOL fCorrelated);

	// generate a random select expression with ALL subquery predicate
	static CExpression *PexprSelectWithAllSubquery(CMemoryPool *mp,
												   BOOL fCorrelated);

	// generate a random select expression with ALL subquery predicate over window operation
	static CExpression *PexprSelectWithAllSubqueryOverWindow(CMemoryPool *mp,
															 BOOL fCorrelated);

	// generate a random select expression with Any subquery whose inner expression is a GbAgg
	static CExpression *PexprSelectWithAllAggSubquery(CMemoryPool *mp,
													  BOOL fCorrelated);

	// generate a random project expression with ALL subquery
	static CExpression *PexprProjectWithAllSubquery(CMemoryPool *mp,
													BOOL fCorrelated);

	// generate a random select expression with EXISTS subquery predicate
	static CExpression *PexprSelectWithExistsSubquery(CMemoryPool *mp,
													  BOOL fCorrelated);

	// generate a random select expression with trimmable EXISTS subquery predicate
	static CExpression *PexprSelectWithTrimmableExists(CMemoryPool *mp,
													   BOOL fCorrelated);

	// generate a random select expression with trimmable NOT EXISTS subquery predicate
	static CExpression *PexprSelectWithTrimmableNotExists(CMemoryPool *mp,
														  BOOL fCorrelated);

	// generate a random select expression with NOT EXISTS subquery predicate
	static CExpression *PexprSelectWithNotExistsSubquery(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate a random project expression with EXISTS subquery
	static CExpression *PexprProjectWithExistsSubquery(CMemoryPool *mp,
													   BOOL fCorrelated);

	// generate a random project expression with NOT EXISTS subquery
	static CExpression *PexprProjectWithNotExistsSubquery(CMemoryPool *mp,
														  BOOL fCorrelated);

	// generate a random select expression with nested comparisons involving subqueries
	static CExpression *PexprSelectWithNestedCmpSubquery(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate randomized select expression with comparison between two subqueries
	static CExpression *PexprSelectWithCmpSubqueries(CMemoryPool *mp,
													 BOOL fCorrelated);

	// generate a random select expression with nested agg subqueries
	static CExpression *PexprSelectWithNestedSubquery(CMemoryPool *mp,
													  BOOL fCorrelated);

	// generate a random select expression with nested All subqueries
	static CExpression *PexprSelectWithNestedAllSubqueries(CMemoryPool *mp,
														   BOOL fCorrelated);

	// generate a random select expression with nested Any subqueries
	static CExpression *PexprSelectWithNestedAnySubqueries(CMemoryPool *mp,
														   BOOL fCorrelated);

	// generate a random select expression with 2-levels correlated subqueries
	static CExpression *PexprSelectWith2LevelsCorrSubquery(CMemoryPool *mp,
														   BOOL fCorrelated);

	// generate a random select expression with subquery predicates in an AND tree
	static CExpression *PexprSelectWithSubqueryConjuncts(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate a random select expression with subquery predicates in an OR tree
	static CExpression *PexprSelectWithSubqueryDisjuncts(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate a random project expression with subqueries
	static CExpression *PexprProjectWithSubqueries(CMemoryPool *mp,
												   BOOL fCorrelated);

	// generate a randomized Select expression to be used for building subquery examples
	static CExpression *PexprSubquery(CMemoryPool *mp, CExpression *pexprOuter,
									  CExpression *pexprInner,
									  BOOL fCorrelated);

	// generate a quantified subquery expression
	static CExpression *PexprSubqueryQuantified(CMemoryPool *mp,
												COperator::EOperatorId op_id,
												CExpression *pexprOuter,
												CExpression *pexprInner,
												BOOL fCorrelated);

	// generate quantified subquery expression over window operations
	static CExpression *PexprSelectWithSubqueryQuantifiedOverWindow(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate existential subquery expression
	static CExpression *PexprSubqueryExistential(CMemoryPool *mp,
												 COperator::EOperatorId op_id,
												 CExpression *pexprOuter,
												 CExpression *pexprInner,
												 BOOL fCorrelated);

	// generate a ScalarSubquery aggregate expression
	static CExpression *PexprSubqueryAgg(CMemoryPool *mp,
										 CExpression *pexprOuter,
										 CExpression *pexprInner,
										 BOOL fCorrelated);

	// generate a random select expression with nested quantified subqueries
	static CExpression *PexprSelectWithNestedQuantifiedSubqueries(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate randomized quantified subquery expression
	static CExpression *PexprSelectWithSubqueryQuantified(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate randomized quantified subquery expression whose inner expression is a Gb
	static CExpression *PexprSelectWithQuantifiedAggSubquery(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate randomized Project expression with quantified subquery
	static CExpression *PexprProjectWithSubqueryQuantified(
		CMemoryPool *mp, CExpression *pexprOuter, CExpression *pexprInner,
		COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate randomized Select with existential subquery expression
	static CExpression *PexprSelectWithSubqueryExistential(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate randomized Select expression with existential subquery predicate that can be trimmed
	static CExpression *PexprSelectWithTrimmableExistentialSubquery(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate randomized Project with existential subquery expression
	static CExpression *PexprProjectWithSubqueryExistential(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate a Select expression with an AND predicate tree involving subqueries
	static CExpression *PexprSelectWithSubqueryBoolOp(
		CMemoryPool *mp, CExpression *pexprOuter, CExpression *pexprInner,
		BOOL fCorrelated, CScalarBoolOp::EBoolOperator);

	// generate a Project expression with multiple subqueries
	static CExpression *PexprProjectWithSubqueries(CMemoryPool *mp,
												   CExpression *pexprOuter,
												   CExpression *pexprInner,
												   BOOL fCorrelated);

	// generate a Select expression with ANY/ALL subquery predicate over a const table get
	static CExpression *PexprSubqueryWithConstTableGet(
		CMemoryPool *mp, COperator::EOperatorId op_id);

	// generate a Select expression with a disjunction of two ANY subqueries
	static CExpression *PexprSubqueryWithDisjunction(CMemoryPool *mp);

	// generate an expression with subquery in null test context
	static CExpression *PexprSubqueriesInNullTestContext(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate an expression with subqueries in both value and filter contexts
	static CExpression *PexprSubqueriesInDifferentContexts(CMemoryPool *mp,
														   BOOL fCorrelated);

	// generate an expression with undecorrelatable quantified subquery
	static CExpression *PexprUndecorrelatableSubquery(
		CMemoryPool *mp, COperator::EOperatorId op_id, BOOL fCorrelated);

	// generate an expression with undecorrelatable ANY subquery
	static CExpression *PexprUndecorrelatableAnySubquery(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate an expression with undecorrelatable ALL subquery
	static CExpression *PexprUndecorrelatableAllSubquery(CMemoryPool *mp,
														 BOOL fCorrelated);

	// generate an expression with undecorrelatable exists subquery
	static CExpression *PexprUndecorrelatableExistsSubquery(CMemoryPool *mp,
															BOOL fCorrelated);

	// generate an expression with undecorrelatable NotExists subquery
	static CExpression *PexprUndecorrelatableNotExistsSubquery(
		CMemoryPool *mp, BOOL fCorrelated);

	// generate an expression with undecorrelatable Scalar subquery
	static CExpression *PexprUndecorrelatableScalarSubquery(CMemoryPool *mp,
															BOOL fCorrelated);

};	// class CSubqueryTestUtils

}  // namespace gpopt

#endif	// !GPOPT_CSubqueryTestUtils_H

// EOF
