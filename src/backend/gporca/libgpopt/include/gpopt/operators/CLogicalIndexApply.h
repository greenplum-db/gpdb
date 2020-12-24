//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	Base Index Apply operator for Inner and Outer Join;
//	a variant of inner/outer apply that captures the need to implement a
//	correlated-execution strategy on the physical side, where the inner
//	side is an index scan with parameters from outer side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalIndexApply_H
#define GPOPT_CLogicalIndexApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{
class CLogicalIndexApply : public CLogicalApply
{
private:
protected:
	// columns used from Apply's outer child used by index in Apply's inner child
	CColRefArray *m_pdrgpcrOuterRefs;

	// is this an outer join?
	BOOL m_fOuterJoin;

	// a copy of the original join predicate that has been pushed down to the inner side
	CExpression *m_origJoinPred;

public:
	CLogicalIndexApply(const CLogicalIndexApply &) = delete;

	// ctor
	CLogicalIndexApply(CMemoryPool *mp, CColRefArray *pdrgpcrOuterRefs,
					   BOOL fOuterJoin, CExpression *origJoinPred);

	// ctor for patterns
	explicit CLogicalIndexApply(CMemoryPool *mp);

	// dtor
	~CLogicalIndexApply() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalIndexApply;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalIndexApply";
	}

	// outer column references accessor
	CColRefArray *
	PdrgPcrOuterRefs() const
	{
		return m_pdrgpcrOuterRefs;
	}

	// outer column references accessor
	BOOL
	FouterJoin() const
	{
		return m_fOuterJoin;
	}

	CExpression *
	OrigJoinPred()
	{
		return m_origJoinPred;
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *
	DeriveOutputColumns(CMemoryPool *mp, CExpressionHandle &exprhdl) override
	{
		GPOS_ASSERT(3 == exprhdl.Arity());

		return PcrsDeriveOutputCombineLogical(mp, exprhdl);
	}

	// derive not nullable columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *mp,
						 CExpressionHandle &exprhdl) const override
	{
		return PcrsDeriveNotNullCombineLogical(mp, exprhdl);
	}

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

	// applicable transformations
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	//-------------------------------------------------------------------------------------
	// Derived Stats
	//-------------------------------------------------------------------------------------

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &	 // exprhdl
	) const override
	{
		return CLogical::EspMedium;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// conversion function
	static CLogicalIndexApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalIndexApply == pop->Eopid());

		return dynamic_cast<CLogicalIndexApply *>(pop);
	}

};	// class CLogicalIndexApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalIndexApply_H

// EOF
