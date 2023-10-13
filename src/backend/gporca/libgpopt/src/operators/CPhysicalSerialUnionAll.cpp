//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalSerialUnionAll.cpp
//
//	@doc:
//		Implementation of physical union all operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalSerialUnionAll.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/CDrvdPropCtxtPlan.h"
#include "gpopt/exception.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarIdent.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSerialUnionAll::CPhysicalSerialUnionAll
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalSerialUnionAll::CPhysicalSerialUnionAll(
	CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
	CColRef2dArray *pdrgpdrgpcrInput)
	: CPhysicalUnionAll(mp, pdrgpcrOutput, pdrgpdrgpcrInput)
{
	// UnionAll creates 3 distribution requests to enforce
	// distribution of its children:
	//
	// Request 1: HASH
	// Pass hashed distribution (requested from above) to child
	// operators and match request exactly
	//
	// Request 2: NON-SINGLETON, matching_dist
	// Request NON-SINGLETON from the outer child, and match the
	// requests on the rest children based what dist spec the outer
	// child NOW delivers (derived from property plan). Note, the
	// NON-SINGLETON that we request from the outer child is not
	// satisfiable by REPLICATED.
	//
	// Request 3: ANY, matching_dist
	// Request ANY distribution from the outer child, and match the
	// requests on the rest children based on what dist spec the outer
	// child delivers. Note, no enforcement should ever be applied to
	// the outer child, because ANY is satisfiable by all specs.
	//
	// If request 1 falls through, request 3 serves as the
	// backup request. Duplicate requests would eventually be
	// deduplicated.

	SetDistrRequests(3 /*ulDistrReq*/);
	GPOS_ASSERT(0 < UlDistrRequests());
}

CPhysicalSerialUnionAll::~CPhysicalSerialUnionAll() = default;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalSerialUnionAll::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalSerialUnionAll::PdsRequired(
	CMemoryPool *mp, CExpressionHandle &exprhdl, CDistributionSpec *pdsRequired,
	ULONG child_index, CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const
{
	GPOS_ASSERT(nullptr != PdrgpdrgpcrInput());
	GPOS_ASSERT(child_index < PdrgpdrgpcrInput()->Size());
	GPOS_ASSERT(3 > ulOptReq);

	// First check if we have to request SINGLETON or REPLICATED
	CDistributionSpec *pds = PdsRequireSingletonOrReplicated(
		mp, exprhdl, pdsRequired, child_index, ulOptReq);
	if (nullptr != pds)
	{
		return pds;
	}

	// Request 1: HASH
	// This request applies to all union all children
	if (0 == ulOptReq && CDistributionSpec::EdtHashed == pdsRequired->Edt())
	{
		// attempt passing requested hashed distribution to children
		CDistributionSpecHashed *pdshashed = PdshashedPassThru(
			mp, CDistributionSpecHashed::PdsConvert(pdsRequired), child_index);
		if (nullptr != pdshashed)
		{
			return pdshashed;
		}
	}

	if (0 == child_index)
	{
		if (1 == ulOptReq)
		{
			// Request 2: NON-SINGLETON from outer child
			return GPOS_NEW(mp) CDistributionSpecNonSingleton(
				false /*fAllowReplicated*/, true /*fAllowEnforced*/);
		}
		else
		{
			// Request 3: ANY from outer child
			return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
		}
	}

	// inspect distribution delivered by outer child
	CDistributionSpec *pdsOuter =
		CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();

	if (CDistributionSpec::EdtSingleton == pdsOuter->Edt() ||
		CDistributionSpec::EdtStrictSingleton == pdsOuter->Edt())
	{
		// outer child is Singleton, require inner child to have matching Singleton distribution
		return CPhysical::PdssMatching(
			mp, CDistributionSpecSingleton::PdssConvert(pdsOuter));
	}

	if (CDistributionSpec::EdtUniversal == pdsOuter->Edt())
	{
		// require inner child to be on a single host in order to avoid
		// duplicate values when doing UnionAll operation with Universal outer child
		// Example: select 1 union all select i from x;
		return GPOS_NEW(mp) CDistributionSpecSingleton();
	}

	if (CDistributionSpec::EdtStrictReplicated == pdsOuter->Edt() ||
		CDistributionSpec::EdtTaintedReplicated == pdsOuter->Edt())
	{
		// outer child is replicated, require inner child to be replicated
		return GPOS_NEW(mp)
			CDistributionSpecReplicated(CDistributionSpec::EdtReplicated);
	}

	// outer child is non-replicated and is distributed across segments,
	// we need to the inner child to be distributed across segments that does
	// not generate duplicate results. That is, inner child should not be replicated.

	return GPOS_NEW(mp) CDistributionSpecNonSingleton(
		false /*fAllowReplicated*/, true /*fAllowEnforced*/);
}

// EOF
