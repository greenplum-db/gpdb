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

	// private copy ctor
	CPhysicalLeftOuterHashJoin(const CPhysicalLeftOuterHashJoin &);

public:
	// ctor
	CPhysicalLeftOuterHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys,
		IMdIdArray *hash_opfamilies = NULL,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	virtual ~CPhysicalLeftOuterHashJoin();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalLeftOuterHashJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalLeftOuterHashJoin";
	}

	// derive distribution
	virtual CDistributionSpec *PdsDerive(CMemoryPool *mp,
										 CExpressionHandle &exprhdl) const;

	// conversion function
	static CPhysicalLeftOuterHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalLeftOuterHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftOuterHashJoin *>(pop);
	}


};	// class CPhysicalLeftOuterHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftOuterHashJoin_H

// EOF
