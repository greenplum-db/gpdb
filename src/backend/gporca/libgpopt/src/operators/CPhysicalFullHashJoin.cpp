//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 VMware by Broadcom
//
//	@filename:
//		CPhysicalFullHashJoin.cpp
//
//	@doc:
//		Implementation of left outer hash join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalFullHashJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysical.h"
#include "gpopt/operators/CPhysicalHashJoin.h"
#include "gpopt/operators/CPhysicalRightOuterHashJoin.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFullHashJoin::CPhysicalFullHashJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalFullHashJoin::CPhysicalFullHashJoin(
	CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
	CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
	BOOL is_null_aware, CXform::EXformId origin_xform)
	: CPhysicalRightOuterHashJoin(mp, pdrgpexprOuterKeys, pdrgpexprInnerKeys,
								  hash_opfamilies, is_null_aware, origin_xform)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalFullHashJoin::~CPhysicalFullHashJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalFullHashJoin::~CPhysicalFullHashJoin() = default;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalJoin::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalFullHashJoin::PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const
{
	GPOS_ASSERT(EopPhysicalFullHashJoin == Eopid());

	// Both left and right side are nullable
	CDistributionSpec *pdsLeft = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
	CDistributionSpec *pdsRight = exprhdl.Pdpplan(1 /*child_index*/)->Pds();

	if (CDistributionSpec::EdtHashed == pdsRight->Edt())
	{
		GPOS_ASSERT(CDistributionSpec::EdtHashed == pdsLeft->Edt());
		CDistributionSpecHashed *pdshashedLeft =
			CDistributionSpecHashed::PdsConvert(pdsLeft);
		CDistributionSpecHashed *pdshashedRight =
			CDistributionSpecHashed::PdsConvert(pdsRight);

		CDistributionSpecHashed *combined_hashed_spec = nullptr;
		if (FSelfJoinWithMatchingJoinKeys(mp, exprhdl))
		{
			// A self join on distributed spec columns will preserve the colocated
			// nulls.
			combined_hashed_spec = pdshashedRight->Combine(mp, pdshashedLeft);
			if (nullptr != combined_hashed_spec)
			{
				return combined_hashed_spec;
			}
		}

		CDistributionSpecHashed *pdshashedLeftCopy =
			pdshashedLeft->Copy(mp, false);
		if (pdshashedLeft->IsCoveredBy(PdrgpexprOuterKeys()) &&
			pdshashedRight->IsCoveredBy(PdrgpexprInnerKeys()))
		{
			// If both sides are hashed on subsets of hash join keys, join's output is
			// distributed on the right spec or (equivalently) on the left spec. So we
			// create a new spec and mark outer and inner as equivalent. Since both
			// sides are nullable, the combined spec's nulls aren't colocated.

			CDistributionSpecHashed *pdshashedRightCopy =
				pdshashedRight->Copy(mp, false);
			combined_hashed_spec =
				pdshashedLeftCopy->Combine(mp, pdshashedRightCopy);
			pdshashedRightCopy->Release();
		}

		if (nullptr != combined_hashed_spec)
		{
			pdshashedLeftCopy->Release();
			return combined_hashed_spec;
		}
		else
		{
			return pdshashedLeftCopy;
		}
	}

	// We don't expect the right child to deliver tainted replicated
	// Tainted replicated doesn't satisfy the distribution request that
	// full hash join sends to its right child -- HASH or coordinator-only
	// SINGLETON. A tainted replicated input would then be enforced
	// into HASH or SINGLETON.
	GPOS_ASSERT(CDistributionSpec::EdtTaintedReplicated != pdsRight->Edt());

	// If the left spec is replicated or universal, return the right spec.
	// Otherwise, return the left spec.
	//
	// In hash join, we always optimize the right child before the left.
	// The request sent to the right child has to be satisfied, whereas
	// the request sent to the left child has to be matched (more strict).
	//
	// So when the left side delivers replicated, the request sent to the
	// left side has to be replicated. And this happens when the right side
	// delivers replicated. In this case, deriving the full hash join
	// property from left or right side would be equivalent.
	//
	// When the left side delivers universal, the request sent to the left
	// side can be replicated or singleton, because universal matches
	// replicated and singleton. If the request sent to the left side is
	// replicated, the right side also delivers replicated. If the request
	// sent to the left side is singleton, the right side may deliver
	// singleton or universal. To summarize, we have three combinations of
	// derived spec pairs from the join children --
	// <universal, replicated>
	// <universal, singleton>
	// <universal, universal>
	// And in all three scenarios, it's safe to derive the full join
	// property from the right spec.
	if (CDistributionSpec::EdtStrictReplicated == pdsLeft->Edt() ||
		CDistributionSpec::EdtUniversal == pdsLeft->Edt())
	{
		pdsRight->AddRef();
		return pdsRight;
	}
	else
	{
		pdsLeft->AddRef();
		return pdsLeft;
	}
}


// Override right outer join, and reuse CPhysical's required partition
// propagation spec
CPartitionPropagationSpec *
CPhysicalFullHashJoin::PppsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CPartitionPropagationSpec *pppsRequired,
									ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const
{
	return CPhysical::PppsRequired(mp, exprhdl, pppsRequired, child_index,
								   pdrgpdpCtxt, ulOptReq);
}

// Override right outer join, and reuse CPhysical's derived partition
// propagation spec
CPartitionPropagationSpec *
CPhysicalFullHashJoin::PppsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	return CPhysical::PppsDerive(mp, exprhdl);
}
// EOF
