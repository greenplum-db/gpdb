//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
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
	// private copy ctor
	CConstExprEvaluatorDefault(const CConstExprEvaluatorDefault &);

public:
	// ctor
	CConstExprEvaluatorDefault() : IConstExprEvaluator()
	{
	}

	// dtor
	virtual ~CConstExprEvaluatorDefault();

	// Evaluate the given expression and return the result as a new expression
	virtual CExpression *PexprEval(CExpression *pexpr);

	// Returns true iff the evaluator can evaluate constant expressions
	virtual BOOL FCanEvalExpressions();
};
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorGPDB_H

// EOF
