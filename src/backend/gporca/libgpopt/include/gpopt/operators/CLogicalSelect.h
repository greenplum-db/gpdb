//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalSelect.h
//
//	@doc:
//		Select operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalSelect_H
#define GPOS_CLogicalSelect_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalUnary.h"

namespace gpopt
{
using ExprPredToExprPredPartMap =
	CHashMap<CExpression, CExpression, CExpression::HashValue, CUtils::Equals,
			 CleanupRelease<CExpression>, CleanupRelease<CExpression>>;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalSelect
//
//	@doc:
//		Select operator
//
//---------------------------------------------------------------------------
class CLogicalSelect : public CLogicalUnary
{
private:
	ExprPredToExprPredPartMap *m_phmPexprPartPred;

	// table descriptor
	CTableDescriptor *m_ptabdesc;

public:
	CLogicalSelect(const CLogicalSelect &) = delete;

	// ctor
	explicit CLogicalSelect(CMemoryPool *mp);

	// ctor
	CLogicalSelect(CMemoryPool *mp, CTableDescriptor *ptabdesc);

	// dtor
	~CLogicalSelect() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalSelect;
	}

	const CHAR *
	SzId() const override
	{
		return "CLogicalSelect";
	}

	// return table's descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *,
									CExpressionHandle &) override;

	// dervive keys
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintFromPredicates(mp, exprhdl);
	}

	// derive table descriptor
	CTableDescriptor *
	DeriveTableDescriptor(CMemoryPool *,  // mp
						  CExpressionHandle &exprhdl) const override
	{
		return exprhdl.DeriveTableDescriptor(0);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// return true if operator can select a subset of input tuples based on some predicate,
	BOOL
	FSelectionOp() const override
	{
		return true;
	}

	// conversion function
	static CLogicalSelect *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalSelect == pop->Eopid());

		return dynamic_cast<CLogicalSelect *>(pop);
	}

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

};	// class CLogicalSelect

}  // namespace gpopt

#endif	// !GPOS_CLogicalSelect_H

// EOF
