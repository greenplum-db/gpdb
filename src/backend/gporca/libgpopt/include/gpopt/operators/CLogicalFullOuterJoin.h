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
	// private copy ctor
	CLogicalFullOuterJoin(const CLogicalFullOuterJoin &);

public:
	// ctor
	explicit CLogicalFullOuterJoin(CMemoryPool *mp);

	// dtor
	virtual ~CLogicalFullOuterJoin()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalFullOuterJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalFullOuterJoin";
	}

	// return true if we can pull projections up past this operator from its given child
	virtual BOOL FCanPullProjectionsUp(ULONG  //child_index
	) const
	{
		return false;
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive not nullable output columns
	virtual CColRefSet *
	DeriveNotNullColumns(CMemoryPool *mp,
						 CExpressionHandle &  //exprhdl
	) const
	{
		// all output columns are nullable
		return GPOS_NEW(mp) CColRefSet(mp);
	}

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &  //exprhdl
	) const
	{
		return GPOS_NEW(mp) CPropConstraint(
			mp, GPOS_NEW(mp) CColRefSetArray(mp), NULL /*pcnstr*/);
	}

	// promise level for stat derivation
	virtual EStatPromise
	Esp(CExpressionHandle &	 //exprhdl
	) const
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
	CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalFullOuterJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalFullOuterJoin == pop->Eopid());

		return dynamic_cast<CLogicalFullOuterJoin *>(pop);
	}

};	// class CLogicalFullOuterJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalFullOuterJoin_H

// EOF
