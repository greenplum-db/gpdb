//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalBitmapTableScan.h
//
//	@doc:
//		Bitmap table scan physical operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CPhysicalBitmapTableScan_H
#define GPOPT_CPhysicalBitmapTableScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalScan.h"

namespace gpopt
{
// fwd declarations
class CDistributionSpec;
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalBitmapTableScan
//
//	@doc:
//		Bitmap table scan physical operator
//
//---------------------------------------------------------------------------
class CPhysicalBitmapTableScan : public CPhysicalScan
{
private:
	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG m_ulOriginOpId;

public:
	CPhysicalBitmapTableScan(const CPhysicalBitmapTableScan &) = delete;

	// ctor
	CPhysicalBitmapTableScan(CMemoryPool *mp, CTableDescriptor *ptabdesc,
							 ULONG ulOriginOpId, const CName *pnameTableAlias,
							 CColRefArray *pdrgpcrOutput);

	// dtor
	~CPhysicalBitmapTableScan() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalBitmapTableScan;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalBitmapTableScan";
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG
	UlOriginOpId() const
	{
		return m_ulOriginOpId;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// derive partition index map
	CPartIndexMap *
	PpimDerive(CMemoryPool *mp,
			   CExpressionHandle &,	 // exprhdl
			   CDrvdPropCtxt *		 //pdpctxt
	) const override
	{
		return GPOS_NEW(mp) CPartIndexMap(mp);
	}

	// statistics derivation during costing
	IStatistics *
	PstatsDerive(CMemoryPool *,		   // mp
				 CExpressionHandle &,  // exprhdl
				 CReqdPropPlan *,	   // prpplan
				 IStatisticsArray *	   //stats_ctxt
	) const override
	{
		GPOS_ASSERT(
			!"stats derivation during costing for bitmap table scan is invalid");

		return NULL;
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

	// conversion function
	static CPhysicalBitmapTableScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalBitmapTableScan == pop->Eopid());

		return dynamic_cast<CPhysicalBitmapTableScan *>(pop);
	}
};
}  // namespace gpopt

#endif	// !GPOPT_CPhysicalBitmapTableScan_H

// EOF
