//
// Created by Sanath Kumar Vobilisetty on 6/6/22.
//

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformAddLimitAfterSplitGbAgg.h
//
//	@doc:
//		Transform that adds limit in between local and global aggregate if
//		the expression has limit and distinct operators.
//---------------------------------------------------------------------------

#ifndef GPOPT_MASTER_CXFORMADDLIMITAFTERSPLITGBAGG_H
#define GPOPT_MASTER_CXFORMADDLIMITAFTERSPLITGBAGG_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"


namespace gpopt
{
using namespace gpos;

class CXformAddLimitAfterSplitGbAgg : public CXformExploration
{
private:
	static CExpression *PexprLimit(
		CMemoryPool *mp,				// memory pool
		CExpression *pexprRelational,	// relational child
		CExpression *pexprScalarStart,	// limit offset
		CExpression *pexprScalarRows,	// limit count
		COrderSpec *pos,				// ordering specification
		BOOL fGlobal,					// is it a local or global limit
		BOOL fHasCount,					// does limit specify a number of rows
		BOOL fTopLimitUnderDML);

public:
	CXformAddLimitAfterSplitGbAgg(const CXformAddLimitAfterSplitGbAgg &) =
		delete;

	// ctor
	explicit CXformAddLimitAfterSplitGbAgg(CMemoryPool *mp);

	// dtor
	~CXformAddLimitAfterSplitGbAgg() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfAddLimitAfterSplitGbAgg;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformAddLimitAfterSplitGbAgg";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformAddLimitAfterSplitGbAgg

}  // namespace gpopt

#endif	// GPOPT_MASTER_CXFORMADDLIMITAFTERSPLITGBAGG_H

// EOF