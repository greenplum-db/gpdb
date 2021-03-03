//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftSemiJoin2InnerJoin.h
//
//	@doc:
//		Transform left semi join to inner join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiJoin2InnerJoin_H
#define GPOPT_CXformLeftSemiJoin2InnerJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiJoin2InnerJoin
//
//	@doc:
//		Transform left semi join to inner join
//
//---------------------------------------------------------------------------
class CXformLeftSemiJoin2InnerJoin : public CXformExploration
{
private:
public:
	CXformLeftSemiJoin2InnerJoin(const CXformLeftSemiJoin2InnerJoin &) = delete;

	// ctor
	explicit CXformLeftSemiJoin2InnerJoin(CMemoryPool *mp);

	// dtor
	~CXformLeftSemiJoin2InnerJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiJoin2InnerJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiJoin2InnerJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftSemiJoin2InnerJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftSemiJoin2InnerJoin_H

// EOF
