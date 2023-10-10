//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	@filename:
//		CXformIndexGet2IndexOnlyScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformIndexGet2IndexOnlyScan.h"

#include <cwchar>

#include "gpos/base.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalIndexGet.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalIndexOnlyScan.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/md/CMDIndexGPDB.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformIndexGet2IndexOnlyScan::CXformIndexGet2IndexOnlyScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformIndexGet2IndexOnlyScan::CXformIndexGet2IndexOnlyScan(CMemoryPool *mp)
	:  // pattern
	  CXformImplementation(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalIndexGet(mp),
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CPatternLeaf(mp))  // index lookup predicate
		  ))
{
}

CXform::EXformPromise
CXformIndexGet2IndexOnlyScan::Exfp(CExpressionHandle &exprhdl) const
{
	CLogicalIndexGet *popGet = CLogicalIndexGet::PopConvert(exprhdl.Pop());
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
//		CXformIndexGet2IndexOnlyScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformIndexGet2IndexOnlyScan::Transform(CXformContext *pxfctxt,
										CXformResult *pxfres,
										CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalIndexGet *pop = CLogicalIndexGet::PopConvert(pexpr->Pop());
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
