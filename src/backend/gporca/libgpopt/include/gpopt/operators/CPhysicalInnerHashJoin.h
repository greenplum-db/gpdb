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

	// private copy ctor
	CPhysicalInnerHashJoin(const CPhysicalInnerHashJoin &);

public:
	// ctor
	CPhysicalInnerHashJoin(CMemoryPool *mp,
						   CExpressionArray *pdrgpexprOuterKeys,
						   CExpressionArray *pdrgpexprInnerKeys,
						   IMdIdArray *hash_opfamilies = NULL,
						   BOOL is_null_aware = true,
						   CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	virtual ~CPhysicalInnerHashJoin();

	// ident accessors

	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalInnerHashJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
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
	virtual CDistributionSpec *PdsDerive(CMemoryPool *mp,
										 CExpressionHandle &exprhdl) const;

	// compute required partition propagation of the n-th child
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

};	// class CPhysicalInnerHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalInnerHashJoin_H

// EOF
