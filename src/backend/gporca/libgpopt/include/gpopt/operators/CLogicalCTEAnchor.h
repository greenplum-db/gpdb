//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalCTEAnchor.h
//
//	@doc:
//		Logical CTE anchor operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalCTEAnchor_H
#define GPOPT_CLogicalCTEAnchor_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalCTEAnchor
//
//	@doc:
//		CTE anchor operator
//
//---------------------------------------------------------------------------
class CLogicalCTEAnchor : public CLogical
{
private:
	// cte identifier
	ULONG m_id;

public:
	CLogicalCTEAnchor(const CLogicalCTEAnchor &) = delete;

	// ctor
	explicit CLogicalCTEAnchor(CMemoryPool *mp);

	// ctor
	CLogicalCTEAnchor(CMemoryPool *mp, ULONG id);

	// dtor
	~CLogicalCTEAnchor() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalCTEAnchor;
	}

	const CHAR *
	SzId() const override
	{
		return "CLogicalCTEAnchor";
	}

	// cte identifier
	ULONG
	Id() const
	{
		return m_id;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

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

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	// derive partition consumer info
	CPartInfo *DerivePartitionInfo(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const override;

	// compute required stats columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *pcrsInput,
			 ULONG	// child_index
	) const override
	{
		return PcrsStatsPassThru(pcrsInput);
	}

	// derive statistics
	IStatistics *
	PstatsDerive(CMemoryPool *,	 //mp,
				 CExpressionHandle &exprhdl,
				 IStatisticsArray *	 //stats_ctxt
	) const override
	{
		return PstatsPassThruOuter(exprhdl);
	}

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalCTEAnchor *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalCTEAnchor == pop->Eopid());

		return dynamic_cast<CLogicalCTEAnchor *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalCTEAnchor

}  // namespace gpopt

#endif	// !GPOPT_CLogicalCTEAnchor_H

// EOF
