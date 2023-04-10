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
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalPartitionSelector::CPhysicalPartitionSelector
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalPartitionSelector::CPhysicalPartitionSelector(CMemoryPool *mp,
													   ULONG scan_id,
													   ULONG selector_id,
													   IMDId *mdid,
													   CExpression *pexprScalar)
	: CPhysical(mp),
	  m_scan_id(scan_id),
	  m_selector_id(selector_id),
	  m_mdid(mdid),
	  m_filter_expr(pexprScalar)
{
	GPOS_ASSERT(0 < scan_id);
	GPOS_ASSERT(mdid->IsValid());
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
	m_mdid->Release();
	CRefCount::SafeRelease(m_filter_expr);
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

	return fScanIdCmp && fMdidCmp;
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
	pcrs->Union(m_filter_expr->DeriveUsedColumns());
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

CPartitionPropagationSpec *
CPhysicalPartitionSelector::PppsRequired(
	CMemoryPool *mp, CExpressionHandle &,
	CPartitionPropagationSpec *pppsRequired,
	ULONG child_index GPOS_ASSERTS_ONLY, CDrvdPropArray *, ULONG) const
{
	GPOS_ASSERT(child_index == 0);

	CPartitionPropagationSpec *pps_result =
		GPOS_NEW(mp) CPartitionPropagationSpec(mp);
	pps_result->InsertAllExcept(pppsRequired, m_scan_id);
	return pps_result;
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

CPartitionPropagationSpec *
CPhysicalPartitionSelector::PppsDerive(CMemoryPool *mp,
									   CExpressionHandle &exprhdl) const
{
	CPartitionPropagationSpec *pps_result =
		GPOS_NEW(mp) CPartitionPropagationSpec(mp);
	CPartitionPropagationSpec *pps_child =
		exprhdl.Pdpplan(0 /* child_index */)->Ppps();

	CBitSet *selector_ids = GPOS_NEW(mp) CBitSet(mp);
	selector_ids->ExchangeSet(m_selector_id);

	pps_result->InsertAll(pps_child);
	pps_result->Insert(m_scan_id, CPartitionPropagationSpec::EpptPropagator,
					   m_mdid, selector_ids, nullptr);
	selector_ids->Release();

	return pps_result;
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

	CPartitionPropagationSpec *ppps = pdpplan->Ppps();

	if (!ppps->Contains(m_scan_id))
	{
		// ensure we don't create a plan with a motion on top of a partition selector
		return CEnfdProp::EpetProhibited;
	}
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
	os << SzId() << ", Id: " << SelectorId() << ", Scan Id: " << m_scan_id
	   << ", Part Table: ";
	MDId()->OsPrint(os);

	return os;
}

// EOF
