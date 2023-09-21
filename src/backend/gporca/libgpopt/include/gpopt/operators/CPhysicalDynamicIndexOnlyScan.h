//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CPhysicalDynamicIndexOnlyScan.h
//
//	@doc:
//		Physical dynamic index only scan operators on partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalDynamicIndexOnlyScan_H
#define GPOPT_CPhysicalDynamicIndexOnlyScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalDynamicIndexScan.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalDynamicIndexOnlyScan
//
//	@doc:
//		Physical dynamic index only scan operators for partitioned tables
//
//---------------------------------------------------------------------------
class CPhysicalDynamicIndexOnlyScan : public CPhysicalDynamicIndexScan
{
public:
	CPhysicalDynamicIndexOnlyScan(const CPhysicalDynamicIndexOnlyScan &) =
		delete;

	// ctors
	CPhysicalDynamicIndexOnlyScan(
		CMemoryPool *mp, CIndexDescriptor *pindexdesc,
		CTableDescriptor *ptabdesc, ULONG ulOriginOpId, const CName *pnameAlias,
		CColRefArray *pdrgpcrOutput, ULONG scan_id,
		CColRef2dArray *pdrgpdrgpcrPart, COrderSpec *pos,
		IMdIdArray *partition_mdids,
		ColRefToUlongMapArray *root_col_mapping_per_part)
		: CPhysicalDynamicIndexScan(
			  mp, pindexdesc, ptabdesc, ulOriginOpId, pnameAlias, pdrgpcrOutput,
			  scan_id, pdrgpdrgpcrPart, pos, partition_mdids,
			  root_col_mapping_per_part,
			  0 /* m_ulUnindexedPredColCount - No residual predicate possible, if we are making index only scan*/)
	{
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalDynamicIndexOnlyScan;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalDynamicIndexOnlyScan";
	}

	BOOL
	Matches(COperator *pop) const override
	{
		return CUtils::FMatchDynamicIndex(this, pop);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	CPartitionPropagationSpec *PppsDerive(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// conversion function
	static CPhysicalDynamicIndexOnlyScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalDynamicIndexOnlyScan == pop->Eopid());

		return dynamic_cast<CPhysicalDynamicIndexOnlyScan *>(pop);
	}

};	// class CPhysicalDynamicIndexOnlyScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalDynamicIndexOnlyScan_H

// EOF
