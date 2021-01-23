//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformCTEAnchor2TrivialSelect.h
//
//	@doc:
//		Transform logical CTE anchor to select with "true" predicate
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformCTEAnchor2TrivialSelect_H
#define GPOPT_CXformCTEAnchor2TrivialSelect_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformCTEAnchor2TrivialSelect
//
//	@doc:
//		Transform logical CTE anchor to select with "true" predicate
//
//---------------------------------------------------------------------------
class CXformCTEAnchor2TrivialSelect : public CXformExploration
{
private:
public:
	CXformCTEAnchor2TrivialSelect(const CXformCTEAnchor2TrivialSelect &) =
		delete;

	// ctor
	explicit CXformCTEAnchor2TrivialSelect(CMemoryPool *mp);

	// dtor
	~CXformCTEAnchor2TrivialSelect() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfCTEAnchor2TrivialSelect;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformCTEAnchor2TrivialSelect";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformCTEAnchor2TrivialSelect
}  // namespace gpopt

#endif	// !GPOPT_CXformCTEAnchor2TrivialSelect_H

// EOF
