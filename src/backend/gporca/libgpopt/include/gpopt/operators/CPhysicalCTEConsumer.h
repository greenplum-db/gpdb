//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalCTEConsumer.h
//
//	@doc:
//		Physical CTE consumer operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalCTEConsumer_H
#define GPOPT_CPhysicalCTEConsumer_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalCTEConsumer
//
//	@doc:
//		CTE consumer operator
//
//---------------------------------------------------------------------------
class CPhysicalCTEConsumer : public CPhysical
{
private:
	// cte identifier
	ULONG m_id;

	// cte columns
	CColRefArray *m_pdrgpcr;

	// hashmap for all the columns in the CTE expression
	UlongToColRefMap *m_phmulcr;

public:
	CPhysicalCTEConsumer(const CPhysicalCTEConsumer &) = delete;

	// ctor
	CPhysicalCTEConsumer(CMemoryPool *mp, ULONG id, CColRefArray *colref_array,
						 UlongToColRefMap *colref_mapping);

	// dtor
	~CPhysicalCTEConsumer() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalCTEConsumer;
	}

	const CHAR *
	SzId() const override
	{
		return "CPhysicalCTEConsumer";
	}

	// cte identifier
	ULONG
	UlCTEId() const
	{
		return m_id;
	}

	// cte columns
	CColRefArray *
	Pdrgpcr() const
	{
		return m_pdrgpcr;
	}

	// column mapping
	UlongToColRefMap *
	Phmulcr() const
	{
		return m_phmulcr;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
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

	// derive cte map
	CCTEMap *PcmDerive(CMemoryPool *mp,
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

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalCTEConsumer *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalCTEConsumer == pop->Eopid());

		return dynamic_cast<CPhysicalCTEConsumer *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CPhysicalCTEConsumer

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalCTEConsumer_H

// EOF
