//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalLeftOuterHashJoin.cpp
//
//	@doc:
//		Implementation of left outer hash join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalLeftOuterHashJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterHashJoin::CPhysicalLeftOuterHashJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterHashJoin::CPhysicalLeftOuterHashJoin(
	CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
	CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
	BOOL is_null_aware, CXform::EXformId origin_xform)
	: CPhysicalHashJoin(mp, pdrgpexprOuterKeys, pdrgpexprInnerKeys,
						hash_opfamilies, is_null_aware, origin_xform)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterHashJoin::~CPhysicalLeftOuterHashJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterHashJoin::~CPhysicalLeftOuterHashJoin()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterHashJoin::PdsDeriveFromHashedChildren
//
//	@doc:
//		Derive hash join distribution from hashed children;
//		return NULL if derivation failed
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLeftOuterHashJoin::PdsDeriveFromHashedChildren(
	CMemoryPool *mp, CDistributionSpec *pdsOuter,
	CDistributionSpec *pdsInner) const
{
	GPOS_ASSERT(NULL != pdsOuter);
	GPOS_ASSERT(NULL != pdsInner);


	CDistributionSpecHashed *pdshashedOuter =
		CDistributionSpecHashed::PdsConvert(pdsOuter);
	CDistributionSpecHashed *pdshashedInner =
		CDistributionSpecHashed::PdsConvert(pdsInner);

	if (pdshashedOuter->IsCoveredBy(PdrgpexprOuterKeys()) &&
		pdshashedInner->IsCoveredBy(PdrgpexprInnerKeys()))
	{
		// if both sides are hashed on subsets of hash join keys, join's output can be
		// seen as distributed on outer spec or (equivalently) on inner spec,
		// so create a new spec and mark outer and inner as equivalent

		CDistributionSpecHashed *pdshashedInnerCopy =
			pdshashedInner->Copy(mp, false);
		CDistributionSpecHashed *combined_hashed_spec =
			pdshashedOuter->Combine(mp, pdshashedInnerCopy);
		pdshashedInnerCopy->Release();
		return combined_hashed_spec;
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalJoin::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLeftOuterHashJoin::PdsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) const
{
	CDistributionSpec *pdsOuter = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
	CDistributionSpec *pdsInner = exprhdl.Pdpplan(1 /*child_index*/)->Pds();

	// We must use the non-nullable side for the distribution spec for outer joins.
	// For right join, the hash side is the non-nullable side, so we swap the inner/outer
	// distribution specs for the logic below
	if (exprhdl.Pop()->Eopid() == EopPhysicalRightOuterHashJoin)
	{
		pdsOuter = exprhdl.Pdpplan(1 /*child_index*/)->Pds();
		pdsInner = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
	}

	if (CDistributionSpec::EdtHashed == pdsOuter->Edt() &&
		CDistributionSpec::EdtHashed == pdsInner->Edt())
	{
		CDistributionSpec *pdsDerived =
			PdsDeriveFromHashedChildren(mp, pdsOuter, pdsInner);
		if (NULL != pdsDerived)
		{
			return pdsDerived;
		}
	}

	CDistributionSpec *pds;
	if (CDistributionSpec::EdtStrictReplicated == pdsOuter->Edt() ||
		CDistributionSpec::EdtUniversal == pdsOuter->Edt())
	{
		// if outer is replicated/universal, return inner distribution
		pds = pdsInner;
	}
	else
	{
		// otherwise, return outer distribution
		pds = pdsOuter;
	}

	// TODO: Similar to the case where both outer and inner relations
	// are hash distributed, when only the outer relation is hash
	// distributed, we can also return a combined hash distribution spec.
	// The combined spec contains nulls colocated outer relation, and
	// nulls not colocated inner relation.
	if (CDistributionSpec::EdtHashed == pds->Edt())
	{
		CDistributionSpecHashed *pdsHashed =
			CDistributionSpecHashed::PdsConvert(pds);

		// Clean up any incomplete distribution specs since they can no longer be completed above
		// Note that, since this is done at the lowest join, no relevant equivalent specs are lost.
		if (!pdsHashed->HasCompleteEquivSpec(mp))
		{
			CExpressionArray *pdrgpexpr = pdsHashed->Pdrgpexpr();
			IMdIdArray *opfamilies = pdsHashed->Opfamilies();

			if (NULL != opfamilies)
			{
				opfamilies->AddRef();
			}
			pdrgpexpr->AddRef();
			return GPOS_NEW(mp) CDistributionSpecHashed(
				pdrgpexpr, pdsHashed->FNullsColocated(), opfamilies);
		}
	}

	pds->AddRef();
	return pds;
}
// EOF
