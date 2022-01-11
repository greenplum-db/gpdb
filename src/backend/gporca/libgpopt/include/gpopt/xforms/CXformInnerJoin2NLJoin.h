//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformInnerJoin2NLJoin.h
//
//	@doc:
//		Transform inner join to inner NLJ
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerJoin2NLJoin_H
#define GPOPT_CXformInnerJoin2NLJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoin2NLJoin
//
//	@doc:
//		Transform inner join to inner NLJ
//		Deprecated in favor of CXformImplementInnerJoin.
//
//---------------------------------------------------------------------------
class CXformInnerJoin2NLJoin : public CXformImplementation
{
private:
public:
	CXformInnerJoin2NLJoin(const CXformInnerJoin2NLJoin &) = delete;

	// ctor
	explicit CXformInnerJoin2NLJoin(CMemoryPool *mp);

	// dtor
	~CXformInnerJoin2NLJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerJoin2NLJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoin2NLJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformInnerJoin2NLJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformInnerJoin2NLJoin_H

// EOF
