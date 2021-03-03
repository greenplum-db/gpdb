//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CXformInsert2DML.h
//
//	@doc:
//		Transform Logical Insert to Logical DML
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInsert2DML_H
#define GPOPT_CXformInsert2DML_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInsert2DML
//
//	@doc:
//		Transform Logical Insert to Logical DML
//
//---------------------------------------------------------------------------
class CXformInsert2DML : public CXformExploration
{
private:
public:
	CXformInsert2DML(const CXformInsert2DML &) = delete;

	// ctor
	explicit CXformInsert2DML(CMemoryPool *mp);

	// dtor
	~CXformInsert2DML() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInsert2DML;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInsert2DML";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformInsert2DML
}  // namespace gpopt

#endif	// !GPOPT_CXformInsert2DML_H

// EOF
