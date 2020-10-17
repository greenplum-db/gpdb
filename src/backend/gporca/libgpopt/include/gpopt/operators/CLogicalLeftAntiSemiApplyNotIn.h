//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2013 EMC Corp.
//
//	@filename:
//		CLogicalLeftAntiSemiApplyNotIn.h
//
//	@doc:
//		Logical Left Anti Semi Apply operator used in NOT IN/ALL subqueries
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftAntiSemiApplyNotIn_H
#define GPOPT_CLogicalLeftAntiSemiApplyNotIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftAntiSemiApplyNotIn
//
//	@doc:
//		Logical Apply operator used in NOT IN/ALL subqueries
//
//---------------------------------------------------------------------------
class CLogicalLeftAntiSemiApplyNotIn : public CLogicalLeftAntiSemiApply
{
private:
	// private copy ctor
	CLogicalLeftAntiSemiApplyNotIn(const CLogicalLeftAntiSemiApplyNotIn &);

public:
	// ctor
	explicit CLogicalLeftAntiSemiApplyNotIn(CMemoryPool *mp)
		: CLogicalLeftAntiSemiApply(mp)
	{
	}

	// ctor
	CLogicalLeftAntiSemiApplyNotIn(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
								   EOperatorId eopidOriginSubq)
		: CLogicalLeftAntiSemiApply(mp, pdrgpcrInner, eopidOriginSubq)
	{
	}

	// dtor
	virtual ~CLogicalLeftAntiSemiApplyNotIn()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftAntiSemiApplyNotIn;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftAntiSemiApplyNotIn";
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// conversion function
	static CLogicalLeftAntiSemiApplyNotIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftAntiSemiApplyNotIn == pop->Eopid());

		return dynamic_cast<CLogicalLeftAntiSemiApplyNotIn *>(pop);
	}

};	// class CLogicalLeftAntiSemiApplyNotIn

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftAntiSemiApplyNotIn_H

// EOF
