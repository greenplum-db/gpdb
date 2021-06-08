//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDifference.h
//
//	@doc:
//		Logical Difference operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDifference_H
#define GPOPT_CLogicalDifference_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalSetOp.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalDifference
//
//	@doc:
//		Difference operators
//
//---------------------------------------------------------------------------
class CLogicalDifference : public CLogicalSetOp
{
private:
public:
	CLogicalDifference(const CLogicalDifference &) = delete;

	// ctor
	explicit CLogicalDifference(CMemoryPool *mp);

	CLogicalDifference(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
					   CColRef2dArray *pdrgpdrgpcrInput);

	// dtor
	~CLogicalDifference() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalDifference;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalDifference";
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

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintSetop(mp, exprhdl, false /*fIntersect*/);
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
	static CLogicalDifference *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalDifference == pop->Eopid());

		return dynamic_cast<CLogicalDifference *>(pop);
	}

};	// class CLogicalDifference

}  // namespace gpopt


#endif	// !GPOPT_CLogicalDifference_H

// EOF
