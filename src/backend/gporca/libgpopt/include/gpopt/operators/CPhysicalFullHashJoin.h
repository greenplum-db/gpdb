//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 VMware by Broadcom.
//
//	@filename:
//		CPhysicalFullHashJoin.h
//
//	@doc:
//		Full outer hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalFullHashJoin_H
#define GPOPT_CPhysicalFullHashJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalRightOuterHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalFullHashJoin
//
//	@doc:
//		Full outer hash join operator
//
//---------------------------------------------------------------------------
class CPhysicalFullHashJoin : public CPhysicalRightOuterHashJoin
{
public:
	CPhysicalFullHashJoin(const CPhysicalFullHashJoin &) = delete;

	// ctor
	CPhysicalFullHashJoin(CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
						  CExpressionArray *pdrgpexprInnerKeys,
						  IMdIdArray *hash_opfamilies, BOOL is_null_aware,
						  CXform::EXformId origin_xform);

	// dtor
	~CPhysicalFullHashJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalFullHashJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalFullHashJoin";
	}

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const override;

	CPartitionPropagationSpec *PppsDerive(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// conversion function
	static CPhysicalFullHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalFullHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalFullHashJoin *>(pop);
	}


};	// class CPhysicalFullHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalFullHashJoin_H

// EOF
