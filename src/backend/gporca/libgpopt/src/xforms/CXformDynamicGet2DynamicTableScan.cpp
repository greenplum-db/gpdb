//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDynamicGet2DynamicTableScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformDynamicGet2DynamicTableScan.h"

#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/operators/CPhysicalDynamicTableScan.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CPartConstraint.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicGet2DynamicTableScan::CXformDynamicGet2DynamicTableScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicGet2DynamicTableScan::CXformDynamicGet2DynamicTableScan(
	CMemoryPool *mp)
	: CXformImplementation(
		  // pattern
		  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalDynamicGet(mp)))
{
}


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicGet2DynamicTableScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicGet2DynamicTableScan::Transform(CXformContext *pxfctxt,
											 CXformResult *pxfres,
											 CExpression *pexpr) const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalDynamicGet *popGet = CLogicalDynamicGet::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	// create/extract components for alternative
	CName *pname = GPOS_NEW(mp) CName(mp, popGet->Name());

	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	ptabdesc->AddRef();

	CColRefArray *pdrgpcrOutput = popGet->PdrgpcrOutput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);

	pdrgpcrOutput->AddRef();

	CColRef2dArray *pdrgpdrgpcrPart = popGet->PdrgpdrgpcrPart();
	pdrgpdrgpcrPart->AddRef();

	// create alternative expression
	CExpression *pexprAlt = GPOS_NEW(mp)
		CExpression(mp, GPOS_NEW(mp) CPhysicalDynamicTableScan(
							mp, pname, ptabdesc, popGet->UlOpId(),
							popGet->ScanId(), pdrgpcrOutput, pdrgpdrgpcrPart));
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF
