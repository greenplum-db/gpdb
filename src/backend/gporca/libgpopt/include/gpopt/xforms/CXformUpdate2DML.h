//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CXformUpdate2DML.h
//
//	@doc:
//		Transform Logical Update to Logical DML
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformUpdate2DML_H
#define GPOPT_CXformUpdate2DML_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformUpdate2DML
//
//	@doc:
//		Transform Logical Update to Logical DML
//
//---------------------------------------------------------------------------
class CXformUpdate2DML : public CXformExploration
{
private:
public:
	CXformUpdate2DML(const CXformUpdate2DML &) = delete;

	// ctor
	explicit CXformUpdate2DML(CMemoryPool *mp);

	// dtor
	~CXformUpdate2DML() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfUpdate2DML;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformUpdate2DML";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformUpdate2DML
}  // namespace gpopt

#endif	// !GPOPT_CXformUpdate2DML_H

// EOF
