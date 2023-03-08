//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalComputeScalar.cpp
//
//	@doc:
//		Implementation of ComputeScalar operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalComputeScalar.h"

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"

#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecRouted.h"
#include "gpopt/base/CDistributionSpecStrictSingleton.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::CPhysicalComputeScalar
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalComputeScalar::CPhysicalComputeScalar(CMemoryPool *mp) : CPhysical(mp)
{
	// When ComputeScalar does not contain volatile functions and includes no outer references, or if the
	// parent node explicitly allows outer refs, we create two optimization requests to enforce
	// distribution of its child:
	// (1) Any: impose no distribution requirement on the child in order to push scalar computation below
	// Motions, and then enforce required distribution on top of ComputeScalar if needed
	// (2) Pass-Thru: impose distribution requirement on child, and then perform scalar computation after
	// Motions are enforced, this is more efficient for Master-Only plans below ComputeScalar

	// Otherwise, correlated execution has to be enforced.
	// In this case, we create two child optimization requests to guarantee correct evaluation of parameters
	// (1) Broadcast
	// (2) Singleton

	SetDistrRequests(2 /*ulDistrReqs*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::~CPhysicalComputeScalar
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalComputeScalar::~CPhysicalComputeScalar() = default;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalComputeScalar::Matches(COperator *pop) const
{
	// ComputeScalar doesn't contain any members as of now
	return Eopid() == pop->Eopid();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//		we only compute required columns for the relational child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalComputeScalar::PcrsRequired(CMemoryPool *mp,
									 CExpressionHandle &exprhdl,
									 CColRefSet *pcrsRequired,
									 ULONG child_index,
									 CDrvdPropArray *,	// pdrgpdpCtxt
									 ULONG				// ulOptReq
)
{
	GPOS_ASSERT(
		0 == child_index &&
		"Required properties can only be computed on the relational child");

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, *pcrsRequired);
	CColRefSet *pcrsChildReqd =
		PcrsChildReqd(mp, exprhdl, pcrs, child_index, 1 /*ulScalarIndex*/);
	pcrs->Release();

	return pcrsChildReqd;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalComputeScalar::PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									COrderSpec *posRequired, ULONG child_index,
									CDrvdPropArray *,  // pdrgpdpCtxt
									ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	CColRefSet *pcrsSort = posRequired->PcrsUsed(m_mp);
	BOOL fUsesDefinedCols = FUnaryUsesDefinedColumns(pcrsSort, exprhdl);
	pcrsSort->Release();

	if (fUsesDefinedCols)
	{
		// if required order uses any column defined by ComputeScalar, we cannot
		// request it from child, and we pass an empty order spec;
		// order enforcer function takes care of enforcing this order on top of
		// ComputeScalar operator
		return GPOS_NEW(mp) COrderSpec(mp);
	}

	// otherwise, we pass through required order
	return PosPassThru(mp, exprhdl, posRequired, child_index);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalComputeScalar::PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CDistributionSpec *pdsRequired,
									ULONG child_index,
									CDrvdPropArray *,  // pdrgpdpCtxt
									ULONG ulOptReq) const
{
	GPOS_ASSERT(0 == child_index);
	GPOS_ASSERT(2 > ulOptReq);
	CDistributionSpec::EDistributionType edtRequired = pdsRequired->Edt();
	// check whether we need singleton execution - but if the parent explicitly
	// allowed outer refs in an "ANY" request, then that alone doesn't qualify
	// as a reason to request singleton
	if (exprhdl.NeedsSingletonExecution() ||
		!(CDistributionSpec::EdtAny == edtRequired &&
		  (CDistributionSpecAny::PdsConvert(pdsRequired))->FAllowOuterRefs()))
	{
		// check if singleton/replicated distribution needs to be requested
		CDistributionSpec *pds = PdsRequireSingletonOrReplicated(
			mp, exprhdl, pdsRequired, child_index, ulOptReq);
		if (nullptr != pds)
		{
			return pds;
		}
	}

	// if a Project operator has a call to a set function, passing a Random distribution through this
	// Project may have the effect of not distributing the results of the set function to all nodes,
	// but only to the nodes on which first child of the Project is distributed.
	// to avoid that, we don't push the distribution requirement in this case and thus, for a random
	// distribution, the result of the set function is spread uniformly over all nodes
	if (exprhdl.DeriveHasNonScalarFunction(1))
	{
		return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
	}

	// if required distribution uses any defined column, it has to be enforced on top of ComputeScalar,
	// in this case, we request Any distribution from the child
	if (CDistributionSpec::EdtHashed == edtRequired)
	{
		CDistributionSpecHashed *pdshashed =
			CDistributionSpecHashed::PdsConvert(pdsRequired);
		CColRefSet *pcrs = pdshashed->PcrsUsed(m_mp);
		BOOL fUsesDefinedCols = FUnaryUsesDefinedColumns(pcrs, exprhdl);
		pcrs->Release();
		if (fUsesDefinedCols)
		{
			return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
		}
	}

	if (CDistributionSpec::EdtRouted == edtRequired)
	{
		CDistributionSpecRouted *pdsrouted =
			CDistributionSpecRouted::PdsConvert(pdsRequired);
		CColRefSet *pcrs = GPOS_NEW(m_mp) CColRefSet(m_mp);
		pcrs->Include(pdsrouted->Pcr());
		BOOL fUsesDefinedCols = FUnaryUsesDefinedColumns(pcrs, exprhdl);
		pcrs->Release();
		if (fUsesDefinedCols)
		{
			return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
		}
	}

	// in case of DML Insert on randomly distributed table, a motion operator
	// must be enforced on top of compute scalar if the children does not provide
	// strict random spec. strict random request is not pushed
	// down through the physical childs of compute scalar as the scalar
	// project list of the compute scalar can have TVF and if the request
	// was pushed down through physical child, data projected by the scalar
	// project list will not be redistributed and will be inserted into a single
	// segment. random motion with non universal child delivers strict random spec,
	// so in case a motion node was added it will satisfy the strict random
	// requested by DML insert
	if (CDistributionSpec::EdtStrictRandom == pdsRequired->Edt())
	{
		return GPOS_NEW(mp) CDistributionSpecRandom();
	}

	if (0 == ulOptReq)
	{
		// Req0: required distribution will be enforced on top of ComputeScalar
		return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
	}

	// Req1: required distribution will be enforced on top of ComputeScalar's child
	return PdsPassThru(mp, exprhdl, pdsRequired, child_index);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalComputeScalar::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
									CDrvdPropArray *,  // pdrgpdpCtxt
									ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);
	if (prsRequired->IsOriginNLJoin())
	{
		CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(
			CRewindabilitySpec::ErtNone, prsRequired->Emht());
		return prs;
	}
	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalComputeScalar::PcteRequired(CMemoryPool *,		   //mp,
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
//		CPhysicalComputeScalar::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalComputeScalar::FProvidesReqdCols(CExpressionHandle &exprhdl,
										  CColRefSet *pcrsRequired,
										  ULONG	 // ulOptReq
) const
{
	GPOS_ASSERT(nullptr != pcrsRequired);
	GPOS_ASSERT(2 == exprhdl.Arity());

	CColRefSet *pcrs = GPOS_NEW(m_mp) CColRefSet(m_mp);
	// include defined columns by scalar project list
	pcrs->Union(exprhdl.DeriveDefinedColumns(1));

	// include output columns of the relational child
	pcrs->Union(exprhdl.DeriveOutputColumns(0 /*child_index*/));

	BOOL fProvidesCols = pcrs->ContainsAll(pcrsRequired);
	pcrs->Release();

	return fProvidesCols;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalComputeScalar::PosDerive(CMemoryPool *,  // mp
								  CExpressionHandle &exprhdl) const
{
	return PosDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalComputeScalar::PdsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	CDistributionSpec *pds = exprhdl.Pdpplan(0 /*child_index*/)->Pds();

	if (CDistributionSpec::EdtStrictReplicated == pds->Edt() &&
		IMDFunction::EfsVolatile ==
			exprhdl.DeriveScalarFunctionProperties(1)->Efs())
	{
		return GPOS_NEW(mp) CDistributionSpecReplicated(
			CDistributionSpec::EdtTaintedReplicated);
	}

	if (CDistributionSpec::EdtUniversal == pds->Edt() &&
		IMDFunction::EfsVolatile ==
			exprhdl.DeriveScalarFunctionProperties(1)->Efs())
	{
		if (COptCtxt::PoctxtFromTLS()->OptimizeDMLQueryWithSingletonSegment())
		{
			return GPOS_NEW(mp) CDistributionSpecStrictSingleton(
				CDistributionSpecSingleton::EstSegment);
		}
		return GPOS_NEW(mp) CDistributionSpecStrictSingleton(
			CDistributionSpecSingleton::EstMaster);
	}

	pds->AddRef();

	return pds;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalComputeScalar::PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	CRewindabilitySpec *prsChild = PrsDerivePassThruOuter(mp, exprhdl);

	if (exprhdl.DeriveHasNonScalarFunction(1) ||
		IMDFunction::EfsVolatile ==
			exprhdl.DeriveScalarFunctionProperties(1)->Efs())
	{
		// ComputeScalar is not rewindable if it has non-scalar/volatile functions in project list
		CRewindabilitySpec *prs = GPOS_NEW(mp) CRewindabilitySpec(
			CRewindabilitySpec::ErtRescannable, prsChild->Emht());
		prsChild->Release();
		return prs;
	}

	return prsChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalComputeScalar::EpetOrder(CExpressionHandle &exprhdl,
								  const CEnfdOrder *peo) const
{
	GPOS_ASSERT(nullptr != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	COrderSpec *pos = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pos();
	if (peo->FCompatible(pos))
	{
		return CEnfdProp::EpetUnnecessary;
	}

	// Sort has to go above ComputeScalar if sort columns use any column
	// defined by ComputeScalar, otherwise, Sort can either go above or below ComputeScalar
	CColRefSet *pcrsSort = peo->PosRequired()->PcrsUsed(m_mp);
	BOOL fUsesDefinedCols = FUnaryUsesDefinedColumns(pcrsSort, exprhdl);
	pcrsSort->Release();
	if (fUsesDefinedCols)
	{
		return CEnfdProp::EpetRequired;
	}

	return CEnfdProp::EpetOptional;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalComputeScalar::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalComputeScalar::EpetRewindability(CExpressionHandle &exprhdl,
										  const CEnfdRewindability *per) const
{
	if (per->PrsRequired()->IsOriginNLJoin())
	{
		return CEnfdProp::EpetRequired;
	}

	CColRefSet *pcrsUsed = exprhdl.DeriveUsedColumns(1);
	CColRefSet *pcrsCorrelatedApply = exprhdl.DeriveCorrelatedApplyColumns();
	if (!pcrsUsed->IsDisjoint(pcrsCorrelatedApply))
	{
		// columns are used from inner children of correlated-apply expressions,
		// this means that a subplan occurs below the Project operator,
		// in this case, rewindability needs to be enforced on operator's output

		return CEnfdProp::EpetRequired;
	}

	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		// required rewindability is already provided
		return CEnfdProp::EpetUnnecessary;
	}

	// rewindability is enforced on operator's output
	return CEnfdProp::EpetRequired;
}
// EOF
