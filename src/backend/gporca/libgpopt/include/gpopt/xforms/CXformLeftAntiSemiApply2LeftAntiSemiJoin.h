//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiApply2LeftAntiSemiJoin.h
//
//	@doc:
//		Turn LAS apply into LAS join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiApply2LeftAntiSemiJoin_H
#define GPOPT_CXformLeftAntiSemiApply2LeftAntiSemiJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiApply2LeftAntiSemiJoin
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiApply2LeftAntiSemiJoin
	: public CXformApply2Join<CLogicalLeftAntiSemiApply,
							  CLogicalLeftAntiSemiJoin>
{
private:
public:
	CXformLeftAntiSemiApply2LeftAntiSemiJoin(
		const CXformLeftAntiSemiApply2LeftAntiSemiJoin &) = delete;

	// ctor
	explicit CXformLeftAntiSemiApply2LeftAntiSemiJoin(CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftAntiSemiApply, CLogicalLeftAntiSemiJoin>(
			  mp, true /*fDeepTree*/)
	{
	}

	// dtor
	~CXformLeftAntiSemiApply2LeftAntiSemiJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiApply2LeftAntiSemiJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiApply2LeftAntiSemiJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiApply2LeftAntiSemiJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftAntiSemiApply2LeftAntiSemiJoin_H

// EOF
