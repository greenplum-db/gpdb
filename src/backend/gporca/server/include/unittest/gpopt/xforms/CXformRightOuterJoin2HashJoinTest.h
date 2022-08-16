//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformRightOuterJoin2HashJoinTest.h
//
//	@doc:
//		Test for right outer join to hash join transform
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformRightOuterJoin2HashJoinTest_H
#define GPOPT_CXformRightOuterJoin2HashJoinTest_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/COperator.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CXformRightOuterJoin2HashJoinTest
//
//	@doc:
//		Tests for logical right outer transform
//
//---------------------------------------------------------------------------
class CXformRightOuterJoin2HashJoinTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Transform();

};	// class CXformRightOuterJoin2HashJoinTest
}  // namespace gpopt


#endif	// !GPOPT_CXformRightOuterJoin2HashJoinTest_H

// EOF
