//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformInnerApply2InnerJoin.h
//
//	@doc:
//		Turn inner apply into inner join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerApply2InnerJoin_H
#define GPOPT_CXformInnerApply2InnerJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerApply.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerApply2InnerJoin
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformInnerApply2InnerJoin
	: public CXformApply2Join<CLogicalInnerApply, CLogicalInnerJoin>
{
private:
public:
	CXformInnerApply2InnerJoin(const CXformInnerApply2InnerJoin &) = delete;

	// ctor
	explicit CXformInnerApply2InnerJoin(CMemoryPool *mp)
		: CXformApply2Join<CLogicalInnerApply, CLogicalInnerJoin>(
			  mp, true /*fDeepTree*/)
	{
	}

	// dtor
	~CXformInnerApply2InnerJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerApply2InnerJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformInnerApply2InnerJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformInnerApply2InnerJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformInnerApply2InnerJoin_H

// EOF
