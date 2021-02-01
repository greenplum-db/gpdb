//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformLeftSemiJoin2NLJoin.h
//
//	@doc:
//		Transform left semi join to left semi NLJ
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiJoin2NLJoin_H
#define GPOPT_CXformLeftSemiJoin2NLJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiJoin2NLJoin
//
//	@doc:
//		Transform left semi join to left semi NLJ
//
//---------------------------------------------------------------------------
class CXformLeftSemiJoin2NLJoin : public CXformImplementation
{
private:
public:
	CXformLeftSemiJoin2NLJoin(const CXformLeftSemiJoin2NLJoin &) = delete;

	// ctor
	explicit CXformLeftSemiJoin2NLJoin(CMemoryPool *mp);

	// dtor
	~CXformLeftSemiJoin2NLJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiJoin2NLJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiJoin2NLJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftSemiJoin2NLJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftSemiJoin2NLJoin_H

// EOF
