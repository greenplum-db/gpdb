//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalCorrelatedInnerNLJoin.h
//
//	@doc:
//		Physical Inner Correlated NLJ operator capturing correlated execution
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalCorrelatedInnerNLJoin_H
#define GPOPT_CPhysicalCorrelatedInnerNLJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalInnerNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalCorrelatedInnerNLJoin
//
//	@doc:
//		Physical inner NLJ operator capturing correlated execution
//
//---------------------------------------------------------------------------
class CPhysicalCorrelatedInnerNLJoin : public CPhysicalInnerNLJoin
{
private:
	// columns from inner child used in correlated execution
	CColRefArray *m_pdrgpcrInner;

	// origin subquery id
	EOperatorId m_eopidOriginSubq;

public:
	CPhysicalCorrelatedInnerNLJoin(const CPhysicalCorrelatedInnerNLJoin &) =
		delete;

	// ctor
	CPhysicalCorrelatedInnerNLJoin(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
								   EOperatorId eopidOriginSubq)
		: CPhysicalInnerNLJoin(mp),
		  m_pdrgpcrInner(pdrgpcrInner),
		  m_eopidOriginSubq(eopidOriginSubq)
	{
		GPOS_ASSERT(nullptr != pdrgpcrInner);

		SetDistrRequests(UlDistrRequestsForCorrelatedJoin());
		GPOS_ASSERT(0 < UlDistrRequests());
	}

	// dtor
	~CPhysicalCorrelatedInnerNLJoin() override
	{
		m_pdrgpcrInner->Release();
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalCorrelatedInnerNLJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalCorrelatedInnerNLJoin";
	}

	// match function
	BOOL
	Matches(COperator *pop) const override
	{
		if (pop->Eopid() == Eopid())
		{
			return m_pdrgpcrInner->Equals(
				CPhysicalCorrelatedInnerNLJoin::PopConvert(pop)
					->PdrgPcrInner());
		}

		return false;
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
				"PdsRequired should not be called for CPhysicalCorrelatedInnerNLJoin"));
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

	// conversion function
	static CPhysicalCorrelatedInnerNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalCorrelatedInnerNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalCorrelatedInnerNLJoin *>(pop);
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

	// print
	IOstream &
	OsPrint(IOstream &os) const override
	{
		os << this->SzId() << "(";
		(void) CUtils::OsPrintDrgPcr(os, m_pdrgpcrInner);
		os << ")";

		return os;
	}

};	// class CPhysicalCorrelatedInnerNLJoin

}  // namespace gpopt


#endif	// !GPOPT_CPhysicalCorrelatedInnerNLJoin_H

// EOF
