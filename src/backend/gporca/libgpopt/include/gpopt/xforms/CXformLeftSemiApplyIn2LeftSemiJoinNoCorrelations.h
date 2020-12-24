//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations.h
//
//	@doc:
//		Turn Left Semi Apply (with IN semantics) into Left Semi join when inner
//		child has no outer references
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations_H
#define GPOPT_CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformLeftSemiApply2LeftSemiJoinNoCorrelations.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations
	: public CXformLeftSemiApply2LeftSemiJoinNoCorrelations
{
private:
public:
	CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations(
		const CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations &) = delete;

	// ctor
	explicit CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations(CMemoryPool *mp)
		: CXformLeftSemiApply2LeftSemiJoinNoCorrelations(
			  mp, GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CLogicalLeftSemiApplyIn(mp),
					  GPOS_NEW(mp) CExpression(
						  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // left child
					  GPOS_NEW(mp) CExpression(
						  mp, GPOS_NEW(mp) CPatternTree(mp)),  // right child
					  GPOS_NEW(mp) CExpression(
						  mp, GPOS_NEW(mp) CPatternTree(mp))  // predicate
					  ))
	{
	}

	// dtor
	~CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiApplyIn2LeftSemiJoinNoCorrelations;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations";
	}


};	// class CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations_H

// EOF
