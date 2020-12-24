//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorDXL.h
//
//	@doc:
//		Constant expression evaluator implementation that delegates to a DXL evaluator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CConstExprEvaluatorDXL_H
#define GPOPT_CConstExprEvaluatorDXL_H

#include "gpos/base.h"

#include "gpopt/eval/IConstExprEvaluator.h"
#include "gpopt/base/CColRef.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"

#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/md/CMDName.h"

// forward declaration
namespace gpos
{
class CMemoryPool;
}

namespace gpopt
{
class CExpression;
class CMDAccessor;
class IConstDXLNodeEvaluator;

//---------------------------------------------------------------------------
//	@class:
//		CConstExprEvaluatorDXL
//
//	@doc:
//		Constant expression evaluator implementation that delegates to a DXL evaluator
//
//---------------------------------------------------------------------------
class CConstExprEvaluatorDXL : public IConstExprEvaluator
{
private:
	// evaluates expressions represented as DXL, not owned
	IConstDXLNodeEvaluator *m_pconstdxleval;

	// translates CExpression's to DXL which can then be sent to the evaluator
	CTranslatorExprToDXL m_trexpr2dxl;

	// translates DXL coming from the evaluator back to CExpression
	CTranslatorDXLToExpr m_trdxl2expr;

public:
	CConstExprEvaluatorDXL(const CConstExprEvaluatorDXL &) = delete;

	// ctor
	CConstExprEvaluatorDXL(CMemoryPool *mp, CMDAccessor *md_accessor,
						   IConstDXLNodeEvaluator *pconstdxleval);

	// dtor
	~CConstExprEvaluatorDXL() override;

	// evaluate the given expression and return the result as a new expression
	// caller takes ownership of returned expression
	CExpression *PexprEval(CExpression *pexpr) override;

	// Returns true iff the evaluator can evaluate expressions
	BOOL FCanEvalExpressions() override;
};
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorDXL_H

// EOF
