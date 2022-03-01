//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2012 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterCorrelatedApply.h
//
//	@doc:
//		Logical Left Outer Correlated Apply operator;
//		a variant of left outer apply that captures the need to implement a
//		correlated-execution strategy on the physical side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftOuterCorrelatedApply_H
#define GPOPT_CLogicalLeftOuterCorrelatedApply_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftOuterApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftOuterCorrelatedApply
//
//	@doc:
//		Logical Apply operator used in scalar subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalLeftOuterCorrelatedApply : public CLogicalLeftOuterApply
{
private:
	BOOL m_allow_predicate_pushdown{true};

public:
	CLogicalLeftOuterCorrelatedApply(const CLogicalLeftOuterCorrelatedApply &) =
		delete;

	// ctor
	CLogicalLeftOuterCorrelatedApply(CMemoryPool *mp,
									 CColRefArray *pdrgpcrInner,
									 EOperatorId eopidOriginSubq);

	// ctor for patterns
	explicit CLogicalLeftOuterCorrelatedApply(CMemoryPool *mp);

	// dtor
	~CLogicalLeftOuterCorrelatedApply() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftOuterCorrelatedApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftOuterCorrelatedApply";
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// applicable transformations
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// return true if operator is a correlated apply
	BOOL
	FCorrelated() const override
	{
		return true;
	}

	BOOL
	IsPredicatePushDownAllowed() const
	{
		return m_allow_predicate_pushdown;
	}

	// conversion function
	static CLogicalLeftOuterCorrelatedApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLeftOuterCorrelatedApply == pop->Eopid());

		return dynamic_cast<CLogicalLeftOuterCorrelatedApply *>(pop);
	}

};	// class CLogicalLeftOuterCorrelatedApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftOuterCorrelatedApply_H

// EOF
