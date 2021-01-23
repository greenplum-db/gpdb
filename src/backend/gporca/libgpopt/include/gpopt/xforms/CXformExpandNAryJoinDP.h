//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformExpandNAryJoinDP.h
//
//	@doc:
//		Expand n-ary join into series of binary joins using dynamic
//		programming
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandNAryJoinDP_H
#define GPOPT_CXformExpandNAryJoinDP_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandNAryJoinDP
//
//	@doc:
//		Expand n-ary join into series of binary joins using dynamic
//		programming
//
//---------------------------------------------------------------------------
class CXformExpandNAryJoinDP : public CXformExploration
{
private:
public:
	CXformExpandNAryJoinDP(const CXformExpandNAryJoinDP &) = delete;

	// ctor
	explicit CXformExpandNAryJoinDP(CMemoryPool *mp);

	// dtor
	~CXformExpandNAryJoinDP() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandNAryJoinDP;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandNAryJoinDP";
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

};	// class CXformExpandNAryJoinDP

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandNAryJoinDP_H

// EOF
