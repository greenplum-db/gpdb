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
	// private copy ctor
	CLogicalDifference(const CLogicalDifference &);

public:
	// ctor
	explicit CLogicalDifference(CMemoryPool *mp);

	CLogicalDifference(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
					   CColRef2dArray *pdrgpdrgpcrInput);

	// dtor
	virtual ~CLogicalDifference();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalDifference;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CLogicalDifference";
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

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
	CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	//-------------------------------------------------------------------------------------
	// Derived Stats
	//-------------------------------------------------------------------------------------

	// stat promise
	virtual EStatPromise
	Esp(CExpressionHandle &) const
	{
		return CLogical::EspHigh;
	}

	// derive statistics
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  IStatisticsArray *stats_ctxt) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalDifference *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalDifference == pop->Eopid());

		return reinterpret_cast<CLogicalDifference *>(pop);
	}

};	// class CLogicalDifference

}  // namespace gpopt


#endif	// !GPOPT_CLogicalDifference_H

// EOF
