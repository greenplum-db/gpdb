//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CXformDynamicForeignGet2DynamicForeignScan.h
//
//	@doc:
//		Transform DynamicForeignGet to DynamicForeignScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDynamicForeignGet2DynamicForeignScan_H
#define GPOPT_CXformDynamicForeignGet2DynamicForeignScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDynamicForeignGet2DynamicForeignScan
//
//	@doc:
//		Transform DynamicForeignGet to DynamicForeignScan
//
//---------------------------------------------------------------------------
class CXformDynamicForeignGet2DynamicForeignScan : public CXformImplementation
{
private:
public:
	CXformDynamicForeignGet2DynamicForeignScan(
		const CXformDynamicForeignGet2DynamicForeignScan &) = delete;

	// ctor
	explicit CXformDynamicForeignGet2DynamicForeignScan(CMemoryPool *);

	// dtor
	~CXformDynamicForeignGet2DynamicForeignScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDynamicForeignGet2DynamicForeignScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformDynamicForeignGet2DynamicForeignScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformDynamicForeignGet2DynamicForeignScan

}  // namespace gpopt

#endif	// !GPOPT_CXformDynamicForeignGet2DynamicForeignScan_H

// EOF
