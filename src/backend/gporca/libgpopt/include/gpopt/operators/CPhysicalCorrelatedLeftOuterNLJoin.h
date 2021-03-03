//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalCorrelatedLeftOuterNLJoin.h
//
//	@doc:
//		Physical Left Outer NLJ  operator capturing correlated execution
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalCorrelatedLeftOuterNLJoin_H
#define GPOPT_CPhysicalCorrelatedLeftOuterNLJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalLeftOuterNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalCorrelatedLeftOuterNLJoin
//
//	@doc:
//		Physical left outer NLJ operator capturing correlated execution
//
//---------------------------------------------------------------------------
class CPhysicalCorrelatedLeftOuterNLJoin : public CPhysicalLeftOuterNLJoin
{
private:
	// columns from inner child used in correlated execution
	CColRefArray *m_pdrgpcrInner;

	// origin subquery id
	EOperatorId m_eopidOriginSubq;

public:
	CPhysicalCorrelatedLeftOuterNLJoin(
		const CPhysicalCorrelatedLeftOuterNLJoin &) = delete;

	// ctor
	CPhysicalCorrelatedLeftOuterNLJoin(CMemoryPool *mp,
									   CColRefArray *pdrgpcrInner,
									   EOperatorId eopidOriginSubq)
		: CPhysicalLeftOuterNLJoin(mp),
		  m_pdrgpcrInner(pdrgpcrInner),
		  m_eopidOriginSubq(eopidOriginSubq)
	{
		GPOS_ASSERT(nullptr != pdrgpcrInner);

		SetDistrRequests(UlDistrRequestsForCorrelatedJoin());
		GPOS_ASSERT(0 < UlDistrRequests());
	}

	// dtor
	~CPhysicalCorrelatedLeftOuterNLJoin() override
	{
		m_pdrgpcrInner->Release();
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalCorrelatedLeftOuterNLJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalCorrelatedLeftOuterNLJoin";
	}

	// match function
	BOOL
	Matches(COperator *pop) const override
	{
		if (pop->Eopid() == Eopid())
		{
			return m_pdrgpcrInner->Equals(
				CPhysicalCorrelatedLeftOuterNLJoin::PopConvert(pop)
					->PdrgPcrInner());
		}

		return false;
	}

	CEnfdDistribution *
	Ped(CMemoryPool *mp, CExpressionHandle &exprhdl, CReqdPropPlan *prppInput,
		ULONG child_index, CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) override
	{
		return PedCorrelatedJoin(mp, exprhdl, prppInput, child_index,
								 pdrgpdpCtxt, ulOptReq);
	}

	// compute required distribution of the n-th child
	CDistributionSpec *
	PdsRequired(CMemoryPool *,		  // mp
				CExpressionHandle &,  // exprhdl,
				CDistributionSpec *,  // pdsRequired,
				ULONG,				  // child_index,
				CDrvdPropArray *,	  // pdrgpdpCtxt,
				ULONG				  //ulOptReq
	) const override
	{
		GPOS_RAISE(
			CException::ExmaInvalid, CException::ExmiInvalid,
			GPOS_WSZ_LIT(
				"PdsRequired should not be called for CPhysicalCorrelatedLeftOuterNLJoin"));
		return nullptr;
	}

	// compute required rewindability of the n-th child
	CRewindabilitySpec *
	PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired, ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const override
	{
		return PrsRequiredCorrelatedJoin(mp, exprhdl, prsRequired, child_index,
										 pdrgpdpCtxt, ulOptReq);
	}

	// distribution matching type
	CEnfdDistribution::EDistributionMatching
	Edm(CReqdPropPlan *,   // prppInput
		ULONG,			   // child_index
		CDrvdPropArray *,  //pdrgpdpCtxt
		ULONG			   // ulOptReq
		) override
	{
		return CEnfdDistribution::EdmSatisfy;
	}

	// return true if operator is a correlated NL Join
	BOOL
	FCorrelated() const override
	{
		return true;
	}

	// return required inner columns
	CColRefArray *
	PdrgPcrInner() const override
	{
		return m_pdrgpcrInner;
	}

	// origin subquery id
	EOperatorId
	EopidOriginSubq() const
	{
		return m_eopidOriginSubq;
	}

	// conversion function
	static CPhysicalCorrelatedLeftOuterNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalCorrelatedLeftOuterNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalCorrelatedLeftOuterNLJoin *>(pop);
	}

};	// class CPhysicalCorrelatedLeftOuterNLJoin

}  // namespace gpopt


#endif	// !GPOPT_CPhysicalCorrelatedLeftOuterNLJoin_H

// EOF
