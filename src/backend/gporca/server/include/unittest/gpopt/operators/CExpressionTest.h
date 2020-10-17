//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CExpressionTest.h
//
//	@doc:
//		Test for CExpression
//---------------------------------------------------------------------------
#ifndef GPOPT_CExpressionTest_H
#define GPOPT_CExpressionTest_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/COperator.h"


namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CExpressionTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CExpressionTest
{
private:
	static CReqdPropPlan *PrppCreateRequiredProperties(CMemoryPool *mp,
													   CColRefSet *pcrs);

	static CExpression *PexprCreateGbyWithColumnFormat(
		CMemoryPool *mp, const WCHAR *wszColNameFormat);

	// helper for testing required column computation
	static GPOS_RESULT EresComputeReqdCols(const CHAR *szFilePath);

	// helper for checking cached required columns
	static GPOS_RESULT EresCheckCachedReqdCols(CMemoryPool *mp,
											   CExpression *pexpr,
											   CReqdPropPlan *prppInput);

	// helper function for the FValidPlan tests
	static void SetupPlanForFValidPlanTest(CMemoryPool *mp,
										   CExpression **ppexprGby,
										   CColRefSet **ppcrs,
										   CExpression **ppexprPlan,
										   CReqdPropPlan **pprpp);

	// return an expression with several joins
	static CExpression *PexprComplexJoinTree(CMemoryPool *mp);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_SimpleOps();
	static GPOS_RESULT EresUnittest_Union();
	static GPOS_RESULT EresUnittest_Const();
	static GPOS_RESULT EresUnittest_BitmapGet();

#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_ComparisonTypes();
#endif	// GPOS_DEBUG

	static GPOS_RESULT EresUnittest_FValidPlan();

	static GPOS_RESULT EresUnittest_FValidPlan_InvalidOrder();

	static GPOS_RESULT EresUnittest_FValidPlan_InvalidDistribution();

	static GPOS_RESULT EresUnittest_FValidPlan_InvalidRewindability();

	static GPOS_RESULT EresUnittest_FValidPlan_InvalidCTEs();

	static GPOS_RESULT EresUnittest_FValidPlanError();

	// test for required columns computation
	static GPOS_RESULT EresUnittest_ReqdCols();

	// negative test for invalid SetOp expression
	static GPOS_RESULT EresUnittest_InvalidSetOp();

};	// class CExpressionTest
}  // namespace gpopt

#endif	// !GPOPT_CExpressionTest_H

// EOF
