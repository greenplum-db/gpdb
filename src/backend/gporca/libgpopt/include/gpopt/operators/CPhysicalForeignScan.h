//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalForeignScan.h
//
//	@doc:
//		Foreign scan operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalForeignScan_H
#define GPOPT_CPhysicalForeignScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalTableScan.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalForeignScan
//
//	@doc:
//		Foreign scan operator
//
//---------------------------------------------------------------------------
class CPhysicalForeignScan : public CPhysicalTableScan
{
private:
public:
	CPhysicalForeignScan(const CPhysicalForeignScan &) = delete;

	// ctor
	CPhysicalForeignScan(CMemoryPool *, const CName *, CTableDescriptor *,
						 CColRefArray *);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalForeignScan;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalForeignScan";
	}

	// match function
	BOOL Matches(COperator *) const override;

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
	static CPhysicalForeignScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalForeignScan == pop->Eopid());

		return dynamic_cast<CPhysicalForeignScan *>(pop);
	}

};	// class CPhysicalForeignScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalForeignScan_H

// EOF
