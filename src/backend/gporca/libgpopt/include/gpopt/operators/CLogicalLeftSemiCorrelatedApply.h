//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
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
public:
	CLogicalLeftSemiCorrelatedApply(const CLogicalLeftSemiCorrelatedApply &) =
		delete;

	// ctor for patterns
	explicit CLogicalLeftSemiCorrelatedApply(CMemoryPool *mp);

	// ctor
	CLogicalLeftSemiCorrelatedApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
									EOperatorId eopidOriginSubq);

	// dtor
	~CLogicalLeftSemiCorrelatedApply() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftSemiCorrelatedApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftSemiCorrelatedApply";
	}

	// applicable transformations
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// return true if operator is a correlated apply
	BOOL
	FCorrelated() const override
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

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
