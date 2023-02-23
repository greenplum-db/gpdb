//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalSelect.cpp
//
//	@doc:
//		Implementation of select operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalSelect.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSelect::CLogicalSelect
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalSelect::CLogicalSelect(CMemoryPool *mp)
	: CLogicalUnary(mp), m_ptabdesc(nullptr)
{
	m_phmPexprPartPred = GPOS_NEW(mp) ExprPredToExprPredPartMap(mp);
}

CLogicalSelect::CLogicalSelect(CMemoryPool *mp, CTableDescriptor *ptabdesc)
	: CLogicalUnary(mp), m_ptabdesc(ptabdesc)
{
	m_phmPexprPartPred = GPOS_NEW(mp) ExprPredToExprPredPartMap(mp);
}

CLogicalSelect::~CLogicalSelect()
{
	m_phmPexprPartPred->Release();
}
//---------------------------------------------------------------------------
//	@function:
//		CLogicalSelect::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalSelect::DeriveOutputColumns(CMemoryPool *,	// mp
									CExpressionHandle &exprhdl)
{
	return PcrsDeriveOutputPassThru(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSelect::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalSelect::DeriveKeyCollection(CMemoryPool *,	// mp
									CExpressionHandle &exprhdl) const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSelect::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalSelect::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfSelect2Apply);
	(void) xform_set->ExchangeSet(CXform::ExfRemoveSubqDistinct);
	(void) xform_set->ExchangeSet(CXform::ExfInlineCTEConsumerUnderSelect);
	(void) xform_set->ExchangeSet(CXform::ExfPushGbWithHavingBelowJoin);
	(void) xform_set->ExchangeSet(CXform::ExfSelect2IndexGet);
	(void) xform_set->ExchangeSet(CXform::ExfSelect2DynamicIndexGet);
	(void) xform_set->ExchangeSet(CXform::ExfSelect2BitmapBoolOp);
	(void) xform_set->ExchangeSet(CXform::ExfSelect2DynamicBitmapBoolOp);
	(void) xform_set->ExchangeSet(CXform::ExfSimplifySelectWithSubquery);
	(void) xform_set->ExchangeSet(CXform::ExfSelect2Filter);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSelect::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalSelect::DeriveMaxCard(CMemoryPool *,  // mp
							  CExpressionHandle &exprhdl) const
{
	// in case of a false condition or a contradiction, maxcard should be zero
	CExpression *pexprScalar = exprhdl.PexprScalarRepChild(1);
	if ((nullptr != pexprScalar &&
		 (CUtils::FScalarConstFalse(pexprScalar) ||
		  CUtils::FScalarConstBoolNull(pexprScalar))) ||
		exprhdl.DerivePropertyConstraint()->FContradiction())
	{
		return CMaxCard(0 /*ull*/);
	}

	// pass on max card of first child
	return exprhdl.DeriveMaxCard(0);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSelect::PstatsDerive
//
//	@doc:
//		Derive statistics based on filter predicates
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalSelect::PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 IStatisticsArray *stats_ctxt) const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);
	IStatistics *child_stats = exprhdl.Pstats(0);

	if (exprhdl.DeriveHasSubquery(1))
	{
		// in case of subquery in select predicate, we return child stats
		child_stats->AddRef();
		return child_stats;
	}

	// remove implied predicates from selection condition to avoid cardinality under-estimation
	CExpression *pexprScalar = exprhdl.PexprScalarRepChild(1 /*child_index*/);
	CExpression *pexprPredicate =
		CPredicateUtils::PexprRemoveImpliedConjuncts(mp, pexprScalar, exprhdl);


	// split selection predicate into local predicate and predicate involving outer references
	CExpression *local_expr = nullptr;
	CExpression *expr_with_outer_refs = nullptr;

	// get outer references from expression handle
	CColRefSet *outer_refs = exprhdl.DeriveOuterReferences();

	CPredicateUtils::SeparateOuterRefs(mp, pexprPredicate, outer_refs,
									   &local_expr, &expr_with_outer_refs);
	pexprPredicate->Release();

	IStatistics *stats = CFilterStatsProcessor::MakeStatsFilterForScalarExpr(
		mp, exprhdl, child_stats, local_expr, expr_with_outer_refs, stats_ctxt);
	local_expr->Release();
	expr_with_outer_refs->Release();

	return stats;
}

// EOF
