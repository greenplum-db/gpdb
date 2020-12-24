//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorDXL.cpp
//
//	@doc:
//		Constant expression evaluator implementation that delegats to a DXL evaluator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/eval/CConstExprEvaluatorDXL.h"
#include "gpopt/eval/IConstDXLNodeEvaluator.h"

#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/exception.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/operators/CExpression.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpopt;
using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorDXL::CConstExprEvaluatorDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CConstExprEvaluatorDXL::CConstExprEvaluatorDXL(
	CMemoryPool *mp, CMDAccessor *md_accessor,
	IConstDXLNodeEvaluator *pconstdxleval)
	: m_pconstdxleval(pconstdxleval),
	  m_trexpr2dxl(mp, md_accessor, NULL /*pdrgpiSegments*/,
				   false /*fInitColumnFactory*/),
	  m_trdxl2expr(mp, md_accessor, false /*fInitColumnFactory*/)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorDXL::~CConstExprEvaluatorDXL
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CConstExprEvaluatorDXL::~CConstExprEvaluatorDXL() = default;

//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorDXL::PexprEval
//
//	@doc:
//		Evaluate the given expression and return the result as a new expression.
//		Caller takes ownership of returned expression
//
//---------------------------------------------------------------------------
CExpression *
CConstExprEvaluatorDXL::PexprEval(CExpression *pexpr)
{
	GPOS_ASSERT(NULL != pexpr);

	if (!CPredicateUtils::FCompareConstToConstIgnoreCast(pexpr))
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiEvalUnsupportedScalarExpr);
	}
	CDXLNode *pdxlnExpr = m_trexpr2dxl.PdxlnScalar(pexpr);
	CDXLNode *pdxlnResult = m_pconstdxleval->EvaluateExpr(pdxlnExpr);

	GPOS_ASSERT(EdxloptypeScalar ==
				pdxlnResult->GetOperator()->GetDXLOperatorType());

	CExpression *pexprResult =
		m_trdxl2expr.PexprTranslateScalar(pdxlnResult, NULL /*colref_array*/);
	pdxlnResult->Release();
	pdxlnExpr->Release();

	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorDXL::FCanEvalExpressions
//
//	@doc:
//		Returns true, since this evaluator always attempts to evaluate the expression and compute a datum
//
//---------------------------------------------------------------------------
BOOL
CConstExprEvaluatorDXL::FCanEvalExpressions()
{
	return m_pconstdxleval->FCanEvalExpressions();
}



// EOF
