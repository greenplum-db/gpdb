//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftSemiJoin2CrossProduct.h
//
//	@doc:
//		Transform left semi join to cross product
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiJoin2CrossProduct_H
#define GPOPT_CXformLeftSemiJoin2CrossProduct_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiJoin2CrossProduct
//
//	@doc:
//		Transform left semi join to cross product
//
//---------------------------------------------------------------------------
class CXformLeftSemiJoin2CrossProduct : public CXformExploration
{
private:
public:
	CXformLeftSemiJoin2CrossProduct(const CXformLeftSemiJoin2CrossProduct &) =
		delete;

	// ctor
	explicit CXformLeftSemiJoin2CrossProduct(CMemoryPool *mp);

	// dtor
	~CXformLeftSemiJoin2CrossProduct() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiJoin2CrossProduct;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiJoin2CrossProduct";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftSemiJoin2CrossProduct

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftSemiJoin2CrossProduct_H

// EOF
