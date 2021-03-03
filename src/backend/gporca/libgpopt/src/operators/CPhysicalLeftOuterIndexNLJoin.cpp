//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Greenplum, Inc.
//
//	Implementation of left outer index nested-loops join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalLeftOuterIndexNLJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/exception.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;

CPhysicalLeftOuterIndexNLJoin::CPhysicalLeftOuterIndexNLJoin(
	CMemoryPool *mp, CColRefArray *colref_array, CExpression *origJoinPred)
	: CPhysicalLeftOuterNLJoin(mp),
	  m_pdrgpcrOuterRefs(colref_array),
	  m_origJoinPred(origJoinPred)
{
	GPOS_ASSERT(nullptr != colref_array);
	if (nullptr != origJoinPred)
	{
		origJoinPred->AddRef();
	}
}


CPhysicalLeftOuterIndexNLJoin::~CPhysicalLeftOuterIndexNLJoin()
{
	m_pdrgpcrOuterRefs->Release();
	CRefCount::SafeRelease(m_origJoinPred);
}


BOOL
CPhysicalLeftOuterIndexNLJoin::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		return m_pdrgpcrOuterRefs->Equals(
			CPhysicalLeftOuterIndexNLJoin::PopConvert(pop)->PdrgPcrOuterRefs());
	}

	return false;
}


CDistributionSpec *
CPhysicalLeftOuterIndexNLJoin::PdsRequired(
	CMemoryPool *mp GPOS_UNUSED, CExpressionHandle &exprhdl GPOS_UNUSED,
	CDistributionSpec *,  //pdsRequired,
	ULONG child_index GPOS_UNUSED, CDrvdPropArray *pdrgpdpCtxt GPOS_UNUSED,
	ULONG  // ulOptReq
) const
{
	GPOS_RAISE(
		CException::ExmaInvalid, CException::ExmiInvalid,
		GPOS_WSZ_LIT(
			"PdsRequired should not be called for CPhysicalLeftOuterIndexNLJoin"));
	return nullptr;
}

CEnfdDistribution *
CPhysicalLeftOuterIndexNLJoin::Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CReqdPropPlan *prppInput, ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq)
{
	GPOS_ASSERT(2 > child_index);

	CEnfdDistribution::EDistributionMatching dmatch =
		Edm(prppInput, child_index, pdrgpdpCtxt, ulOptReq);

	if (1 == child_index)
	{
		// inner (index-scan side) is requested for Any distribution,
		// we allow outer references on the inner child of the join since it needs
		// to refer to columns in join's outer child
		return GPOS_NEW(mp) CEnfdDistribution(
			GPOS_NEW(mp)
				CDistributionSpecAny(this->Eopid(), true /*fAllowOuterRefs*/),
			dmatch);
	}

	// we need to match distribution of inner
	CDistributionSpec *pdsInner =
		CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
	CDistributionSpec::EDistributionType edtInner = pdsInner->Edt();
	if (CDistributionSpec::EdtSingleton == edtInner ||
		CDistributionSpec::EdtStrictSingleton == edtInner ||
		CDistributionSpec::EdtUniversal == edtInner)
	{
		// enforce executing on a single host
		return GPOS_NEW(mp) CEnfdDistribution(
			GPOS_NEW(mp) CDistributionSpecSingleton(), dmatch);
	}

	if (CDistributionSpec::EdtHashed == edtInner)
	{
		// check if we could create an equivalent hashed distribution request to the inner child
		CDistributionSpecHashed *pdshashed =
			CDistributionSpecHashed::PdsConvert(pdsInner);
		CDistributionSpecHashed *pdshashedEquiv = pdshashed->PdshashedEquiv();

		// If the inner child is a IndexScan on a multi-key distributed index, it
		// may derive an incomplete equiv spec (see CPhysicalScan::PdsDerive()).
		// However, there is no point to using that here since there will be no
		// operator above this that can complete it.
		//
		// NB: Technically for Outer joins, the entire distribution key of the
		// table must be present in the join clause to produce the index scan
		// alternative in the first place (see CXformJoin2IndexApplyBase).
		// Therefore, when an incomplete spec is created in the inner subtree for
		// such a table, there will also be a CPhysicalFilter (that has the
		// remaining predicates) on top to complete the spec. Thus, at this point
		// in the code, pdshashedEquiv should be complete. However, just in case
		// that precondition is not met, it is safer to to check for completeness
		// properly anyway.
		if (pdshashed->HasCompleteEquivSpec(mp))
		{
			// request hashed distribution from outer
			pdshashedEquiv->Pdrgpexpr()->AddRef();
			CDistributionSpecHashed *pdsHashedRequired = GPOS_NEW(mp)
				CDistributionSpecHashed(pdshashedEquiv->Pdrgpexpr(),
										pdshashedEquiv->FNullsColocated());
			pdsHashedRequired->ComputeEquivHashExprs(mp, exprhdl);

			return GPOS_NEW(mp) CEnfdDistribution(pdsHashedRequired, dmatch);
		}

		// if the equivalent spec cannot be used, request the original - even
		// though this spec will fail to produce a plan during property
		// enforcement, it is still better than falling back to planner, since
		// there may be other alternatives that will succeed.
		pdshashed->AddRef();
		return GPOS_NEW(mp) CEnfdDistribution(pdshashed, dmatch);
	}

	// shouldn't come here!
	GPOS_RAISE(
		gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
		GPOS_WSZ_LIT("Left outer index nestloop join broadcasting outer side"));
	// otherwise, require outer child to be replicated
	return GPOS_NEW(mp) CEnfdDistribution(
		GPOS_NEW(mp)
			CDistributionSpecReplicated(CDistributionSpec::EdtStrictReplicated),
		dmatch);
}


// EOF
