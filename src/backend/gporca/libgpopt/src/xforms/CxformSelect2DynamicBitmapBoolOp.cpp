//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformSelect2DynamicBitmapBoolOp.cpp
//
//	@doc:
//		Transform select over partitioned table into a dynamic bitmap table get
//		over bitmap bool op
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformSelect2DynamicBitmapBoolOp.h"

#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2DynamicBitmapBoolOp::CXformSelect2DynamicBitmapBoolOp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSelect2DynamicBitmapBoolOp::CXformSelect2DynamicBitmapBoolOp(
	CMemoryPool *mp)
	: CXformExploration(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalSelect(mp),
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalDynamicGet(mp)),	 // logical child
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))  // predicate tree
		  ))
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2DynamicBitmapBoolOp::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformSelect2DynamicBitmapBoolOp::Exfp(CExpressionHandle &) const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2DynamicBitmapBoolOp::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformSelect2DynamicBitmapBoolOp::Transform(CXformContext *pxfctxt,
											CXformResult *pxfres,
											CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalDynamicGet *popGet =
		CLogicalDynamicGet::PopConvert((*pexpr)[0]->Pop());
	// Do not run if contains foreign partitions, instead run CXformExpandDynamicGetWithForeignPartitions
	if (popGet->ContainsForeignParts())
	{
		return;
	}

	// We need to early exit when the relation contains security quals
	// because we are adding the security quals when translating from DXL to
	// Planned Statement as a filter. If we don't early exit then it may happen
	// that we generate a plan where the index condition contains non-leakproof
	// expressions. This can lead to data leak as we always want our security
	// quals to be executed first.
	if (popGet->HasSecurityQuals())
	{
		return;
	}

	CMemoryPool *mp = pxfctxt->Pmp();
	CExpression *pexprResult = CXformUtils::PexprSelect2BitmapBoolOp(mp, pexpr);

	if (nullptr != pexprResult)
	{
		// create a redundant SELECT on top of DynamicIndexGet to be able to use predicate in partition elimination
		CExpression *pexprRedundantSelect =
			CXformUtils::PexprRedundantSelectForDynamicIndex(mp, pexprResult);
		pexprResult->Release();

		pxfres->Add(pexprRedundantSelect);
	}
}

// EOF
