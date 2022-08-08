//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalLeftOuterHashJoin.h
//
//	@doc:
//		Left outer hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftOuterHashJoin_H
#define GPOPT_CPhysicalLeftOuterHashJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftOuterHashJoin
//
//	@doc:
//		Left outer hash join operator
//
//---------------------------------------------------------------------------
class CPhysicalLeftOuterHashJoin : public CPhysicalHashJoin
{
private:
	// helper for deriving hash join distribution from hashed children
	CDistributionSpec *PdsDeriveFromHashedChildren(
		CMemoryPool *mp, CDistributionSpec *pdsOuter,
		CDistributionSpec *pdsInner) const;

public:
	CPhysicalLeftOuterHashJoin(const CPhysicalLeftOuterHashJoin &) = delete;

	// ctor
	CPhysicalLeftOuterHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
		BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CPhysicalLeftOuterHashJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftOuterHashJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftOuterHashJoin";
	}

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// conversion function
	static CPhysicalLeftOuterHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalLeftOuterHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftOuterHashJoin *>(pop);
	}


};	// class CPhysicalLeftOuterHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftOuterHashJoin_H

// EOF
