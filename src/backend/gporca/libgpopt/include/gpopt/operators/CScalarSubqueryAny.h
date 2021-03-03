//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryAny.h
//
//	@doc:
//		Class for scalar subquery ANY operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSubqueryAny_H
#define GPOPT_CScalarSubqueryAny_H

#include "gpos/base.h"

#include "gpopt/operators/CScalarSubqueryQuantified.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSubqueryAny
//
//	@doc:
//		Scalar subquery ANY.
//		A scalar subquery ANY expression has two children: relational and scalar.
//
//---------------------------------------------------------------------------
class CScalarSubqueryAny : public CScalarSubqueryQuantified
{
private:
public:
	CScalarSubqueryAny(const CScalarSubqueryAny &) = delete;

	// ctor
	CScalarSubqueryAny(CMemoryPool *mp, IMDId *scalar_op_mdid,
					   const CWStringConst *pstrScalarOp,
					   const CColRef *colref);

	// dtor
	~CScalarSubqueryAny() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarSubqueryAny;
	}

	// return a string for scalar subquery
	const CHAR *
	SzId() const override
	{
		return "CScalarSubqueryAny";
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// conversion function
	static CScalarSubqueryAny *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSubqueryAny == pop->Eopid());

		return dynamic_cast<CScalarSubqueryAny *>(pop);
	}

};	// class CScalarSubqueryAny
}  // namespace gpopt

#endif	// !GPOPT_CScalarSubqueryAny_H

// EOF
