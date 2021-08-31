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
	// private copy ctor
	CLogicalRightOuterJoin(const CLogicalRightOuterJoin &);

public:
	// ctor
	explicit CLogicalRightOuterJoin(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	virtual ~CLogicalRightOuterJoin()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalRightOuterJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalRightOuterJoin";
	}

	// return true if we can pull projections up past this operator from its given child
	virtual BOOL
	FCanPullProjectionsUp(ULONG child_index) const
	{
		return (1 == child_index);
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive not nullable output columns
	virtual CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const
	{
		// right outer join passes through not null columns from inner child only
		// may have additional children that are ignored, e.g., scalar children
		GPOS_ASSERT(1 <= exprhdl.Arity());

		CColRefSet *pcrs = exprhdl.DeriveNotNullColumns(1);
		pcrs->AddRef();

		return pcrs;
	}

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const
	{
		return PpcDeriveConstraintPassThru(exprhdl, 1 /*ulChild*/);
	}

	// promise level for stat derivation
	virtual EStatPromise
	Esp(CExpressionHandle &	 //exprhdl
	) const
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
	CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalRightOuterJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalRightOuterJoin == pop->Eopid());

		return dynamic_cast<CLogicalRightOuterJoin *>(pop);
	}

};	// class CLogicalRightOuterJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalRightOuterJoin_H

// EOF
