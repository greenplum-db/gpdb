//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations.h
//
//	@doc:
//		Turn LS apply into LS join when inner child has no outer references
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations_H
#define GPOPT_CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations
	: public CXformApply2Join<CLogicalLeftAntiSemiApply,
							  CLogicalLeftAntiSemiJoin>
{
private:
public:
	CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations(
		const CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations &) =
		delete;

	// ctor
	explicit CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations(
		CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftAntiSemiApply, CLogicalLeftAntiSemiJoin>(
			  mp)
	{
	}

	// dtor
	~CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations() override =
		default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations_H

// EOF
