//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDecorrelator.cpp
//
//	@doc:
//		Implementation of decorrelation logic
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/operators/CLogicalSequenceProject.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "gpopt/xforms/CDecorrelator.h"

#include "naucrates/md/IMDScalarOp.h"


using namespace gpopt;

// initialization of handlers array
const CDecorrelator::SOperatorProcessor CDecorrelator::m_rgopproc[] =
{
	{COperator::EopLogicalSelect, FProcessSelect},
	{COperator::EopLogicalGbAgg, FProcessGbAgg},
	{COperator::EopLogicalInnerJoin, FProcessJoin},
	{COperator::EopLogicalInnerCorrelatedApply, FProcessJoin},
	{COperator::EopLogicalLeftSemiJoin, FProcessJoin},
	{COperator::EopLogicalLeftSemiCorrelatedApplyIn, FProcessJoin},
	{COperator::EopLogicalLeftAntiSemiJoin, FProcessJoin},
	{COperator::EopLogicalLeftOuterJoin, FProcessJoin},
	{COperator::EopLogicalLeftOuterCorrelatedApply, FProcessJoin},
	{COperator::EopLogicalNAryJoin, FProcessJoin},
	{COperator::EopLogicalProject, FProcessProject},
	{COperator::EopLogicalSequenceProject, FProcessProject},
	{COperator::EopLogicalAssert, FProcessAssert},
	{COperator::EopLogicalMaxOneRow, FProcessMaxOneRow},
	{COperator::EopLogicalLimit, FProcessLimit},
};


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FPullableCorrelations
//
//	@doc:
//		Helper to check if correlations below join are valid to be pulled-up
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FPullableCorrelations
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	CExpressionArray *pdrgpexprChildren,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(NULL != pdrgpexprChildren);
	GPOS_ASSERT(NULL != pdrgpexprCorrelations);

	// check for pullable semi join correlations
	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();
	BOOL semi_join =
		COperator::EopLogicalLeftSemiJoin == op_id ||
		COperator::EopLogicalLeftAntiSemiJoin == op_id ||
		CUtils::FCorrelatedApply(pexpr->Pop());
	GPOS_ASSERT_IMP(semi_join, 2 == pdrgpexprChildren->Size());

	if (semi_join)
	{
		return CPredicateUtils::FValidSemiJoinCorrelations(mp, (*pdrgpexprChildren)[0], (*pdrgpexprChildren)[1], pdrgpexprCorrelations);
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FDelayableScalarOp
//
//	@doc:
//		Check if scalar operator can be delayed
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FDelayableScalarOp
	(
	CExpression *pexprScalar
	)
{
	GPOS_ASSERT(NULL != pexprScalar);

	// check operator
	COperator::EOperatorId eopidScalar = pexprScalar->Pop()->Eopid();
	switch(eopidScalar)
	{
		case COperator::EopScalarIdent:
			return true;

		case COperator::EopScalarCast:
			return CScalarIdent::FCastedScId(pexprScalar);

		case COperator::EopScalarCmp:
			return CPredicateUtils::IsEqualityOp(pexprScalar);

		default:
			return false;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FDelayable
//
//	@doc:
//		Check if predicate can be delayed
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FDelayable
	(
	CExpression *pexprLogical, // logical parent of predicate tree
	CExpression *pexprScalar,
	BOOL fEqualityOnly
	)
{
	GPOS_CHECK_STACK_SIZE;

	GPOS_ASSERT(NULL != pexprLogical);
	GPOS_ASSERT(pexprLogical->Pop()->FLogical());
	GPOS_ASSERT(NULL != pexprScalar);
	GPOS_ASSERT(pexprScalar->Pop()->FScalar());
	
	BOOL fDelay = true;

	COperator::EOperatorId op_id = pexprLogical->Pop()->Eopid();
 	if (COperator::EopLogicalLeftSemiJoin == op_id || COperator::EopLogicalLeftAntiSemiJoin == op_id)
 	{
 		// for semi-joins, we disallow predicates referring to inner child to be pulled above the join
 		CColRefSet *pcrsUsed = pexprScalar->DeriveUsedColumns();
 		CColRefSet *pcrsInner = (*pexprLogical)[1]->DeriveOutputColumns();
 		if (!pcrsUsed->IsDisjoint(pcrsInner))
 		{
 			// predicate uses a column produced by semi-join inner child

 			fDelay = false;
 		}
 	}
	
	if (fDelay && fEqualityOnly)
	{
		// check operator 
		fDelay = FDelayableScalarOp(pexprScalar);
	}

	// check its children
	const ULONG arity = pexprScalar->Arity();
	for (ULONG ul = 0; ul < arity && fDelay; ul++)
	{
		fDelay = FDelayable(pexprLogical, (*pexprScalar)[ul], fEqualityOnly);
	}

	return fDelay;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcess
//
//	@doc:
//		Main driver for decorrelation of expression
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcess
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_CHECK_STACK_SIZE;

	// only correlated Apply can be encountered here
	GPOS_ASSERT_IMP(CUtils::FApply(pexpr->Pop()), CUtils::FCorrelatedApply(pexpr->Pop()) &&
			"Apply expression is encountered by decorrelator");

	// no outer references?
	if (0 == pexpr->DeriveOuterReferences()->Size())
	{
		pexpr->AddRef();
		*ppexprDecorrelated = pexpr;
		return true;
	}
	
	BOOL fSuccess = FProcessOperator(mp, pexpr, fEqualityOnly, ppexprDecorrelated, pdrgpexprCorrelations);

	// in case of success make sure there are no outer references left
	GPOS_ASSERT_IMP
		(
		fSuccess,
		0 == (*ppexprDecorrelated)->DeriveOuterReferences()->Size()
		);

	return fSuccess;
}	


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessOperator
//
//	@doc:
//		Router for different operators to process
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessOperator
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	FnProcessor *pfnp = NULL;
	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();
	const ULONG size = GPOS_ARRAY_SIZE(m_rgopproc);
	// find the handler corresponding to the given operator
	for (ULONG ul = 0; pfnp == NULL && ul < size; ul++)
	{
		if (op_id == m_rgopproc[ul].m_eopid)
		{
			pfnp = m_rgopproc[ul].m_pfnp;
		}
	}
	
	if (NULL != pfnp)
	{
		// subqueries must be processed before reaching here
		const ULONG arity = pexpr->Arity();
		if ((*pexpr)[arity - 1]->Pop()->FScalar() && CUtils::FHasSubquery((*pexpr)[arity - 1]))
		{
			return false;
		}

		// check for abort before descending into recursion
		GPOS_CHECK_ABORT;

		return pfnp(mp, pexpr, fEqualityOnly, ppexprDecorrelated, pdrgpexprCorrelations);
	}
	
	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessPredicate
//
//	@doc:
//		Decorrelate predicate
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessPredicate
	(
	CMemoryPool *mp,
	CExpression *pexprLogical, // logical parent of predicate tree
	CExpression *pexprScalar,
	BOOL fEqualityOnly,
	CColRefSet *pcrsOutput,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(pexprLogical->Pop()->FLogical());
	GPOS_ASSERT(pexprScalar->Pop()->FScalar());

	*ppexprDecorrelated = NULL;
	
	CExpressionArray *pdrgpexprConj = CPredicateUtils::PdrgpexprConjuncts(mp, pexprScalar);
	CExpressionArray *pdrgpexprResiduals = GPOS_NEW(mp) CExpressionArray(mp);
	BOOL fSuccess = true;
	
	// divvy up the predicates in residuals (w/ no outer ref) and correlations (w/ outer refs)
	ULONG length = pdrgpexprConj->Size();
	for(ULONG ul = 0; ul < length && fSuccess; ul++)
	{
		CExpression *pexprConj = (*pdrgpexprConj)[ul];
		CColRefSet *pcrsUsed = pexprConj->DeriveUsedColumns();
		
		if (pcrsOutput->ContainsAll(pcrsUsed))
		{
			// no outer ref
			pexprConj->AddRef();
			pdrgpexprResiduals->Append(pexprConj);			

			continue;
		}
		
		fSuccess = FDelayable(pexprLogical, pexprConj, fEqualityOnly);
		if (fSuccess)
		{
			pexprConj->AddRef();
			pdrgpexprCorrelations->Append(pexprConj);
		}
		
	}

	pdrgpexprConj->Release();

	if (!fSuccess || 0 == pdrgpexprResiduals->Size())
	{
		// clean up
		pdrgpexprResiduals->Release();
	}
	else
	{
		// residuals become new predicate
		*ppexprDecorrelated = CPredicateUtils::PexprConjunction(mp, pdrgpexprResiduals);
	}


	return fSuccess;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessSelect
//
//	@doc:
//		Decorrelate select operator
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessSelect
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(COperator::EopLogicalSelect == pexpr->Pop()->Eopid());

	// decorrelate relational child
	CExpression *pexprRelational = NULL;
	if (!FProcess(mp, (*pexpr)[0], fEqualityOnly, &pexprRelational, pdrgpexprCorrelations))
	{
		GPOS_ASSERT(NULL == pexprRelational);
		return false;
	}

	// process predicate
	CExpression *pexprPredicate = NULL;
	CColRefSet *pcrsOutput = pexpr->DeriveOutputColumns();
	BOOL fSuccess  = FProcessPredicate(mp, pexpr, (*pexpr)[1], fEqualityOnly, pcrsOutput, &pexprPredicate, pdrgpexprCorrelations);
	
	// build substitute
	if (fSuccess)
	{
		if (NULL != pexprPredicate)
		{
			CLogicalSelect *popSelect = CLogicalSelect::PopConvert(pexpr->Pop());
			popSelect->AddRef();
			
			*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, popSelect, pexprRelational, pexprPredicate);
		}
		else 
		{
			*ppexprDecorrelated = pexprRelational;
		}

	}
	else
	{
		pexprRelational->Release();
	}
	
	return fSuccess;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessGbAgg
//
//	@doc:
//		Decorrelate GbAgg operator
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessGbAgg
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL, // fEqualityOnly
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	CLogicalGbAgg *popAggOriginal = CLogicalGbAgg::PopConvert(pexpr->Pop());
	
	// fail if agg has outer references
	if (CUtils::HasOuterRefs(pexpr) && !CUtils::HasOuterRefs((*pexpr)[0]))
	{
		return false;
	}

	// TODO: 12/20/2012 - ; check for strictness of agg function

	// decorrelate relational child
	CExpression *pexprRelational = NULL;
	if (!FProcess(mp, (*pexpr)[0], true /*fEqualityOnly*/, &pexprRelational, pdrgpexprCorrelations))
	{
		GPOS_ASSERT(NULL == pexprRelational);
		return false;
	}
	
	// get the output columns of decorrelated child
	CColRefSet *pcrsOutput = pexprRelational->DeriveOutputColumns();

	// create temp expression of correlations to determine inner columns
	pdrgpexprCorrelations->AddRef();
	CExpression *pexprTemp = CPredicateUtils::PexprConjunction(mp, pdrgpexprCorrelations);
	CColRefSet *pcrs = 
		GPOS_NEW(mp) CColRefSet(mp,
			*(pexprTemp->DeriveUsedColumns()));
		
	pcrs->Intersection(pcrsOutput);
	pexprTemp->Release();

	// add grouping columns from original agg
	pcrs->Include(popAggOriginal->Pdrgpcr());

	// assemble grouping columns
	CColRefArray *colref_array = pcrs->Pdrgpcr(mp);
	pcrs->Release();
	
	// assemble agg
	CExpression *pexprProjList = (*pexpr)[1];
	pexprProjList->AddRef();
	CLogicalGbAgg *popAgg = GPOS_NEW(mp) CLogicalGbAgg(mp, colref_array, popAggOriginal->Egbaggtype());
	*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, popAgg, pexprRelational, pexprProjList);
	
	return true;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessJoin
//
//	@doc:
//		Decorrelate a join expression;
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessJoin
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(CUtils::FLogicalJoin(pexpr->Pop()) || CUtils::FApply(pexpr->Pop()));

	ULONG arity = pexpr->Arity();	
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp, arity);
	CColRefSet *pcrsOutput = GPOS_NEW(mp) CColRefSet(mp);

	// decorrelate all relational children
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CExpression *pexprInput = NULL;
		if (FProcess(mp, (*pexpr)[ul], fEqualityOnly, &pexprInput, pdrgpexprCorrelations))
		{
			pdrgpexpr->Append(pexprInput);
			pcrsOutput->Union(pexprInput->DeriveOutputColumns());
		}
		else
		{
			pdrgpexpr->Release();
			pcrsOutput->Release();
			
			return false;
		}
	}

	// check for valid semi join correlations
	if (!FPullableCorrelations(mp, pexpr, pdrgpexpr, pdrgpexprCorrelations))
	{
		pdrgpexpr->Release();
		pcrsOutput->Release();

		return false;
	 }

	// decorrelate predicate and build new join operator
	CExpression *pexprPredicate = NULL;
	BOOL fSuccess = FProcessPredicate(mp, pexpr, (*pexpr)[arity - 1], fEqualityOnly, pcrsOutput, &pexprPredicate, pdrgpexprCorrelations);
	pcrsOutput->Release();

	if (fSuccess)
	{
		// in case entire predicate is being deferred, plug in a 'true'
		if (NULL == pexprPredicate)
		{
			pexprPredicate = CUtils::PexprScalarConstBool(mp, true /*value*/);
		}
		
		pdrgpexpr->Append(pexprPredicate);
		
		COperator *pop = pexpr->Pop();
		pop->AddRef();
		*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, pop, pdrgpexpr);
	}
	else
	{
		pdrgpexpr->Release();
		CRefCount::SafeRelease(pexprPredicate);
	}
	
	return fSuccess;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessAssert
