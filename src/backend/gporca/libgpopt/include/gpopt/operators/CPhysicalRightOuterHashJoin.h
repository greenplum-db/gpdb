//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CPhysicalRightOuterHashJoin.h
//
//	@doc:
//		Right outer hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalRightOuterHashJoin_H
#define GPOPT_CPhysicalRightOuterHashJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalRightOuterHashJoin
//
//	@doc:
//		Right outer hash join operator
//
//---------------------------------------------------------------------------
class CPhysicalRightOuterHashJoin : public CPhysicalHashJoin
{
private:
protected:
	// create optimization requests
	void CreateOptRequests(CMemoryPool *mp) override;

public:
	CPhysicalRightOuterHashJoin(const CPhysicalRightOuterHashJoin &) = delete;

	// ctor
	CPhysicalRightOuterHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
		BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CPhysicalRightOuterHashJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalRightOuterHashJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalRightOuterHashJoin";
	}

	// conversion function
	static CPhysicalRightOuterHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalRightOuterHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalRightOuterHashJoin *>(pop);
	}
	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required distribution of the n-th child
	CEnfdDistribution *Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
						   CReqdPropPlan *prppInput, ULONG child_index,
						   CDrvdPropArray *pdrgpdpCtxt,
						   ULONG ulOptReq) override;

};	// class CPhysicalRightOuterHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalRightOuterHashJoin_H

// EOF
