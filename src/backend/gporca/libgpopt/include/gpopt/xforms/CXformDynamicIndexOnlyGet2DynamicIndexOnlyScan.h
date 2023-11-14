//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan.h
//
//	@doc:
//		Transform DynamicIndexOnlyGet to DynamicIndexOnlyScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan_H
#define GPOPT_CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan
//
//	@doc:
//		Transform DynamicIndexOnlyGet to DynamicIndexOnlyScan
//
//---------------------------------------------------------------------------
class CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan
	: public CXformImplementation
{
private:
public:
	CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan(
		const CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan &) = delete;

	// ctor
	explicit CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan(CMemoryPool *mp);

	// dtor
	~CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDynamicIndexOnlyGet2DynamicIndexOnlyScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan

}  // namespace gpopt


#endif	// !GPOPT_CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan_H

// EOF
