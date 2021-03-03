//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalPartitionSelector.cpp
//
//	@doc:
//		Implementation of physical partition selector
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalPartitionSelector.h"

#include "gpos/base.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDrvdPropCtxtPlan.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::CPhysicalPartitionSelector
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalPartitionSelector::CPhysicalPartitionSelector(
	CMemoryPool *mp, ULONG scan_id, IMDId *mdid, CColRef2dArray *pdrgpdrgpcr,
	UlongToPartConstraintMap *ppartcnstrmap, CPartConstraint *ppartcnstr,
	UlongToExprMap *phmulexprEqPredicates, UlongToExprMap *phmulexprPredicates,
	CExpression *pexprResidual)
	: CPhysical(mp),
	  m_scan_id(scan_id),
	  m_mdid(mdid),
	  m_pdrgpdrgpcr(pdrgpdrgpcr),
	  m_ppartcnstrmap(ppartcnstrmap),
	  m_part_constraint(ppartcnstr),
	  m_phmulexprEqPredicates(phmulexprEqPredicates),
	  m_phmulexprPredicates(phmulexprPredicates),
	  m_pexprResidual(pexprResidual)
{
	GPOS_ASSERT(0 < scan_id);
	GPOS_ASSERT(mdid->IsValid());
	GPOS_ASSERT(nullptr != pdrgpdrgpcr);
	GPOS_ASSERT(0 < pdrgpdrgpcr->Size());
	GPOS_ASSERT(nullptr != ppartcnstrmap);
	GPOS_ASSERT(nullptr != ppartcnstr);
	GPOS_ASSERT(nullptr != phmulexprEqPredicates);
	GPOS_ASSERT(nullptr != phmulexprPredicates);

	m_pexprCombinedPredicate = PexprCombinedPartPred(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::CPhysicalPartitionSelector
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalPartitionSelector::CPhysicalPartitionSelector(
	CMemoryPool *mp, IMDId *mdid, UlongToExprMap *phmulexprEqPredicates)
	: CPhysical(mp),
	  m_scan_id(0),
	  m_mdid(mdid),
	  m_pdrgpdrgpcr(nullptr),
	  m_ppartcnstrmap(nullptr),
	  m_part_constraint(nullptr),
	  m_phmulexprEqPredicates(phmulexprEqPredicates),
	  m_phmulexprPredicates(nullptr),
	  m_pexprResidual(nullptr),
	  m_pexprCombinedPredicate(nullptr)
{
	GPOS_ASSERT(mdid->IsValid());
	GPOS_ASSERT(nullptr != phmulexprEqPredicates);

	m_phmulexprPredicates = GPOS_NEW(mp) UlongToExprMap(mp);
	m_pexprCombinedPredicate = PexprCombinedPartPred(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::~CPhysicalPartitionSelector
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalPartitionSelector::~CPhysicalPartitionSelector()
{
	CRefCount::SafeRelease(m_pdrgpdrgpcr);
	CRefCount::SafeRelease(m_part_constraint);
	CRefCount::SafeRelease(m_ppartcnstrmap);
	m_phmulexprPredicates->Release();
	m_mdid->Release();
	m_phmulexprEqPredicates->Release();
	CRefCount::SafeRelease(m_pexprResidual);
	CRefCount::SafeRelease(m_pexprCombinedPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::FMatchExprMaps
//
//	@doc:
//		Check whether two expression maps match
//
//---------------------------------------------------------------------------
BOOL
CPhysicalPartitionSelector::FMatchExprMaps(UlongToExprMap *phmulexprFst,
										   UlongToExprMap *phmulexprSnd)
{
	GPOS_ASSERT(nullptr != phmulexprFst);
	GPOS_ASSERT(nullptr != phmulexprSnd);

	const ULONG ulEntries = phmulexprFst->Size();
	if (ulEntries != phmulexprSnd->Size())
	{
		return false;
	}

	UlongToExprMapIter hmulei(phmulexprFst);

	while (hmulei.Advance())
	{
		ULONG ulKey = *(hmulei.Key());
		const CExpression *pexprFst = hmulei.Value();
		CExpression *pexprSnd = phmulexprSnd->Find(&ulKey);
		if (!CUtils::Equals(pexprFst, pexprSnd))
		{
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::FMatchPartCnstr
//
//	@doc:
//		Match part constraints
//
//---------------------------------------------------------------------------
BOOL
CPhysicalPartitionSelector::FMatchPartCnstr(
	UlongToPartConstraintMap *ppartcnstrmap) const
{
	if (nullptr == m_ppartcnstrmap || nullptr == ppartcnstrmap)
	{
		return nullptr == m_ppartcnstrmap && nullptr == ppartcnstrmap;
	}

	return m_ppartcnstrmap->Size() == ppartcnstrmap->Size() &&
		   FSubsetPartCnstr(ppartcnstrmap, m_ppartcnstrmap);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::FSubsetPartCnstr
//
//	@doc:
//		Check if first part constraint map is a subset of the second one
//
//---------------------------------------------------------------------------
BOOL
CPhysicalPartitionSelector::FSubsetPartCnstr(
	UlongToPartConstraintMap *ppartcnstrmapFst,
	UlongToPartConstraintMap *ppartcnstrmapSnd)
{
	GPOS_ASSERT(nullptr != ppartcnstrmapFst);
	GPOS_ASSERT(nullptr != ppartcnstrmapSnd);
	if (ppartcnstrmapFst->Size() > ppartcnstrmapSnd->Size())
	{
		return false;
	}

	UlongToPartConstraintMapIter partcnstriter(ppartcnstrmapFst);

	while (partcnstriter.Advance())
	{
		ULONG ulKey = *(partcnstriter.Key());
		CPartConstraint *ppartcnstr = ppartcnstrmapSnd->Find(&ulKey);

		if (nullptr == ppartcnstr ||
			!partcnstriter.Value()->FEquivalent(ppartcnstr))
		{
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::FHasFilter
//
//	@doc:
//		Check whether this operator has a partition selection filter
//
//---------------------------------------------------------------------------
BOOL
CPhysicalPartitionSelector::FHasFilter() const
{
	return (nullptr != m_pexprResidual || 0 < m_phmulexprEqPredicates->Size() ||
			0 < m_phmulexprPredicates->Size());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalPartitionSelector::Matches(COperator *pop) const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CPhysicalPartitionSelector *popPartSelector =
		CPhysicalPartitionSelector::PopConvert(pop);

	BOOL fScanIdCmp = popPartSelector->ScanId() == m_scan_id;
	BOOL fMdidCmp = popPartSelector->MDId()->Equals(MDId());
	BOOL fPartCnstrMapCmp = FMatchPartCnstr(popPartSelector->m_ppartcnstrmap);
	BOOL fColRefCmp =
		CColRef::Equals(popPartSelector->Pdrgpdrgpcr(), m_pdrgpdrgpcr);
	BOOL fPartCnstrEquiv =
		popPartSelector->m_part_constraint->FEquivalent(m_part_constraint);
	BOOL fEqPredCmp = FMatchExprMaps(popPartSelector->m_phmulexprEqPredicates,
									 m_phmulexprEqPredicates);
	BOOL fPredCmp = FMatchExprMaps(popPartSelector->m_phmulexprPredicates,
								   m_phmulexprPredicates);
	BOOL fResPredCmp =
		CUtils::Equals(popPartSelector->m_pexprResidual, m_pexprResidual);

	return fScanIdCmp && fMdidCmp && fPartCnstrMapCmp && fColRefCmp &&
		   fPartCnstrEquiv && fEqPredCmp && fResPredCmp && fPredCmp;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::HashValue
//
//	@doc:
//		Hash operator
//
//---------------------------------------------------------------------------
ULONG
CPhysicalPartitionSelector::HashValue() const
{
	return gpos::CombineHashes(
		Eopid(), gpos::CombineHashes(m_scan_id, MDId()->HashValue()));
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PexprEqFilter
//
//	@doc:
//		Return the equality filter expression for the given level
//
//---------------------------------------------------------------------------
CExpression *
CPhysicalPartitionSelector::PexprEqFilter(ULONG ulPartLevel) const
{
	return m_phmulexprEqPredicates->Find(&ulPartLevel);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PexprFilter
//
//	@doc:
//		Return the non-equality filter expression for the given level
//
//---------------------------------------------------------------------------
CExpression *
CPhysicalPartitionSelector::PexprFilter(ULONG ulPartLevel) const
{
	return m_phmulexprPredicates->Find(&ulPartLevel);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PexprPartPred
//
//	@doc:
//		Return the partition selection predicate for the given level
//
//---------------------------------------------------------------------------
CExpression *
CPhysicalPartitionSelector::PexprPartPred(CMemoryPool *mp,
										  ULONG ulPartLevel) const
{
	GPOS_ASSERT(ulPartLevel < UlPartLevels());

	CExpression *pexpr = PexprEqFilter(ulPartLevel);
	if (nullptr != pexpr)
	{
		// we have one side of an equality predicate - need to construct the
		// whole predicate
		GPOS_ASSERT(nullptr == m_phmulexprPredicates->Find(&ulPartLevel));
		pexpr->AddRef();
		if (nullptr != m_pdrgpdrgpcr)
		{
			CColRef *pcrPartKey = (*(*m_pdrgpdrgpcr)[ulPartLevel])[0];
			return CUtils::PexprScalarEqCmp(mp, pcrPartKey, pexpr);
		}
		else
		{
			return pexpr;
		}
	}

	pexpr = m_phmulexprPredicates->Find(&ulPartLevel);
	if (nullptr != pexpr)
	{
		pexpr->AddRef();
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PexprCombinedPartPred
//
//	@doc:
//		Return a single combined partition selection predicate
//
//---------------------------------------------------------------------------
CExpression *
CPhysicalPartitionSelector::PexprCombinedPartPred(CMemoryPool *mp) const
{
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);

	const ULONG ulLevels = UlPartLevels();
	for (ULONG ul = 0; ul < ulLevels; ul++)
	{
		CExpression *pexpr = PexprPartPred(mp, ul);
		if (nullptr != pexpr)
		{
			pdrgpexpr->Append(pexpr);
		}
	}

	if (nullptr != m_pexprResidual)
	{
		m_pexprResidual->AddRef();
		pdrgpexpr->Append(m_pexprResidual);
	}

	return CPredicateUtils::PexprConjunction(mp, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::UlPartLevels
//
//	@doc:
//		Number of partitioning levels
//
//---------------------------------------------------------------------------
ULONG
CPhysicalPartitionSelector::UlPartLevels() const
{
	if (nullptr != m_pdrgpdrgpcr)
	{
		return m_pdrgpdrgpcr->Size();
	}

	return m_phmulexprEqPredicates->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//		we only compute required columns for the relational child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalPartitionSelector::PcrsRequired(CMemoryPool *mp,
										 CExpressionHandle &exprhdl,
										 CColRefSet *pcrsInput,
										 ULONG child_index,
										 CDrvdPropArray *,	// pdrgpdpCtxt
										 ULONG				// ulOptReq
)
{
	GPOS_ASSERT(
		0 == child_index &&
		"Required properties can only be computed on the relational child");

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, *pcrsInput);
	pcrs->Union(m_pexprCombinedPredicate->DeriveUsedColumns());
	pcrs->Intersection(exprhdl.DeriveOutputColumns(child_index));

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalPartitionSelector::PosRequired(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										COrderSpec *posRequired,
										ULONG child_index,
										CDrvdPropArray *,  // pdrgpdpCtxt
										ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	return PosPassThru(mp, exprhdl, posRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalPartitionSelector::PdsRequired(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										CDistributionSpec *pdsInput,
										ULONG child_index,
										CDrvdPropArray *,  // pdrgpdpCtxt
										ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	CPartInfo *ppartinfo = exprhdl.DerivePartitionInfo();
	BOOL fCovered = ppartinfo->FContainsScanId(m_scan_id);

	if (fCovered)
	{
		// if partition consumer is defined below, do not pass distribution
		// requirements down as this will cause the consumer and enforcer to be
		// in separate slices
		return GPOS_NEW(mp) CDistributionSpecAny(this->Eopid());
	}

	return PdsPassThru(mp, exprhdl, pdsInput, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalPartitionSelector::PrsRequired(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										CRewindabilitySpec *prsRequired,
										ULONG child_index,
										CDrvdPropArray *,  // pdrgpdpCtxt
										ULONG			   // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalPartitionSelector::PcteRequired(CMemoryPool *,		   //mp,
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
//		CPhysicalPartitionSelector::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalPartitionSelector::FProvidesReqdCols(CExpressionHandle &exprhdl,
											  CColRefSet *pcrsRequired,
											  ULONG	 // ulOptReq
) const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalPartitionSelector::PosDerive(CMemoryPool *,  // mp
									  CExpressionHandle &exprhdl) const
{
	return PosDerivePassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalPartitionSelector::PdsDerive(CMemoryPool *,  // mp
									  CExpressionHandle &exprhdl) const
{
	return PdsDerivePassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalPartitionSelector::PrsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) const
{
	return PrsDerivePassThruOuter(mp, exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::EpetDistribution
//
//	@doc:
//		Return the enforcing type for distribution property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalPartitionSelector::EpetDistribution(CExpressionHandle &exprhdl,
											 const CEnfdDistribution *ped) const
{
	CDrvdPropPlan *pdpplan = exprhdl.Pdpplan(0 /* child_index */);

	if (ped->FCompatible(pdpplan->Pds()))
	{
		// required distribution established by the operator
		return CEnfdProp::EpetUnnecessary;
	}

	// GPDB_12_MERGE_FIXME: Check part propagation spec
#if 0
	CPartIndexMap *ppimDrvd = pdpplan->Ppim();
	if (!ppimDrvd->Contains(m_scan_id))
	{
		// part consumer is defined above: prohibit adding a motion on top of the
		// part resolver as this will create two slices
		return CEnfdProp::EpetProhibited;
	}
#endif

	// part consumer found below: enforce distribution on top of part resolver
	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalPartitionSelector::EpetRewindability(
	CExpressionHandle &exprhdl, const CEnfdRewindability *per) const
{
	// get rewindability delivered by the node
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		// required rewindability is already provided
		return CEnfdProp::EpetUnnecessary;
	}

	// always force spool to be on top of filter
	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalPartitionSelector::EpetOrder(CExpressionHandle &,	// exprhdl,
									  const CEnfdOrder *	// ped
) const
{
	return CEnfdProp::EpetOptional;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalPartitionSelector::OsPrint(IOstream &os) const
{
	os << SzId() << ", Scan Id: " << m_scan_id << ", Part Table: ";
	MDId()->OsPrint(os);

	return os;
}

// EOF
