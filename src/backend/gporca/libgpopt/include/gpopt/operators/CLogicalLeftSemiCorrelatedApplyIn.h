//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalLeftSemiCorrelatedApplyIn.h
//
//	@doc:
//		Logical Left Semi Correlated Apply operator;
//		a variant of left semi apply that captures the need to implement a
//		correlated-execution strategy on the physical side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftSemiCorrelatedApplyIn_H
#define GPOPT_CLogicalLeftSemiCorrelatedApplyIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftSemiApplyIn.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftSemiCorrelatedApplyIn
//
//	@doc:
//		Logical Apply operator used in scalar subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalLeftSemiCorrelatedApplyIn : public CLogicalLeftSemiApplyIn
{
private:
public:
	CLogicalLeftSemiCorrelatedApplyIn(
		const CLogicalLeftSemiCorrelatedApplyIn &) = delete;

	// ctor for patterns
	explicit CLogicalLeftSemiCorrelatedApplyIn(CMemoryPool *mp);

	// ctor
	CLogicalLeftSemiCorrelatedApplyIn(CMemoryPool *mp,
									  CColRefArray *pdrgpcrInner,
									  EOperatorId eopidOriginSubq);

	// dtor
	~CLogicalLeftSemiCorrelatedApplyIn() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftSemiCorrelatedApplyIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftSemiCorrelatedApplyIn";
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
	static CLogicalLeftSemiCorrelatedApplyIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftSemiCorrelatedApplyIn == pop->Eopid());

		return dynamic_cast<CLogicalLeftSemiCorrelatedApplyIn *>(pop);
	}

};	// class CLogicalLeftSemiCorrelatedApplyIn

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftSemiCorrelatedApplyIn_H

// EOF
