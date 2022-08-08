//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CPhysicalLeftAntiSemiHashJoinNotIn.cpp
//
//	@doc:
//		Implementation of left anti semi hash join operator with NotIn semantics
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalLeftAntiSemiHashJoinNotIn.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftAntiSemiHashJoinNotIn::CPhysicalLeftAntiSemiHashJoinNotIn
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftAntiSemiHashJoinNotIn::CPhysicalLeftAntiSemiHashJoinNotIn(
	CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
	CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
	BOOL is_null_aware, CXform::EXformId origin_xform)
	: CPhysicalLeftAntiSemiHashJoin(mp, pdrgpexprOuterKeys, pdrgpexprInnerKeys,
									hash_opfamilies, is_null_aware,
									origin_xform)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftAntiSemiHashJoinNotIn::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLeftAntiSemiHashJoinNotIn::PdsRequired(
	CMemoryPool * /*mp*/, CExpressionHandle & /*exprhdl*/,
	CDistributionSpec * /*pdsInput*/, ULONG /*child_index*/,
	CDrvdPropArray * /*pdrgpdpCtxt*/,
	ULONG  //ulOptReq// identifies which optimization request should be created
) const
{
	GPOS_RAISE(
		CException::ExmaInvalid, CException::ExmiInvalid,
		GPOS_WSZ_LIT(
			"PdsRequired should not be called for CPhysicalLeftAntiSemiHashJoinNotIn"));
	return NULL;
}

CEnfdDistribution *
CPhysicalLeftAntiSemiHashJoinNotIn::Ped(
	CMemoryPool *mp, CExpressionHandle &exprhdl, CReqdPropPlan *prppInput,
	ULONG child_index, CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq)
{
	GPOS_ASSERT(2 > child_index);
	GPOS_ASSERT(ulOptReq < UlDistrRequests());

	if (0 == ulOptReq && 1 == child_index &&
		(FNullableHashKeys(exprhdl.DeriveNotNullColumns(0), false /*fInner*/) ||
		 FNullableHashKeys(exprhdl.DeriveNotNullColumns(1), true /*fInner*/)))
	{
		// we need to replicate the inner if any of the following is true:
		// a. if the outer hash keys are nullable, because the executor needs to detect
		//	  whether the inner is empty, and this needs to be detected everywhere
		// b. if the inner hash keys are nullable, because every segment needs to
		//	  detect nulls coming from the inner child
		return GPOS_NEW(mp) CEnfdDistribution(
			GPOS_NEW(mp)
				CDistributionSpecReplicated(CDistributionSpec::EdtReplicated),
			CEnfdDistribution::EdmSatisfy);
	}

	return CPhysicalHashJoin::Ped(mp, exprhdl, prppInput, child_index,
								  pdrgpdpCtxt, ulOptReq);
}

// EOF
