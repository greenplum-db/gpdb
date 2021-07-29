//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalNLJoin.cpp
//
//	@doc:
//		Implementation of base nested-loops join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalNLJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CRewindabilitySpec.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalCorrelatedInLeftSemiNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedInnerNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftOuterNLJoin.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::CPhysicalNLJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalNLJoin::CPhysicalNLJoin(CMemoryPool *mp) : CPhysicalJoin(mp)
{
	// NLJ creates two partition propagation requests for children:
	// (0) push possible Dynamic Partition Elimination (DPE) predicates from join's predicate to
	//		outer child, since outer child executes first
	// (1) ignore DPE opportunities in join's predicate, and push incoming partition propagation
	//		request to both children,
	//		this request handles the case where the inner child needs to be broadcasted, which prevents
	//		DPE by outer child since a Motion operator gets in between PartitionSelector and DynamicScan

	SetPartPropagateRequests(2);
    
    // NLJ creates two distribution requests for children:
    // (0) outer side requires Any distribution, inner attempts to match iff hashed
    // (1) outer side requires Any distribution, inner requires a replicated
    
    SetDistrRequests(2);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::~CPhysicalNLJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalNLJoin::~CPhysicalNLJoin() = default;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalNLJoin::PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 COrderSpec *posInput, ULONG child_index,
							 CDrvdPropArray *,	// pdrgpdpCtxt
							 ULONG				// ulOptReq
) const
{
	GPOS_ASSERT(
		child_index < 2 &&
		"Required sort order can be computed on the relational child only");

	if (0 == child_index)
	{
		return PosPropagateToOuter(mp, exprhdl, posInput);
	}

	return GPOS_NEW(mp) COrderSpec(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalNLJoin::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CRewindabilitySpec *prsRequired, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG	// ulOptReq
) const
{
	GPOS_ASSERT(
		child_index < 2 &&
		"Required rewindability can be computed on the relational child only");

	// inner child has to be rewindable
	if (1 == child_index)
	{
		if (FFirstChildToOptimize(child_index))
		{
			// for index nested loop joins, inner child is optimized first
			return GPOS_NEW(mp) CRewindabilitySpec(
				CRewindabilitySpec::ErtRewindable, prsRequired->Emht());
		}

		CRewindabilitySpec *prsOuter =
			CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0 /*outer child*/])->Prs();
		CRewindabilitySpec::EMotionHazardType motion_hazard =
			GPOS_FTRACE(EopttraceMotionHazardHandling) &&
					(prsOuter->HasMotionHazard() ||
					 prsRequired->HasMotionHazard())
				? CRewindabilitySpec::EmhtMotion
				: CRewindabilitySpec::EmhtNoMotion;

		return GPOS_NEW(mp) CRewindabilitySpec(
			CRewindabilitySpec::ErtRewindable, motion_hazard);
	}

	GPOS_ASSERT(0 == child_index);

	return PrsPassThru(mp, exprhdl, prsRequired, 0 /*child_index*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::PcrsRequired
//
//	@doc:
//		Compute required output columns of n-th child
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalNLJoin::PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CColRefSet *pcrsRequired, ULONG child_index,
							  CDrvdPropArray *,	 // pdrgpdpCtxt
							  ULONG				 // ulOptReq
)
{
	GPOS_ASSERT(
		child_index < 2 &&
		"Required properties can only be computed on the relational child");

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	pcrs->Include(pcrsRequired);

	// For subqueries in the projection list, the required columns from the outer child
	// are often pushed down to the inner child and are not visible at the top level
	// so we can use the outer refs of the inner child as required from outer child
	if (0 == child_index)
	{
		CColRefSet *outer_refs = exprhdl.DeriveOuterReferences(1);
		pcrs->Include(outer_refs);
	}

	// request inner child of correlated join to provide required inner columns
	if (1 == child_index && FCorrelated())
	{
		pcrs->Include(PdrgPcrInner());
	}

	CColRefSet *pcrsReqd =
		PcrsChildReqd(mp, exprhdl, pcrs, child_index, 2 /*ulScalarIndex*/);
	pcrs->Release();

	return pcrsReqd;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator;
//
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalNLJoin::EpetOrder(CExpressionHandle &exprhdl,
						   const CEnfdOrder *peo) const
{
	GPOS_ASSERT(nullptr != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	if (FSortColsInOuterChild(m_mp, exprhdl, peo->PosRequired()))
	{
		return CEnfdProp::EpetOptional;
	}

	return CEnfdProp::EpetRequired;
}

CEnfdDistribution *
CPhysicalNLJoin::Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
					 CReqdPropPlan *prppInput, ULONG child_index,
					 CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq)
{
	GPOS_ASSERT(2 > child_index);
	GPOS_ASSERT(ulOptReq < UlDistrRequests());

	CEnfdDistribution::EDistributionMatching dmatch =
		Edm(prppInput, child_index, pdrgpdpCtxt, ulOptReq);
	CDistributionSpec *const pdsRequired = prppInput->Ped()->PdsRequired();

	// if expression has to execute on a single host then we need a gather
	if (exprhdl.NeedsSingletonExecution())
	{
		return GPOS_NEW(mp) CEnfdDistribution(
			PdsRequireSingleton(mp, exprhdl, pdsRequired, child_index), dmatch);
	}

	if (exprhdl.HasOuterRefs())
	{
		if (CDistributionSpec::EdtSingleton == pdsRequired->Edt() ||
			CDistributionSpec::EdtStrictReplicated == pdsRequired->Edt())
		{
			return GPOS_NEW(mp) CEnfdDistribution(
				PdsPassThru(mp, exprhdl, pdsRequired, child_index), dmatch);
		}
		return GPOS_NEW(mp) CEnfdDistribution(
			GPOS_NEW(mp)
				CDistributionSpecReplicated(CDistributionSpec::EdtReplicated),
			CEnfdDistribution::EdmSatisfy);
	}

    if (1 == child_index && ulOptReq == 1)
    {
        // compute a matching distribution based on derived distribution of outer child
        CDistributionSpec *pdsOuter =
            CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
        if (CDistributionSpec::EdtHashed == pdsOuter->Edt())
        {
            // require inner child to have matching hashed distribution
            CExpression *pexprScPredicate = exprhdl.PexprScalarExactChild(
                2, true /*error_on_null_return*/);
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
                fSuccess = (nullptr != pexprMatching);
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
                return GPOS_NEW(mp)
                    CEnfdDistribution(pdshashedEquiv, dmatch);
            }
            pdrgpexprMatching->Release();
        }
    }
    return CPhysicalJoin::Ped(mp, exprhdl, prppInput, child_index,
                              pdrgpdpCtxt, ulOptReq);
}

// EOF
