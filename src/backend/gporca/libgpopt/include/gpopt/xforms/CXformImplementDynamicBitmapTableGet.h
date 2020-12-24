//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformImplementDynamicBitmapTableGet
//
//	@doc:
//		Implement DynamicBitmapTableGet
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CXformImplementDynamicBitmapTableGet_H
#define GPOPT_CXformImplementDynamicBitmapTableGet_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CXformImplementDynamicBitmapTableGet
//
//	@doc:
//		Implement CLogicalDynamicBitmapTableGet as a CPhysicalDynamicBitmapTableScan
//
//---------------------------------------------------------------------------
class CXformImplementDynamicBitmapTableGet : public CXformImplementation
{
private:
public:
	CXformImplementDynamicBitmapTableGet(
		const CXformImplementDynamicBitmapTableGet &) = delete;

	// ctor
	explicit CXformImplementDynamicBitmapTableGet(CMemoryPool *mp);

	// dtor
	~CXformImplementDynamicBitmapTableGet() override = default;

	// identifier
	EXformId
	Exfid() const override
	{
		return ExfImplementDynamicBitmapTableGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementDynamicBitmapTableGet";
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

};	// class CXformImplementDynamicBitmapTableGet
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementDynamicBitmapTableGet_H

// EOF
