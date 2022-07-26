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

CPropConstraint *
CLogicalSelect::DerivePropertyConstraint(CMemoryPool *mp,
						 CExpressionHandle &exprhdl) const
{
	CColRefSetArray *pdrgpcrs = GPOS_NEW(mp) CColRefSetArray(mp);
	CConstraintArray *pdrgpcnstr = GPOS_NEW(mp) CConstraintArray(mp);

	// collect constraint properties from relational children
	// and predicates from scalar children
	for (ULONG ul = 0; ul < exprhdl.Arity(); ul++)
	{
		if (exprhdl.FScalarChild(ul))
		{
			CExpression *pexprScalar = exprhdl.PexprScalarExactChild(ul);
			if (nullptr == pexprScalar)
			{
				continue;
			}

			if (CUtils::FPredicate(pexprScalar))
			{
				CColRefSetArray *pdrgpcrsChild = nullptr;
				CConstraint *pcnstr = CConstraint::PcnstrFromScalarExpr(
					mp, pexprScalar, &pdrgpcrsChild);

				if (nullptr != pcnstr)
				{
					pdrgpcnstr->Append(pcnstr);

					// merge with the equivalence classes we have so far
					CColRefSetArray *pdrgpcrsMerged =
						CUtils::PdrgpcrsMergeEquivClasses(mp, pdrgpcrs,
														  pdrgpcrsChild);
					pdrgpcrs->Release();
					pdrgpcrs = pdrgpcrsMerged;
				}
				CRefCount::SafeRelease(pdrgpcrsChild);
			}
			else if (CUtils::FAnySubquery(pexprScalar->Pop()))
			{
				CExpression *pexprRel = (*pexprScalar)[0];
				GPOS_ASSERT(pexprRel->Pop()->FLogical());
				if (pexprRel->HasOuterRefs())
				{
					CPropConstraint *ppc = pexprRel->DerivePropertyConstraint();
					CColRefSet *outRefs = pexprRel->DeriveOuterReferences();

					CColRefSetIter crsi(*outRefs);
					while (ppc != nullptr && crsi.Advance())
					{
						CColRef *colref = crsi.Pcr();
						CColRefSet *equivOutRefs = ppc->PcrsEquivClass(colref);
						if (equivOutRefs == nullptr || equivOutRefs->Size() == 0)
						{
							CRefCount::SafeRelease(equivOutRefs);
							continue;
						}
						CConstraint *cnstr4Outer = ppc->Pcnstr()->Pcnstr(mp, equivOutRefs);
						if (cnstr4Outer == nullptr || cnstr4Outer->IsConstraintUnbounded())
						{
							CRefCount::SafeRelease(equivOutRefs);
							CRefCount::SafeRelease(cnstr4Outer);
							continue;
						}

						CConstraint *cnstrCol = cnstr4Outer->PcnstrRemapForColumn(mp, colref);
						pdrgpcnstr->Append(cnstrCol);
						cnstr4Outer->Release();

						CColRefSet *crs = GPOS_NEW(mp) CColRefSet(mp);
						crs->Include(colref);

						CColRefSetArray *pdrgpcrsMerged =
							CUtils::AddEquivClassToArray(mp, crs, pdrgpcrs);
						pdrgpcrs->Release();
						crs->Release();
						pdrgpcrs = pdrgpcrsMerged;
					}
				}
			}
		}
		else // for relational child
		{
			CPropConstraint *ppc = exprhdl.DerivePropertyConstraint(ul);
			// equivalence classes coming from child
			CColRefSetArray *pdrgpcrsChild = ppc->PdrgpcrsEquivClasses();

			// merge with the equivalence classes we have so far
			CColRefSetArray *pdrgpcrsMerged =
				CUtils::PdrgpcrsMergeEquivClasses(mp, pdrgpcrs, pdrgpcrsChild);
			pdrgpcrs->Release();
			pdrgpcrs = pdrgpcrsMerged;

			// constraint coming from child
			CConstraint *pcnstr = ppc->Pcnstr();
			if (nullptr != pcnstr)
			{
				pcnstr->AddRef();
				pdrgpcnstr->Append(pcnstr);
			}
		}
	}

	CConstraint *pcnstrNew = CConstraint::PcnstrConjunction(mp, pdrgpcnstr);
	return GPOS_NEW(mp) CPropConstraint(mp, pdrgpcrs, pcnstrNew);
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

// compute partition predicate to pass down to n-th child.
// given an input scalar expression, find out the predicate
// in the scalar expression which is used for partitioning
//
// clang-format off
// Input Expr:
//	+--CScalarBoolOp (EboolopAnd)
//	|--CScalarArrayCmp Any (=)
//	|  |--CScalarIdent "risk_set_row_id" (2)
//	|  +--CScalarArray: {eleMDId: (23,1.0), arrayMDId: (1007,1.0) CScalarConst (32) CScalarConst (33) CScalarConst (43) CScalarConst (9) CScalarConst (10) CScalarConst (15) CScalarConst (16) CScalarConst (36) CScalarConst (11) CScalarConst (50) CScalarConst (46) CScalarConst (356) CScalarConst (468) CScalarConst (42)}
//	+--CScalarCmp (=)
//	|--CScalarIdent "value_date" (0)
//	+--CScalarConst (559094400000000.000)
//
// Let's say the partition key is on value_date, then the extracted
// predicate is as below:
// Output Expr:
//	+--CScalarCmp (=)
//	|--CScalarIdent "value_date" (0)
//	+--CScalarConst (559094400000000.000)
// clang-format on
CExpression *
CLogicalSelect::PexprPartPred(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CExpression *,  //pexprInput
							  ULONG
#ifdef GPOS_DEBUG
								  child_index
#endif	//GPOS_DEBUG
) const
{
	GPOS_ASSERT(0 == child_index);

	CExpression *pexprScalar = exprhdl.PexprScalarExactChild(1 /*child_index*/);

	if (nullptr == pexprScalar)
	{
		// no exact predicate is available (e.g. when we have a subquery in the predicate)
		return nullptr;
	}

	// get partition keys
	CPartInfo *ppartinfo = exprhdl.DerivePartitionInfo();
	GPOS_ASSERT(nullptr != ppartinfo);

	// we assume that the select is right on top of the dynamic get, so there
	// should be only one part consumer. If there is more, then we are higher up so
	// we do not push down any predicates
	if (1 != ppartinfo->UlConsumers())
	{
		return nullptr;
	}

	// check if a corresponding predicate has already been cached
	CExpression *pexprPredOnPartKey = m_phmPexprPartPred->Find(pexprScalar);
	if (pexprPredOnPartKey != nullptr)
	{
		// predicate on partition key found in cache
		pexprPredOnPartKey->AddRef();
		return pexprPredOnPartKey;
	}

	CPartKeysArray *pdrgppartkeys = ppartinfo->Pdrgppartkeys(0 /*ulPos*/);
	const ULONG ulKeySets = pdrgppartkeys->Size();
	for (ULONG ul = 0; nullptr == pexprPredOnPartKey && ul < ulKeySets; ul++)
	{
		pexprPredOnPartKey = CPredicateUtils::PexprExtractPredicatesOnPartKeys(
			mp, pexprScalar, (*pdrgppartkeys)[ul]->Pdrgpdrgpcr(),
			nullptr,  //pcrsAllowedRefs
			true	  //fUseConstraints
		);
	}

	if (pexprPredOnPartKey != nullptr)
	{
		// insert the scalar expression and the corresponding partitioning predicate
		// in the hashmap
		pexprPredOnPartKey->AddRef();
		pexprScalar->AddRef();
		m_phmPexprPartPred->Insert(pexprScalar, pexprPredOnPartKey);
	}

	return pexprPredOnPartKey;
}

// EOF
