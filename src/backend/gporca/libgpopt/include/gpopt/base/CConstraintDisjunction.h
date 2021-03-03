//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraintDisjunction.h
//
//	@doc:
//		Representation of a disjunction constraint. A disjunction is a number
//		of ORed constraints
//---------------------------------------------------------------------------
#ifndef GPOPT_CConstraintDisjunction_H
#define GPOPT_CConstraintDisjunction_H

#include "gpos/base.h"

#include "gpopt/base/CConstraint.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CConstraintDisjunction
//
//	@doc:
//		Representation of a disjunction constraint
//
//---------------------------------------------------------------------------
class CConstraintDisjunction : public CConstraint
{
private:
	// array of constraints
	CConstraintArray *m_pdrgpcnstr;

	// mapping colref -> array of child constraints
	ColRefToConstraintArrayMap *m_phmcolconstr;

public:
	CConstraintDisjunction(const CConstraintDisjunction &) = delete;

	// ctor
	CConstraintDisjunction(CMemoryPool *mp, CConstraintArray *pdrgpcnstr);

	// dtor
	~CConstraintDisjunction() override;

	// constraint type accessor
	EConstraintType
	Ect() const override
	{
		return CConstraint::EctDisjunction;
	}

	// all constraints in disjunction
	CConstraintArray *
	Pdrgpcnstr() const
	{
		return m_pdrgpcnstr;
	}

	// is this constraint a contradiction
	BOOL FContradiction() const override;

	// return a copy of the constraint with remapped columns
	CConstraint *PcnstrCopyWithRemappedColumns(CMemoryPool *mp,
											   UlongToColRefMap *colref_mapping,
											   BOOL must_exist) override;

	// scalar expression
	CExpression *PexprScalar(CMemoryPool *mp) override;

	// check if there is a constraint on the given column
	BOOL FConstraint(const CColRef *colref) const override;

	// return constraint on a given column
	CConstraint *Pcnstr(CMemoryPool *mp, const CColRef *colref) override;

	// return constraint on a given column set
	CConstraint *Pcnstr(CMemoryPool *mp, CColRefSet *pcrs) override;

	// return a clone of the constraint for a different column
	CConstraint *PcnstrRemapForColumn(CMemoryPool *mp,
									  CColRef *colref) const override;

	// print
	IOstream &
	OsPrint(IOstream &os) const override
	{
		return PrintConjunctionDisjunction(os, m_pdrgpcnstr);
	}

};	// class CConstraintDisjunction
}  // namespace gpopt

#endif	// !GPOPT_CConstraintDisjunction_H

// EOF
