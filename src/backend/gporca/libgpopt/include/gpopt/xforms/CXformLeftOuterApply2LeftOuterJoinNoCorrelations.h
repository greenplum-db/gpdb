//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftOuterApply2LeftOuterJoinNoCorrelations.h
//
//	@doc:
//		Turn inner Apply into Inner Join when Apply's inner child has no
//		correlations
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftOuterApply2LeftOuterJoinNoCorrelations_H
#define GPOPT_CXformLeftOuterApply2LeftOuterJoinNoCorrelations_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftOuterApply.h"
#include "gpopt/operators/CLogicalLeftOuterJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftOuterApply2LeftOuterJoinNoCorrelations
//
//	@doc:
//		Transform left outer apply into left outer join
//
//---------------------------------------------------------------------------
class CXformLeftOuterApply2LeftOuterJoinNoCorrelations
	: public CXformApply2Join<CLogicalLeftOuterApply, CLogicalLeftOuterJoin>
{
private:
public:
	CXformLeftOuterApply2LeftOuterJoinNoCorrelations(
		const CXformLeftOuterApply2LeftOuterJoinNoCorrelations &) = delete;

	// ctor
	explicit CXformLeftOuterApply2LeftOuterJoinNoCorrelations(CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftOuterApply, CLogicalLeftOuterJoin>(mp)
	{
	}

	// dtor
	~CXformLeftOuterApply2LeftOuterJoinNoCorrelations() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftOuterApply2LeftOuterJoinNoCorrelations;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftOuterApply2LeftOuterJoinNoCorrelations";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftOuterApply2LeftOuterJoinNoCorrelations

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftOuterApply2LeftOuterJoinNoCorrelations_H

// EOF
