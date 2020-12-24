//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformMaxOneRow2Assert.h
//
//	@doc:
//		Transform MaxOneRow into LogicalAssert
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformMaxOneRow2Assert_H
#define GPOPT_CXformMaxOneRow2Assert_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformMaxOneRow2Assert
//
//	@doc:
//		Transform MaxOneRow into LogicalAssert
//
//---------------------------------------------------------------------------
class CXformMaxOneRow2Assert : public CXformExploration
{
private:
public:
	CXformMaxOneRow2Assert(const CXformMaxOneRow2Assert &) = delete;

	// ctor
	explicit CXformMaxOneRow2Assert(CMemoryPool *mp);

	// dtor
	~CXformMaxOneRow2Assert() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfMaxOneRow2Assert;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformMaxOneRow2Assert";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformMaxOneRow2Assert
}  // namespace gpopt

#endif	// !GPOPT_CXformMaxOneRow2Assert_H

// EOF
