//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryNotExists.h
//
//	@doc:
//		Scalar subquery NOT EXISTS operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSubqueryNotExists_H
#define GPOPT_CScalarSubqueryNotExists_H

#include "gpos/base.h"

#include "gpopt/operators/CScalarSubqueryExistential.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSubqueryNotExists
//
//	@doc:
//		Scalar subquery NOT EXISTS.
//
//---------------------------------------------------------------------------
class CScalarSubqueryNotExists : public CScalarSubqueryExistential
{
private:
public:
	CScalarSubqueryNotExists(const CScalarSubqueryNotExists &) = delete;

	// ctor
	CScalarSubqueryNotExists(CMemoryPool *mp) : CScalarSubqueryExistential(mp)
	{
	}

	// dtor
	~CScalarSubqueryNotExists() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarSubqueryNotExists;
	}

	// return a string for scalar subquery
	const CHAR *
	SzId() const override
	{
		return "CScalarSubqueryNotExists";
	}

	// conversion function
	static CScalarSubqueryNotExists *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSubqueryNotExists == pop->Eopid());

		return dynamic_cast<CScalarSubqueryNotExists *>(pop);
	}

};	// class CScalarSubqueryNotExists
}  // namespace gpopt

#endif	// !GPOPT_CScalarSubqueryNotExists_H

// EOF
