//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubquery.h
//
//	@doc:
//		Class for scalar subqueries
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSubquery_H
#define GPOPT_CScalarSubquery_H

#include "gpos/base.h"

#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSubquery
//
//	@doc:
//		Scalar subquery
//
//---------------------------------------------------------------------------
class CScalarSubquery : public CScalar
{
private:
	// computed column reference
	const CColRef *m_pcr;

	// is subquery generated from existential subquery?
	BOOL m_fGeneratedByExist;

	// is subquery generated from quantified subquery?
	BOOL m_fGeneratedByQuantified;

public:
	CScalarSubquery(const CScalarSubquery &) = delete;

	// ctor
	CScalarSubquery(CMemoryPool *mp, const CColRef *colref,
					BOOL fGeneratedByExist, BOOL fGeneratedByQuantified);

	// dtor
	~CScalarSubquery() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarSubquery;
	}

	// return a string for scalar subquery
	const CHAR *
	SzId() const override
	{
		return "CScalarSubquery";
	}

	// accessor to computed column reference
	const CColRef *
	Pcr() const
	{
		return m_pcr;
	}

	// the type of the scalar expression
	IMDId *MdidType() const override;

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// return locally used columns
	CColRefSet *PcrsUsed(CMemoryPool *mp, CExpressionHandle &exprhdl) override;

	// is subquery generated from existential subquery?
	BOOL
	FGeneratedByExist() const
	{
		return m_fGeneratedByExist;
	}

	// is subquery generated from quantified subquery?
	BOOL
	FGeneratedByQuantified() const
	{
		return m_fGeneratedByQuantified;
	}

	// derive partition consumer info
	CPartInfo *PpartinfoDerive(CMemoryPool *mp,
							   CExpressionHandle &exprhdl) const override;

	// conversion function
	static CScalarSubquery *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSubquery == pop->Eopid());

		return dynamic_cast<CScalarSubquery *>(pop);
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CScalarSubquery
}  // namespace gpopt

#endif	// !GPOPT_CScalarSubquery_H

// EOF
