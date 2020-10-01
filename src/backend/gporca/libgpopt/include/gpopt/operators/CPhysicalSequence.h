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

	// private copy ctor
	CPhysicalSequence(const CPhysicalSequence &);

public:
	// ctor
	explicit CPhysicalSequence(CMemoryPool *mp);

	// dtor
	virtual ~CPhysicalSequence();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalSequence;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalSequence";
	}

	// match function
	BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	virtual CColRefSet *PcrsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
		ULONG child_index, CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	// compute required ctes of the n-th child
	virtual CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								  CCTEReq *pcter, ULONG child_index,
								  CDrvdPropArray *pdrgpdpCtxt,
								  ULONG ulOptReq) const;

	// compute required sort columns of the n-th child
	virtual COrderSpec *PosRequired(CMemoryPool *,		  // mp
									CExpressionHandle &,  // exprhdl
									COrderSpec *,		  // posRequired
									ULONG,				  // child_index
									CDrvdPropArray *,	  // pdrgpdpCtxt
									ULONG				  // ulOptReq
	) const;

	// compute required distribution of the n-th child
	virtual CDistributionSpec *PdsRequired(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CDistributionSpec *pdsRequired,
										   ULONG child_index,
										   CDrvdPropArray *pdrgpdpCtxt,
										   ULONG ulOptReq) const;

	// compute required rewindability of the n-th child
	virtual CRewindabilitySpec *PrsRequired(CMemoryPool *,		   //mp
											CExpressionHandle &,   //exprhdl
											CRewindabilitySpec *,  //prsRequired
											ULONG,			   // child_index
											CDrvdPropArray *,  // pdrgpdpCtxt
											ULONG ulOptReq) const;

	// compute required partition propagation of the n-th child
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *,				  //mp,
		CExpressionHandle &,		  //exprhdl,
		CPartitionPropagationSpec *,  //pppsRequired,
		ULONG,						  //child_index,
		CDrvdPropArray *,			  //pdrgpdpCtxt,
		ULONG						  //ulOptReq
	);

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order from the last child
	COrderSpec *PosDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const;

	// derive distribution
	virtual CDistributionSpec *PdsDerive(CMemoryPool *mp,
										 CExpressionHandle &exprhdl) const;

	// derive rewindability
	virtual CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
										  CExpressionHandle &exprhdl) const;

	// derive partition index map
	virtual CPartIndexMap *
	PpimDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
			   CDrvdPropCtxt *	//pdpctxt
	) const
	{
		return PpimDeriveCombineRelational(mp, exprhdl);
	}

	// derive partition filter map
	virtual CPartFilterMap *
	PpfmDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const
	{
		// combine part filter maps from relational children
		return PpfmDeriveCombineRelational(mp, exprhdl);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const;

	// return rewindability property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl, const CEnfdRewindability *per) const;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	virtual BOOL
	FPassThruStats() const
	{
		return false;
	}


};	// class CPhysicalSequence

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalSequence_H

// EOF
