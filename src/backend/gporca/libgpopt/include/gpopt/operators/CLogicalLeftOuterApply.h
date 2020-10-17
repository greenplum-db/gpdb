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
#include "gpopt/operators/CLogicalApply.h"
#include "gpopt/operators/CExpressionHandle.h"

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
	// private copy ctor
	CLogicalLeftOuterApply(const CLogicalLeftOuterApply &);

public:
	// ctor for patterns
	explicit CLogicalLeftOuterApply(CMemoryPool *mp);

	// ctor
	CLogicalLeftOuterApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
						   EOperatorId eopidOriginSubq);

	// dtor
	virtual ~CLogicalLeftOuterApply();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftOuterApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftOuterApply";
	}

	// return true if we can pull projections up past this operator from its given child
	virtual BOOL
	FCanPullProjectionsUp(ULONG child_index) const
	{
		return (0 == child_index);
	}

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	virtual CColRefSet *
	DeriveOutputColumns(CMemoryPool *mp, CExpressionHandle &exprhdl)
	{
		GPOS_ASSERT(3 == exprhdl.Arity());

		return PcrsDeriveOutputCombineLogical(mp, exprhdl);
	}

	// derive not nullable output columns
	virtual CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const
	{
		// left outer apply passes through not null columns from outer child only
		return PcrsDeriveNotNullPassThruOuter(exprhdl);
	}

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const
	{
		return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	virtual CXformSet *PxfsCandidates(CMemoryPool *) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalLeftOuterApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalLeftOuterApply == pop->Eopid());

		return dynamic_cast<CLogicalLeftOuterApply *>(pop);
	}

};	// class CLogicalLeftOuterApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftOuterApply_H

// EOF
