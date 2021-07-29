//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterJoin.h
//
//	@doc:
//		Left outer join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalLeftOuterJoin_H
#define GPOS_CLogicalLeftOuterJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftOuterJoin
//
//	@doc:
//		Left outer join operator
//
//---------------------------------------------------------------------------
class CLogicalLeftOuterJoin : public CLogicalJoin
{
private:
public:
	CLogicalLeftOuterJoin(const CLogicalLeftOuterJoin &) = delete;

	// ctor
	explicit CLogicalLeftOuterJoin(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CLogicalLeftOuterJoin() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftOuterJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftOuterJoin";
	}

	// return true if we can pull projections up past this operator from its given child
	BOOL
	FCanPullProjectionsUp(ULONG child_index) const override
	{
		return (0 == child_index);
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive not nullable output columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const override
	{
		// left outer join passes through not null columns from outer child only
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
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalLeftOuterJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLeftOuterJoin == pop->Eopid());

		return dynamic_cast<CLogicalLeftOuterJoin *>(pop);
	}

};	// class CLogicalLeftOuterJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalLeftOuterJoin_H

// EOF
