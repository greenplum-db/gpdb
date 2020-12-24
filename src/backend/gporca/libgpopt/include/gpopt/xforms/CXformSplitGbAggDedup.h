//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformSplitGbAggDedup.h
//
//	@doc:
//		Split a dedup aggregate into a pair of local and global dedup aggregates
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSplitGbAggDedup_H
#define GPOPT_CXformSplitGbAggDedup_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformSplitGbAgg.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSplitGbAggDedup
//
//	@doc:
//		Split a dedup aggregate operator into pair of local and global aggregates
//
//---------------------------------------------------------------------------
class CXformSplitGbAggDedup : public CXformSplitGbAgg
{
private:
public:
	CXformSplitGbAggDedup(const CXformSplitGbAggDedup &) = delete;

	// ctor
	explicit CXformSplitGbAggDedup(CMemoryPool *mp);

	// dtor
	~CXformSplitGbAggDedup() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSplitGbAggDedup;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSplitGbAggDedup";
	}

	// Compatibility function for splitting aggregates
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return (CXform::ExfSplitGbAggDedup != exfid);
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformSplitGbAggDedup

}  // namespace gpopt

#endif	// !GPOPT_CXformSplitGbAggDedup_H

// EOF
