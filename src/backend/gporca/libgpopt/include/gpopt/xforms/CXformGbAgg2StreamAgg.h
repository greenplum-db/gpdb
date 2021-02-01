//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformGbAgg2StreamAgg.h
//
//	@doc:
//		Transform GbAgg to StreamAgg
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformGbAgg2StreamAgg_H
#define GPOPT_CXformGbAgg2StreamAgg_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformGbAgg2StreamAgg
//
//	@doc:
//		Transform GbAgg to Stream Agg
//
//---------------------------------------------------------------------------
class CXformGbAgg2StreamAgg : public CXformImplementation
{
private:
public:
	CXformGbAgg2StreamAgg(const CXformGbAgg2StreamAgg &) = delete;

	// ctor
	CXformGbAgg2StreamAgg(CMemoryPool *mp);

	// ctor
	explicit CXformGbAgg2StreamAgg(CExpression *pexprPattern);

	// dtor
	~CXformGbAgg2StreamAgg() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfGbAgg2StreamAgg;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformGbAgg2StreamAgg";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformGbAgg2StreamAgg

}  // namespace gpopt


#endif	// !GPOPT_CXformGbAgg2StreamAgg_H

// EOF
