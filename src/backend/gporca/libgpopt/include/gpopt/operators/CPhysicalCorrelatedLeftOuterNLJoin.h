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

	// private copy ctor
	CPhysicalCorrelatedLeftOuterNLJoin(
		const CPhysicalCorrelatedLeftOuterNLJoin &);

public:
	// ctor
	CPhysicalCorrelatedLeftOuterNLJoin(CMemoryPool *mp,
									   CColRefArray *pdrgpcrInner,
									   EOperatorId eopidOriginSubq)
		: CPhysicalLeftOuterNLJoin(mp),
		  m_pdrgpcrInner(pdrgpcrInner),
		  m_eopidOriginSubq(eopidOriginSubq)
	{
		GPOS_ASSERT(NULL != pdrgpcrInner);

		SetDistrRequests(UlDistrRequestsForCorrelatedJoin());
		GPOS_ASSERT(0 < UlDistrRequests());
	}

	// dtor
	virtual ~CPhysicalCorrelatedLeftOuterNLJoin()
	{
		m_pdrgpcrInner->Release();
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalCorrelatedLeftOuterNLJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalCorrelatedLeftOuterNLJoin";
	}

	// match function
	virtual BOOL
	Matches(COperator *pop) const
	{
		if (pop->Eopid() == Eopid())
		{
			return m_pdrgpcrInner->Equals(
				CPhysicalCorrelatedLeftOuterNLJoin::PopConvert(pop)
					->PdrgPcrInner());
		}

		return false;
	}

	// compute required distribution of the n-th child
	virtual CDistributionSpec *
	PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired, ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const
	{
		return PdsRequiredCorrelatedJoin(mp, exprhdl, pdsRequired, child_index,
										 pdrgpdpCtxt, ulOptReq);
	}

	// compute required rewindability of the n-th child
	virtual CRewindabilitySpec *
	PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired, ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const
	{
		return PrsRequiredCorrelatedJoin(mp, exprhdl, prsRequired, child_index,
										 pdrgpdpCtxt, ulOptReq);
	}

	// distribution matching type
	virtual CEnfdDistribution::EDistributionMatching
	Edm(CReqdPropPlan *,   // prppInput
		ULONG,			   // child_index
		CDrvdPropArray *,  //pdrgpdpCtxt
		ULONG			   // ulOptReq
	)
	{
		return CEnfdDistribution::EdmSatisfy;
	}

	// return true if operator is a correlated NL Join
	virtual BOOL
	FCorrelated() const
	{
		return true;
	}

	// return required inner columns
	virtual CColRefArray *
	PdrgPcrInner() const
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
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalCorrelatedLeftOuterNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalCorrelatedLeftOuterNLJoin *>(pop);
	}

};	// class CPhysicalCorrelatedLeftOuterNLJoin

}  // namespace gpopt


#endif	// !GPOPT_CPhysicalCorrelatedLeftOuterNLJoin_H

// EOF
