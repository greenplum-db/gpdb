//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiJoinNotIn2HashJoinNotIn.h
//
//	@doc:
//		Transform left anti semi join to left anti semi hash join (NotIn semantics)
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiJoinNotIn2HashJoinNotIn_H
#define GPOPT_CXformLeftAntiSemiJoinNotIn2HashJoinNotIn_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiJoinNotIn2HashJoinNotIn
//
//	@doc:
//		Transform left semi join to left anti semi hash join (NotIn semantics)
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiJoinNotIn2HashJoinNotIn : public CXformImplementation
{
private:
public:
	CXformLeftAntiSemiJoinNotIn2HashJoinNotIn(
		const CXformLeftAntiSemiJoinNotIn2HashJoinNotIn &) = delete;

	// ctor
	explicit CXformLeftAntiSemiJoinNotIn2HashJoinNotIn(CMemoryPool *mp);

	// dtor
	~CXformLeftAntiSemiJoinNotIn2HashJoinNotIn() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiJoinNotIn2HashJoinNotIn;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiJoinNotIn2HashJoinNotIn";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiJoinNotIn2HashJoinNotIn

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftAntiSemiJoinNotIn2HashJoinNotIn_H

// EOF
