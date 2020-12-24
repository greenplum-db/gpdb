//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	Template Class for Inner / Left Outer Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementIndexApply_H
#define GPOPT_CXformImplementIndexApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalIndexApply.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalInnerIndexNLJoin.h"
#include "gpopt/operators/CPhysicalLeftOuterIndexNLJoin.h"
#include "gpopt/operators/CPhysicalNLJoin.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

class CXformImplementIndexApply : public CXformImplementation
{
private:
public:
	CXformImplementIndexApply(const CXformImplementIndexApply &) = delete;

	// ctor
	explicit CXformImplementIndexApply(CMemoryPool *mp)
		:  // pattern
		  CXformImplementation(GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalIndexApply(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // outer child
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // inner child
			  GPOS_NEW(mp)
				  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))  // predicate
			  ))
	{
	}

	// dtor
	~CXformImplementIndexApply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementIndexApply;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementIndexApply";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  // exprhdl
	) const override
	{
		return ExfpHigh;
	}

	// actual transform
	void
	Transform(CXformContext *pxfctxt, CXformResult *pxfres,
			  CExpression *pexpr) const override
	{
		GPOS_ASSERT(NULL != pxfctxt);
		GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
		GPOS_ASSERT(FCheckPattern(pexpr));

		CMemoryPool *mp = pxfctxt->Pmp();
		CLogicalIndexApply *indexApply =
			CLogicalIndexApply::PopConvert(pexpr->Pop());

		// extract components
		CExpression *pexprOuter = (*pexpr)[0];
		CExpression *pexprInner = (*pexpr)[1];
		CExpression *pexprScalar = (*pexpr)[2];
		CColRefArray *colref_array = indexApply->PdrgPcrOuterRefs();
		colref_array->AddRef();

		// addref all components
		pexprOuter->AddRef();
		pexprInner->AddRef();
		pexprScalar->AddRef();

		// assemble physical operator
		CPhysicalNLJoin *pop = NULL;

		if (CLogicalIndexApply::PopConvert(pexpr->Pop())->FouterJoin())
			pop = GPOS_NEW(mp) CPhysicalLeftOuterIndexNLJoin(
				mp, colref_array, indexApply->OrigJoinPred());
		else
			pop = GPOS_NEW(mp) CPhysicalInnerIndexNLJoin(
				mp, colref_array, indexApply->OrigJoinPred());

		CExpression *pexprResult = GPOS_NEW(mp)
			CExpression(mp, pop, pexprOuter, pexprInner, pexprScalar);

		// add alternative to results
		pxfres->Add(pexprResult);
	}

};	// class CXformImplementIndexApply

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementIndexApply_H

// EOF
