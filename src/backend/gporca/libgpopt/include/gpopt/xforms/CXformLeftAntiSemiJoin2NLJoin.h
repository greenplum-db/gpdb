//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiJoin2NLJoin.h
//
//	@doc:
//		Transform left anti semi join to left anti semi NLJ
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiJoin2NLJoin_H
#define GPOPT_CXformLeftAntiSemiJoin2NLJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiJoin2NLJoin
//
//	@doc:
//		Transform left anti semi join to left anti semi NLJ
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiJoin2NLJoin : public CXformImplementation
{
private:
public:
	CXformLeftAntiSemiJoin2NLJoin(const CXformLeftAntiSemiJoin2NLJoin &) =
		delete;

	// ctor
	explicit CXformLeftAntiSemiJoin2NLJoin(CMemoryPool *mp);

	// dtor
	~CXformLeftAntiSemiJoin2NLJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiJoin2NLJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiJoin2NLJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiJoin2NLJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftAntiSemiJoin2NLJoin_H

// EOF
