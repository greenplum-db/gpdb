//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalFullMergeJoin.cpp
//
//	@doc:
//		Implementation of full merge join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CCastUtils.h"

#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "gpopt/operators/CPhysicalFullMergeJoin.h"

using namespace gpopt;

#define GPOPT_MAX_HASH_DIST_REQUESTS 6

// ctor
CPhysicalFullMergeJoin::CPhysicalFullMergeJoin(
	CMemoryPool *mp, CExpressionArray *outer_merge_clauses,
	CExpressionArray *inner_merge_clauses, IMdIdArray *)
	: CPhysicalJoin(mp),
	  m_outer_merge_clauses(outer_merge_clauses),
	  m_inner_merge_clauses(inner_merge_clauses)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != outer_merge_clauses);
	GPOS_ASSERT(NULL != inner_merge_clauses);
	GPOS_ASSERT(outer_merge_clauses->Size() == inner_merge_clauses->Size());

	// There is one request per col, up to the max number of requests
	// plus an additional request for all the cols, and one for the singleton.
	ULONG num_hash_reqs = std::min((ULONG) GPOPT_MAX_HASH_DIST_REQUESTS,
								   outer_merge_clauses->Size());
	SetDistrRequests(num_hash_reqs + 2);
}


// dtor
CPhysicalFullMergeJoin::~CPhysicalFullMergeJoin()
{
	m_outer_merge_clauses->Release();
	m_inner_merge_clauses->Release();
}

CDistributionSpec *
CPhysicalFullMergeJoin::PdsRequired(CMemoryPool *mp GPOS_UNUSED,
									CExpressionHandle &exprhdl GPOS_UNUSED,
									CDistributionSpec *pdsRequired GPOS_UNUSED,
									ULONG child_index GPOS_UNUSED,
									CDrvdPropArray *,  //pdrgpdpCtxt,
									ULONG ulOptReq GPOS_UNUSED) const
{
	GPOS_RAISE(
		CException::ExmaInvalid, CException::ExmiInvalid,
		GPOS_WSZ_LIT(
			"PdsRequired should not be called for CPhysicalFullMergeJoin"));
	return nullptr;
}

CEnfdDistribution *
CPhysicalFullMergeJoin::Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
							CReqdPropPlan *prppInput, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt GPOS_UNUSED,
							ULONG ulOptReq)
{
	GPOS_ASSERT(2 > child_index);

	CDistributionSpec *const pdsRequired = prppInput->Ped()->PdsRequired();

	// if expression has to execute on a single host then we need a gather
	if (exprhdl.NeedsSingletonExecution() || exprhdl.HasOuterRefs())
	{
		return GPOS_NEW(mp) CEnfdDistribution(
			PdsRequireSingleton(mp, exprhdl, pdsRequired, child_index),
			CEnfdDistribution::EdmExact);
	}

	BOOL nulls_collocated = true;
	if (CPredicateUtils::ExprContainsOnlyStrictComparisons(
			mp, exprhdl.PexprScalarExactChild(2)))
	{
		// There is no need to require NULL rows to be collocated if the merge clauses
		// only contain STRICT operators. This is because any NULL row will automatically
		// not match any row on the other side.
		nulls_collocated = false;
	}

	CExpressionArray *clauses =
		(child_index == 0) ? m_outer_merge_clauses : m_inner_merge_clauses;

	// TODO: Handle matching/ equivalent distribution spec (e.g using pdsRequired)
	ULONG num_hash_reqs =
		std::min((ULONG) GPOPT_MAX_HASH_DIST_REQUESTS, clauses->Size());
	if (ulOptReq < num_hash_reqs)
	{
		CExpressionArray *pdrgpexprCurrent = GPOS_NEW(mp) CExpressionArray(mp);
		CExpression *expr = (*clauses)[ulOptReq];
		expr->AddRef();
		pdrgpexprCurrent->Append(expr);

		CDistributionSpecHashed *pds = GPOS_NEW(mp)
			CDistributionSpecHashed(pdrgpexprCurrent, nulls_collocated);
		return GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);
	}
	else if (ulOptReq == num_hash_reqs)
	{
		clauses->AddRef();
		CDistributionSpecHashed *pds =
			GPOS_NEW(mp) CDistributionSpecHashed(clauses, nulls_collocated);
		return GPOS_NEW(mp) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);
	}
	else
	{
		GPOS_ASSERT(ulOptReq == (num_hash_reqs + 1));
		return GPOS_NEW(mp) CEnfdDistribution(
			PdsRequireSingleton(mp, exprhdl, pdsRequired, child_index),
			CEnfdDistribution::EdmExact);
	}
}

