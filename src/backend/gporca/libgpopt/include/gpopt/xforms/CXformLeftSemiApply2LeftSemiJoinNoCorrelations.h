//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftSemiApply2LeftSemiJoinNoCorrelations.h
//
//	@doc:
//		Turn LS apply into LS join when inner child has no outer references
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiApply2LeftSemiJoinNoCorrelations_H
#define GPOPT_CXformLeftSemiApply2LeftSemiJoinNoCorrelations_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftSemiApply.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiApply2LeftSemiJoinNoCorrelations
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftSemiApply2LeftSemiJoinNoCorrelations
	: public CXformApply2Join<CLogicalLeftSemiApply, CLogicalLeftSemiJoin>
{
private:
public:
	CXformLeftSemiApply2LeftSemiJoinNoCorrelations(
		const CXformLeftSemiApply2LeftSemiJoinNoCorrelations &) = delete;

	// ctor
	explicit CXformLeftSemiApply2LeftSemiJoinNoCorrelations(CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftSemiApply, CLogicalLeftSemiJoin>(mp)
	{
	}

	// dtor
	~CXformLeftSemiApply2LeftSemiJoinNoCorrelations() override = default;

	// ctor with a passed pattern
	CXformLeftSemiApply2LeftSemiJoinNoCorrelations(CMemoryPool *mp,
												   CExpression *pexprPattern)
		: CXformApply2Join<CLogicalLeftSemiApply, CLogicalLeftSemiJoin>(
			  mp, pexprPattern)
	{
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiApply2LeftSemiJoinNoCorrelations;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiApply2LeftSemiJoinNoCorrelations";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformLeftSemiApply2LeftSemiJoinNoCorrelations

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftSemiApply2LeftSemiJoinNoCorrelations_H

// EOF
