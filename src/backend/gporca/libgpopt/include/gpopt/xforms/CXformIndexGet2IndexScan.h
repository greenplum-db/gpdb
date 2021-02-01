//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformIndexGet2IndexScan.h
//
//	@doc:
//		Transform Index Get to Index Scan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformIndexGet2IndexScan_H
#define GPOPT_CXformIndexGet2IndexScan_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformIndexGet2IndexScan
//
//	@doc:
//		Transform Index Get to Index Scan
//
//---------------------------------------------------------------------------
class CXformIndexGet2IndexScan : public CXformImplementation
{
private:
public:
	CXformIndexGet2IndexScan(const CXformIndexGet2IndexScan &) = delete;

	// ctor
	explicit CXformIndexGet2IndexScan(CMemoryPool *);

	// dtor
	~CXformIndexGet2IndexScan() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfIndexGet2IndexScan;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformIndexGet2IndexScan";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &	//exprhdl
	) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformIndexGet2IndexScan

}  // namespace gpopt

#endif	// !GPOPT_CXformIndexGet2IndexScan_H

// EOF
