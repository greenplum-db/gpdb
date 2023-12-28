//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CXformSelect2IndexOnlyGet.cpp
//
//	@doc:
//		Implementation of select over a table to an index only get transformation
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformSelect2IndexOnlyGet.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/md/CMDIndexGPDB.h"
#include "naucrates/md/CMDRelationGPDB.h"

using namespace gpopt;
using namespace gpmd;


CXformSelect2IndexOnlyGet::CXformSelect2IndexOnlyGet(CMemoryPool *mp)
	:  // pattern
	  CXformExploration(GPOS_NEW(mp) CExpression(
		  mp, GPOS_NEW(mp) CLogicalSelect(mp),
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalGet(mp)),  // relational child
		  GPOS_NEW(mp)
			  CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))  // predicate tree
		  ))
{
}

CXform::EXformPromise
CXformSelect2IndexOnlyGet::Exfp(CExpressionHandle &exprhdl) const
{
	if (exprhdl.DeriveHasSubquery(1))
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2IndexOnlyGet::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformSelect2IndexOnlyGet::Transform(CXformContext *pxfctxt,
									 CXformResult *pxfres,
									 CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();

	// extract components
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalar = (*pexpr)[1];

	// get the indexes on this relation
	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprRelational->Pop());
	const ULONG ulIndices = popGet->Ptabdesc()->IndexCount();
	if (0 == ulIndices)
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

	// array of expressions in the scalar expression
	CExpressionArray *pdrgpexpr =
		CPredicateUtils::PdrgpexprConjuncts(mp, pexprScalar);
	GPOS_ASSERT(pdrgpexpr->Size() > 0);

	// derive the scalar and relational properties to build set of required columns
	CColRefSet *pcrsScalarExpr = pexprScalar->DeriveUsedColumns();

	// find the indexes whose included columns meet the required columns
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(popGet->Ptabdesc()->MDId());

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);
		// We consider ForwardScan here because, BackwardScan is only supported
		// in the case where we have Order by clause in the query, but this
		// xform handles scenario of a filter on top of a regular table
		CExpression *pexprIndexGet = CXformUtils::PexprBuildBtreeIndexPlan(
			mp, md_accessor, pexprRelational, pexpr->Pop()->UlOpId(), pdrgpexpr,
			pcrsScalarExpr, nullptr /*outer_refs*/, pmdindex, pmdrel,
			EForwardScan /*indexScanDirection*/, false /*indexForOrderBy*/,
			true /*indexonly*/);
		if (nullptr != pexprIndexGet)
		{
			pxfres->Add(pexprIndexGet);
		}
	}

	pdrgpexpr->Release();
}

// EOF
