//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CPhysicalExternalScan.cpp
//
//	@doc:
//		Implementation of external scan operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CDistributionSpecExternal.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalExternalScan.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CName.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalExternalScan::CPhysicalExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalExternalScan::CPhysicalExternalScan
	(
	CMemoryPool *mp,
	const CName *pnameAlias,
	CTableDescriptor *ptabdesc,
	CColRefArray *pdrgpcrOutput
	)
	:
	CPhysicalTableScan(mp, pnameAlias, ptabdesc, pdrgpcrOutput)
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

	m_pds = GPOS_NEW(mp) CDistributionSpecExternal();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalExternalScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalExternalScan::Matches
	(
	COperator *pop
	)
	const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CPhysicalExternalScan *popExternalScan = CPhysicalExternalScan::PopConvert(pop);
	return m_ptabdesc == popExternalScan->Ptabdesc() &&
			m_pdrgpcrOutput->Equals(popExternalScan->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalExternalScan::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalExternalScan::EpetRewindability
	(
	CExpressionHandle &exprhdl,
	const CEnfdRewindability *per
	)
	const
{
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		return CEnfdProp::EpetUnnecessary;
	}

    return CEnfdProp::EpetRequired;
}

// EOF

