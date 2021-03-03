//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftSemiJoin2HashJoin.h
//
//	@doc:
//		Transform left semi join to left semi hash join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiJoin2HashJoin_H
#define GPOPT_CXformLeftSemiJoin2HashJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiJoin2HashJoin
//
//	@doc:
//		Transform left semi join to left semi hash join
//
//---------------------------------------------------------------------------
class CXformLeftSemiJoin2HashJoin : public CXformImplementation
{
private:
public:
	CXformLeftSemiJoin2HashJoin(const CXformLeftSemiJoin2HashJoin &) = delete;

	// ctor
	explicit CXformLeftSemiJoin2HashJoin(CMemoryPool *mp);

	// dtor
	~CXformLeftSemiJoin2HashJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiJoin2HashJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiJoin2HashJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftSemiJoin2HashJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftSemiJoin2HashJoin_H

// EOF
