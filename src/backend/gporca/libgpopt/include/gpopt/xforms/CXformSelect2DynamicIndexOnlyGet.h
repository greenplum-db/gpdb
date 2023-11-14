//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformSelect2DynamicIndexOnlyGet.h
//
//	@doc:
//		Transform select over partitioned table into a dynamic index only get
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2DynamicIndexOnlyGet_H
#define GPOPT_CXformSelect2DynamicIndexOnlyGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSelect2DynamicIndexOnlyGet
//
//	@doc:
//		Transform select over a partitioned table into a dynamic index only get
//
//---------------------------------------------------------------------------
class CXformSelect2DynamicIndexOnlyGet : public CXformExploration
{
public:
	// ctor
	explicit CXformSelect2DynamicIndexOnlyGet(CMemoryPool *mp);

	CXformSelect2DynamicIndexOnlyGet(const CXformSelect2DynamicIndexOnlyGet &) =
		delete;

	// dtor
	~CXformSelect2DynamicIndexOnlyGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSelect2DynamicIndexOnlyGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSelect2DynamicIndexOnlyGet";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformSelect2DynamicIndexOnlyGet

}  // namespace gpopt

#endif	// !GPOPT_CXformSelect2DynamicIndexOnlyGet_H

// EOF
