//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLeftSemiJoin2InnerJoinUnderGb.h
//
//	@doc:
//		Transform left semi join to inner join under a groupby
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiJoin2InnerJoinUnderGb_H
#define GPOPT_CXformLeftSemiJoin2InnerJoinUnderGb_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiJoin2InnerJoinUnderGb
//
//	@doc:
//		Transform left semi join to inner join under a groupby
//
//---------------------------------------------------------------------------
class CXformLeftSemiJoin2InnerJoinUnderGb : public CXformExploration
{
private:
public:
	CXformLeftSemiJoin2InnerJoinUnderGb(
		const CXformLeftSemiJoin2InnerJoinUnderGb &) = delete;

	// ctor
	explicit CXformLeftSemiJoin2InnerJoinUnderGb(CMemoryPool *mp);

	// dtor
	~CXformLeftSemiJoin2InnerJoinUnderGb() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiJoin2InnerJoinUnderGb;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiJoin2InnerJoinUnderGb";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftSemiJoin2InnerJoinUnderGb

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftSemiJoin2InnerJoinUnderGb_H

// EOF
