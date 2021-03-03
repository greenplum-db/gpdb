//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalSequence.h
//
//	@doc:
//		Physical sequence operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalSequence_H
#define GPOPT_CPhysicalSequence_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalSequence
//
//	@doc:
//		Physical sequence operator
//
//---------------------------------------------------------------------------
class CPhysicalSequence : public CPhysical
{
private:
	// empty column set to be requested from all children except last child
	CColRefSet *m_pcrsEmpty;

public:
	CPhysicalSequence(const CPhysicalSequence &) = delete;

	// ctor
	explicit CPhysicalSequence(CMemoryPool *mp);

	// dtor
	~CPhysicalSequence() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalSequence;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalSequence";
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsRequired, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG ulOptReq) override;

	// compute required ctes of the n-th child
	CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  CCTEReq *pcter, ULONG child_index,
						  CDrvdPropArray *pdrgpdpCtxt,
						  ULONG ulOptReq) const override;

	// compute required sort columns of the n-th child
	COrderSpec *PosRequired(CMemoryPool *,		  // mp
							CExpressionHandle &,  // exprhdl
							COrderSpec *,		  // posRequired
							ULONG,				  // child_index
							CDrvdPropArray *,	  // pdrgpdpCtxt
							ULONG				  // ulOptReq
	) const override;

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	// compute required rewindability of the n-th child
	CRewindabilitySpec *PrsRequired(CMemoryPool *,		   //mp
									CExpressionHandle &,   //exprhdl
									CRewindabilitySpec *,  //prsRequired
									ULONG,				   // child_index
									CDrvdPropArray *,	   // pdrgpdpCtxt
									ULONG ulOptReq) const override;

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order from the last child
	COrderSpec *PosDerive(CMemoryPool *mp,
						  CExpressionHandle &exprhdl) const override;

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// derive rewindability
	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl,
		const CEnfdRewindability *per) const override;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return false;
	}


};	// class CPhysicalSequence

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalSequence_H

// EOF
