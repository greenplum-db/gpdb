//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CPhysicalMultiExternalScan.h
//
//	@doc:
//  	External scan operator for multiple tables sharing a common
//  	column layout
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalMultiExternalScan_H
#define GPOPT_CPhysicalMultiExternalScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalDynamicScan.h"

namespace gpopt
{
// External scan operator for multiple tables sharing a common column layout.
// Currently only used for external leaf partitions in a partitioned table.
class CPhysicalMultiExternalScan : public CPhysicalDynamicScan
{
private:
	// private copy ctor
	CPhysicalMultiExternalScan(const CPhysicalMultiExternalScan &);

	// partition mdids to scan
	IMdIdArray *m_part_mdids;

public:
	// ctor
	CPhysicalMultiExternalScan(CMemoryPool *mp, IMdIdArray *part_mdids,
							   BOOL is_partial, CTableDescriptor *ptabdesc,
							   ULONG ulOriginOpId, const CName *pnameAlias,
							   ULONG scan_id, CColRefArray *pdrgpcrOutput,
							   CColRef2dArray *pdrgpdrgpcrParts,
							   ULONG ulSecondaryScanId,
							   CPartConstraint *ppartcnstr,
							   CPartConstraint *ppartcnstrRel);

	~CPhysicalMultiExternalScan();

	// ident accessors
	IMdIdArray *
	GetScanPartitionMdids() const
	{
		return m_part_mdids;
	}

	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalMultiExternalScan;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalMultiExternalScan";
	}

	// match function
	virtual BOOL Matches(COperator *) const;

	// statistics derivation during costing
	virtual IStatistics *
	PstatsDerive(CMemoryPool *,		   // mp
				 CExpressionHandle &,  // exprhdl
				 CReqdPropPlan *,	   // prpplan
				 IStatisticsArray *	   //stats_ctxt
	) const
	{
		GPOS_ASSERT(
			!"stats derivation during costing for table scan is invalid");

		return NULL;
	}

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive rewindability
	virtual CRewindabilitySpec *
	PrsDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const
	{
		// external tables are neither rewindable nor rescannable
		return GPOS_NEW(mp) CRewindabilitySpec(
			CRewindabilitySpec::ErtNone, CRewindabilitySpec::EmhtNoMotion);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return rewindability property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl, const CEnfdRewindability *per) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalMultiExternalScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalMultiExternalScan == pop->Eopid());

		return reinterpret_cast<CPhysicalMultiExternalScan *>(pop);
	}

};	// class CPhysicalMultiExternalScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalMultiExternalScan_H

// EOF
