//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorDefault.h
//
//	@doc:
//		Dummy implementation of the constant expression evaluator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CConstExprEvaluatorDefault_H
#define GPOPT_CConstExprEvaluatorDefault_H

#include "gpos/base.h"

#include "gpopt/eval/IConstExprEvaluator.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CConstExprEvaluatorDefault
//
//	@doc:
//		Constant expression evaluator default implementation for the case when
//		no database instance is available
//
//---------------------------------------------------------------------------
class CConstExprEvaluatorDefault : public IConstExprEvaluator
{
private:
public:
	CConstExprEvaluatorDefault(const CConstExprEvaluatorDefault &) = delete;

	// ctor
	CConstExprEvaluatorDefault() : IConstExprEvaluator()
	{
	}

	// dtor
	~CConstExprEvaluatorDefault() override;

	// Evaluate the given expression and return the result as a new expression
	CExpression *PexprEval(CExpression *pexpr) override;

	// Returns true iff the evaluator can evaluate constant expressions
	BOOL FCanEvalExpressions() override;
};
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorGPDB_H

// EOF
