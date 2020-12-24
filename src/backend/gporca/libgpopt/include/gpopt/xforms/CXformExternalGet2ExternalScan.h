//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformExternalGet2ExternalScan.h
//
//	@doc:
//		Transform ExternalGet to ExternalScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExternalGet2ExternalScan_H
#define GPOPT_CXformExternalGet2ExternalScan_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExternalGet2ExternalScan
//
//	@doc:
//		Transform ExternalGet to ExternalScan
//
//---------------------------------------------------------------------------
class CXformExternalGet2ExternalScan : public CXformImplementation
{
private:
public:
	CXformExternalGet2ExternalScan(const CXformExternalGet2ExternalScan &) =
		delete;

	// ctor
	explicit CXformExternalGet2ExternalScan(CMemoryPool *);

	// dtor
	~CXformExternalGet2ExternalScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExternalGet2ExternalScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExternalGet2ExternalScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformExternalGet2ExternalScan

}  // namespace gpopt

#endif	// !GPOPT_CXformExternalGet2ExternalScan_H

// EOF
