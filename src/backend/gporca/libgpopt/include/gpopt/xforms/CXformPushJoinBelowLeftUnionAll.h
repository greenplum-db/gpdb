//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformPushJoinBelowLeftUnionAll.h
//
//	@doc:
//		Push join below left union all transform
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushJoinBelowLeftUnionAll_H
#define GPOPT_CXformPushJoinBelowLeftUnionAll_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalUnionAll.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPatternMultiLeaf.h"
#include "gpopt/operators/CPatternNode.h"
#include "gpopt/operators/CPatternTree.h"
#include "gpopt/xforms/CXformPushJoinBelowUnionAll.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformPushJoinBelowLeftUnionAll
//
//	@doc:
//		Push join below left union all transform
//
//---------------------------------------------------------------------------
class CXformPushJoinBelowLeftUnionAll : public CXformPushJoinBelowUnionAll
{
private:
public:
	CXformPushJoinBelowLeftUnionAll(const CXformPushJoinBelowLeftUnionAll &) =
		delete;

	// ctor
	explicit CXformPushJoinBelowLeftUnionAll(CMemoryPool *mp)
		: CXformPushJoinBelowUnionAll(
			  // pattern
			  // We have to obtain the left tree, because we
			  // are swapping the sequence of execution of
			  // the union all and joins
			  // We have to obtain the predicate tree, because we need
			  // to copy the full tree with remapped columns from the
			  // 2nd union all inputs and onward
			  GPOS_NEW(mp) CExpression(
				  mp,
				  GPOS_NEW(mp) CPatternNode(
					  mp, CPatternNode::EmtMatchInnerOrLeftOuterJoin),

				  GPOS_NEW(mp)
					  CExpression  // outer child is a union all operation
				  (mp, GPOS_NEW(mp) CLogicalUnionAll(mp),
				   GPOS_NEW(mp)
					   CExpression(mp, GPOS_NEW(mp) CPatternMultiLeaf(mp))),
				  GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CPatternTree(mp)),  // inner child
				  GPOS_NEW(mp)
					  CExpression(mp,
								  GPOS_NEW(mp) CPatternTree(mp)))  // predicate
		  )
	{
	}

	// dtor
	~CXformPushJoinBelowLeftUnionAll() override = default;

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformPushJoinBelowLeftUnionAll";
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfPushJoinBelowLeftUnionAll;
	}

};	// class CXformPushJoinBelowLeftUnionAll

}  // namespace gpopt


#endif	// !GPOPT_CXformPushJoinBelowLeftUnionAll_H

// EOF
