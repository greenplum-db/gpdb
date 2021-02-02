//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalDynamicTableScan.h
//
//	@doc:
//		Dynamic Table scan operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalDynamicTableScan_H
#define GPOPT_CPhysicalDynamicTableScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalDynamicScan.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalDynamicTableScan
//
//	@doc:
//		Dynamic Table scan operator
//
//---------------------------------------------------------------------------
class CPhysicalDynamicTableScan : public CPhysicalDynamicScan
{
private:
	// GPDB_12_MERGE_FIXME: Move this to the base class once supported by siblings
	IMdIdArray *m_partition_mdids;

	// Map of Root colref -> col index in child tabledesc
	// per child partition in m_partition_mdid
	ColRefToUlongMapArray *m_root_col_mapping_per_part = nullptr;

public:
	CPhysicalDynamicTableScan(const CPhysicalDynamicTableScan &) = delete;

	// ctors
	CPhysicalDynamicTableScan(CMemoryPool *mp, const CName *pnameAlias,
							  CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
							  ULONG scan_id, CColRefArray *pdrgpcrOutput,
							  CColRef2dArray *pdrgpdrgpcrParts,
							  IMdIdArray *partition_mdids,
							  ColRefToUlongMapArray *root_col_mapping_per_part);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalDynamicTableScan;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalDynamicTableScan";
	}

	// match function
	BOOL Matches(COperator *) const override;

	// statistics derivation during costing
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CReqdPropPlan *prpplan,
							  IStatisticsArray *stats_ctxt) const override;

	IMdIdArray *
	GetPartitionMdids() const
	{
		return m_partition_mdids;
	}

	ColRefToUlongMapArray *
	GetRootColMappingPerPart() const
	{
		return m_root_col_mapping_per_part;
	}

	// conversion function
	static CPhysicalDynamicTableScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalDynamicTableScan == pop->Eopid());

		return dynamic_cast<CPhysicalDynamicTableScan *>(pop);
	}

	~CPhysicalDynamicTableScan() override
	{
		CRefCount::SafeRelease(m_partition_mdids);
		CRefCount::SafeRelease(m_root_col_mapping_per_part);
	}

};	// class CPhysicalDynamicTableScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalDynamicTableScan_H

// EOF
