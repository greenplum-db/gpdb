//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalMotionBroadcast.h
//
//	@doc:
//		Physical Broadcast motion operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalMotionBroadcast_H
#define GPOPT_CPhysicalMotionBroadcast_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CPhysicalMotion.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalMotionBroadcast
//
//	@doc:
//		Broadcast motion operator
//
//---------------------------------------------------------------------------
class CPhysicalMotionBroadcast : public CPhysicalMotion
{
private:
	// output distribution
	CDistributionSpecReplicated *m_pdsReplicated;

public:
	CPhysicalMotionBroadcast(const CPhysicalMotionBroadcast &) = delete;

	// ctor
	explicit CPhysicalMotionBroadcast(CMemoryPool *mp,
									  BOOL ignore_broadcast_threshold);

	// dtor
	~CPhysicalMotionBroadcast() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalMotionBroadcast;
	}

	const CHAR *
	SzId() const override
	{
		return "CPhysicalMotionBroadcast";
	}

	// output distribution accessor
	CDistributionSpec *
	Pds() const override
	{
		return m_pdsReplicated;
	}

	// match function
	BOOL Matches(COperator *) const override;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsInput, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG ulOptReq) override;

	// compute required sort order of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posInput, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;


	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	COrderSpec *PosDerive(CMemoryPool *mp,
						  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// print
	IOstream &OsPrint(IOstream &) const override;

	// conversion function
	static CPhysicalMotionBroadcast *PopConvert(COperator *pop);

};	// class CPhysicalMotionBroadcast

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalMotionBroadcast_H

// EOF
