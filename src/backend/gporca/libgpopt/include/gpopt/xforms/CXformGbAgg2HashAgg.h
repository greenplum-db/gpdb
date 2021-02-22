//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformGbAgg2HashAgg.h
//
//	@doc:
//		Transform GbAgg to HashAgg
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformGbAgg2HashAgg_H
#define GPOPT_CXformGbAgg2HashAgg_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformGbAgg2HashAgg
//
//	@doc:
//		Transform Get to TableScan
//
//---------------------------------------------------------------------------
class CXformGbAgg2HashAgg : public CXformImplementation
{
private:
protected:
	// check if the transformation is applicable
	static BOOL FApplicable(CExpression *pexpr);

public:
	CXformGbAgg2HashAgg(const CXformGbAgg2HashAgg &) = delete;

	// ctor
	CXformGbAgg2HashAgg(CMemoryPool *mp);

	// ctor
	explicit CXformGbAgg2HashAgg(CExpression *pexprPattern);

	// dtor
	~CXformGbAgg2HashAgg() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfGbAgg2HashAgg;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformGbAgg2HashAgg";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformGbAgg2HashAgg

}  // namespace gpopt


#endif	// !GPOPT_CXformGbAgg2HashAgg_H

// EOF
