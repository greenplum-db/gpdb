//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalIntersectAll.h
//
//	@doc:
//		Logical Intersect all operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalIntersectAll_H
#define GPOPT_CLogicalIntersectAll_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalSetOp.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalIntersectAll
//
//	@doc:
//		Intersect all operators
//
//---------------------------------------------------------------------------
class CLogicalIntersectAll : public CLogicalSetOp
{
private:
public:
	CLogicalIntersectAll(const CLogicalIntersectAll &) = delete;

	// ctor
	explicit CLogicalIntersectAll(CMemoryPool *mp);

	CLogicalIntersectAll(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
						 CColRef2dArray *pdrgpdrgpcrInput);

	// dtor
	~CLogicalIntersectAll() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalIntersectAll;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalIntersectAll";
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive key collections
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintSetop(mp, exprhdl, true /*fIntersect*/);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	// Derived Stats
	//-------------------------------------------------------------------------------------

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalIntersectAll *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalIntersectAll == pop->Eopid());

		return dynamic_cast<CLogicalIntersectAll *>(pop);
	}

	// derive statistics
	static IStatistics *PstatsDerive(CMemoryPool *mp,
									 CExpressionHandle &exprhdl,
									 CColRef2dArray *pdrgpdrgpcrInput,
									 CColRefSetArray *output_colrefsets);

};	// class CLogicalIntersectAll

}  // namespace gpopt

#endif	// !GPOPT_CLogicalIntersectAll_H

// EOF
