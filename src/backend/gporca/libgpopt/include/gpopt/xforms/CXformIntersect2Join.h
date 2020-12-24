//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformIntersect2Join.h
//
//	@doc:
//		Class to transform of Intersect into a Join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformIntersect2Join_H
#define GPOPT_CXformIntersect2Join_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformIntersect2Join
//
//	@doc:
//		Class to transform of Intersect into a Join
//
//---------------------------------------------------------------------------
class CXformIntersect2Join : public CXformExploration
{
private:
public:
	CXformIntersect2Join(const CXformIntersect2Join &) = delete;

	// ctor
	explicit CXformIntersect2Join(CMemoryPool *mp);

	// dtor
	~CXformIntersect2Join() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfIntersect2Join;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformIntersect2Join";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  // exprhdl
	) const override
	{
		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformIntersect2Join

}  // namespace gpopt

#endif	// !GPOPT_CXformIntersect2Join_H

// EOF
