//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 VMware, Inc. or its affiliates.
//
#ifndef GPOPT_CXformImplementFullOuterMergeJoin_H
#define GPOPT_CXformImplementFullOuterMergeJoin_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

class CXformImplementFullOuterMergeJoin : public CXformExploration
{
private:
public:
	CXformImplementFullOuterMergeJoin(
		const CXformImplementFullOuterMergeJoin &) = delete;

	// ctor
	explicit CXformImplementFullOuterMergeJoin(CMemoryPool *mp);

	// dtor
	~CXformImplementFullOuterMergeJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementFullOuterMergeJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementFullOuterMergeJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementFullOuterMergeJoin
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementFullOuterMergeJoin_H

// EOF
