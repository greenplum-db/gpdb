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

#include "gpopt/operators/CPhysicalInnerNLJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CCastUtils.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"


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
CPhysicalInnerNLJoin::~CPhysicalInnerNLJoin() = default;



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
CPhysicalInnerNLJoin::PdsRequired(CMemoryPool *mp GPOS_UNUSED,
								  CExpressionHandle &exprhdl GPOS_UNUSED,
								  CDistributionSpec *,	//pdsRequired,
								  ULONG child_index GPOS_UNUSED,
								  CDrvdPropArray *pdrgpdpCtxt GPOS_UNUSED,
								  ULONG	 // ulOptReq
) const
{
	GPOS_RAISE(
		CException::ExmaInvalid, CException::ExmiInvalid,
		GPOS_WSZ_LIT(
			"PdsRequired should not be called for CPhysicalInnerNLJoin"));
	return nullptr;
}

CEnfdDistribution *
CPhysicalInnerNLJoin::Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
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

	if (GPOS_FTRACE(EopttraceDisableReplicateInnerNLJOuterChild) ||
		0 == ulOptReq)
	{
		if (1 == child_index)
		{
			CEnfdDistribution *pEnfdHashedDistribution =
				CPhysicalJoin::PedInnerHashedFromOuterHashed(
					mp, exprhdl, dmatch, (*pdrgpdpCtxt)[0]);
			if (pEnfdHashedDistribution)
				return pEnfdHashedDistribution;
		}
		return CPhysicalJoin::Ped(mp, exprhdl, prppInput, child_index,
								  pdrgpdpCtxt, ulOptReq);
	}
	GPOS_ASSERT(1 == ulOptReq);

	if (0 == child_index)
	{
		return GPOS_NEW(mp) CEnfdDistribution(
			GPOS_NEW(mp)
				CDistributionSpecReplicated(CDistributionSpec::EdtReplicated),
			dmatch);
	}

	// compute a matching distribution based on derived distribution of outer child
	CDistributionSpec *pdsOuter =
		CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
	if (CDistributionSpec::EdtUniversal == pdsOuter->Edt())
	{
		// first child is universal, request second child to execute on a single host to avoid duplicates
		return GPOS_NEW(mp) CEnfdDistribution(
			GPOS_NEW(mp) CDistributionSpecSingleton(), dmatch);
	}

	return GPOS_NEW(mp)
		CEnfdDistribution(GPOS_NEW(mp) CDistributionSpecNonSingleton(), dmatch);
}

// EOF
