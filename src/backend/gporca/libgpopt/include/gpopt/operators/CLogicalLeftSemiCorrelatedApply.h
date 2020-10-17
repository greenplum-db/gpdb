//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CLogicalLeftSemiCorrelatedApply.h
//
//	@doc:
//		Logical Left Semi Correlated Apply operator;
//		a variant of left semi apply that captures the need to implement a
//		correlated-execution strategy for EXISTS subquery
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftSemiCorrelatedApply_H
#define GPOPT_CLogicalLeftSemiCorrelatedApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftSemiApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftSemiCorrelatedApply
//
//	@doc:
//		Logical Apply operator used in scalar subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalLeftSemiCorrelatedApply : public CLogicalLeftSemiApply
{
private:
	// private copy ctor
	CLogicalLeftSemiCorrelatedApply(const CLogicalLeftSemiCorrelatedApply &);

public:
	// ctor for patterns
	explicit CLogicalLeftSemiCorrelatedApply(CMemoryPool *mp);

	// ctor
	CLogicalLeftSemiCorrelatedApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
									EOperatorId eopidOriginSubq);

	// dtor
	virtual ~CLogicalLeftSemiCorrelatedApply()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftSemiCorrelatedApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftSemiCorrelatedApply";
	}

	// applicable transformations
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	// return true if operator is a correlated apply
	virtual BOOL
	FCorrelated() const
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// conversion function
	static CLogicalLeftSemiCorrelatedApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftSemiCorrelatedApply == pop->Eopid());

		return dynamic_cast<CLogicalLeftSemiCorrelatedApply *>(pop);
	}

};	// class CLogicalLeftSemiCorrelatedApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftSemiCorrelatedApply_H

// EOF
