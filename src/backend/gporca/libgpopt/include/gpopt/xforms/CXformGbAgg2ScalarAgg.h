//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformGbAgg2ScalarAgg.h
//
//	@doc:
//		Transform GbAgg to ScalarAgg
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformGbAgg2ScalarAgg_H
#define GPOPT_CXformGbAgg2ScalarAgg_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformGbAgg2ScalarAgg
//
//	@doc:
//		Transform Get to TableScan
//
//---------------------------------------------------------------------------
class CXformGbAgg2ScalarAgg : public CXformImplementation
{
private:
public:
	CXformGbAgg2ScalarAgg(const CXformGbAgg2ScalarAgg &) = delete;

	// ctor
	CXformGbAgg2ScalarAgg(CMemoryPool *mp);

	// dtor
	~CXformGbAgg2ScalarAgg() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfGbAgg2ScalarAgg;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformGbAgg2ScalarAgg";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformGbAgg2ScalarAgg

}  // namespace gpopt


#endif	// !GPOPT_CXformGbAgg2ScalarAgg_H

// EOF
