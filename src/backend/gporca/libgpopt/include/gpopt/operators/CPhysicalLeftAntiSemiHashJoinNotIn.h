//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CPhysicalLeftAntiSemiHashJoinNotIn.h
//
//	@doc:
//		Left anti semi hash join operator with NotIn semantics
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftAntiSemiHashJoinNotIn_H
#define GPOPT_CPhysicalLeftAntiSemiHashJoinNotIn_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalLeftAntiSemiHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftAntiSemiHashJoinNotIn
//
//	@doc:
//		Left anti semi hash join operator with NotIn semantics
//
//---------------------------------------------------------------------------
class CPhysicalLeftAntiSemiHashJoinNotIn : public CPhysicalLeftAntiSemiHashJoin
{
private:
public:
	CPhysicalLeftAntiSemiHashJoinNotIn(
		const CPhysicalLeftAntiSemiHashJoinNotIn &) = delete;

	// ctor
	CPhysicalLeftAntiSemiHashJoinNotIn(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
		BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftAntiSemiHashJoinNotIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftAntiSemiHashJoinNotIn";
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

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

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalLeftAntiSemiHashJoinNotIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftAntiSemiHashJoinNotIn == pop->Eopid());

		return dynamic_cast<CPhysicalLeftAntiSemiHashJoinNotIn *>(pop);
	}

};	// class CPhysicalLeftAntiSemiHashJoinNotIn

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftAntiSemiHashJoinNotIn_H

// EOF
