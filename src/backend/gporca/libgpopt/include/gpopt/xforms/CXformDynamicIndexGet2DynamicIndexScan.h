//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDynamicIndexGet2DynamicIndexScan.h
//
//	@doc:
//		Transform DynamicIndexGet to DynamicIndexScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDynamicIndexGet2DynamicIndexScan_H
#define GPOPT_CXformDynamicIndexGet2DynamicIndexScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDynamicIndexGet2DynamicIndexScan
//
//	@doc:
//		Transform DynamicIndexGet to DynamicIndexScan
//
//---------------------------------------------------------------------------
class CXformDynamicIndexGet2DynamicIndexScan : public CXformImplementation
{
private:
public:
	CXformDynamicIndexGet2DynamicIndexScan(
		const CXformDynamicIndexGet2DynamicIndexScan &) = delete;

	// ctor
	explicit CXformDynamicIndexGet2DynamicIndexScan(CMemoryPool *mp);

	// dtor
	~CXformDynamicIndexGet2DynamicIndexScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDynamicIndexGet2DynamicIndexScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformDynamicIndexGet2DynamicIndexScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformDynamicIndexGet2DynamicIndexScan

}  // namespace gpopt


#endif	// !GPOPT_CXformDynamicIndexGet2DynamicIndexScan_H

// EOF
