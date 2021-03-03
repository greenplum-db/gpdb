//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementConstTableGet.h
//
//	@doc:
//		Implement logical const table with a physical const table get
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementConstTableGet_H
#define GPOPT_CXformImplementConstTableGet_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementConstTableGet
//
//	@doc:
//		Implement const table get
//
//---------------------------------------------------------------------------
class CXformImplementConstTableGet : public CXformImplementation
{
private:
public:
	CXformImplementConstTableGet(const CXformImplementConstTableGet &) = delete;

	// ctor
	explicit CXformImplementConstTableGet(CMemoryPool *);

	// dtor
	~CXformImplementConstTableGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementConstTableGet;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementConstTableGet";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  // exprhdl
	) const override
	{
		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementConstTableGet

}  // namespace gpopt


#endif	// !GPOPT_CXformImplementConstTableGet_H

// EOF
