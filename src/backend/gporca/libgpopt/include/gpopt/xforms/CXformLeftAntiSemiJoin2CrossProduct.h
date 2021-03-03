//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiJoin2CrossProduct.h
//
//	@doc:
//		Transform left anti semi join to cross product
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiJoin2CrossProduct_H
#define GPOPT_CXformLeftAntiSemiJoin2CrossProduct_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiJoin2CrossProduct
//
//	@doc:
//		Transform left anti semi join to cross product
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiJoin2CrossProduct : public CXformExploration
{
private:
public:
	CXformLeftAntiSemiJoin2CrossProduct(
		const CXformLeftAntiSemiJoin2CrossProduct &) = delete;

	// ctor
	explicit CXformLeftAntiSemiJoin2CrossProduct(CMemoryPool *mp);

	// ctor
	explicit CXformLeftAntiSemiJoin2CrossProduct(CExpression *pexprPattern);

	// dtor
	~CXformLeftAntiSemiJoin2CrossProduct() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiJoin2CrossProduct;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiJoin2CrossProduct";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiJoin2CrossProduct

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftAntiSemiJoin2CrossProduct_H

// EOF
