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
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalCTEConsumer.h"
#include "gpopt/operators/CPhysicalMotionBroadcast.h"
#include "gpopt/operators/CPhysicalMotionGather.h"
#include "gpopt/operators/CPhysicalMotionRandom.h"

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
	: CPhysical(mp), m_pcrsEmpty(nullptr)
{
	// Sequence generates two distribution requests for its children:
	// (1) If incoming distribution from above is Singleton, pass it through
	//		to all children, otherwise request Non-Singleton on all children
	//
	// (2)	Optimize first child with Any distribution requirement, and compute
	//		distribution request on other children based on derived distribution
	//		of first child:
	//			* If distribution of first child is a Singleton, request Singleton
	//				on all children
	//			* If distribution of first child is a Non-Singleton, request
	//				Non-Singleton on all children

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
	GPOS_ASSERT(nullptr != pcter);
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
	GPOS_ASSERT(nullptr != pcrsRequired);

	// last child must provide required columns
	ULONG arity = exprhdl.Arity();
	GPOS_ASSERT(0 < arity);

	CColRefSet *pcrsChild = exprhdl.DeriveOutputColumns(arity - 1);

	return pcrsChild->ContainsAll(pcrsRequired);
}

// We try and find whether any parent of the CTE consumer (and child of ours)
// derives a distribution spec. The CTE producer is a direct child
// so whatever distribution we derive here will be its distribution.
// We only need to match the distribution of the consumer with the distribution
// of the producer in the number of segments (so there is no distinction between
// singleton segment and singleton master, for example)

static BOOL
FSearchOperationLocality(CMemoryPool *mp, CGroupExpression *pgexpr,
						 COperator::EOperatorId eopid,
						 CUtils::EExecLocalityType *eelt, ULONG visitBit)
{
	GPOS_CHECK_STACK_SIZE;

	if (pgexpr == nullptr)
	{
		return false;
	}

	// recursion limit
	if (pgexpr->Id() > 8)
	{
		return false;
	}

	GPOS_ASSERT(pgexpr->Id() < sizeof(ULONG) * 8);
	const ulong visitMask = 1 << pgexpr->Id();
	if (visitBit & visitMask)
	{
		return false;
	}

	CUtils::EExecLocalityType prevEelt = *eelt;
	CDistributionSpec *pds = nullptr;
	switch (pgexpr->Pop()->Eopid())
	{
		case COperator::EopPhysicalMotionBroadcast:
		{
			CPhysicalMotionBroadcast *motion =
				CPhysicalMotionBroadcast::PopConvert(pgexpr->Pop());
			GPOS_ASSERT(motion != nullptr);
			pds = motion->Pds();
		}
		break;
		case COperator::EopPhysicalMotionGather:
		{
			CPhysicalMotionGather *motion =
				CPhysicalMotionGather::PopConvert(pgexpr->Pop());
			GPOS_ASSERT(motion != nullptr);
			pds = motion->Pds();
		}
		break;
		case COperator::EopPhysicalMotionRandom:
		{
			CPhysicalMotionRandom *motion =
				CPhysicalMotionRandom::PopConvert(pgexpr->Pop());
			GPOS_ASSERT(motion != nullptr);
			pds = motion->Pds();
		}
		break;
		case COperator::EopPhysicalCTEConsumer:
			// NOTE: Presumably the default state is matching the CTE Producer, which
			// we have to give it as a relational child. This is the default
			// value for eelt, so we return true without writing to it
			return true;
			break;
		default:
			break;
	}

	if (pds)
	{
		*eelt = CUtils::ExecLocalityType(pds);
	}

	for (ULONG i = 0; i < pgexpr->Arity(); i++)
	{
		CGroupExpression *cur = (*pgexpr)[i]->PgexprFirst();
		do
		{
			if (FSearchOperationLocality(mp, cur, eopid, eelt,
										 visitBit | visitMask))
			{
				return true;
			}
		} while ((cur = (*pgexpr)[i]->PgexprNext(cur)) != nullptr);
	}

	*eelt = prevEelt;

	return false;
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
CPhysicalSequence::PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							   CDistributionSpec *pdsRequired,
							   ULONG child_index, CDrvdPropArray *pdrgpdpCtxt,
							   ULONG ulOptReq) const
{
	GPOS_ASSERT(2 == exprhdl.Arity());
	GPOS_ASSERT(child_index < exprhdl.Arity());
	GPOS_ASSERT(ulOptReq < UlDistrRequests());

	if (0 == ulOptReq)
	{
		if (CDistributionSpec::EdtSingleton == pdsRequired->Edt() ||
			CDistributionSpec::EdtStrictSingleton == pdsRequired->Edt())
		{
			// incoming request is a singleton, request singleton on all children
			CDistributionSpecSingleton *pdss =
				CDistributionSpecSingleton::PdssConvert(pdsRequired);
			return GPOS_NEW(mp) CDistributionSpecSingleton(pdss->Est());
		}

		// incoming request is a non-singleton, request non-singleton on all children
		return GPOS_NEW(mp) CDistributionSpecNonSingleton();
	}
	GPOS_ASSERT(1 == ulOptReq);

	if (0 == child_index)
	{
		// CTE Producer distribution must match the CTE consumer distribution,
		// so we traverse the tree, track the distribution nodes, and enforce
		// the distribution which satisfies the locality. pdsRequired is the
		// default derived distribution for CTE Producer, so unless there are
		// motions that cause a cardinality mis-match, we can use that.
		CUtils::EExecLocalityType eelt = CUtils::ExecLocalityType(pdsRequired);

		if (FSearchOperationLocality(
				m_mp, exprhdl.Pgexpr(),
				COperator::EOperatorId::EopPhysicalCTEProducer, &eelt, 0) &&
			CUtils::ExecLocalityType(pdsRequired) != eelt)
		{
			// CTE Consumer exists as a child and eelt is populated with the
			// locality. We force the CTE Producer child to have the same
			// number of nodes (the physical location does not matter)

			switch (eelt)
			{
				case CUtils::EeltMaster:
				case CUtils::EeltSingleton:
					return GPOS_NEW(mp) CDistributionSpecSingleton();
				case CUtils::EeltSegments:
					return GPOS_NEW(mp) CDistributionSpecNonSingleton();
				default:
					GPOS_ASSERT(false);
			}
		}
		else
		{
			return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
		}
	}

	// get derived plan properties of first child
	CDrvdPropPlan *pdpplan = CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0]);
	CDistributionSpec *pds = pdpplan->Pds();

	if (pds->FSingletonOrStrictSingleton())
	{
		// first child is singleton, request singleton distribution on second child
		CDistributionSpecSingleton *pdss =
			CDistributionSpecSingleton::PdssConvert(pds);
		return GPOS_NEW(mp) CDistributionSpecSingleton(pdss->Est());
	}

	if (CDistributionSpec::EdtUniversal == pds->Edt())
	{
		// first child is universal, impose no requirements on second child
		return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
	}

	// first child is non-singleton, request a non-singleton distribution on second child
	return GPOS_NEW(mp) CDistributionSpecNonSingleton();
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
	GPOS_ASSERT(nullptr != peo);

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
