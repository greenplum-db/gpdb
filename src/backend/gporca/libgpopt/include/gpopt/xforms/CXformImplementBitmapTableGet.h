//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformImplementBitmapTableGet
//
//	@doc:
//		Implement BitmapTableGet
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CXformImplementBitmapTableGet_H
#define GPOPT_CXformImplementBitmapTableGet_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CXformImplementBitmapTableGet
//
//	@doc:
//		Implement CLogicalBitmapTableGet as a CPhysicalBitmapTableScan
//
//---------------------------------------------------------------------------
class CXformImplementBitmapTableGet : public CXformImplementation
{
private:
public:
	CXformImplementBitmapTableGet(const CXformImplementBitmapTableGet &) =
		delete;

	// ctor
	explicit CXformImplementBitmapTableGet(CMemoryPool *mp);

	// dtor
	~CXformImplementBitmapTableGet() override = default;

	// identifier
	EXformId
	Exfid() const override
	{
		return ExfImplementBitmapTableGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementBitmapTableGet";
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

};	// class CXformImplementBitmapTableGet
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementBitmapTableGet_H

// EOF
