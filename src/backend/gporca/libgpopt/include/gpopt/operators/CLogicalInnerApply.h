//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2011 EMC Corp.
//
//	@filename:
//		CLogicalInnerApply.h
//
//	@doc:
//		Logical Inner Apply operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalInnerApply_H
#define GPOPT_CLogicalInnerApply_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalInnerApply
//
//	@doc:
//		Logical Apply operator used in scalar subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalInnerApply : public CLogicalApply
{
private:
public:
	CLogicalInnerApply(const CLogicalInnerApply &) = delete;

	// ctor for patterns
	explicit CLogicalInnerApply(CMemoryPool *mp);

	// ctor
	CLogicalInnerApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
					   EOperatorId eopidOriginSubq);

	// dtor
	~CLogicalInnerApply() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalInnerApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalInnerApply";
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

	// derive not nullable columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *mp,
						 CExpressionHandle &exprhdl) const override
	{
		return PcrsDeriveNotNullCombineLogical(mp, exprhdl);
	}

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintFromPredicates(mp, exprhdl);
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
	static CLogicalInnerApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalInnerApply == pop->Eopid());

		return dynamic_cast<CLogicalInnerApply *>(pop);
	}

};	// class CLogicalInnerApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalInnerApply_H

// EOF