//
//	@doc:
//		Decorrelate assert operator
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessAssert
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator *pop = pexpr->Pop();
	GPOS_ASSERT(COperator::EopLogicalAssert == pop->Eopid());

	CExpression *pexprScalar = (*pexpr)[1];

	// fail if assert expression has outer references
	CColRefSet *pcrsOutput = (*pexpr)[0]->DeriveOutputColumns();
	CColRefSet *pcrsUsed = pexprScalar->DeriveUsedColumns();
	if (!pcrsOutput->ContainsAll(pcrsUsed))
	{
		return false;
	}

	// decorrelate relational child
	CExpression *pexprRelational = NULL;
	if (!FProcess(mp, (*pexpr)[0], fEqualityOnly, &pexprRelational, pdrgpexprCorrelations))
	{
		GPOS_ASSERT(NULL == pexprRelational);
		return false;
	}

	// assemble new project
	pop->AddRef();
	pexprScalar->AddRef();
	*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, pop, pexprRelational, pexprScalar);

	return true;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessMaxOneRow
//
//	@doc:
//		Decorrelate max one row operator
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessMaxOneRow
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator *pop = pexpr->Pop();
	GPOS_ASSERT(COperator::EopLogicalMaxOneRow == pop->Eopid());

	// fail if MaxOneRow expression has outer references
	if (CUtils::HasOuterRefs(pexpr))
	{
		return false;
	}

	// decorrelate relational child
	CExpression *pexprRelational = NULL;
	if (!FProcess(mp, (*pexpr)[0], fEqualityOnly, &pexprRelational, pdrgpexprCorrelations))
	{
		GPOS_ASSERT(NULL == pexprRelational);
		return false;
	}

	// assemble new project
	pop->AddRef();
	*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, pop, pexprRelational);

	return true;
}

