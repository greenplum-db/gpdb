//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformRemoveSubqDistinct.h
//
//	@doc:
//		Transform that removes distinct clause from subquery
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformRemoveSubqDistinct_H
#define GPOPT_CXformRemoveSubqDistinct_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformSimplifySubquery.h"

namespace gpopt
{
using namespace gpos;

class CXformRemoveSubqDistinct : public CXformExploration
{
private:
public:
	CXformRemoveSubqDistinct(const CXformRemoveSubqDistinct &) = delete;

	// ctor
	explicit CXformRemoveSubqDistinct(CMemoryPool *mp);

	// dtor
	~CXformRemoveSubqDistinct() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfRemoveSubqDistinct;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformRemoveSubqDistinct";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformRemoveSubqDistinct

}  // namespace gpopt

#endif	// !GPOPT_CXformRemoveSubqDistinct_H

// EOF
