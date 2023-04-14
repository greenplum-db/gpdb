//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalInnerHashJoin.h
//
//	@doc:
//		Inner hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalInnerHashJoin_H
#define GPOPT_CPhysicalInnerHashJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalInnerHashJoin
//
//	@doc:
//		Inner hash join operator
//
//---------------------------------------------------------------------------
class CPhysicalInnerHashJoin : public CPhysicalHashJoin
{
private:
	// helper for computing a hashed distribution matching the given distribution
	CDistributionSpecHashed *PdshashedCreateMatching(
		CMemoryPool *mp, CDistributionSpecHashed *pdshashed,
		ULONG ulSourceChild) const;

	// helper for deriving hash join distribution from hashed children
	CDistributionSpec *PdsDeriveFromHashedChildren(
		CMemoryPool *mp, CDistributionSpec *pdsOuter,
		CDistributionSpec *pdsInner) const;

	// helper for deriving hash join distribution from replicated outer child
	CDistributionSpec *PdsDeriveFromReplicatedOuter(
		CMemoryPool *mp, CDistributionSpec *pdsOuter,
		CDistributionSpec *pdsInner) const;

	// helper for deriving hash join distribution from hashed outer child
	CDistributionSpec *PdsDeriveFromHashedOuter(
		CMemoryPool *mp, CDistributionSpec *pdsOuter,
		CDistributionSpec *pdsInner) const;

public:
	CPhysicalInnerHashJoin(const CPhysicalInnerHashJoin &) = delete;

	// ctor
	CPhysicalInnerHashJoin(CMemoryPool *mp,
						   CExpressionArray *pdrgpexprOuterKeys,
						   CExpressionArray *pdrgpexprInnerKeys,
						   IMdIdArray *hash_opfamilies,
						   BOOL is_null_aware = true,
						   CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CPhysicalInnerHashJoin() override;

	// ident accessors

	EOperatorId
	Eopid() const override
	{
		return EopPhysicalInnerHashJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalInnerHashJoin";
	}

	// conversion function
	static CPhysicalInnerHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalInnerHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalInnerHashJoin *>(pop);
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
};	// class CPhysicalInnerHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalInnerHashJoin_H

// EOF
