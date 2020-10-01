//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSubqueryHandlerTest.h
//
//	@doc:
//		Tests for converting subquery expressions into Apply trees
//---------------------------------------------------------------------------
#ifndef GPOPT_CSubqueryHandlerTest_H
#define GPOPT_CSubqueryHandlerTest_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/CPrintPrefix.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/COperator.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CSubqueryHandlerTest
//
//	@doc:
//		Tests for converting subquery expressions into Apply trees
//
//---------------------------------------------------------------------------
class CSubqueryHandlerTest
{
private:
	// counter used to mark last successful test
	static gpos::ULONG m_ulSubqueryHandlerMinidumpTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Subquery2Apply();
	static GPOS_RESULT EresUnittest_Subquery2OrTree();
	static GPOS_RESULT EresUnittest_Subquery2AndTree();
	static GPOS_RESULT EresUnittest_SubqueryWithConstSubqueries();
	static GPOS_RESULT EresUnittest_SubqueryWithDisjunction();
	static GPOS_RESULT EresUnittest_RunMinidumpTests();

};	// class CSubqueryHandlerTest
}  // namespace gpopt


#endif	// !GPOPT_CSubqueryHandlerTest_H

// EOF
