//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformSelect2BitmapBoolOp.h
//
//	@doc:
//		Transform select over table into a bitmap table get with bitmap bool op
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2BitmapBoolOp_H
#define GPOPT_CXformSelect2BitmapBoolOp_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CXformSelect2BitmapBoolOp
//
//	@doc:
//		Transform select over a table into a bitmap table get with bitmap bool op
//
//---------------------------------------------------------------------------
class CXformSelect2BitmapBoolOp : public CXformExploration
{
private:
public:
	CXformSelect2BitmapBoolOp(const CXformSelect2BitmapBoolOp &) = delete;

	// ctor
	explicit CXformSelect2BitmapBoolOp(CMemoryPool *mp);

	// dtor
	~CXformSelect2BitmapBoolOp() override = default;

	// identifier
	EXformId
	Exfid() const override
	{
		return ExfSelect2BitmapBoolOp;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSelect2BitmapBoolOp";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformSelect2BitmapBoolOp
}  // namespace gpopt

#endif	// !GPOPT_CXformSelect2BitmapBoolOp_H

// EOF
