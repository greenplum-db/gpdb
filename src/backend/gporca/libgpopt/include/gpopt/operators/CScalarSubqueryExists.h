//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryExists.h
//
//	@doc:
//		Scalar subquery EXISTS operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSubqueryExists_H
#define GPOPT_CScalarSubqueryExists_H

#include "gpos/base.h"

#include "gpopt/operators/CScalarSubqueryExistential.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSubqueryExists
//
//	@doc:
//		Scalar subquery EXISTS.
//
//---------------------------------------------------------------------------
class CScalarSubqueryExists : public CScalarSubqueryExistential
{
private:
public:
	CScalarSubqueryExists(const CScalarSubqueryExists &) = delete;

	// ctor
	CScalarSubqueryExists(CMemoryPool *mp) : CScalarSubqueryExistential(mp)
	{
	}

	// dtor
	~CScalarSubqueryExists() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarSubqueryExists;
	}

	// return a string for scalar subquery
	const CHAR *
	SzId() const override
	{
		return "CScalarSubqueryExists";
	}

	// conversion function
	static CScalarSubqueryExists *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSubqueryExists == pop->Eopid());

		return dynamic_cast<CScalarSubqueryExists *>(pop);
	}

};	// class CScalarSubqueryExists
}  // namespace gpopt

#endif	// !GPOPT_CScalarSubqueryExists_H

// EOF
