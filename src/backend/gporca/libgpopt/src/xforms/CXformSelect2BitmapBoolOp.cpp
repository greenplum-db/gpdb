//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformSelect2BitmapBoolOp.cpp
//
//	@doc:
//		Transform select over table into a bitmap table get over bitmap bool op
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformSelect2BitmapBoolOp.h"

#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2BitmapBoolOp::CXformSelect2BitmapBoolOp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSelect2BitmapBoolOp::CXformSelect2BitmapBoolOp(CMemoryPool *mp)
	: CXformExploration(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalSelect(mp),
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CLogicalGet(mp)),  // logical child
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))  // predicate tree
		  ))
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2BitmapBoolOp::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformSelect2BitmapBoolOp::Exfp(CExpressionHandle &	 // exprhdl
) const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2BitmapBoolOp::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformSelect2BitmapBoolOp::Transform(CXformContext *pxfctxt,
									 CXformResult *pxfres,
									 CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CExpression *pexprRelational = (*pexpr)[0];
	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprRelational->Pop());

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

	CExpression *pexprResult =
		CXformUtils::PexprSelect2BitmapBoolOp(pxfctxt->Pmp(), pexpr);

	if (nullptr != pexprResult)
	{
		pxfres->Add(pexprResult);
	}
}

// EOF
