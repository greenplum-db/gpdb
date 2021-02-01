//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSubqueryUnnest.h
//
//	@doc:
//		Base class for subquery unnesting xforms
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSubqueryUnnest_H
#define GPOPT_CXformSubqueryUnnest_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSubqueryUnnest
//
//	@doc:
//		Base class for subquery unnesting xforms
//
//---------------------------------------------------------------------------
class CXformSubqueryUnnest : public CXformExploration
{
private:
protected:
	// helper for subquery unnesting
	static CExpression *PexprSubqueryUnnest(CMemoryPool *mp, CExpression *pexpr,
											BOOL fEnforceCorrelatedApply);

	// actual transform
	virtual void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
						   CExpression *pexpr,
						   BOOL fEnforceCorrelatedApply) const;

public:
	CXformSubqueryUnnest(const CXformSubqueryUnnest &) = delete;

	// ctor
	explicit CXformSubqueryUnnest(CExpression *pexprPattern)
		: CXformExploration(pexprPattern)
	{
	}

	// dtor
	~CXformSubqueryUnnest() override = default;

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

	// is transformation a subquery unnesting (Subquery To Apply) xform?
	BOOL
	FSubqueryUnnesting() const override
	{
		return true;
	}

};	// class CXformSubqueryUnnest

}  // namespace gpopt

#endif	// !GPOPT_CXformSubqueryUnnest_H

// EOF
