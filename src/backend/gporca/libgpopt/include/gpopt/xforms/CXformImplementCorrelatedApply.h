//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementCorrelatedApply.h
//
//	@doc:
//		Base class for implementing correlated NLJ
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementCorrelatedApply_H
#define GPOPT_CXformImplementCorrelatedApply_H

#include "gpos/base.h"

#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalCorrelatedInLeftSemiNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedInnerNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftAntiSemiNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftOuterNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftSemiNLJoin.h"
#include "gpopt/operators/CPhysicalCorrelatedNotInLeftAntiSemiNLJoin.h"
#include "gpopt/xforms/CXformImplementation.h"
#include "gpopt/xforms/CXformUtils.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementCorrelatedApply
//
//	@doc:
//		Implement correlated Apply
//
//---------------------------------------------------------------------------
template <class TLogicalApply, class TPhysicalJoin>
class CXformImplementCorrelatedApply : public CXformImplementation
{
private:
public:
	CXformImplementCorrelatedApply(const CXformImplementCorrelatedApply &) =
		delete;

	// ctor
	explicit CXformImplementCorrelatedApply(CMemoryPool *mp)
		:  // pattern
		  CXformImplementation(GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) TLogicalApply(mp),
			  GPOS_NEW(mp)
				  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // left child
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // right child
			  GPOS_NEW(mp)
				  CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))  // predicate
			  ))
	{
	}

	// dtor
	~CXformImplementCorrelatedApply() override = default;

	EXformPromise
	Exfp(CExpressionHandle &exprhdl) const override
	{
		if (exprhdl.DeriveHasSubquery(2))
		{
			return CXform::ExfpNone;
		}
		return CXform::ExfpHigh;
	}

	// actual transform
	void
	Transform(CXformContext *pxfctxt, CXformResult *pxfres,
			  CExpression *pexpr) const override
	{
		GPOS_ASSERT(nullptr != pxfctxt);
		GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
		GPOS_ASSERT(FCheckPattern(pexpr));

		CMemoryPool *mp = pxfctxt->Pmp();

		// extract components
		CExpression *pexprLeft = (*pexpr)[0];
		CExpression *pexprRight = (*pexpr)[1];
		CExpression *pexprScalar = (*pexpr)[2];
		TLogicalApply *popApply = TLogicalApply::PopConvert(pexpr->Pop());
		CColRefArray *colref_array = popApply->PdrgPcrInner();

		colref_array->AddRef();

		// addref all children
		pexprLeft->AddRef();
		pexprRight->AddRef();
		pexprScalar->AddRef();

		// assemble physical operator
		CExpression *pexprPhysicalApply = GPOS_NEW(mp) CExpression(
			mp,
			GPOS_NEW(mp)
				TPhysicalJoin(mp, colref_array, popApply->EopidOriginSubq()),
			pexprLeft, pexprRight, pexprScalar);

		// if the correlated apply has a join predicate we can communicate
		// for hash distribution optimizations, pass that over

		CExpression *pexprPredicate = popApply->GetPexprPredicate();
		if (pexprPredicate != nullptr)
		{
			switch (pexprPhysicalApply->Pop()->Eopid())
			{
				case COperator::EopPhysicalCorrelatedInnerNLJoin:
					break;
				case COperator::EopPhysicalCorrelatedLeftOuterNLJoin:
					pexprPredicate->AddRef();
					CPhysicalCorrelatedLeftOuterNLJoin::PopConvert(
						pexprPhysicalApply->Pop())
						->SetPexprPredicate(pexprPredicate);
					break;
					/*
     case COperator::EopPhysicalCorrelatedLeftSemiNLJoin:
                    pexprPredicate->AddRef();
                    CPhysicalCorrelatedLeftSemiNLJoin::PopConvert(pexprPhysicalApply->Pop())->SetPexprPredicate(pexprPredicate);
                    break;
                case COperator::EopPhysicalCorrelatedInLeftSemiNLJoin:
                    pexprPredicate->AddRef();
                    CPhysicalCorrelatedInLeftSemiNLJoin::PopConvert(pexprPhysicalApply->Pop())->SetPexprPredicate(pexprPredicate);
                    break;
                case COperator::EopPhysicalCorrelatedLeftAntiSemiNLJoin:
                    pexprPredicate->AddRef();
                    CPhysicalCorrelatedLeftAntiSemiNLJoin::PopConvert(pexprPhysicalApply->Pop())->SetPexprPredicate(pexprPredicate);
                    break;
                case COperator::EopPhysicalCorrelatedNotInLeftAntiSemiNLJoin:
                    pexprPredicate->AddRef();
                    CPhysicalCorrelatedNotInLeftAntiSemiNLJoin::PopConvert(pexprPhysicalApply->Pop())->SetPexprPredicate(pexprPredicate);
                    break;
     */
				default:
					break;
			}
		}

		// add alternative to results
		pxfres->Add(pexprPhysicalApply);
	}

};	// class CXformImplementCorrelatedApply

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementCorrelatedApply_H

// EOF
