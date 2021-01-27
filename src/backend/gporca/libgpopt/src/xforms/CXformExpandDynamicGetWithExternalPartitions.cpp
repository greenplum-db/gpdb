//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformExpandDynamicGetWithExternalPartitions.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformExpandDynamicGetWithExternalPartitions.h"

#include "gpos/base.h"

#include "gpopt/exception.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalMultiExternalGet.h"
#include "gpopt/operators/CLogicalUnionAll.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformExpandDynamicGetWithExternalPartitions::CXformExpandDynamicGetWithExternalPartitions
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformExpandDynamicGetWithExternalPartitions::
	CXformExpandDynamicGetWithExternalPartitions(CMemoryPool *mp)
	: CXformExploration(
		  // pattern
		  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CLogicalDynamicGet(mp)))
{
}


// Converts a CLogicalDynamicGet on a partitioned table containing external partitions
// into a UNION ALL over two partial scans:
// - Partial CLogicalDynamicGet with part constraints of non-external partitions
// - Partial CLogicalMultiExternalGet with part constraints for all external partitions
void
CXformExpandDynamicGetWithExternalPartitions::Transform(
	CXformContext *pxfctxt, CXformResult *pxfres, CExpression *pexpr) const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMDAccessor *mda = COptCtxt::PoctxtFromTLS()->Pmda();

	CLogicalDynamicGet *popGet = CLogicalDynamicGet::PopConvert(pexpr->Pop());
	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	const IMDRelation *relation = mda->RetrieveRel(ptabdesc->MDId());

	if (!relation->HasExternalPartitions() || popGet->IsPartial())
	{
		// no external partitions or already a partial dynamic get;
		// do not try to split further
		return;
	}

	CMemoryPool *mp = pxfctxt->Pmp();

	// Iterate over all the External Scans to determine partial scan contraints
	CColRef2dArray *pdrgpdrgpcrPartKeys = popGet->PdrgpdrgpcrPart();

	// capture constraints of all external and remaining (non-external) scans in
	// ppartcnstrCovered & ppartcnstrRest respectively
	CPartConstraint *ppartcnstrCovered = NULL;
	CPartConstraint *ppartcnstrRest = NULL;

	IMdIdArray *external_part_mdids = relation->GetExternalPartitions();
	GPOS_ASSERT(external_part_mdids->Size() > 0);
	for (ULONG ul = 0; ul < external_part_mdids->Size(); ul++)
	{
		IMDId *extpart_mdid = (*external_part_mdids)[ul];
		const IMDRelation *extpart = mda->RetrieveRel(extpart_mdid);
		GPOS_ASSERT(NULL != extpart->MDPartConstraint());

		CPartConstraint *ppartcnstr = CUtils::PpartcnstrFromMDPartCnstr(
			mp, mda, pdrgpdrgpcrPartKeys, extpart->MDPartConstraint(),
			popGet->PdrgpcrOutput());
		GPOS_ASSERT(NULL != ppartcnstr);

		CPartConstraint *ppartcnstrNewlyCovered =
			CXformUtils::PpartcnstrDisjunction(mp, ppartcnstrCovered,
											   ppartcnstr);

		if (NULL == ppartcnstrNewlyCovered)
		{
			// FIXME: Can this happen here?
			CRefCount::SafeRelease(ppartcnstr);
			continue;
		}
		CRefCount::SafeRelease(ppartcnstrCovered);
		CRefCount::SafeRelease(ppartcnstr);
		ppartcnstrCovered = ppartcnstrNewlyCovered;
	}
	CPartConstraint *ppartcnstrRel = CUtils::PpartcnstrFromMDPartCnstr(
		mp, mda, popGet->PdrgpdrgpcrPart(), relation->MDPartConstraint(),
		popGet->PdrgpcrOutput());
	ppartcnstrRest = ppartcnstrRel->PpartcnstrRemaining(mp, ppartcnstrCovered);

	// PpartcnstrRemaining() returns NULL if ppartcnstrCovered has no constraint
	// on the first level and contraints on higher levels are bounded (see
	// CPartConstraint::FCanNegate()), which is the case for external partitions
	// on multi-level partitioned tables,
	// FIXME: Support multi-level external partitions
	if (ppartcnstrRest == NULL)
	{
		// FIXME: Just return here instead? OR fall back earlier in the translator?
		GPOS_RAISE(
			gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
			GPOS_WSZ_LIT(
				"Query over external partitions in multi-level partitioned table"));
	}

	// Create new partial DynamicGet node with part constraints from ppartcnstrRest
	CName *pnameDG = GPOS_NEW(mp) CName(mp, popGet->Name());
	ptabdesc->AddRef();
	popGet->PdrgpcrOutput()->AddRef();
	popGet->PdrgpdrgpcrPart()->AddRef();
	CLogicalDynamicGet *popPartialDynamicGet = GPOS_NEW(mp) CLogicalDynamicGet(
		mp, pnameDG, ptabdesc, popGet->ScanId(), popGet->PdrgpcrOutput(),
		popGet->PdrgpdrgpcrPart(),
		COptCtxt::PoctxtFromTLS()->UlPartIndexNextVal(), true, /* is_partial */
		ppartcnstrRest, ppartcnstrRel);

	CExpression *pexprPartialDynamicGet =
		GPOS_NEW(mp) CExpression(mp, popPartialDynamicGet);

	// Create new MultiExternalGet node capturing all the external scans with part constraints
	// from ppartcnstrCovered
	CName *pnameMEG = GPOS_NEW(mp) CName(mp, popGet->Name());
	CColRefArray *pdrgpcrNew = CUtils::PdrgpcrCopy(mp, popGet->PdrgpcrOutput());
	ptabdesc->AddRef();
	external_part_mdids->AddRef();

	CLogicalMultiExternalGet *popMultiExternalGet = GPOS_NEW(mp)
		CLogicalMultiExternalGet(mp, external_part_mdids, pnameMEG, ptabdesc,
								 popGet->ScanId(), pdrgpcrNew);
	popMultiExternalGet->SetSecondaryScanId(
		COptCtxt::PoctxtFromTLS()->UlPartIndexNextVal());
	popMultiExternalGet->SetPartial();
	popMultiExternalGet->SetPartConstraint(ppartcnstrCovered);
	CExpression *pexprMultiExternalGet =
		GPOS_NEW(mp) CExpression(mp, popMultiExternalGet);

	// Create a UNION ALL node above the two Gets
	CColRef2dArray *pdrgpdrgpcrInput = GPOS_NEW(mp) CColRef2dArray(mp);

	popPartialDynamicGet->PdrgpcrOutput()->AddRef();
	pdrgpdrgpcrInput->Append(popPartialDynamicGet->PdrgpcrOutput());
	popMultiExternalGet->PdrgpcrOutput()->AddRef();
	pdrgpdrgpcrInput->Append(popMultiExternalGet->PdrgpcrOutput());

	CExpressionArray *pdrgpexprInput = GPOS_NEW(mp) CExpressionArray(mp);
	pdrgpexprInput->Append(pexprPartialDynamicGet);
	pdrgpexprInput->Append(pexprMultiExternalGet);

	popGet->PdrgpcrOutput()->AddRef();
	CExpression *pexprResult = GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp) CLogicalUnionAll(mp, popGet->PdrgpcrOutput(),
									  pdrgpdrgpcrInput, popGet->ScanId()),
		pdrgpexprInput);

	// add alternative to transformation result
	pxfres->Add(pexprResult);
}


// EOF
