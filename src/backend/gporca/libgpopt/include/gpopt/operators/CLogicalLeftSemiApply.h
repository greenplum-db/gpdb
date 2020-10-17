//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftSemiApply.h
//
//	@doc:
//		Logical Left Semi Apply operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftSemiApply_H
#define GPOPT_CLogicalLeftSemiApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalApply.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftSemiApply
//
//	@doc:
//		Logical Apply operator used in EXISTS subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalLeftSemiApply : public CLogicalApply
{
private:
	// private copy ctor
	CLogicalLeftSemiApply(const CLogicalLeftSemiApply &);

public:
	// ctor
	explicit CLogicalLeftSemiApply(CMemoryPool *mp) : CLogicalApply(mp)
	{
		m_pdrgpcrInner = GPOS_NEW(mp) CColRefArray(mp);
	}

	// ctor
	CLogicalLeftSemiApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
						  EOperatorId eopidOriginSubq)
		: CLogicalApply(mp, pdrgpcrInner, eopidOriginSubq)
	{
	}

	// dtor
	virtual ~CLogicalLeftSemiApply()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalLeftSemiApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalLeftSemiApply";
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
	virtual CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
											CExpressionHandle &exprhdl);

	// derive not nullable output columns
	virtual CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const
	{
		return PcrsDeriveNotNullPassThruOuter(exprhdl);
	}

	// derive keys
	CKeyCollection *
	DeriveKeyCollection(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl) const
	{
		return PkcDeriveKeysPassThru(exprhdl, 0 /*child_index*/);
	}

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

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// return true if operator is a left semi apply
	virtual BOOL
	FLeftSemiApply() const
	{
		return true;
	}

	// conversion function
	static CLogicalLeftSemiApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(CUtils::FLeftSemiApply(pop));

		return dynamic_cast<CLogicalLeftSemiApply *>(pop);
	}

};	// class CLogicalLeftSemiApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftSemiApply_H

// EOF
