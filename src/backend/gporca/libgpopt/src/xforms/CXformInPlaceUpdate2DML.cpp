//---------------------------------------------------------------------------
//	@filename:
//		CXformInPlaceUpdate2DML.cpp
//
//	@doc:
//		Implementation of transform to convert InPlaceUpdate operator to
//		LogicalDML
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformInPlaceUpdate2DML.h"

#include "gpos/base.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalInPlaceUpdate.h"
#include "gpopt/operators/CLogicalPartitionSelector.h"
#include "gpopt/operators/CLogicalSplit.h"
#include "gpopt/operators/CLogicalUpdate.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CScalarProjectElement.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/md/IMDTypeInt4.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformInPlaceUpdate2DML::CXformInPlaceUpdate2DML
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformInPlaceUpdate2DML::CXformInPlaceUpdate2DML(CMemoryPool *mp)
	: CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalInPlaceUpdate(mp),
			  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))))
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformInPlaceUpdate2DML::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformInPlaceUpdate2DML::Exfp(CExpressionHandle &  // exprhdl
) const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformInPlaceUpdate2DML::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformInPlaceUpdate2DML::Transform(CXformContext *pxfctxt, CXformResult *pxfres,
								   CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CLogicalInPlaceUpdate *popUpdate =
		CLogicalInPlaceUpdate::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	// extract components for alternative
	CTableDescriptor *ptabdesc = popUpdate->Ptabdesc();
	CColRefArray *pdrgpcrInsert = popUpdate->PdrgpcrInsert();
	CColRefArray *pdrgpcrDelete = popUpdate->PdrgpcrDelete();
	CColRef *pcrCtid = popUpdate->PcrCtid();
	CColRef *pcrSegmentId = popUpdate->PcrSegmentId();

	IMDId *rel_mdid = ptabdesc->MDId();
	// child of update operator
	CExpression *pexprChild = (*pexpr)[0];
	pexprChild->AddRef();

	if (CXformUtils::FTriggersExist(CLogicalDML::EdmlInPlaceUpdate, ptabdesc,
									true /*fBefore*/))
	{
		rel_mdid->AddRef();
		pdrgpcrDelete->AddRef();
		pdrgpcrInsert->AddRef();
		pexprChild = CXformUtils::PexprRowTrigger(
			mp, pexprChild, CLogicalDML::EdmlSplitUpdate, rel_mdid,
			true /*fBefore*/, pdrgpcrDelete, pdrgpcrInsert);
	}

	COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();
	CColumnFactory *col_factory = poctxt->Pcf();

	const IMDType *pmdtype = md_accessor->PtMDType<IMDTypeInt4>();
	CColRef *pcrAction = col_factory->PcrCreate(pmdtype, default_type_modifier);

	// add assert checking that no NULL values are inserted for nullable columns
	// or no check constraints are violated
	COptimizerConfig *optimizer_config =
		COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
	CExpression *pexprAssertConstraints;
	if (optimizer_config->GetHint()->FEnforceConstraintsOnDML())
	{
		pexprAssertConstraints = CXformUtils::PexprAssertConstraints(
			mp, pexprChild, ptabdesc, pdrgpcrInsert);
	}
	else
	{
		pexprAssertConstraints = pexprChild;
	}

	CExpression *pexprProject = nullptr;
	CColRef *pcrTableOid = nullptr;
	if (ptabdesc->IsPartitioned())
	{
		// generate a partition selector
		pexprProject = CXformUtils::PexprLogicalPartitionSelector(
			mp, ptabdesc, pdrgpcrInsert, pexprAssertConstraints);
		pcrTableOid = CLogicalPartitionSelector::PopConvert(pexprProject->Pop())
						  ->PcrOid();
	}
	else
	{
		OID oidTable = CMDIdGPDB::CastMdid(rel_mdid)->Oid();
		CExpression *pexprOid = CUtils::PexprScalarConstOid(mp, oidTable);
		pexprProject =
			CUtils::PexprAddProjection(mp, pexprAssertConstraints, pexprOid);
		CExpression *pexprPrL = (*pexprProject)[1];
		pcrTableOid = CUtils::PcrFromProjElem((*pexprPrL)[0]);
	}

	GPOS_ASSERT(nullptr != pcrTableOid);

	pdrgpcrInsert->AddRef();
	ptabdesc->AddRef();
	CExpression *pexprDML = GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp)
			CLogicalDML(mp, CLogicalDML::EdmlInPlaceUpdate, ptabdesc,
						pdrgpcrInsert, GPOS_NEW(mp) CBitSet(mp), pcrAction,
						pcrTableOid, pcrCtid, pcrSegmentId, nullptr),
		pexprProject);

	// TODO:  - Aug 1, 2022; detect and handle AFTER triggers on update
	pxfres->Add(pexprDML);
}

// EOF