// decorrelate project/sequence project
// clang-format off
// if the expression can be decorrelated, the scalar cmp is pulled up and is replaced by
// the project list.
// input ex with CLogicalSequenceProject:
//      +--CLogicalSequenceProject (Partition By Keys:HASHED: [ +--CScalarIdent "i" (9)
//         |--CLogicalSelect   origin: [Grp:5, GrpExpr:0]
//         |  |--CLogicalGet "b" ("b"), Columns: [..]
//         |  +--CScalarCmp (=)   origin: [Grp:4, GrpExpr:0]
//         |     |--CScalarIdent "i" (0)   origin: [Grp:2, GrpExpr:0]
//         |     +--CScalarIdent "i" (9)   origin: [Grp:3, GrpExpr:0]
//         +--CScalarProjectList   origin: [Grp:8, GrpExpr:0]
//            +--CScalarProjectElement "avg" (18)   origin: [Grp:7, GrpExpr:0]
//               +--CScalarWindowFunc (avg , Agg: true , Distinct: false , StarArgument: false , SimpleAgg: true)   origin: [Grp:6, GrpExpr:0]
//                  +--CScalarIdent "i" (9)

// results:
// decorrelated expression, ppexprDecorrelated
//     +--CLogicalSequenceProject
//        |--CLogicalGet "b" ("b"), Columns: [...]
//        +--CScalarProjectList   origin: [Grp:8, GrpExpr:0]
//           +--CScalarProjectElement "avg" (18)   origin: [Grp:7, GrpExpr:0]
//              +--CScalarWindowFunc (avg , Agg: true , Distinct: false , StarArgument: false , SimpleAgg: true)   origin: [Grp:6, GrpExpr:0]
//              +--CScalarIdent "i" (9)   origin: [Grp:3, GrpExpr:0]
// array of quals
// pdrgpexprCorrelations
//         +--CScalarCmp (=)   origin: [Grp:4, GrpExpr:0]
//            |--CScalarIdent "i" (0)   origin: [Grp:2, GrpExpr:0]
//            +--CScalarIdent "i" (9)   origin: [Grp:3, GrpExpr:0]
// clang-format on
BOOL
CDecorrelator::FProcessProject
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();

	GPOS_ASSERT(COperator::EopLogicalProject == op_id ||
			COperator::EopLogicalSequenceProject == op_id);

	CExpression *pexprPrjList = (*pexpr)[1];

	// fail if project elements have outer references
	CColRefSet *pcrsOutput = (*pexpr)[0]->DeriveOutputColumns();
	CColRefSet *pcrsUsed = pexprPrjList->DeriveUsedColumns();
	if (!pcrsOutput->ContainsAll(pcrsUsed))
	{
		return false;
	}

	if (COperator::EopLogicalSequenceProject == op_id)
	{
		CExpressionHandle exprhdl(mp);
		exprhdl.Attach(pexpr);
		exprhdl.DeriveProps(NULL /*pdpctxt*/);
		
		// fail decorrelation in the following two cases;
		// 1. if the LogicalSequenceProject node has local outer references in order by or partition by or window frame
		// of a window function
		// ex: select C.j from C where C.i in (select rank() over (order by C.i) from B where B.i=C.i);
		// 2. if the relational child of LogicalSequenceProject node does not have any aggregate window function

		// if the project list contains aggregrate on window function, then
		// we can decorrelate it as the aggregate is performed over a column or count(*).
		// The IN condition will be translated to a join instead of a correlated plan.
		// ex: select C.j from C where C.i in (select avg(i) over (partition by B.i) from B where B.i=C.i);
		// ===> (resulting join condition) b.i = c.i and c.i = avg(i)
		if (CLogicalSequenceProject::PopConvert(pexpr->Pop())->FHasLocalOuterRefs(exprhdl)
			|| !CUtils::FHasAggWindowFunc(pexprPrjList))
		{
			return false;
		}
	}

	// decorrelate relational child
	CExpression *pexprRelational = NULL;
	if (!FProcess(mp, (*pexpr)[0], fEqualityOnly, &pexprRelational, pdrgpexprCorrelations))
	{
		GPOS_ASSERT(NULL == pexprRelational);
		return false;
	}
	
	// assemble new project
	COperator *pop = pexpr->Pop();
	pop->AddRef();
	pexprPrjList->AddRef();
	
	*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, pop, pexprRelational, pexprPrjList);
	
	return true;
}


