//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CXformDelete2DML.h
//
//	@doc:
//		Transform Logical Delete to Logical DML
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDelete2DML_H
#define GPOPT_CXformDelete2DML_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDelete2DML
//
//	@doc:
//		Transform Logical Delete to Logical DML
//
//---------------------------------------------------------------------------
class CXformDelete2DML : public CXformExploration
{
private:
public:
	CXformDelete2DML(const CXformDelete2DML &) = delete;

	// ctor
	explicit CXformDelete2DML(CMemoryPool *mp);

	// dtor
	~CXformDelete2DML() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDelete2DML;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformDelete2DML";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformDelete2DML
}  // namespace gpopt

#endif	// !GPOPT_CXformDelete2DML_H

// EOF
