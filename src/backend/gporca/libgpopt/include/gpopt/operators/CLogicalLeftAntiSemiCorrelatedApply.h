//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalLeftAntiSemiCorrelatedApply.h
//
//	@doc:
//		Logical Left Anti Semi Correlated Apply operator;
//		a variant of left anti semi apply (for NOT EXISTS subqueries)
//		to capture the need to implement a correlated-execution strategy
//		on the physical side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftAntiSemiCorrelatedApply_H
#define GPOPT_CLogicalLeftAntiSemiCorrelatedApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftAntiSemiCorrelatedApply
//
//	@doc:
//		Logical Apply operator used in correlated execution of NOT EXISTS subqueries
//
//---------------------------------------------------------------------------
class CLogicalLeftAntiSemiCorrelatedApply : public CLogicalLeftAntiSemiApply
{
private:
public:
	CLogicalLeftAntiSemiCorrelatedApply(
		const CLogicalLeftAntiSemiCorrelatedApply &) = delete;

	// ctor
	explicit CLogicalLeftAntiSemiCorrelatedApply(CMemoryPool *mp)
		: CLogicalLeftAntiSemiApply(mp)
	{
	}

	// ctor
	CLogicalLeftAntiSemiCorrelatedApply(CMemoryPool *mp,
										CColRefArray *pdrgpcrInner,
										EOperatorId eopidOriginSubq)
		: CLogicalLeftAntiSemiApply(mp, pdrgpcrInner, eopidOriginSubq)
	{
	}

	// dtor
	~CLogicalLeftAntiSemiCorrelatedApply() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftAntiSemiCorrelatedApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftAntiSemiCorrelatedApply";
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

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
	static CLogicalLeftAntiSemiCorrelatedApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftAntiSemiCorrelatedApply == pop->Eopid());

		return dynamic_cast<CLogicalLeftAntiSemiCorrelatedApply *>(pop);
	}

};	// class CLogicalLeftAntiSemiCorrelatedApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftAntiSemiCorrelatedApply_H

// EOF
