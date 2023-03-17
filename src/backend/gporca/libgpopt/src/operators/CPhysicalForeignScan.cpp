//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalForeignScan.cpp
//
//	@doc:
//		Implementation of foreign scan operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalForeignScan.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalForeignScan::CPhysicalForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalForeignScan::CPhysicalForeignScan(CMemoryPool *mp,
										   const CName *pnameAlias,
										   CTableDescriptor *ptabdesc,
										   CColRefArray *pdrgpcrOutput)
	: CPhysicalTableScan(mp, pnameAlias, ptabdesc, pdrgpcrOutput)
{
	// if this table is master only, then keep the original distribution spec.
	if (IMDRelation::EreldistrMasterOnly == ptabdesc->GetRelDistribution())
	{
		return;
	}

	// otherwise, override the distribution spec for foreign table
	if (m_pds)
	{
		m_pds->Release();
	}

	m_pds = GPOS_NEW(mp) CDistributionSpecRandom();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalForeignScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalForeignScan::Matches(COperator *pop) const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CPhysicalForeignScan *popForeignScan =
		CPhysicalForeignScan::PopConvert(pop);
	return m_ptabdesc->MDId()->Equals(popForeignScan->Ptabdesc()->MDId()) &&
		   m_pdrgpcrOutput->Equals(popForeignScan->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalForeignScan::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalForeignScan::EpetRewindability(CExpressionHandle &exprhdl,
										const CEnfdRewindability *per) const
{
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		return CEnfdProp::EpetUnnecessary;
	}

	return CEnfdProp::EpetRequired;
}

// EOF
