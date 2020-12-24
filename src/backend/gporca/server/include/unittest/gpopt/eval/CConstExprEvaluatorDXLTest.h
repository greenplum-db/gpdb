//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CConstExprEvaluatorDXLTest.h
//
//	@doc:
//		Unit tests for CConstExprEvaluatorDXL
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CConstExprEvaluatorDXLTest_H
#define GPOPT_CConstExprEvaluatorDXLTest_H

#include "gpos/base.h"

#include "gpopt/eval/IConstDXLNodeEvaluator.h"

// forward decl
namespace gpdxl
{
class CDXLNode;
}

namespace gpopt
{
using namespace gpos;

// forward decl
class CMDAccessor;

//---------------------------------------------------------------------------
//	@class:
//		CConstExprEvaluatorDXLTest
//
//	@doc:
//		Unit tests for CConstExprEvaluatorDXL
//
//---------------------------------------------------------------------------
class CConstExprEvaluatorDXLTest
{
private:
	class CDummyConstDXLNodeEvaluator : public IConstDXLNodeEvaluator
	{
	private:
		// memory pool
		CMemoryPool *m_mp;

		// metadata accessor
		CMDAccessor *m_pmda;

		// dummy value to return
		INT m_val;

	public:
		CDummyConstDXLNodeEvaluator(const CDummyConstDXLNodeEvaluator &) =
			delete;

		// ctor
		CDummyConstDXLNodeEvaluator(CMemoryPool *mp, CMDAccessor *md_accessor,
									INT val)
			: m_mp(mp), m_pmda(md_accessor), m_val(val)
		{
		}

		// dtor
		~CDummyConstDXLNodeEvaluator() override = default;

		// evaluate the given DXL node representing an expression and returns a dummy value as DXL
		gpdxl::CDXLNode *EvaluateExpr(
			const gpdxl::CDXLNode *pdxlnExpr) override;

		// can evaluate expressions
		BOOL
		FCanEvalExpressions() override
		{
			return true;
		}
	};

	// value  which the dummy constant evaluator should produce
	static const INT m_iDefaultEvalValue;

public:
	// run unittests
	static GPOS_RESULT EresUnittest();

	// test that evaluation fails for a non scalar input
	static GPOS_RESULT EresUnittest_NonScalar();

	// test that evaluation fails for a scalar with a nested subquery
	static GPOS_RESULT EresUnittest_NestedSubquery();

	// test that evaluation fails for a scalar with variables
	static GPOS_RESULT EresUnittest_ScalarContainingVariables();
};
}  // namespace gpopt

#endif	// !GPOPT_CConstExprEvaluatorDXLTest_H

// EOF
