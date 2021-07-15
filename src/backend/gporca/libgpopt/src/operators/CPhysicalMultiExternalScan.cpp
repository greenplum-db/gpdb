//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CPhysicalMultiExternalScan.cpp
//
//	@doc:
//		Implementation of external scan operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalMultiExternalScan.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMultiExternalScan::CPhysicalMultiExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalMultiExternalScan::CPhysicalMultiExternalScan(
	CMemoryPool *mp, IMdIdArray *part_mdids, BOOL is_partial,
	CTableDescriptor *ptabdesc, ULONG ulOriginOpId, const CName *pnameAlias,
	ULONG scan_id, CColRefArray *pdrgpcrOutput,
	CColRef2dArray *pdrgpdrgpcrParts, ULONG ulSecondaryScanId,
	CPartConstraint *ppartcnstr, CPartConstraint *ppartcnstrRel)
	: CPhysicalDynamicScan(mp, is_partial, ptabdesc, ulOriginOpId, pnameAlias,
						   scan_id, pdrgpcrOutput, pdrgpdrgpcrParts,
						   ulSecondaryScanId, ppartcnstr, ppartcnstrRel),
	  m_part_mdids(part_mdids)
{
	// if this table is master only, then keep the original distribution spec.
	if (IMDRelation::EreldistrMasterOnly == ptabdesc->GetRelDistribution())
	{
		return;
	}

	// otherwise, override the distribution spec for external table
	if (m_pds)
	{
		m_pds->Release();
	}

	m_pds = GPOS_NEW(mp) CDistributionSpecRandom();

	GPOS_ASSERT(NULL != m_part_mdids && m_part_mdids->Size() > 0);
}

CPhysicalMultiExternalScan::~CPhysicalMultiExternalScan()
{
	CRefCount::SafeRelease(m_part_mdids);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMultiExternalScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalMultiExternalScan::Matches(COperator *pop) const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CPhysicalMultiExternalScan *popExternalScan =
		CPhysicalMultiExternalScan::PopConvert(pop);
	return m_ptabdesc == popExternalScan->Ptabdesc() &&
		   m_pdrgpcrOutput->Equals(popExternalScan->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMultiExternalScan::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalMultiExternalScan::EpetRewindability(
	CExpressionHandle &exprhdl, const CEnfdRewindability *per) const
{
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		return CEnfdProp::EpetUnnecessary;
	}

	return CEnfdProp::EpetRequired;
}

// EOF
