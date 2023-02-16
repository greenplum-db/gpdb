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
	BOOL is_master_only)


	: CPhysicalDynamicScan(mp, ptabdesc, ulOriginOpId, pnameAlias, scan_id,
						   pdrgpcrOutput, pdrgpdrgpcrParts, partition_mdids,
						   root_col_mapping_per_part),
	  m_foreign_server_oid(foreign_server_oid),
	  m_is_master_only(is_master_only)
{
	m_pds->Release();
	// if this table is master only, use a strict singleton distribution request
	if (is_master_only)
	{
		m_pds = GPOS_NEW(mp) CDistributionSpecStrictSingleton(
			CDistributionSpecSingleton::EstMaster);
	}
	// otherwise, we want to execute on each segment (but can't assume anything about the distribution)
	else
	{
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
