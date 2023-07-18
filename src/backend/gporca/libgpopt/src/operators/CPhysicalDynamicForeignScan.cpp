//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CPhysicalDynamicForeignScan.cpp
//
//	@doc:
//		Implementation of dynamic foreign scan operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalDynamicForeignScan.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecStrictSingleton.h"
#include "gpopt/base/CDistributionSpecUniversal.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicForeignScan::CPhysicalDynamicForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalDynamicForeignScan::CPhysicalDynamicForeignScan(
	CMemoryPool *mp, const CName *pnameAlias, CTableDescriptor *ptabdesc,
	ULONG ulOriginOpId, ULONG scan_id, CColRefArray *pdrgpcrOutput,
	CColRef2dArray *pdrgpdrgpcrParts, IMdIdArray *partition_mdids,
	ColRefToUlongMapArray *root_col_mapping_per_part, OID foreign_server_oid,
	IMDRelation::Ereldistrpolicy exec_location)


	: CPhysicalDynamicScan(mp, ptabdesc, ulOriginOpId, pnameAlias, scan_id,
						   pdrgpcrOutput, pdrgpdrgpcrParts, partition_mdids,
						   root_col_mapping_per_part),
	  m_foreign_server_oid(foreign_server_oid),
	  m_exec_location(exec_location)
{
	// we need to overwrite the distribution spec for DynamicForeignGets, as
	// the partition table can have one distribution, but the distribution for the
	// ForeignGet can be different. Note this distribution spec mismatch is only
	// allowed for foreign partitions
	m_pds->Release();
	// if this table is coordinator only, set the distribution as strict singleton
	if (m_exec_location == IMDRelation::EreldistrCoordinatorOnly)
	{
		m_pds = GPOS_NEW(mp) CDistributionSpecStrictSingleton(
			CDistributionSpecSingleton::EstCoordinator);
	}
	// if the table can be executed on either a segment or coordinator, set it as universal
	else if (m_exec_location == IMDRelation::EreldistrUniversal)
	{
		m_pds = GPOS_NEW(mp) CDistributionSpecUniversal();
	}
	// if the distribution is set to "all segments", it is set to a random distribution
	else
	{
		GPOS_ASSERT(m_exec_location == IMDRelation::EreldistrRandom);
		m_pds = GPOS_NEW(mp) CDistributionSpecRandom();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicForeignScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalDynamicForeignScan::Matches(COperator *pop) const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CPhysicalDynamicForeignScan *popForeignScan =
		CPhysicalDynamicForeignScan::PopConvert(pop);
	return CUtils::FMatchDynamicScan(this, pop) &&
		   m_foreign_server_oid == popForeignScan->GetForeignServerOid();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicForeignScan::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalDynamicForeignScan::EpetRewindability(
	CExpressionHandle &exprhdl, const CEnfdRewindability *per) const
{
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		return CEnfdProp::EpetUnnecessary;
	}

	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicForeignScan::PstatsDerive
//
//	@doc:
//		Statistics derivation during costing
//
//---------------------------------------------------------------------------
IStatistics *
CPhysicalDynamicForeignScan::PstatsDerive(CMemoryPool *mp,
										  CExpressionHandle &exprhdl,
										  CReqdPropPlan *prpplan,
										  IStatisticsArray *  // stats_ctxt
) const
{
	GPOS_ASSERT(nullptr != prpplan);

	return CStatisticsUtils::DeriveStatsForDynamicScan(
		mp, exprhdl, ScanId(), prpplan->Pepp()->PppsRequired());
}

CPartitionPropagationSpec *
CPhysicalDynamicForeignScan::PppsDerive(CMemoryPool *mp,
										CExpressionHandle &) const
{
	CPartitionPropagationSpec *pps = GPOS_NEW(mp) CPartitionPropagationSpec(mp);
	pps->Insert(ScanId(), CPartitionPropagationSpec::EpptConsumer,
				Ptabdesc()->MDId(), nullptr, nullptr);

	return pps;
}


// EOF
