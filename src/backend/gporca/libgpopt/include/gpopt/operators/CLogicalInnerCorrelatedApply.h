//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalInnerCorrelatedApply.h
//
//	@doc:
//		Logical Inner Correlated Apply operator;
//		a variant of inner apply that captures the need to implement a
//		correlated-execution strategy on the physical side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalInnerCorrelatedApply_H
#define GPOPT_CLogicalInnerCorrelatedApply_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalInnerCorrelatedApply
//
//	@doc:
//		Logical Apply operator used in scalar subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalInnerCorrelatedApply : public CLogicalInnerApply
{
private:
	// private copy ctor
	CLogicalInnerCorrelatedApply(const CLogicalInnerCorrelatedApply &);

public:
	// ctor
	CLogicalInnerCorrelatedApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
								 EOperatorId eopidOriginSubq);

	// ctor for patterns
	explicit CLogicalInnerCorrelatedApply(CMemoryPool *mp);

	// dtor
	virtual ~CLogicalInnerCorrelatedApply()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalInnerCorrelatedApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalInnerCorrelatedApply";
	}

	// applicable transformations
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// return true if operator is a correlated apply
	virtual BOOL
	FCorrelated() const
	{
		return true;
	}

	// conversion function
	static CLogicalInnerCorrelatedApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalInnerCorrelatedApply == pop->Eopid());

		return dynamic_cast<CLogicalInnerCorrelatedApply *>(pop);
	}

};	// class CLogicalInnerCorrelatedApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalInnerCorrelatedApply_H

// EOF
