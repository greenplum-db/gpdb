//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CXformDynamicForeignGet2DynamicForeignScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformDynamicForeignGet2DynamicForeignScan.h"

#include "gpos/base.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalDynamicForeignGet.h"
#include "gpopt/operators/CPhysicalDynamicForeignScan.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicForeignGet2DynamicForeignScan::CXformDynamicForeignGet2DynamicForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicForeignGet2DynamicForeignScan::
	CXformDynamicForeignGet2DynamicForeignScan(CMemoryPool *mp)
	: CXformImplementation(
		  // pattern
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CLogicalDynamicForeignGet(mp)))
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicForeignGet2DynamicForeignScan::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformDynamicForeignGet2DynamicForeignScan::Exfp(CExpressionHandle &  //exprhdl
) const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicForeignGet2DynamicForeignScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicForeignGet2DynamicForeignScan::Transform(CXformContext *pxfctxt,
													  CXformResult *pxfres,
													  CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalDynamicForeignGet *popGet =
		CLogicalDynamicForeignGet::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	// create/extract components for alternative
	CName *pname = GPOS_NEW(mp) CName(mp, popGet->Name());

	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	ptabdesc->AddRef();

	CColRefArray *pdrgpcrOutput = popGet->PdrgpcrOutput();
	GPOS_ASSERT(nullptr != pdrgpcrOutput);

	pdrgpcrOutput->AddRef();

	CColRef2dArray *pdrgpdrgpcrPart = popGet->PdrgpdrgpcrPart();
	pdrgpdrgpcrPart->AddRef();

	popGet->GetPartitionMdids()->AddRef();
	popGet->GetRootColMappingPerPart()->AddRef();

	// create alternative expression
	CExpression *pexprAlt = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CPhysicalDynamicForeignScan(
				mp, pname, ptabdesc, popGet->UlOpId(), popGet->ScanId(),
				pdrgpcrOutput, pdrgpdrgpcrPart, popGet->GetPartitionMdids(),
				popGet->GetRootColMappingPerPart(),
				popGet->GetForeignServerOid(), popGet->IsMasterOnly()));
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF
