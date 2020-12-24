//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalLeftAntiSemiCorrelatedApplyNotIn.h
//
//	@doc:
//		Logical Left Anti Semi Correlated Apply operator;
//		a variant of left anti semi apply (for ALL/NOT IN subqueries)
//		to capture the need to implement a correlated-execution strategy
//		on the physical side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftAntiSemiCorrelatedApplyNotIn_H
#define GPOPT_CLogicalLeftAntiSemiCorrelatedApplyNotIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApplyNotIn.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftAntiSemiCorrelatedApplyNotIn
//
//	@doc:
//		Logical Apply operator used in correlated execution of NOT IN/ALL subqueries
//
//---------------------------------------------------------------------------
class CLogicalLeftAntiSemiCorrelatedApplyNotIn
	: public CLogicalLeftAntiSemiApplyNotIn
{
private:
public:
	CLogicalLeftAntiSemiCorrelatedApplyNotIn(
		const CLogicalLeftAntiSemiCorrelatedApplyNotIn &) = delete;

	// ctor
	explicit CLogicalLeftAntiSemiCorrelatedApplyNotIn(CMemoryPool *mp)
		: CLogicalLeftAntiSemiApplyNotIn(mp)
	{
	}

	// ctor
	CLogicalLeftAntiSemiCorrelatedApplyNotIn(CMemoryPool *mp,
											 CColRefArray *pdrgpcrInner,
											 EOperatorId eopidOriginSubq)
		: CLogicalLeftAntiSemiApplyNotIn(mp, pdrgpcrInner, eopidOriginSubq)
	{
	}

	// dtor
	~CLogicalLeftAntiSemiCorrelatedApplyNotIn() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftAntiSemiCorrelatedApplyNotIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftAntiSemiCorrelatedApplyNotIn";
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
	static CLogicalLeftAntiSemiCorrelatedApplyNotIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftAntiSemiCorrelatedApplyNotIn == pop->Eopid());

		return dynamic_cast<CLogicalLeftAntiSemiCorrelatedApplyNotIn *>(pop);
	}

};	// class CLogicalLeftAntiSemiCorrelatedApplyNotIn

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftAntiSemiCorrelatedApplyNotIn_H

// EOF
