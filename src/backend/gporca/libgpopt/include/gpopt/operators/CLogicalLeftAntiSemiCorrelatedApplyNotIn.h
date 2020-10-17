//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
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
	// private copy ctor
	CLogicalLeftAntiSemiCorrelatedApplyNotIn(
		const CLogicalLeftAntiSemiCorrelatedApplyNotIn &);

public:
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
	virtual ~CLogicalLeftAntiSemiCorrelatedApplyNotIn()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftAntiSemiCorrelatedApplyNotIn;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftAntiSemiCorrelatedApplyNotIn";
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

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
