//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDecorrelatorTest.h
//
//	@doc:
//		Tests for decorrelating expression trees
//---------------------------------------------------------------------------
#ifndef GPOPT_CDecorrelatorTest_H
#define GPOPT_CDecorrelatorTest_H

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
//		CDecorrelatorTest
//
//	@doc:
//		Tests for decorrelating expressions
//
//---------------------------------------------------------------------------
class CDecorrelatorTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Decorrelate();
	static GPOS_RESULT EresUnittest_DecorrelateSelect();
	static GPOS_RESULT EresUnittest_DecorrelateGbAgg();

};	// class CDecorrelatorTest
}  // namespace gpopt


#endif	// !GPOPT_CDecorrelatorTest_H

// EOF
