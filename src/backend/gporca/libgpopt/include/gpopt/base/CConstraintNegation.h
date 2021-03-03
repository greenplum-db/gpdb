//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraintNegation.h
//
//	@doc:
//		Representation of a negation constraint
//---------------------------------------------------------------------------
#ifndef GPOPT_CConstraintNegation_H
#define GPOPT_CConstraintNegation_H

#include "gpos/base.h"

#include "gpopt/base/CConstraint.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CConstraintNegation
//
//	@doc:
//		Representation of a negation constraint
//
//---------------------------------------------------------------------------
class CConstraintNegation : public CConstraint
{
private:
	// child constraint
	CConstraint *m_pcnstr;

public:
	CConstraintNegation(const CConstraintNegation &) = delete;

	// ctor
	CConstraintNegation(CMemoryPool *mp, CConstraint *pcnstr);

	// dtor
	~CConstraintNegation() override;

	// constraint type accessor
	EConstraintType
	Ect() const override
	{
		return CConstraint::EctNegation;
	}

	// child constraint
	CConstraint *
	PcnstrChild() const
	{
		return m_pcnstr;
	}

	// is this constraint a contradiction
	BOOL
	FContradiction() const override
	{
		return m_pcnstr->IsConstraintUnbounded();
	}

	// is this constraint unbounded
	BOOL
	IsConstraintUnbounded() const override
	{
		return m_pcnstr->FContradiction();
	}

	// scalar expression
	CExpression *PexprScalar(CMemoryPool *mp) override;

	// check if there is a constraint on the given column
	BOOL
	FConstraint(const CColRef *colref) const override
	{
		return m_pcnstr->FConstraint(colref);
	}

	// return a copy of the constraint with remapped columns
	CConstraint *PcnstrCopyWithRemappedColumns(CMemoryPool *mp,
											   UlongToColRefMap *colref_mapping,
											   BOOL must_exist) override;

	// return constraint on a given column
	CConstraint *Pcnstr(CMemoryPool *mp, const CColRef *colref) override;

	// return constraint on a given column set
	CConstraint *Pcnstr(CMemoryPool *mp, CColRefSet *pcrs) override;

	// return a clone of the constraint for a different column
	CConstraint *PcnstrRemapForColumn(CMemoryPool *mp,
									  CColRef *colref) const override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CConstraintNegation
}  // namespace gpopt

#endif	// !GPOPT_CConstraintNegation_H

// EOF
