//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorForDates.h
//
//	@doc:
//		Implementation of a constant expression evaluator for dates data
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CConstExprEvaluatorForDates_H
#define GPOPT_CConstExprEvaluatorForDates_H

#include "gpos/base.h"

#include "gpopt/eval/IConstExprEvaluator.h"

namespace gpopt
{
// fwd declarations
class CExpression;

//---------------------------------------------------------------------------
//	@class:
//		CConstExprEvaluatorForDates
//
//	@doc:
//		Implementation of a constant expression evaluator for dates data.
//		It is meant to be used in Optimizer tests that have no access to
//		backend evaluator.
//
//---------------------------------------------------------------------------
class CConstExprEvaluatorForDates : public IConstExprEvaluator
{
private:
	// memory pool, not owned
	CMemoryPool *m_mp;

public:
	CConstExprEvaluatorForDates(const CConstExprEvaluatorForDates &) = delete;

	// ctor
	explicit CConstExprEvaluatorForDates(CMemoryPool *mp) : m_mp(mp)
	{
	}

	// dtor
	~CConstExprEvaluatorForDates() override = default;

	// evaluate the given expression and return the result as a new expression
	// caller takes ownership of returned expression
	CExpression *PexprEval(CExpression *pexpr) override;

	// returns true iff the evaluator can evaluate constant expressions
	BOOL
	FCanEvalExpressions() override
	{
		return true;
	}
};	// class CConstExprEvaluatorForDates
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorForDates_H

// EOF
