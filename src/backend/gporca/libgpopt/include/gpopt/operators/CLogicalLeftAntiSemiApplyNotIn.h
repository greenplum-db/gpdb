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

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"

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
public:
	CLogicalLeftAntiSemiApplyNotIn(const CLogicalLeftAntiSemiApplyNotIn &) =
		delete;

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
	~CLogicalLeftAntiSemiApplyNotIn() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftAntiSemiApplyNotIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftAntiSemiApplyNotIn";
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// conversion function
	static CLogicalLeftAntiSemiApplyNotIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLeftAntiSemiApplyNotIn == pop->Eopid());

		return dynamic_cast<CLogicalLeftAntiSemiApplyNotIn *>(pop);
	}

};	// class CLogicalLeftAntiSemiApplyNotIn

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftAntiSemiApplyNotIn_H

// EOF
