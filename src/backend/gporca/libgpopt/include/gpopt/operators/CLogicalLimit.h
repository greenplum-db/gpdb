//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLimit.h
//
//	@doc:
//		Limit operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLimit_H
#define GPOPT_CLogicalLimit_H

#include "gpos/base.h"

#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CLogical.h"
#include "naucrates/md/IMDId.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLimit
//
//	@doc:
//		Limit operator;
//		Scalar children compute (1) offset of start row, (2) number of rows
//
//---------------------------------------------------------------------------
class CLogicalLimit : public CLogical
{
private:
	// required sort order
	COrderSpec *m_pos;

	// global limit
	BOOL m_fGlobal;

	// does limit specify a number of rows?
	BOOL m_fHasCount;

	// the limit must be kept, even if it has no offset, nor count
	BOOL m_top_limit_under_dml;

public:
	CLogicalLimit(const CLogicalLimit &) = delete;

	// ctors
	explicit CLogicalLimit(CMemoryPool *mp);
	CLogicalLimit(CMemoryPool *mp, COrderSpec *pos, BOOL fGlobal,
				  BOOL fHasCount, BOOL fTopLimitUnderDML);

	// dtor
	~CLogicalLimit() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLimit;
	}

	const CHAR *
	SzId() const override
	{
		return "CLogicalLimit";
	}

	// order spec
	COrderSpec *
	Pos() const
	{
		return m_pos;
	}

	// global limit
	BOOL
	FGlobal() const
	{
		return m_fGlobal;
	}

	// does limit specify a number of rows
	BOOL
	FHasCount() const
	{
		return m_fHasCount;
	}

	// must the limit be always kept
	BOOL
	IsTopLimitUnderDMLorCTAS() const
	{
		return m_top_limit_under_dml;
	}

	// match function
	BOOL Matches(COperator *) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// hash function
	ULONG HashValue() const override;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &exprhdl) override;

	// derive outer references
	CColRefSet *DeriveOuterReferences(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) override;

	// dervive keys
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl) const override
	{
		return PpartinfoPassThruOuter(exprhdl);
	}

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

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
	CXformSet *PxfsCandidates(CMemoryPool *) const override;

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
	static CLogicalLimit *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLimit == pop->Eopid());

		return dynamic_cast<CLogicalLimit *>(pop);
	}

};	// class CLogicalLimit

}  // namespace gpopt

#endif	// !GPOPT_CLogicalLimit_H

// EOF
