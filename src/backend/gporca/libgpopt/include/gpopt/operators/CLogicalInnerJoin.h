//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalInnerJoin.h
//
//	@doc:
//		Inner join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalInnerJoin_H
#define GPOS_CLogicalInnerJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
// fwd declaration
class CColRefSet;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalInnerJoin
//
//	@doc:
//		Inner join operator
//
//---------------------------------------------------------------------------
class CLogicalInnerJoin : public CLogicalJoin
{
private:
public:
	CLogicalInnerJoin(const CLogicalInnerJoin &) = delete;

	// ctor
	explicit CLogicalInnerJoin(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CLogicalInnerJoin() override = default;


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalInnerJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalInnerJoin";
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

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
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalInnerJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalInnerJoin == pop->Eopid());

		return dynamic_cast<CLogicalInnerJoin *>(pop);
	}

	// determine if an innerJoin group expression has
	// less conjuncts than another
	static BOOL FFewerConj(CMemoryPool *mp, CGroupExpression *pgexprFst,
						   CGroupExpression *pgexprSnd);


};	// class CLogicalInnerJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalInnerJoin_H

// EOF