//---------------------------------------------------------------------------
//	@function:
//		CDecorrelator::FProcessLimit
//
//	@doc:
//		Decorrelate limit
//
//---------------------------------------------------------------------------
BOOL
CDecorrelator::FProcessLimit
	(
	CMemoryPool *mp,
	CExpression *pexpr,
	BOOL fEqualityOnly,
	CExpression **ppexprDecorrelated,
	CExpressionArray *pdrgpexprCorrelations
	)
{
	GPOS_ASSERT(COperator::EopLogicalLimit == pexpr->Pop()->Eopid());

	CExpression *pexprOffset = (*pexpr)[1];
	CExpression *pexprRowCount = (*pexpr)[2];

	// fail if there are any outer references below Limit
	if (CUtils::HasOuterRefs(pexpr))
	{
		return false;
	}

	// decorrelate relational child
	CExpression *pexprRelational = NULL;
	if (!FProcess(mp, (*pexpr)[0], fEqualityOnly, &pexprRelational, pdrgpexprCorrelations))
	{
		GPOS_ASSERT(NULL == pexprRelational);
		return false;
	}

	// assemble new project
	COperator *pop = pexpr->Pop();
	pop->AddRef();
	pexprOffset->AddRef();
	pexprRowCount->AddRef();

	*ppexprDecorrelated = GPOS_NEW(mp) CExpression(mp, pop, pexprRelational, pexprOffset, pexprRowCount);

	return true;
}

// EOF
