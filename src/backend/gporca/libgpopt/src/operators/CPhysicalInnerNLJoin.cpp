//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalInnerNLJoin.cpp
//
//	@doc:
//		Implementation of inner nested-loops join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CCastUtils.h"

#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "gpopt/operators/CPhysicalInnerNLJoin.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerNLJoin::CPhysicalInnerNLJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalInnerNLJoin::CPhysicalInnerNLJoin(CMemoryPool *mp)
	: CPhysicalNLJoin(mp)
{
	// Inner NLJ creates two distribution requests for children:
	// (0) Outer child is requested for ANY distribution, and inner child is requested for a Replicated (or a matching) distribution
	// (1) Outer child is requested for Replicated distribution, and inner child is requested for Non-Singleton

	SetDistrRequests(2);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerNLJoin::~CPhysicalInnerNLJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalInnerNLJoin::~CPhysicalInnerNLJoin()
{
}



//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerNLJoin::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child;
//		this function creates two distribution requests:
//
//		(0) Outer child is requested for ANY distribution, and inner child is
//		  requested for a Replicated (or a matching) distribution,
//		  this request is created by calling CPhysicalJoin::PdsRequired()
//
//		(1) Outer child is requested for Replicated distribution, and inner child
//		  is requested for Non-Singleton (or Singleton if outer delivered Universal distribution)
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalInnerNLJoin::PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								  CDistributionSpec *pdsRequired,
								  ULONG child_index,
								  CDrvdPropArray *pdrgpdpCtxt,
								  ULONG ulOptReq) const
{
	GPOS_ASSERT(2 > child_index);
	GPOS_ASSERT(ulOptReq < UlDistrRequests());

	// if expression has to execute on a single host then we need a gather
	if (exprhdl.NeedsSingletonExecution())
	{
		return PdsRequireSingleton(mp, exprhdl, pdsRequired, child_index);
	}

	if (exprhdl.HasOuterRefs())
	{
		if (CDistributionSpec::EdtSingleton == pdsRequired->Edt() ||
			CDistributionSpec::EdtReplicated == pdsRequired->Edt())
		{
			return PdsPassThru(mp, exprhdl, pdsRequired, child_index);
		}
		return GPOS_NEW(mp) CDistributionSpecReplicated();
	}

	if (GPOS_FTRACE(EopttraceDisableReplicateInnerNLJOuterChild) ||
		0 == ulOptReq)
	{
		if (1 == child_index)
		{
			// compute a matching distribution based on derived distribution of outer child
			CDistributionSpec *pdsOuter =
				CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
			if (CDistributionSpec::EdtHashed == pdsOuter->Edt())
			{
				// require inner child to have matching hashed distribution
				CExpression *pexprScPredicate =
					exprhdl.PexprScalarExactChild(2);
				CExpressionArray *pdrgpexpr =
					CPredicateUtils::PdrgpexprConjuncts(mp, pexprScPredicate);

				CExpressionArray *pdrgpexprMatching =
					GPOS_NEW(mp) CExpressionArray(mp);
				CDistributionSpecHashed *pdshashed =
					CDistributionSpecHashed::PdsConvert(pdsOuter);
				CExpressionArray *pdrgpexprHashed = pdshashed->Pdrgpexpr();
				const ULONG ulSize = pdrgpexprHashed->Size();

				BOOL fSuccess = true;
				for (ULONG ul = 0; fSuccess && ul < ulSize; ul++)
				{
					CExpression *pexpr = (*pdrgpexprHashed)[ul];
					// get matching expression from predicate for the corresponding outer child
					// to create CDistributionSpecHashed for inner child
					CExpression *pexprMatching =
						CUtils::PexprMatchEqualityOrINDF(pexpr, pdrgpexpr);
					fSuccess = (NULL != pexprMatching);
					if (fSuccess)
					{
						pexprMatching->AddRef();
						pdrgpexprMatching->Append(pexprMatching);
					}
				}
				pdrgpexpr->Release();

				if (fSuccess)
				{
					GPOS_ASSERT(pdrgpexprMatching->Size() ==
								pdrgpexprHashed->Size());

					// create a matching hashed distribution request
					BOOL fNullsColocated = pdshashed->FNullsColocated();
					CDistributionSpecHashed *pdshashedEquiv =
						GPOS_NEW(mp) CDistributionSpecHashed(pdrgpexprMatching,
															 fNullsColocated);
					pdshashedEquiv->ComputeEquivHashExprs(mp, exprhdl);
					return pdshashedEquiv;
				}
				pdrgpexprMatching->Release();
			}
		}
		return CPhysicalJoin::PdsRequired(mp, exprhdl, pdsRequired, child_index,
										  pdrgpdpCtxt, ulOptReq);
	}
	GPOS_ASSERT(1 == ulOptReq);

	if (0 == child_index)
	{
		return GPOS_NEW(mp) CDistributionSpecReplicated();
	}

	// compute a matching distribution based on derived distribution of outer child
	CDistributionSpec *pdsOuter =
		CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
	if (CDistributionSpec::EdtUniversal == pdsOuter->Edt())
	{
		// first child is universal, request second child to execute on a single host to avoid duplicates
		return GPOS_NEW(mp) CDistributionSpecSingleton();
	}

	return GPOS_NEW(mp) CDistributionSpecNonSingleton();
}

// EOF
