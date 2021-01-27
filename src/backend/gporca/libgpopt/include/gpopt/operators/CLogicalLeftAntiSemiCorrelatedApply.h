//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
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

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"

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
	// private copy ctor
	CLogicalLeftAntiSemiCorrelatedApply(
		const CLogicalLeftAntiSemiCorrelatedApply &);

public:
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
	virtual ~CLogicalLeftAntiSemiCorrelatedApply()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftAntiSemiCorrelatedApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftAntiSemiCorrelatedApply";
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
