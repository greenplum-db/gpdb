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
CPhysicalLeftOuterHashJoin::~CPhysicalLeftOuterHashJoin() = default;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerHashJoin::PdsDeriveFromHashedChildren
//
//	@doc:
//		Derive hash join distribution from hashed children;
//		return NULL if derivation failed
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLeftOuterHashJoin::PdsDeriveFromHashedChildren(
	CMemoryPool *mp, CDistributionSpec *pdsOuter, CDistributionSpec *pdsInner,
	BOOL isselfjoin) const
{
	GPOS_ASSERT(nullptr != pdsOuter);
	GPOS_ASSERT(nullptr != pdsInner);

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
		//
		// if this is a self join and both sides are null colocated then we can
		// assume the result is also null colocated.
		CDistributionSpecHashed *pdshashedInnerCopy = pdshashedInner->Copy(
			mp, pdshashedOuter->FNullsColocated() &&
					pdshashedInner->FNullsColocated() && isselfjoin);
		CDistributionSpecHashed *combined_hashed_spec =
			pdshashedOuter->Combine(mp, pdshashedInnerCopy);
		pdshashedInnerCopy->Release();
		return combined_hashed_spec;
	}

	return nullptr;
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
		CTableDescriptor *tabOuter =
			exprhdl.DeriveTableDescriptor(0 /*child_index*/);
		CTableDescriptor *tabInner =
			exprhdl.DeriveTableDescriptor(1 /*child_index*/);
		BOOL isselfjoin = (nullptr != tabOuter && nullptr != tabInner)
							  ? tabOuter->MDId()->Equals(tabInner->MDId())
							  : false;
		CDistributionSpec *pdsDerived =
			PdsDeriveFromHashedChildren(mp, pdsOuter, pdsInner, isselfjoin);
		if (nullptr != pdsDerived)
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

			if (nullptr != opfamilies)
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
