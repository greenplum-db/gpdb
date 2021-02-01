//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformGet2TableScan.h
//
//	@doc:
//		Transform Get to TableScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformGet2TableScan_H
#define GPOPT_CXformGet2TableScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformGet2TableScan
//
//	@doc:
//		Transform Get to TableScan
//
//---------------------------------------------------------------------------
class CXformGet2TableScan : public CXformImplementation
{
private:
public:
	CXformGet2TableScan(const CXformGet2TableScan &) = delete;

	// ctor
	explicit CXformGet2TableScan(CMemoryPool *);

	// dtor
	~CXformGet2TableScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfGet2TableScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformGet2TableScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformGet2TableScan

}  // namespace gpopt


#endif	// !GPOPT_CXformGet2TableScan_H

// EOF
