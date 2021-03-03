//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CXformImplementSplit.h
//
//	@doc:
//		Transform Logical Split to Physical Split
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementSplit_H
#define GPOPT_CXformImplementSplit_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementSplit
//
//	@doc:
//		Transform Logical Split to Physical Split
//
//---------------------------------------------------------------------------
class CXformImplementSplit : public CXformImplementation
{
private:
public:
	CXformImplementSplit(const CXformImplementSplit &) = delete;

	// ctor
	explicit CXformImplementSplit(CMemoryPool *mp);

	// dtor
	~CXformImplementSplit() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementSplit;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementSplit";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementSplit
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementSplit_H

// EOF
