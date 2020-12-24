//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformExpandNAryJoinGreedy.h
//
//	@doc:
//		Expand n-ary join into series of binary joins while minimizing
//		cardinality of intermediate results and delay cross joins to
//		the end
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandNAryJoinGreedy_H
#define GPOPT_CXformExpandNAryJoinGreedy_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandNAryJoinGreedy
//
//	@doc:
//		Expand n-ary join into series of binary joins while minimizing
//		cardinality of intermediate results and delay cross joins to
//		the end
//
//---------------------------------------------------------------------------
class CXformExpandNAryJoinGreedy : public CXformExploration
{
private:
public:
	CXformExpandNAryJoinGreedy(const CXformExpandNAryJoinGreedy &) = delete;

	// ctor
	explicit CXformExpandNAryJoinGreedy(CMemoryPool *pmp);

	// dtor
	~CXformExpandNAryJoinGreedy() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandNAryJoinGreedy;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandNAryJoinGreedy";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// do stats need to be computed before applying xform?
	BOOL
	FNeedsStats() const override
	{
		return true;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

	BOOL
	IsApplyOnce() override
	{
		return true;
	}
};	// class CXformExpandNAryJoinGreedy

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandNAryJoinGreedy_H

// EOF
