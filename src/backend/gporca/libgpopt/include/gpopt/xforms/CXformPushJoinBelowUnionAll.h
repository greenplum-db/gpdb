//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformPushJoinBelowUnionAll.h
//
//	@doc:
//		Push join below union all transform
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushJoinBelowUnionAll_H
#define GPOPT_CXformPushJoinBelowUnionAll_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformPushJoinBelowUnionAll
//
//	@doc:
//		Push join below union all transform
//
//---------------------------------------------------------------------------
class CXformPushJoinBelowUnionAll : public CXformExploration
{
private:
public:
	CXformPushJoinBelowUnionAll(const CXformPushJoinBelowUnionAll &) = delete;

	// ctor
	explicit CXformPushJoinBelowUnionAll(CExpression *pexprPattern)
		: CXformExploration(pexprPattern)
	{
	}

	// dtor
	~CXformPushJoinBelowUnionAll() override = default;

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

	// This xform matches a large expression pattern, of
	// three children, one is a multileaf, and two are trees
	// To prevent the search space from exploding, return true
	// for xform to be applied only once
	BOOL
	IsApplyOnce() override
	{
		return true;
	};
};	// class CXformPushJoinBelowUnionAll

}  // namespace gpopt


#endif	// !GPOPT_CXformPushJoinBelowUnionAll_H

// EOF
