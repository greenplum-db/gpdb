//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiJoin2HashJoin.h
//
//	@doc:
//		Transform left anti semi join to left anti semi hash join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiJoin2HashJoin_H
#define GPOPT_CXformLeftAntiSemiJoin2HashJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiJoin2HashJoin
//
//	@doc:
//		Transform left semi join to left anti semi hash join
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiJoin2HashJoin : public CXformImplementation
{
private:
public:
	CXformLeftAntiSemiJoin2HashJoin(const CXformLeftAntiSemiJoin2HashJoin &) =
		delete;

	// ctor
	explicit CXformLeftAntiSemiJoin2HashJoin(CMemoryPool *mp);

	// dtor
	~CXformLeftAntiSemiJoin2HashJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiJoin2HashJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiJoin2HashJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiJoin2HashJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftAntiSemiJoin2HashJoin_H

// EOF
