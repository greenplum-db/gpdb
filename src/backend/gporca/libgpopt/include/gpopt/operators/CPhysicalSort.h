//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalSort.h
//
//	@doc:
//		Physical sort operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalSort_H
#define GPOS_CPhysicalSort_H

#include "gpos/base.h"

#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CPhysical.h"


namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalSort
//
//	@doc:
//		Sort operator
//
//---------------------------------------------------------------------------
class CPhysicalSort : public CPhysical
{
private:
	// order spec
	COrderSpec *m_pos;

	// columns used by order spec
	CColRefSet *m_pcrsSort;

public:
	CPhysicalSort(const CPhysicalSort &) = delete;

	// ctor
	CPhysicalSort(CMemoryPool *mp, COrderSpec *pos);

	// dtor
	~CPhysicalSort() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalSort;
	}

	// sort order accessor
	virtual const COrderSpec *
	Pos() const
	{
		return m_pos;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalSort";
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

	// compute required sort order of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posRequired, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	// compute required rewindability of the n-th child
	CRewindabilitySpec *PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const override;

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;


	// distribution matching type
	CEnfdDistribution::EDistributionMatching
	Edm(CReqdPropPlan *prppInput,
		ULONG,			   // child_index
		CDrvdPropArray *,  //pdrgpdpCtxt
		ULONG			   // ulOptReq
		) override
	{
		// Sort does not require Motions to be enforced on top,
		// we need to pass down incoming matching type
		return prppInput->Ped()->Edm();
	}

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
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

	// return distribution property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl,
		const CEnfdDistribution *ped) const override;

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl,
		const CEnfdRewindability *per) const override;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CPhysicalSort *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalSort == pop->Eopid());

		return dynamic_cast<CPhysicalSort *>(pop);
	}

};	// class CPhysicalSort

}  // namespace gpopt


#endif	// !GPOS_CPhysicalSort_H

// EOF
