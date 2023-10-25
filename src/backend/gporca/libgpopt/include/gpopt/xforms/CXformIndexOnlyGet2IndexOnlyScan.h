//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	@filename:
//		CXformIndexOnlyGet2IndexOnlyScan.h
//
//	@doc:
//		Transform Index Get to Index Only Scan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformIndexOnlyGet2IndexOnlyScan_H
#define GPOPT_CXformIndexOnlyGet2IndexOnlyScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformIndexOnlyGet2IndexOnlyScan
//
//	@doc:
//		Transform Index Only Get to Index Only Scan
//
//---------------------------------------------------------------------------
class CXformIndexOnlyGet2IndexOnlyScan : public CXformImplementation
{
private:
public:
	CXformIndexOnlyGet2IndexOnlyScan(const CXformIndexOnlyGet2IndexOnlyScan &) =
		delete;

	// ctor
	explicit CXformIndexOnlyGet2IndexOnlyScan(CMemoryPool *);

	// dtor
	~CXformIndexOnlyGet2IndexOnlyScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfIndexOnlyGet2IndexOnlyScan;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformIndexOnlyGet2IndexOnlyScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &	//exprhdl
	) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformIndexOnlyGet2IndexOnlyScan

}  // namespace gpopt

#endif	// !GPOPT_CXformIndexOnlyGet2IndexOnlyScan_H

// EOF
