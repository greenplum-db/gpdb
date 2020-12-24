//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformSimplifyLeftOuterJoin.h
//
//	@doc:
//		Simplify Left Outer Join with constant false predicate
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSimplifyLeftOuterJoin_H
#define GPOPT_CXformSimplifyLeftOuterJoin_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSimplifyLeftOuterJoin
//
//	@doc:
//		Simplify Left Outer Join with constant false predicate
//
//---------------------------------------------------------------------------
class CXformSimplifyLeftOuterJoin : public CXformExploration
{
private:
public:
	CXformSimplifyLeftOuterJoin(const CXformSimplifyLeftOuterJoin &) = delete;

	// ctor
	explicit CXformSimplifyLeftOuterJoin(CMemoryPool *mp);

	// dtor
	~CXformSimplifyLeftOuterJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSimplifyLeftOuterJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSimplifyLeftOuterJoin";
	}

	// Compatibility function for simplifying aggregates
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return (CXform::ExfSimplifyLeftOuterJoin != exfid);
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformSimplifyLeftOuterJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformSimplifyLeftOuterJoin_H

// EOF
