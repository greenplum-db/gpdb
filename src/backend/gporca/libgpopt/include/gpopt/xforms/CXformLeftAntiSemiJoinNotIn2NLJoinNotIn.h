//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiJoinNotIn2NLJoinNotIn.h
//
//	@doc:
//		Transform left anti semi join to left anti semi NLJ (NotIn semantics)
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiJoinNotIn2NLJoinNotIn_H
#define GPOPT_CXformLeftAntiSemiJoinNotIn2NLJoinNotIn_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiJoinNotIn2NLJoinNotIn
//
//	@doc:
//		Transform left anti semi join to left anti semi NLJ (NotIn semantics)
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiJoinNotIn2NLJoinNotIn : public CXformImplementation
{
private:
public:
	CXformLeftAntiSemiJoinNotIn2NLJoinNotIn(
		const CXformLeftAntiSemiJoinNotIn2NLJoinNotIn &) = delete;

	// ctor
	explicit CXformLeftAntiSemiJoinNotIn2NLJoinNotIn(CMemoryPool *mp);

	// dtor
	~CXformLeftAntiSemiJoinNotIn2NLJoinNotIn() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiJoinNotIn2NLJoinNotIn;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiJoinNotIn2NLJoinNotIn";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiJoinNotIn2NLJoinNotIn

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftAntiSemiJoinNotIn2NLJoinNotIn_H

// EOF
