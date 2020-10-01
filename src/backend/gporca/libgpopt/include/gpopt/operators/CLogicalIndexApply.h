//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
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
	// private copy ctor
	CLogicalIndexApply(const CLogicalIndexApply &);

protected:
	// columns used from Apply's outer child used by index in Apply's inner child
	CColRefArray *m_pdrgpcrOuterRefs;

	// is this an outer join?
	BOOL m_fOuterJoin;

public:
	// ctor
	CLogicalIndexApply(CMemoryPool *mp, CColRefArray *pdrgpcrOuterRefs,
					   BOOL fOuterJoin);

	// ctor for patterns
	explicit CLogicalIndexApply(CMemoryPool *mp);

	// dtor
	virtual ~CLogicalIndexApply();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopLogicalIndexApply;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
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

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	virtual CColRefSet *
	DeriveOutputColumns(CMemoryPool *mp, CExpressionHandle &exprhdl)
	{
		GPOS_ASSERT(3 == exprhdl.Arity());

		return PcrsDeriveOutputCombineLogical(mp, exprhdl);
	}

	// derive not nullable columns
	virtual CColRefSet *
	DeriveNotNullColumns(CMemoryPool *mp, CExpressionHandle &exprhdl) const
	{
		return PcrsDeriveNotNullCombineLogical(mp, exprhdl);
	}

	// derive max card
	virtual CMaxCard DeriveMaxCard(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const;

	// derive constraint property
	virtual CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *mp, CExpressionHandle &exprhdl) const
	{
		return PpcDeriveConstraintFromPredicates(mp, exprhdl);
	}

	// applicable transformations
	virtual CXformSet *PxfsCandidates(CMemoryPool *mp) const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	//-------------------------------------------------------------------------------------
	// Derived Stats
	//-------------------------------------------------------------------------------------

	// derive statistics
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  IStatisticsArray *stats_ctxt) const;

	// stat promise
	virtual EStatPromise
	Esp(CExpressionHandle &	 // exprhdl
	) const
	{
		return CLogical::EspMedium;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *PopCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

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
