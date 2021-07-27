//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CLogicalFullOuterJoin.h
//
//	@doc:
//		Full outer join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalFullOuterJoin_H
#define GPOS_CLogicalFullOuterJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalFullOuterJoin
//
//	@doc:
//		Full outer join operator
//
//---------------------------------------------------------------------------
class CLogicalFullOuterJoin : public CLogicalJoin
{
private:
public:
	CLogicalFullOuterJoin(const CLogicalFullOuterJoin &) = delete;

	// ctor
	explicit CLogicalFullOuterJoin(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CLogicalFullOuterJoin() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalFullOuterJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalFullOuterJoin";
	}

	// return true if we can pull projections up past this operator from its given child
	BOOL FCanPullProjectionsUp(ULONG  //child_index
	) const override
	{
		return false;
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive not nullable output columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *mp,
						 CExpressionHandle &  //exprhdl
	) const override
	{
		// all output columns are nullable
		return GPOS_NEW(mp) CColRefSet(mp);
	}

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &  //exprhdl
	) const override
	{
		return GPOS_NEW(mp) CPropConstraint(
			mp, GPOS_NEW(mp) CColRefSetArray(mp), nullptr /*pcnstr*/);
	}

	// promise level for stat derivation
	EStatPromise
	Esp(CExpressionHandle &	 //exprhdl
	) const override
	{
		// Disable stats derivation for CLogicalFullOuterJoin because it is
		// currently not implemented. Instead rely on stats coming from the
		// equivalent UNION expression (formed using ExfExpandFullOuterJoin).
		// Also see CXformCTEAnchor2Sequence::Transform() and
		// CXformCTEAnchor2TrivialSelect::Transform().
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
	static CLogicalFullOuterJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalFullOuterJoin == pop->Eopid());

		return dynamic_cast<CLogicalFullOuterJoin *>(pop);
	}

};	// class CLogicalFullOuterJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalFullOuterJoin_H

// EOF
