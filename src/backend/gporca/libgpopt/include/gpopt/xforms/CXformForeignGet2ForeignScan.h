//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformForeignGet2ForeignScan.h
//
//	@doc:
//		Transform ForeignGet to ForeignScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformForeignGet2ForeignScan_H
#define GPOPT_CXformForeignGet2ForeignScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformForeignGet2ForeignScan
//
//	@doc:
//		Transform ForeignGet to ForeignScan
//
//---------------------------------------------------------------------------
class CXformForeignGet2ForeignScan : public CXformImplementation
{
private:
public:
	CXformForeignGet2ForeignScan(const CXformForeignGet2ForeignScan &) = delete;

	// ctor
	explicit CXformForeignGet2ForeignScan(CMemoryPool *);

	// dtor
	~CXformForeignGet2ForeignScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfForeignGet2ForeignScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformForeignGet2ForeignScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformForeignGet2ForeignScan

}  // namespace gpopt

#endif	// !GPOPT_CXformForeignGet2ForeignScan_H

// EOF
