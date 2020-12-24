//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalMaxOneRow.h
//
//	@doc:
//		MaxOneRow operator,
//		an operator that can pass at most one row from its input
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalMaxOneRow_H
#define GPOPT_CLogicalMaxOneRow_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalMaxOneRow
//
//	@doc:
//		MaxOneRow operator
//
//---------------------------------------------------------------------------
class CLogicalMaxOneRow : public CLogical
{
private:
public:
	CLogicalMaxOneRow(const CLogicalMaxOneRow &) = delete;

	// ctors
	explicit CLogicalMaxOneRow(CMemoryPool *mp) : CLogical(mp)
	{
	}


	// dtor
	~CLogicalMaxOneRow() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalMaxOneRow;
	}

	// name of operator
	const CHAR *
	SzId() const override
	{
		return "CLogicalMaxOneRow";
	}

	// match function;
	BOOL
	Matches(COperator *pop) const override
	{
		return (Eopid() == pop->Eopid());
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
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

	// derive output columns
	CColRefSet *
	DeriveOutputColumns(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl) override
	{
		return PcrsDeriveOutputPassThru(exprhdl);
	}

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *mp,
						CExpressionHandle &exprhdl) const override
	{
		return PpartinfoDeriveCombine(mp, exprhdl);
	}

	// dervive keys
	CKeyCollection *
	DeriveKeyCollection(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl) const override
	{
		return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
	}

	// derive max card
	CMaxCard
	DeriveMaxCard(CMemoryPool *,	   // mp,
				  CExpressionHandle &  // exprhdl
	) const override
	{
		return CMaxCard(1 /*ull*/);
	}

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintFromPredicates(mp, exprhdl);
	}

	// promise level for stat derivation
	EStatPromise Esp(CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *PcrsStat(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CColRefSet *pcrsInput,
						 ULONG child_index) const override;

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalMaxOneRow *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalMaxOneRow == pop->Eopid());

		return reinterpret_cast<CLogicalMaxOneRow *>(pop);
	}

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *  // stats_ctxt
	) const override;


};	// class CLogicalMaxOneRow

}  // namespace gpopt

#endif	// !GPOPT_CLogicalMaxOneRow_H

// EOF
