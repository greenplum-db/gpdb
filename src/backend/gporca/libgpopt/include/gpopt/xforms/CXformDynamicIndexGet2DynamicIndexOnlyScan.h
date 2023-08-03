//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformDynamicIndexGet2DynamicIndexOnlyScan.h
//
//	@doc:
//		Transform DynamicIndexGet to DynamicIndexOnlyScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDynamicIndexGet2DynamicIndexOnlyScan_H
#define GPOPT_CXformDynamicIndexGet2DynamicIndexOnlyScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDynamicIndexGet2DynamicIndexOnlyScan
//
//	@doc:
//		Transform DynamicIndexGet to DynamicIndexOnlyScan
//
//---------------------------------------------------------------------------
class CXformDynamicIndexGet2DynamicIndexOnlyScan : public CXformImplementation
{
private:
public:
	CXformDynamicIndexGet2DynamicIndexOnlyScan(
		const CXformDynamicIndexGet2DynamicIndexOnlyScan &) = delete;

	// ctor
	explicit CXformDynamicIndexGet2DynamicIndexOnlyScan(CMemoryPool *mp);

	// dtor
	~CXformDynamicIndexGet2DynamicIndexOnlyScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDynamicIndexGet2DynamicIndexOnlyScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformDynamicIndexGet2DynamicIndexOnlyScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformDynamicIndexGet2DynamicIndexOnlyScan

}  // namespace gpopt


#endif	// !GPOPT_CXformDynamicIndexGet2DynamicIndexOnlyScan_H

// EOF
