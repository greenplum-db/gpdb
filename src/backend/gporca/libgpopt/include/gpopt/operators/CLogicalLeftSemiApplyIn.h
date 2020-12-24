//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalLeftSemiApplyIn.h
//
//	@doc:
//		Logical Left Semi Apply operator used in IN/ANY subqueries
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftSemiApplyIn_H
#define GPOPT_CLogicalLeftSemiApplyIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftSemiApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftSemiApplyIn
//
//	@doc:
//		Logical Apply operator used in IN/ANY subqueries
//
//---------------------------------------------------------------------------
class CLogicalLeftSemiApplyIn : public CLogicalLeftSemiApply
{
private:
public:
	CLogicalLeftSemiApplyIn(const CLogicalLeftSemiApplyIn &) = delete;

	// ctor
	explicit CLogicalLeftSemiApplyIn(CMemoryPool *mp)
		: CLogicalLeftSemiApply(mp)
	{
	}

	// ctor
	CLogicalLeftSemiApplyIn(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
							EOperatorId eopidOriginSubq)
		: CLogicalLeftSemiApply(mp, pdrgpcrInner, eopidOriginSubq)
	{
	}

	// dtor
	~CLogicalLeftSemiApplyIn() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftSemiApplyIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftSemiApplyIn";
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
	static CLogicalLeftSemiApplyIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftSemiApplyIn == pop->Eopid());

		return dynamic_cast<CLogicalLeftSemiApplyIn *>(pop);
	}

};	// class CLogicalLeftSemiApplyIn

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftSemiApplyIn_H

// EOF
