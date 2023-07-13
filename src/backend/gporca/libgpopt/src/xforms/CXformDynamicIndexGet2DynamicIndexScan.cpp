//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDynamicIndexGet2DynamicIndexScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformDynamicIndexGet2DynamicIndexScan.h"

#include "gpos/base.h"

#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalDynamicIndexGet.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalDynamicIndexScan.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexGet2DynamicIndexScan::CXformDynamicIndexGet2DynamicIndexScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicIndexGet2DynamicIndexScan::CXformDynamicIndexGet2DynamicIndexScan(
	CMemoryPool *mp)
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
CXformDynamicIndexGet2DynamicIndexScan::Exfp(CExpressionHandle &exprhdl) const
{
	CLogicalDynamicIndexGet *popGet =
		CLogicalDynamicIndexGet::PopConvert(exprhdl.Pop());

	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	BOOL possible_ao_table = ptabdesc->IsAORowOrColTable() ||
							 ptabdesc->RetrieveRelStorageType() ==
								 IMDRelation::ErelstorageMixedPartitioned;
	if (possible_ao_table || exprhdl.DeriveHasSubquery(0))
	{
		// GPDB does not support dynamic index scan on AO/AOCS table.
		return CXform::ExfpNone;
	}
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexGet2DynamicIndexScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicIndexGet2DynamicIndexScan::Transform(
	CXformContext *pxfctxt GPOS_ASSERTS_ONLY, CXformResult *pxfres GPOS_UNUSED,
	CExpression *pexpr GPOS_ASSERTS_ONLY) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalDynamicIndexGet *popIndexGet =
		CLogicalDynamicIndexGet::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	// create/extract components for alternative
	CName *pname = GPOS_NEW(mp) CName(mp, popIndexGet->Name());
	GPOS_ASSERT(pname != nullptr);

	// extract components
	CExpression *pexprIndexCond = (*pexpr)[0];
	pexprIndexCond->AddRef();

	CTableDescriptor *ptabdesc = popIndexGet->Ptabdesc();
	ptabdesc->AddRef();

	CIndexDescriptor *pindexdesc = popIndexGet->Pindexdesc();
	pindexdesc->AddRef();

	CColRefArray *pdrgpcrOutput = popIndexGet->PdrgpcrOutput();
	GPOS_ASSERT(nullptr != pdrgpcrOutput);
	pdrgpcrOutput->AddRef();

	CColRef2dArray *pdrgpdrgpcrPart = popIndexGet->PdrgpdrgpcrPart();
	pdrgpdrgpcrPart->AddRef();

	COrderSpec *pos = popIndexGet->Pos();
	pos->AddRef();

	popIndexGet->GetPartitionMdids()->AddRef();
	popIndexGet->GetRootColMappingPerPart()->AddRef();

	// create alternative expression
	CExpression *pexprAlt = GPOS_NEW(mp)
		CExpression(mp,
					GPOS_NEW(mp) CPhysicalDynamicIndexScan(
						mp, pindexdesc, ptabdesc, pexpr->Pop()->UlOpId(), pname,
						pdrgpcrOutput, popIndexGet->ScanId(), pdrgpdrgpcrPart,
						pos, popIndexGet->GetPartitionMdids(),
						popIndexGet->GetRootColMappingPerPart(),
						popIndexGet->ResidualPredicateSize()),
					pexprIndexCond);
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF
