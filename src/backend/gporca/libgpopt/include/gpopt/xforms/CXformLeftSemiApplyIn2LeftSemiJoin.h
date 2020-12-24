//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLeftSemiApplyIn2LeftSemiJoin.h
//
//	@doc:
//		Turn Left Semi Apply with IN semantics into Left Semi Join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiApplyIn2LeftSemiJoin_H
#define GPOPT_CXformLeftSemiApplyIn2LeftSemiJoin_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftSemiApplyIn.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPatternTree.h"
#include "gpopt/xforms/CXformLeftSemiApply2LeftSemiJoin.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiApplyIn2LeftSemiJoin
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftSemiApplyIn2LeftSemiJoin
	: public CXformLeftSemiApply2LeftSemiJoin
{
private:
public:
	CXformLeftSemiApplyIn2LeftSemiJoin(
		const CXformLeftSemiApplyIn2LeftSemiJoin &) = delete;

	// ctor
	explicit CXformLeftSemiApplyIn2LeftSemiJoin(CMemoryPool *mp)
		: CXformLeftSemiApply2LeftSemiJoin(
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
	~CXformLeftSemiApplyIn2LeftSemiJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiApplyIn2LeftSemiJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiApplyIn2LeftSemiJoin";
	}


};	// class CXformLeftSemiApplyIn2LeftSemiJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftSemiApplyIn2LeftSemiJoin_H

// EOF
