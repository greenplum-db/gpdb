//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalMotion.h
//
//	@doc:
//		Base class for Motion operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalMotion_H
#define GPOPT_CPhysicalMotion_H

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPhysical.h"


namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalMotion
//
//	@doc:
//		Base class for Motion operators
//
//---------------------------------------------------------------------------
class CPhysicalMotion : public CPhysical
{
private:
protected:
	// ctor
	explicit CPhysicalMotion(CMemoryPool *mp) : CPhysical(mp)
	{
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

public:
	CPhysicalMotion(const CPhysicalMotion &) = delete;

	// output distribution accessor
	virtual CDistributionSpec *Pds() const = 0;

	// check if optimization contexts is valid
	BOOL FValidContext(CMemoryPool *mp, COptimizationContext *poc,
					   COptimizationContextArray *pdrgpocChild) const override;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required ctes of the n-th child
	CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  CCTEReq *pcter, ULONG child_index,
						  CDrvdPropArray *pdrgpdpCtxt,
						  ULONG ulOptReq) const override;

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	// compute required rewindability of the n-th child
	CRewindabilitySpec *PrsRequired(CMemoryPool *mp,
									CExpressionHandle &,   // exprhdl
									CRewindabilitySpec *,  // prsRequired
									ULONG,				   // child_index
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const override;

	// compute required partition propagation spec of the n-th child
	CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// derive rewindability
	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	// derived properties: derive partition propagation spec
	CPartitionPropagationSpec *PppsDerive(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return distribution property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl,
		const CEnfdDistribution *ped) const override;

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &,		// exprhdl
		const CEnfdRewindability *	// per
	) const override;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return true;
	}

	// conversion function
	static CPhysicalMotion *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(CUtils::FPhysicalMotion(pop));

		return dynamic_cast<CPhysicalMotion *>(pop);
	}

};	// class CPhysicalMotion

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalMotion_H

// EOF
