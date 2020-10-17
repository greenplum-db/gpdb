//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLimit.cpp
//
//	@doc:
//		Implementation of limit operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalLimit.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::CPhysicalLimit
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLimit::CPhysicalLimit(CMemoryPool *mp, COrderSpec *pos, BOOL fGlobal,
							   BOOL fHasCount, BOOL fTopLimitUnderDML)
	: CPhysical(mp),
	  m_pos(pos),
	  m_fGlobal(fGlobal),
	  m_fHasCount(fHasCount),
	  m_top_limit_under_dml(fTopLimitUnderDML),
	  m_pcrsSort(NULL)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != pos);

	m_pcrsSort = m_pos->PcrsUsed(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::~CPhysicalLimit
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalLimit::~CPhysicalLimit()
{
	m_pos->Release();
	m_pcrsSort->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalLimit::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		CPhysicalLimit *popLimit = CPhysicalLimit::PopConvert(pop);

		if (popLimit->FGlobal() == m_fGlobal &&
			popLimit->FHasCount() == m_fHasCount)
		{
			// match if order specs match
			return m_pos->Matches(popLimit->m_pos);
		}
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PcrsRequired
//
//	@doc:
//		Columns required by Limit's relational child
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalLimit::PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsRequired, ULONG child_index,
							 CDrvdPropArray *,	// pdrgpdpCtxt
							 ULONG				// ulOptReq
)
{
	GPOS_ASSERT(0 == child_index);

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, *m_pcrsSort);
	pcrs->Union(pcrsRequired);

	CColRefSet *pcrsChildReqd =
		PcrsChildReqd(mp, exprhdl, pcrs, child_index, gpos::ulong_max);
	pcrs->Release();

	return pcrsChildReqd;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalLimit::PosRequired(CMemoryPool *,		  // mp
							CExpressionHandle &,  // exprhdl
							COrderSpec *,		  // posInput
							ULONG
#ifdef GPOS_DEBUG
								child_index
#endif	// GPOS_DEBUG
							,
							CDrvdPropArray *,  // pdrgpdpCtxt
							ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	// limit requires its internal order spec to be satisfied by its child;
	// an input required order to the limit operator is always enforced on
	// top of limit
	m_pos->AddRef();

	return m_pos;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLimit::PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							CDistributionSpec *pdsInput, ULONG child_index,
							CDrvdPropArray *,  // pdrgpdpCtxt
							ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	if (FGlobal())
	{
		// TODO:  - Mar 19, 2012; Cleanup: move this check to the caller
		if (exprhdl.HasOuterRefs())
		{
			return PdsPassThru(mp, exprhdl, pdsInput, child_index);
		}

		CExpression *pexprOffset =
			exprhdl.PexprScalarExactChild(1 /*child_index*/);
		if (!m_fHasCount && CUtils::FScalarConstIntZero(pexprOffset))
		{
			// pass through input distribution if it has no count nor offset and is not
			// a singleton
			if (CDistributionSpec::EdtSingleton != pdsInput->Edt() &&
				CDistributionSpec::EdtStrictSingleton != pdsInput->Edt())
			{
				return PdsPassThru(mp, exprhdl, pdsInput, child_index);
			}

			return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
		}
		if (CDistributionSpec::EdtSingleton == pdsInput->Edt())
		{
			// pass through input distribution if it is a singleton (and it has count or offset)
			return PdsPassThru(mp, exprhdl, pdsInput, child_index);
		}

		// otherwise, require a singleton explicitly
		return GPOS_NEW(mp) CDistributionSpecSingleton();
	}

	// if expression has to execute on a single host then we need a gather
	if (exprhdl.NeedsSingletonExecution())
	{
		return PdsRequireSingleton(mp, exprhdl, pdsInput, child_index);
	}

	// no local limits are generated if there are outer references, so if this
	// is a local limit, there should be no outer references
	GPOS_ASSERT(0 == exprhdl.DeriveOuterReferences()->Size());

	// for local limit, we impose no distribution requirements
	return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalLimit::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							CRewindabilitySpec *prsRequired, ULONG child_index,
							CDrvdPropArray *,  // pdrgpdpCtxt
							ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	if (exprhdl.HasOuterRefs())
	{
		// If the Limit op or its subtree contains an outer ref, then it must
		// request rewindability with a motion hazard (a Blocking Spool) from its
		// subtree. Otherwise, if a streaming Spool is added to the subtree, it will
		// only return tuples it materialized in its first execution (i.e with the
		// first value of the outer ref) for every re-execution. This can produce
		// wrong results.
		// E.g select *, (select 1 from generate_series(1, 10) limit t1.a) from t1;
		return GPOS_NEW(mp) CRewindabilitySpec(prsRequired->Ert(),
											   CRewindabilitySpec::EmhtMotion);
	}

	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PppsRequired
//
//	@doc:
//		Compute required partition propagation of the n-th child
//
//---------------------------------------------------------------------------
CPartitionPropagationSpec *
CPhysicalLimit::PppsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CPartitionPropagationSpec *pppsRequired,
							 ULONG
#ifdef GPOS_DEBUG
								 child_index
#endif
							 ,
							 CDrvdPropArray *,	//pdrgpdpCtxt
							 ULONG				//ulOptReq
)
{
	GPOS_ASSERT(0 == child_index);
	GPOS_ASSERT(NULL != pppsRequired);

	// limit should not push predicate below it as it will generate wrong results
	// for example, the following two queries are not equivalent.
	// Q1: select * from (select * from foo order by a limit 1) x where x.a = 10
	// Q2: select * from (select * from foo where a = 10 order by a limit 1) x

	return CPhysical::PppsRequiredPushThruUnresolvedUnary(
		mp, exprhdl, pppsRequired, CPhysical::EppcProhibited, NULL);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalLimit::PcteRequired(CMemoryPool *,		   //mp,
							 CExpressionHandle &,  //exprhdl,
							 CCTEReq *pcter,
							 ULONG
#ifdef GPOS_DEBUG
								 child_index
#endif
							 ,
							 CDrvdPropArray *,	//pdrgpdpCtxt,
							 ULONG				//ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);
	return PcterPushThru(pcter);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalLimit::FProvidesReqdCols(CExpressionHandle &exprhdl,
								  CColRefSet *pcrsRequired,
								  ULONG	 // ulOptReq
) const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalLimit::PosDerive(CMemoryPool *,	   // mp
						  CExpressionHandle &  // exprhdl
) const
{
	m_pos->AddRef();

	return m_pos;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLimit::PdsDerive(CMemoryPool *,  // mp
						  CExpressionHandle &exprhdl) const
{
	return PdsDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalLimit::PrsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const
{
	return PrsDerivePassThruOuter(mp, exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalLimit::EpetOrder(CExpressionHandle &,	// exprhdl
						  const CEnfdOrder *peo) const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	if (peo->FCompatible(m_pos))
	{
		// required order will be established by the limit operator
		return CEnfdProp::EpetUnnecessary;
	}

	// required order will be enforced on limit's output
	return CEnfdProp::EpetRequired;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::EpetDistribution
//
//	@doc:
//		Return the enforcing type for distribution property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalLimit::EpetDistribution(CExpressionHandle &exprhdl,
								 const CEnfdDistribution *ped) const
{
	GPOS_ASSERT(NULL != ped);

	// get distribution delivered by the limit node
	CDistributionSpec *pds = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds();

	if (ped->FCompatible(pds))
	{
		if (m_fGlobal)
		{
			return CEnfdProp::EpetUnnecessary;
		}

		// prohibit the plan if local limit already delivers the enforced distribution, since
		// otherwise we would create two limits with no intermediate motion operators
		return CEnfdProp::EpetProhibited;
	}

	return CEnfdProp::EpetRequired;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalLimit::EpetRewindability(CExpressionHandle &,		  // exprhdl
								  const CEnfdRewindability *  // per
) const
{
	// rewindability is preserved on operator's output
	return CEnfdProp::EpetOptional;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLimit::OsPrint
//
//	@doc:
//		Print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalLimit::OsPrint(IOstream &os) const
{
	os << SzId() << " " << (*m_pos) << " " << (m_fGlobal ? "global" : "local");

	return os;
}



// EOF
