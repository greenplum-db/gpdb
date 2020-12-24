//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformSelect2DynamicBitmapBoolOp.h
//
//	@doc:
//		Transform select over partitioned table into a dynamic bitmap table get
//		with a bitmap bool op child
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2DynamicBitmapBoolOp_H
#define GPOPT_CXformSelect2DynamicBitmapBoolOp_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CXformSelect2DynamicBitmapBoolOp
//
//	@doc:
//		Transform select over partitioned table table into a dynamic bitmap
//		table get with bitmap bool op
//---------------------------------------------------------------------------
class CXformSelect2DynamicBitmapBoolOp : public CXformExploration
{
private:
public:
	CXformSelect2DynamicBitmapBoolOp(const CXformSelect2DynamicBitmapBoolOp &) =
		delete;

	// ctor
	explicit CXformSelect2DynamicBitmapBoolOp(CMemoryPool *mp);

	// dtor
	~CXformSelect2DynamicBitmapBoolOp() override = default;

	// identifier
	EXformId
	Exfid() const override
	{
		return ExfSelect2DynamicBitmapBoolOp;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSelect2DynamicBitmapBoolOp";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformSelect2DynamicBitmapBoolOp
}  // namespace gpopt

#endif	// !GPOPT_CXformSelect2DynamicBitmapBoolOp_H

// EOF
