//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalSequence.cpp
//
//	@doc:
//		Implementation of physical sequence operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalSequence.h"

#include "gpos/base.h"

#include "gpopt/base/CCTEReq.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::CPhysicalSequence
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalSequence::CPhysicalSequence(CMemoryPool *mp)
	: CPhysical(mp), m_pcrsEmpty(NULL)
{
	// Sequence generates two distribution requests for its children:
	// (1) If incoming distribution from above is Singleton, pass it through
	//		to all children, otherwise request Non-Singleton (Non-Replicated)
	//		on all children
	//
	// (2)	Optimize first child with Any distribution requirement, and compute
	//		distribution request on other children based on derived distribution
	//		of first child:
	//			* If distribution of first child is a Singleton, request Singleton
	//				on the second child
	//			* If distribution of first child is Replicated, request Replicated
	//				on the second child
	//			* Otherwise, request Non-Singleton (Non-Replicated) on the second
	//				child

	SetDistrRequests(2);

	m_pcrsEmpty = GPOS_NEW(mp) CColRefSet(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::~CPhysicalSequence
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalSequence::~CPhysicalSequence()
{
	m_pcrsEmpty->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalSequence::Matches(COperator *pop) const
{
	return Eopid() == pop->Eopid();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PcrsRequired
//
//	@doc:
//		Compute required output columns of n-th child
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalSequence::PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								CColRefSet *pcrsRequired, ULONG child_index,
								CDrvdPropArray *,  // pdrgpdpCtxt
								ULONG			   // ulOptReq
)
{
	const ULONG arity = exprhdl.Arity();
	if (child_index == arity - 1)
	{
		// request required columns from the last child of the sequence
		return PcrsChildReqd(mp, exprhdl, pcrsRequired, child_index,
							 gpos::ulong_max);
	}

	m_pcrsEmpty->AddRef();
	GPOS_ASSERT(0 == m_pcrsEmpty->Size());

	return m_pcrsEmpty;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PppsRequired
//
//	@doc:
//		Compute required partition propagation of the n-th child
//
//---------------------------------------------------------------------------
CPartitionPropagationSpec *
CPhysicalSequence::PppsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								CPartitionPropagationSpec *pppsRequired,
								ULONG child_index,
								CDrvdPropArray *,  //pdrgpdpCtxt,
								ULONG			   //ulOptReq
)
{
	GPOS_ASSERT(NULL != pppsRequired);

	return CPhysical::PppsRequiredPushThruNAry(mp, exprhdl, pppsRequired,
											   child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalSequence::PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								CCTEReq *pcter, ULONG child_index,
								CDrvdPropArray *pdrgpdpCtxt,
								ULONG  //ulOptReq
) const
{
	GPOS_ASSERT(NULL != pcter);
	if (child_index < exprhdl.Arity() - 1)
	{
		return pcter->PcterAllOptional(mp);
	}

	// derived CTE maps from previous children
	CCTEMap *pcmCombined = PcmCombine(mp, pdrgpdpCtxt);

	// pass the remaining requirements that have not been resolved
	CCTEReq *pcterUnresolved =
		pcter->PcterUnresolvedSequence(mp, pcmCombined, pdrgpdpCtxt);
	pcmCombined->Release();

	return pcterUnresolved;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::FProvidesReqdCols
//
//	@doc:
//		Helper for checking if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalSequence::FProvidesReqdCols(CExpressionHandle &exprhdl,
									 CColRefSet *pcrsRequired,
									 ULONG	// ulOptReq
) const
{
	GPOS_ASSERT(NULL != pcrsRequired);

	// last child must provide required columns
	ULONG arity = exprhdl.Arity();
	GPOS_ASSERT(0 < arity);

	CColRefSet *pcrsChild = exprhdl.DeriveOutputColumns(arity - 1);

	return pcrsChild->ContainsAll(pcrsRequired);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSequence::PdsRequired(CMemoryPool *mp,
							   CExpressionHandle &
#ifdef GPOS_DEBUG
								   exprhdl
#endif	// GPOS_DEBUG
							   ,
							   CDistributionSpec *pdsRequired,
							   ULONG child_index, CDrvdPropArray *pdrgpdpCtxt,
							   ULONG ulOptReq) const
{
	GPOS_ASSERT(2 == exprhdl.Arity());
	GPOS_ASSERT(child_index < exprhdl.Arity());
	GPOS_ASSERT(ulOptReq < UlDistrRequests());

	// The following distribution requests ensure matching number of
	// producers and consumers. Any mismatch, either more producers
	// than consumers, or more consumers than producers, will cause
	// the query to hang

	// 1st request
	if (0 == ulOptReq)
	{
		// If the incoming request is a singleton, request singleton
		// on ALL the children. No need to check strict singleton,
		// cause strict singleton is a derived only spec.
		if (CDistributionSpec::EdtSingleton == pdsRequired->Edt())
		{
			CDistributionSpecSingleton *pdss =
				CDistributionSpecSingleton::PdssConvert(pdsRequired);
			return GPOS_NEW(mp) CDistributionSpecSingleton(pdss->Est());
		}

		// Otherwise, request non-singleton (excluding replicated)
		// on ALL the children.

		// If the input is hashed or randomly distributed, no motion
		// will be enforced. Both the producer and the consumer are
		// executed on all the segments. That is, no mismatch between
		// the number of producers and consumers.

		// If the input is replicated, a hash filter will be applied
		// to the input. The hash filter resolves the duplicate hazard,
		// and now the producer is randomly distributed. The consumer
		// will continue to be executed on all the segments. Again, no
		// mismatch between the number of producers and consumers.
		return GPOS_NEW(mp)
			CDistributionSpecNonSingleton(false /* fAllowReplicated */);
	}

	// 2nd request
	GPOS_ASSERT(1 == ulOptReq);

	if (0 == child_index)
	{
		// Request Any on the first child
		return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
	}

	// Get derived plan properties of first child
	CDrvdPropPlan *pdpplan = CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0]);
	// Because we request Any on the first child, no motion could
	// have been enforced. The derived spec of the first child is
	// what it delivers in the first place.
	CDistributionSpec *pds = pdpplan->Pds();

	// If the first child is singleton, request singleton on the
	// second child. This request is identical to the 1st request
	// (ulOptReq = 0), where we request singleton on ALL the
	// children, and would eventually be deduplicated.
	if (pds->FSingletonOrStrictSingleton())
	{
		CDistributionSpecSingleton *pdss =
			CDistributionSpecSingleton::PdssConvert(pds);
		return GPOS_NEW(mp) CDistributionSpecSingleton(pdss->Est());
	}

	// If the first child is universal, request Any on the second
	// child. This is because if the producer is universally
	// available, no matter what the locality of the consumer
	// is, the consumer would be able to access the shared data
	if (CDistributionSpec::EdtUniversal == pds->Edt())
	{
		return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
	}

	// If the first child is replicated, request replicated on the
	// second child. This way, the producer is only executed on one
	// segment, and so is the consumer. The plan would look like:
	// Gather Motion 1:1  (slice1; segments: 1)
	//   ->  Sequence
	//         ->  Shared Scan (share slice:id 1:1)
	//               ->  Seq Scan on rep
	//         ->  Broadcast Motion 3:1  (slice2; segments: 3)
	//             ...
	//             ...
	//             ->  Gather Motion 1:1  (sliceN; segments: 1)
	//                   ->  Shared Scan (share slice:id N:1)
	if (CDistributionSpec::EdtStrictReplicated == pds->Edt() ||
		CDistributionSpec::EdtTaintedReplicated == pds->Edt())
	{
		return GPOS_NEW(mp)
			CDistributionSpecReplicated(CDistributionSpec::EdtReplicated);
	}

	// Request non-singleton (excluding replicated) on the second
	// child
	return GPOS_NEW(mp)
		CDistributionSpecNonSingleton(false /* fAllowReplicated */);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalSequence::PosRequired(CMemoryPool *mp,
							   CExpressionHandle &,	 // exprhdl,
							   COrderSpec *,		 // posRequired,
							   ULONG,				 // child_index,
							   CDrvdPropArray *,	 // pdrgpdpCtxt
							   ULONG				 // ulOptReq
) const
{
	// no order requirement on the children
	return GPOS_NEW(mp) COrderSpec(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PrsRequired
//
//	@doc:
//		Compute required rewindability order of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalSequence::PrsRequired(CMemoryPool *,		 // mp,
							   CExpressionHandle &,	 // exprhdl,
							   CRewindabilitySpec *prsRequired,
							   ULONG,			  // child_index,
							   CDrvdPropArray *,  // pdrgpdpCtxt
							   ULONG			  // ulOptReq
) const
{
	// TODO: shardikar; Handle outer refs in the subtree correctly, by passing
	// "Rescannable' Also, maybe it should pass through the prsRequired, since it
	// doesn't materialize any results? It's important to consider performance
	// consequences of that also.
	return GPOS_NEW(m_mp)
		CRewindabilitySpec(CRewindabilitySpec::ErtNone, prsRequired->Emht());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalSequence::PosDerive(CMemoryPool *,	 // mp,
							 CExpressionHandle &exprhdl) const
{
	// pass through sort order from last child
	const ULONG arity = exprhdl.Arity();

	GPOS_ASSERT(1 <= arity);

	COrderSpec *pos = exprhdl.Pdpplan(arity - 1 /*child_index*/)->Pos();
	pos->AddRef();

	return pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSequence::PdsDerive(CMemoryPool *,	 // mp,
							 CExpressionHandle &exprhdl) const
{
	// pass through distribution from last child
	const ULONG arity = exprhdl.Arity();

	GPOS_ASSERT(1 <= arity);

	CDistributionSpec *pds = exprhdl.Pdpplan(arity - 1 /*child_index*/)->Pds();
	pds->AddRef();

	return pds;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalSequence::PrsDerive(CMemoryPool *,	 //mp
							 CExpressionHandle &exprhdl) const
{
	const ULONG arity = exprhdl.Arity();
	GPOS_ASSERT(1 <= arity);

	CRewindabilitySpec::EMotionHazardType motion_hazard =
		CRewindabilitySpec::EmhtNoMotion;
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CRewindabilitySpec *prs = exprhdl.Pdpplan(ul)->Prs();
		if (prs->HasMotionHazard())
		{
			motion_hazard = CRewindabilitySpec::EmhtMotion;
			break;
		}
	}

	// TODO: shardikar; Fix this implementation. Although CPhysicalSequence is
	// not rewindable, all its children might be rewindable. This implementation
	// ignores the rewindability of the op's children
	return GPOS_NEW(m_mp)
		CRewindabilitySpec(CRewindabilitySpec::ErtNone, motion_hazard);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::EpetOrder
//
//	@doc:
//		Return the enforcing type for the order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSequence::EpetOrder(CExpressionHandle &exprhdl,
							 const CEnfdOrder *peo) const
{
	GPOS_ASSERT(NULL != peo);

	// get order delivered by the sequence node
	COrderSpec *pos = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pos();

	if (peo->FCompatible(pos))
	{
		// required order will be established by the sequence operator
		return CEnfdProp::EpetUnnecessary;
	}

	// required distribution will be enforced on sequence's output
	return CEnfdProp::EpetRequired;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSequence::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalSequence::EpetRewindability(CExpressionHandle &,		 // exprhdl
									 const CEnfdRewindability *	 // per
) const
{
	// rewindability must be enforced on operator's output
	return CEnfdProp::EpetRequired;
}


// EOF
