//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2012 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterApply.h
//
//	@doc:
//		Logical Left Outer Apply operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftOuterApply_H
#define GPOPT_CLogicalLeftOuterApply_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftOuterApply
//
//	@doc:
//		Logical left outer Apply operator used in subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalLeftOuterApply : public CLogicalApply
{
private:
public:
	CLogicalLeftOuterApply(const CLogicalLeftOuterApply &) = delete;

	// ctor for patterns
	explicit CLogicalLeftOuterApply(CMemoryPool *mp);

	// ctor
	CLogicalLeftOuterApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
						   EOperatorId eopidOriginSubq);

	// dtor
	~CLogicalLeftOuterApply() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftOuterApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftOuterApply";
	}

	// return true if we can pull projections up past this operator from its given child
	BOOL
	FCanPullProjectionsUp(ULONG child_index) const override
	{
		return (0 == child_index);
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *
	DeriveOutputColumns(CMemoryPool *mp, CExpressionHandle &exprhdl) override
	{
		GPOS_ASSERT(3 == exprhdl.Arity());

		return PcrsDeriveOutputCombineLogical(mp, exprhdl);
	}

	// derive not nullable output columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const override
	{
		// left outer apply passes through not null columns from outer child only
		return PcrsDeriveNotNullPassThruOuter(exprhdl);
	}

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalLeftOuterApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLeftOuterApply == pop->Eopid());

		return dynamic_cast<CLogicalLeftOuterApply *>(pop);
	}

};	// class CLogicalLeftOuterApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftOuterApply_H

// EOF
