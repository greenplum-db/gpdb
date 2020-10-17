//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
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

	// disable copy ctor
	CConstExprEvaluatorForDates(const CConstExprEvaluatorForDates &);

public:
	// ctor
	explicit CConstExprEvaluatorForDates(CMemoryPool *mp) : m_mp(mp)
	{
	}

	// dtor
	virtual ~CConstExprEvaluatorForDates()
	{
	}

	// evaluate the given expression and return the result as a new expression
	// caller takes ownership of returned expression
	virtual CExpression *PexprEval(CExpression *pexpr);

	// returns true iff the evaluator can evaluate constant expressions
	virtual BOOL
	FCanEvalExpressions()
	{
		return true;
	}
};	// class CConstExprEvaluatorForDates
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorForDates_H

// EOF
