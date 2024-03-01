//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2024 VMware, Inc. or its affiliates.
//
//	@filename:
//		CJoinOrderHintsPreprocessor.cpp
//
//	@doc:
//		Preprocessing routines of join order hints
//---------------------------------------------------------------------------
//
#include "gpopt/operators/CJoinOrderHintsPreprocessor.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalNAryJoin.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/optimizer/COptimizerConfig.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		GetLeaf
//
//	@doc:
//		Return the child expression of pexpr that has a single table descriptor
//		matching alias. If none exists, return nullptr.
//---------------------------------------------------------------------------
static CExpression *
GetLeaf(CExpression *pexpr, const CWStringConst *alias)
{
	GPOS_ASSERT(COperator::EopLogicalNAryJoin == pexpr->Pop()->Eopid());

	for (ULONG ul = 0; ul < pexpr->Arity(); ul++)
	{
		CTableDescriptorHashSet *ptabs = (*pexpr)[ul]->DeriveTableDescriptor();
		if (ptabs->Size() == 1 && ptabs->First()->Name().Pstr()->Equals(alias))
		{
			return (*pexpr)[ul];
		}
	}

	return nullptr;
}


//---------------------------------------------------------------------------
//	@function:
//		GetChild
//
//	@doc:
//		Return the child expression of pexpr that has a table descriptor set
//		containing the aliases. If none exists, return nullptr.
//---------------------------------------------------------------------------
static CExpression *
GetChild(CMemoryPool *mp, CExpression *pexpr, CWStringConstHashSet *aliases)
{
	GPOS_ASSERT(COperator::EopLogicalNAryJoin == pexpr->Pop()->Eopid());

	for (ULONG ul = 0; ul < pexpr->Arity(); ul++)
	{
		CWStringConstHashSet *pexpr_aliases =
			CHintUtils::GetAliasesFromTableDescriptors(
				mp, (*pexpr)[ul]->DeriveTableDescriptor());

		bool is_contained = true;

		CWStringConstHashSetIter hsiter(aliases);
		while (hsiter.Advance())
		{
			if (!pexpr_aliases->Contains(hsiter.Get()))
			{
				is_contained = false;
				break;
			}
		}

		pexpr_aliases->Release();
		if (is_contained)
		{
			return (*pexpr)[ul];
		}
	}

	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		IsChild
//
//	@doc:
//		Return whether a child expression of pexpr that has a table descriptor
//		set containing the aliases.
//---------------------------------------------------------------------------
static bool
IsChild(CMemoryPool *mp, CExpression *pexpr, CWStringConstHashSet *aliases)
{
	return nullptr != GetChild(mp, pexpr, aliases);
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderHintsPreprocessor::GetUnusedChildren
//
//	@doc:
//		Return list of all the children of fromExpr that are not referenced in
//		toExpr.
//---------------------------------------------------------------------------
CExpressionArray *
CJoinOrderHintsPreprocessor::GetUnusedChildren(CMemoryPool *mp,
											   CExpression *fromExpr,
											   CExpression *toExpr)
{
	GPOS_ASSERT(COperator::EopLogicalNAryJoin == fromExpr->Pop()->Eopid());

	// get all the alias used in the toExpr
	CWStringConstHashSet *usedNames = GPOS_NEW(mp) CWStringConstHashSet(mp);

	CWStringConstHashSet *pexprAliasesFromTabs =
		CHintUtils::GetAliasesFromTableDescriptors(
			mp, toExpr->DeriveTableDescriptor());
	CWStringConstHashSetIter iter(pexprAliasesFromTabs);
	while (iter.Advance())
	{
		usedNames->Insert(iter.Get());
	}
	pexprAliasesFromTabs->Release();

	CExpressionArray *unusedChildren = GPOS_NEW(mp) CExpressionArray(mp);

	// check for hint aliases not used in the inner/outer child
	for (ULONG ul = 0; ul < fromExpr->Arity(); ul++)
	{
		if ((*fromExpr)[ul]->DeriveTableDescriptor()->Size() == 0)
		{
			continue;
		}

		const CWStringConst *alias =
			(*fromExpr)[ul]->DeriveTableDescriptor()->First()->Name().Pstr();
		if (!usedNames->Contains(alias))
		{
			unusedChildren->Append(CJoinOrderHintsPreprocessor::PexprPreprocess(
				mp, (*fromExpr)[ul]));
		}
	}

	usedNames->Release();
	return unusedChildren;
}


//---------------------------------------------------------------------------
//	@function:
//		GetOnPreds
//
//	@doc:
//		Builds the predicates to join the inner and outer expressions.
//---------------------------------------------------------------------------
static CExpression *
GetOnPreds(CMemoryPool *mp, CExpression *outer, CExpression *inner,
		   CExpressionArray *allOnPreds)
{
	CExpressionArray *preds = GPOS_NEW(mp) CExpressionArray(mp);

	CColRefSet *innerAndOuter = GPOS_NEW(mp) CColRefSet(mp);
	innerAndOuter->Include(outer->DeriveOutputColumns());
	innerAndOuter->Include(inner->DeriveOutputColumns());

	for (ULONG ul = 0; ul < allOnPreds->Size(); ul++)
	{
		CColRefSet *predCols = (*allOnPreds)[ul]->DeriveUsedColumns();
		GPOS_ASSERT(predCols != nullptr);

		if (innerAndOuter->ContainsAll(predCols) &&
			predCols->FIntersects(outer->DeriveOutputColumns()) &&
			predCols->FIntersects(inner->DeriveOutputColumns()))
		{
			(*allOnPreds)[ul]->AddRef();
			preds->Append((*allOnPreds)[ul]);
		}
	}

	if (preds->Size() > 0)
	{
		innerAndOuter->Release();
		return CPredicateUtils::PexprConjunction(mp, preds);
	}

	preds->Release();
	innerAndOuter->Release();

	// No explicit predicate found, use "ON true".
	return CUtils::PexprScalarConstBool(mp, /*fVal*/ true, /*is_null*/ false);
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderHintsPreprocessor::RecursiveApplyJoinOrderHintsOnNAryJoin
//
//	@doc:
//		Recursively constructs CLogicalInnerJoin expressions using the children
//		of a CLogicalNAryJoin.
//---------------------------------------------------------------------------
CExpression *
CJoinOrderHintsPreprocessor::RecursiveApplyJoinOrderHintsOnNAryJoin(
	CMemoryPool *mp, CExpression *pexpr, const CJoinHint::JoinPair *joinpair)
{
	GPOS_ASSERT(COperator::EopLogicalNAryJoin == pexpr->Pop()->Eopid());

	CExpression *appliedHints = nullptr;

	CWStringConstHashSet *hint_aliases =
		CHintUtils::GetAliasesFromHint(mp, joinpair);
	if (joinpair->GetName())
	{
		// Base case when hint specifies name, then return the child of
		// CLogicalNAryJoin by that name.
		appliedHints = GetLeaf(pexpr, joinpair->GetName());
		if (nullptr != appliedHints)
		{
			appliedHints->AddRef();
		}
	}
	else if (IsChild(mp, pexpr, hint_aliases))
	{
		// If a child covers the aliases in the jointree, then recursively
		// apply the hint to that child. For example, if child is a GROUP BY or
		// LIMIT expression, like:
		//
		// Leading(T1 T2)
		// SELECT * FROM (SELECT a FROM T1, T2 LIMIT 42) q, T3;
		//
		// Note: We already have a joinpair hint we are trying to satisfy. So,
		// explicity pass that one along so we don't try to find another
		// matching hint
		appliedHints = CJoinOrderHintsPreprocessor::PexprPreprocess(
			mp, GetChild(mp, pexpr, hint_aliases), joinpair);
	}
	else
	{
		// If no single child covers the hint, then apply the joinpair
		// inner/outer to the CLogicalNAryJoin. For example:
		//
		// Hint: Leading((T1 T2) (T3 T4))
		// Operator: NAryJoin [T1 T2 T3 T4]
		//
		// Then left (T1 T2) and right (T3 T4) hints need to be applied to the
		// operator, and the result joined.
		CExpression *outer = RecursiveApplyJoinOrderHintsOnNAryJoin(
			mp, pexpr, joinpair->GetOuter());
		CExpression *inner = RecursiveApplyJoinOrderHintsOnNAryJoin(
			mp, pexpr, joinpair->GetInner());

		// Hint not satisfied. This can happen, for example, if joins hint
		// splits across a GROUP BY, like:
		//
		// Leading(T1 T3)
		// SELECT * FROM (SELECT a FROM T1, T2 LIMIT 42) q, T3;
		if (outer == nullptr || inner == nullptr)
		{
			hint_aliases->Release();
			pexpr->AddRef();
			CRefCount::SafeRelease(outer);
			CRefCount::SafeRelease(inner);

			return pexpr;
		}

		CExpressionArray *all_on_preds = CPredicateUtils::PdrgpexprConjuncts(
			mp, (*pexpr->PdrgPexpr())[pexpr->PdrgPexpr()->Size() - 1]);
		appliedHints = GPOS_NEW(mp)
			CExpression(mp, GPOS_NEW(mp) CLogicalInnerJoin(mp), outer, inner,
						GetOnPreds(mp, outer, inner, all_on_preds));
		all_on_preds->Release();
	}

	hint_aliases->Release();
	return appliedHints;
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinOrderHintsPreprocessor::PexprPreprocess
//
//	@doc:
//		Search for and apply join order hints on an expression. The result of
//		this converts two or more children of a CLogicalNAryJoin into one or
//		more CLogicalInnerJoin(s).
//---------------------------------------------------------------------------
CExpression *
CJoinOrderHintsPreprocessor::PexprPreprocess(
	CMemoryPool *mp, CExpression *pexpr, const CJoinHint::JoinPair *joinpair)
{
	// protect against stack overflow during recursion
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(nullptr != mp);
	GPOS_ASSERT(nullptr != pexpr);

	COperator *pop = pexpr->Pop();

	// Search for a join order hint for this expression.
	if (nullptr == joinpair)
	{
		CPlanHint *planhint =
			COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetPlanHint();

		CJoinHint *joinhint = planhint->GetJoinHint(pexpr);
		if (joinhint)
		{
			joinpair = joinhint->GetJoinPair();
		}
	}

	// Given a hint, recursively traverse the hint and (bottom-up) construct a
	// new join expression. Any leftover children are appended to the nary
	// join.
	if (COperator::EopLogicalNAryJoin == pop->Eopid() && nullptr != joinpair)
	{
		CExpression *appliedHints =
			RecursiveApplyJoinOrderHintsOnNAryJoin(mp, pexpr, joinpair);

		CExpressionArray *naryChildren =
			GetUnusedChildren(mp, pexpr, appliedHints);
		if (naryChildren->Size() == 0)
		{
			naryChildren->Release();
			return appliedHints;
		}
		else
		{
			naryChildren->Append(appliedHints);
			(*pexpr->PdrgPexpr())[pexpr->PdrgPexpr()->Size() - 1]->AddRef();
			naryChildren->Append(
				(*pexpr->PdrgPexpr())[pexpr->PdrgPexpr()->Size() - 1]);

			return GPOS_NEW(mp) CExpression(
				mp, GPOS_NEW(mp) CLogicalNAryJoin(mp), naryChildren);
		}
	}

	// If either there is no hint or this not an nary join expression, then
	// recurse into our children.
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	CExpressionArray *pdrgexprChildren = pexpr->PdrgPexpr();
	for (ULONG ul = 0; ul < pexpr->Arity(); ul++)
	{
		pdrgpexpr->Append(CJoinOrderHintsPreprocessor::PexprPreprocess(
			mp, (*pdrgexprChildren)[ul], joinpair));
	}

	pop->AddRef();
	return GPOS_NEW(mp) CExpression(mp, pop, pdrgpexpr);
}
