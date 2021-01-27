//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CXformMultiExternalGet2MultiExternalScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformMultiExternalGet2MultiExternalScan.h"

#include "gpos/base.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalMultiExternalGet.h"
#include "gpopt/operators/CPhysicalMultiExternalScan.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformMultiExternalGet2MultiExternalScan::CXformMultiExternalGet2MultiExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformMultiExternalGet2MultiExternalScan::
	CXformMultiExternalGet2MultiExternalScan(CMemoryPool *mp)
	: CXformImplementation(
		  // pattern
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CLogicalMultiExternalGet(mp)))
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformMultiExternalGet2MultiExternalScan::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformMultiExternalGet2MultiExternalScan::Exfp(CExpressionHandle &	//exprhdl
) const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformMultiExternalGet2MultiExternalScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformMultiExternalGet2MultiExternalScan::Transform(CXformContext *pxfctxt,
													CXformResult *pxfres,
													CExpression *pexpr) const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalMultiExternalGet *popGet =
		CLogicalMultiExternalGet::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	// extract components for alternative
	CName *pname = GPOS_NEW(mp) CName(mp, popGet->Name());

	CColRefArray *pdrgpcrOutput = popGet->PdrgpcrOutput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);

	popGet->GetScanPartitionMdids()->AddRef();
	popGet->Ptabdesc()->AddRef();
	popGet->PdrgpdrgpcrPart()->AddRef();
	popGet->Ppartcnstr()->AddRef();
	popGet->PpartcnstrRel()->AddRef();
	pdrgpcrOutput->AddRef();

	// create alternative expression
	CExpression *pexprAlt = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CPhysicalMultiExternalScan(
				mp, popGet->GetScanPartitionMdids(), popGet->IsPartial(),
				popGet->Ptabdesc(), popGet->UlOpId(), pname, popGet->ScanId(),
				pdrgpcrOutput, popGet->PdrgpdrgpcrPart(),
				popGet->UlSecondaryScanId(), popGet->Ppartcnstr(),
				popGet->PpartcnstrRel()));

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF
