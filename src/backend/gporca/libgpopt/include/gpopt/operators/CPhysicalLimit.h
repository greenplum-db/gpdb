//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLimit.h
//
//	@doc:
//		Physical Limit operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLimit_H
#define GPOPT_CPhysicalLimit_H

#include "gpos/base.h"

#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLimit
//
//	@doc:
//		Limit operator
//
//---------------------------------------------------------------------------
class CPhysicalLimit : public CPhysical
{
private:
	// order spec
	COrderSpec *m_pos;

	// global limit
	BOOL m_fGlobal;

	// does limit specify a number of rows?
	BOOL m_fHasCount;

	// this is a top limit right under a DML or CTAS operation
	BOOL m_top_limit_under_dml;

	// columns used by order spec
	CColRefSet *m_pcrsSort;

public:
	CPhysicalLimit(const CPhysicalLimit &) = delete;

	// ctor
	CPhysicalLimit(CMemoryPool *mp, COrderSpec *pos, BOOL fGlobal,
				   BOOL fHasCount, BOOL fTopLimitUnderDML);

	// dtor
	~CPhysicalLimit() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLimit;
	}

	const CHAR *
	SzId() const override
	{
		return "CPhysicalLimit";
	}

	// hash function
	ULONG
	HashValue() const override
	{
		return gpos::CombineHashes(
			gpos::CombineHashes(COperator::HashValue(), m_pos->HashValue()),
			gpos::CombineHashes(gpos::HashValue<BOOL>(&m_fGlobal),
								gpos::HashValue<BOOL>(&m_fHasCount)));
	}

	// order spec
	COrderSpec *
	Pos() const
	{
		return m_pos;
	}

	// global limit
	BOOL
	FGlobal() const
	{
		return m_fGlobal;
	}

	// does limit specify a number of rows
	BOOL
	FHasCount() const
	{
		return m_fHasCount;
	}

	// must the limit be always kept
	BOOL
	IsTopLimitUnderDMLorCTAS() const
	{
		return m_top_limit_under_dml;
	}

	// match function
	BOOL Matches(COperator *) const override;

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
		CExpressionHandle &,		// exprhdl
		const CEnfdRewindability *	// per
	) const override;

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

	// print
	IOstream &OsPrint(IOstream &) const override;

	// conversion function
	static CPhysicalLimit *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalLimit == pop->Eopid());

		return dynamic_cast<CPhysicalLimit *>(pop);
	}

	CEnfdDistribution *Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
						   CReqdPropPlan *prppInput, ULONG child_index,
						   CDrvdPropArray *pdrgpdpCtxt,
						   ULONG ulDistrReq) override;

};	// class CPhysicalLimit

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLimit_H

// EOF
