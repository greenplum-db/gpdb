//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalTableScan.h
//
//	@doc:
//		Table scan operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalTableScan_H
#define GPOPT_CPhysicalTableScan_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalScan.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalTableScan
//
//	@doc:
//		Table scan operator
//
//---------------------------------------------------------------------------
class CPhysicalTableScan : public CPhysicalScan
{
private:
	// private copy ctor
	CPhysicalTableScan(const CPhysicalTableScan &);

public:
	// ctors
	explicit CPhysicalTableScan(CMemoryPool *mp);
	CPhysicalTableScan(CMemoryPool *, const CName *, CTableDescriptor *,
					   CColRefArray *);

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalTableScan;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalTableScan";
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	BOOL Matches(COperator *) const;

	// derive partition index map
	virtual CPartIndexMap *
	PpimDerive(CMemoryPool *mp,
			   CExpressionHandle &,	 // exprhdl
			   CDrvdPropCtxt *		 //pdpctxt
	) const
	{
		return GPOS_NEW(mp) CPartIndexMap(mp);
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;


	// conversion function
	static CPhysicalTableScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalTableScan == pop->Eopid() ||
					EopPhysicalExternalScan == pop->Eopid());

		return reinterpret_cast<CPhysicalTableScan *>(pop);
	}

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

	virtual CRewindabilitySpec *
	PrsDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const
	{
		// mark-restorability of output is always true
		return GPOS_NEW(mp)
			CRewindabilitySpec(CRewindabilitySpec::ErtMarkRestore,
							   CRewindabilitySpec::EmhtNoMotion);
	}


};	// class CPhysicalTableScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalTableScan_H

// EOF