COrderSpec *
CPhysicalFullMergeJoin::PosRequired(CMemoryPool *mp,
									CExpressionHandle &,  //exprhdl,
									COrderSpec *,		  //posInput
									ULONG child_index,
									CDrvdPropArray *,  //pdrgpdpCtxt
									ULONG			   //ulOptReq
) const
{
	// Merge joins require their input to be sorted on corresponsing join clauses. Without
	// making dangerous assumptions of the implementation of the merge joins, it is difficult
	// to predict the order of the output of the merge join. (This may not be true). In that
	// case, it is better to not push down any order requests from above.

	COrderSpec *os = GPOS_NEW(mp) COrderSpec(mp);

	CExpressionArray *clauses;
	if (child_index == 0)
	{
		clauses = m_outer_merge_clauses;
	}
	else
	{
		GPOS_ASSERT(child_index == 1);
		clauses = m_inner_merge_clauses;
	}

	for (ULONG ul = 0; ul < clauses->Size(); ++ul)
	{
		CExpression *expr = (*clauses)[ul];

		GPOS_ASSERT(CUtils::FScalarIdent(expr));
		const CColRef *colref = CCastUtils::PcrExtractFromScIdOrCastScId(expr);

		// Make sure that the corresponding properties (mergeStrategies, mergeNullsFirst)
		// in CTranslatorDXLToPlStmt::TranslateDXLMergeJoin() match.
		//
		// NB: The operator used for sorting here is the '<' operator in the
		// default btree opfamily of the column's type. For this to work correctly,
		// the '=' operator of the merge join clauses must also belong to the same
		// opfamily, which in this case, is the default of the type.
		// See FMergeJoinCompatible() where predicates using a different opfamily
		// are rejected from merge clauses.
		gpmd::IMDId *mdid =
			colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
		mdid->AddRef();
		os->Append(mdid, colref, COrderSpec::EntLast);
	}

	return os;
}

// compute required rewindability of the n-th child
CRewindabilitySpec *
CPhysicalFullMergeJoin::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
									CDrvdPropArray *,  // pdrgpdpCtxt
									ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(
		child_index < 2 &&
		"Required rewindability can be computed on the relational child only");

	// Merge join may need to rescan a portion of the tuples on the inner side, so require mark-restore
	// on the inner child
	if (child_index == 1)
	{
		// Merge joins are disabled if there are outer references
		GPOS_ASSERT(!exprhdl.HasOuterRefs());
		return GPOS_NEW(mp) CRewindabilitySpec(
			CRewindabilitySpec::ErtMarkRestore, prsRequired->Emht());
	}

	// pass through requirements to outer child
	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

// return order property enforcing type for this operator
CEnfdProp::EPropEnforcingType
CPhysicalFullMergeJoin::EpetOrder(CExpressionHandle &, const CEnfdOrder *
#ifdef GPOS_DEBUG
														   peo
#endif	// GPOS_DEBUG
) const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	// merge join is not order-preserving, at least in
	// the sense that nulls maybe interleaved;
	// any order requirements have to be enforced on top
	return CEnfdProp::EpetRequired;
}

CEnfdDistribution::EDistributionMatching
CPhysicalFullMergeJoin::Edm(CReqdPropPlan *,   // prppInput
							ULONG,			   // child_index,
							CDrvdPropArray *,  // pdrgpdpCtxt,
							ULONG			   // ulOptReq
)
{
	return CEnfdDistribution::EdmExact;
}

CDistributionSpec *
CPhysicalFullMergeJoin::PdsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	CDistributionSpec *pdsOuter = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
	CDistributionSpec *pdsInner = exprhdl.Pdpplan(1 /*child_index*/)->Pds();

	if (CDistributionSpec::EdtHashed == pdsOuter->Edt() &&
		CDistributionSpec::EdtHashed == pdsInner->Edt())
	{
		// Merge join requires either both sides to be hashed ...
		CDistributionSpecHashed *pdshashedOuter =
			CDistributionSpecHashed::PdsConvert(pdsOuter);
		CDistributionSpecHashed *pdshashedInner =
			CDistributionSpecHashed::PdsConvert(pdsInner);

		// Create a hash spec similar to the outer spec, but with fNullsColocated = false because
		// nulls appear as the results get computed, so we cannot verify that they will be colocated.
		pdshashedOuter->Pdrgpexpr()->AddRef();
		CDistributionSpecHashed *pds = GPOS_NEW(mp) CDistributionSpecHashed(
			pdshashedOuter->Pdrgpexpr(), false /* fNullsCollocated */);

		// NB: Logic is similar to CPhysicalInnerHashJoin::PdsDeriveFromHashedChildren()
		if (pdshashedOuter->IsCoveredBy(m_outer_merge_clauses) &&
			pdshashedInner->IsCoveredBy(m_inner_merge_clauses))
		{
			CDistributionSpecHashed *pdsCombined =
				pds->Combine(mp, pdshashedInner);
			pds->Release();
			return pdsCombined;
		}
		else
		{
			return pds;
		}
	}

	// ... or both sides to be singleton/universal
	GPOS_ASSERT(CDistributionSpec::EdtSingleton == pdsOuter->Edt() ||
				CDistributionSpec::EdtStrictSingleton == pdsOuter->Edt() ||
				CDistributionSpec::EdtUniversal == pdsOuter->Edt());

	// otherwise, pass through outer distribution
	pdsOuter->AddRef();
	return pdsOuter;
}
