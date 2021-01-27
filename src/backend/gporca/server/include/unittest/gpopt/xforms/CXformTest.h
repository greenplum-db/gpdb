//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformTest.h
//
//	@doc:
//		Test for CXForm
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformTest_H
#define GPOPT_CXformTest_H

#include "gpos/base.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/operators/CExpression.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CXformTest
{
private:
	// path to metadata test file
	static const CHAR *m_szMDFilePath;

	// accessor to metadata cache
	static CMDAccessor *m_pmda;

	// generate a random join tree
	static CExpression *PexprJoinTree(CMemoryPool *mp);

	// generate random star join tree
	static CExpression *PexprStarJoinTree(CMemoryPool *mp, ULONG ulTabs);

	// application of different xforms for the given expression
	static void ApplyExprXforms(CMemoryPool *mp, IOstream &os,
								CExpression *pexpr);

public:
	// test driver
	static GPOS_RESULT EresUnittest();

	// test application of different xforms
	static GPOS_RESULT EresUnittest_ApplyXforms();

	// test application of cte-related xforms
	static GPOS_RESULT EresUnittest_ApplyXforms_CTE();

#ifdef GPOS_DEBUG
	// test name -> xform mapping
	static GPOS_RESULT EresUnittest_Mapping();
#endif	// GPOS_DEBUG

};	// class CXformTest
}  // namespace gpopt

#endif	// !GPOPT_CXformTest_H

// EOF
