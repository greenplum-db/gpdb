//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorProxy.h
//
//	@doc:
//		Evaluator for constant expressions passed as DXL
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CConstExprEvaluator_H
#define GPDXL_CConstExprEvaluator_H

#include "gpos/base.h"

#include "gpopt/eval/IConstDXLNodeEvaluator.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/translate/CMappingColIdVar.h"
#include "gpopt/translate/CTranslatorDXLToScalar.h"

namespace gpdxl
{
class CDXLNode;

//---------------------------------------------------------------------------
//	@class:
//		CConstExprEvaluatorProxy
//
//	@doc:
//		Wrapper over GPDB's expression evaluator that takes a constant expression,
//		given as DXL, tries to evaluate it and returns the result as DXL.
//
//		The metadata cache should have been initialized by the caller before
//		creating an instance of this class and should not be released before
//		the destructor of this class.
//
//---------------------------------------------------------------------------
class CConstExprEvaluatorProxy : public gpopt::IConstDXLNodeEvaluator
{
private:
	//---------------------------------------------------------------------------
	//	@class:
	//		CEmptyMappingColIdVar
	//
	//	@doc:
	//		Dummy class to implement an empty variable mapping. Variable lookups
	//		raise exceptions.
	//
	//---------------------------------------------------------------------------
	class CEmptyMappingColIdVar : public CMappingColIdVar
	{
	public:
		explicit CEmptyMappingColIdVar(CMemoryPool *mp) : CMappingColIdVar(mp)
		{
		}

		~CEmptyMappingColIdVar() override = default;

		Var *VarFromDXLNodeScId(const CDXLScalarIdent *scalar_ident) override;
	};

	// memory pool, not owned
	CMemoryPool *m_mp;

	// empty mapping needed for the translator
	CEmptyMappingColIdVar m_emptymapcidvar;

	// pointer to metadata cache accessor
	CMDAccessor *m_md_accessor;

	// translator for the DXL input -> GPDB Expr
	CTranslatorDXLToScalar m_dxl2scalar_translator;

public:
	// ctor
	CConstExprEvaluatorProxy(CMemoryPool *mp, CMDAccessor *md_accessor)
		: m_mp(mp),
		  m_emptymapcidvar(m_mp),
		  m_md_accessor(md_accessor),
		  m_dxl2scalar_translator(m_mp, m_md_accessor, 0)
	{
	}

	// dtor
	~CConstExprEvaluatorProxy() override = default;

	// evaluate given constant expressionand return the DXL representation of the result.
	// if the expression has variables, an error is thrown.
	// caller keeps ownership of 'expr_dxlnode' and takes ownership of the returned pointer
	CDXLNode *EvaluateExpr(const CDXLNode *expr) override;

	// returns true iff the evaluator can evaluate constant expressions without subqueries
	BOOL
	FCanEvalExpressions() override
	{
		return true;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CConstExprEvaluator_H

// EOF
