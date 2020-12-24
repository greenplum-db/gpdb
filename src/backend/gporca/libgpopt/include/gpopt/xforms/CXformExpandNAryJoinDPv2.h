//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (C) 2019 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformExpandNAryJoinDPv2.h
//
//	@doc:
//		Expand n-ary join into series of binary joins using dynamic
//		programming
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandNAryJoinDPv2_H
#define GPOPT_CXformExpandNAryJoinDPv2_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandNAryJoinDPv2
//
//	@doc:
//		Expand n-ary join into series of binary joins using dynamic
//		programming
//
//---------------------------------------------------------------------------
class CXformExpandNAryJoinDPv2 : public CXformExploration
{
private:
public:
	CXformExpandNAryJoinDPv2(const CXformExpandNAryJoinDPv2 &) = delete;

	// ctor
	explicit CXformExpandNAryJoinDPv2(CMemoryPool *mp);

	// dtor
	~CXformExpandNAryJoinDPv2() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandNAryJoinDPv2;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandNAryJoinDPv2";
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

};	// class CXformExpandNAryJoinDPv2

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandNAryJoinDPv2_H

// EOF
