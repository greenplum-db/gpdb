//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan.h"

#include "gpos/base.h"

#include "gpopt/hints/CHintUtils.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalDynamicIndexOnlyGet.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalDynamicIndexOnlyScan.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan::CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan::
	CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan(CMemoryPool *mp)
	: CXformImplementation(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalDynamicIndexOnlyGet(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp))  // index lookup predicate
			  ))
{
}

CXform::EXformPromise
CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan::Exfp(
	CExpressionHandle &exprhdl) const
{
	CLogicalDynamicIndexOnlyGet *popGet =
		CLogicalDynamicIndexOnlyGet::PopConvert(exprhdl.Pop());
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
//		CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicIndexOnlyGet2DynamicIndexOnlyScan::Transform(
	CXformContext *pxfctxt GPOS_ASSERTS_ONLY, CXformResult *pxfres GPOS_UNUSED,
	CExpression *pexpr GPOS_ASSERTS_ONLY) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CExpression *pexprIndexCond = (*pexpr)[0];

	CLogicalDynamicIndexOnlyGet *popIndexGet =
		CLogicalDynamicIndexOnlyGet::PopConvert(pexpr->Pop());
	if (!CHintUtils::SatisfiesPlanHints(
			popIndexGet,
			COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetPlanHint()))
	{
		return;
	}

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
