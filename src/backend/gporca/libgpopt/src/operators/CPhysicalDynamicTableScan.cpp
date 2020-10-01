//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalDynamicTableScan.cpp
//
//	@doc:
//		Implementation of dynamic table scan operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/CPartIndexMap.h"

#include "gpopt/operators/CPhysicalDynamicTableScan.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CName.h"

#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicTableScan::CPhysicalDynamicTableScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalDynamicTableScan::CPhysicalDynamicTableScan(
	CMemoryPool *mp, BOOL is_partial, const CName *pnameAlias,
	CTableDescriptor *ptabdesc, ULONG ulOriginOpId, ULONG scan_id,
	CColRefArray *pdrgpcrOutput, CColRef2dArray *pdrgpdrgpcrParts,
	ULONG ulSecondaryScanId, CPartConstraint *ppartcnstr,
	CPartConstraint *ppartcnstrRel)
	: CPhysicalDynamicScan(mp, is_partial, ptabdesc, ulOriginOpId, pnameAlias,
						   scan_id, pdrgpcrOutput, pdrgpdrgpcrParts,
						   ulSecondaryScanId, ppartcnstr, ppartcnstrRel)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicTableScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalDynamicTableScan::Matches(COperator *pop) const
{
	return CUtils::FMatchDynamicScan(this, pop);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicTableScan::PstatsDerive
//
//	@doc:
//		Statistics derivation during costing
//
//---------------------------------------------------------------------------
IStatistics *
CPhysicalDynamicTableScan::PstatsDerive(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										CReqdPropPlan *prpplan,
										IStatisticsArray *	// stats_ctxt
) const
{
	GPOS_ASSERT(NULL != prpplan);

	return CStatisticsUtils::DeriveStatsForDynamicScan(
		mp, exprhdl, ScanId(), prpplan->Pepp()->PpfmDerived());
}

// EOF
