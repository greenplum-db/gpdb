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
public:
	CLogicalLeftSemiApply(const CLogicalLeftSemiApply &) = delete;

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
	~CLogicalLeftSemiApply() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftSemiApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftSemiApply";
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

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &exprhdl) override;

	// derive not nullable output columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const override
	{
		return PcrsDeriveNotNullPassThruOuter(exprhdl);
	}

	// derive keys
	CKeyCollection *
	DeriveKeyCollection(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl) const override
	{
		return PkcDeriveKeysPassThru(exprhdl, 0 /*child_index*/);
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

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// return true if operator is a left semi apply
	BOOL
	FLeftSemiApply() const override
	{
		return true;
	}

	// conversion function
	static CLogicalLeftSemiApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(CUtils::FLeftSemiApply(pop));

		return dynamic_cast<CLogicalLeftSemiApply *>(pop);
	}

};	// class CLogicalLeftSemiApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalLeftSemiApply_H

// EOF
