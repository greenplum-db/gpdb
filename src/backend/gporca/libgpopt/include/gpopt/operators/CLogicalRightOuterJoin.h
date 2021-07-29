//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CLogicalRightOuterJoin.h
//
//	@doc:
//		Right outer join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalRightOuterJoin_H
#define GPOS_CLogicalRightOuterJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalRightOuterJoin
//
//	@doc:
//		Right outer join operator
//
//---------------------------------------------------------------------------
class CLogicalRightOuterJoin : public CLogicalJoin
{
private:
public:
	CLogicalRightOuterJoin(const CLogicalRightOuterJoin &) = delete;

	// ctor
	explicit CLogicalRightOuterJoin(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CLogicalRightOuterJoin() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalRightOuterJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalRightOuterJoin";
	}

	// return true if we can pull projections up past this operator from its given child
	BOOL
	FCanPullProjectionsUp(ULONG child_index) const override
	{
		return (1 == child_index);
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive not nullable output columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const override
	{
		// right outer join passes through not null columns from inner child only
		// may have additional children that are ignored, e.g., scalar children
		GPOS_ASSERT(1 <= exprhdl.Arity());

		CColRefSet *pcrs = exprhdl.DeriveNotNullColumns(1);
		pcrs->AddRef();

		return pcrs;
	}

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, 1 /*ulChild*/);
	}

	// promise level for stat derivation
	EStatPromise
	Esp(CExpressionHandle &	 //exprhdl
	) const override
	{
		// Disable stats derivation for CLogicalRightOuterJoin because it is
		// currently not implemented. Instead rely on stats coming from the
		// equivalent LOJ expression.
		return EspLow;
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
	static CLogicalRightOuterJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalRightOuterJoin == pop->Eopid());

		return dynamic_cast<CLogicalRightOuterJoin *>(pop);
	}

};	// class CLogicalRightOuterJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalRightOuterJoin_H

// EOF
