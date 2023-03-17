//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDynamicGet2DynamicTableScan.h
//
//	@doc:
//		Transform DynamicGet to DynamicTableScan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDynamicGet2DynamicTableScan_H
#define GPOPT_CXformDynamicGet2DynamicTableScan_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDynamicGet2DynamicTableScan
//
//	@doc:
//		Transform DynamicGet to DynamicTableScan
//
//---------------------------------------------------------------------------
class CXformDynamicGet2DynamicTableScan : public CXformImplementation
{
private:
public:
	CXformDynamicGet2DynamicTableScan(
		const CXformDynamicGet2DynamicTableScan &) = delete;

	// ctor
	explicit CXformDynamicGet2DynamicTableScan(CMemoryPool *mp);

	// dtor
	~CXformDynamicGet2DynamicTableScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDynamicGet2DynamicTableScan;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformDynamicGet2DynamicTableScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformDynamicGet2DynamicTableScan

}  // namespace gpopt


#endif	// !GPOPT_CXformDynamicGet2DynamicTableScan_H

// EOF
