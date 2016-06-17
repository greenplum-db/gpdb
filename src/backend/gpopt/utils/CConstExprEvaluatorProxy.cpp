//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CConstExprEvaluatorProxy.cpp
//
//	@doc:
//		Wrapper over GPDB's expression evaluator that takes a constant expression,
//		given as DXL, evaluates it and returns the result as DXL. In case the expression
//		has variables, an exception is raised
//
//	@test:
//
//---------------------------------------------------------------------------


#include "postgres.h"
#include "executor/executor.h"
#include "nodes/nodes.h"

#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/IMemoryPool.h"

#include "gpopt/utils/CConstExprEvaluatorProxy.h"

#include "gpopt/gpdbwrappers.h"
#include "gpopt/translate/CTranslatorScalarToDXL.h"

#include "naucrates/exception.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDType.h"

#include "utils/guc.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorProxy::EmptyMappingColIdVar::PvarFromDXLNodeScId
//
//	@doc:
//		Raises an exception in case someone looks up a variable
//
//---------------------------------------------------------------------------
Var *
CConstExprEvaluatorProxy::CEmptyMappingColIdVar::PvarFromDXLNodeScId
	(
	const CDXLScalarIdent */*pdxlop*/
	)
{
	elog(LOG, "Expression passed to CConstExprEvaluatorProxy contains variables. "
			"Evaluation will fail and an exception will be thrown.");
	GPOS_RAISE(gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError);
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstExprEvaluatorProxy::EvaluateExpr
//
//	@doc:
//		Evaluate 'pdxlnExpr', assumed to be a constant expression, and return the DXL representation
// 		of the result. Caller keeps ownership of 'pdxlnExpr' and takes ownership of the returned pointer.
//
//---------------------------------------------------------------------------
CDXLNode *
CConstExprEvaluatorProxy::PdxlnEvaluateExpr
	(
	const CDXLNode *pdxlnExpr
	)
{
	// Translate DXL -> GPDB Expr
	int resultNodeTag = -1;
	Expr *pexpr = m_trdxl2scalar.PexprFromDXLNodeScalar(pdxlnExpr, &m_emptymapcidvar);
	GPOS_ASSERT(NULL != pexpr);

	// Evaluate the expression
	Expr *pexprResult = gpdb::PexprEvaluate(pexpr,
						gpdb::OidExprType((Node *)pexpr),
						gpdb::IExprTypeMod((Node *)pexpr));

	if (!IsA(pexprResult, Const))
	{
		#ifdef GPOS_DEBUG
		elog(NOTICE, "Expression did not evaluate to Const, but to an expression of type %d", resultNodeTag);
		#endif
		GPOS_RAISE(gpdxl::ExmaConstExprEval, gpdxl::ExmiConstExprEvalNonConst);
	}

	Const *pconstResult = (Const *)pexprResult;
	CDXLDatum *pdxldatum = CTranslatorScalarToDXL::Pdxldatum(m_pmp, m_pmda, pconstResult);
	CDXLNode *pdxlnResult = GPOS_NEW(m_pmp) CDXLNode(m_pmp, GPOS_NEW(m_pmp) CDXLScalarConstValue(m_pmp, pdxldatum));
	gpdb::GPDBFree(pexprResult);
	gpdb::GPDBFree(pexpr);

	return pdxlnResult;
}

// EOF
