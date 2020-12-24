//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformGbAggDedup2HashAggDedup.h
//
//	@doc:
//		Transform GbAggDeduplicate to HashAggDeduplicate
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformGbAggDedup2HashAggDedup_H
#define GPOPT_CXformGbAggDedup2HashAggDedup_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformGbAgg2HashAgg.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformGbAggDedup2HashAggDedup
//
//	@doc:
//		Transform GbAggDeduplicate to HashAggDeduplicate
//
//---------------------------------------------------------------------------
class CXformGbAggDedup2HashAggDedup : public CXformGbAgg2HashAgg
{
private:
public:
	CXformGbAggDedup2HashAggDedup(const CXformGbAggDedup2HashAggDedup &) =
		delete;

	// ctor
	CXformGbAggDedup2HashAggDedup(CMemoryPool *mp);

	// dtor
	~CXformGbAggDedup2HashAggDedup() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfGbAggDedup2HashAggDedup;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformGbAggDedup2HashAggDedup";
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformGbAggDedup2HashAggDedup

}  // namespace gpopt


#endif	// !GPOPT_CXformGbAggDedup2HashAggDedup_H

// EOF
