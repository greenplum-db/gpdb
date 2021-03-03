//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalSequence.h
//
//	@doc:
//		Logical sequence operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalSequence_H
#define GPOPT_CLogicalSequence_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalSequence
//
//	@doc:
//		Logical sequence operator
//
//---------------------------------------------------------------------------
class CLogicalSequence : public CLogical
{
private:
public:
	CLogicalSequence(const CLogicalSequence &) = delete;

	// ctor
	explicit CLogicalSequence(CMemoryPool *mp);

	// dtor
	~CLogicalSequence() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalSequence;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalSequence";
	}

	// match function
	BOOL Matches(COperator *pop) const override;


	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
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

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &exprhdl) override;

	// dervive keys
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive partition consumer info
	CPartInfo *DerivePartitionInfo(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, exprhdl.Arity() - 1);
	}

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *mp, CExpressionHandle &exprhdl, CColRefSet *pcrsInput,
			 ULONG child_index) const override
	{
		const ULONG ulLastChildIndex = exprhdl.Arity() - 1;
		if (child_index == ulLastChildIndex)
		{
			// only pass through the required stats column to the last child since
			// the output of the sequence operator is the output of the last child
			return PcrsStatsPassThru(pcrsInput);
		}

		return GPOS_NEW(mp) CColRefSet(mp);
	}

	// derive statistics
	IStatistics *
	PstatsDerive(CMemoryPool *,	 //mp,
				 CExpressionHandle &exprhdl,
				 IStatisticsArray *	 //stats_ctxt
	) const override
	{
		// pass through stats from last child
		IStatistics *stats = exprhdl.Pstats(exprhdl.Arity() - 1);
		stats->AddRef();

		return stats;
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalSequence *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalSequence == pop->Eopid());

		return dynamic_cast<CLogicalSequence *>(pop);
	}

};	// class CLogicalSequence

}  // namespace gpopt


#endif	// !GPOPT_CLogicalSequence_H

// EOF
