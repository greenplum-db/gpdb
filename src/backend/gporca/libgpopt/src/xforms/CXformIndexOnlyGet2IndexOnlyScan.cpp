//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	@filename:
//		CXformIndexOnlyGet2IndexOnlyScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformIndexOnlyGet2IndexOnlyScan.h"

#include <cwchar>

#include "gpos/base.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/hints/CHintUtils.h"
#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalIndexOnlyGet.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalIndexOnlyScan.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/md/CMDIndexGPDB.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformIndexOnlyGet2IndexOnlyScan::CXformIndexOnlyGet2IndexOnlyScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformIndexOnlyGet2IndexOnlyScan::CXformIndexOnlyGet2IndexOnlyScan(
	CMemoryPool *mp)
	:  // pattern
	  CXformImplementation(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalIndexOnlyGet(mp),
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CPatternLeaf(mp))  // index lookup predicate
		  ))
{
}

CXform::EXformPromise
CXformIndexOnlyGet2IndexOnlyScan::Exfp(CExpressionHandle &exprhdl) const
{
	CLogicalIndexOnlyGet *popGet =
		CLogicalIndexOnlyGet::PopConvert(exprhdl.Pop());
	CIndexDescriptor *pindexdesc = popGet->Pindexdesc();
	CTableDescriptor *ptabdesc = popGet->Ptabdesc();

	if (!pindexdesc->SupportsIndexOnlyScan(ptabdesc))
	{
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
//		CXformIndexOnlyGet2IndexOnlyScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformIndexOnlyGet2IndexOnlyScan::Transform(CXformContext *pxfctxt,
											CXformResult *pxfres,
											CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalIndexOnlyGet *pop = CLogicalIndexOnlyGet::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();
	CIndexDescriptor *pindexdesc = pop->Pindexdesc();
	CTableDescriptor *ptabdesc = pop->Ptabdesc();
	CColRefArray *pdrgpcrOutput = pop->PdrgpcrOutput();

	// extract components
	CExpression *pexprIndexCond = (*pexpr)[0];
	if (pexprIndexCond->DeriveHasSubquery() ||
		!CXformUtils::FCoverIndex(mp, pindexdesc, ptabdesc, pdrgpcrOutput))
	{
		return;
	}

	if (!CHintUtils::SatisfiesPlanHints(
			pop,
			COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetPlanHint()))
	{
		return;
	}

	pindexdesc->AddRef();
	ptabdesc->AddRef();

	COrderSpec *pos = pop->Pos();
	GPOS_ASSERT(nullptr != pos);
	pos->AddRef();

	// addref all children
	pexprIndexCond->AddRef();

	CExpression *pexprAlt = GPOS_NEW(mp)
		CExpression(mp,
					GPOS_NEW(mp) CPhysicalIndexOnlyScan(
						mp, pindexdesc, ptabdesc, pexpr->Pop()->UlOpId(),
						GPOS_NEW(mp) CName(mp, pop->NameAlias()), pdrgpcrOutput,
						pos, pop->ScanDirection()),
					pexprIndexCond);
	pxfres->Add(pexprAlt);
}


// EOF
