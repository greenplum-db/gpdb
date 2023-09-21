//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformDynamicIndexGet2DynamicIndexOnlyScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformDynamicIndexGet2DynamicIndexOnlyScan.h"

#include "gpos/base.h"

#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalDynamicIndexGet.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalDynamicIndexOnlyScan.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexGet2DynamicIndexOnlyScan::CXformDynamicIndexGet2DynamicIndexOnlyScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicIndexGet2DynamicIndexOnlyScan::
	CXformDynamicIndexGet2DynamicIndexOnlyScan(CMemoryPool *mp)
	: CXformImplementation(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalDynamicIndexGet(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp))  // index lookup predicate
			  ))
{
}

CXform::EXformPromise
CXformDynamicIndexGet2DynamicIndexOnlyScan::Exfp(
	CExpressionHandle &exprhdl) const
{
	CLogicalDynamicIndexGet *popGet =
		CLogicalDynamicIndexGet::PopConvert(exprhdl.Pop());
	CIndexDescriptor *pindexdesc = popGet->Pindexdesc();
	CTableDescriptor *ptabdesc = popGet->Ptabdesc();

	if (!pindexdesc->SupportsIndexOnlyScan(ptabdesc))
	{
		// FIXME: relax btree requirement. GiST and SP-GiST indexes can support some operator classes, but Gin cannot
		return CXform::ExfpNone;
	}

	if (exprhdl.DeriveHasSubquery(0))
	{
		return CXform::ExfpNone;
	}
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexGet2DynamicIndexOnlyScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicIndexGet2DynamicIndexOnlyScan::Transform(
	CXformContext *pxfctxt GPOS_ASSERTS_ONLY, CXformResult *pxfres GPOS_UNUSED,
	CExpression *pexpr GPOS_ASSERTS_ONLY) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CExpression *pexprIndexCond = (*pexpr)[0];

	CLogicalDynamicIndexGet *popIndexGet =
		CLogicalDynamicIndexGet::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	CTableDescriptor *ptabdesc = popIndexGet->Ptabdesc();
	CIndexDescriptor *pindexdesc = popIndexGet->Pindexdesc();
	CColRefArray *pdrgpcrOutput = popIndexGet->PdrgpcrOutput();

	if (pexprIndexCond->DeriveHasSubquery() ||
		!CXformUtils::FCoverIndex(mp, pindexdesc, ptabdesc, pdrgpcrOutput))
	{
		return;
	}

	pexprIndexCond->AddRef();
	ptabdesc->AddRef();
	pindexdesc->AddRef();

	CColRef2dArray *pdrgpdrgpcrPart = popIndexGet->PdrgpdrgpcrPart();
	pdrgpdrgpcrPart->AddRef();

	COrderSpec *pos = popIndexGet->Pos();
	pos->AddRef();

	popIndexGet->GetPartitionMdids()->AddRef();
	popIndexGet->GetRootColMappingPerPart()->AddRef();

	// create alternative expression
	pxfres->Add(GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp) CPhysicalDynamicIndexOnlyScan(
			mp, pindexdesc, ptabdesc, pexpr->Pop()->UlOpId(),
			GPOS_NEW(mp) CName(mp, popIndexGet->Name()), pdrgpcrOutput,
			popIndexGet->ScanId(), pdrgpdrgpcrPart, pos,
			popIndexGet->GetPartitionMdids(),
			popIndexGet->GetRootColMappingPerPart()),
		pexprIndexCond));
}


// EOF
