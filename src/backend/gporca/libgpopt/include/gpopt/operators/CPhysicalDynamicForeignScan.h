//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CPhysicalDynamicForeignScan.h
//
//	@doc:
//  	Foreign scan operator for multiple tables sharing a common
//  	column layout
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalDynamicForeignScan_H
#define GPOPT_CPhysicalDynamicForeignScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalDynamicScan.h"

namespace gpopt
{
// Foreign scan operator for multiple tables sharing a common column layout.
class CPhysicalDynamicForeignScan : public CPhysicalDynamicScan
{
private:
	OID m_foreign_server_oid;

	BOOL m_is_master_only;

public:
	CPhysicalDynamicForeignScan(const CPhysicalDynamicForeignScan &) = delete;

	// ctor
	CPhysicalDynamicForeignScan(
		CMemoryPool *mp, const CName *pnameAlias, CTableDescriptor *ptabdesc,
		ULONG ulOriginOpId, ULONG scan_id, CColRefArray *pdrgpcrOutput,
		CColRef2dArray *pdrgpdrgpcrParts, IMdIdArray *partition_mdids,
		ColRefToUlongMapArray *root_col_mapping_per_part,
		OID foreign_server_oid, BOOL is_master_only);


	EOperatorId
	Eopid() const override
	{
		return EopPhysicalDynamicForeignScan;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalDynamicForeignScan";
	}

	// match function
	BOOL Matches(COperator *) const override;

	// statistics derivation during costing
	IStatistics *PstatsDerive(CMemoryPool *,		// mp
							  CExpressionHandle &,	// exprhdl
							  CReqdPropPlan *,		// prpplan
							  IStatisticsArray *	//stats_ctxt
	) const override;

	OID
	GetForeignServerOid() const
	{
		return m_foreign_server_oid;
	}

	BOOL
	IsMasterOnly() const
	{
		return m_is_master_only;
	}
	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive rewindability
	CRewindabilitySpec *
	PrsDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const override
	{
		// foreign tables are neither rewindable nor rescannable
		return GPOS_NEW(mp) CRewindabilitySpec(
			CRewindabilitySpec::ErtNone, CRewindabilitySpec::EmhtNoMotion);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl,
		const CEnfdRewindability *per) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalDynamicForeignScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalDynamicForeignScan == pop->Eopid());

		return reinterpret_cast<CPhysicalDynamicForeignScan *>(pop);
	}

	CPartitionPropagationSpec *PppsDerive(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

};	// class CPhysicalDynamicForeignScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalDynamicForeignScan_H

// EOF
