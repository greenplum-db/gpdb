//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUnionAll.h
//
//	@doc:
//		Logical Union all operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalUnionAll_H
#define GPOPT_CLogicalUnionAll_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalUnion.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalUnionAll
//
//	@doc:
//		Union all operators
//
//---------------------------------------------------------------------------
class CLogicalUnionAll : public CLogicalUnion
{
public:
	CLogicalUnionAll(const CLogicalUnionAll &) = delete;

	// ctor
	explicit CLogicalUnionAll(CMemoryPool *mp);

	CLogicalUnionAll(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
					 CColRef2dArray *pdrgpdrgpcrInput);

	// dtor
	~CLogicalUnionAll() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalUnionAll;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalUnionAll";
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

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;


	// conversion function
	static CLogicalUnionAll *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalUnionAll == pop->Eopid());

		return dynamic_cast<CLogicalUnionAll *>(pop);
	}

	// derive statistics based on union all semantics
	static IStatistics *PstatsDeriveUnionAll(CMemoryPool *mp,
											 CExpressionHandle &exprhdl);

};	// class CLogicalUnionAll

}  // namespace gpopt

#endif	// !GPOPT_CLogicalUnionAll_H

// EOF
