//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 VMware, Inc. or its affiliates.
//
//	@filename:
//		CJoinOrderHintsPreprocessor.h
//
//	@doc:
//		Preprocessing routines of join order hints
//---------------------------------------------------------------------------

#ifndef GPOPT_CJoinOrderHintsPreprocessor_H
#define GPOPT_CJoinOrderHintsPreprocessor_H

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/hints/CHintUtils.h"
#include "gpopt/operators/CExpression.h"

namespace gpopt
{
class CJoinOrderHintsPreprocessor
{
private:
	// Return list of all the children of fromExpr that are not referenced in
	// toExpr.
	static CExpressionArray *GetUnusedChildren(CMemoryPool *mp,
											   CExpression *fromExpr,
											   CExpression *toExpr);

	// Recursively constructs CLogicalInnerJoin expressions using the children
	// of a CLogicalNAryJoin.
	static CExpression *RecursiveApplyJoinOrderHintsOnNAryJoin(
		CMemoryPool *mp, CExpression *pexpr,
		const CJoinHint::JoinPair *joinpair);

public:
	CJoinOrderHintsPreprocessor(const CJoinOrderHintsPreprocessor &) = delete;

	// main driver
	//
	// Search for and apply join order hints on an expression. The result of
	// this converts two or more children of a CLogicalNAryJoin into one or
	// more CLogicalInnerJoin(s).
	static CExpression *PexprPreprocess(
		CMemoryPool *mp, CExpression *pexpr,
		const CJoinHint::JoinPair *joinpair = nullptr);
};

}  // namespace gpopt


#endif
