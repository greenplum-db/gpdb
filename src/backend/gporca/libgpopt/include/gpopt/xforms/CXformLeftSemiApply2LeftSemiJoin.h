//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformLeftSemiApply2LeftSemiJoin.h
//
//	@doc:
//		Turn LS apply into LS join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiApply2LeftSemiJoin_H
#define GPOPT_CXformLeftSemiApply2LeftSemiJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftSemiApply.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiApply2LeftSemiJoin
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftSemiApply2LeftSemiJoin
	: public CXformApply2Join<CLogicalLeftSemiApply, CLogicalLeftSemiJoin>
{
private:
public:
	CXformLeftSemiApply2LeftSemiJoin(const CXformLeftSemiApply2LeftSemiJoin &) =
		delete;

	// ctor
	explicit CXformLeftSemiApply2LeftSemiJoin(CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftSemiApply, CLogicalLeftSemiJoin>(
			  mp, true /*fDeepTree*/)
	{
	}

	// ctor with a passed pattern
	CXformLeftSemiApply2LeftSemiJoin(CMemoryPool *mp, CExpression *pexprPattern)
		: CXformApply2Join<CLogicalLeftSemiApply, CLogicalLeftSemiJoin>(
			  mp, pexprPattern)
	{
	}

	// dtor
	~CXformLeftSemiApply2LeftSemiJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiApply2LeftSemiJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiApply2LeftSemiJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformLeftSemiApply2LeftSemiJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftSemiApply2LeftSemiJoin_H

// EOF
