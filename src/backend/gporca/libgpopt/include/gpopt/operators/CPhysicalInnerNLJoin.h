//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalInnerNLJoin.h
//
//	@doc:
//		Inner nested-loops join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalInnerNLJoin_H
#define GPOPT_CPhysicalInnerNLJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalInnerNLJoin
//
//	@doc:
//		Inner nested-loops join operator
//
//---------------------------------------------------------------------------
class CPhysicalInnerNLJoin : public CPhysicalNLJoin
{
private:
public:
	CPhysicalInnerNLJoin(const CPhysicalInnerNLJoin &) = delete;

	// ctor
	explicit CPhysicalInnerNLJoin(CMemoryPool *mp);

	// dtor
	~CPhysicalInnerNLJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalInnerNLJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalInnerNLJoin";
	}

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	CEnfdDistribution *Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
						   CReqdPropPlan *prppInput, ULONG child_index,
						   CDrvdPropArray *pdrgpdpCtxt,
						   ULONG ulDistrReq) override;

	// conversion function
	static CPhysicalInnerNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalInnerNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalInnerNLJoin *>(pop);
	}


};	// class CPhysicalInnerNLJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalInnerNLJoin_H

// EOF
