//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformCollapseGbAgg.h
//
//	@doc:
//		Collapse two cascaded GbAgg operators into a single GbAgg
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformCollapseGbAgg_H
#define GPOPT_CXformCollapseGbAgg_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformCollapseGbAgg
//
//	@doc:
//		Collapse two cascaded GbAgg operators into a single GbAgg
//
//---------------------------------------------------------------------------
class CXformCollapseGbAgg : public CXformExploration
{
private:
public:
	CXformCollapseGbAgg(const CXformCollapseGbAgg &) = delete;

	// ctor
	explicit CXformCollapseGbAgg(CMemoryPool *mp);

	// dtor
	~CXformCollapseGbAgg() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfCollapseGbAgg;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformCollapseGbAgg";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformCollapseGbAgg

}  // namespace gpopt

#endif	// !GPOPT_CXformCollapseGbAgg_H

// EOF
