//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformIntersectAll2LeftSemiJoin.h
//
//	@doc:
//		Class to transform of CLogicalIntersectAll into a left semi join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformIntersectAll2LeftSemiJoin_H
#define GPOPT_CXformIntersectAll2LeftSemiJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformIntersectAll2LeftSemiJoin
//
//	@doc:
//		Class to transform of CLogicalIntersectAll into a left semi join
//
//---------------------------------------------------------------------------
class CXformIntersectAll2LeftSemiJoin : public CXformExploration
{
private:
public:
	CXformIntersectAll2LeftSemiJoin(const CXformIntersectAll2LeftSemiJoin &) =
		delete;

	// ctor
	explicit CXformIntersectAll2LeftSemiJoin(CMemoryPool *mp);

	// dtor
	~CXformIntersectAll2LeftSemiJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfIntersectAll2LeftSemiJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformIntersectAll2LeftSemiJoin";
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

};	// class CXformIntersectAll2LeftSemiJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformIntersectAll2LeftSemiJoin_H

// EOF
