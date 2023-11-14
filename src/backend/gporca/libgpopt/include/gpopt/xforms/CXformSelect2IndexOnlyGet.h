//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformSelect2IndexOnlyGet.h
//
//	@doc:
//		Transform select over table into an index only get
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2IndexOnlyGet_H
#define GPOPT_CXformSelect2IndexOnlyGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CXformSelect2IndexGet.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSelect2IndexOnlyGet
//
//	@doc:
//		Transform select over a table into an index only get
//
//---------------------------------------------------------------------------
class CXformSelect2IndexOnlyGet : public CXformExploration
{
public:
	CXformSelect2IndexOnlyGet(const CXformSelect2IndexOnlyGet &) = delete;

	// ctor
	explicit CXformSelect2IndexOnlyGet(CMemoryPool *mp);

	// dtor
	~CXformSelect2IndexOnlyGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSelect2IndexOnlyGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSelect2IndexOnlyGet";
	}

	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformSelect2IndexOnlyGet

}  // namespace gpopt

#endif	// !GPOPT_CXformSelect2IndexOnlyGet_H

// EOF
