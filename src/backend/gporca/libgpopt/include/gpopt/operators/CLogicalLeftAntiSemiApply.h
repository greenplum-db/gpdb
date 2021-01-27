//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftAntiSemiApply.h
//
//	@doc:
//		Logical Left Anti Semi Apply operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftAntiSemiApply_H
#define GPOPT_CLogicalLeftAntiSemiApply_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftAntiSemiApply
//
//	@doc:
//		Logical Left Anti Semi Apply operator
//
//---------------------------------------------------------------------------
class CLogicalLeftAntiSemiApply : public CLogicalApply
{
private:
	// private copy ctor
	CLogicalLeftAntiSemiApply(const CLogicalLeftAntiSemiApply &);

public:
	// ctor
	explicit CLogicalLeftAntiSemiApply(CMemoryPool *mp) : CLogicalApply(mp)
	{
	}

	// ctor
	CLogicalLeftAntiSemiApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
							  EOperatorId eopidOriginSubq)
		: CLogicalApply(mp, pdrgpcrInner, eopidOriginSubq)
	{
	}

	// dtor
	virtual ~CLogicalLeftAntiSemiApply()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftAntiSemiApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftAntiSemiApply";
	}

	// return true if we can pull projections up past this operator from its given child
	virtual BOOL
	FCanPullProjectionsUp(ULONG child_index) const
	{
		return (0 == child_index);
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	virtual CColRefSet *
	DeriveOutputColumns(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl)
	{
		GPOS_ASSERT(3 == exprhdl.Arity());

		return PcrsDeriveOutputPassThru(exprhdl);
	}

	// derive not nullable output columns
	virtual CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const
	{
		return PcrsDeriveNotNullPassThruOuter(exprhdl);
	}

	// dervive keys
	virtual CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const;

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
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// return true if operator is a left anti semi apply
	virtual BOOL
	FLeftAntiSemiApply() const
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// conversion function
	static CLogicalLeftAntiSemiApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(CUtils::FLeftAntiSemiApply(pop));

		return dynamic_cast<CLogicalLeftAntiSemiApply *>(pop);
	}

};	// class CLogicalLeftAntiSemiApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftAntiSemiApply_H

// EOF
