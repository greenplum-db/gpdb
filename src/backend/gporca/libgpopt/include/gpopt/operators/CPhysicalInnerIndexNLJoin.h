//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CPhysicalInnerIndexNLJoin.h
//
//	@doc:
//		Inner index nested-loops join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalInnerIndexNLJoin_H
#define GPOPT_CPhysicalInnerIndexNLJoin_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalInnerNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalInnerIndexNLJoin
//
//	@doc:
//		Inner index nested-loops join operator
//
//---------------------------------------------------------------------------
class CPhysicalInnerIndexNLJoin : public CPhysicalInnerNLJoin
{
private:
	// columns from outer child used for index lookup in inner child
	CColRefArray *m_pdrgpcrOuterRefs;

	// private copy ctor
	CPhysicalInnerIndexNLJoin(const CPhysicalInnerIndexNLJoin &);

public:
	// ctor
	CPhysicalInnerIndexNLJoin(CMemoryPool *mp, CColRefArray *colref_array);

	// dtor
	virtual ~CPhysicalInnerIndexNLJoin();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalInnerIndexNLJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalInnerIndexNLJoin";
	}

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// outer column references accessor
	CColRefArray *
	PdrgPcrOuterRefs() const
	{
		return m_pdrgpcrOuterRefs;
	}

	// compute required distribution of the n-th child
	virtual CDistributionSpec *PdsRequired(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CDistributionSpec *pdsRequired,
										   ULONG child_index,
										   CDrvdPropArray *pdrgpdpCtxt,
										   ULONG ulOptReq) const;

	// execution order of children
	virtual EChildExecOrder
	Eceo() const
	{
		// we optimize inner (right) child first to be able to match child hashed distributions
		return EceoRightToLeft;
	}

	// conversion function
	static CPhysicalInnerIndexNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalInnerIndexNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalInnerIndexNLJoin *>(pop);
	}

};	// class CPhysicalInnerIndexNLJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalInnerIndexNLJoin_H

// EOF
