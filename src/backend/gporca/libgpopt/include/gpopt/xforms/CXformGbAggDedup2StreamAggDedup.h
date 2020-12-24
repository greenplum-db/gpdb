//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformGbAggDedup2StreamAggDedup.h
//
//	@doc:
//		Transform GbAggDeduplicate to StreamAggDeduplicate
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformGbAggDedup2StreamAggDedup_H
#define GPOPT_CXformGbAggDedup2StreamAggDedup_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformGbAgg2StreamAgg.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformGbAggDedup2StreamAggDedup
//
//	@doc:
//		Transform GbAggDeduplicate to StreamAggDeduplicate
//
//---------------------------------------------------------------------------
class CXformGbAggDedup2StreamAggDedup : public CXformGbAgg2StreamAgg
{
private:
public:
	CXformGbAggDedup2StreamAggDedup(const CXformGbAggDedup2StreamAggDedup &) =
		delete;

	// ctor
	CXformGbAggDedup2StreamAggDedup(CMemoryPool *mp);

	// dtor
	~CXformGbAggDedup2StreamAggDedup() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfGbAggDedup2StreamAggDedup;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformGbAggDedup2StreamAggDedup";
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformGbAggDedup2StreamAggDedup

}  // namespace gpopt


#endif	// !GPOPT_CXformGbAggDedup2StreamAggDedup_H

// EOF
