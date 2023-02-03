//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CPhysicalRightOuterHashJoin.cpp
//
//	@doc:
//		Implementation of right outer hash join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalRightOuterHashJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/operators/CExpressionHandle.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRightOuterHashJoin::CPhysicalRightOuterHashJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalRightOuterHashJoin::CPhysicalRightOuterHashJoin(
	CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
	CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
	BOOL is_null_aware, CXform::EXformId origin_xform)
	: CPhysicalHashJoin(mp, pdrgpexprOuterKeys, pdrgpexprInnerKeys,
						hash_opfamilies, is_null_aware, origin_xform)
{
	ULONG ulDistrReqs = 1 + NumDistrReq();
	SetDistrRequests(ulDistrReqs);
	SetPartPropagateRequests(2);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRightOuterHashJoin::~CPhysicalRightOuterHashJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalRightOuterHashJoin::~CPhysicalRightOuterHashJoin() = default;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRightOuterHashJoin::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CEnfdDistribution *
CPhysicalRightOuterHashJoin::Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
								 CReqdPropPlan *prppInput, ULONG child_index,
								 CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq)
{
	// create the following requests:
	// 1) hash-hash (provided by CPhysicalHashJoin::Ped)
	// 2) singleton-singleton
	//
	// We also could create a replicated-hashed and replicated-non-singleton request, but that isn't a promising
	// alternative as we would be broadcasting the outer side. In that case, an LOJ would be better.

	CDistributionSpec *const pdsInput = prppInput->Ped()->PdsRequired();
	CEnfdDistribution::EDistributionMatching dmatch =
		Edm(prppInput, child_index, pdrgpdpCtxt, ulOptReq);

	if (exprhdl.NeedsSingletonExecution() || exprhdl.HasOuterRefs())
	{
		return GPOS_NEW(mp) CEnfdDistribution(
			PdsRequireSingleton(mp, exprhdl, pdsInput, child_index), dmatch);
	}

	const ULONG ulHashDistributeRequests = NumDistrReq();

	if (ulOptReq < ulHashDistributeRequests)
	{
		// requests 1 .. N are (redistribute, redistribute)
		CDistributionSpec *pds = PdsRequiredRedistribute(
			mp, exprhdl, pdsInput, child_index, pdrgpdpCtxt, ulOptReq);
		if (CDistributionSpec::EdtHashed == pds->Edt())
		{
			CDistributionSpecHashed *pdsHashed =
				CDistributionSpecHashed::PdsConvert(pds);
			pdsHashed->ComputeEquivHashExprs(mp, exprhdl);
		}
		return GPOS_NEW(mp) CEnfdDistribution(pds, dmatch);
	}
	GPOS_ASSERT(ulOptReq == NumDistrReq());
	return GPOS_NEW(mp) CEnfdDistribution(
		PdsRequiredSingleton(mp, exprhdl, pdsInput, child_index, pdrgpdpCtxt),
		dmatch);
}

void
CPhysicalRightOuterHashJoin::CreateOptRequests(CMemoryPool *mp)
{
	CreateHashRedistributeRequests(mp);

	// given an optimization context, Right Hash Join creates 2 optimization requests
	// to enforce distribution of its children:
	// Req(1 to N) (redistribute, redistribute), where we request the first hash join child
	//              to be distributed on single hash join keys separately, as well as the set
	//              of all hash join keys, the second hash join child is always required to
	// 				match the distribution returned by first child
	// Req(N + 1) (singleton, singleton)
	ULONG ulDistrReqs = 1 + NumDistrReq();
	SetDistrRequests(ulDistrReqs);

	SetPartPropagateRequests(2);
}
// EOF
