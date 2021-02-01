//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations.h
//
//	@doc:
//		Turn LS apply into LS join (NotIn semantics) when inner child has no
//		outer references
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations_H
#define GPOPT_CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiApplyNotIn.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"
#include "gpopt/xforms/CXformApply2Join.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations
	: public CXformApply2Join<CLogicalLeftAntiSemiApplyNotIn,
							  CLogicalLeftAntiSemiJoinNotIn>
{
private:
public:
	CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations(
		const CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations
			&) = delete;

	// ctor
	explicit CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations(
		CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftAntiSemiApplyNotIn,
						   CLogicalLeftAntiSemiJoinNotIn>(mp)
	{
	}

	// dtor
	~CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations()
		override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations_H

// EOF
